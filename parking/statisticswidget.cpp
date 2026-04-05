#include "statisticswidget.h"
#include "parkingdbmanager.h"
#include <QHBoxLayout>
#include <QScrollArea>
#include <QGraphicsDropShadowEffect>
#include <QFrame>
#include <QPainter>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QSqlDatabase>
#include <QDateTime>
#include <QDebug>

// ═══════════════════════════════════════════════════════════════
//  Statistics Widget — Direct queries on PARKING_SESSIONS (Oracle)
// ═══════════════════════════════════════════════════════════════

static QGraphicsDropShadowEffect* statCardShadow(QWidget *p) {
    auto *s = new QGraphicsDropShadowEffect(p);
    s->setBlurRadius(16); s->setColor(QColor(0,0,0,12)); s->setOffset(0,3);
    return s;
}

StatisticsWidget::StatisticsWidget(const QString &userName, const QString &userRole,
                                   int userId, QWidget *parent)
    : QWidget(parent), m_userName(userName), m_userRole(userRole),
      m_isMoniteur(userRole == "Moniteur"), m_eleveId(0),
      m_valSessions(nullptr), m_trendSessions(nullptr),
      m_valTaux(nullptr), m_trendTaux(nullptr),
      m_valHeures(nullptr), m_trendHeures(nullptr),
      m_valBest(nullptr), m_trendBest(nullptr),
      m_activityLayout(nullptr)
{
    if (!m_isMoniteur && userId > 0)
        m_eleveId = userId;
    setupUI();
    refreshData();
}

void StatisticsWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0,0,0,0);
    mainLayout->setSpacing(0);

    // ── Banner ──
    QFrame *banner = new QFrame(this);
    banner->setFixedHeight(70);
    banner->setStyleSheet(
        "QFrame{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "stop:0 #6c5ce7, stop:1 #a29bfe);border:none;}");
    QHBoxLayout *bannerL = new QHBoxLayout(banner);
    bannerL->setContentsMargins(30,0,30,0);

    QLabel *title = new QLabel(QString::fromUtf8("\xf0\x9f\x93\x8a  STATISTICS & PROGRESS"), banner);
    title->setStyleSheet("QLabel{font-size:17px;font-weight:bold;color:white;"
        "letter-spacing:3px;background:transparent;border:none;}");
    bannerL->addWidget(title);
    bannerL->addStretch();

    QPushButton *refreshBtn = new QPushButton(QString::fromUtf8("\xf0\x9f\x94\x84 Refresh"), banner);
    refreshBtn->setCursor(Qt::PointingHandCursor);
    refreshBtn->setStyleSheet(
        "QPushButton{background:rgba(255,255,255,0.2);color:white;border:1px solid rgba(255,255,255,0.3);"
        "border-radius:10px;padding:8px 18px;font-size:11px;font-weight:bold;}"
        "QPushButton:hover{background:rgba(255,255,255,0.35);}");
    connect(refreshBtn, &QPushButton::clicked, this, &StatisticsWidget::refreshData);
    bannerL->addWidget(refreshBtn);

    mainLayout->addWidget(banner);

    // ── Scrollable content ──
    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(
        "QScrollArea{border:none;background:#f0f2f5;}"
        "QScrollBar:vertical{width:6px;background:#f0f2f5;}"
        "QScrollBar::handle:vertical{background:#ccc;border-radius:3px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}");

    QWidget *content = new QWidget(this);
    content->setObjectName("statsContent");
    content->setStyleSheet("QWidget#statsContent{background:#f0f2f5;}");
    QVBoxLayout *cl = new QVBoxLayout(content);
    cl->setContentsMargins(30,20,30,20);
    cl->setSpacing(20);

    // ── Row 1: 4 stat cards ──
    QHBoxLayout *statsRow = new QHBoxLayout();
    statsRow->setSpacing(16);
    statsRow->addWidget(createStatCard(QString::fromUtf8("\xf0\x9f\x93\x8b"), "—",
        "Sessions", "#00b894", &m_valSessions, &m_trendSessions));
    statsRow->addWidget(createStatCard(QString::fromUtf8("\xf0\x9f\x8f\x86"), "—",
        "Success Rate", "#6c5ce7", &m_valTaux, &m_trendTaux));
    statsRow->addWidget(createStatCard(QString::fromUtf8("\xe2\x8f\xb1"), "—",
        "Practice Hours", "#0984e3", &m_valHeures, &m_trendHeures));
    statsRow->addWidget(createStatCard(QString::fromUtf8("\xf0\x9f\xa5\x87"), "—",
        "Best Time", "#e17055", &m_valBest, &m_trendBest));
    cl->addLayout(statsRow);

    // ── Row 2: Chart + Progress ──
    QHBoxLayout *chartsRow = new QHBoxLayout();
    chartsRow->setSpacing(16);
    chartsRow->addWidget(createChartCard(), 1);
    chartsRow->addWidget(createProgressCard(), 1);
    cl->addLayout(chartsRow);

    // ── Row 3: Activity feed ──
    cl->addWidget(createActivityFeed());

    cl->addStretch();
    scroll->setWidget(content);
    mainLayout->addWidget(scroll, 1);
}

