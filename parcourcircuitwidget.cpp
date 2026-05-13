#include "parcourcircuitwidget.h"
#include <QPainter>
#include <QGraphicsDropShadowEffect>
#include <QSqlQuery>
#include <QSqlError>
#include <QHeaderView>
#include <QScrollArea>
#include <QScrollBar>
#include <QSerialPortInfo>
#include <QMessageBox>
#include <QDateTime>
#include <QResizeEvent>

// ─── static helpers ──────────────────────────────────────────────────────────
static QGraphicsDropShadowEffect *makeShadow(QWidget *p, int blur = 18, int a = 20)
{
    auto *s = new QGraphicsDropShadowEffect(p);
    s->setBlurRadius(blur);
    s->setColor(QColor(0, 0, 0, a));
    s->setOffset(0, 3);
    return s;
}

static QString cardSS(const QString &id)
{
    return QString(
        "QFrame#%1{"
        "background:white;"
        "border-radius:16px;"
        "border:1px solid #e2e8f0;"
        "}").arg(id);
}

// ─────────────────────────────────────────────────────────────────────────────
//  CircuitTrackView — photo-based diagram
//  Photo mapping (matches physical circuit illustrations):
//    steps 0-3  → circuit1.jpeg  (full overview, car at START bottom-left)
//    step  4    → circuit2.jpeg  (car at traffic light / Turn Right 192cm)
//    step  5    → circuit3.jpeg  (car on Segment 6 / Doudane 76cm)
//    step  ≥6   → circuit4.jpeg  (car past barrier — session done)
// ─────────────────────────────────────────────────────────────────────────────
CircuitTrackView::CircuitTrackView(QWidget *parent) : QLabel(parent)
{
    setMinimumSize(360, 240);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAlignment(Qt::AlignCenter);
    setStyleSheet("background:#f5ede0;border-radius:12px;");
    setScaledContents(false);

    // Load circuit photos as PNG (always supported by Qt, no plugin needed).
    // Primary: Qt resource :/assets/circuitN.png (alias in assets.qrc)
    // Fallback: filesystem absolute path
    const QString assetDir = "C:/Users/hboug/OneDrive/Desktop/maryem/assets/";

    for (int i = 0; i < 4; ++i) {
        const QString name = QString("circuit%1.png").arg(i + 1);
        if (!m_photos[i].load(QString(":/assets/") + name))
            m_photos[i].load(assetDir + name);
    }

    // Progress bar overlay at bottom (child QLabel)
    m_progressBar = new QLabel(this);
    m_progressBar->setFixedHeight(8);
    m_progressBar->setStyleSheet("background:#e5e7eb;border-radius:4px;");
    m_progressBar->setAttribute(Qt::WA_TransparentForMouseEvents);

    // Step badge overlay in top-left corner
    m_stepOverlay = new QLabel(this);
    m_stepOverlay->setStyleSheet(
        "background:rgba(79,70,229,0.88);color:white;"
        "border-radius:10px;padding:3px 10px;"
        "font-size:11px;font-weight:700;");
    m_stepOverlay->setText(QString::fromUtf8("\xc3\x89tape 1 / 6"));
    m_stepOverlay->adjustSize();
    m_stepOverlay->setAttribute(Qt::WA_TransparentForMouseEvents);

    reloadPixmap();
}

void CircuitTrackView::setStep(int step, int totalSteps)
{
    m_step       = step;
    m_totalSteps = totalSteps;

    // Update step badge
    if (m_stepOverlay) {
        if (step >= totalSteps)
            m_stepOverlay->setText(
                QString::fromUtf8("\xf0\x9f\x8f\x81 Termin\xc3\xa9 !"));
        else
            m_stepOverlay->setText(
                QString(QString::fromUtf8("\xc3\x89tape %1 / %2"))
                    .arg(step + 1).arg(totalSteps));
        m_stepOverlay->adjustSize();
    }

    reloadPixmap();
}

