#include "radarwidget.h"
#include "soundmanager.h"

#include <QScrollArea>
#include <QDateTime>
#include <QUrlQuery>
#include <QRandomGenerator>
#include <QApplication>
#include <QStyle>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

/* ══════════════════════════════════════════════════════════════════
 *  DESIGN SYSTEM — Light Theme (matching Wino app)
 *  Palette:
 *    page-bg  #f5f6fa     card-bg   #ffffff     card-alt  #f8f9fa
 *    accent   #00b894     accent2   #0984e3     accent3   #d63031
 *    accent4  #fdcb6e     text      #2d3436     text-dim  #636e72
 *    border   #e8e8e8
 * ══════════════════════════════════════════════════════════════════ */

static const char* PAGE_BG   = "#f5f6fa";
static const char* CARD_BG   = "#ffffff";
static const char* CARD_ALT  = "#f8f9fa";
static const char* ACCENT    = "#00b894";
static const char* ACCENT2   = "#0984e3";
static const char* ACCENT3   = "#d63031";
static const char* ACCENT4   = "#fdcb6e";
static const char* TXT       = "#2d3436";
static const char* TXT_DIM   = "#636e72";
static const char* BORDER    = "#e8e8e8";

static QString card(const QString &id) {
    return QString(
        "QFrame#%1{background:white;border-radius:12px;"
        "border:1px solid #e8e8e8;}").arg(id);
}

// ═══════════════════════════════════════════════════════════════
//  RadarMapWidget
// ═══════════════════════════════════════════════════════════════

RadarMapWidget::RadarMapWidget(QWidget *parent) : TileMapWidget(parent) {}

void RadarMapWidget::clearMarkers() { m_alerts.clear(); m_jams.clear(); m_infoText.clear(); update(); }
void RadarMapWidget::addAlertMarker(const AlertMarker &m) { m_alerts.append(m); update(); }
void RadarMapWidget::addJamMarker(const JamSegment &j) { m_jams.append(j); update(); }
void RadarMapWidget::setInfoText(const QString &t) { m_infoText = t; update(); }

void RadarMapWidget::setGpsPosition(double lat, double lng, bool visible) {
    m_gpsLat = lat; m_gpsLng = lng; m_gpsVisible = visible; update();
}
void RadarMapWidget::setGpsAccuracy(double meters) {
    m_gpsAccuracyM = meters; update();
}
void RadarMapWidget::addGpsTrailPoint(double lat, double lng) {
    m_gpsTrail.append(QPointF(lat, lng));
    if (m_gpsTrail.size() > 500) m_gpsTrail.removeFirst(); // limit trail
    update();
}
void RadarMapWidget::clearGpsTrail() { m_gpsTrail.clear(); update(); }

void RadarMapWidget::paintEvent(QPaintEvent *event)
{
    TileMapWidget::paintEvent(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // ── Jam markers — pulsing rings ──
    for (const auto &j : m_jams) {
        QPointF pt = latLngToWidget(j.lat, j.lng);
        if (!rect().contains(pt.toPoint())) continue;
        int r = 8 + j.level * 3;
        QColor outer = j.color; outer.setAlpha(45);
        p.setPen(Qt::NoPen); p.setBrush(outer);
        p.drawEllipse(pt, r + 6, r + 6);
        QColor mid = j.color; mid.setAlpha(90);
        p.setBrush(mid);
        p.drawEllipse(pt, r, r);
        p.setBrush(j.color);
        p.drawEllipse(pt, 4, 4);
    }

    // ── Alert markers — teardrop pins ──
    for (const auto &a : m_alerts) {
        QPointF pt = latLngToWidget(a.lat, a.lng);
        if (!rect().contains(pt.toPoint())) continue;

        // Shadow
        QColor sh(0,0,0,60);
        p.setPen(Qt::NoPen); p.setBrush(sh);
        p.drawEllipse(pt + QPointF(1,3), 10, 10);

        // Outer ring
        QColor glow = a.color; glow.setAlpha(70);
        p.setBrush(glow);
        p.drawEllipse(pt, 14, 14);

        // Pin body
        p.setBrush(a.color);
        p.drawEllipse(pt, 9, 9);

        // White center
        p.setBrush(QColor(255,255,255,230));
        p.drawEllipse(pt, 4, 4);

        // Letter
        QString ch;
        if (a.type == "POLICE") ch = "P";
        else if (a.type == "ACCIDENT") ch = "!";
        else if (a.type == "HAZARD") ch = "H";
        else if (a.type == "ROAD_CLOSED") ch = "X";
        else if (a.type == "CONSTRUCTION") ch = "C";
        else if (a.type == "CAMERA") ch = "R";
        else ch = "?";
        QFont f("Segoe UI", 7, QFont::Bold);
        p.setFont(f); p.setPen(a.color);
        p.drawText(QRectF(pt.x()-4, pt.y()-4, 8, 8), Qt::AlignCenter, ch);
    }

    // ── HUD overlay top-left ──
    if (!m_infoText.isEmpty()) {
        QFont hf("Segoe UI", 10, QFont::DemiBold);
        p.setFont(hf);
        QFontMetrics fm(hf);
        QStringList lines = m_infoText.split('\n');
        int maxW = 0;
        for (auto &l : lines) maxW = qMax(maxW, fm.horizontalAdvance(l));
        int bw = maxW + 28, bh = lines.size() * 18 + 20;
        QRect box(12, 12, bw, bh);

        // Frosted glass background
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(15, 25, 35, 200));
        p.drawRoundedRect(box, 10, 10);
        p.setPen(QColor(0, 229, 160, 60));
        p.drawRoundedRect(box.adjusted(0,0,-1,-1), 10, 10);

        p.setPen(QColor(255, 255, 255, 220));
        int y = box.y() + 16;
        for (auto &l : lines) {
            p.drawText(box.x() + 14, y, l);
            y += 18;
        }
    }

    // ── GPS trail (green line) ──
    if (m_gpsTrail.size() >= 2) {
        QPen trailPen(QColor(0, 229, 160, 180), 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(trailPen);
        p.setBrush(Qt::NoBrush);
        QPainterPath path;
        QPointF first = latLngToWidget(m_gpsTrail[0].x(), m_gpsTrail[0].y());
        path.moveTo(first);
        for (int i = 1; i < m_gpsTrail.size(); i++) {
            QPointF pt = latLngToWidget(m_gpsTrail[i].x(), m_gpsTrail[i].y());
            path.lineTo(pt);
        }
        p.drawPath(path);
    }

    // ── GPS current position (pulsing green dot + accuracy circle) ──
    if (m_gpsVisible) {
        QPointF gp = latLngToWidget(m_gpsLat, m_gpsLng);
        if (rect().contains(gp.toPoint())) {

            // ── Accuracy circle ──────────────────────────────────
            if (m_gpsAccuracyM > 0) {
                // Convert accuracy (metres) to pixels at current zoom
                // 1 degree lat ≈ 111 km.  We map accuracy → widget pixels.
                QPointF edgePt = latLngToWidget(
                    m_gpsLat + (m_gpsAccuracyM / 111000.0), m_gpsLng);
                double radiusPx = qAbs(gp.y() - edgePt.y());
                radiusPx = qMax(radiusPx, 14.0); // always at least 14px

                // Choose colour: green=precise, blue=network
                QColor accColor = (m_gpsAccuracyM < 500)
                    ? QColor(0, 229, 160, 35)   // GPS exact
                    : QColor(9, 132, 227, 25);  // IP réseau
                QColor accBorder = (m_gpsAccuracyM < 500)
                    ? QColor(0, 229, 160, 100)
                    : QColor(9, 132, 227, 80);

                p.setPen(QPen(accBorder, 1.5, Qt::DashLine));
                p.setBrush(accColor);
                p.drawEllipse(gp, radiusPx, radiusPx);
            }
            // ─────────────────────────────────────────────────────

            // Outer pulse ring
            QColor pulse(0, 229, 160, 40);
            p.setPen(Qt::NoPen);
            p.setBrush(pulse);
            p.drawEllipse(gp, 26, 26);

            // Middle ring
            QColor mid(0, 229, 160, 80);
            p.setBrush(mid);
            p.drawEllipse(gp, 16, 16);

            // Core dot
            p.setBrush(QColor(0, 229, 160));
            p.drawEllipse(gp, 8, 8);

            // White inner
            p.setBrush(Qt::white);
            p.drawEllipse(gp, 4, 4);

            // Label
            QFont gf("Segoe UI", 8, QFont::Bold);
            p.setFont(gf);
            QRectF labelRect(gp.x() - 30, gp.y() - 30, 60, 16);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 229, 160, 200));
            p.drawRoundedRect(labelRect, 4, 4);
            p.setPen(Qt::white);
            p.drawText(labelRect, Qt::AlignCenter, QString::fromUtf8("📍 Moi"));
        }
    }

    // ── Legend bottom-left — dark glass ──
    int lx = 12, ly = height() - 108;
    QRect lr(lx, ly, 170, 98);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(15, 25, 35, 210));
    p.drawRoundedRect(lr, 10, 10);
    p.setPen(QColor(BORDER));
    p.drawRoundedRect(lr.adjusted(0,0,-1,-1), 10, 10);

    QFont lf("Segoe UI", 8, QFont::Bold);
    p.setFont(lf); p.setPen(QColor(ACCENT));
    p.drawText(lx + 10, ly + 15, QString::fromUtf8("LEGEND"));

    struct LI { QColor c; QString t; };
    QList<LI> leg = {
        {QColor(ACCENT3), "Police / Radar"},
        {QColor("#e17055"), "Accident"},
        {QColor(ACCENT4), "Bouchon"},
        {QColor(ACCENT2), "Danger"},
        {QColor("#a29bfe"), "Travaux"},
    };
    QFont lf2("Segoe UI", 8); p.setFont(lf2);
    for (int i = 0; i < leg.size(); i++) {
        int y = ly + 28 + i * 14;
        p.setPen(Qt::NoPen); p.setBrush(leg[i].c);
        p.drawEllipse(QPointF(lx + 16, y + 2), 4, 4);
        p.setPen(QColor(TXT)); p.drawText(lx + 26, y + 6, leg[i].t);
    }
}

// ═══════════════════════════════════════════════════════════════
//  Helpers
// ═══════════════════════════════════════════════════════════════

QGraphicsDropShadowEffect* RadarWidget::glow(QWidget *p, const QColor &c, int blur) {
    auto *s = new QGraphicsDropShadowEffect(p);
    s->setBlurRadius(blur); s->setColor(c); s->setOffset(0, 0);
    return s;
}

QGraphicsDropShadowEffect* RadarWidget::softShadow(QWidget *p) {
    auto *s = new QGraphicsDropShadowEffect(p);
    s->setBlurRadius(24); s->setColor(QColor(0,0,0,80)); s->setOffset(0, 4);
    return s;
}

// ═══════════════════════════════════════════════════════════════
//  Constructor
// ═══════════════════════════════════════════════════════════════

RadarWidget::RadarWidget(const QString &userName, const QString &userRole, QWidget *parent)
    : QWidget(parent), m_userName(userName), m_userRole(userRole), m_autoRefresh(false)
{
    // ORS API key — get yours free at https://openrouteservice.org
    // Free tier: 2,500 requests/day — covers incidents, flow, routing, geocoding
    m_orsApiKey = "YOUR_ORS_API_KEY";

    m_networkManager = new QNetworkAccessManager(this);
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(60000);
    connect(m_refreshTimer, &QTimer::timeout, this, &RadarWidget::refreshTrafficData);

    m_pulseTimer = new QTimer(this);
    m_pulseTimer->setInterval(1200);
    connect(m_pulseTimer, &QTimer::timeout, this, &RadarWidget::onPulseTimer);
    m_pulseTimer->start();

    // GPS tracking — multi-source location
    m_gpsAccuracyLabel = nullptr;
    m_gpsLevel = 0;
    m_gpsSourceName = "";

    // Level 1: Try Qt Positioning (real GPS/WiFi from OS)
    m_geoSource = QGeoPositionInfoSource::createDefaultSource(this);
    if (m_geoSource) {
        m_geoSource->setUpdateInterval(5000);
        connect(m_geoSource, &QGeoPositionInfoSource::positionUpdated,
                this, &RadarWidget::onGeoPositionUpdated);
        connect(m_geoSource, &QGeoPositionInfoSource::errorOccurred,
                this, &RadarWidget::onGeoError);
    }

    // Level 2: Network fallback (always available)
    m_gpsNam = new QNetworkAccessManager(this);
    connect(m_gpsNam, &QNetworkAccessManager::finished,
            this, &RadarWidget::onNetworkGpsReceived);

    m_gpsTimer = new QTimer(this);
    m_gpsTimer->setInterval(8000); // 8 seconds
    connect(m_gpsTimer, &QTimer::timeout, this, &RadarWidget::fetchGpsPosition);

    m_cities = {
        {"Tunis",36.75,10.10,36.85,10.25},{"Sfax",34.70,10.70,34.80,10.80},
        {"Sousse",35.80,10.58,35.85,10.64},{"Kairouan",35.65,10.08,35.72,10.12},
        {"Bizerte",37.25,9.85,37.30,9.90},{QString::fromUtf8("Gabes"),33.87,10.08,33.92,10.12},
        {"Ariana",36.85,10.15,36.90,10.20},{"Monastir",35.76,10.80,35.80,10.84},
        {"Ben Arous",36.74,10.22,36.78,10.26},{"Nabeul",36.44,10.72,36.48,10.76},
        {"Paris",48.82,2.25,48.90,2.42},{"Lyon",45.72,4.80,45.78,4.88},
    };
    setupUI();
}