QWidget* StatisticsWidget::createStatCard(const QString &icon, const QString &defaultVal,
                                           const QString &label, const QString &color,
                                           QLabel **valRef, QLabel **trendRef)
{
    QFrame *card = new QFrame(this);
    static int sid = 0;
    QString oid = QString("swC%1").arg(sid++);
    card->setObjectName(oid);
    card->setStyleSheet(QString("QFrame#%1{background:white;border-radius:12px;"
        "border:1px solid #e8e8e8;}").arg(oid));
    card->setGraphicsEffect(statCardShadow(card));
    card->setFixedHeight(100);

    QVBoxLayout *lay = new QVBoxLayout(card);
    lay->setContentsMargins(18,14,18,14);
    lay->setSpacing(4);

    QHBoxLayout *top = new QHBoxLayout();
    QLabel *ic = new QLabel(icon, card);
    ic->setStyleSheet("QLabel{font-size:22px;background:transparent;border:none;}");
    top->addWidget(ic);
    top->addStretch();

    QLabel *val = new QLabel(defaultVal, card);
    val->setStyleSheet(QString("QLabel{font-size:22px;font-weight:bold;color:%1;"
        "background:transparent;border:none;}").arg(color));
    val->setAlignment(Qt::AlignRight);
    top->addWidget(val);
    lay->addLayout(top);

    QLabel *lbl = new QLabel(label, card);
    lbl->setStyleSheet("QLabel{font-size:10px;color:#636e72;font-weight:600;"
        "letter-spacing:1px;background:transparent;border:none;}");
    lay->addWidget(lbl);

    QLabel *trend = new QLabel("", card);
    trend->setStyleSheet("QLabel{font-size:9px;color:#b2bec3;background:transparent;border:none;}");
    lay->addWidget(trend);

    if (valRef)   *valRef   = val;
    if (trendRef) *trendRef = trend;

    return card;
}

QWidget* StatisticsWidget::createChartCard()
{
    QFrame *card = new QFrame(this);
    card->setObjectName("swChartCard");
    card->setStyleSheet("QFrame#swChartCard{background:white;border-radius:12px;border:1px solid #e8e8e8;}");
    card->setGraphicsEffect(statCardShadow(card));

    QVBoxLayout *lay = new QVBoxLayout(card);
    lay->setContentsMargins(18,16,18,16);
    lay->setSpacing(12);

    QLabel *title = new QLabel(QString::fromUtf8("\xf0\x9f\x93\x8a Mastery by Maneuver"), card);
    title->setStyleSheet("QLabel{font-size:14px;font-weight:bold;color:#2d3436;"
        "background:transparent;border:none;}");
    lay->addWidget(title);

    QStringList names  = {"Parallel", "Bataille", "Diagonal", "Reverse", "U-turn"};
    QStringList colors = {"#00b894", "#0984e3", "#6c5ce7", "#e17055", "#fdcb6e"};

    for (int i = 0; i < names.size(); i++) {
        QHBoxLayout *row = new QHBoxLayout();
        row->setSpacing(10);

        QLabel *nm = new QLabel(names[i], card);
        nm->setFixedWidth(110);
        nm->setStyleSheet("QLabel{font-size:11px;color:#2d3436;font-weight:600;"
            "background:transparent;border:none;}");
        row->addWidget(nm);

        QProgressBar *bar = new QProgressBar(card);
        bar->setRange(0, 100);
        bar->setValue(0);
        bar->setTextVisible(false);
        bar->setFixedHeight(10);
        bar->setStyleSheet(QString(
            "QProgressBar{background:#f0f0f0;border-radius:5px;border:none;}"
            "QProgressBar::chunk{background:%1;border-radius:5px;}").arg(colors[i]));
        row->addWidget(bar, 1);
        m_bars.append(bar);

        QLabel *pct = new QLabel("0%", card);
        pct->setFixedWidth(40);
        pct->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        pct->setStyleSheet(QString("QLabel{font-size:11px;font-weight:bold;color:%1;"
            "background:transparent;border:none;}").arg(colors[i]));
        row->addWidget(pct);
        m_pcts.append(pct);

        lay->addLayout(row);
    }

    lay->addStretch();
    return card;
}