void CircuitTrackView::reloadPixmap()
{
    // Photo changes every 2 steps so user sees progress
    // Steps 0-1 → photo 1 (départ + segment 1)
    // Steps 2-3 → photo 2 (virage gauche + segment 5)
    // Steps 4-5 → photo 3 (virage droit + doudane)
    // Done      → photo 4 (terminé)
    int photoIdx = 0;
    if      (m_step >= m_totalSteps) photoIdx = 3;
    else if (m_step >= 4)            photoIdx = 2;
    else if (m_step >= 2)            photoIdx = 1;
    else                             photoIdx = 0;

    const QPixmap &src = m_photos[photoIdx];

    if (src.isNull()) {
        // Should not happen — photos are loaded from QRC at construction
        setText(QString(QString::fromUtf8("Photo %1 introuvable\n(:/assets/circuit%1.png)"))
                .arg(photoIdx + 1));
    } else {
        QSize target = size().isEmpty() ? QSize(640, 380) : size();
        setPixmap(src.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    // Reposition progress bar and badge
    if (m_progressBar && m_totalSteps > 0) {
        int pw = qMax(20, width() - 20);
        m_progressBar->setGeometry(10, height() - 14, pw, 8);
        double pct = qBound(0.0, (double)m_step / m_totalSteps, 1.0);
        // Gradient: indigo fill up to pct, gray from pct onward
        if (pct < 0.001) {
            m_progressBar->setStyleSheet("background:#e5e7eb;border-radius:4px;");
        } else if (pct > 0.999) {
            m_progressBar->setStyleSheet(
                "background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                "stop:0 #818cf8,stop:1 #6366f1);border-radius:4px;");
        } else {
            m_progressBar->setStyleSheet(
                QString("background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                        "stop:0 #818cf8,"
                        "stop:%1 #6366f1,"
                        "stop:%2 #e5e7eb,"
                        "stop:1 #e5e7eb);border-radius:4px;")
                    .arg(qMax(0.001, pct - 0.001), 0, 'f', 4)
                    .arg(qMin(0.999, pct + 0.001), 0, 'f', 4));
        }
        m_progressBar->raise();
    }

    if (m_stepOverlay) {
        m_stepOverlay->move(10, 10);
        m_stepOverlay->raise();
    }
}

void CircuitTrackView::resizeEvent(QResizeEvent *event)
{
    QLabel::resizeEvent(event);
    reloadPixmap();
}

// ─────────────────────────────────────────────────────────────────────────────
//  ParcourCircuitWidget
// ─────────────────────────────────────────────────────────────────────────────
ParcourCircuitWidget::ParcourCircuitWidget(QWidget *parent)
    : QWidget(parent)
{
    // Define the 6 circuit steps matching the physical track photos
    m_steps = {
        { QString::fromUtf8("D\xc3\xa9part \xe2\x80\x94 Zone de s\xc3\xa9" "curit\xc3\xa9"),
          "PREP",
          QString::fromUtf8("\xe2\x9c\x8b\xc3\x89tape manuelle \xe2\x80\x94 V\xc3\xa9rifiez le v\xc3\xa9hicule puis appuyez sur Next") },

        { "Segment 1 \xe2\x80\x94 Ligne droite 45 cm",
          "SEG1",
          QString::fromUtf8("\xf0\x9f\x9f\xa2 En cours \xe2\x80\x94 avancement automatique via Arduino") },

        { QString::fromUtf8("Virage Gauche \xe2\x80\x94 94 cm"),
          "TURN_L",
          QString::fromUtf8("\xf0\x9f\x9f\xa2 En cours \xe2\x80\x94 avancement automatique via Arduino") },

        { "Segment 5 \xe2\x80\x94 Ligne droite 90 cm",
          "SEG5",
          QString::fromUtf8("\xf0\x9f\x9f\xa2 En cours \xe2\x80\x94 avancement automatique via Arduino") },

        { QString::fromUtf8("Virage Droit \xe2\x80\x94 Feu tricolore 192 cm"),
          "LIGHT_R",
          QString::fromUtf8("\xf0\x9f\x9f\xa2 En cours \xe2\x80\x94 avancement automatique via Arduino") },

        { "Segment 6 \xe2\x80\x94 Doudane 76 cm",
          "FINAL",
          QString::fromUtf8("\xf0\x9f\x8f\x81 Derni\xc3\xa8re \xc3\xa9tape \xe2\x80\x94 appuyez sur \xe2\x9c\x85 Terminer") },
    };
    m_totalSteps = m_steps.size();

    // Timer
    m_tickTimer = new QTimer(this);
    m_tickTimer->setInterval(1000);
    connect(m_tickTimer, &QTimer::timeout, this, &ParcourCircuitWidget::onTimerTick);

    // Retry timer for Arduino reconnect
    m_retryTimer = new QTimer(this);
    m_retryTimer->setInterval(5000);
    connect(m_retryTimer, &QTimer::timeout, this, &ParcourCircuitWidget::onConnectArduino);

    buildUi();
    loadHistory();
}

ParcourCircuitWidget::~ParcourCircuitWidget()
{
    if (m_serial      && m_serial->isOpen())      m_serial->close();
    if (m_serialLight && m_serialLight->isOpen()) m_serialLight->close();
}

// ── UI construction ───────────────────────────────────────────────────────────
void ParcourCircuitWidget::buildUi()
{
    setStyleSheet("background:#f0f2f5;");

    QVBoxLayout *rootLay = new QVBoxLayout(this);
    rootLay->setContentsMargins(0, 0, 0, 0);
    rootLay->setSpacing(0);

    rootLay->addWidget(buildTopBar());

    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(
        "QScrollArea{border:none;background:#f0f2f5;}"
        "QScrollBar:vertical{width:6px;background:transparent;}"
        "QScrollBar::handle:vertical{background:#d0d5db;border-radius:3px;min-height:30px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}");

    QWidget *body = new QWidget;
    body->setStyleSheet("background:#f0f2f5;");
    QVBoxLayout *bodyLay = new QVBoxLayout(body);
    bodyLay->setContentsMargins(24, 20, 24, 30);
    bodyLay->setSpacing(18);

    QHBoxLayout *topRow = new QHBoxLayout;
    topRow->setSpacing(16);
    topRow->addWidget(buildSessionCard(), 3);
    topRow->addWidget(buildArduinoCard(), 2);
    bodyLay->addLayout(topRow);

    bodyLay->addWidget(buildHistoryCard());
    bodyLay->addStretch();

    scroll->setWidget(body);
    rootLay->addWidget(scroll, 1);

    m_floatingMsg = new QLabel(this);
    m_floatingMsg->setAlignment(Qt::AlignCenter);
    m_floatingMsg->setWordWrap(false);
    m_floatingMsg->hide();

    updateStepUI();
}

QWidget *ParcourCircuitWidget::buildTopBar()
{
    QWidget *bar = new QWidget(this);
    bar->setFixedHeight(56);
    bar->setStyleSheet(
        "background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "stop:0 #4f46e5,stop:1 #7c3aed);"
        "border-bottom:1px solid rgba(255,255,255,0.15);");

    QHBoxLayout *h = new QHBoxLayout(bar);
    h->setContentsMargins(20, 0, 20, 0);
    h->setSpacing(10);

    QLabel *icon = new QLabel(QString::fromUtf8("\xf0\x9f\x8f\x81"));
    icon->setStyleSheet("font-size:22px;background:transparent;");

    QLabel *title = new QLabel(
        QString::fromUtf8("Circuit Course \xe2\x80\x94 Instructor Dashboard"));
    title->setStyleSheet(
        "font-size:16px;font-weight:700;color:white;background:transparent;");

    m_timerLabel = new QLabel("00:00");
    m_timerLabel->setStyleSheet(
        "font-size:18px;font-weight:700;color:#c7d2fe;"
        "background:transparent;font-family:'Courier New';");

    h->addWidget(icon);
    h->addWidget(title);
    h->addStretch();
    h->addWidget(m_timerLabel);

    return bar;
}

QWidget *ParcourCircuitWidget::buildSessionCard()
{
    QFrame *card = new QFrame(this);
    card->setObjectName("sessionCard");
    card->setStyleSheet(cardSS("sessionCard"));
    card->setGraphicsEffect(makeShadow(card));

    QVBoxLayout *lay = new QVBoxLayout(card);
    lay->setContentsMargins(16, 14, 16, 14);
    lay->setSpacing(10);

    // ── Divider ──────────────────────────────────────────────────────────────
    // (Student is selected in CircuitDashboard's top combo — no duplicate here)
    QFrame *divider = new QFrame(card);
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet("background:#e2e8f0;");
    divider->setFixedHeight(1);
    lay->addWidget(divider);

    // Photo-based track view
    m_trackView = new CircuitTrackView(card);
    m_trackView->setMinimumHeight(280);
    m_trackView->setStep(0, m_totalSteps);
    lay->addWidget(m_trackView);

    // Step header row
    QHBoxLayout *stepRow = new QHBoxLayout;
    m_stepLabel = new QLabel(QString::fromUtf8("\xc3\x89tape 1 / 6"));
    m_stepLabel->setStyleSheet(
        "font-size:12px;font-weight:700;color:#6366f1;"
        "background:#eef2ff;padding:4px 10px;border-radius:10px;");
    m_stepName = new QLabel(m_steps[0].label);
    m_stepName->setStyleSheet("font-size:14px;font-weight:700;color:#1e293b;");
    stepRow->addWidget(m_stepLabel);
    stepRow->addWidget(m_stepName, 1);
    lay->addLayout(stepRow);

    // Guidance
    m_guidanceLabel = new QLabel(m_steps[0].hint);
    m_guidanceLabel->setWordWrap(true);
    m_guidanceLabel->setStyleSheet(
        "font-size:12px;color:#475569;"
        "background:#f8fafc;padding:8px 12px;border-radius:8px;"
        "border-left:3px solid #6366f1;");
    lay->addWidget(m_guidanceLabel);

    // Buttons
    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->setSpacing(10);

    m_startBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x9a\xa6 D\xc3\xa9marrer Session"));
    m_startBtn->setStyleSheet(
        "QPushButton{background:#10b981;color:white;border:none;"
        "border-radius:8px;padding:9px 18px;font-size:13px;font-weight:700;}"
        "QPushButton:hover{background:#059669;}"
        "QPushButton:disabled{background:#d1fae5;color:#6ee7b7;}");
    connect(m_startBtn, &QPushButton::clicked, this, &ParcourCircuitWidget::startSession);

    m_nextBtn = new QPushButton(QString::fromUtf8("Suivant \xe2\x9e\xa1"));
    m_nextBtn->setStyleSheet(
        "QPushButton{background:#6366f1;color:white;border:none;"
        "border-radius:8px;padding:9px 18px;font-size:13px;font-weight:700;}"
        "QPushButton:hover{background:#4f46e5;}"
        "QPushButton:disabled{background:#e0e7ff;color:#a5b4fc;}");
    m_nextBtn->setEnabled(false);
    connect(m_nextBtn, &QPushButton::clicked, this, &ParcourCircuitWidget::onNextClicked);

    m_endBtn = new QPushButton(QString::fromUtf8("\xe2\x9c\x85 Terminer"));
    m_endBtn->setStyleSheet(
        "QPushButton{background:#ef4444;color:white;border:none;"
        "border-radius:8px;padding:9px 18px;font-size:13px;font-weight:700;}"
        "QPushButton:hover{background:#dc2626;}"
        "QPushButton:disabled{background:#fee2e2;color:#fca5a5;}");
    m_endBtn->setEnabled(false);
    connect(m_endBtn, &QPushButton::clicked, this, &ParcourCircuitWidget::endSession);

    // Mode Examen toggle button
    m_examBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x93\x9d Exam Mode: OFF"), card);
    m_examBtn->setCheckable(true);
    m_examBtn->setChecked(false);
    m_examBtn->setStyleSheet(
        "QPushButton{"
        "  background:#f1f5f9;color:#475569;"
        "  border:2px solid #cbd5e1;"
        "  border-radius:8px;padding:9px 16px;"
        "  font-size:12px;font-weight:700;}"
        "QPushButton:hover{background:#e2e8f0;}"
        "QPushButton:checked{"
        "  background:#7c3aed;color:white;"
        "  border:2px solid #6d28d9;}"
        "QPushButton:checked:hover{background:#6d28d9;}");
    connect(m_examBtn, &QPushButton::toggled, this, [this](bool checked){
        m_examMode = checked;
        m_examBtn->setText(checked
            ? QString::fromUtf8("\xf0\x9f\x93\x9d Exam Mode: ON")
            : QString::fromUtf8("\xf0\x9f\x93\x9d Exam Mode: OFF"));
    });

    btnRow->addWidget(m_startBtn);
    btnRow->addWidget(m_nextBtn);
    btnRow->addWidget(m_examBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_endBtn);
    lay->addLayout(btnRow);

    return card;
}

QWidget *ParcourCircuitWidget::buildArduinoCard()
{
    QFrame *card = new QFrame(this);
    card->setObjectName("arduinoCard");
    card->setStyleSheet(
        "QFrame#arduinoCard{"
        "background:qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "stop:0 #1e1b4b,stop:1 #312e81);"
        "border-radius:16px;border:1px solid rgba(99,102,241,0.4);}");
    card->setGraphicsEffect(makeShadow(card, 22, 28));

    QVBoxLayout *lay = new QVBoxLayout(card);
    lay->setContentsMargins(16, 14, 16, 14);
    lay->setSpacing(10);

    QLabel *hdr = new QLabel(
        QString::fromUtf8("\xf0\x9f\x94\x8c  Connexion Arduino HC-05"));
    hdr->setStyleSheet(
        "font-size:13px;font-weight:700;color:#c7d2fe;background:transparent;");
    lay->addWidget(hdr);

    QHBoxLayout *statusRow = new QHBoxLayout;
    m_connStatus = new QLabel(
        QString::fromUtf8("\xf0\x9f\x94\xb4 D\xc3\xa9" "connect\xc3\xa9"));
    m_connStatus->setStyleSheet("font-size:11px;color:#a5b4fc;background:transparent;");

    m_connectBtn = new QPushButton(
        QString::fromUtf8("Connecter Maquette"));
    m_connectBtn->setStyleSheet(
        "QPushButton{background:#4f46e5;color:white;border:none;"
        "border-radius:6px;padding:6px 12px;font-size:11px;font-weight:600;}"
        "QPushButton:hover{background:#4338ca;}");
    connect(m_connectBtn, &QPushButton::clicked,
            this, &ParcourCircuitWidget::onConnectArduino);

    // ── Mode buttons: Neighborhood (1) and Wide Street (2) ───────────────────
    // Two exclusive toggle buttons — active one is highlighted teal/indigo
    QPushButton *btnNeighbor = new QPushButton("Neighborhood", card);
    QPushButton *btnWide     = new QPushButton("Wide Street",  card);

    const QString modeSSActive =
        "QPushButton{color:white;border:none;"
        "border-radius:6px;padding:6px 10px;font-size:11px;font-weight:700;}";
    const QString modeSSInactive =
        "QPushButton{color:#94a3b8;border:1px solid rgba(99,102,241,0.35);"
        "background:transparent;"
        "border-radius:6px;padding:6px 10px;font-size:11px;font-weight:600;}"
        "QPushButton:hover{background:rgba(99,102,241,0.15);color:#c7d2fe;}";

    // Neighborhood starts active (mode 1 is default)
    btnNeighbor->setStyleSheet(modeSSActive +
        "QPushButton{background:#0f766e;}"
        "QPushButton:hover{background:#0d9488;}");
    btnWide->setStyleSheet(modeSSInactive);

    // Neighborhood clicked → send '1', highlight it, dim Wide Street
    connect(btnNeighbor, &QPushButton::clicked, this, [=]{
        m_activeMode = '1';
        sendToArduino('1');
        btnNeighbor->setStyleSheet(modeSSActive +
            "QPushButton{background:#0f766e;}"
            "QPushButton:hover{background:#0d9488;}");
        btnWide->setStyleSheet(modeSSInactive);
        static qint64 last = 0; qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - last > 2000) {
            appendArduinoMessage(
                QString::fromUtf8(">>> Mode Neighborhood (1) envoy\xc3\xa9 <<<"));
            last = now;
        }
    });

    // Wide Street clicked → send '2', highlight it, dim Neighborhood
    connect(btnWide, &QPushButton::clicked, this, [=]{
        m_activeMode = '2';
        sendToArduino('2');
        btnWide->setStyleSheet(modeSSActive +
            "QPushButton{background:#4f46e5;}"
            "QPushButton:hover{background:#4338ca;}");
        btnNeighbor->setStyleSheet(modeSSInactive);
        static qint64 last = 0; qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - last > 2000) {
            appendArduinoMessage(
                QString::fromUtf8(">>> Mode Wide Street (2) envoy\xc3\xa9 <<<"));
            last = now;
        }
    });

    statusRow->addWidget(m_connStatus, 1);
    statusRow->addWidget(btnNeighbor);
    statusRow->addWidget(btnWide);
    statusRow->addWidget(m_connectBtn);
    lay->addLayout(statusRow);

    m_arduinoLog = new QTextEdit(card);
    m_arduinoLog->setReadOnly(true);
    m_arduinoLog->setMinimumHeight(200);
    m_arduinoLog->setStyleSheet(
        "QTextEdit{"
        "background:#0f172a;color:#e2e8f0;"
        "border:1px solid rgba(99,102,241,0.3);"
        "border-radius:8px;"
        "font-family:'Courier New',monospace;font-size:10px;padding:6px;}"
        "QScrollBar:vertical{width:4px;background:transparent;}"
        "QScrollBar::handle:vertical{background:#4f46e5;border-radius:2px;}");
    m_arduinoLog->setPlaceholderText(
        QString::fromUtf8("Waiting for Arduino connection...\nPort: COM3 (HC-05 Bluetooth)"));
    lay->addWidget(m_arduinoLog, 1);

    // ── Live sensor bar ──────────────────────────────────────────────────────
    m_sensorBar = new QLabel(
        QString::fromUtf8("L: —  |  R: —  |  bZ: —  |  MPU: —"), card);
    m_sensorBar->setAlignment(Qt::AlignCenter);
    m_sensorBar->setStyleSheet(
        "color:#94a3b8;font-size:10px;font-family:'Courier New';"
        "background:#0f172a;padding:4px;border-radius:4px;");
    lay->addWidget(m_sensorBar);

    // ── Alert banner (ERREUR / AVERT) ────────────────────────────────────────
    m_alertBanner = new QLabel("", card);
    m_alertBanner->setAlignment(Qt::AlignCenter);
    m_alertBanner->setWordWrap(true);
    m_alertBanner->setStyleSheet(
        "color:#fca5a5;font-size:11px;font-weight:700;"
        "background:#450a0a;padding:5px;border-radius:6px;"
        "border:1px solid #ef4444;");
    m_alertBanner->hide();
    lay->addWidget(m_alertBanner);

    // ── Error counter row ────────────────────────────────────────────────────
    QHBoxLayout *errRow = new QHBoxLayout;
    QLabel *errLbl = new QLabel(QString::fromUtf8("Fautes session :"), card);
    errLbl->setStyleSheet("color:#94a3b8;font-size:10px;background:transparent;");
    m_errorCountLbl = new QLabel("0", card);
    m_errorCountLbl->setStyleSheet(
        "color:#f87171;font-size:13px;font-weight:700;background:transparent;");
    errRow->addStretch();
    errRow->addWidget(errLbl);
    errRow->addWidget(m_errorCountLbl);
    lay->addLayout(errRow);

    // ════════════════════════════════════════════════════════════════════════
    // ── Traffic light Arduino section ────────────────────────────────────────
    // ════════════════════════════════════════════════════════════════════════
    QFrame *sep = new QFrame(card);
    sep->setFrameShape(QFrame::HLine);
    sep->setFixedHeight(1);
    sep->setStyleSheet("background:rgba(99,102,241,0.35);");
    lay->addWidget(sep);

    QLabel *lightHdr = new QLabel(
        QString::fromUtf8("\xf0\x9f\x9a\xa6  Feu de Route \xe2\x80\x94 Capteur IR"));
    lightHdr->setStyleSheet(
        "font-size:12px;font-weight:700;color:#c7d2fe;background:transparent;");
    lay->addWidget(lightHdr);

    // Status row: indicator + status text + port combo + connect button
    QHBoxLayout *lightRow = new QHBoxLayout;
    lightRow->setSpacing(6);

    m_lightIndicator = new QLabel(QString::fromUtf8("\xe2\x9a\xab"), card);  // ⚫ = not connected
    m_lightIndicator->setStyleSheet("font-size:16px;background:transparent;");
    m_lightIndicator->setFixedWidth(22);

    m_lightStatus = new QLabel(QString::fromUtf8("D\xc3\xa9connect\xc3\xa9"), card);
    m_lightStatus->setStyleSheet("font-size:10px;color:#a5b4fc;background:transparent;");

    // Port selection combo (auto-populated, excludes COM3 which is the sensor Arduino)
    QComboBox *lightPortCombo = new QComboBox(card);
    lightPortCombo->setFixedWidth(72);
    lightPortCombo->setStyleSheet(
        "QComboBox{background:#1e1b4b;color:#c7d2fe;"
        "border:1px solid rgba(99,102,241,0.4);"
        "border-radius:4px;padding:2px 4px;font-size:10px;}"
        "QComboBox::drop-down{border:none;}"
        "QComboBox QAbstractItemView{"
        "background:#1e1b4b;color:#c7d2fe;border:none;selection-background-color:#4f46e5;}");
    lightPortCombo->addItem("COM4");   // default — change to match your traffic light port
    const auto allPorts = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &pi : allPorts) {
        if (pi.portName() != "COM3" && lightPortCombo->findText(pi.portName()) < 0)
            lightPortCombo->addItem(pi.portName());
    }

    m_connectLightBtn = new QPushButton(
        QString::fromUtf8("Connecter Feu"), card);
    m_connectLightBtn->setStyleSheet(
        "QPushButton{background:#7c3aed;color:white;border:none;"
        "border-radius:6px;padding:5px 10px;font-size:10px;font-weight:600;}"
        "QPushButton:hover{background:#6d28d9;}");
    // Lambda captures the combo so the chosen port is passed to onConnectLight()
    connect(m_connectLightBtn, &QPushButton::clicked,
            this, [this, lightPortCombo]() {
        onConnectLight(lightPortCombo->currentText());
    });

    lightRow->addWidget(m_lightIndicator);
    lightRow->addWidget(m_lightStatus, 1);
    lightRow->addWidget(lightPortCombo);
    lightRow->addWidget(m_connectLightBtn);
    lay->addLayout(lightRow);

    // Red light infraction counter
    QHBoxLayout *redRow = new QHBoxLayout;
    QLabel *redLbl = new QLabel(
        QString::fromUtf8("Passages au rouge :"), card);
    redLbl->setStyleSheet("color:#94a3b8;font-size:10px;background:transparent;");
    m_redErrLbl = new QLabel("0", card);
    m_redErrLbl->setStyleSheet(
        "color:#f87171;font-size:13px;font-weight:700;background:transparent;");
    redRow->addStretch();
    redRow->addWidget(redLbl);
    redRow->addWidget(m_redErrLbl);
    lay->addLayout(redRow);

    return card;
}