RadarWidget::~RadarWidget() {
    if (m_refreshTimer && m_refreshTimer->isActive()) m_refreshTimer->stop();
    if (m_gpsTimer && m_gpsTimer->isActive()) m_gpsTimer->stop();
    if (m_pulseTimer && m_pulseTimer->isActive()) m_pulseTimer->stop();
    if (m_geoSource) m_geoSource->stopUpdates();
}

void RadarWidget::onPulseTimer() {
    m_pulsePhase = (m_pulsePhase + 1) % 3;
    if (m_liveDot) {
        int alpha = (m_pulsePhase == 0) ? 255 : (m_pulsePhase == 1) ? 160 : 80;
        m_liveDot->setStyleSheet(QString(
            "QLabel{background:rgba(255,255,255,%1);border-radius:5px;border:none;}").arg(alpha));
    }
}

// ═══════════════════════════════════════════════════════════════
//  Setup UI
// ═══════════════════════════════════════════════════════════════

void RadarWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0,0,0,0);
    mainLayout->setSpacing(0);

    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(
        "QScrollArea{background:#f5f6fa;border:none;}"
        "QScrollBar:vertical{width:6px;background:transparent;}"
        "QScrollBar::handle:vertical{background:#ccc;border-radius:3px;min-height:30px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}"
    );

    QWidget *content = new QWidget();
    content->setObjectName("radarContent");
    content->setStyleSheet("QWidget#radarContent{background:#f5f6fa;}");
    QVBoxLayout *cl = new QVBoxLayout(content);
    cl->setContentsMargins(28, 20, 28, 28);
    cl->setSpacing(18);

    cl->addWidget(createBanner());
    cl->addWidget(createStatsRow());

    QHBoxLayout *midRow = new QHBoxLayout();
    midRow->setSpacing(18);
    midRow->addWidget(createMapCard(), 3);
    midRow->addWidget(createAlertsPanel(), 2);
    cl->addLayout(midRow);

    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(18);
    bottomRow->addWidget(createTrafficJamsPanel(), 3);
    bottomRow->addWidget(createRoutePanel(), 2);
    cl->addLayout(bottomRow);

    // ── GPS Tracking Panel ──
    cl->addWidget(createGpsPanel());

    // ── Terminal log ──
    QFrame *logFrame = new QFrame(content);
    logFrame->setObjectName("logF");
    logFrame->setStyleSheet(card("logF"));
    logFrame->setGraphicsEffect(softShadow(logFrame));
    QVBoxLayout *logLay = new QVBoxLayout(logFrame);
    logLay->setContentsMargins(18, 14, 18, 14);
    logLay->setSpacing(8);

    QHBoxLayout *logTitleRow = new QHBoxLayout();
    QLabel *logIcon = new QLabel(QString::fromUtf8("\xe2\x96\x88"), logFrame);
    logIcon->setStyleSheet(QString("QLabel{color:%1;font-size:10px;background:transparent;border:none;}").arg(ACCENT));
    logTitleRow->addWidget(logIcon);
    QLabel *logTitle = new QLabel("TERMINAL — TRAFFIC API", logFrame);
    logTitle->setStyleSheet(QString("QLabel{font-size:10px;font-weight:bold;color:%1;"
        "letter-spacing:2px;background:transparent;border:none;font-family:'Consolas','Courier New',monospace;}").arg(TXT_DIM));
    logTitleRow->addWidget(logTitle);
    logTitleRow->addStretch();
    logLay->addLayout(logTitleRow);

    m_logConsole = new QTextEdit(logFrame);
    m_logConsole->setReadOnly(true);
    m_logConsole->setFixedHeight(90);
    m_logConsole->setStyleSheet(
        "QTextEdit{background:#1e272e;color:#55efc4;font-family:'Consolas','Courier New',monospace;"
        "font-size:10px;border:1px solid #dfe6e9;border-radius:10px;padding:10px;}");
    logLay->addWidget(m_logConsole);
    cl->addWidget(logFrame);

    cl->addStretch();
    scroll->setWidget(content);
    mainLayout->addWidget(scroll);

    addStatusMessage(QString::fromUtf8("Radar initialized — standby."));
}

// ═══════════════════════════════════════════════════════════════
//  Banner — gradient glass with live dot
// ═══════════════════════════════════════════════════════════════

QWidget* RadarWidget::createBanner()
{
    QFrame *banner = new QFrame(this);
    banner->setObjectName("rBanner");
    banner->setFixedHeight(82);
    banner->setStyleSheet(
        "QFrame#rBanner{"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
        "    stop:0 #00b894, stop:0.5 #00a884, stop:1 #009d7e);"
        "  border-radius:16px;"
        "}");
    banner->setGraphicsEffect(softShadow(banner));

    QHBoxLayout *bl = new QHBoxLayout(banner);
    bl->setContentsMargins(26, 0, 22, 0);
    bl->setSpacing(0);

    // Radar icon
    QLabel *icon = new QLabel(QString::fromUtf8("\xf0\x9f\x93\xa1"), banner);
    icon->setFixedSize(44, 44);
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet(
        "QLabel{font-size:28px;background:rgba(255,255,255,0.2);border-radius:22px;border:none;}");
    bl->addWidget(icon);
    bl->addSpacing(16);

    QVBoxLayout *textCol = new QVBoxLayout();
    textCol->setSpacing(1);

    QHBoxLayout *titleRow = new QHBoxLayout();
    titleRow->setSpacing(10);
    QLabel *title = new QLabel("TRAFFIC SPEED CAM", banner);
    title->setStyleSheet(
        "QLabel{font-size:17px;font-weight:bold;color:white;letter-spacing:3px;"
        "font-family:'Georgia',serif;background:transparent;border:none;}");
    titleRow->addWidget(title);

    // Live indicator
    m_liveDot = new QLabel(banner);
    m_liveDot->setFixedSize(10, 10);
    m_liveDot->setStyleSheet("QLabel{background:white;border-radius:5px;border:none;}");
    titleRow->addWidget(m_liveDot);

    QLabel *liveLbl = new QLabel("LIVE", banner);
    liveLbl->setStyleSheet(
        "QLabel{font-size:9px;font-weight:bold;color:rgba(255,255,255,0.85);letter-spacing:2px;"
        "background:transparent;border:none;}");
    titleRow->addWidget(liveLbl);
    titleRow->addStretch();
    textCol->addLayout(titleRow);

    QLabel *sub = new QLabel(QString::fromUtf8("OpenRouteService · Alerts · Jams · Routes"), banner);
    sub->setStyleSheet("QLabel{font-size:11px;color:rgba(255,255,255,0.8);background:transparent;border:none;}");
    textCol->addWidget(sub);
    bl->addLayout(textCol, 1);

    // ORS API Key field
    m_apiKeyEdit = new QLineEdit(banner);
    m_apiKeyEdit->setPlaceholderText(QString::fromUtf8("ORS API Key"));
    m_apiKeyEdit->setText(m_orsApiKey == "YOUR_ORS_API_KEY" ? "" : m_orsApiKey);
    m_apiKeyEdit->setFixedWidth(160);
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    m_apiKeyEdit->setStyleSheet(
        "QLineEdit{background:rgba(255,255,255,0.2);color:white;border:1px solid rgba(255,255,255,0.3);"
        "border-radius:10px;padding:7px 10px;font-size:10px;font-family:'Consolas',monospace;}"
        "QLineEdit:focus{border-color:rgba(255,255,255,0.6);}");
    connect(m_apiKeyEdit, &QLineEdit::textChanged, this, [this](const QString &key) {
        m_orsApiKey = key.trimmed();
    });
    bl->addWidget(m_apiKeyEdit);
    bl->addSpacing(6);

    // City selector
    m_cityCombo = new QComboBox(banner);
    for (const auto &c : m_cities) m_cityCombo->addItem(c.name);
    m_cityCombo->setFixedWidth(128);
    m_cityCombo->setStyleSheet(
        "QComboBox{background:rgba(255,255,255,0.2);color:white;border:1px solid rgba(255,255,255,0.3);"
        "border-radius:10px;padding:7px 12px;font-size:11px;font-weight:bold;}"
        "QComboBox::drop-down{border:none;width:20px;}"
        "QComboBox QAbstractItemView{background:white;color:#2d3436;selection-background-color:#00b894;"
        "selection-color:white;border-radius:8px;}");
    m_cityCombo->setCurrentIndex(1);
    m_currentCity = 1;
    connect(m_cityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadarWidget::onCityChanged);
    bl->addWidget(m_cityCombo);
    bl->addSpacing(10);

    // Refresh button
    QString btnSS =
        "QPushButton{background:rgba(255,255,255,0.2);color:white;border:1px solid rgba(255,255,255,0.3);"
        "border-radius:10px;padding:8px 14px;font-size:11px;font-weight:bold;}"
        "QPushButton:hover{background:rgba(255,255,255,0.35);}";

    m_refreshBtn = new QPushButton(QString::fromUtf8("↻  Refresh"), banner);
    m_refreshBtn->setCursor(Qt::PointingHandCursor);
    m_refreshBtn->setStyleSheet(btnSS);
    connect(m_refreshBtn, &QPushButton::clicked, this, &RadarWidget::refreshTrafficData);
    bl->addWidget(m_refreshBtn);
    bl->addSpacing(6);

    m_autoRefreshBtn = new QPushButton("AUTO", banner);
    m_autoRefreshBtn->setCursor(Qt::PointingHandCursor);
    m_autoRefreshBtn->setCheckable(true);
    m_autoRefreshBtn->setStyleSheet(
        "QPushButton{background:rgba(255,255,255,0.15);color:rgba(255,255,255,0.7);border:1px solid rgba(255,255,255,0.2);"
        "border-radius:10px;padding:8px 12px;font-size:10px;font-weight:bold;letter-spacing:1px;}"
        "QPushButton:hover{background:rgba(255,255,255,0.25);}"
        "QPushButton:checked{background:rgba(255,255,255,0.3);border-color:white;color:white;}");
    connect(m_autoRefreshBtn, &QPushButton::clicked, this, &RadarWidget::toggleAutoRefresh);
    bl->addWidget(m_autoRefreshBtn);

    return banner;
}

// ═══════════════════════════════════════════════════════════════
//  Stats Row — glowing metric cards
// ═══════════════════════════════════════════════════════════════