QWidget* StatisticsWidget::createProgressCard()
{
    QFrame *card = new QFrame(this);
    card->setObjectName("swProgressCard");
    card->setStyleSheet("QFrame#swProgressCard{background:white;border-radius:12px;border:1px solid #e8e8e8;}");
    card->setGraphicsEffect(statCardShadow(card));

    QVBoxLayout *lay = new QVBoxLayout(card);
    lay->setContentsMargins(18,16,18,16);
    lay->setSpacing(12);

    QLabel *title = new QLabel(QString::fromUtf8("\xf0\x9f\x8f\x85 Badges & Rewards"), card);
    title->setStyleSheet("QLabel{font-size:14px;font-weight:bold;color:#2d3436;"
        "background:transparent;border:none;}");
    lay->addWidget(title);

    struct Badge { QString icon, name, desc; };
    QList<Badge> badges = {
        {QString::fromUtf8("\xf0\x9f\x8c\x9f"), "First Step",  QString::fromUtf8("1st session completed")},
        {QString::fromUtf8("\xf0\x9f\x94\xa5"), "Dedicated",   "10 sessions"},
        {QString::fromUtf8("\xf0\x9f\xa7\xa0"), "Expert",      QString::fromUtf8("5 perfect runs")},
        {QString::fromUtf8("\xf0\x9f\x9a\x80"), "Pilot",       "2h practice"},
        {QString::fromUtf8("\xe2\xad\x90"),      "5 Stars",     QString::fromUtf8("90% success rate")},
    };

    for (const auto &b : badges) {
        QHBoxLayout *row = new QHBoxLayout();
        row->setSpacing(10);
        QLabel *bIcon = new QLabel(b.icon, card);
        bIcon->setFixedSize(36,36);
        bIcon->setAlignment(Qt::AlignCenter);
        bIcon->setStyleSheet("QLabel{background:#f5f6fa;border-radius:18px;font-size:18px;border:none;}");
        row->addWidget(bIcon);

        QVBoxLayout *bInfo = new QVBoxLayout();
        bInfo->setSpacing(0);
        QLabel *bName = new QLabel(b.name, card);
        bName->setStyleSheet("QLabel{font-size:12px;font-weight:600;color:#2d3436;background:transparent;border:none;}");
        bInfo->addWidget(bName);
        QLabel *bDesc = new QLabel(b.desc, card);
        bDesc->setStyleSheet("QLabel{font-size:10px;color:#b2bec3;background:transparent;border:none;}");
        bInfo->addWidget(bDesc);
        row->addLayout(bInfo, 1);
        lay->addLayout(row);
    }

    lay->addStretch();
    return card;
}

QWidget* StatisticsWidget::createActivityFeed()
{
    QFrame *card = new QFrame(this);
    card->setObjectName("swActivityCard");
    card->setStyleSheet("QFrame#swActivityCard{background:white;border-radius:12px;border:1px solid #e8e8e8;}");
    card->setGraphicsEffect(statCardShadow(card));

    QVBoxLayout *lay = new QVBoxLayout(card);
    lay->setContentsMargins(18,16,18,16);
    lay->setSpacing(10);

    QLabel *title = new QLabel(QString::fromUtf8("\xf0\x9f\x93\x8b Session History"), card);
    title->setStyleSheet("QLabel{font-size:14px;font-weight:bold;color:#2d3436;"
        "background:transparent;border:none;}");
    lay->addWidget(title);

    m_activityLayout = new QVBoxLayout();
    m_activityLayout->setSpacing(6);
    lay->addLayout(m_activityLayout);

    QLabel *empty = new QLabel(QString::fromUtf8("No sessions recorded"), card);
    empty->setStyleSheet("QLabel{font-size:11px;color:#b2bec3;background:transparent;border:none;padding:20px;}");
    empty->setAlignment(Qt::AlignCenter);
    m_activityLayout->addWidget(empty);

    lay->addStretch();
    return card;
}

// ═══════════════════════════════════════════════════════════════
//  refreshData — Direct PARKING_SESSIONS queries (no SESSIONS UNION)
// ═══════════════════════════════════════════════════════════════