QWidget *ParcourCircuitWidget::buildHistoryCard()
{
    QFrame *card = new QFrame(this);
    card->setObjectName("histCard");
    card->setStyleSheet(cardSS("histCard"));
    card->setGraphicsEffect(makeShadow(card));

    QVBoxLayout *lay = new QVBoxLayout(card);
    lay->setContentsMargins(20, 18, 20, 18);
    lay->setSpacing(10);

    QHBoxLayout *titleRow = new QHBoxLayout;
    QLabel *title = new QLabel(
        QString::fromUtf8("\xf0\x9f\x93\x8a  Session History \xe2\x80\x94 Circuit Course"));
    title->setStyleSheet("font-size:14px;font-weight:700;color:#1e293b;");

    QPushButton *refreshBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\x84 Refresh"));
    refreshBtn->setStyleSheet(
        "QPushButton{background:#f1f5f9;color:#475569;border:1px solid #e2e8f0;"
        "border-radius:6px;padding:5px 12px;font-size:11px;}"
        "QPushButton:hover{background:#e2e8f0;}");
    connect(refreshBtn, &QPushButton::clicked,
            this, &ParcourCircuitWidget::loadHistory);

    titleRow->addWidget(title);
    titleRow->addStretch();
    titleRow->addWidget(refreshBtn);
    lay->addLayout(titleRow);

    // ── Stats summary row (filled by loadHistory) ─────────────────────────────
    QHBoxLayout *statsRow = new QHBoxLayout;
    statsRow->setSpacing(10);

    struct StatCard { QString id; QString icon; QString label; QString dflt; };
    QVector<StatCard> scards = {
        { "sc_sessions", QString::fromUtf8("\xf0\x9f\x93\x8b"), "Sessions",  "0"    },
        { "sc_avg",      QString::fromUtf8("\xf0\x9f\x8f\x86"), QString::fromUtf8("Score Moy."), "--"   },
        { "sc_best",     QString::fromUtf8("\xe2\xad\x90"),      "Meilleur",  "--"   },
        { "sc_stress",   QString::fromUtf8("\xf0\x9f\x92\x93"), "Stress Moy.", "--"  },
        { "sc_anxiety",  QString::fromUtf8("\xf0\x9f\x98\x9f"), QString::fromUtf8("Anx. Moy."), "--" },
        { "sc_errors",   QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f"), "Total Fautes", "0" },
    };
    for (const StatCard &sc : scards) {
        QFrame *f = new QFrame(card);
        f->setObjectName(sc.id + "_frame");
        f->setStyleSheet(
            "QFrame{background:#f8fafc;border-radius:10px;border:1px solid #e2e8f0;}");
        QVBoxLayout *fl = new QVBoxLayout(f);
        fl->setContentsMargins(10, 8, 10, 8);
        fl->setSpacing(2);
        QLabel *fIcon = new QLabel(sc.icon);
        fIcon->setStyleSheet("font-size:16px;");
        QLabel *fVal = new QLabel(sc.dflt);
        fVal->setObjectName(sc.id + "_val");
        fVal->setStyleSheet("font-size:18px;font-weight:800;color:#1e293b;");
        fVal->setAlignment(Qt::AlignCenter);
        QLabel *fLbl = new QLabel(sc.label);
        fLbl->setStyleSheet("font-size:10px;color:#64748b;");
        fLbl->setAlignment(Qt::AlignCenter);
        fl->addWidget(fIcon, 0, Qt::AlignCenter);
        fl->addWidget(fVal);
        fl->addWidget(fLbl);
        statsRow->addWidget(f, 1);
    }
    lay->addLayout(statsRow);

    // ── History table ──────────────────────────────────────────────────────────
    m_historyTable = new QTableWidget(card);
    m_historyTable->setColumnCount(9);
    m_historyTable->setHorizontalHeaderLabels({
        "Date",
        QString::fromUtf8("Dur\xc3\xa9" "e"),
        QString::fromUtf8("\xc3\x89tapes"),
        "Score",
        "Stress",
        "Anxiety",
        "Faults",
        "Status",
        "Examen"
    });
    m_historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_historyTable->setAlternatingRowColors(true);
    m_historyTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_historyTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_historyTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_historyTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_historyTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_historyTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_historyTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    m_historyTable->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    m_historyTable->horizontalHeader()->setSectionResizeMode(8, QHeaderView::ResizeToContents);
    m_historyTable->verticalHeader()->hide();
    m_historyTable->setMinimumHeight(160);
    m_historyTable->setStyleSheet(
        "QTableWidget{border:none;border-radius:8px;font-size:12px;}"
        "QHeaderView::section{background:#f8fafc;font-weight:700;color:#475569;"
        "border:none;border-bottom:2px solid #e2e8f0;padding:6px;}"
        "QTableWidget::item{padding:6px;}"
        "QTableWidget::item:alternate{background:#f8fafc;}");
    lay->addWidget(m_historyTable);

    return card;
}

// ── Step UI update ─────────────────────────────────────────────────────────────
void ParcourCircuitWidget::updateStepUI()
{
    if (m_currentStep >= m_totalSteps) return;

    const CircuitStep &s = m_steps[m_currentStep];

    m_stepLabel->setText(
        QString(QString::fromUtf8("\xc3\x89tape %1 / %2"))
            .arg(m_currentStep + 1).arg(m_totalSteps));
    m_stepName->setText(s.label);
    m_trackView->setStep(m_currentStep, m_totalSteps);

    bool isPrep = (s.sensorPhase == "PREP");
    bool isDone = (s.sensorPhase == "FINAL") || (m_currentStep == m_totalSteps - 1);

    if (isPrep) {
        m_guidanceLabel->setStyleSheet(
            "font-size:12px;color:#1e40af;"
            "background:#eff6ff;padding:8px 12px;border-radius:8px;"
            "border-left:3px solid #3b82f6;");
        m_guidanceLabel->setText(s.hint);
        m_nextBtn->setEnabled(m_sessionActive);
        m_endBtn->setEnabled(false);
    } else if (isDone) {
        m_guidanceLabel->setStyleSheet(
            "font-size:12px;color:#065f46;"
            "background:#ecfdf5;padding:8px 12px;border-radius:8px;"
            "border-left:3px solid #10b981;");
        m_guidanceLabel->setText(s.hint);
        m_nextBtn->setEnabled(false);
        m_endBtn->setEnabled(m_sessionActive);
        disconnect(m_endBtn, &QPushButton::clicked, this, &ParcourCircuitWidget::endSession);
        connect(m_endBtn,    &QPushButton::clicked, this, &ParcourCircuitWidget::endSession);
    } else {
        m_guidanceLabel->setStyleSheet(
            "font-size:12px;color:#065f46;"
            "background:#ecfdf5;padding:8px 12px;border-radius:8px;"
            "border-left:3px solid #10b981;");
        m_guidanceLabel->setText(s.hint);
        m_nextBtn->setEnabled(false);
        m_endBtn->setEnabled(false);
    }
}

// ── Session control ────────────────────────────────────────────────────────────
void ParcourCircuitWidget::startSession()
{
    m_currentStep        = 0;
    m_sessionActive      = true;
    m_sessionEndHandled  = false;
    m_advancePending     = false;
    m_elapsedSecs        = 0;
    m_frontCloseCount    = 0;
    m_hadFrontClose      = false;
    m_errorCount         = 0;
    // Reset per-session analysis counters + graph data
    m_sessionData.clear();
    m_speedErrors = m_bumpErrors = 0;
    m_curbLeftErrors = m_curbRightErrors = 0;
    m_curbLeftWarnings = m_curbRightWarnings = 0;
    m_sumBumpZ = m_maxBumpZ = 0.0;
    m_bumpCount = m_distCount = 0;
    m_sumLeft = m_sumRight = 0.0;
    m_redLightErrors  = 0;
    m_lightIsRed      = false;
    m_errorLog.clear();          // clear per-session error log for CIRCUIT_ERRORS table
    // Note: m_examMode is NOT reset here — instructor sets it before starting
    m_stepElapsed.start();
    if (m_errorCountLbl) m_errorCountLbl->setText("0");
    if (m_redErrLbl)     m_redErrLbl->setText("0");
    if (m_alertBanner)   m_alertBanner->hide();
    m_elapsed.start();

    m_startBtn->setEnabled(false);
    m_timerLabel->setText("00:00");
    m_tickTimer->start();

    // Do NOT clear the log — keep all Arduino boot messages visible
    updateStepUI();
    showFloatingMessage(
        QString::fromUtf8("\xf0\x9f\x9a\xa6 Session started!"),
        "#10b981");
    appendArduinoMessage(
        QString::fromUtf8(">>> Circuit Course Session started <<<"));
}

void ParcourCircuitWidget::endSession()
{
    if (m_sessionEndHandled) return;
    m_sessionEndHandled = true;
    m_sessionActive     = false;
    m_advancePending    = false;

    m_tickTimer->stop();

    // Force photo 4 (last photo) immediately
    m_trackView->setStep(m_totalSteps, m_totalSteps);
    m_trackView->repaint();   // force immediate repaint so photo 4 appears

    m_startBtn->setEnabled(true);
    m_nextBtn->setEnabled(false);
    m_endBtn->setEnabled(false);

    showFloatingMessage(
        QString::fromUtf8("\xf0\x9f\x8f\x81 Session termin\xc3\xa9" "e \xe2\x80\x94 ") +
        QString("%1 s").arg(m_elapsedSecs),
        "#6366f1");

    saveSessionToDb();
    loadHistory();

    // Show analysis popup after a brief delay (let UI repaint first)
    QTimer::singleShot(700, this, [this]{ showSessionAnalysis(); });
}

void ParcourCircuitWidget::advanceStep()
{
    m_advancePending    = false;
    m_frontCloseCount   = 0;
    m_hadFrontClose     = false;
    m_stepElapsed.start();   // reset step timer on each advance
    if (!m_sessionActive) return;
    if (m_currentStep >= m_totalSteps - 1) {
        endSession();
        return;
    }
    m_currentStep++;
    updateStepUI();
    showFloatingMessage(
        QString::fromUtf8("\xe2\x9c\x85 Segment valid\xc3\xa9 \xe2\x80\x94 ") +
        QString(QString::fromUtf8("\xc3\x89tape %1/%2"))
            .arg(m_currentStep + 1).arg(m_totalSteps),
        "#6366f1");
}

void ParcourCircuitWidget::onNextClicked()
{
    if (!m_sessionActive) return;
    advanceStep();
}

void ParcourCircuitWidget::onTimerTick()
{
    m_elapsedSecs = (int)(m_elapsed.elapsed() / 1000);
    int mm = m_elapsedSecs / 60;
    int ss = m_elapsedSecs % 60;
    m_timerLabel->setText(
        QString("%1:%2")
            .arg(mm, 2, 10, QChar('0'))
            .arg(ss, 2, 10, QChar('0')));
}

// ── Arduino ────────────────────────────────────────────────────────────────────
void ParcourCircuitWidget::onConnectArduino()
{
    if (m_retryTimer->isActive()) m_retryTimer->stop();

    if (!m_serial) {
        m_serial = new QSerialPort(this);
        connect(m_serial, &QSerialPort::readyRead,
                this, &ParcourCircuitWidget::onSerialDataReady);
        connect(m_serial, &QSerialPort::errorOccurred,
                this, &ParcourCircuitWidget::onSerialError);
    }

    if (m_serial->isOpen()) {
        m_serial->close();
        m_arduinoConnected = false;
    }

    m_serial->setPortName("COM3");
    m_serial->setBaudRate(QSerialPort::Baud9600);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    if (m_serial->open(QIODevice::ReadWrite)) {
        m_arduinoConnected = true;
        m_modeWaiting      = true;   // expect mode selection prompt
        m_errorCount       = 0;
        if (m_errorCountLbl) m_errorCountLbl->setText("0");
        if (m_alertBanner)   m_alertBanner->hide();

        m_connStatus->setText(
            QString::fromUtf8("\xf0\x9f\x9f\xa2 Connect\xc3\xa9 \xe2\x80\x94 COM3"));
        m_connectBtn->setText(
            QString::fromUtf8("D\xc3\xa9" "connecter"));
        appendArduinoMessage(
            QString::fromUtf8(">>> Arduino circuit connect\xc3\xa9 sur COM3 <<<"));
        appendArduinoMessage(
            QString::fromUtf8(">>> Envoi mode: ") +
            (m_activeMode == '2'
                ? "2 (Wide Street) <<<"
                : "1 (Neighborhood) <<<"));
        // Keep sending the active mode char every 2s until Arduino responds
        m_arduinoResponded = false;
        if (!m_modeSendTimer) {
            m_modeSendTimer = new QTimer(this);
            m_modeSendTimer->setInterval(2000);
            connect(m_modeSendTimer, &QTimer::timeout, this, [this]{
                if (m_arduinoResponded) {
                    m_modeSendTimer->stop();
                    return;
                }
                sendToArduino(m_activeMode);
                if (m_connStatus)
                    m_connStatus->setText(
                        QString::fromUtf8("\xf0\x9f\x9f\xa2 COM3 \xe2\x80\x94 waiting for Arduino response..."));
            });
        }
        // First attempt after 2s, then every 2s after that
        QTimer::singleShot(2000, this, [this]{
            if (!m_arduinoResponded) {
                sendToArduino(m_activeMode);
                m_modeSendTimer->start();
            }
        });

        // After 10s with no response — show clear diagnostic
        QTimer::singleShot(10000, this, [this]{
            if (!m_arduinoResponded && m_arduinoConnected) {
                appendArduinoMessage(
                    "════════════════════════════════");
                appendArduinoMessage(
                    QString::fromUtf8("ATTENTION: HC-05 connect\xc3\xa9 sur COM3"));
                appendArduinoMessage(
                    QString::fromUtf8("mais aucune donn\xc3\xa9" "e re\xc3\xa7ue."));
                appendArduinoMessage(
                    "Verifiez :");
                appendArduinoMessage(
                    QString::fromUtf8("1) LED HC-05 clignote lentement (2s) ?"));
                appendArduinoMessage(
                    QString::fromUtf8("2) Arduino est aliment\xc3\xa9 (LED ON) ?"));
                appendArduinoMessage(
                    QString::fromUtf8("3) HC-05 branch\xc3\xa9 sur pins 0 et 1 ?"));
                appendArduinoMessage(
                    "════════════════════════════════");
                showFloatingMessage(
                    QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f HC-05 sans r\xc3\xa9ponse \xe2\x80\x94 v\xc3\xa9rifiez le cablage"),
                    "#dc2626");
            }
        });
    } else {
        m_arduinoConnected = false;
        m_connStatus->setText(
            QString::fromUtf8("\xf0\x9f\x94\x84 R\xc3\xa9tentative dans 5s..."));
        appendArduinoMessage(
            QString::fromUtf8("!!! Connexion \xc3\xa9" "chou\xc3\xa9"
                               "e (COM3) \xe2\x80\x94 r\xc3\xa9tentative dans 5s..."));
        m_retryTimer->start();
    }
}

void ParcourCircuitWidget::onSerialDataReady()
{
    QByteArray data = m_serial->readAll();
    if (data.isEmpty()) return;

    // First byte received — Arduino is alive
    if (!m_arduinoResponded) {
        m_arduinoResponded = true;
        if (m_modeSendTimer) m_modeSendTimer->stop();
        if (m_connStatus)
            m_connStatus->setText(
                QString::fromUtf8("\xf0\x9f\x9f\xa2 Connect\xc3\xa9 \xe2\x80\x94 COM3"));

        // Auto-start session immediately on first data
        if (!m_sessionActive) {
            QTimer::singleShot(300, this, [this]{
                if (!m_sessionActive) startSession();
            });
        }
    }

    m_serialBuf += QString::fromLatin1(data);

    while (m_serialBuf.contains('\n')) {
        int idx     = m_serialBuf.indexOf('\n');
        QString line = m_serialBuf.left(idx).trimmed();
        m_serialBuf  = m_serialBuf.mid(idx + 1);
        if (!line.isEmpty())
            appendArduinoMessage(line);
    }
}

void ParcourCircuitWidget::onSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) return;
    m_arduinoConnected = false;
    m_arduinoResponded = false;
    if (m_modeSendTimer) m_modeSendTimer->stop();
    m_connStatus->setText(
        QString::fromUtf8("\xf0\x9f\x94\xb4 D\xc3\xa9" "connect\xc3\xa9"));
    m_connectBtn->setText(QString::fromUtf8("Connecter Maquette"));
    if (!m_retryTimer->isActive()) m_retryTimer->start();
}