QWidget* RadarWidget::createStatsRow()
{
    QWidget *row = new QWidget(this);
    row->setStyleSheet("background:transparent;");
    QHBoxLayout *rl = new QHBoxLayout(row);
    rl->setContentsMargins(0,0,0,0);
    rl->setSpacing(14);

    m_alertCountLabel = new QLabel("0");
    m_jamCountLabel   = new QLabel("0");
    m_avgSpeedLabel   = new QLabel("--");
    m_delayLabel      = new QLabel("--");

    rl->addWidget(createStatPill(QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f"), "0", "Alerts", ACCENT3), 1);
    rl->addWidget(createStatPill(QString::fromUtf8("\xf0\x9f\x9a\xa7"), "0", "Jams", ACCENT4), 1);
    rl->addWidget(createStatPill(QString::fromUtf8("\xf0\x9f\x8f\x8e\xef\xb8\x8f"), "--", "Avg. Speed", ACCENT2), 1);
    rl->addWidget(createStatPill(QString::fromUtf8("\xe2\x8f\xb1\xef\xb8\x8f"), "--", "Total Delay", ACCENT), 1);
    return row;
}

QWidget* RadarWidget::createStatPill(const QString &icon, const QString &val,
                                      const QString &label, const QString &baseColor)
{
    QFrame *pill = new QFrame(this);
    static int pid = 0;
    QString oid = QString("sp%1").arg(pid++);
    pill->setObjectName(oid);
    pill->setStyleSheet(QString(
        "QFrame#%1{background:%2;border-radius:14px;border:1px solid %3;}")
        .arg(oid, CARD_BG, BORDER));
    pill->setGraphicsEffect(softShadow(pill));
    pill->setFixedHeight(76);

    QHBoxLayout *pl = new QHBoxLayout(pill);
    pl->setContentsMargins(18, 10, 18, 10);
    pl->setSpacing(12);

    // Colored accent line
    QLabel *accentBar = new QLabel(pill);
    accentBar->setFixedSize(3, 36);
    accentBar->setStyleSheet(QString("QLabel{background:%1;border-radius:1px;border:none;}").arg(baseColor));
    pl->addWidget(accentBar);

    QLabel *ic = new QLabel(icon, pill);
    ic->setStyleSheet("QLabel{font-size:22px;background:transparent;border:none;}");
    pl->addWidget(ic);

    QVBoxLayout *vl = new QVBoxLayout();
    vl->setSpacing(0);
    QLabel *valLbl = new QLabel(val, pill);
    valLbl->setStyleSheet(QString("QLabel{font-size:20px;font-weight:bold;color:%1;"
        "background:transparent;border:none;font-family:'Segoe UI';}").arg(baseColor));
    vl->addWidget(valLbl);
    QLabel *lblLbl = new QLabel(label, pill);
    lblLbl->setStyleSheet(QString("QLabel{font-size:9px;color:%1;font-weight:600;"
        "letter-spacing:1px;background:transparent;border:none;text-transform:uppercase;}").arg(TXT_DIM));
    vl->addWidget(lblLbl);
    pl->addLayout(vl, 1);

    if (label == "Alerts") m_alertCountLabel = valLbl;
    else if (label == "Jams") m_jamCountLabel = valLbl;
    else if (label == "Avg. Speed") m_avgSpeedLabel = valLbl;
    else if (label == "Total Delay") m_delayLabel = valLbl;

    return pill;
}

// ═══════════════════════════════════════════════════════════════
//  Map Card
// ═══════════════════════════════════════════════════════════════

QWidget* RadarWidget::createMapCard()
{
    QFrame *c = new QFrame(this);
    c->setObjectName("mapC");
    c->setStyleSheet(card("mapC"));
    c->setGraphicsEffect(softShadow(c));
    c->setMinimumHeight(440);

    QVBoxLayout *cl = new QVBoxLayout(c);
    cl->setContentsMargins(14, 12, 14, 12);
    cl->setSpacing(8);

    // Title row
    QHBoxLayout *tr = new QHBoxLayout();
    QLabel *title = new QLabel("CARTE TRAFIC", c);
    title->setStyleSheet(QString("QLabel{font-size:11px;font-weight:bold;color:%1;"
        "letter-spacing:2px;background:transparent;border:none;}").arg(TXT_DIM));
    tr->addWidget(title);
    tr->addStretch();

    QString zSS = QString(
        "QPushButton{background:%1;color:%2;border:1px solid %3;border-radius:8px;"
        "font-size:15px;font-weight:bold;}"
        "QPushButton:hover{background:%4;border-color:%5;}"
    ).arg(CARD_ALT, TXT, BORDER, "#1e3450", ACCENT);

    QPushButton *zi = new QPushButton("+", c);
    zi->setFixedSize(30, 30); zi->setCursor(Qt::PointingHandCursor);
    zi->setStyleSheet(zSS);
    connect(zi, &QPushButton::clicked, this, &RadarWidget::zoomIn);
    tr->addWidget(zi);

    QPushButton *zo = new QPushButton(QString::fromUtf8("\xe2\x88\x92"), c);
    zo->setFixedSize(30, 30); zo->setCursor(Qt::PointingHandCursor);
    zo->setStyleSheet(zSS);
    connect(zo, &QPushButton::clicked, this, &RadarWidget::zoomOut);
    tr->addWidget(zo);
    cl->addLayout(tr);

    // Map
    m_radarMap = new RadarMapWidget(c);
    m_radarMap->setMinimumHeight(360);
    m_radarMap->setStyleSheet(QString("border:1px solid %1;border-radius:12px;").arg(BORDER));

    const CityBBox &city = m_cities[m_currentCity];
    m_radarMap->setCenter((city.bottom + city.top) / 2.0, (city.left + city.right) / 2.0);
    m_radarMap->setZoom(14);
    m_radarMap->setInfoText(QString::fromUtf8("📡 %1 — Refresh pour charger").arg(city.name));
    cl->addWidget(m_radarMap, 1);

    m_lastUpdateLabel = new QLabel(QString::fromUtf8("—"), c);
    m_lastUpdateLabel->setStyleSheet(QString("QLabel{font-size:9px;color:%1;background:transparent;border:none;}").arg(TXT_DIM));
    cl->addWidget(m_lastUpdateLabel);

    return c;
}

void RadarWidget::zoomIn()  { int z = m_radarMap->zoom(); if (z < 18) m_radarMap->setZoom(z + 1); }
void RadarWidget::zoomOut() { int z = m_radarMap->zoom(); if (z > 2)  m_radarMap->setZoom(z - 1); }

// ═══════════════════════════════════════════════════════════════
//  Alerts Panel
// ═══════════════════════════════════════════════════════════════

QWidget* RadarWidget::createAlertsPanel()
{
    QFrame *c = new QFrame(this);
    c->setObjectName("alC");
    c->setStyleSheet(card("alC"));
    c->setGraphicsEffect(softShadow(c));
    c->setMinimumHeight(440);

    QVBoxLayout *cl = new QVBoxLayout(c);
    cl->setContentsMargins(18, 14, 18, 14);
    cl->setSpacing(8);

    QHBoxLayout *hdr = new QHBoxLayout();
    QLabel *title = new QLabel("ALERTES", c);
    title->setStyleSheet(QString("QLabel{font-size:11px;font-weight:bold;color:%1;"
        "letter-spacing:2px;background:transparent;border:none;}").arg(TXT_DIM));
    hdr->addWidget(title);
    hdr->addStretch();

    m_statusLabel = new QLabel(QString::fromUtf8("En attente…"), c);
    m_statusLabel->setStyleSheet(QString(
        "QLabel{font-size:9px;color:%1;background:%2;"
        "border-radius:8px;padding:4px 10px;border:1px solid %3;}"
    ).arg(TXT_DIM, CARD_ALT, BORDER));
    hdr->addWidget(m_statusLabel);
    cl->addLayout(hdr);

    QScrollArea *sc = new QScrollArea(c);
    sc->setWidgetResizable(true);
    sc->setFrameShape(QFrame::NoFrame);
    sc->setStyleSheet(QString(
        "QScrollArea{background:transparent;border:none;}"
        "QScrollBar:vertical{width:3px;background:transparent;}"
        "QScrollBar::handle:vertical{background:%1;border-radius:1px;}"
    ).arg(BORDER));
    QWidget *cont = new QWidget();
    cont->setStyleSheet("background:transparent;");
    m_alertsLayout = new QVBoxLayout(cont);
    m_alertsLayout->setContentsMargins(0,0,0,0);
    m_alertsLayout->setSpacing(6);
    m_alertsLayout->addStretch();
    sc->setWidget(cont);
    cl->addWidget(sc, 1);

    return c;
}

// ═══════════════════════════════════════════════════════════════
//  Jams Panel
// ═══════════════════════════════════════════════════════════════

QWidget* RadarWidget::createTrafficJamsPanel()
{
    QFrame *c = new QFrame(this);
    c->setObjectName("jmC");
    c->setStyleSheet(card("jmC"));
    c->setGraphicsEffect(softShadow(c));
    c->setMinimumHeight(280);

    QVBoxLayout *cl = new QVBoxLayout(c);
    cl->setContentsMargins(18, 14, 18, 14);
    cl->setSpacing(8);

    QLabel *title = new QLabel("BOUCHONS", c);
    title->setStyleSheet(QString("QLabel{font-size:11px;font-weight:bold;color:%1;"
        "letter-spacing:2px;background:transparent;border:none;}").arg(TXT_DIM));
    cl->addWidget(title);

    QScrollArea *sc = new QScrollArea(c);
    sc->setWidgetResizable(true);
    sc->setFrameShape(QFrame::NoFrame);
    sc->setStyleSheet(QString(
        "QScrollArea{background:transparent;border:none;}"
        "QScrollBar:vertical{width:3px;background:transparent;}"
        "QScrollBar::handle:vertical{background:%1;border-radius:1px;}"
    ).arg(BORDER));
    QWidget *cont = new QWidget();
    cont->setStyleSheet("background:transparent;");
    m_jamsLayout = new QVBoxLayout(cont);
    m_jamsLayout->setContentsMargins(0,0,0,0);
    m_jamsLayout->setSpacing(6);
    m_jamsLayout->addStretch();
    sc->setWidget(cont);
    cl->addWidget(sc, 1);

    return c;
}

// ═══════════════════════════════════════════════════════════════
//  Route Panel
// ═══════════════════════════════════════════════════════════════

QWidget* RadarWidget::createRoutePanel()
{
    QFrame *c = new QFrame(this);
    c->setObjectName("rtC");
    c->setStyleSheet(card("rtC"));
    c->setGraphicsEffect(softShadow(c));
    c->setMinimumHeight(280);

    QVBoxLayout *cl = new QVBoxLayout(c);
    cl->setContentsMargins(18, 14, 18, 14);
    cl->setSpacing(10);

    QLabel *title = new QLabel(QString::fromUtf8("ROUTE"), c);
    title->setStyleSheet(QString("QLabel{font-size:11px;font-weight:bold;color:%1;"
        "letter-spacing:2px;background:transparent;border:none;}").arg(TXT_DIM));
    cl->addWidget(title);

    QString editSS = QString(
        "QLineEdit{background:%1;border:1px solid %2;border-radius:10px;"
        "padding:9px 14px;font-size:12px;color:%3;font-family:'Segoe UI';}"
        "QLineEdit:focus{border-color:%4;}"
        "QLineEdit::placeholder{color:#b2bec3;}"
    ).arg(CARD_ALT, BORDER, TXT, ACCENT);

    QLabel *fl = new QLabel(QString::fromUtf8("DEPARTURE"), c);
    fl->setStyleSheet(QString("QLabel{font-size:9px;font-weight:bold;color:%1;"
        "letter-spacing:1px;background:transparent;border:none;}").arg(TXT_DIM));
    cl->addWidget(fl);
    m_fromEdit = new QLineEdit(c);
    m_fromEdit->setPlaceholderText(QString::fromUtf8("Avenue Habib Bourguiba, Sfax"));
    m_fromEdit->setStyleSheet(editSS);
    cl->addWidget(m_fromEdit);

    QLabel *tl = new QLabel(QString::fromUtf8("ARRIVAL"), c);
    tl->setStyleSheet(QString("QLabel{font-size:9px;font-weight:bold;color:%1;"
        "letter-spacing:1px;background:transparent;border:none;}").arg(TXT_DIM));
    cl->addWidget(tl);
    m_toEdit = new QLineEdit(c);
    m_toEdit->setPlaceholderText(QString::fromUtf8("Wino Driving School Center"));
    m_toEdit->setStyleSheet(editSS);
    cl->addWidget(m_toEdit);

    QPushButton *goBtn = new QPushButton(QString::fromUtf8("CALCULER L'ROUTE"), c);
    goBtn->setCursor(Qt::PointingHandCursor);
    goBtn->setStyleSheet(QString(
        "QPushButton{background:%1;color:white;border:none;border-radius:10px;"
        "padding:11px;font-size:11px;font-weight:bold;letter-spacing:1px;}"
        "QPushButton:hover{background:%2;}"
    ).arg(ACCENT, "#00cc8e"));
    connect(goBtn, &QPushButton::clicked, this, &RadarWidget::searchRoute);
    cl->addWidget(goBtn);

    m_routeResultLayout = new QVBoxLayout();
    m_routeResultLayout->setSpacing(4);
    m_routeNameLabel = new QLabel("", c);
    m_routeNameLabel->setStyleSheet(QString("QLabel{font-size:12px;font-weight:bold;color:%1;background:transparent;border:none;}").arg(ACCENT));
    m_routeNameLabel->hide();
    m_routeResultLayout->addWidget(m_routeNameLabel);
    m_routeDistLabel = new QLabel("", c);
    m_routeDistLabel->setStyleSheet(QString("QLabel{font-size:11px;color:%1;background:transparent;border:none;}").arg(TXT));
    m_routeDistLabel->hide();
    m_routeResultLayout->addWidget(m_routeDistLabel);
    m_routeTimeLabel = new QLabel("", c);
    m_routeTimeLabel->setStyleSheet(QString("QLabel{font-size:11px;color:%1;background:transparent;border:none;}").arg(TXT));
    m_routeTimeLabel->hide();
    m_routeResultLayout->addWidget(m_routeTimeLabel);
    cl->addLayout(m_routeResultLayout);
    cl->addStretch();

    return c;
}

// ═══════════════════════════════════════════════════════════════
//  Alert Row — dark glass card with left accent
// ═══════════════════════════════════════════════════════════════

QWidget* RadarWidget::createAlertRow(const QString &icon, const QString &type,
                                      const QString &street, const QString &info,
                                      int confidence, const QString &color)
{
    QFrame *row = new QFrame(this);
    static int aid = 0;
    QString oid = QString("aR%1").arg(aid++);
    row->setObjectName(oid);
    row->setStyleSheet(QString(
        "QFrame#%1{background:%2;border-radius:10px;border-left:3px solid %3;border:1px solid %4;"
        "border-left:3px solid %3;}")
        .arg(oid, CARD_ALT, color, BORDER));
    row->setFixedHeight(54);

    QHBoxLayout *rl = new QHBoxLayout(row);
    rl->setContentsMargins(12, 6, 12, 6);
    rl->setSpacing(10);

    QLabel *ic = new QLabel(icon, row);
    ic->setFixedWidth(26);
    ic->setStyleSheet("QLabel{font-size:16px;background:transparent;border:none;}");
    rl->addWidget(ic);

    QVBoxLayout *tc = new QVBoxLayout();
    tc->setSpacing(1);
    QLabel *typeLbl = new QLabel(type, row);
    typeLbl->setStyleSheet(QString("QLabel{font-size:10px;font-weight:bold;color:%1;"
        "letter-spacing:1px;background:transparent;border:none;}").arg(color));
    tc->addWidget(typeLbl);
    QLabel *stLbl = new QLabel(street.isEmpty() ? "Unknown road" : street, row);
    stLbl->setStyleSheet(QString("QLabel{font-size:9px;color:%1;background:transparent;border:none;}").arg(TXT_DIM));
    tc->addWidget(stLbl);
    rl->addLayout(tc, 1);

    // Confidence mini bar
    QProgressBar *conf = new QProgressBar(row);
    conf->setRange(0, 10); conf->setValue(confidence);
    conf->setFixedWidth(44); conf->setFixedHeight(4);
    conf->setTextVisible(false);
    conf->setStyleSheet(QString(
        "QProgressBar{background:%1;border:none;border-radius:2px;}"
        "QProgressBar::chunk{background:%2;border-radius:2px;}").arg(BORDER, color));
    rl->addWidget(conf);

    return row;
}

// ═══════════════════════════════════════════════════════════════
//  Jam Row
// ═══════════════════════════════════════════════════════════════

QWidget* RadarWidget::createJamRow(const QString &street, int level, int speed,
                                    int delay, double length)
{
    QFrame *row = new QFrame(this);
    static int jid = 0;
    QString oid = QString("jR%1").arg(jid++);
    row->setObjectName(oid);

    QString lc, lt;
    switch(level) {
        case 0: lc="#00b894"; lt="OK"; break;
        case 1: lc="#fdcb6e"; lt="SLOW"; break;
        case 2: lc="#e17055"; lt="HEAVY"; break;
        case 3: lc="#d63031"; lt=QString::fromUtf8("BLOCKED"); break;
        default: lc="#a29bfe"; lt="STOP"; break;
    }

    row->setStyleSheet(QString(
        "QFrame#%1{background:%2;border-radius:10px;border:1px solid %3;border-left:3px solid %4;}")
        .arg(oid, CARD_ALT, BORDER, lc));
    row->setFixedHeight(50);

    QHBoxLayout *rl = new QHBoxLayout(row);
    rl->setContentsMargins(12, 6, 12, 6);
    rl->setSpacing(10);

    // Level dot
    QLabel *dot = new QLabel(row);
    dot->setFixedSize(10, 10);
    dot->setStyleSheet(QString("QLabel{background:%1;border-radius:5px;border:none;}").arg(lc));
    rl->addWidget(dot);

    QVBoxLayout *tc = new QVBoxLayout();
    tc->setSpacing(1);
    QLabel *stLbl = new QLabel(street.isEmpty() ? "—" : street, row);
    stLbl->setStyleSheet(QString("QLabel{font-size:10px;font-weight:bold;color:%1;background:transparent;border:none;}").arg(TXT));
    tc->addWidget(stLbl);
    QLabel *det = new QLabel(
        QString("%1 km/h · %2 min · %3 km")
            .arg(speed).arg(delay/60).arg(length/1000.0,0,'f',1), row);
    det->setStyleSheet(QString("QLabel{font-size:8px;color:%1;background:transparent;border:none;}").arg(TXT_DIM));
    tc->addWidget(det);
    rl->addLayout(tc, 1);

    QLabel *badge = new QLabel(lt, row);
    badge->setFixedWidth(52); badge->setAlignment(Qt::AlignCenter);
    badge->setStyleSheet(QString(
        "QLabel{font-size:8px;font-weight:bold;color:%1;background:rgba(%2,%3,%4,0.15);"
        "border-radius:6px;padding:3px 6px;border:none;letter-spacing:1px;}")
        .arg(lc).arg(QColor(lc).red()).arg(QColor(lc).green()).arg(QColor(lc).blue()));
    rl->addWidget(badge);

    return row;
}

// ═══════════════════════════════════════════════════════════════
//  Network
// ═══════════════════════════════════════════════════════════════

void RadarWidget::refreshTrafficData()
{
    SoundManager::instance()->play(SoundManager::NavClick);
    if (m_currentCity < 0 || m_currentCity >= m_cities.size()) return;
    const CityBBox &city = m_cities[m_currentCity];
    m_radarMap->setInfoText(QString::fromUtf8("⏳ %1…").arg(city.name));

    // ── Trafic en temps réel — estimation intelligente ──
    // Basé sur l'heure actuelle, le jour de la semaine et les patterns
    // de congestion connus pour les villes tunisiennes.
    // (Aucune API gratuite sans CB ne couvre le trafic en Tunisie)
    addStatusMessage(QString::fromUtf8("📡 Traffic analysis [%1]...").arg(city.name));
    loadSmartTrafficData();
    addStatusMessage(QString::fromUtf8("✓ Traffic updated"));
}

void RadarWidget::onTrafficDataReceived(QNetworkReply *reply)
{
    reply->deleteLater(); // kept for routing callbacks
}

// ═══════════════════════════════════════════════════════════════
//  Smart Traffic Data — time-aware estimation for Tunisia
//  Generates realistic traffic based on hour, day, city patterns
// ═══════════════════════════════════════════════════════════════

void RadarWidget::loadSmartTrafficData()
{
    clearAlerts(); clearJams();
    m_alertMarkers.clear(); m_jamMarkers.clear();

    if (m_currentCity < 0 || m_currentCity >= m_cities.size()) return;
    const CityBBox &city = m_cities[m_currentCity];
    double cLat = (city.bottom + city.top) / 2.0;
    double cLon = (city.left + city.right) / 2.0;
    double spread = (city.top - city.bottom) / 2.5;
    auto rng = QRandomGenerator::global();

    QDateTime now = QDateTime::currentDateTime();
    int hour = now.time().hour();
    int dayOfWeek = now.date().dayOfWeek(); // 1=Mon, 7=Sun
    bool isWeekend = (dayOfWeek == 6 || dayOfWeek == 7); // Fri-Sat in Tunisia (or Sat-Sun)

    // ── Rush hour multiplier (Tunisia patterns) ──
    // Morning rush: 7-9h, Evening rush: 17-19h
    // Midday: moderate, Night: low
    double rushFactor;
    if (hour >= 7 && hour <= 9)       rushFactor = isWeekend ? 0.4 : 0.9;
    else if (hour >= 12 && hour <= 14) rushFactor = isWeekend ? 0.3 : 0.6;
    else if (hour >= 17 && hour <= 19) rushFactor = isWeekend ? 0.5 : 1.0;
    else if (hour >= 20 || hour <= 5)  rushFactor = 0.15;
    else                                rushFactor = isWeekend ? 0.25 : 0.45;

    addStatusMessage(QString::fromUtf8("⏰ %1h — rush factor: %2")
        .arg(hour).arg(rushFactor, 0, 'f', 2));

    // ── City-specific streets ──
    struct StreetInfo { QString name; double latOff; double lonOff; double congestionBase; };
    QList<StreetInfo> streets;

    if (city.name == "Sfax") {
        streets = {
            {"Avenue Habib Bourguiba", 0.01, 0.005, 0.8},
            {"Route de Tunis (RN1)", 0.02, -0.01, 0.7},
            {"Route de Gremda", -0.015, 0.02, 0.6},
            {"Avenue Majida Boulila", 0.005, -0.005, 0.75},
            {"Rocade km4", -0.02, -0.015, 0.65},
            {"Avenue des Martyrs", 0.008, 0.01, 0.5},
            {"Bvd de l'Environnement", -0.008, 0.008, 0.55},
            {"Route de l'Aéroport", 0.025, 0.015, 0.4},
            {"Avenue Ali Belhouane", -0.005, -0.008, 0.6},
            {"Route Menzel Chaker", -0.01, -0.02, 0.35},
        };
    } else if (city.name == "Tunis") {
        streets = {
            {"Avenue Habib Bourguiba", 0.01, 0.005, 0.9},
            {"Avenue Mohamed V", 0.005, -0.01, 0.85},
            {"Route X (Autoroute)", 0.03, 0.02, 0.7},
            {"Avenue de la Liberté", -0.01, 0.005, 0.75},
            {"Rue de Marseille", 0.008, -0.005, 0.65},
            {"Bvd du 9 Avril", -0.005, 0.01, 0.7},
            {"Avenue de Carthage", 0.015, 0.008, 0.6},
            {"Route de la Marsa", 0.02, -0.015, 0.55},
        };
    } else {
        // Generic streets for other cities
        streets = {
            {"Main Avenue", 0.005, 0.003, 0.7},
            {"National Road", 0.015, -0.008, 0.6},
            {"Central Boulevard", -0.008, 0.005, 0.65},
            {"Commerce Street", 0.003, -0.003, 0.55},
            {"Avenue de la République", -0.01, 0.01, 0.5},
            {"Ring Road", 0.02, 0.012, 0.45},
        };
    }

    // ── Generate jams based on rush factor ──
    m_totalJams = 0; m_totalAlerts = 0;
    double totalSpeed = 0; m_totalDelaySec = 0;

    for (const auto &st : streets) {
        double congestion = st.congestionBase * rushFactor + (rng->bounded(20) - 10) / 100.0;
        congestion = qBound(0.0, congestion, 1.0);

        double freeSpeed = 50.0 + rng->bounded(30); // 50-80 km/h
        double currentSpeed = freeSpeed * (1.0 - congestion * 0.85);
        if (currentSpeed < 3) currentSpeed = 3;

        int level;
        if (congestion >= 0.8)      level = 3;
        else if (congestion >= 0.6) level = 2;
        else if (congestion >= 0.35) level = 1;
        else                         level = 0;

        double length = 800 + rng->bounded(3000);
        int delay = static_cast<int>((length / (currentSpeed / 3.6)) - (length / (freeSpeed / 3.6)));
        if (delay < 0) delay = 0;

        totalSpeed += currentSpeed;

        if (level >= 1) { // Only show congested segments
            m_totalJams++;
            m_totalDelaySec += delay;

            m_jamsLayout->insertWidget(m_jamsLayout->count() - 1,
                createJamRow(st.name, level, static_cast<int>(currentSpeed), delay, length));

            QColor jc;
            switch(level) {
                case 1: jc = QColor("#fdcb6e"); break;
                case 2: jc = QColor("#e17055"); break;
                case 3: jc = QColor("#d63031"); break;
                default: jc = QColor("#00b894"); break;
            }
            JamSegment seg;
            seg.lat = cLat + st.latOff + (rng->bounded(60) - 30) / 10000.0;
            seg.lng = cLon + st.lonOff + (rng->bounded(60) - 30) / 10000.0;
            seg.level = level; seg.speed = static_cast<int>(currentSpeed);
            seg.street = st.name; seg.color = jc;
            m_jamMarkers.append(seg);
        }
    }

    m_avgSpeed = streets.isEmpty() ? 0 : totalSpeed / streets.size();

    // ── Generate alerts (random based on rush) ──
    struct AlertTemplate { QString icon; QString type; QString color; double probability; };
    QList<AlertTemplate> alertTemplates = {
        {QString::fromUtf8("\xf0\x9f\x9a\xa8"), "POLICE",    "#d63031", 0.6},
        {QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f"), "ACCIDENT",  "#e17055", rushFactor * 0.5},
        {QString::fromUtf8("\xf0\x9f\x9a\xa7"), "ROADWORKS",   "#a29bfe", 0.4},
        {QString::fromUtf8("\xf0\x9f\x93\xb7"), "SPEED CAM",     "#6c5ce7", 0.5},
        {QString::fromUtf8("\xe2\x9b\xbd"),     "DANGER",    "#0984e3", rushFactor * 0.3},
        {QString::fromUtf8("\xf0\x9f\x9a\xab"), "CLOSED",    "#636e72", 0.2},
    };

    for (const auto &at : alertTemplates) {
        if (rng->bounded(100) / 100.0 < at.probability) {
            m_totalAlerts++;
            int stIdx = rng->bounded(streets.size());
            const auto &st = streets[stIdx];
            int conf = 5 + rng->bounded(5);

            m_alertsLayout->insertWidget(m_alertsLayout->count() - 1,
                createAlertRow(at.icon, at.type, st.name, at.type, conf, at.color));

            AlertMarker marker;
            marker.lat = cLat + st.latOff + (rng->bounded(80) - 40) / 10000.0;
            marker.lng = cLon + st.lonOff + (rng->bounded(80) - 40) / 10000.0;
            marker.type = at.type; marker.street = st.name;
            marker.confidence = conf; marker.color = QColor(at.color);
            m_alertMarkers.append(marker);
        }
    }

    updateStatsDisplay();
    updateMapMarkers();

    m_lastUpdateLabel->setText(QDateTime::currentDateTime().toString("hh:mm:ss · dd/MM/yyyy"));
    m_statusLabel->setText(QString::fromUtf8("%1 alertes · %2 bouchons · %3 km/h")
        .arg(m_totalAlerts).arg(m_totalJams).arg(m_avgSpeed, 0, 'f', 0));
    m_statusLabel->setStyleSheet(QString(
        "QLabel{font-size:9px;color:%1;background:rgba(0,184,148,0.08);"
        "border-radius:8px;padding:4px 10px;border:1px solid rgba(0,184,148,0.2);}").arg(ACCENT));

    m_radarMap->setInfoText(QString::fromUtf8("%1 · %2 alertes · %3 bouchons · %4 km/h")
        .arg(city.name).arg(m_totalAlerts).arg(m_totalJams).arg(m_avgSpeed, 0, 'f', 0));
}

// ═══════════════════════════════════════════════════════════════
//  Demo Data
// ═══════════════════════════════════════════════════════════════

void RadarWidget::loadDemoData()
{
    clearAlerts(); clearJams(); m_alertMarkers.clear(); m_jamMarkers.clear();
    const CityBBox &city = m_cities[m_currentCity];
    double cLat=(city.bottom+city.top)/2.0, cLon=(city.left+city.right)/2.0;
    double sp=(city.top-city.bottom)/2.5;
    auto rng=QRandomGenerator::global();

    struct DA{QString i,t,s;int c;QString cl;};
    QList<DA> da={
        {QString::fromUtf8("\xf0\x9f\x9a\xa8"),"POLICE","Route de Tunis",8,ACCENT3},
        {QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f"),"ACCIDENT","Ave. Habib Bourguiba",7,"#e17055"},
        {QString::fromUtf8("\xf0\x9f\x9a\xa7"),"ROAD_CLOSED",QString::fromUtf8("Rue de la République"),9,ACCENT4},
        {QString::fromUtf8("\xe2\x9b\xbd"),"HAZARD",QString::fromUtf8("Route de l'Aéroport"),5,ACCENT2},
        {QString::fromUtf8("\xf0\x9f\x93\xb7"),"CAMERA","Bvd du 14 Janvier",6,"#a29bfe"},
        {QString::fromUtf8("\xf0\x9f\x9a\xa7"),"CONSTRUCTION","Ave. Ali Belhouane",8,"#e17055"},
    };
    m_totalAlerts=da.size();
    for(auto &a:da){
        m_alertsLayout->insertWidget(m_alertsLayout->count()-1,createAlertRow(a.i,a.t,a.s,a.t,a.c,a.cl));
        AlertMarker mk; mk.lat=cLat+(rng->bounded(200)-100)/100.0*sp;
        mk.lng=cLon+(rng->bounded(200)-100)/100.0*sp;
        mk.type=a.t;mk.street=a.s;mk.confidence=a.c;mk.color=QColor(a.cl);
        m_alertMarkers.append(mk);
    }

    struct DJ{QString s;int l,sp,d;double ln;};
    QList<DJ> dj={
        {"Ave. Habib Bourguiba",3,8,420,2300},{"Route de Tunis (RN1)",2,22,180,4500},
        {"Bvd de l'Environnement",1,35,90,1800},{"Ave. Majida Boulila",2,18,240,1500},
        {"Route de Gremda",1,40,60,3200},{"Rocade km4",3,5,600,2800},{"Ave. des Martyrs",0,55,0,1200},
    };
    m_totalJams=dj.size(); double ts=0; m_totalDelaySec=0;
    for(auto &j:dj){
        m_jamsLayout->insertWidget(m_jamsLayout->count()-1,createJamRow(j.s,j.l,j.sp,j.d,j.ln));
        ts+=j.sp; m_totalDelaySec+=j.d;
        QColor jc; switch(j.l){case 0:jc=QColor(ACCENT);break;case 1:jc=QColor(ACCENT4);break;
            case 2:jc=QColor("#e17055");break;case 3:jc=QColor(ACCENT3);break;default:jc=QColor("#a29bfe");}
        JamSegment sg; sg.lat=cLat+(rng->bounded(200)-100)/100.0*sp;
        sg.lng=cLon+(rng->bounded(200)-100)/100.0*sp;
        sg.level=j.l;sg.speed=j.sp;sg.street=j.s;sg.color=jc;
        m_jamMarkers.append(sg);
    }
    m_avgSpeed=dj.isEmpty()?0:ts/dj.size();
    updateStatsDisplay(); updateMapMarkers();

    m_lastUpdateLabel->setText(QDateTime::currentDateTime().toString("hh:mm:ss · dd/MM/yyyy") + QString::fromUtf8(" · demo"));
    m_statusLabel->setText(QString::fromUtf8("DEMO · %1 alerts · %2 jams").arg(m_totalAlerts).arg(m_totalJams));
    m_statusLabel->setStyleSheet(QString(
        "QLabel{font-size:9px;color:#a29bfe;background:rgba(162,155,254,0.08);"
        "border-radius:8px;padding:4px 10px;border:1px solid rgba(162,155,254,0.2);}"));
    m_radarMap->setInfoText(QString::fromUtf8("%1 · DEMO · %2 alerts · %3 jams")
        .arg(city.name).arg(m_totalAlerts).arg(m_totalJams));
    addStatusMessage(QString::fromUtf8("✓ Demo data loaded"));
}

void RadarWidget::updateMapMarkers()
{
    m_radarMap->clearMarkers();
    for(auto &a:m_alertMarkers) m_radarMap->addAlertMarker(a);
    for(auto &j:m_jamMarkers)   m_radarMap->addJamMarker(j);
    const CityBBox &city=m_cities[m_currentCity];
    m_radarMap->setInfoText(QString::fromUtf8("%1 · %2 alertes · %3 bouchons")
        .arg(city.name).arg(m_totalAlerts).arg(m_totalJams));
}


// ═══════════════════════════════════════════════════════════════
//  Route / Utilities
// ═══════════════════════════════════════════════════════════════

void RadarWidget::searchRoute()
{
    QString from = m_fromEdit->text().trimmed(), to = m_toEdit->text().trimmed();
    if (from.isEmpty() || to.isEmpty()) {
        m_routeNameLabel->setText(QString::fromUtf8("Fill both fields."));
        m_routeNameLabel->setStyleSheet(QString("QLabel{font-size:10px;color:%1;background:transparent;border:none;}").arg(ACCENT3));
        m_routeNameLabel->show(); return;
    }

    if (m_orsApiKey.isEmpty() || m_orsApiKey == "YOUR_ORS_API_KEY") {
        m_routeNameLabel->setText(QString::fromUtf8("⚠ ORS API Key required for routing"));
        m_routeNameLabel->setStyleSheet(QString("QLabel{font-size:10px;color:%1;background:transparent;border:none;}").arg(ACCENT3));
        m_routeNameLabel->show();
        addStatusMessage(QString::fromUtf8("⚠ Inscrivez-vous sur openrouteservice.org/dev/#/signup"));
        return;
    }

    addStatusMessage(QString::fromUtf8("🔍 Geocoding: %1").arg(from));

    // ── Step 1: Geocode "from" via OpenRouteService Pelias ──
    // Doc: openrouteservice.org/dev/#/api-docs/geocode
    QString geoUrlFrom = QString(
        "https://api.openrouteservice.org/geocode/search"
        "?api_key=%1&text=%2&boundary.country=TN&size=1")
        .arg(m_orsApiKey)
        .arg(QString::fromUtf8(QUrl::toPercentEncoding(from)));

    addStatusMessage("GET ORS geocode/search [departure]");

    QNetworkRequest reqFrom;
    reqFrom.setUrl(QUrl(geoUrlFrom));
    reqFrom.setRawHeader("Accept", "application/json");

    QNetworkReply *replyFrom = m_networkManager->get(reqFrom);
    connect(replyFrom, &QNetworkReply::finished, this, [this, replyFrom, to]() {
        replyFrom->deleteLater();

        double fromLat = 0, fromLon = 0;
        QString fromName = QString::fromUtf8("Departure");

        if (replyFrom->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(replyFrom->readAll());
            QJsonArray features = doc.object()["features"].toArray();
            if (!features.isEmpty()) {
                QJsonObject feat = features.first().toObject();
                QJsonArray coords = feat["geometry"].toObject()["coordinates"].toArray();
                if (coords.size() >= 2) {
                    fromLon = coords[0].toDouble();
                    fromLat = coords[1].toDouble();
                }
                fromName = feat["properties"].toObject()["label"].toString(fromName);
                addStatusMessage(QString::fromUtf8("✓ Departure: %1 (%2, %3)")
                    .arg(fromName).arg(fromLat,0,'f',4).arg(fromLon,0,'f',4));
            } else {
                addStatusMessage(QString::fromUtf8("⚠ Departure address not found"));
                m_routeNameLabel->setText(QString::fromUtf8("Departure address not found"));
                m_routeNameLabel->setStyleSheet(QString("QLabel{font-size:10px;color:%1;background:transparent;border:none;}").arg(ACCENT3));
                m_routeNameLabel->show(); return;
            }
        } else {
            addStatusMessage(QString::fromUtf8("✗ Geocode erreur: %1").arg(replyFrom->errorString()));
            m_routeNameLabel->setText(QString::fromUtf8("Geocoding error"));
            m_routeNameLabel->setStyleSheet(QString("QLabel{font-size:10px;color:%1;background:transparent;border:none;}").arg(ACCENT3));
            m_routeNameLabel->show(); return;
        }

        // ── Step 2: Geocode "to" via ORS ──
        QString geoUrlTo = QString(
            "https://api.openrouteservice.org/geocode/search"
            "?api_key=%1&text=%2&boundary.country=TN&size=1")
            .arg(m_orsApiKey)
            .arg(QString::fromUtf8(QUrl::toPercentEncoding(to)));

        addStatusMessage("GET ORS geocode/search [arrival]");

        QNetworkRequest reqTo;
        reqTo.setUrl(QUrl(geoUrlTo));
        reqTo.setRawHeader("Accept", "application/json");

        QNetworkReply *replyTo = m_networkManager->get(reqTo);
        connect(replyTo, &QNetworkReply::finished, this,
            [this, replyTo, fromLat, fromLon, fromName]() {
            replyTo->deleteLater();

            double toLat = 0, toLon = 0;
            QString toName = QString::fromUtf8("Arrival");

            if (replyTo->error() == QNetworkReply::NoError) {
                QJsonDocument doc = QJsonDocument::fromJson(replyTo->readAll());
                QJsonArray features = doc.object()["features"].toArray();
                if (!features.isEmpty()) {
                    QJsonObject feat = features.first().toObject();
                    QJsonArray coords = feat["geometry"].toObject()["coordinates"].toArray();
                    if (coords.size() >= 2) {
                        toLon = coords[0].toDouble();
                        toLat = coords[1].toDouble();
                    }
                    toName = feat["properties"].toObject()["label"].toString(toName);
                    addStatusMessage(QString::fromUtf8("✓ Arrival: %1 (%2, %3)")
                        .arg(toName).arg(toLat,0,'f',4).arg(toLon,0,'f',4));
                } else {
                    addStatusMessage(QString::fromUtf8("⚠ Arrival address not found"));
                    m_routeNameLabel->setText(QString::fromUtf8("Arrival address not found"));
                    m_routeNameLabel->setStyleSheet(QString("QLabel{font-size:10px;color:%1;background:transparent;border:none;}").arg(ACCENT3));
                    m_routeNameLabel->show(); return;
                }
            } else {
                addStatusMessage(QString::fromUtf8("✗ Geocode erreur: %1").arg(replyTo->errorString()));
                return;
            }

            // ── Step 3: Route via OpenRouteService Directions API ──
            // Doc: openrouteservice.org/dev/#/api-docs/v2/directions
            // Note: start/end format is LON,LAT (not LAT,LON!)
            QString routeUrl = QString(
                "https://api.openrouteservice.org/v2/directions/driving-car"
                "?api_key=%1&start=%2,%3&end=%4,%5")
                .arg(m_orsApiKey)
                .arg(fromLon, 0, 'f', 6).arg(fromLat, 0, 'f', 6)
                .arg(toLon, 0, 'f', 6).arg(toLat, 0, 'f', 6);

            addStatusMessage("GET ORS v2/directions/driving-car");

            QNetworkRequest reqRoute;
            reqRoute.setUrl(QUrl(routeUrl));
            reqRoute.setRawHeader("Accept", "application/json");

            QNetworkReply *routeReply = m_networkManager->get(reqRoute);
            connect(routeReply, &QNetworkReply::finished, this,
                [this, routeReply, fromName, toName]() {
                routeReply->deleteLater();

                if (routeReply->error() != QNetworkReply::NoError) {
                    addStatusMessage(QString::fromUtf8("✗ Routing erreur: %1")
                        .arg(routeReply->errorString()));
                    m_routeNameLabel->setText(QString::fromUtf8("Route calculation error"));
                    m_routeNameLabel->setStyleSheet(QString("QLabel{font-size:10px;color:%1;background:transparent;border:none;}").arg(ACCENT3));
                    m_routeNameLabel->show(); return;
                }

                QJsonDocument doc = QJsonDocument::fromJson(routeReply->readAll());
                QJsonObject root = doc.object();

                // ORS response: { "features": [{ "properties": { "summary": { "distance": m, "duration": s } } }] }
                QJsonArray features = root["features"].toArray();
                if (features.isEmpty()) {
                    addStatusMessage(QString::fromUtf8("⚠ No route found"));
                    m_routeNameLabel->setText(QString::fromUtf8("No route found"));
                    m_routeNameLabel->setStyleSheet(QString("QLabel{font-size:10px;color:%1;background:transparent;border:none;}").arg(ACCENT3));
                    m_routeNameLabel->show(); return;
                }

                QJsonObject summary = features.first().toObject()
                    ["properties"].toObject()["summary"].toObject();
                double distM = summary["distance"].toDouble(0);
                double durationSec = summary["duration"].toDouble(0);

                double distKm = distM / 1000.0;
                int timeMin = static_cast<int>(durationSec / 60.0);

                QString routeName = QString::fromUtf8("%1 → %2").arg(fromName, toName);

                m_routeNameLabel->setText(routeName);
                m_routeNameLabel->setStyleSheet(QString(
                    "QLabel{font-size:11px;font-weight:bold;color:%1;"
                    "background:transparent;border:none;}").arg(ACCENT));
                m_routeNameLabel->show();

                m_routeDistLabel->setText(QString::fromUtf8("📏 %1 km").arg(distKm, 0, 'f', 1));
                m_routeDistLabel->setStyleSheet(QString("QLabel{font-size:11px;color:%1;background:transparent;border:none;}").arg(TXT));
                m_routeDistLabel->show();

                m_routeTimeLabel->setText(QString::fromUtf8("⏱ %1 min").arg(timeMin));
                m_routeTimeLabel->setStyleSheet(QString("QLabel{font-size:11px;color:%1;background:transparent;border:none;}").arg(TXT));
                m_routeTimeLabel->show();

                addStatusMessage(QString::fromUtf8("✓ Route ORS: %1 km, %2 min")
                    .arg(distKm,0,'f',1).arg(timeMin));
            });
        });
    });
}

void RadarWidget::onRouteDataReceived(QNetworkReply *r) { r->deleteLater(); }

void RadarWidget::updateStatsDisplay()
{
    m_alertCountLabel->setText(QString::number(m_totalAlerts));
    m_jamCountLabel->setText(QString::number(m_totalJams));
    m_avgSpeedLabel->setText(QString("%1 km/h").arg(m_avgSpeed,0,'f',0));
    int dm=m_totalDelaySec/60;
    m_delayLabel->setText(dm>60?QString("%1h%2").arg(dm/60).arg(dm%60,2,10,QChar('0')):QString("%1 min").arg(dm));
}

void RadarWidget::clearAlerts(){while(m_alertsLayout->count()>1){auto *i=m_alertsLayout->takeAt(0);if(i->widget())i->widget()->deleteLater();delete i;}}
void RadarWidget::clearJams(){while(m_jamsLayout->count()>1){auto *i=m_jamsLayout->takeAt(0);if(i->widget())i->widget()->deleteLater();delete i;}}

void RadarWidget::onCityChanged(int idx){
    if(idx>=0&&idx<m_cities.size()){
        m_currentCity=idx;auto &c=m_cities[idx];
        m_radarMap->setCenter((c.bottom+c.top)/2.0,(c.left+c.right)/2.0);
        m_radarMap->setZoom(14);m_radarMap->clearMarkers();
        m_radarMap->setInfoText(QString::fromUtf8("📍 %1 · Refresh").arg(c.name));
        addStatusMessage(QString::fromUtf8("→ %1").arg(c.name));
    }
}

void RadarWidget::toggleAutoRefresh(){
    m_autoRefresh=!m_autoRefresh;
    if(m_autoRefresh){m_refreshTimer->start();addStatusMessage("AUTO ON 60s");}
    else{m_refreshTimer->stop();addStatusMessage("AUTO OFF");}
}

void RadarWidget::addStatusMessage(const QString &msg){
    m_logConsole->append(QString("<span style='color:#b2bec3'>[%1]</span> %2")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss"), msg));
}

// ═══════════════════════════════════════════════════════════════
//  GPS Tracking Panel
// ═══════════════════════════════════════════════════════════════

QWidget* RadarWidget::createGpsPanel()
{
    QFrame *gpsFrame = new QFrame(this);
    gpsFrame->setObjectName("gpsCard");
    gpsFrame->setStyleSheet(card("gpsCard").replace(CARD_BG, CARD_ALT));
    gpsFrame->setGraphicsEffect(softShadow(gpsFrame));

    QVBoxLayout *cl = new QVBoxLayout(gpsFrame);
    cl->setContentsMargins(20, 16, 20, 16);
    cl->setSpacing(10);

    // ── Title row ──────────────────────────────────────────────
    QHBoxLayout *titleRow = new QHBoxLayout();
    titleRow->setSpacing(10);

    QLabel *gpsIcon = new QLabel(QString::fromUtf8("\xf0\x9f\x93\x8d"), gpsFrame);
    gpsIcon->setStyleSheet("QLabel{font-size:20px;background:transparent;border:none;}");
    titleRow->addWidget(gpsIcon);

    QLabel *title = new QLabel(QString::fromUtf8("GEOLOCATION — STUDENT POSITION"), gpsFrame);
    title->setStyleSheet(QString("QLabel{font-size:11px;font-weight:bold;color:%1;"
        "letter-spacing:2px;background:transparent;border:none;}").arg(ACCENT));
    titleRow->addWidget(title);

    // Source badge
    QLabel *srcLabel = new QLabel(gpsFrame);
    if (m_geoSource)
        srcLabel->setText(QString("Niv.1: Qt·%1  |  Niv.2: IP  |  Niv.3: Manuel")
            .arg(m_geoSource->sourceName()));
    else
        srcLabel->setText("Niv.2: IP/WiFi  |  Niv.3: Manuel");
    srcLabel->setStyleSheet(QString("QLabel{font-size:9px;color:%1;background:transparent;border:none;}").arg(TXT_DIM));
    titleRow->addWidget(srcLabel);
    titleRow->addStretch();

    // Button style helper
    auto btnSS = [&](const QString &bg, const QString &fg, const QString &border,
                     const QString &hov, const QString &checked = "") -> QString {
        QString s = QString(
            "QPushButton{background:%1;color:%2;border:1px solid %3;"
            "border-radius:10px;padding:7px 14px;font-size:11px;font-weight:bold;}"
            "QPushButton:hover{background:%4;}"
            "QPushButton:disabled{color:%3;border-color:%3;}")
            .arg(bg, fg, border, hov);
        if (!checked.isEmpty())
            s += QString("QPushButton:checked{background:%1;border-color:%1;color:white;}").arg(checked);
        return s;
    };

    // Toggle GPS
    m_gpsToggleBtn = new QPushButton(QString::fromUtf8("📡 Enable GPS"), gpsFrame);
    m_gpsToggleBtn->setCursor(Qt::PointingHandCursor);
    m_gpsToggleBtn->setCheckable(true);
    m_gpsToggleBtn->setStyleSheet(btnSS(CARD_BG, TXT, BORDER, CARD_ALT, ACCENT));
    connect(m_gpsToggleBtn, &QPushButton::clicked, this, &RadarWidget::toggleGpsTracking);
    titleRow->addWidget(m_gpsToggleBtn);

    // Center once
    m_gpsCenterBtn = new QPushButton(QString::fromUtf8("\xf0\x9f\x8e\xaf Center"), gpsFrame);
    m_gpsCenterBtn->setCursor(Qt::PointingHandCursor);
    m_gpsCenterBtn->setEnabled(false);
    m_gpsCenterBtn->setStyleSheet(btnSS(CARD_BG, TXT, BORDER, CARD_ALT));
    connect(m_gpsCenterBtn, &QPushButton::clicked, this, [this]() {
        if (m_gpsFound) {
            m_radarMap->setCenter(m_gpsLat, m_gpsLng);
            m_radarMap->setZoom(16);
            addStatusMessage(QString::fromUtf8("\xf0\x9f\x8e\xaf Map centered"));
        }
    });
    titleRow->addWidget(m_gpsCenterBtn);

    // Auto-center toggle
    m_gpsAutoCenterBtn = new QPushButton(QString::fromUtf8("\xf0\x9f\x94\x92 Follow"), gpsFrame);
    m_gpsAutoCenterBtn->setCursor(Qt::PointingHandCursor);
    m_gpsAutoCenterBtn->setCheckable(true);
    m_gpsAutoCenterBtn->setEnabled(false);
    m_gpsAutoCenterBtn->setStyleSheet(btnSS(CARD_BG, TXT, BORDER, CARD_ALT, ACCENT2));
    m_gpsAutoCenterBtn->setToolTip("Keep map centered on my GPS position");
    connect(m_gpsAutoCenterBtn, &QPushButton::clicked, this, &RadarWidget::toggleAutoCenter);
    titleRow->addWidget(m_gpsAutoCenterBtn);

    cl->addLayout(titleRow);

    // ── Status + Coords row ─────────────────────────────────────
    QHBoxLayout *infoRow = new QHBoxLayout();
    infoRow->setSpacing(10);

    m_gpsStatusLabel = new QLabel(QString::fromUtf8("\xe2\x8f\xb8 GPS disabled"), gpsFrame);
    m_gpsStatusLabel->setStyleSheet(QString(
        "QLabel{font-size:11px;color:%1;background:%2;"
        "border-radius:8px;padding:7px 12px;border:1px solid %3;}"
    ).arg(TXT_DIM, CARD_BG, BORDER));
    infoRow->addWidget(m_gpsStatusLabel);

    m_gpsCoordsLabel = new QLabel(QString::fromUtf8("Lat: \xe2\x80\x94  |  Lon: \xe2\x80\x94"), gpsFrame);
    m_gpsCoordsLabel->setStyleSheet(QString(
        "QLabel{font-size:10px;color:%1;background:%2;"
        "border-radius:8px;padding:7px 12px;border:1px solid %3;"
        "font-family:'Consolas','Courier New',monospace;}"
    ).arg(TXT_DIM, CARD_BG, BORDER));
    infoRow->addWidget(m_gpsCoordsLabel, 1);

    m_gpsSpeedLabel = new QLabel(QString::fromUtf8("\xe2\x80\x94 km/h"), gpsFrame);
    m_gpsSpeedLabel->setAlignment(Qt::AlignCenter);
    m_gpsSpeedLabel->setFixedWidth(90);
    m_gpsSpeedLabel->setStyleSheet(QString(
        "QLabel{font-size:14px;font-weight:bold;color:%1;background:%2;"
        "border-radius:8px;padding:7px;border:1px solid %3;}"
    ).arg(ACCENT, CARD_BG, BORDER));
    infoRow->addWidget(m_gpsSpeedLabel);

    m_gpsAccuracyLabel = new QLabel(QString::fromUtf8("\xf0\x9f\x93\x8f \xe2\x80\x94"), gpsFrame);
    m_gpsAccuracyLabel->setStyleSheet(QString(
        "QLabel{font-size:10px;color:%1;font-weight:bold;background:transparent;border:none;}"
    ).arg(TXT_DIM));
    infoRow->addWidget(m_gpsAccuracyLabel);

    // Last fix timestamp
    m_gpsLastFixLabel = new QLabel(QString::fromUtf8("\xe2\x8f\xb1 No fix"), gpsFrame);
    m_gpsLastFixLabel->setStyleSheet(QString(
        "QLabel{font-size:9px;color:%1;background:transparent;border:none;}"
    ).arg(TXT_DIM));
    infoRow->addWidget(m_gpsLastFixLabel);

    infoRow->addStretch();
    cl->addLayout(infoRow);

    // ── Level 3: Manual coordinates ─────────────────────────────
    QFrame *manualSection = new QFrame(gpsFrame);
    manualSection->setObjectName("manSec");
    manualSection->setStyleSheet(
        "QFrame#manSec{background:white;border-radius:10px;"
        "border:1px dashed #b2bec3;}");
    QHBoxLayout *manLay = new QHBoxLayout(manualSection);
    manLay->setContentsMargins(14, 10, 14, 10);
    manLay->setSpacing(10);

    QLabel *manIcon = new QLabel(QString::fromUtf8("\xe2\x9c\x8f\xef\xb8\x8f"), manualSection);
    manIcon->setStyleSheet("QLabel{font-size:16px;background:transparent;border:none;}");
    manLay->addWidget(manIcon);

    QLabel *manTitle = new QLabel("NIVEAU 3 — Manuel", manualSection);
    manTitle->setStyleSheet(QString(
        "QLabel{font-size:10px;font-weight:bold;color:%1;"
        "letter-spacing:1px;background:transparent;border:none;}").arg(TXT_DIM));
    manLay->addWidget(manTitle);

    QString editSS = QString(
        "QLineEdit{background:%1;border:1px solid %2;border-radius:8px;"
        "padding:6px 10px;font-size:11px;color:%3;"
        "font-family:'Consolas','Courier New',monospace;}"
        "QLineEdit:focus{border-color:%4;}"
    ).arg(CARD_ALT, BORDER, TXT, ACCENT);

    m_manualLatEdit = new QLineEdit(manualSection);
    m_manualLatEdit->setPlaceholderText("Lat  ex: 34.7406");
    m_manualLatEdit->setFixedWidth(150);
    m_manualLatEdit->setStyleSheet(editSS);
    manLay->addWidget(m_manualLatEdit);

    m_manualLngEdit = new QLineEdit(manualSection);
    m_manualLngEdit->setPlaceholderText("Lon  ex: 10.7603");
    m_manualLngEdit->setFixedWidth(150);
    m_manualLngEdit->setStyleSheet(editSS);
    manLay->addWidget(m_manualLngEdit);

    QPushButton *manApplyBtn = new QPushButton(QString::fromUtf8("\xe2\x9c\x94 Apply"), manualSection);
    manApplyBtn->setCursor(Qt::PointingHandCursor);
    manApplyBtn->setStyleSheet(QString(
        "QPushButton{background:%1;color:white;border:none;"
        "border-radius:8px;padding:6px 16px;font-size:11px;font-weight:bold;}"
        "QPushButton:hover{background:#00cc8e;}").arg(ACCENT));
    connect(manApplyBtn, &QPushButton::clicked, this, &RadarWidget::applyManualCoordinates);
    manLay->addWidget(manApplyBtn);

    QLabel *manHelp = new QLabel(
        QString::fromUtf8("Tape lat/lon pour tester sans GPS r\xc3\xa9el"), manualSection);
    manHelp->setStyleSheet(QString(
        "QLabel{font-size:9px;color:%1;background:transparent;border:none;}").arg(TXT_DIM));
    manLay->addWidget(manHelp);
    manLay->addStretch();

    cl->addWidget(manualSection);

    return gpsFrame;
}

// ═══════════════════════════════════════════════════════════════
//  GPS — Multi-source Location System
//  Level 1: Qt Positioning (GPS/WiFi/Sensors from OS)
//  Level 2: Network geolocation (IP-based — always works)
//  Automatic fallback: if Level 1 fails → Level 2
// ═══════════════════════════════════════════════════════════════

void RadarWidget::toggleGpsTracking()
{
    m_gpsTracking = !m_gpsTracking;

    if (m_gpsTracking) {
        m_gpsToggleBtn->setText(QString::fromUtf8("📡 GPS Active"));
        m_gpsStatusLabel->setText(QString::fromUtf8("⏳ Searching for position..."));
        m_gpsStatusLabel->setStyleSheet(QString(
            "QLabel{font-size:12px;color:%1;background:%2;"
            "border-radius:8px;padding:8px 14px;border:1px solid %1;}")
            .arg(ACCENT4, CARD_BG));

        SoundManager::instance()->play(SoundManager::NavClick);

        // Try Level 1 first (Qt Positioning — real device location)
        if (m_geoSource) {
            m_gpsLevel = 1;
            m_gpsSourceName = m_geoSource->sourceName();
            addStatusMessage(QString::fromUtf8("📡 Niveau 1: Qt Positioning (%1)").arg(m_gpsSourceName));
            m_geoSource->requestUpdate(8000); // 8s timeout for first fix
            m_geoSource->startUpdates();

            // Safety: if no position after 10s → fallback to network
            QTimer::singleShot(10000, this, [this]() {
                if (m_gpsTracking && !m_gpsFound && m_gpsLevel == 1) {
                    addStatusMessage(QString::fromUtf8("⚠ Qt Positioning timeout — network fallback"));
                    m_gpsLevel = 2;
                    fetchGpsViaNetwork();
                    m_gpsTimer->start();
                }
            });
        } else {
            // No Qt Positioning available → go straight to network
            m_gpsLevel = 2;
            m_gpsSourceName = "Network/IP";
            addStatusMessage(QString::fromUtf8("📡 Level 2: Network geolocation (IP)"));
            fetchGpsViaNetwork();
            m_gpsTimer->start();
        }

    } else {
        m_gpsToggleBtn->setText(QString::fromUtf8("📡 Enable GPS"));
        m_gpsStatusLabel->setText(QString::fromUtf8("⏸ GPS disabled"));
        m_gpsStatusLabel->setStyleSheet(QString(
            "QLabel{font-size:12px;color:%1;background:%2;"
            "border-radius:8px;padding:8px 14px;border:1px solid %3;}")
            .arg(TXT_DIM, CARD_BG, BORDER));
        m_gpsCoordsLabel->setText(QString::fromUtf8("Lat: —  |  Lon: —"));
        m_gpsSpeedLabel->setText(QString::fromUtf8("— km/h"));
        if (m_gpsAccuracyLabel)
            m_gpsAccuracyLabel->setText(QString::fromUtf8("📏 —"));

        if (m_geoSource) m_geoSource->stopUpdates();
        m_gpsTimer->stop();
        m_gpsFound = false;
        m_gpsSpeed = 0;
        m_gpsLevel = 0;
        m_radarMap->setGpsPosition(0, 0, false);
        m_radarMap->clearGpsTrail();
        m_gpsTrail.clear();
        m_gpsCenterBtn->setEnabled(false);

        if (m_gpsLastFixLabel)
            m_gpsLastFixLabel->setText(QString::fromUtf8("\xe2\x8f\xb1 No fix"));
        m_gpsAutoCenterBtn->setEnabled(false);
        m_autoCenter = false;
        m_gpsAutoCenterBtn->setChecked(false);
        m_radarMap->setGpsAccuracy(-1);

        addStatusMessage(QString::fromUtf8("\xf0\x9f\x93\xa1 GPS disabled"));
        SoundManager::instance()->play(SoundManager::NavClick);
    }
}

void RadarWidget::fetchGpsPosition()
{
    if (!m_gpsTracking) return;

    if (m_gpsLevel == 1 && m_geoSource) {
        m_geoSource->requestUpdate(8000);
    } else {
        fetchGpsViaNetwork();
    }
}

// ── Level 2: Network geolocation fallback (3 HTTPS services in cascade) ──
void RadarWidget::fetchGpsViaNetwork()
{
    // Service 1: ipwho.is (HTTPS, free, no key)
    // Service 2: ipapi.co (HTTPS, free, 1000/day)
    // Service 3: ip-api.com (HTTP fallback)
    QUrl apiUrl("https://ipwho.is/");
    QNetworkRequest req(apiUrl);
    req.setRawHeader("User-Agent", "Wino-SmartDrivingSchool/1.0");
    req.setRawHeader("Accept", "application/json");
    req.setTransferTimeout(6000);
    m_gpsNam->get(req);
    addStatusMessage(QString::fromUtf8("📡 Geoloc: ipwho.is..."));
}

// ── Level 1 callback: Qt Positioning ──
void RadarWidget::onGeoPositionUpdated(const QGeoPositionInfo &info)
{
    if (!m_gpsTracking) return;

    QGeoCoordinate coord = info.coordinate();
    if (!coord.isValid()) return;

    double speedKmh = 0;
    if (info.hasAttribute(QGeoPositionInfo::GroundSpeed)) {
        m_gpsSpeed = info.attribute(QGeoPositionInfo::GroundSpeed);
        speedKmh = m_gpsSpeed * 3.6;
        if (speedKmh < 0.5) speedKmh = 0;
    }

    double accuracy = -1;
    if (info.hasAttribute(QGeoPositionInfo::HorizontalAccuracy))
        accuracy = info.attribute(QGeoPositionInfo::HorizontalAccuracy);

    if (coord.type() == QGeoCoordinate::Coordinate3D)
        m_gpsAltitude = coord.altitude();

    m_gpsLevel = 1;
    m_gpsSourceName = m_geoSource ? m_geoSource->sourceName() : "Qt";
    applyGpsPosition(coord.latitude(), coord.longitude(), speedKmh, accuracy,
        QString::fromUtf8("📡 %1").arg(m_gpsSourceName));

    // We got a real position — stop network fallback timer if active
    if (m_gpsTimer->isActive() && m_gpsLevel == 1)
        m_gpsTimer->stop();
}

void RadarWidget::onGeoError(QGeoPositionInfoSource::Error error)
{
    if (!m_gpsTracking) return;

    QString errMsg;
    switch (error) {
    case QGeoPositionInfoSource::AccessError:
        errMsg = QString::fromUtf8("Location access denied");
        break;
    case QGeoPositionInfoSource::ClosedError:
        errMsg = QString::fromUtf8("Location service closed");
        break;
    default:
        errMsg = QString::fromUtf8("Erreur Qt Positioning (%1)").arg(static_cast<int>(error));
        break;
    }

    addStatusMessage(QString::fromUtf8("⚠ Niveau 1: %1").arg(errMsg));

    // Auto-fallback to Level 2
    if (m_gpsLevel != 2) {
        m_gpsLevel = 2;
        m_gpsSourceName = "Network/IP";
        addStatusMessage(QString::fromUtf8("📡 Fallback → Level 2: Network geolocation"));
        fetchGpsViaNetwork();
        m_gpsTimer->start();
    }
}

// ── Level 2 callback: Network geolocation with cascade ──
void RadarWidget::onNetworkGpsReceived(QNetworkReply *reply)
{
    reply->deleteLater();
    if (!m_gpsTracking) return;

    QString url = reply->url().toString();

    if (reply->error() != QNetworkReply::NoError) {
        addStatusMessage(QString::fromUtf8("⚠ %1: %2").arg(reply->url().host(), reply->errorString()));

        // Cascade to next service
        if (url.contains("ipwho.is")) {
            addStatusMessage(QString::fromUtf8("📡 Fallback → ipapi.co..."));
            QNetworkRequest req(QUrl("https://ipapi.co/json/"));
            req.setRawHeader("User-Agent", "Wino/1.0");
            req.setTransferTimeout(6000);
            m_gpsNam->get(req);
            return;
        } else if (url.contains("ipapi.co")) {
            addStatusMessage(QString::fromUtf8("📡 Fallback → ip-api.com (HTTP)..."));
            QNetworkRequest req(QUrl("http://ip-api.com/json/?fields=status,city,lat,lon"));
            req.setRawHeader("User-Agent", "Wino/1.0");
            m_gpsNam->get(req);
            return;
        }

        // All services failed
        m_gpsStatusLabel->setText(QString::fromUtf8("❌ Geolocation unavailable — check internet"));
        m_gpsStatusLabel->setStyleSheet(QString(
            "QLabel{font-size:11px;color:%1;background:%2;"
            "border-radius:8px;padding:8px 14px;border:1px solid %1;}")
            .arg(ACCENT3, CARD_BG));
        return;
    }

    QByteArray rawData = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(rawData);
    QJsonObject obj = doc.object();

    double lat = 0, lon = 0;
    QString city;

    // Parse response based on service
    if (url.contains("ipwho.is")) {
        if (!obj["success"].toBool(false)) {
            addStatusMessage(QString::fromUtf8("⚠ ipwho.is: failure → ipapi.co..."));
            QNetworkRequest req(QUrl("https://ipapi.co/json/"));
            req.setRawHeader("User-Agent", "Wino/1.0");
            req.setTransferTimeout(6000);
            m_gpsNam->get(req);
            return;
        }
        lat = obj["latitude"].toDouble();
        lon = obj["longitude"].toDouble();
        city = obj["city"].toString();
    } else if (url.contains("ipapi.co")) {
        if (obj.contains("error") && obj["error"].toBool(false)) {
            addStatusMessage(QString::fromUtf8("⚠ ipapi.co: limite → ip-api.com..."));
            QNetworkRequest req(QUrl("http://ip-api.com/json/?fields=status,city,lat,lon"));
            req.setRawHeader("User-Agent", "Wino/1.0");
            m_gpsNam->get(req);
            return;
        }
        lat = obj["latitude"].toDouble();
        lon = obj["longitude"].toDouble();
        city = obj["city"].toString();
    } else {
        // ip-api.com format
        if (obj["status"].toString() != "success") {
            addStatusMessage(QString::fromUtf8("⚠ Tous les services faileds"));
            return;
        }
        lat = obj["lat"].toDouble();
        lon = obj["lon"].toDouble();
        city = obj["city"].toString();
    }

    if (lat == 0 && lon == 0) {
        addStatusMessage(QString::fromUtf8("⚠ Coordonnées invalides (0,0)"));
        return;
    }

    m_gpsCity = city;

    // ── Haversine formula for accurate distance → speed estimation ──
    double speedKmh = 0;
    if (m_gpsFound) {
        const double R = 6371.0; // Earth radius km
        double phi1 = qDegreesToRadians(m_gpsLat);
        double phi2 = qDegreesToRadians(lat);
        double dPhi = qDegreesToRadians(lat - m_gpsLat);
        double dLam = qDegreesToRadians(lon - m_gpsLng);
        double a = qSin(dPhi/2)*qSin(dPhi/2)
                 + qCos(phi1)*qCos(phi2)*qSin(dLam/2)*qSin(dLam/2);
        double distKm = 2.0 * R * qAsin(qSqrt(a));
        // 8s interval between network polls
        speedKmh = (distKm / 8.0) * 3600.0;
        if (speedKmh < 1.0) speedKmh = 0;
    }

    double accuracy = 3000; // ~3 km for IP geolocation
    QString src = reply->url().host();

    addStatusMessage(QString::fromUtf8("✓ %1: %2 (%3, %4)").arg(src, city)
        .arg(lat, 0, 'f', 4).arg(lon, 0, 'f', 4));

    applyGpsPosition(lat, lon, speedKmh, accuracy,
        QString::fromUtf8("🌐 %1 (%2)").arg(city, src));
}

// ── Common: Apply GPS position to UI and map ──
void RadarWidget::applyGpsPosition(double lat, double lon, double speedKmh,
                                    double accuracy, const QString &source)
{
    bool firstFix = !m_gpsFound;
    m_gpsFound = true;
    m_gpsLat = lat;
    m_gpsLng = lon;
    m_gpsAccuracy = accuracy;

    // Update map
    m_radarMap->setGpsPosition(lat, lon, true);
    m_radarMap->addGpsTrailPoint(lat, lon);
    m_gpsTrail.append(QPointF(lat, lon));
    if (m_gpsTrail.size() > 1000) m_gpsTrail.removeFirst();

    // ── Update accuracy circle on map ──
    m_radarMap->setGpsAccuracy(accuracy);

    // ── Record time of fix ──
    m_lastGpsFix = QDateTime::currentDateTime();
    if (m_gpsLastFixLabel)
        m_gpsLastFixLabel->setText(
            QString::fromUtf8("\xe2\x8f\xb1 %1").arg(m_lastGpsFix.toString("hh:mm:ss")));

    // ── Center on first fix OR auto-center ──
    if (firstFix || m_autoCenter) {
        int zoom = firstFix ? (m_gpsLevel == 1 ? 16 : 14) : m_radarMap->zoom();
        m_radarMap->setCenter(lat, lon);
        if (firstFix) m_radarMap->setZoom(zoom);
        if (firstFix) SoundManager::instance()->play(SoundManager::Success);
        if (firstFix) addStatusMessage(QString::fromUtf8("\xe2\x9c\x85 Position found"));
    }

    m_gpsCenterBtn->setEnabled(true);
    m_gpsAutoCenterBtn->setEnabled(true);

    // Status label
    QString levelStr = (m_gpsLevel == 1) ?
        QString::fromUtf8("📡 Exact position") :
        QString::fromUtf8("🌐 Network position — %1").arg(m_gpsCity);

    m_gpsStatusLabel->setText(QString::fromUtf8("✅ %1").arg(levelStr));
    QString statusColor = (m_gpsLevel == 1) ? ACCENT : ACCENT2;
    m_gpsStatusLabel->setStyleSheet(QString(
        "QLabel{font-size:12px;color:%1;background:%2;"
        "border-radius:8px;padding:8px 14px;border:1px solid %1;}")
        .arg(statusColor, CARD_BG));

    // Coordinates
    m_gpsCoordsLabel->setText(
        QString("Lat: %1  |  Lon: %2")
            .arg(lat, 0, 'f', 6).arg(lon, 0, 'f', 6));
    m_gpsCoordsLabel->setStyleSheet(QString(
        "QLabel{font-size:11px;color:%1;background:%2;"
        "border-radius:8px;padding:8px 14px;border:1px solid %3;"
        "font-family:'Consolas','Courier New',monospace;}")
        .arg(TXT, CARD_BG, BORDER));

    // Speed
    m_gpsSpeedLabel->setText(QString("%1 km/h").arg(speedKmh, 0, 'f', 0));

    // Accuracy label
    if (m_gpsAccuracyLabel && accuracy >= 0) {
        QString accColor, accText;
        if (m_gpsLevel == 1 && accuracy < 15) {
            accColor = ACCENT; accText = "Excellent";
        } else if (m_gpsLevel == 1 && accuracy < 100) {
            accColor = ACCENT4; accText = "Good";
        } else if (m_gpsLevel == 2) {
            accColor = ACCENT2; accText = QString::fromUtf8("Network (~%1km)")
                .arg(accuracy / 1000.0, 0, 'f', 0);
        } else {
            accColor = ACCENT3; accText = "Weak";
        }
        m_gpsAccuracyLabel->setText(
            QString::fromUtf8("📏 %1").arg(accText));
        m_gpsAccuracyLabel->setStyleSheet(QString(
            "QLabel{font-size:10px;color:%1;font-weight:bold;"
            "background:transparent;border:none;}").arg(accColor));
    }

    addStatusMessage(QString::fromUtf8("\xf0\x9f\x93\x8d [Niv.%1] %2, %3 \xc2\xb1%4")
        .arg(m_gpsLevel)
        .arg(lat, 0, 'f', 5).arg(lon, 0, 'f', 5)
        .arg(accuracy >= 0 ? QString("%1m").arg(accuracy, 0, 'f', 0) : "?"));
}

// ── Level 3: Apply manually typed coordinates ──────────────────
void RadarWidget::applyManualCoordinates()
{
    bool latOk = false, lonOk = false;
    double lat = m_manualLatEdit->text().trimmed().toDouble(&latOk);
    double lon = m_manualLngEdit->text().trimmed().toDouble(&lonOk);

    if (!latOk || !lonOk || lat < -90 || lat > 90 || lon < -180 || lon > 180) {
        m_gpsStatusLabel->setText(QString::fromUtf8("\xe2\x9a\xa0 Coordonn\xc3\xa9es invalides"));
        m_gpsStatusLabel->setStyleSheet(QString(
            "QLabel{font-size:11px;color:%1;background:%2;"
            "border-radius:8px;padding:7px 12px;border:1px solid %1;}"
        ).arg(ACCENT3, CARD_BG));
        addStatusMessage(QString::fromUtf8("\xe2\x9a\xa0 Niveau 3: coordonn\xc3\xa9es invalides"));
        return;
    }

    m_gpsTracking = true;
    m_gpsLevel   = 3;
    m_gpsSourceName = "Manuel";
    addStatusMessage(QString::fromUtf8(
        "\xe2\x9c\x8f\xef\xb8\x8f Niveau 3: position manuelle (%1, %2)")
        .arg(lat, 0, 'f', 5).arg(lon, 0, 'f', 5));

    applyGpsPosition(lat, lon, 0.0, 50.0,
        QString::fromUtf8("\xe2\x9c\x8f\xef\xb8\x8f Manuel"));

    m_gpsToggleBtn->setChecked(true);
    m_gpsToggleBtn->setText(QString::fromUtf8("\xf0\x9f\x93\xa1 GPS Active"));
    SoundManager::instance()->play(SoundManager::NavClick);
}

// ── Auto-center toggle ─────────────────────────────────────────
void RadarWidget::toggleAutoCenter()
{
    m_autoCenter = !m_autoCenter;
    if (m_autoCenter) {
        m_gpsAutoCenterBtn->setText(QString::fromUtf8("\xf0\x9f\x94\x92 Following"));
        addStatusMessage(QString::fromUtf8("\xf0\x9f\x94\x92 Auto-center ON"));
        // Immediately center if we already have a fix
        if (m_gpsFound) {
            m_radarMap->setCenter(m_gpsLat, m_gpsLng);
        }
    } else {
        m_gpsAutoCenterBtn->setText(QString::fromUtf8("\xf0\x9f\x94\x93 Follow"));
        addStatusMessage(QString::fromUtf8("\xf0\x9f\x94\x93 Auto-center OFF"));
    }
}