void StatisticsWidget::refreshData()
{
    // Use the same default Oracle connection the rest of the app uses
    QSqlDatabase db = ParkingDBManager::instance().db();

    if (!db.isOpen()) {
        qDebug() << "[StatisticsWidget] DB not open";
        if (m_valSessions) m_valSessions->setText("—");
        if (m_trendSessions) m_trendSessions->setText("DB not connected");
        return;
    }

    if (m_eleveId <= 0 && !m_isMoniteur) {
        if (m_valSessions)   m_valSessions->setText("—");
        if (m_trendSessions) m_trendSessions->setText("Profile not found in DB");
        if (m_valTaux)       m_valTaux->setText("—");
        if (m_valHeures)     m_valHeures->setText("—");
        if (m_valBest)       m_valBest->setText("—");
        return;
    }

    QSqlQuery q(db);

    // ── Total sessions ──
    q.prepare("SELECT COUNT(*) FROM PARKING_SESSIONS WHERE student_id = :id");
    q.bindValue(":id", m_eleveId);
    int totalSessions = (q.exec() && q.next()) ? q.value(0).toInt() : 0;
    if (q.lastError().isValid())
        qDebug() << "[Stats] COUNT sessions error:" << q.lastError().text();

    // ── Successful sessions ──
    q.prepare("SELECT COUNT(*) FROM PARKING_SESSIONS WHERE student_id = :id AND is_successful = 1");
    q.bindValue(":id", m_eleveId);
    int totalSuccess = (q.exec() && q.next()) ? q.value(0).toInt() : 0;

    double tauxReussite = (totalSessions > 0) ? (double)totalSuccess / totalSessions * 100.0 : 0.0;

    // ── Total practice seconds ──
    q.prepare("SELECT NVL(SUM(NVL(duration_seconds, 0)), 0) FROM PARKING_SESSIONS WHERE student_id = :id");
    q.bindValue(":id", m_eleveId);
    int totalPracticeSec = (q.exec() && q.next()) ? q.value(0).toInt() : 0;
    double totalHours = totalPracticeSec / 3600.0;

    // ── Best time (minimum duration across all maneuvers) ──
    q.prepare("SELECT MIN(duration_seconds) FROM PARKING_SESSIONS "
              "WHERE student_id = :id AND is_successful = 1 AND duration_seconds > 0");
    q.bindValue(":id", m_eleveId);
    int overallBest = 999;
    if (q.exec() && q.next() && !q.value(0).isNull())
        overallBest = q.value(0).toInt();

    // ── Update stat cards ──
    if (m_valSessions) {
        m_valSessions->setText(QString::number(totalSessions));
        if (m_trendSessions)
            m_trendSessions->setText(QString::fromUtf8("%1 passed \xc2\xb7 %2 failed")
                .arg(totalSuccess).arg(totalSessions - totalSuccess));
    }

    if (m_valTaux) {
        m_valTaux->setText(QString("%1%").arg(tauxReussite, 0, 'f', 0));
        if (m_trendTaux) {
            QString trendColor = tauxReussite >= 70 ? "#00b894"
                               : (tauxReussite >= 40 ? "#fdcb6e" : "#e17055");
            QString trendText  = tauxReussite >= 70 ? "Excellent"
                               : (tauxReussite >= 40 ? "Improving" : "Needs work");
            m_trendTaux->setText(trendText);
            m_trendTaux->setStyleSheet(QString(
                "QLabel{font-size:9px;color:%1;background:transparent;border:none;font-weight:bold;}")
                .arg(trendColor));
        }
    }

    if (m_valHeures) {
        m_valHeures->setText(QString("%1h").arg(totalHours, 0, 'f', 1));
        if (m_trendHeures) {
            int pctOf8h = qMin(100, (int)(totalHours / 8.0 * 100));
            m_trendHeures->setText(QString::fromUtf8("%1% of 8h target").arg(pctOf8h));
        }
    }

    if (m_valBest) {
        if (overallBest < 999) {
            int mn = overallBest / 60, sc = overallBest % 60;
            m_valBest->setText(QString("%1:%2").arg(mn).arg(sc, 2, 10, QChar('0')));
        } else {
            m_valBest->setText("—");
        }
        if (m_trendBest) m_trendBest->setText("Fastest maneuver");
    }

    // ── Maneuver mastery bars (direct PARKING_SESSIONS) ──
    QStringList manTypes = {"creneau", "bataille", "epi", "marche_arriere", "demi_tour"};
    for (int i = 0; i < manTypes.size() && i < m_bars.size(); i++) {
        q.prepare("SELECT COUNT(*) FROM PARKING_SESSIONS "
                  "WHERE student_id = :id AND maneuver_type = :type");
        q.bindValue(":id", m_eleveId);
        q.bindValue(":type", manTypes[i]);
        int attempts = (q.exec() && q.next()) ? q.value(0).toInt() : 0;

        q.prepare("SELECT COUNT(*) FROM PARKING_SESSIONS "
                  "WHERE student_id = :id AND maneuver_type = :type AND is_successful = 1");
        q.bindValue(":id", m_eleveId);
        q.bindValue(":type", manTypes[i]);
        int successes = (q.exec() && q.next()) ? q.value(0).toInt() : 0;

        int pct = (attempts > 0) ? (successes * 100 / attempts) : 0;
        m_bars[i]->setValue(pct);
        if (i < m_pcts.size())
            m_pcts[i]->setText(QString("%1%").arg(pct));
    }

    // ── Session history (last 10 from PARKING_SESSIONS JOIN CARS) ──
    if (m_activityLayout) {
        while (m_activityLayout->count() > 0) {
            QLayoutItem *item = m_activityLayout->takeAt(0);
            if (item->widget()) delete item->widget();
            delete item;
        }

        q.prepare(
            "SELECT * FROM ("
            "  SELECT s.maneuver_type, s.duration_seconds, s.is_successful,"
            "         CAST(s.session_start AS DATE) AS session_start,"
            "         NVL(c.brand, ' ') || ' ' || NVL(c.model, ' ') AS modele"
            "  FROM PARKING_SESSIONS s"
            "  LEFT JOIN CARS c ON s.car_id = c.id"
            "  WHERE s.student_id = :id"
            "  ORDER BY s.session_start DESC"
            ") WHERE ROWNUM <= 10");
        q.bindValue(":id", m_eleveId);

        bool hasRows = false;
        if (q.exec()) {
            while (q.next()) {
                hasRows = true;
                bool ok     = q.value("is_successful").toInt() == 1;
                QString man = q.value("maneuver_type").toString();
                int    dur  = q.value("duration_seconds").toInt();
                QString veh = q.value("modele").toString().trimmed();
                QString dat = q.value("session_start").toString();

                // Friendly names
                if      (man == "creneau")        man = "Parallel";
                else if (man == "bataille")       man = "Bataille";
                else if (man == "epi")            man = "Diagonal";
                else if (man == "marche_arriere") man = "Reverse";
                else if (man == "demi_tour")      man = "U-turn";

                QFrame *row = new QFrame(this);
                row->setStyleSheet("QFrame{background:#f8f9fa;border-radius:8px;border:none;}");
                row->setFixedHeight(44);

                QHBoxLayout *rl = new QHBoxLayout(row);
                rl->setContentsMargins(12,6,12,6);
                rl->setSpacing(10);

                QLabel *ic = new QLabel(ok ? QString::fromUtf8("\xe2\x9c\x85")
                                           : QString::fromUtf8("\xe2\x9d\x8c"), row);
                ic->setStyleSheet("QLabel{font-size:14px;background:transparent;border:none;}");
                rl->addWidget(ic);

                QLabel *nm = new QLabel(man, row);
                nm->setStyleSheet("QLabel{font-size:11px;font-weight:bold;color:#2d3436;"
                    "background:transparent;border:none;}");
                rl->addWidget(nm);

                QLabel *tm = new QLabel(QString("%1:%2")
                    .arg(dur/60).arg(dur%60, 2, 10, QChar('0')), row);
                tm->setStyleSheet("QLabel{font-size:10px;color:#636e72;"
                    "background:transparent;border:none;font-family:'Courier New',monospace;}");
                rl->addWidget(tm);

                if (!veh.isEmpty()) {
                    QLabel *vl = new QLabel(
                        QString::fromUtf8("\xf0\x9f\x9a\x97 %1").arg(veh), row);
                    vl->setStyleSheet("QLabel{font-size:9px;color:#b2bec3;"
                        "background:transparent;border:none;}");
                    rl->addWidget(vl);
                }

                rl->addStretch();

                QLabel *dt = new QLabel(dat.left(16), row);
                dt->setStyleSheet("QLabel{font-size:9px;color:#b2bec3;"
                    "background:transparent;border:none;}");
                rl->addWidget(dt);

                m_activityLayout->addWidget(row);
            }
        } else {
            qDebug() << "[Stats] session history query failed:" << q.lastError().text();
        }

        if (!hasRows) {
            QLabel *empty = new QLabel(QString::fromUtf8("No sessions recorded yet"), this);
            empty->setStyleSheet("QLabel{font-size:11px;color:#b2bec3;background:transparent;"
                "border:none;padding:20px;}");
            empty->setAlignment(Qt::AlignCenter);
            m_activityLayout->addWidget(empty);
        }
    }
}