// ── Send one byte to the circuit car Arduino ──────────────────────────────────
void ParcourCircuitWidget::sendToArduino(char c)
{
    if (m_serial && m_serial->isOpen()) {
        m_serial->write(&c, 1);
    }
}

// ══════════════════════════════════════════════════════════════════════════════
//  Traffic Light Arduino integration (second serial device)
// ══════════════════════════════════════════════════════════════════════════════

// ── Connect / disconnect the traffic light Arduino ────────────────────────────
void ParcourCircuitWidget::onConnectLight(const QString &port)
{
    if (!m_serialLight) {
        m_serialLight = new QSerialPort(this);
        connect(m_serialLight, &QSerialPort::readyRead,
                this, &ParcourCircuitWidget::onLightDataReady);
    }

    // Toggle: if already open → disconnect
    if (m_serialLight->isOpen()) {
        m_serialLight->close();
        m_lightConnected = false;
        m_lightIsRed     = false;
        if (m_connectLightBtn)
            m_connectLightBtn->setText(QString::fromUtf8("Connecter Feu"));
        if (m_lightStatus)
            m_lightStatus->setText(QString::fromUtf8("D\xc3\xa9" "connect\xc3\xa9"));
        updateLightIndicator();
        appendArduinoMessage(
            QString::fromUtf8(">>> [FEU] D\xc3\xa9" "connect\xc3\xa9 <<<"));
        return;
    }

    // Try to open
    m_serialLight->setPortName(port);
    m_serialLight->setBaudRate(QSerialPort::Baud9600);
    m_serialLight->setDataBits(QSerialPort::Data8);
    m_serialLight->setParity(QSerialPort::NoParity);
    m_serialLight->setStopBits(QSerialPort::OneStop);
    m_serialLight->setFlowControl(QSerialPort::NoFlowControl);

    if (m_serialLight->open(QIODevice::ReadOnly)) {
        m_lightConnected = true;
        if (m_connectLightBtn)
            m_connectLightBtn->setText(
                QString::fromUtf8("D\xc3\xa9" "connecter Feu"));
        if (m_lightStatus)
            m_lightStatus->setText(
                QString::fromUtf8("\xf0\x9f\x9f\xa2 Connect\xc3\xa9 \xe2\x80\x94 ") + port);
        updateLightIndicator();   // shows 🔴 until first phase message
        appendArduinoMessage(
            QString::fromUtf8(">>> [FEU] Feu de Route connect\xc3\xa9 sur ")
            + port + " <<<");
    } else {
        if (m_lightStatus)
            m_lightStatus->setText(
                QString::fromUtf8("\xe2\x9d\x8c Error: ") + m_serialLight->errorString());
        appendArduinoMessage(
            QString::fromUtf8("!!! [LIGHT] Connection failed on ")
            + port + ": " + m_serialLight->errorString());
    }
}

// ── readyRead slot: buffer + dispatch lines ───────────────────────────────────
void ParcourCircuitWidget::onLightDataReady()
{
    if (!m_serialLight) return;
    QByteArray raw = m_serialLight->readAll();
    if (raw.isEmpty()) return;

    m_lightBuf += QString::fromLatin1(raw);
    while (m_lightBuf.contains('\n')) {
        int     idx = m_lightBuf.indexOf('\n');
        QString ln  = m_lightBuf.left(idx).trimmed();
        m_lightBuf  = m_lightBuf.mid(idx + 1);
        if (!ln.isEmpty())
            parseLightLine(ln);
    }
}

// ── Parse one line from the traffic light Arduino ─────────────────────────────
// Expected lines (from your Arduino code):
//   "Phase : ROUGE"             → red light ON
//   "Phase : ROUGE + ORANGE"    → transition (still red)
//   "Phase : VERT"              → green light
//   "Phase : ORANGE"            → orange (countdown)
//   ">>> INFRACTION #N - Passage au rouge !" → red light violation
//   "Voiture detectee (feu vert/orange) - OK" → legal passage detected
//   "=== Feu de route initialise ==="          → startup confirmation
void ParcourCircuitWidget::parseLightLine(const QString &line)
{
    QString ll = line.toLower();

    // ── 1. Traffic light phase update ────────────────────────────────────────
    if (ll.startsWith("phase")) {
        bool wasRed = m_lightIsRed;
        // "ROUGE" or "ROUGE + ORANGE" → red; "VERT" / "ORANGE" → not red
        m_lightIsRed = ll.contains("rouge");
        updateLightIndicator();

        if (m_lightIsRed != wasRed || true) {   // always log phase changes
            QString phaseClr = m_lightIsRed ? "#f87171" : "#4ade80";
            QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
            if (m_arduinoLog) {
                m_arduinoLog->moveCursor(QTextCursor::End);
                m_arduinoLog->insertHtml(
                    QString("<span style='color:#475569;font-size:9px;'>[%1]</span> "
                            "<span style='color:%2;font-size:10px;font-weight:bold;'>"
                            "[FEU] %3</span><br>")
                    .arg(ts, phaseClr, line.toHtmlEscaped()));
                m_arduinoLog->moveCursor(QTextCursor::End);
            }
        }
        return;
    }

    // ── 2. Red light infraction detected by IR sensor ────────────────────────
    if (ll.contains("infraction") || ll.contains("passage au rouge")) {
        m_redLightErrors++;
        m_errorCount++;

        // Append to per-session error log
        {
            CircuitErrorRecord rec;
            rec.type      = "ERREUR";
            rec.category  = "Feu Rouge";
            rec.message   = line;
            rec.step      = m_currentStep;
            rec.stepName  = (m_currentStep < m_totalSteps)
                                ? m_steps[m_currentStep].label
                                : QString::fromUtf8("Hors \xc3\xa9tape");
            rec.timestamp = QDateTime::currentDateTime();
            m_errorLog.append(rec);
        }
        if (m_redErrLbl)     m_redErrLbl->setText(QString::number(m_redLightErrors));
        if (m_errorCountLbl) m_errorCountLbl->setText(QString::number(m_errorCount));

        // Show red alert banner
        if (m_alertBanner) {
            m_alertBanner->setStyleSheet(
                "color:#fca5a5;font-size:11px;font-weight:700;"
                "background:#450a0a;padding:5px;border-radius:6px;"
                "border:2px solid #ef4444;");
            m_alertBanner->setText(
                QString::fromUtf8("\xf0\x9f\x9a\xa8  [FEU ROUGE] INFRACTION \xe2\x80\x94 "
                                  "Passage au rouge ! (#%1)").arg(m_redLightErrors));
            m_alertBanner->show();
            QTimer::singleShot(5000, this, [this]{
                if (m_alertBanner) m_alertBanner->hide();
            });
        }
        showFloatingMessage(
            QString::fromUtf8("\xf0\x9f\x9a\xa8 ERREUR FEU ROUGE !"), "#ef4444");

        // Log in main console with red color
        QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
        if (m_arduinoLog) {
            m_arduinoLog->moveCursor(QTextCursor::End);
            m_arduinoLog->insertHtml(
                QString("<span style='color:#475569;font-size:9px;'>[%1]</span> "
                        "<span style='color:#f87171;font-size:10px;font-weight:bold;'>"
                        "[FEU] ERREUR feu rouge — Passage au rouge ! "
                        "(faute #%2)</span><br>")
                .arg(ts).arg(m_redLightErrors));
            m_arduinoLog->moveCursor(QTextCursor::End);
        }
        return;
    }

    // ── 3. Legal car detection (green or orange light) ───────────────────────
    //    IR sensor saw the car while the light was green → record position
    if ((ll.contains("voiture") && ll.contains("detect")) ||
         ll.contains("feu vert") || ll.contains("ok")) {
        appendArduinoMessage(
            QString::fromUtf8("[FEU] \xe2\x9c\x85 Voiture d\xc3\xa9"
                              "tect\xc3\xa9""e (feu vert/orange) "
                              "\xe2\x80\x94 Position exacte: feu tricolore"));

        // If car is currently on the LIGHT_R step → IR confirms crossing → auto-advance
        if (m_sessionActive && !m_advancePending
                && m_currentStep < m_totalSteps
                && m_steps[m_currentStep].sensorPhase == "LIGHT_R") {
            m_advancePending = true;
            showFloatingMessage(
                QString::fromUtf8(
                    "\xf0\x9f\x9a\xa6 Feu tricolore franchi \xe2\x80\x94 "
                    "segment valid\xc3\xa9 !"),
                "#10b981");
            QTimer::singleShot(400, this, [this]{
                if (m_sessionActive) advanceStep();
            });
        }
        return;
    }

    // ── 4. Startup or test messages — log but don't act on them ─────────────
    if (ll.contains("initialise") || ll.contains("=== feu")) {
        appendArduinoMessage(
            QString::fromUtf8("[FEU] ") + line +
            QString::fromUtf8(" \xe2\x80\x94 pr\xc3\xaat"));
        return;
    }

    // ── 5. Anything else from the traffic light → just log it ───────────────
    if (!ll.isEmpty() && !ll.startsWith("=== test") && !ll.startsWith("posez")) {
        appendArduinoMessage(QString("[FEU] ") + line);
    }
}

// ── Update the traffic light color indicator widget ───────────────────────────
void ParcourCircuitWidget::updateLightIndicator()
{
    if (!m_lightIndicator) return;
    if (!m_lightConnected) {
        m_lightIndicator->setText(QString::fromUtf8("\xe2\x9a\xab"));  // ⚫
        return;
    }
    m_lightIndicator->setText(
        m_lightIsRed
        ? QString::fromUtf8("\xf0\x9f\x94\xb4")   // 🔴 red light
        : QString::fromUtf8("\xf0\x9f\x9f\xa2"));  // 🟢 green/orange light
}

// ── Parse live sensor line + auto-advance steps ────────────────────────────────
// Format: "Mode:N | F:15.2 | L:3.1 | R:2.8 | BR:-1.0 | BL:-1.0 | bZ:0.005 | ..."
void ParcourCircuitWidget::parseSensorLine(const QString &line)
{
    auto extract = [&](const QString &key) -> QString {
        int idx = line.indexOf(key);
        if (idx < 0) return "-";
        int start = idx + key.length();
        int end   = start;
        while (end < line.length() && line[end] != ' ' && line[end] != '|')
            end++;
        return line.mid(start, end - start).trimmed();
    };

    QString F   = extract("F:");
    QString L   = extract("L:");
    QString R   = extract("R:");
    QString bZ  = extract("bZ:");
    QString mpu = extract("MPU:");

    // ── Update sensor bar ────────────────────────────────────────────────────
    if (m_sensorBar) {
        bool lClose = false, rClose = false, fClose = false;
        double lv = L.toDouble(&lClose); lClose = lClose && lv > 0 && lv <= 2.0;
        double rv = R.toDouble(&rClose); rClose = rClose && rv > 0 && rv <= 2.0;
        double fv = F.toDouble(&fClose); fClose = fClose && fv > 0 && fv <= 12.0;

        QString lColor = lClose ? "#f87171" : "#94a3b8";
        QString rColor = rClose ? "#f87171" : "#94a3b8";
        QString fColor = fClose ? "#fb923c" : "#94a3b8";

        m_sensorBar->setText(
            QString("<span style='color:%1'>F: %2</span>"
                    " | <span style='color:%3'>L: %4</span>"
                    " | <span style='color:%5'>R: %6</span>"
                    " | <span style='color:#818cf8'>bZ: %7</span>"
                    " | <span style='color:#34d399'>MPU: %8</span>")
            .arg(fColor, F, lColor, L, rColor, R, bZ, mpu));
    }

    // ── Accumulate sensor data for end-of-session analysis ───────────────────
    if (m_sessionActive) {
        bool bOk = false;
        double bv = bZ.toDouble(&bOk);
        if (bOk && bv >= 0.0) {
            m_sumBumpZ += bv;
            if (bv > m_maxBumpZ) m_maxBumpZ = bv;
            m_bumpCount++;
        }
        bool lOk = false, rOk = false;
        double lVal = L.toDouble(&lOk);
        double rVal = R.toDouble(&rOk);
        if (lOk && lVal > 0.0) { m_sumLeft  += lVal; m_distCount++; }
        if (rOk && rVal > 0.0)   m_sumRight += rVal;

        // ── Build per-tick ProcessedData entry for CircuitDashboard graphs ────
        bool fOk2 = false;
        double fVal = F.toDouble(&fOk2);
        ProcessedData pd;
        pd.timestamp         = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
        pd.accel_magnitude   = bOk ? (bv * 9.81) : 0.0;   // g → m/s²
        pd.gyro_magnitude    = 0.0;
        pd.speed             = 0.0;
        pd.speed_variation   = 0.0;
        // Proximity risk: 0 = safe, 1 = touching. Based on min(L,R) distance
        double minSide = 99.0;
        if (lOk && lVal > 0) minSide = qMin(minSide, lVal);
        if (rOk && rVal > 0) minSide = qMin(minSide, rVal);
        pd.proximity_risk    = (minSide < 99.0) ? qMax(0.0, 1.0 - minSide / 5.0) : 0.0;
        pd.sensor_front      = fOk2 ? fVal : 0.0;
        pd.sensor_side_left  = lOk  ? lVal : 0.0;
        pd.sensor_side_right = rOk  ? rVal : 0.0;
        m_sessionData.append(pd);
    }

    // ── Auto-advance steps ───────────────────────────────────────────────────
    if (!m_sessionActive || m_advancePending || m_currentStep >= m_totalSteps)
        return;

    QString phase = m_steps[m_currentStep].sensorPhase;
    bool    fOk   = false;
    double  fv    = F.toDouble(&fOk);
    bool frontClose = fOk && fv > 0 && fv < 12.0;
    bool frontClear = fOk && fv > 25.0;
    bool fValid     = fOk && fv > 0;             // front sensor actually reading

    // Time spent on this step (milliseconds)
    qint64 stepMs = m_stepElapsed.elapsed();

    // ── STEP TIMINGS (adjust to match your real circuit speed) ───────────────
    // SEG1  45cm  → ~12s   TURN_L 94cm  → ~15s
    // SEG5  30cm  → ~10s   LIGHT_R 192cm → ~20s
    // FINAL 76cm  → end session
    const qint64 SEG_TIMEOUT_MS  = 12000;   // straight segments
    const qint64 TURN_TIMEOUT_MS = 15000;   // turn segments

    auto doAdvance = [&](const QString &msg, const QString &color) {
        m_advancePending = true;
        showFloatingMessage(msg, color);
        QTimer::singleShot(300, this, [this]{ if (m_sessionActive) advanceStep(); });
    };

    // ── PREP → advance immediately on first sensor data ──────────────────────
    if (phase == "PREP") {
        doAdvance(QString::fromUtf8("\xf0\x9f\x9a\xa6 D\xc3\xa9part !"), "#10b981");
    }
    // ── STRAIGHT segments ────────────────────────────────────────────────────
    else if (phase == "SEG1" || phase == "SEG5") {
        if (fValid && frontClose) {
            // Sensor sees wall → advance now
            m_frontCloseCount++;
            if (m_frontCloseCount >= 2)
                doAdvance(QString::fromUtf8("\xf0\x9f\x9f\xa0 Fin segment \xe2\x80\x94 virage"), "#f59e0b");
        } else {
            m_frontCloseCount = 0;
            // Timeout fallback when front sensor not available
            if (!fValid && stepMs > SEG_TIMEOUT_MS)
                doAdvance(QString::fromUtf8("\xe2\x8f\xb1 Segment termin\xc3\xa9 (temps)"), "#6366f1");
        }
    }
    // ── TURN segments ────────────────────────────────────────────────────────
    else if (phase == "TURN_L" || phase == "LIGHT_R") {
        if (fValid) {
            if (frontClose) { m_hadFrontClose = true; m_frontCloseCount = 0; }
            else if (m_hadFrontClose && frontClear) {
                if (++m_frontCloseCount >= 2)
                    doAdvance(QString::fromUtf8("\xe2\x9c\x85 Virage termin\xc3\xa9"), "#6366f1");
            }
        } else if (stepMs > TURN_TIMEOUT_MS) {
            // Timeout fallback
            doAdvance(QString::fromUtf8("\xe2\x8f\xb1 Virage termin\xc3\xa9 (temps)"), "#818cf8");
        }
    }
    // ── FINAL step → auto-end after 20s OR user clicks Terminer ─────────────
    else if (phase == "FINAL") {
        if (stepMs > 20000) {
            m_advancePending = true;
            showFloatingMessage(
                QString::fromUtf8("\xf0\x9f\x8f\x81 Course completed!"), "#4ade80");
            QTimer::singleShot(500, this, [this]{
                if (m_sessionActive) endSession();
            });
        }
    }
}

// ── Handle ERREUR / AVERT messages ────────────────────────────────────────────
void ParcourCircuitWidget::handleCircuitAlert(const QString &line)
{
    bool isError   = line.contains("ERREUR", Qt::CaseInsensitive);
    bool isWarning = line.contains("AVERT",  Qt::CaseInsensitive);
    if (!isError && !isWarning) return;

    // ── Categorize for session analysis ──────────────────────────────────────
    QString ll = line.toLower();
    QString category;
    if (isError) {
        // Detect error type by keywords in the message
        if (ll.contains("vitesse")) {
            m_speedErrors++;
            category = "Vitesse";
        } else if (ll.contains("dos") || ll.contains("ane") || ll.contains("\xc3\xa2ne")) {
            m_bumpErrors++;
            category = QString::fromUtf8("Dos d'\xc3\xa2ne");
        } else if (ll.contains("gauche") || ll.contains("left")) {
            m_curbLeftErrors++;
            category = "Trottoir Gauche";
        } else if (ll.contains("droit") || ll.contains("right")) {
            m_curbRightErrors++;
            category = "Trottoir Droit";
        } else if (ll.contains("trottoir") || ll.contains("curb")) {
            m_curbLeftErrors++;   // unspecified side → count as left
            category = "Trottoir";
        } else {
            m_speedErrors++;      // unknown → fall back to speed category
            category = "Autre";
        }
    } else { // AVERT
        if (ll.contains("gauche") || ll.contains("left")) {
            m_curbLeftWarnings++;
            category = "Proche Trottoir Gauche";
        } else if (ll.contains("droit") || ll.contains("right")) {
            m_curbRightWarnings++;
            category = "Proche Trottoir Droit";
        } else {
            m_curbLeftWarnings++; // unspecified → left
            category = "Proche Trottoir";
        }
    }

    // ── Append to per-session error log (saved to CIRCUIT_ERRORS at end) ─────
    {
        CircuitErrorRecord rec;
        rec.type      = isError ? "ERREUR" : "AVERT";
        rec.category  = category;
        rec.message   = line;
        rec.step      = m_currentStep;
        rec.stepName  = (m_currentStep < m_totalSteps)
                            ? m_steps[m_currentStep].label
                            : QString::fromUtf8("Hors \xc3\xa9tape");
        rec.timestamp = QDateTime::currentDateTime();
        m_errorLog.append(rec);
    }

    if (isError) {
        m_errorCount++;
        if (m_errorCountLbl)
            m_errorCountLbl->setText(QString::number(m_errorCount));

        if (m_alertBanner) {
            m_alertBanner->setStyleSheet(
                "color:#fca5a5;font-size:11px;font-weight:700;"
                "background:#450a0a;padding:5px;border-radius:6px;"
                "border:1px solid #ef4444;");
            m_alertBanner->setText(QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f  ") + line);
            m_alertBanner->show();
        }
        showFloatingMessage(
            QString::fromUtf8("\xf0\x9f\x9a\xa8 ") + line, "#ef4444");

    } else { // AVERT
        if (m_alertBanner) {
            m_alertBanner->setStyleSheet(
                "color:#fde68a;font-size:11px;font-weight:700;"
                "background:#451a03;padding:5px;border-radius:6px;"
                "border:1px solid #f59e0b;");
            m_alertBanner->setText(QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f  ") + line);
            m_alertBanner->show();
        }
        showFloatingMessage(
            QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f ") + line, "#f59e0b");
    }

    // Auto-hide alert banner after 4 seconds
    QTimer::singleShot(4000, this, [this]{
        if (m_alertBanner) m_alertBanner->hide();
    });
}

// ── Arduino message handler ────────────────────────────────────────────────────
// Shows EVERY line from Arduino exactly like the Serial Monitor, with colors.
void ParcourCircuitWidget::appendArduinoMessage(const QString &line)
{
    if (!m_arduinoLog) return;

    QString lw    = line.toLower();
    QString color = "#e2e8f0";   // default: bright white — always visible

    // ── Sensor data line → update sensor bar + show compact ──────────────────
    if (lw.startsWith("mode:") && lw.contains("| f:") && lw.contains("| l:")) {
        parseSensorLine(line);
        // Show compact one-liner instead of the long raw string
        auto ex = [&](const QString &k) -> QString {
            int i = line.indexOf(k); if (i<0) return "—";
            int s=i+k.length(), e=s;
            while(e<line.length() && line[e]!=' ' && line[e]!='|') e++;
            return line.mid(s,e-s).trimmed();
        };
        QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
        m_arduinoLog->moveCursor(QTextCursor::End);
        m_arduinoLog->insertHtml(
            QString("<span style='color:#64748b;font-size:8px;'>[%1]</span> "
                    "<span style='color:#94a3b8;font-size:9px;'>"
                    "F:<b>%2</b>  L:<b>%3</b>  R:<b>%4</b>  "
                    "bZ:<b>%5</b>  MPU:<b>%6</b></span><br>")
            .arg(ts, ex("F:"), ex("L:"), ex("R:"), ex("bZ:"), ex("MPU:")));
        m_arduinoLog->moveCursor(QTextCursor::End);
        return;
    }

    // ── Color coding (everything still printed) ───────────────────────────────
    if (line.startsWith("===")) {
        color = "#38bdf8";                          // separator  → cyan
    } else if (lw.contains("erreur")) {
        color = "#f87171";                          // error      → red
        handleCircuitAlert(line);
    } else if (lw.contains("avert")) {
        color = "#fbbf24";                          // warning    → orange
        handleCircuitAlert(line);
    } else if (lw.contains("systeme pret") || lw.contains("syst") ) {
        color = "#4ade80";                          // ready → green (session starts on first data)
    } else if (lw.contains("mpu ok") || lw.contains("base z") ||
               lw.contains("calibrat") || lw.contains("ne bougez")) {
        color = "#34d399";                          // MPU/calib  → teal
    } else if (lw.contains("choisir mode") || lw.contains("neighborhood") ||
               lw.contains("wide street")  || lw.contains("seuil") ||
               lw.contains("regle")) {
        color = "#a78bfa";                          // mode info  → purple
    } else if (lw.contains(">>>") || lw.contains("<<<")) {
        color = "#38bdf8";                          // app msgs   → cyan
    } else if (lw.contains("first movement") || lw.contains("premier mouvement")) {
        color = "#fb923c";                          // start event→ amber
    }
    // everything else stays #e2e8f0 (white) — fully readable

    QString ts   = QDateTime::currentDateTime().toString("hh:mm:ss");
    m_arduinoLog->moveCursor(QTextCursor::End);
    m_arduinoLog->insertHtml(
        QString("<span style='color:#475569;font-size:9px;'>[%1]</span> "
                "<span style='color:%2;font-size:10px;'>%3</span><br>")
        .arg(ts, color, line.toHtmlEscaped()));
    m_arduinoLog->moveCursor(QTextCursor::End);
}

// ── Floating message ──────────────────────────────────────────────────────────
void ParcourCircuitWidget::showFloatingMessage(const QString &text, const QString &color)
{
    if (!m_floatingMsg) return;
    m_floatingMsg->setText(text);
    m_floatingMsg->setStyleSheet(
        QString("background:%1;color:white;border-radius:20px;"
                "padding:8px 20px;font-size:13px;font-weight:700;"
                "border:2px solid rgba(255,255,255,0.4);").arg(color));
    m_floatingMsg->adjustSize();
    int x = (width() - m_floatingMsg->width()) / 2;
    m_floatingMsg->move(x, 70);
    m_floatingMsg->show();
    m_floatingMsg->raise();
    QTimer::singleShot(2500, m_floatingMsg, &QLabel::hide);
}

// ── DB save ────────────────────────────────────────────────────────────────────
void ParcourCircuitWidget::saveSessionToDb()
{
    QSqlDatabase db = QSqlDatabase::database();

    // Tables CIRCUIT_SESSIONS and CIRCUIT_ERRORS are pre-created in Oracle
    // via the SQL setup script (Oracle XE 11g — uses sequences, not IDENTITY).
    // Nothing to create here.

    // ── Re-compute scores (same formula as showSessionAnalysis) ─────────────────
    int totalErrors   = m_speedErrors + m_bumpErrors
                      + m_curbLeftErrors + m_curbRightErrors;
    int totalWarnings = m_curbLeftWarnings + m_curbRightWarnings;
    int speedControl  = qMax(0, 100 - m_speedErrors * 20 - m_bumpErrors * 15);
    int laneControl   = qMax(0, 100
                         - (m_curbLeftErrors + m_curbRightErrors) * 25
                         - (m_curbLeftWarnings + m_curbRightWarnings) * 8);
    int stressLevel   = qMin(100, (int)(totalErrors * 12 + totalWarnings * 4
                                       + m_maxBumpZ * 50.0 + m_redLightErrors * 8));
    int anxietyLevel  = qMin(100, (m_curbLeftErrors + m_curbRightErrors) * 20
                                + (m_curbLeftWarnings + m_curbRightWarnings) * 10);
    int calmness      = qMax(0, 100 - stressLevel);
    int confidence    = qMax(0, 100 - anxietyLevel);
    int overall       = qMax(0,
        (int)(speedControl * 0.35 + laneControl * 0.35
            + calmness     * 0.15 + confidence  * 0.15)
        - m_redLightErrors * 20);

    // ── INSERT with all columns (id supplied by sequence trigger automatically) ──
    QSqlQuery q(db);
    q.prepare(
        "INSERT INTO CIRCUIT_SESSIONS "
        "(instructor_id, school_id, duration_secs, steps_completed, session_date, status,"
        " overall_score, stress_level, anxiety_level, speed_ctrl, lane_ctrl,"
        " total_errors, total_warns, red_light_errs, mode_examen) "
        "VALUES (:iid,:sid,:dur,:steps,SYSDATE,:status,"
        "        :overall,:stress,:anxiety,:speed,:lane,:errs,:warns,:redlight,:mexamen)");
    q.bindValue(":iid",      m_instructorId);
    q.bindValue(":sid",      m_schoolId);
    q.bindValue(":dur",      m_elapsedSecs);
    q.bindValue(":steps",    m_currentStep + 1);
    q.bindValue(":status",   (m_currentStep >= m_totalSteps - 1) ? "COMPLETED" : "PARTIAL");
    q.bindValue(":overall",  overall);
    q.bindValue(":stress",   stressLevel);
    q.bindValue(":anxiety",  anxietyLevel);
    q.bindValue(":speed",    speedControl);
    q.bindValue(":lane",     laneControl);
    q.bindValue(":errs",     totalErrors);
    q.bindValue(":warns",    totalWarnings);
    q.bindValue(":redlight", m_redLightErrors);
    q.bindValue(":mexamen",  m_examMode ? "OUI" : "NON");

    if (!q.exec()) {
        QString err = q.lastError().text();
        appendArduinoMessage(
            QString::fromUtf8("!!! DB ERREUR sauvegarde: ") + err);
        // Show floating alert so instructor sees the problem
        showFloatingMessage(
            QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f DB save \xc3\xa9"
                              "chou\xc3\xa9 \xe2\x80\x94 voir log"),
            "#dc2626");
    } else {
        appendArduinoMessage(
            QString::fromUtf8(">>> Session sauvegard\xc3\xa9"
                              "e (Score:%1  Stress:%2  Fautes:%3) <<<")
            .arg(overall).arg(stressLevel).arg(totalErrors));

        // ── Retrieve the session_id just inserted, then save all errors ──────
        QSqlQuery idQ(db);
        idQ.exec(
            QString("SELECT NVL(MAX(id),-1) FROM CIRCUIT_SESSIONS "
                    "WHERE instructor_id=%1 AND school_id=%2")
            .arg(m_instructorId).arg(m_schoolId));
        int sessionId = -1;
        if (idQ.next()) sessionId = idQ.value(0).toInt();
        idQ.finish();

        if (sessionId > 0 && !m_errorLog.isEmpty())
            saveErrorsToDb(db, sessionId);
    }
}

// ── Save individual errors to CIRCUIT_ERRORS table ────────────────────────────
// Table is pre-created in Oracle XE 11g via setup SQL script.
// The trigger trg_circuit_errors_id fills id automatically from seq_circuit_errors.
void ParcourCircuitWidget::saveErrorsToDb(const QSqlDatabase &db, int sessionId)
{
    // ── Batch INSERT all error records ────────────────────────────────────────
    QSqlQuery ins(db);
    ins.prepare(
        "INSERT INTO CIRCUIT_ERRORS "
        "(session_id, instructor_id, school_id, student_id, student_name,"
        " error_date, error_type, error_category, error_message, circuit_step, step_name) "
        "VALUES (:sess,:iid,:sid,:stuid,:stuname,"
        "        :edate,:etype,:ecat,:emsg,:estep,:estepname)");

    int saved = 0;
    for (const CircuitErrorRecord &rec : m_errorLog) {
        ins.bindValue(":sess",     sessionId);
        ins.bindValue(":iid",      m_instructorId);
        ins.bindValue(":sid",      m_schoolId);
        if (m_studentId > 0)
            ins.bindValue(":stuid",   m_studentId);
        else
            ins.bindValue(":stuid",   QVariant());   // NULL
        if (!m_studentName.isEmpty())
            ins.bindValue(":stuname", m_studentName);
        else
            ins.bindValue(":stuname", QVariant());   // NULL
        ins.bindValue(":edate",    rec.timestamp.toString("yyyy-MM-dd HH:mm:ss"));
        ins.bindValue(":etype",    rec.type);
        ins.bindValue(":ecat",     rec.category);
        ins.bindValue(":emsg",     rec.message.left(300));
        ins.bindValue(":estep",    rec.step);
        ins.bindValue(":estepname",rec.stepName.left(120));
        if (ins.exec()) saved++;
        ins.finish();
    }

    appendArduinoMessage(
        QString::fromUtf8(">>> %1 erreur(s) sauvegard\xc3\xa9""e(s) dans CIRCUIT_ERRORS <<<")
        .arg(saved));
}

// ── History load ───────────────────────────────────────────────────────────────
void ParcourCircuitWidget::loadHistory()
{
    if (!m_historyTable) return;
    m_historyTable->setRowCount(0);

    QSqlQuery q(QSqlDatabase::database());

    // Try full query (including mode_examen) first
    bool ok = q.exec(
        "SELECT TO_CHAR(session_date,'DD/MM/YYYY HH24:MI'), "
        "       duration_secs, steps_completed, status, "
        "       NVL(overall_score,0),  NVL(stress_level,0), "
        "       NVL(anxiety_level,0), "
        "       NVL(total_errors,0)+NVL(total_warns,0)+NVL(red_light_errs,0),"
        "       NVL(mode_examen,'NON') "
        "FROM CIRCUIT_SESSIONS "
        "ORDER BY session_date DESC");

    if (!ok) {
        // mode_examen not yet present — fall back without it
        q.finish();
        ok = q.exec(
            "SELECT TO_CHAR(session_date,'DD/MM/YYYY HH24:MI'), "
            "       duration_secs, steps_completed, status, "
            "       NVL(overall_score,0), NVL(stress_level,0), "
            "       NVL(anxiety_level,0), "
            "       NVL(total_errors,0)+NVL(total_warns,0)+NVL(red_light_errs,0),'NON' "
            "FROM CIRCUIT_SESSIONS ORDER BY session_date DESC");
    }

    if (!ok) {
        // red_light_errs also missing
        q.finish();
        ok = q.exec(
            "SELECT TO_CHAR(session_date,'DD/MM/YYYY HH24:MI'), "
            "       duration_secs, steps_completed, status, "
            "       NVL(overall_score,0), NVL(stress_level,0), "
            "       NVL(anxiety_level,0), NVL(total_errors,0)+NVL(total_warns,0),'NON' "
            "FROM CIRCUIT_SESSIONS ORDER BY session_date DESC");
    }

    if (!ok) {
        // Analysis columns don't exist yet — fall back to basic columns only
        q.finish();
        ok = q.exec(
            "SELECT TO_CHAR(session_date,'DD/MM/YYYY HH24:MI'), "
            "       duration_secs, steps_completed, status, 0,0,0,0,'NON' "
            "FROM CIRCUIT_SESSIONS ORDER BY session_date DESC");
    }

    if (!ok) {
        // Table doesn't exist at all — show empty state silently
        m_historyTable->insertRow(0);
        m_historyTable->setItem(0, 0, new QTableWidgetItem(
            QString::fromUtf8("Aucune session enregistr\xc3\xa9""e")));
        for (int c = 1; c < 9; ++c)
            m_historyTable->setItem(0, c,
                new QTableWidgetItem(QString::fromUtf8("\xe2\x80\x94")));
        return;
    }

    // ── Accumulators for stats summary ────────────────────────────────────────
    int    totalSessions = 0;
    double sumScore = 0, sumStress = 0, sumAnxiety = 0;
    int    bestScore = -1, totalFautes = 0;

    int row = 0;
    while (q.next()) {
        m_historyTable->insertRow(row);

        QString date    = q.value(0).toString();
        int     dur     = q.value(1).toInt();
        int     steps   = q.value(2).toInt();
        QString stat    = q.value(3).toString();
        int     score   = q.value(4).toInt();
        int     stress  = q.value(5).toInt();
        int     anxiety = q.value(6).toInt();
        int     fautes  = q.value(7).toInt();
        QString examen  = q.value(8).toString();   // "OUI" or "NON"

        totalSessions++;
        sumScore   += score;
        sumStress  += stress;
        sumAnxiety += anxiety;
        totalFautes += fautes;
        if (score > bestScore) bestScore = score;

        QString durStr = QString("%1:%2")
            .arg(dur / 60, 2, 10, QChar('0'))
            .arg(dur % 60, 2, 10, QChar('0'));

        // ── Score color ───────────────────────────────────────────────────────
        QString scoreColor = score >= 80 ? "#10b981"
                           : score >= 60 ? "#f59e0b"
                           : "#ef4444";
        QString stressColor = stress <= 30 ? "#10b981"
                            : stress <= 60 ? "#f59e0b"
                            : "#ef4444";
        QString anxColor    = anxiety <= 30 ? "#10b981"
                            : anxiety <= 60 ? "#f59e0b"
                            : "#ef4444";

        // Col 0 — Date
        m_historyTable->setItem(row, 0, new QTableWidgetItem(date));

        // Col 1 — Durée
        m_historyTable->setItem(row, 1, new QTableWidgetItem(durStr));

        // Col 2 — Étapes
        m_historyTable->setItem(row, 2, new QTableWidgetItem(
            QString("%1/%2").arg(steps).arg(m_totalSteps)));

        // Col 3 — Score Global (colored)
        auto *scoreItem = new QTableWidgetItem(
            score > 0 ? QString("%1/100").arg(score) : QString::fromUtf8("\xe2\x80\x94"));
        scoreItem->setForeground(QColor(scoreColor));
        QFont sf = scoreItem->font(); sf.setBold(true); scoreItem->setFont(sf);
        m_historyTable->setItem(row, 3, scoreItem);

        // Col 4 — Stress
        auto *stressItem = new QTableWidgetItem(
            stress > 0 ? QString("%1%%").arg(stress) : QString::fromUtf8("\xe2\x80\x94"));
        stressItem->setForeground(QColor(stressColor));
        m_historyTable->setItem(row, 4, stressItem);

        // Col 5 — Anxiété
        auto *anxItem = new QTableWidgetItem(
            anxiety > 0 ? QString("%1%%").arg(anxiety) : QString::fromUtf8("\xe2\x80\x94"));
        anxItem->setForeground(QColor(anxColor));
        m_historyTable->setItem(row, 5, anxItem);

        // Col 6 — Fautes totales
        auto *fautesItem = new QTableWidgetItem(
            fautes > 0 ? QString::number(fautes) : "0");
        fautesItem->setForeground(fautes > 0 ? QColor("#ef4444") : QColor("#10b981"));
        fautesItem->setTextAlignment(Qt::AlignCenter);
        m_historyTable->setItem(row, 6, fautesItem);

        // Col 7 — Statut
        auto *si = new QTableWidgetItem(stat);
        si->setForeground(stat == "COMPLETED" ? QColor("#10b981") : QColor("#f59e0b"));
        m_historyTable->setItem(row, 7, si);

        // Col 8 — Mode Examen
        auto *exItem = new QTableWidgetItem(examen);
        exItem->setTextAlignment(Qt::AlignCenter);
        if (examen == "OUI") {
            exItem->setForeground(QColor("#7c3aed"));
            QFont ef = exItem->font(); ef.setBold(true); exItem->setFont(ef);
        } else {
            exItem->setForeground(QColor("#94a3b8"));
        }
        m_historyTable->setItem(row, 8, exItem);

        ++row;
    }

    if (row == 0) {
        m_historyTable->insertRow(0);
        m_historyTable->setItem(0, 0, new QTableWidgetItem(
            QString::fromUtf8("Aucune session enregistr\xc3\xa9" "e")));
        for (int c = 1; c < 9; ++c)
            m_historyTable->setItem(0, c,
                new QTableWidgetItem(QString::fromUtf8("\xe2\x80\x94")));
    }

    // ── Update stats summary cards ────────────────────────────────────────────
    auto setCard = [this](const QString &id, const QString &val, const QString &color = "#1e293b") {
        QLabel *lbl = findChild<QLabel*>(id + "_val");
        if (lbl) {
            lbl->setText(val);
            lbl->setStyleSheet(
                QString("font-size:18px;font-weight:800;color:%1;").arg(color));
        }
    };

    if (totalSessions > 0) {
        int avgScore   = (int)(sumScore   / totalSessions);
        int avgStress  = (int)(sumStress  / totalSessions);
        int avgAnxiety = (int)(sumAnxiety / totalSessions);

        QString avgColor  = avgScore  >= 80 ? "#10b981" : avgScore  >= 60 ? "#f59e0b" : "#ef4444";
        QString stColor   = avgStress <= 30 ? "#10b981" : avgStress <= 60 ? "#f59e0b" : "#ef4444";
        QString anxColor2 = avgAnxiety<= 30 ? "#10b981" : avgAnxiety<= 60 ? "#f59e0b" : "#ef4444";

        setCard("sc_sessions", QString::number(totalSessions), "#6366f1");
        setCard("sc_avg",      QString("%1/100").arg(avgScore),  avgColor);
        setCard("sc_best",     bestScore >= 0 ? QString("%1/100").arg(bestScore) : "--",
                bestScore >= 80 ? "#10b981" : "#f59e0b");
        setCard("sc_stress",   QString("%1%%").arg(avgStress),   stColor);
        setCard("sc_anxiety",  QString("%1%%").arg(avgAnxiety),  anxColor2);
        setCard("sc_errors",
                totalFautes > 0 ? QString::number(totalFautes) : "0",
                totalFautes > 0 ? "#ef4444" : "#10b981");
    } else {
        setCard("sc_sessions", "0",  "#6366f1");
        setCard("sc_avg",      "--", "#94a3b8");
        setCard("sc_best",     "--", "#94a3b8");
        setCard("sc_stress",   "--", "#94a3b8");
        setCard("sc_anxiety",  "--", "#94a3b8");
        setCard("sc_errors",   "0",  "#10b981");
    }
}

// ── Session Analysis Dialog ────────────────────────────────────────────────────
// Computes and displays stress, anxiety and driving scores from Arduino data.
void ParcourCircuitWidget::showSessionAnalysis()
{
    // ── Score computation ────────────────────────────────────────────────────
    int totalErrors   = m_speedErrors + m_bumpErrors
                      + m_curbLeftErrors + m_curbRightErrors;
    int totalWarnings = m_curbLeftWarnings + m_curbRightWarnings;
    // m_redLightErrors tracked separately (traffic violations)

    // Speed Control  100 − (speed errors × 20) − (bump errors × 15)  [0..100]
    int speedControl = qMax(0, 100 - m_speedErrors * 20 - m_bumpErrors * 15);

    // Lane Control   100 − (curb touches × 25) − (warnings × 8)  [0..100]
    int laneControl  = qMax(0, 100
                        - (m_curbLeftErrors + m_curbRightErrors) * 25
                        - (m_curbLeftWarnings + m_curbRightWarnings) * 8);

    // Stress Level   errors × 12  +  warnings × 4  +  peak-bZ × 50  +  red-light × 8
    int stressLevel  = qMin(100, (int)(totalErrors * 12
                                     + totalWarnings * 4
                                     + m_maxBumpZ * 50.0
                                     + m_redLightErrors * 8));

    // Anxiety Level  curb errors × 20  +  curb warnings × 10  [0..100]
    int anxietyLevel = qMin(100,
                        (m_curbLeftErrors + m_curbRightErrors) * 20
                      + (m_curbLeftWarnings + m_curbRightWarnings) * 10);

    int calmness   = qMax(0, 100 - stressLevel);
    int confidence = qMax(0, 100 - anxietyLevel);

    // Overall  speedCtrl×35%  +  laneCtrl×35%  +  calmness×15%  +  confidence×15%
    //          − 20 per red light violation (severe traffic infraction)
    int overall = qMax(0,
        (int)(speedControl * 0.35 + laneControl * 0.35
            + calmness     * 0.15 + confidence  * 0.15)
        - m_redLightErrors * 20);

    double avgBumpZ = (m_bumpCount > 0) ? (m_sumBumpZ / m_bumpCount) : 0.0;

    // ── Signal CircuitDashboard with the live-sensor data ─────────────────────
    // riskScore = weighted curb/speed events; stressIndex = stress/100
    double riskScore  = qMin(1.0, totalErrors  * 0.15 + totalWarnings * 0.05);
    double stressIdx  = stressLevel  / 100.0;
    emit sessionCompleted(m_sessionData, stressIdx, riskScore);

    // ── Build dialog ─────────────────────────────────────────────────────────
    QDialog *dlg = new QDialog(this);
    dlg->setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);
    dlg->setWindowTitle(
        QString::fromUtf8("Session Analysis \xe2\x80\x94 Circuit Course"));
    dlg->setMinimumSize(660, 580);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setStyleSheet("background:#f0f2f5;");

    QVBoxLayout *root = new QVBoxLayout(dlg);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header ───────────────────────────────────────────────────────────────
    QWidget *hdr = new QWidget;
    hdr->setFixedHeight(74);
    hdr->setStyleSheet(
        "background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "stop:0 #4f46e5,stop:1 #7c3aed);");
    QHBoxLayout *hdrLay = new QHBoxLayout(hdr);
    hdrLay->setContentsMargins(20, 0, 20, 0);
    hdrLay->setSpacing(12);

    QLabel *hdrIcon = new QLabel(QString::fromUtf8("\xf0\x9f\x8f\x81"));
    hdrIcon->setStyleSheet("font-size:30px;background:transparent;");
    hdrLay->addWidget(hdrIcon);

    QVBoxLayout *hdrTxt = new QVBoxLayout;
    hdrTxt->setSpacing(2);
    QLabel *hdrTitle = new QLabel(
        QString::fromUtf8("Analyse de Session Termin\xc3\xa9" "e"));
    hdrTitle->setStyleSheet(
        "font-size:16px;font-weight:700;color:white;background:transparent;");

    int mm2 = m_elapsedSecs / 60, ss2 = m_elapsedSecs % 60;
    QLabel *hdrSub = new QLabel(
        QString(QString::fromUtf8(
            "Dur\xc3\xa9""e: %1:%2  \xe2\x80\xa2  "
            "\xc3\x89""tapes: %3/%4  \xe2\x80\xa2  "
            "Fautes: %5  \xe2\x80\xa2  Feux rouges: %6  \xe2\x80\xa2  Avertissements: %7"))
        .arg(mm2, 2, 10, QChar('0')).arg(ss2, 2, 10, QChar('0'))
        .arg(m_currentStep + 1).arg(m_totalSteps)
        .arg(totalErrors).arg(m_redLightErrors).arg(totalWarnings));
    hdrSub->setStyleSheet("font-size:11px;color:#c7d2fe;background:transparent;");
    hdrTxt->addWidget(hdrTitle);
    hdrTxt->addWidget(hdrSub);
    hdrLay->addLayout(hdrTxt, 1);
    root->addWidget(hdr);

    // ── Scrollable body ──────────────────────────────────────────────────────
    QScrollArea *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(
        "QScrollArea{border:none;background:#f0f2f5;}"
        "QScrollBar:vertical{width:6px;background:transparent;}"
        "QScrollBar::handle:vertical{background:#d0d5db;border-radius:3px;}");

    QWidget *body = new QWidget;
    body->setStyleSheet("background:#f0f2f5;");
    QVBoxLayout *bodyLay = new QVBoxLayout(body);
    bodyLay->setContentsMargins(20, 20, 20, 20);
    bodyLay->setSpacing(16);

    // ── Overall score card ───────────────────────────────────────────────────
    QString ovColor = overall >= 80 ? "#10b981"
                    : overall >= 60 ? "#f59e0b"
                    : "#ef4444";
    QString ovLabel = overall >= 80
        ? QString::fromUtf8("Excellent \xf0\x9f\x8f\x86")
        : overall >= 60
            ? QString::fromUtf8("Bien \xf0\x9f\x91\x8d")
            : overall >= 40
                ? QString::fromUtf8("\xc3\x80 am\xc3\xa9""liorer \xf0\x9f\x93\x88")
                : QString::fromUtf8("Insuffisant \xe2\x9a\xa0\xef\xb8\x8f");

    QFrame *ovCard = new QFrame;
    ovCard->setStyleSheet(
        QString("QFrame{background:white;border-radius:16px;"
                "border:2px solid %1;}").arg(ovColor));
    ovCard->setGraphicsEffect(makeShadow(ovCard, 20, 30));

    QHBoxLayout *ovLay = new QHBoxLayout(ovCard);
    ovLay->setContentsMargins(24, 18, 24, 18);
    ovLay->setSpacing(24);

    // Left: big score number
    QVBoxLayout *ovLeft = new QVBoxLayout;
    ovLeft->setSpacing(4);
    QLabel *ovTitleLbl = new QLabel(QString::fromUtf8("Score Global"));
    ovTitleLbl->setStyleSheet("font-size:12px;font-weight:600;color:#64748b;");
    QLabel *ovNum = new QLabel(QString::number(overall));
    ovNum->setStyleSheet(
        QString("font-size:52px;font-weight:900;color:%1;").arg(ovColor));
    QLabel *ovOf = new QLabel("/100");
    ovOf->setStyleSheet("font-size:14px;color:#94a3b8;");
    QLabel *ovBadge = new QLabel(ovLabel);
    ovBadge->setStyleSheet(
        QString("font-size:12px;font-weight:700;color:white;"
                "background:%1;padding:4px 14px;border-radius:10px;").arg(ovColor));
    QHBoxLayout *ovScoreRow = new QHBoxLayout;
    ovScoreRow->setSpacing(4);
    ovScoreRow->addWidget(ovNum);
    ovScoreRow->addWidget(ovOf, 0, Qt::AlignBottom);
    ovScoreRow->addStretch();
    ovLeft->addWidget(ovTitleLbl);
    ovLeft->addLayout(ovScoreRow);
    ovLeft->addWidget(ovBadge);
    ovLeft->addStretch();
    ovLay->addLayout(ovLeft, 1);

    // Right: mini progress bars
    QVBoxLayout *ovRight = new QVBoxLayout;
    ovRight->setSpacing(8);

    struct BarDef { QString label; int val; QString col; };
    QVector<BarDef> minitBars = {
        { QString::fromUtf8("Contr\xc3\xb4le Vitesse"),    speedControl, "#6366f1" },
        { QString::fromUtf8("Contr\xc3\xb4le de Voie"),    laneControl,  "#8b5cf6" },
        { QString::fromUtf8("Calme (inv. stress)"),         calmness,     "#10b981" },
        { QString::fromUtf8("Confiance (inv. anx.)"),       confidence,   "#f59e0b" },
    };
    for (const BarDef &b : minitBars) {
        QHBoxLayout *bRow = new QHBoxLayout;
        bRow->setSpacing(8);
        QLabel *bLbl = new QLabel(b.label);
        bLbl->setStyleSheet("font-size:10px;color:#64748b;");
        bLbl->setFixedWidth(145);
        QProgressBar *pb = new QProgressBar;
        pb->setRange(0, 100); pb->setValue(b.val);
        pb->setFixedHeight(7); pb->setTextVisible(false);
        pb->setStyleSheet(
            QString("QProgressBar{background:#f1f5f9;border-radius:3px;}"
                    "QProgressBar::chunk{background:%1;border-radius:3px;}").arg(b.col));
        QLabel *bNum = new QLabel(QString::number(b.val));
        bNum->setStyleSheet(
            QString("font-size:10px;font-weight:700;color:%1;").arg(b.col));
        bNum->setFixedWidth(28);
        bRow->addWidget(bLbl);
        bRow->addWidget(pb, 1);
        bRow->addWidget(bNum);
        ovRight->addLayout(bRow);
    }
    ovRight->addStretch();
    ovLay->addLayout(ovRight, 2);
    bodyLay->addWidget(ovCard);

    // ── 4 metric cards row ────────────────────────────────────────────────────
    QHBoxLayout *metrRow = new QHBoxLayout;
    metrRow->setSpacing(12);

    struct MetrDef {
        QString icon; QString title; int val; QString col; QString sub;
    };
    QVector<MetrDef> metrs = {
        { QString::fromUtf8("\xf0\x9f\x9a\x97"),
          QString::fromUtf8("Contr\xc3\xb4le\nVitesse"),
          speedControl,
          speedControl >= 80 ? "#10b981" : speedControl >= 60 ? "#f59e0b" : "#ef4444",
          QString::fromUtf8("%1 err. vitesse  %2 dos d'\xc3\xa2ne")
              .arg(m_speedErrors).arg(m_bumpErrors) },

        { QString::fromUtf8("\xf0\x9f\x9b\xa3"),
          QString::fromUtf8("Contr\xc3\xb4le\nde Voie"),
          laneControl,
          laneControl >= 80 ? "#10b981" : laneControl >= 60 ? "#f59e0b" : "#ef4444",
          QString::fromUtf8("G: %1err %2av   D: %3err %4av")
              .arg(m_curbLeftErrors).arg(m_curbLeftWarnings)
              .arg(m_curbRightErrors).arg(m_curbRightWarnings) },

        { QString::fromUtf8("\xf0\x9f\x92\x93"),
          QString::fromUtf8("Niveau\nde Stress"),
          stressLevel,
          stressLevel <= 30 ? "#10b981" : stressLevel <= 60 ? "#f59e0b" : "#ef4444",
          QString::fromUtf8("Pic bZ: %1 g   Moy: %2 g")
              .arg(m_maxBumpZ, 0, 'f', 3)
              .arg(avgBumpZ,   0, 'f', 3) },

        { QString::fromUtf8("\xf0\x9f\x98\x9f"),
          QString::fromUtf8("Niveau\nd'Anxi\xc3\xa9t\xc3\xa9"),
          anxietyLevel,
          anxietyLevel <= 30 ? "#10b981" : anxietyLevel <= 60 ? "#f59e0b" : "#ef4444",
          QString::fromUtf8("Trottoir: %1 err + %2 avert")
              .arg(m_curbLeftErrors + m_curbRightErrors)
              .arg(m_curbLeftWarnings + m_curbRightWarnings) },
    };

    for (const MetrDef &md : metrs) {
        QFrame *mc = new QFrame;
        mc->setStyleSheet(
            "QFrame{background:white;border-radius:12px;border:1px solid #e2e8f0;}");
        mc->setGraphicsEffect(makeShadow(mc));
        QVBoxLayout *mcLay = new QVBoxLayout(mc);
        mcLay->setContentsMargins(14, 12, 14, 12);
        mcLay->setSpacing(4);

        QLabel *mIcon = new QLabel(md.icon);
        mIcon->setStyleSheet("font-size:20px;");
        QLabel *mTitle = new QLabel(md.title);
        mTitle->setStyleSheet("font-size:10px;font-weight:600;color:#64748b;");
        QLabel *mVal = new QLabel(QString::number(md.val));
        mVal->setStyleSheet(
            QString("font-size:32px;font-weight:900;color:%1;").arg(md.col));
        QProgressBar *mpb = new QProgressBar;
        mpb->setRange(0, 100); mpb->setValue(md.val);
        mpb->setFixedHeight(6); mpb->setTextVisible(false);
        mpb->setStyleSheet(
            QString("QProgressBar{background:#f1f5f9;border-radius:3px;}"
                    "QProgressBar::chunk{background:%1;border-radius:3px;}").arg(md.col));
        QLabel *mSub = new QLabel(md.sub);
        mSub->setStyleSheet("font-size:9px;color:#94a3b8;");
        mSub->setWordWrap(true);

        mcLay->addWidget(mIcon);
        mcLay->addWidget(mTitle);
        mcLay->addWidget(mVal);
        mcLay->addWidget(mpb);
        mcLay->addWidget(mSub);
        mcLay->addStretch();
        metrRow->addWidget(mc);
    }
    bodyLay->addLayout(metrRow);

    // ── Error breakdown table ─────────────────────────────────────────────────
    QFrame *tblCard = new QFrame;
    tblCard->setStyleSheet(
        "QFrame{background:white;border-radius:12px;border:1px solid #e2e8f0;}");
    tblCard->setGraphicsEffect(makeShadow(tblCard));
    QVBoxLayout *tblLay = new QVBoxLayout(tblCard);
    tblLay->setContentsMargins(16, 12, 16, 12);
    tblLay->setSpacing(8);

    QLabel *tblTitle = new QLabel(
        QString::fromUtf8("\xf0\x9f\x93\x8b  D\xc3\xa9""tail des Fautes et Avertissements"));
    tblTitle->setStyleSheet("font-size:12px;font-weight:700;color:#1e293b;");
    tblLay->addWidget(tblTitle);

    QTableWidget *tbl = new QTableWidget(7, 3);
    tbl->setHorizontalHeaderLabels({
        QString::fromUtf8("Type d'erreur / avertissement"),
        QString::fromUtf8("Cat\xc3\xa9""gorie"),
        "Nombre"
    });
    tbl->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    tbl->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tbl->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    tbl->verticalHeader()->hide();
    tbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tbl->setSelectionMode(QAbstractItemView::NoSelection);
    tbl->setAlternatingRowColors(true);
    tbl->setFixedHeight(238);   // 7 rows × 34px
    tbl->setStyleSheet(
        "QTableWidget{border:none;font-size:11px;}"
        "QHeaderView::section{background:#f8fafc;font-weight:700;color:#475569;"
        "border:none;border-bottom:2px solid #e2e8f0;padding:5px;}"
        "QTableWidget::item{padding:5px;}"
        "QTableWidget::item:alternate{background:#f8fafc;}");

    struct TRow { QString type; QString cat; bool isErr; int cnt; };
    QVector<TRow> trows = {
        { QString::fromUtf8("ERREUR Feu Rouge \xf0\x9f\x9a\xa6"),
          QString::fromUtf8("Feu tricolore (IR)"),
          true,  m_redLightErrors },
        { "ERREUR VITESSE",
          QString::fromUtf8("Vitesse excessive"),
          true,  m_speedErrors },
        { QString::fromUtf8("ERREUR dos d'\xc3\xa2ne"),
          QString::fromUtf8("Ralentisseur"),
          true,  m_bumpErrors },
        { QString::fromUtf8("ERREUR trottoir gauche"),
          QString::fromUtf8("Voie gauche"),
          true,  m_curbLeftErrors },
        { QString::fromUtf8("ERREUR trottoir droit"),
          QString::fromUtf8("Voie droite"),
          true,  m_curbRightErrors },
        { QString::fromUtf8("AVERT proche trottoir G."),
          QString::fromUtf8("Avertissement gauche"),
          false, m_curbLeftWarnings },
        { QString::fromUtf8("AVERT proche trottoir D."),
          QString::fromUtf8("Avertissement droit"),
          false, m_curbRightWarnings },
    };

    for (int i = 0; i < trows.size(); ++i) {
        const TRow &r = trows[i];
        auto *t0 = new QTableWidgetItem(r.type);
        auto *t1 = new QTableWidgetItem(r.cat);
        auto *t2 = new QTableWidgetItem(QString::number(r.cnt));
        t2->setTextAlignment(Qt::AlignCenter);
        if (r.cnt > 0) {
            QColor c = r.isErr ? QColor("#ef4444") : QColor("#f59e0b");
            t0->setForeground(c);
            t2->setForeground(c);
            QFont f = t2->font(); f.setBold(true); t2->setFont(f);
        } else {
            t2->setForeground(QColor("#10b981"));
        }
        tbl->setItem(i, 0, t0);
        tbl->setItem(i, 1, t1);
        tbl->setItem(i, 2, t2);
    }
    tblLay->addWidget(tbl);
    bodyLay->addWidget(tblCard);

    // ── Recommendations (or perfect run message) ──────────────────────────────
    if (totalErrors > 0 || totalWarnings > 0 || m_redLightErrors > 0) {
        // Use red border if there were red light violations (most serious infraction)
        QString recBg     = m_redLightErrors > 0 ? "#fff1f2" : "#fffbeb";
        QString recBorder = m_redLightErrors > 0 ? "#fecaca" : "#fde68a";
        QString recTxtCol = m_redLightErrors > 0 ? "#7f1d1d" : "#92400e";

        QFrame *recCard = new QFrame;
        recCard->setStyleSheet(
            QString("QFrame{background:%1;border-radius:12px;border:1px solid %2;}")
            .arg(recBg, recBorder));
        QVBoxLayout *recLay = new QVBoxLayout(recCard);
        recLay->setContentsMargins(16, 12, 16, 12);
        recLay->setSpacing(6);

        QLabel *recTitle = new QLabel(
            QString::fromUtf8(
                "\xf0\x9f\x92\xa1  Recommandations pour l'\xc3\x89l\xc3\xa8ve"));
        recTitle->setStyleSheet(
            QString("font-size:12px;font-weight:700;color:%1;").arg(recTxtCol));
        recLay->addWidget(recTitle);

        QStringList recs;
        // ── Red light violations (most serious — listed first) ────────────────
        if (m_redLightErrors > 0)
            recs << QString::fromUtf8(
                "\xf0\x9f\x9a\xa8 %1 passage(s) au FEU ROUGE \xe2\x80\x94 "
                "infraction grave ! Respectez absolument les signaux tricolores.")
                      .arg(m_redLightErrors);
        if (m_speedErrors > 0)
            recs << QString::fromUtf8(
                "\xe2\x80\xa2 R\xc3\xa9""duisez la vitesse dans les zones limit\xc3\xa9""es "
                "(%1 d\xc3\xa9""passement(s))").arg(m_speedErrors);
        if (m_bumpErrors > 0)
            recs << QString::fromUtf8(
                "\xe2\x80\xa2 Ralentissez avant les dos d'\xc3\xa2ne "
                "(%1 choc(s) d\xc3\xa9""tect\xc3\xa9(s) par le MPU-6050)").arg(m_bumpErrors);
        if (m_curbLeftErrors + m_curbRightErrors > 0)
            recs << QString::fromUtf8(
                "\xe2\x80\xa2 Am\xc3\xa9""liorez le contr\xc3\xb4le lat\xc3\xa9ral : "
                "%1 touche(s) de trottoir")
                      .arg(m_curbLeftErrors + m_curbRightErrors);
        if (m_curbLeftWarnings + m_curbRightWarnings > 0)
            recs << QString::fromUtf8(
                "\xe2\x80\xa2 Restez plus centr\xc3\xa9(e) dans la voie "
                "(%1 approche(s) trop proche du trottoir)")
                      .arg(m_curbLeftWarnings + m_curbRightWarnings);

        QLabel *recTxt = new QLabel(recs.join("\n"));
        recTxt->setStyleSheet(
            "font-size:11px;color:#78350f;line-height:1.5;");
        recTxt->setWordWrap(true);
        recLay->addWidget(recTxt);
        bodyLay->addWidget(recCard);
    } else {
        QFrame *perfCard = new QFrame;
        perfCard->setStyleSheet(
            "QFrame{background:#ecfdf5;border-radius:12px;border:1px solid #a7f3d0;}");
        QVBoxLayout *perfLay = new QVBoxLayout(perfCard);
        perfLay->setContentsMargins(16, 12, 16, 12);
        QLabel *perfLbl = new QLabel(
            QString::fromUtf8(
                "\xf0\x9f\x8e\x89  Excellent parcours ! Aucune faute ni avertissement. "
                "L'\xc3\xa9l\xc3\xa8ve ma\xc3\xaetrise parfaitement le circuit."));
        perfLbl->setStyleSheet(
            "font-size:12px;color:#065f46;font-weight:600;");
        perfLbl->setWordWrap(true);
        perfLay->addWidget(perfLbl);
        bodyLay->addWidget(perfCard);
    }

    // ── Close button ──────────────────────────────────────────────────────────
    QPushButton *closeBtn = new QPushButton(
        QString::fromUtf8("\xe2\x9c\x85  Fermer l'Analyse"));
    closeBtn->setStyleSheet(
        "QPushButton{background:#6366f1;color:white;border:none;"
        "border-radius:8px;padding:10px 28px;font-size:13px;font-weight:700;}"
        "QPushButton:hover{background:#4f46e5;}");
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);
    bodyLay->addWidget(closeBtn, 0, Qt::AlignCenter);

    scroll->setWidget(body);
    root->addWidget(scroll, 1);

    dlg->exec();
}
