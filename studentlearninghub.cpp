#include "studentlearninghub.h"
#include "C:/Users/hboug/Downloads/final/SmartDrivingSchool/SmartDrivingSchool/studentportal.h"
#include "C:/Users/hboug/Downloads/final/SmartDrivingSchool/SmartDrivingSchool/circuitdb.h"
#include "wino/wino_studentdashboard.h"
#include "wino/wino_bootstrap.h"
#include "wino/thememanager.h"
#include <QHBoxLayout>
#include <QApplication>
#include <QScreen>
#include <QSqlQuery>
#include <QMessageBox>
#include <QRegularExpression>
#include <QTimer>
#include <QPointer>
#include <QGraphicsDropShadowEffect>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QLineEdit>
#include <QSlider>
#include <QSettings>
#include <QTextToSpeech>
#include <QSvgRenderer>
#include <QPainter>

// â”€â”€ Palette (theme-aware) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// All helpers read from ThemeManager at call time so they respond to toggles.
static inline bool   hubDark()        { return ThemeManager::instance()->currentTheme() == ThemeManager::Dark; }
static inline QString HUB_BG()        { return hubDark() ? "#0F172A"              : "#F1F5F9"; }
static inline QString HUB_SIDEBAR()   { return hubDark() ? "#111827"              : "#FFFFFF"; }
static inline QString HUB_TEAL()      { return "#14B8A6"; }
static inline QString HUB_BLUE()      { return "#38BDF8"; }
static inline QString HUB_TEXT()      { return hubDark() ? "#E2E8F0"              : "#111827"; }
static inline QString HUB_MUTED()     { return hubDark() ? "#64748B"              : "#6B7280"; }
static inline QString HUB_ACTIVE()    { return hubDark() ? "rgba(20,184,166,0.15)": "rgba(20,184,166,0.12)"; }
static inline QString HUB_BORDER()    { return hubDark() ? "#1E293B"              : "#E2E8F0"; }
static inline QString HUB_SUB_ACTIVE(){ return hubDark() ? "rgba(20,184,166,0.10)": "rgba(20,184,166,0.08)"; }
static inline QString HUB_LOCKED_TEXT(){ return hubDark() ? "#475569"             : "#9CA3AF"; }

// â”€â”€ loadStudentStep â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Reads progression from WINO_PROGRESS, bootstrapping from course_level when
// the row doesn't yet exist OR when the stored step is behind the enrolled level.
//
// course_level (STUDENTS table) â†’ initial WINO_PROGRESS state:
//   "Theory"    â†’ step 1, Code=IN_PROGRESS, Circuit=LOCKED,     Parking=LOCKED
//   "Practical" â†’ step 2, Code=COMPLETED,   Circuit=IN_PROGRESS, Parking=LOCKED
//   "Parking"   â†’ step 3, Code=COMPLETED,   Circuit=COMPLETED,   Parking=IN_PROGRESS
void StudentLearningHub::loadStudentStep()
{
    WinoBootstrap::bootstrap();

    // â”€â”€ 1. Read the student's registered course level â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    QString courseLevel = "Theory";   // safe default
    {
        QSqlQuery cl;
        cl.prepare("SELECT NVL(course_level,'Theory') FROM STUDENTS WHERE id = ?");
        cl.addBindValue(m_studentId);
        if (cl.exec() && cl.next())
            courseLevel = cl.value(0).toString();   // "Theory" | "Practical" | "Parking"
    }

    // Derive the minimum step & statuses that the course level implies
    int    minStep       = 1;
    QString codeStatus   = "IN_PROGRESS";
    QString circStatus   = "LOCKED";
    QString parkStatus   = "LOCKED";
    double  codeFloor    = 0.0;
    double  circFloor    = 0.0;

    if (courseLevel == "Practical") {
        minStep     = 2;
        codeStatus  = "COMPLETED";
        circStatus  = "IN_PROGRESS";
        codeFloor   = 100.0;          // Code already fully achieved
    } else if (courseLevel == "Parking") {
        minStep     = 3;
        codeStatus  = "COMPLETED";
        circStatus  = "COMPLETED";
        parkStatus  = "IN_PROGRESS";
        codeFloor   = 100.0;
        circFloor   = 100.0;          // Code + Circuit already fully achieved
    }

    // â”€â”€ 2. Check for an existing WINO_PROGRESS row â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    QSqlQuery q;
    q.prepare(
        "SELECT current_step, "
        "       NVL(code_score,0), NVL(circuit_score,0), NVL(parking_score,0) "
        "FROM WINO_PROGRESS WHERE user_id = ?");
    q.addBindValue(m_studentId);

    if (q.exec() && q.next()) {
        int    wStep  = q.value(0).toInt();
        double cScore = q.value(1).toDouble();
        double rScore = q.value(2).toDouble();
        double pScore = q.value(3).toDouble();

        // â”€â”€ 3. Correct the row if the stored step is behind course level â”€â”€â”€â”€â”€â”€
        //    (e.g. student registered as "Practical" but row was created as step=1)
        if (wStep < minStep) {
            QSqlQuery upd;
            upd.prepare(
                "UPDATE WINO_PROGRESS "
                "SET current_step    = ?,"
                "    code_status     = ?,"
                "    circuit_status  = ?,"
                "    parking_status  = ?,"
                "    code_score      = GREATEST(NVL(code_score,0),    ?),"
                "    circuit_score   = GREATEST(NVL(circuit_score,0), ?)"
                "WHERE user_id = ?");
            upd.addBindValue(minStep);
            upd.addBindValue(codeStatus);
            upd.addBindValue(circStatus);
            upd.addBindValue(parkStatus);
            upd.addBindValue(codeFloor);
            upd.addBindValue(circFloor);
            upd.addBindValue(m_studentId);
            upd.exec();

            wStep  = minStep;
            cScore = qMax(cScore, codeFloor);
            rScore = qMax(rScore, circFloor);
        }

        m_studentStep  = qBound(0, wStep - 1, 2);
        m_codeScore    = cScore;
        m_circuitScore = rScore;
        m_parkingScore = pScore;

    } else {
        // â”€â”€ 4. First ever login: insert the correct initial row â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        QSqlQuery ins;
        ins.prepare(
            "INSERT INTO WINO_PROGRESS "
            "(user_id, current_step, code_status, circuit_status, parking_status,"
            " code_score, circuit_score, parking_score, total_score) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, 0, ?)");
        ins.addBindValue(m_studentId);
        ins.addBindValue(minStep);
        ins.addBindValue(codeStatus);
        ins.addBindValue(circStatus);
        ins.addBindValue(parkStatus);
        ins.addBindValue(codeFloor);
        ins.addBindValue(circFloor);
        ins.addBindValue(codeFloor + circFloor);   // total_score
        ins.exec();

        m_studentStep  = qBound(0, minStep - 1, 2);
        m_codeScore    = codeFloor;
        m_circuitScore = circFloor;
        m_parkingScore = 0;
    }
}

// â”€â”€ syncScoresToProgress â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Reads live scores from TACHE (USER_PROGRESS) and Circuit modules,
// writes them into WINO_PROGRESS so the Sessions dashboard shows correct values.
void StudentLearningHub::syncScoresToProgress()
{
    // â”€â”€ Code score: read from TACHE USER_PROGRESS.progress_percent â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    {
        QSqlQuery q;
        q.prepare("SELECT NVL(progress_percent, 0) FROM USER_PROGRESS WHERE user_id = ?");
        q.addBindValue(m_studentId);
        if (q.exec() && q.next()) {
            double codeScore = q.value(0).toDouble();
            m_codeScore = codeScore;
            QSqlQuery upd;
            upd.prepare("UPDATE WINO_PROGRESS SET code_score = ? WHERE user_id = ?");
            upd.addBindValue(codeScore);
            upd.addBindValue(m_studentId);
            upd.exec();
        }
    }

    // â”€â”€ Circuit score: % of analysed sessions (max 25 = 100%) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // Only count sessions that have real SESSION_ANALYSIS data to avoid
    // pre-populated test rows inflating the score.
    {
        QSqlQuery q;
        q.prepare(
            "SELECT COUNT(*) FROM DRIVING_SESSIONS ds "
            "WHERE ds.student_id = ? "
            "AND EXISTS (SELECT 1 FROM SESSION_ANALYSIS sa "
            "            WHERE sa.driving_session_id = ds.driving_session_id)");
        q.addBindValue(m_studentId);
        if (q.exec() && q.next()) {
            int sessions = q.value(0).toInt();
            double circScore = qMin(100.0, sessions * 4.0);  // 25 sessions = 100%
            m_circuitScore = circScore;
            QSqlQuery upd;
            upd.prepare("UPDATE WINO_PROGRESS SET circuit_score = ? WHERE user_id = ?");
            upd.addBindValue(circScore);
            upd.addBindValue(m_studentId);
            upd.exec();
        }
    }
}

// â”€â”€ updateScoreLabel â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
void StudentLearningHub::updateScoreLabel(int activeStep)
{
    if (!m_stepScoreLbl) return;
    double score = (activeStep == 0) ? m_codeScore :
                   (activeStep == 1) ? m_circuitScore : m_parkingScore;
    QString stepName = (activeStep == 0) ? "Code" :
                       (activeStep == 1) ? "Circuit" : "Parking";
    if (score > 0)
        m_stepScoreLbl->setText(QString("Score %1 : %2%").arg(stepName).arg(score, 0, 'f', 1));
    else
        m_stepScoreLbl->setText(QString("Score %1 : —").arg(stepName));
}

// â”€â”€ Constructor â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
StudentLearningHub::StudentLearningHub(int studentId, QWidget *parent)
    : QMainWindow(parent)
    , m_studentId(studentId)
{
    loadStudentStep();

    setWindowTitle("Student Learning Hub");
    setMinimumSize(1100, 780);
    showMaximized();

    QWidget *root = new QWidget(this);
    QHBoxLayout *rootLayout = new QHBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    setCentralWidget(root);

    // â”€â”€ Left sidebar â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    m_sidebar = new QFrame(root);
    QFrame *sidebar = m_sidebar;   // local alias used throughout this constructor
    sidebar->setFixedWidth(230);
    sidebar->setStyleSheet(QString(
        "QFrame { background-color: %1; border-right: 1px solid %2; }")
        .arg(HUB_SIDEBAR(), HUB_BORDER()));

    QVBoxLayout *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    sideLayout->setSpacing(0);

    // Helper: horizontal divider
    auto addDivider = [&]() {
        QFrame *d = new QFrame(sidebar);
        d->setFrameShape(QFrame::HLine);
        d->setStyleSheet(QString("background: %1; margin: 0;").arg(HUB_BORDER()));
        d->setFixedHeight(1);
        sideLayout->addWidget(d);
    };

    // â”€â”€ Brand header — WINO logo2.png â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    QLabel *logo = new QLabel();
    logo->setAlignment(Qt::AlignCenter);
    logo->setStyleSheet("padding: 16px 0 4px 0; background: transparent;");
    {
        QPixmap lp(":/assets/logo2.png");
        if (lp.isNull())
            lp.load("C:/Users/hboug/OneDrive/Desktop/maryem/assets/logo2.png");
        if (!lp.isNull()) {
            QPixmap scaled = lp.scaledToWidth(160, Qt::SmoothTransformation);
            QImage img = scaled.toImage().convertToFormat(QImage::Format_ARGB32);
            int top = 0, bottom = img.height() - 1;
            for (int y = 0; y < img.height() && top == 0; ++y)
                for (int x = 0; x < img.width(); ++x)
                    if (qAlpha(img.pixel(x, y)) > 10) { top = y; break; }
            for (int y = img.height()-1; y >= 0 && bottom == img.height()-1; --y)
                for (int x = 0; x < img.width(); ++x)
                    if (qAlpha(img.pixel(x, y)) > 10) { bottom = y; break; }
            top    = qMax(0, top - 4);
            bottom = qMin(img.height()-1, bottom + 4);
            QPixmap cropped = scaled.copy(0, top, scaled.width(), bottom - top + 1);
            logo->setPixmap(cropped);
            logo->setFixedHeight(cropped.height() + 20);
        }
    }
    QLabel *platformSub = new QLabel("Learning Platform");
    platformSub->setAlignment(Qt::AlignCenter);
    platformSub->setStyleSheet(QString(
        "font-size: 11px; color: %1; background: transparent; "
        "padding-bottom: 10px;").arg(HUB_MUTED()));

    sideLayout->addWidget(logo);
    sideLayout->addWidget(platformSub);
    addDivider();

    // â”€â”€ MON PARCOURS â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    sideLayout->addSpacing(10);

    QLabel *parcoursLbl = new QLabel(QString::fromUtf8("  \xf0\x9f\x8e\xaf  MY PROGRESS"));
    parcoursLbl->setStyleSheet(QString(
        "font-size: 10px; font-weight: bold; color: %1; letter-spacing: 1px; "
        "background: transparent; padding: 0 16px 8px 16px;").arg(HUB_MUTED()));
    sideLayout->addWidget(parcoursLbl);

    QWidget *stepTracker = new QWidget(sidebar);
    stepTracker->setStyleSheet("background: transparent;");
    QVBoxLayout *trackerLay = new QVBoxLayout(stepTracker);
    trackerLay->setContentsMargins(12, 0, 12, 8);
    trackerLay->setSpacing(5);

    // â”€â”€ Step circles â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    QHBoxLayout *circlesRow = new QHBoxLayout();
    circlesRow->setSpacing(0);
    circlesRow->setContentsMargins(0, 0, 0, 0);

    struct StepDef { QString name; QString icon; };
    const QList<StepDef> stepDefs = {
        { "Code",    QString::fromUtf8("\xf0\x9f\x93\x96") },
        { "Circuit", QString::fromUtf8("\xf0\x9f\x9a\xa6") },
        { "Parking", QString::fromUtf8("\xf0\x9f\x85\xbf\xef\xb8\x8f") },
    };

    for (int i = 0; i < 3; ++i) {
        QVBoxLayout *col = new QVBoxLayout();
        col->setSpacing(3);
        col->setAlignment(Qt::AlignHCenter);

        QLabel *circle = new QLabel();
        circle->setFixedSize(32, 32);
        circle->setAlignment(Qt::AlignCenter);

        // Circuit (i==1) and Parking (i==2) are always accessible — never lock
        const bool isAlwaysOpen = (i >= 1);

        if (m_studentStep > i) {
            // Completed
            circle->setText(QString::fromUtf8("\xe2\x9c\x93"));
            circle->setStyleSheet(
                "background: #14B8A6; color: white; border-radius: 16px; "
                "font-size: 15px; font-weight: bold; border: none;");
        } else if (m_studentStep == i || isAlwaysOpen) {
            // In progress / always open
            circle->setText(stepDefs[i].icon);
            circle->setStyleSheet(QString(
                "background: #1E3A5F; color: %1; border-radius: 16px; "
                "font-size: 14px; border: 2px solid %1;").arg(HUB_BLUE()));
        } else {
            // Locked (only Circuit can be locked)
            circle->setText(QString::fromUtf8("\xf0\x9f\x94\x92"));
            circle->setStyleSheet(
                "background: #1E293B; color: #475569; border-radius: 16px; "
                "font-size: 11px; border: 2px solid #334155;");
        }

        QLabel *nameLbl = new QLabel(stepDefs[i].name);
        nameLbl->setAlignment(Qt::AlignCenter);
        nameLbl->setStyleSheet(QString(
            "font-size: 9px; font-weight: %1; color: %2; background: transparent;")
            .arg((m_studentStep >= i || isAlwaysOpen) ? "bold" : "normal")
            .arg((m_studentStep >= i || isAlwaysOpen) ? HUB_TEXT() : HUB_MUTED()));

        col->addWidget(circle, 0, Qt::AlignHCenter);
        col->addWidget(nameLbl, 0, Qt::AlignHCenter);
        circlesRow->addLayout(col, 2);

        if (i < 2) {
            // Connector line
            QFrame *connector = new QFrame();
            connector->setFixedSize(18, 2);
            connector->setStyleSheet(QString("background: %1; margin-bottom: 14px;")
                .arg(m_studentStep > i ? "#14B8A6" : "#334155"));
            circlesRow->addWidget(connector, 0, Qt::AlignVCenter);
        }
    }
    trackerLay->addLayout(circlesRow);

    // Progress bar
    QProgressBar *progBar = new QProgressBar();
    progBar->setRange(0, 2);
    progBar->setValue(m_studentStep);
    progBar->setFixedHeight(5);
    progBar->setTextVisible(false);
    progBar->setStyleSheet(
        "QProgressBar { background: #1E293B; border-radius: 2px; border: none; }"
        "QProgressBar::chunk { background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        " stop:0 #14B8A6, stop:1 #38BDF8); border-radius: 2px; }");
    trackerLay->addWidget(progBar);

    // Progression percentage label
    int pct = (m_studentStep == 0 ? 0 : m_studentStep == 1 ? 33 : 100);
    QLabel *pctLbl = new QLabel(QString("Progress: %1%").arg(pct));
    pctLbl->setAlignment(Qt::AlignCenter);
    pctLbl->setStyleSheet(QString(
        "font-size: 10px; color: %1; background: transparent;").arg(HUB_MUTED()));
    trackerLay->addWidget(pctLbl);

    // Step score label (updates when navigating tabs)
    m_stepScoreLbl = new QLabel();
    m_stepScoreLbl->setAlignment(Qt::AlignCenter);
    m_stepScoreLbl->setStyleSheet(QString(
        "font-size: 11px; font-weight: bold; color: %1; "
        "background: transparent;").arg(HUB_TEAL()));
    trackerLay->addWidget(m_stepScoreLbl);
    updateScoreLabel(0);

    sideLayout->addWidget(stepTracker);
    addDivider();
    sideLayout->addSpacing(8);

    // â”€â”€ NAVIGATION section â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    QLabel *navLabel = new QLabel("  NAVIGATION");
    navLabel->setStyleSheet(QString(
        "font-size: 10px; font-weight: bold; color: %1; letter-spacing: 2px; "
        "background: transparent; padding: 0 20px 8px 20px;").arg(HUB_MUTED()));
    sideLayout->addWidget(navLabel);

    const bool circuitLocked = false;  // unlocked for all students

    m_theoryBtn   = makeStepNavBtn(sidebar, 1, QString::fromUtf8("\xf0\x9f\x93\x96"), "Code / Theory", false);
    // Add collapsed arrow to theory button
    m_theoryBtn->setText(QString::fromUtf8("  \xf0\x9f\x93\x96  1 \xc2\xb7 Code / Theory  \xe2\x96\xb8"));
    m_circuitBtn  = makeStepNavBtn(sidebar, 2, QString::fromUtf8("\xf0\x9f\x9a\xa6"), "Circuit",        circuitLocked);
    m_circuitBtn->setText(QString::fromUtf8("  \xf0\x9f\x9a\xa6  2 \xc2\xb7 Circuit  \xe2\x96\xb8"));

    m_parkingBtn   = makeStepNavBtn(sidebar, 3, QString::fromUtf8("\xf0\x9f\x85\xbf\xef\xb8\x8f"), "Parking",        false);
    m_parkingBtn->setText(QString::fromUtf8("  \xf0\x9f\x85\xbf\xef\xb8\x8f  3 \xc2\xb7 Parking  \xe2\x96\xb8"));
    m_sessionsBtn = makeStepNavBtn(sidebar, 4, QString::fromUtf8("\xf0\x9f\x93\x85"), "Sessions",       false);
    m_sessionsBtn->setText(QString::fromUtf8("  \xf0\x9f\x93\x85  4 \xc2\xb7 Sessions  \xe2\x96\xb8"));

    // â”€â”€ Accordion sub-nav (sits directly below Code/Theory button) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    m_theorySubNav = new QWidget(sidebar);
    m_theorySubNav->setStyleSheet(QString(
        "QWidget { background: %1; border-left: 2px solid %2; margin-left: 18px; }")
        .arg(HUB_BG(), HUB_TEAL()));
    m_theorySubNav->setVisible(false);   // collapsed by default

    QVBoxLayout *subLayout = new QVBoxLayout(m_theorySubNav);
    subLayout->setContentsMargins(0, 4, 0, 8);
    subLayout->setSpacing(0);

    struct SubItem { QString icon; QString label; int idx; };
    const QList<SubItem> subItems = {
        { QString::fromUtf8("\xf0\x9f\x93\x8b"), "Dashboard",    0 },
        { QString::fromUtf8("\xf0\x9f\x8f\x86"), "Badges",       1 },
        { QString::fromUtf8("\xf0\x9f\x93\xb7"), "Sign Scanner", 2 },
    };

    for (const auto &item : subItems) {
        QPushButton *btn = new QPushButton(m_theorySubNav);
        btn->setText(QString("  %1  %2").arg(item.icon, item.label));
        btn->setFixedHeight(36);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString(
            "QPushButton { background: transparent; color: %1; "
            "border: none; border-left: 3px solid transparent; "
            "text-align: left; padding-left: 16px; "
            "font-size: 12px; font-weight: 500; border-radius: 0; }"
            "QPushButton:hover { background: rgba(20,184,166,0.08); color: %2; }")
            .arg(HUB_MUTED(), HUB_TEAL()));
        subLayout->addWidget(btn);
        m_theoryNavBtns.append(btn);

        int idx = item.idx;
        connect(btn, &QPushButton::clicked, this, [this, idx]() {
            setActiveTheoryBtn(idx);
            if (m_theoryPage) m_theoryPage->navigateToIndex(idx);
        });
    }

    // â”€â”€ Sessions accordion sub-nav â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    m_sessionsSubNav = new QWidget(sidebar);
    m_sessionsSubNav->setStyleSheet(QString(
        "QWidget { background: %1; border-left: 2px solid %2; margin-left: 18px; }")
        .arg(HUB_BG(), HUB_TEAL()));
    m_sessionsSubNav->setVisible(false);

    QVBoxLayout *sessSubLayout = new QVBoxLayout(m_sessionsSubNav);
    sessSubLayout->setContentsMargins(0, 4, 0, 8);
    sessSubLayout->setSpacing(0);

    struct SessItem { QString icon; QString label; int idx; };
    const QList<SessItem> sessItems = {
        { QString::fromUtf8("\xf0\x9f\x8f\xa0"), "Dashboard",        0 },
        { QString::fromUtf8("\xf0\x9f\x93\x85"), "Book Session",     1 },
        { QString::fromUtf8("\xe2\x9c\xa8"), "AI Recommendation",2 },
        { QString::fromUtf8("\xf0\x9f\x97\x93"), "Calendar",         3 },
    };

    for (const auto &item : sessItems) {
        QPushButton *btn = new QPushButton(m_sessionsSubNav);
        btn->setText(QString("  %1  %2").arg(item.icon, item.label));
        btn->setFixedHeight(36);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString(
            "QPushButton { background: transparent; color: %1; "
            "border: none; border-left: 3px solid transparent; "
            "text-align: left; padding-left: 16px; "
            "font-size: 12px; font-weight: 500; border-radius: 0; }"
            "QPushButton:hover { background: rgba(20,184,166,0.08); color: %2; }")
            .arg(HUB_MUTED(), HUB_TEAL()));
        sessSubLayout->addWidget(btn);

        int idx = item.idx;
        connect(btn, &QPushButton::clicked, this, [this, idx, btn]() {
            // Highlight active sub-item
            for (int c = 0; c < m_sessionsSubNav->layout()->count(); ++c) {
                if (auto *b = qobject_cast<QPushButton*>(
                        m_sessionsSubNav->layout()->itemAt(c)->widget())) {
                    bool active = (b == btn);
                    b->setStyleSheet(active
                        ? QString("QPushButton{background:rgba(20,184,166,0.10);color:%1;"
                                  "border:none;border-left:3px solid %1;"
                                  "text-align:left;padding-left:13px;"
                                  "font-size:12px;font-weight:600;border-radius:0;}").arg(HUB_TEAL())
                        : QString("QPushButton{background:transparent;color:%1;"
                                  "border:none;border-left:3px solid transparent;"
                                  "text-align:left;padding-left:16px;"
                                  "font-size:12px;font-weight:500;border-radius:0;}"
                                  "QPushButton:hover{background:rgba(20,184,166,0.08);color:%2;}")
                          .arg(HUB_MUTED(), HUB_TEAL()));
                }
            }
            if (m_sessionsPage) m_sessionsPage->navigateToSection(idx);
        });
    }

    // â”€â”€ Parking accordion sub-nav â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    m_parkingSubNav = new QWidget(sidebar);
    m_parkingSubNav->setStyleSheet(QString(
        "QWidget { background: %1; border-left: 2px solid %2; margin-left: 18px; }")
        .arg(HUB_BG(), HUB_TEAL()));
    m_parkingSubNav->setVisible(false);

    QVBoxLayout *pkSubLayout = new QVBoxLayout(m_parkingSubNav);
    pkSubLayout->setContentsMargins(0, 4, 0, 8);
    pkSubLayout->setSpacing(0);

    struct PkItem { QString icon; QString label; int idx; };
    const QList<PkItem> pkItems = {
        { QString::fromUtf8("\xf0\x9f\x8f\xa0"),  "Dashboard",      0 },
        { QString::fromUtf8("\xf0\x9f\x8e\xac"),  "Tutoriels",      1 },
        { QString::fromUtf8("\xf0\x9f\x93\x9c"),  "Historique",     2 },
        { QString::fromUtf8("\xf0\x9f\x8f\x86"),  "Badges",         3 },
    };

    for (const auto &item : pkItems) {
        QPushButton *btn = new QPushButton(m_parkingSubNav);
        btn->setText(QString("  %1  %2").arg(item.icon, item.label));
        btn->setFixedHeight(36);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString(
            "QPushButton { background: transparent; color: %1; "
            "border: none; border-left: 3px solid transparent; "
            "text-align: left; padding-left: 16px; "
            "font-size: 12px; font-weight: 500; border-radius: 0; }"
            "QPushButton:hover { background: rgba(20,184,166,0.08); color: %2; }")
            .arg(HUB_MUTED(), HUB_TEAL()));
        pkSubLayout->addWidget(btn);

        int idx = item.idx;
        connect(btn, &QPushButton::clicked, this, [this, idx, btn]() {
            // Highlight active
            for (int c = 0; c < m_parkingSubNav->layout()->count(); ++c) {
                if (auto *b = qobject_cast<QPushButton*>(
                        m_parkingSubNav->layout()->itemAt(c)->widget())) {
                    bool active = (b == btn);
                    b->setStyleSheet(active
                        ? QString("QPushButton{background:rgba(20,184,166,0.10);color:%1;"
                                  "border:none;border-left:3px solid %1;"
                                  "text-align:left;padding-left:13px;"
                                  "font-size:12px;font-weight:600;border-radius:0;}")
                                  .arg(HUB_TEAL())
                        : QString("QPushButton{background:transparent;color:%1;"
                                  "border:none;border-left:3px solid transparent;"
                                  "text-align:left;padding-left:16px;"
                                  "font-size:12px;font-weight:500;border-radius:0;}"
                                  "QPushButton:hover{background:rgba(20,184,166,0.08);color:%2;}")
                          .arg(HUB_MUTED(), HUB_TEAL()));
                }
            }
            if (m_parkingPage) m_parkingPage->navigateParkingSection(idx);
            m_stack->setCurrentIndex(2);  // ensure parking page is visible
        });
    }

    // â”€â”€ Circuit accordion sub-nav â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    m_circuitSubNav = new QWidget(sidebar);
    m_circuitSubNav->setStyleSheet(QString(
        "QWidget { background: %1; border-left: 2px solid %2; margin-left: 18px; }")
        .arg(HUB_BG(), HUB_TEAL()));
    m_circuitSubNav->setVisible(false);

    QVBoxLayout *circSubLayout = new QVBoxLayout(m_circuitSubNav);
    circSubLayout->setContentsMargins(0, 4, 0, 8);
    circSubLayout->setSpacing(0);

    struct CircSubItem { QString icon; QString label; int tabIdx; };
    const QList<CircSubItem> circItems = {
        { QString::fromUtf8("\xf0\x9f\x93\x8a"), "Dashboard",   0 },
        { QString::fromUtf8("\xf0\x9f\x93\x8b"), "My Sessions", 1 },
        { QString::fromUtf8("\xf0\x9f\x93\x88"), "Progress",    2 },
    };

    for (const CircSubItem &item : circItems) {
        QPushButton *btn = new QPushButton(m_circuitSubNav);
        btn->setText(QString("    %1  %2").arg(item.icon, item.label));
        btn->setFixedHeight(34);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString(
            "QPushButton { background: transparent; color: %1; "
            "border: none; text-align: left; padding-left: 8px; "
            "font-size: 12px; font-weight: 500; border-radius: 0; }"
            "QPushButton:hover { background: %2; color: %3; }")
            .arg(HUB_MUTED(), HUB_SUB_ACTIVE(), HUB_TEAL()));
        int tabIdx = item.tabIdx;
        connect(btn, &QPushButton::clicked, this, [this, tabIdx]() {
            // Ensure circuit page is loaded first
            if (!m_circuitPage) showCircuit();
            m_stack->setCurrentIndex(1);
            // Navigate to the correct tab in StudentPortal
            StudentPortal *portal = qobject_cast<StudentPortal*>(m_circuitPage);
            if (portal) portal->navigateToTab(tabIdx);
        });
        circSubLayout->addWidget(btn);
    }

    // Insert in layout: Theory â†’ sub-nav â†’ Circuit â†’ circuitSubNav â†’ Parking â†’ ParkingSubNav â†’ Sessions â†’ sub-nav
    sideLayout->addWidget(m_theoryBtn);
    sideLayout->addWidget(m_theorySubNav);
    sideLayout->addWidget(m_circuitBtn);
    sideLayout->addWidget(m_circuitSubNav);
    sideLayout->addWidget(m_parkingBtn);
    sideLayout->addWidget(m_parkingSubNav);
    sideLayout->addWidget(m_sessionsBtn);
    sideLayout->addWidget(m_sessionsSubNav);
    sideLayout->addStretch();

    // â”€â”€ Theme toggle (bottom of sidebar) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    addDivider();
    {
        bool isDark = ThemeManager::instance()->currentTheme() == ThemeManager::Dark;

        m_themeBtn = new QPushButton(sidebar);
        m_themeBtn->setFixedHeight(44);
        m_themeBtn->setCursor(Qt::PointingHandCursor);
        m_themeBtn->setToolTip("Toggle light / dark mode");
        m_themeBtn->setText(isDark
            ? QString::fromUtf8("  \xe2\x98\x80\xef\xb8\x8f  Light mode")
            : QString::fromUtf8("  \xf0\x9f\x8c\x99  Dark mode"));
        m_themeBtn->setStyleSheet(QString(
            "QPushButton { background: transparent; color: %1; "
            "border: none; border-left: 3px solid transparent; "
            "text-align: left; padding-left: 16px; "
            "font-size: 13px; font-weight: 500; border-radius: 0; }"
            "QPushButton:hover { background: rgba(128,128,128,0.08); color: %2; }")
            .arg(HUB_MUTED(), HUB_TEXT()));
        connect(m_themeBtn, &QPushButton::clicked, []() {
            ThemeManager::instance()->toggleTheme();
        });
        sideLayout->addWidget(m_themeBtn);
    }

    // â”€â”€ Settings button (bottom of sidebar) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    {
        m_settingsBtn = new QPushButton(sidebar);
        m_settingsBtn->setFixedHeight(44);
        m_settingsBtn->setCursor(Qt::PointingHandCursor);
        m_settingsBtn->setText(QString::fromUtf8("  \xe2\x9a\x99\xef\xb8\x8f  Settings"));
        m_settingsBtn->setStyleSheet(QString(
            "QPushButton { background: transparent; color: %1; "
            "border: none; border-left: 3px solid transparent; "
            "text-align: left; padding-left: 16px; "
            "font-size: 13px; font-weight: 500; border-radius: 0; }"
            "QPushButton:hover { background: rgba(128,128,128,0.08); color: %2; }")
            .arg(HUB_MUTED(), HUB_TEXT()));
        connect(m_settingsBtn, &QPushButton::clicked, this, &StudentLearningHub::showSettings);
        sideLayout->addWidget(m_settingsBtn);
    }

    // â”€â”€ Logout button (bottom of sidebar) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    {
        m_logoutBtn = new QPushButton(sidebar);
        m_logoutBtn->setFixedHeight(44);
        m_logoutBtn->setCursor(Qt::PointingHandCursor);
        m_logoutBtn->setText(QString::fromUtf8("  \xf0\x9f\x9a\xaa  Logout"));
        m_logoutBtn->setStyleSheet(
            "QPushButton { background: transparent; color: #e17055; "
            "border: none; border-left: 3px solid transparent; "
            "text-align: left; padding-left: 16px; "
            "font-size: 13px; font-weight: 500; border-radius: 0; }"
            "QPushButton:hover { background: rgba(225,112,85,0.10); color: #d63031; }");
        connect(m_logoutBtn, &QPushButton::clicked, this, [this]() {
            emit logoutRequested();
        });
        sideLayout->addWidget(m_logoutBtn);
        sideLayout->addSpacing(8);
    }

    // â”€â”€ Main content stack â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    m_stack = new QStackedWidget(root);
    m_stack->setStyleSheet(QString("background: %1;").arg(HUB_BG()));
    m_stack->addWidget(new QWidget());  // index 0 = theory    placeholder
    m_stack->addWidget(new QWidget());  // index 1 = circuit   placeholder
    m_stack->addWidget(new QWidget());  // index 2 = parking   placeholder
    m_stack->addWidget(new QWidget());  // index 3 = sessions  placeholder
    m_stack->addWidget(new QWidget());  // index 4 = assistant placeholder
    m_stack->addWidget(createSettingsPage()); // index 5 = settings

    rootLayout->addWidget(sidebar);
    rootLayout->addWidget(m_stack, 1);

    connect(m_theoryBtn,   &QPushButton::clicked, this, &StudentLearningHub::showTheory);
    connect(m_circuitBtn,  &QPushButton::clicked, this, &StudentLearningHub::showCircuit);
    connect(m_sessionsBtn, &QPushButton::clicked, this, &StudentLearningHub::showSessions);
    connect(m_parkingBtn,  &QPushButton::clicked, this, &StudentLearningHub::showParking);
    connect(m_settingsBtn, &QPushButton::clicked, this, &StudentLearningHub::showSettings);

    // â”€â”€ Floating assistant (FAB + chat panel) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    createFloatingAssistant(root);

    // â”€â”€ Theme: redraw sidebar whenever the user toggles light/dark â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &StudentLearningHub::applyTheme);

    // â”€â”€ Apply saved theme immediately on startup â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // Ensures the app opens in the correct light/dark state from QSettings.
    applyTheme();

    // â”€â”€ Navigate directly to the student's current step on login â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // m_studentStep: 0=Code, 1=Circuit, 2=Sessions/Parking
    if (m_studentStep >= 2)
        showParking();     // Parking course level â†’ land on Parking
    else if (m_studentStep == 1)
        showCircuit();     // Practical course level â†’ land on Circuit
    else
        showTheory();      // Theory (default) â†’ land on Code
}

// â”€â”€ makeStepNavBtn â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
QPushButton* StudentLearningHub::makeStepNavBtn(QWidget *parent, int stepNum,
                                                const QString &icon,
                                                const QString &label, bool locked)
{
    QPushButton *btn = new QPushButton(parent);
    btn->setFixedHeight(48);

    if (locked) {
        btn->setText(QString("  %1  %2 Â· %3")
            .arg(QString::fromUtf8("\xf0\x9f\x94\x92")).arg(stepNum).arg(label));
        btn->setEnabled(false);
        btn->setCursor(Qt::ForbiddenCursor);
        btn->setToolTip("Complete the previous step to unlock this section.");
        btn->setStyleSheet(QString(
            "QPushButton { background: transparent; color: %1; "
            "border: none; border-left: 3px solid transparent; "
            "text-align: left; padding-left: 16px; "
            "font-size: 13px; font-weight: 400; border-radius: 0; }")
            .arg(HUB_LOCKED_TEXT()));
    } else {
        btn->setText(QString("  %1  %2 Â· %3").arg(icon).arg(stepNum).arg(label));
        btn->setEnabled(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString(
            "QPushButton { background: transparent; color: %1; "
            "border: none; border-left: 3px solid transparent; "
            "text-align: left; padding-left: 16px; "
            "font-size: 14px; font-weight: 500; border-radius: 0; }"
            "QPushButton:hover { background: rgba(128,128,128,0.08); color: %2; }")
            .arg(HUB_MUTED(), HUB_TEXT()));
    }
    return btn;
}

// â”€â”€ setActiveBtn â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
void StudentLearningHub::setActiveBtn(QPushButton *active)
{
    m_activeMainBtn = active;   // remember for applyTheme()
    struct BtnInfo { QPushButton *btn; bool locked; };
    const QList<BtnInfo> btns = {
        { m_theoryBtn,   false             },
        { m_circuitBtn,  false },  // unlocked for all students
        { m_sessionsBtn, false             },  // Sessions always accessible
        { m_parkingBtn,  false             },  // Parking always accessible
    };

    for (const auto &bi : btns) {
        if (bi.locked) continue;  // never restyle a locked button

        if (bi.btn == active) {
            bi.btn->setStyleSheet(QString(
                "QPushButton { background: %1; color: %2; "
                "border-left: 3px solid %2; text-align: left; padding-left: 13px; "
                "font-size: 14px; font-weight: 600; border-radius: 0; "
                "border-top: none; border-right: none; border-bottom: none; }")
                .arg(HUB_ACTIVE(), HUB_TEAL()));
        } else {
            bi.btn->setStyleSheet(QString(
                "QPushButton { background: transparent; color: %1; "
                "border: none; border-left: 3px solid transparent; "
                "text-align: left; padding-left: 16px; "
                "font-size: 14px; font-weight: 500; border-radius: 0; }"
                "QPushButton:hover { background: rgba(128,128,128,0.08); color: %2; }")
                .arg(HUB_MUTED(), HUB_TEXT()));
        }
    }
    // Keep FAB always on top regardless of which page is shown
    if (m_fabBtn)   m_fabBtn->raise();
    if (m_chatPanel && m_chatOpen) m_chatPanel->raise();
}

// â”€â”€ setActiveTheoryBtn â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
void StudentLearningHub::setActiveTheoryBtn(int index)
{
    m_activeTheoryIdx = index;
    for (int i = 0; i < m_theoryNavBtns.size(); i++) {
        if (i == index) {
            m_theoryNavBtns[i]->setStyleSheet(QString(
                "QPushButton { background: %1; color: %2; "
                "border-left: 3px solid %2; text-align: left; padding-left: 15px; "
                "font-size: 13px; font-weight: 600; border-radius: 0; "
                "border-top: none; border-right: none; border-bottom: none; }")
                .arg(HUB_SUB_ACTIVE(), HUB_TEAL()));
        } else {
            m_theoryNavBtns[i]->setStyleSheet(QString(
                "QPushButton { background: transparent; color: %1; "
                "border: none; border-left: 3px solid transparent; "
                "text-align: left; padding-left: 18px; "
                "font-size: 13px; font-weight: 500; border-radius: 0; }"
                "QPushButton:hover { background: rgba(128,128,128,0.08); color: %2; }")
                .arg(HUB_MUTED(), HUB_TEXT()));
        }
    }
}

// â”€â”€ showTheory â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
void StudentLearningHub::showTheory()
{
    bool alreadyOpen = m_theorySubNav->isVisible();
    setActiveBtn(m_theoryBtn);
    // Collapse sessions + parking + circuit accordions when switching to theory
    if (m_sessionsSubNav) m_sessionsSubNav->setVisible(false);
    m_sessionsBtn->setText(QString::fromUtf8("  \xf0\x9f\x93\x85  4 \xc2\xb7 Sessions  \xe2\x96\xb8"));
    if (m_parkingSubNav)  m_parkingSubNav->setVisible(false);
    m_parkingBtn->setText(QString::fromUtf8("  \xf0\x9f\x85\xbf\xef\xb8\x8f  3 \xc2\xb7 Parking  \xe2\x96\xb8"));
    if (m_circuitSubNav)  m_circuitSubNav->setVisible(false);
    m_circuitBtn->setText(QString::fromUtf8("  \xf0\x9f\x9a\xa6  2 \xc2\xb7 Circuit  \xe2\x96\xb8"));
    // Toggle accordion — clicking again collapses it
    bool open = !alreadyOpen;
    m_theorySubNav->setVisible(open);
    // Update arrow on the button
    m_theoryBtn->setText(open
        ? QString::fromUtf8("  \xf0\x9f\x93\x96  Code / Theory  \xe2\x96\xbe")
        : QString::fromUtf8("  \xf0\x9f\x93\x96  Code / Theory  \xe2\x96\xb8"));
    updateScoreLabel(0);
    if (!open) return;   // collapsed — don't switch page

    if (!m_theoryPage) {
        m_theoryPage = new SmartDriveWindow(m_studentId, this);
        m_stack->removeWidget(m_stack->widget(0));
        m_stack->insertWidget(0, m_theoryPage);
    }
    m_stack->setCurrentIndex(0);
    setActiveTheoryBtn(m_activeTheoryIdx);
}

// â”€â”€ showCircuit â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
void StudentLearningHub::showCircuit()
{
    // Collapse all other accordions
    m_theorySubNav->setVisible(false);
    m_theoryBtn->setText(QString::fromUtf8("  \xf0\x9f\x93\x96  1 \xc2\xb7 Code / Theory  \xe2\x96\xb8"));
    if (m_sessionsSubNav) m_sessionsSubNav->setVisible(false);
    m_sessionsBtn->setText(QString::fromUtf8("  \xf0\x9f\x93\x85  4 \xc2\xb7 Sessions  \xe2\x96\xb8"));
    if (m_parkingSubNav)  m_parkingSubNav->setVisible(false);
    m_parkingBtn->setText(QString::fromUtf8("  \xf0\x9f\x85\xbf\xef\xb8\x8f  3 \xc2\xb7 Parking  \xe2\x96\xb8"));

    // Toggle circuit accordion
    bool circAlreadyOpen = m_circuitSubNav && m_circuitSubNav->isVisible();
    bool circOpen = !circAlreadyOpen;
    if (m_circuitSubNav) m_circuitSubNav->setVisible(circOpen);
    m_circuitBtn->setText(circOpen
        ? QString::fromUtf8("  \xf0\x9f\x9a\xa6  2 \xc2\xb7 Circuit  \xe2\x96\xbe")
        : QString::fromUtf8("  \xf0\x9f\x9a\xa6  2 \xc2\xb7 Circuit  \xe2\x96\xb8"));

    setActiveBtn(m_circuitBtn);
    updateScoreLabel(1);

    if (!m_circuitPage) {
        if (!CircuitDB::instance()->isOpen()) {
            OracleConnectionParams p;
            p.drivingSchoolId = 1;
            p.instructorId    = 1;
            CircuitDB::instance()->initialize(p);
        }

        StudentProfile sp = CircuitDB::instance()->getStudent(m_studentId);
        if (sp.id <= 0) {
            sp.id        = m_studentId;
            sp.firstName = "Student";
            sp.lastName  = QString::number(m_studentId);
            sp.name      = sp.firstName + " " + sp.lastName;
        }

        StudentPortal *portal = new StudentPortal(sp, this);
        m_circuitPage = portal;
        m_stack->removeWidget(m_stack->widget(1));
        m_stack->insertWidget(1, m_circuitPage);
        // Re-apply theme after the global stylesheet settles (singleShot(0) fires first)
        QTimer::singleShot(50, portal, [portal]() { portal->applyTheme(); });
    }
    m_stack->setCurrentIndex(1);
}

// â”€â”€ showSessions â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
void StudentLearningHub::showSessions()
{
    // Sync live scores from TACHE + Circuit â†’ WINO_PROGRESS before displaying
    syncScoresToProgress();

    bool sessAlreadyOpen = m_sessionsSubNav && m_sessionsSubNav->isVisible();
    setActiveBtn(m_sessionsBtn);
    m_theorySubNav->setVisible(false);
    m_theoryBtn->setText(QString::fromUtf8("  \xf0\x9f\x93\x96  1 \xc2\xb7 Code / Theory  \xe2\x96\xb8"));
    if (m_circuitSubNav)  m_circuitSubNav->setVisible(false);
    m_circuitBtn->setText(QString::fromUtf8("  \xf0\x9f\x9a\xa6  2 \xc2\xb7 Circuit  \xe2\x96\xb8"));
    if (m_parkingSubNav)  m_parkingSubNav->setVisible(false);
    m_parkingBtn->setText(QString::fromUtf8("  \xf0\x9f\x85\xbf\xef\xb8\x8f  3 \xc2\xb7 Parking  \xe2\x96\xb8"));
    // Toggle sessions accordion
    bool sessOpen = !sessAlreadyOpen;
    if (m_sessionsSubNav) m_sessionsSubNav->setVisible(sessOpen);
    m_sessionsBtn->setText(sessOpen
        ? QString::fromUtf8("  \xf0\x9f\x93\x85  4 \xc2\xb7 Sessions  \xe2\x96\xbe")
        : QString::fromUtf8("  \xf0\x9f\x93\x85  4 \xc2\xb7 Sessions  \xe2\x96\xb8"));
    updateScoreLabel(m_studentStep);

    // Lazy-load once (same pattern as showTheory / showCircuit / showParking)
    if (!m_sessionsPage) {
        WinoBootstrap::bootstrap();
        qApp->setProperty("currentUserId", m_studentId);

        m_sessionsPage = new WinoStudentDashboard(this);
        m_stack->removeWidget(m_stack->widget(3));   // remove placeholder at index 3
        m_stack->insertWidget(3, m_sessionsPage);
    }
    m_stack->setCurrentIndex(3);
}

// â”€â”€ showParking â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
void StudentLearningHub::showParking()
{
    bool pkAlreadyOpen = m_parkingSubNav && m_parkingSubNav->isVisible();
    setActiveBtn(m_parkingBtn);
    m_theorySubNav->setVisible(false);
    m_theoryBtn->setText(QString::fromUtf8("  \xf0\x9f\x93\x96  1 \xc2\xb7 Code / Theory  \xe2\x96\xb8"));
    if (m_circuitSubNav)  m_circuitSubNav->setVisible(false);
    m_circuitBtn->setText(QString::fromUtf8("  \xf0\x9f\x9a\xa6  2 \xc2\xb7 Circuit  \xe2\x96\xb8"));
    if (m_sessionsSubNav) m_sessionsSubNav->setVisible(false);
    m_sessionsBtn->setText(QString::fromUtf8("  \xf0\x9f\x93\x85  4 \xc2\xb7 Sessions  \xe2\x96\xb8"));

    // Toggle parking accordion
    bool pkOpen = !pkAlreadyOpen;
    if (m_parkingSubNav) m_parkingSubNav->setVisible(pkOpen);
    m_parkingBtn->setText(pkOpen
        ? QString::fromUtf8("  \xf0\x9f\x85\xbf\xef\xb8\x8f  3 \xc2\xb7 Parking  \xe2\x96\xbe")
        : QString::fromUtf8("  \xf0\x9f\x85\xbf\xef\xb8\x8f  3 \xc2\xb7 Parking  \xe2\x96\xb8"));
    updateScoreLabel(m_studentStep);

    if (!m_parkingPage) {
        ParkingDBManager::instance().initialize("", "", "");
        m_parkingPage = new ParkingWidget(
            QString("Student"),
            QString("Eleve"),
            m_studentId,
            this
        );
        m_stack->removeWidget(m_stack->widget(2));
        m_stack->insertWidget(2, m_parkingPage);
    }
    m_stack->setCurrentIndex(2);
}

// â”€â”€ createFloatingAssistant â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
void StudentLearningHub::createFloatingAssistant(QWidget *container)
{
    // â”€â”€ FAB button â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    m_fabBtn = new QPushButton(container);
    m_fabBtn->setFixedSize(60, 60);
    m_fabBtn->setCursor(Qt::PointingHandCursor);
    m_fabBtn->setToolTip("3am Jalel");
    m_fabBtn->setText("");
    {
        QPixmap pm(60, 60);
        pm.fill(Qt::transparent);
        QSvgRenderer renderer(QString(":/assets/jalel.svg"));
        QPainter painter(&pm);
        renderer.render(&painter);
        painter.end();
        m_fabBtn->setIcon(QIcon(pm));
        m_fabBtn->setIconSize(QSize(60, 60));
    }
    m_fabBtn->setStyleSheet(
        "QPushButton {"
        "  background: transparent;"
        "  border: none;"
        "}"
        "QPushButton:hover { background: transparent; }"
        "QPushButton:pressed { background: transparent; }");
    auto *fabShadow = new QGraphicsDropShadowEffect(m_fabBtn);
    fabShadow->setBlurRadius(20);
    fabShadow->setColor(QColor(0,0,0,60));
    fabShadow->setOffset(0,4);
    m_fabBtn->setGraphicsEffect(fabShadow);
    m_fabBtn->raise();
    m_fabBtn->show();

    // â”€â”€ Chat panel â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    m_chatPanel = new QFrame(container);
    m_chatPanel->setObjectName("chatPanel");
    m_chatPanel->setFixedSize(370, 520);
    m_chatPanel->setStyleSheet(
        "QFrame#chatPanel {"
        "  background:#ffffff;"
        "  border-radius:18px;"
        "  border:1px solid #e2e8f0;"
        "}");
    auto *panelShadow = new QGraphicsDropShadowEffect(m_chatPanel);
    panelShadow->setBlurRadius(32);
    panelShadow->setColor(QColor(0,0,0,45));
    panelShadow->setOffset(0,8);
    m_chatPanel->setGraphicsEffect(panelShadow);

    QVBoxLayout *panelLayout = new QVBoxLayout(m_chatPanel);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->setSpacing(0);

    // Panel header
    QFrame *panelHdr = new QFrame();
    panelHdr->setObjectName("panelHdr");
    panelHdr->setFixedHeight(54);
    panelHdr->setStyleSheet(
        "QFrame#panelHdr {"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "    stop:0 #14B8A6, stop:1 #0d9488);"
        "  border-radius: 18px 18px 0 0;"
        "  border: none;"
        "}");
    QHBoxLayout *hdrL = new QHBoxLayout(panelHdr);
    hdrL->setContentsMargins(16, 0, 10, 0);

    QLabel *botIcon = new QLabel();
    {
        QPixmap pm(":/assets/jalel.svg");
        if (!pm.isNull())
            botIcon->setPixmap(pm.scaled(36, 36, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        else
            botIcon->setText(QString::fromUtf8("\xf0\x9f\x91\xb4"));
    }
    botIcon->setStyleSheet("background:transparent;border:none;");

    QLabel *botTitle = new QLabel(QString::fromUtf8("3am Jalel"));
    botTitle->setStyleSheet(
        "color:#ffffff;font-size:14px;font-weight:700;"
        "background:transparent;border:none;");

    QLabel *onlineDot = new QLabel(QString::fromUtf8("\xe2\x80\xa2 Online"));
    onlineDot->setStyleSheet(
        "color:rgba(255,255,255,0.75);font-size:11px;"
        "background:transparent;border:none;");

    QPushButton *closeBtn = new QPushButton(QString::fromUtf8("\xe2\x9c\x95"));
    closeBtn->setFixedSize(30, 30);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet(
        "QPushButton{background:rgba(255,255,255,0.18);color:#ffffff;"
        "border:none;border-radius:15px;font-size:13px;}"
        "QPushButton:hover{background:rgba(255,255,255,0.30);}");

    QVBoxLayout *titleCol = new QVBoxLayout();
    titleCol->setSpacing(1);
    titleCol->addWidget(botTitle);
    titleCol->addWidget(onlineDot);

    hdrL->addWidget(botIcon);
    hdrL->addSpacing(8);
    hdrL->addLayout(titleCol, 1);
    hdrL->addWidget(closeBtn);

    // Chat content
    WinoAssistantWidget *chatWidget = new WinoAssistantWidget(
        "Student", "Eleve", m_chatPanel);

    panelLayout->addWidget(panelHdr);
    panelLayout->addWidget(chatWidget, 1);

    m_chatPanel->hide();
    m_chatPanel->raise();

    // â”€â”€ Connections â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    connect(m_fabBtn, &QPushButton::clicked,
            this, &StudentLearningHub::toggleAssistant);
    connect(closeBtn, &QPushButton::clicked,
            this, &StudentLearningHub::toggleAssistant);

    // Reposition after layout has settled (window not shown yet at ctor time)
    QTimer::singleShot(50,  this, [this]{ repositionFloatingAssistant(); });
    QTimer::singleShot(200, this, [this]{ repositionFloatingAssistant(); });
}

void StudentLearningHub::toggleAssistant()
{
    m_chatOpen = !m_chatOpen;
    m_chatPanel->setVisible(m_chatOpen);
    // Keep FAB icon; no text change needed (icon is always jalel.svg)
    m_fabBtn->setText("");
    if (m_chatOpen) {
        m_chatPanel->raise();
        m_fabBtn->raise();
    }
}

void StudentLearningHub::repositionFloatingAssistant()
{
    if (!m_fabBtn || !m_chatPanel) return;
    QWidget *p = centralWidget();
    if (!p) return;
    const int margin = 24;
    const int fabW = m_fabBtn->width();
    const int fabH = m_fabBtn->height();
    const int panW = m_chatPanel->width();
    const int panH = m_chatPanel->height();
    int pw = p->width();
    int ph = p->height();
    // FAB: bottom-right corner
    m_fabBtn->move(pw - fabW - margin, ph - fabH - margin);
    // Chat panel: just above the FAB
    m_chatPanel->move(pw - panW - margin, ph - panH - fabH - margin - 12);
}

void StudentLearningHub::resizeEvent(QResizeEvent *e)
{
    QMainWindow::resizeEvent(e);
    repositionFloatingAssistant();
}

void StudentLearningHub::showEvent(QShowEvent *e)
{
    QMainWindow::showEvent(e);
    QTimer::singleShot(0, this, [this]{ repositionFloatingAssistant(); });
}

// â”€â”€ applyTheme â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Central theme dispatcher — called whenever ThemeManager emits themeChanged.
// 1. Applies a global QApplication stylesheet so every QWidget in the app responds.
// 2. Re-styles the sidebar widgets specifically (hardcoded-color elements).
// 3. WinoStudentDashboard updates itself via its own ThemeManager connection.
void StudentLearningHub::applyTheme()
{
    if (!m_sidebar) return;
    ThemeManager *tm = ThemeManager::instance();
    bool isDark = (tm->currentTheme() == ThemeManager::Dark);

    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    // GLOBAL stylesheet — cascades to ALL QWidget instances in the entire app.
    // Per-widget stylesheets that are set explicitly take precedence over these.
    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    // Build the global stylesheet string now (captures current theme colors),
    // but apply it deferred so it runs AFTER all themeChanged slots complete.
    // Calling qApp->setStyleSheet() synchronously inside a slot triggers an
    // immediate style-change event on every widget, which can crash if other
    // slots in the same signal dispatch are concurrently creating/destroying widgets.
    QString globalSS = QString(
        "QMainWindow, QDialog { background-color: %1; }"
        "QWidget { background-color: %1; color: %2; }"
        "QFrame { background-color: %3; color: %2; }"
        "QLabel { color: %2; background: transparent; }"
        "QScrollArea { background-color: %1; border: none; }"
        "QScrollBar:vertical { background: %4; width: 8px; border-radius: 4px; }"
        "QScrollBar::handle:vertical { background: %5; border-radius: 4px; min-height: 30px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { border: none; background: none; }"
        "QPushButton { background-color: %3; color: %2; border: 1px solid %4; border-radius: 6px; padding: 6px 14px; }"
        "QPushButton:hover { background-color: %4; }"
        "QLineEdit, QTextEdit, QPlainTextEdit { background-color: %3; color: %2; border: 1px solid %4; border-radius: 6px; padding: 4px 8px; }"
        "QComboBox { background-color: %3; color: %2; border: 1px solid %4; border-radius: 6px; padding: 4px 8px; }"
        "QComboBox QAbstractItemView { background-color: %3; color: %2; selection-background-color: %5; }"
        "QTableWidget, QListWidget, QTreeWidget { background-color: %3; color: %2; gridline-color: %4; border: 1px solid %4; }"
        "QTableWidget::item:selected, QListWidget::item:selected { background-color: %5; color: white; }"
        "QHeaderView::section { background-color: %4; color: %2; border: none; padding: 6px; }"
        "QTabBar::tab { background-color: %3; color: %2; border: 1px solid %4; padding: 8px 16px; }"
        "QTabBar::tab:selected { background-color: %1; color: %6; border-bottom: 2px solid %6; }"
        "QGroupBox { color: %2; border: 1px solid %4; border-radius: 8px; margin-top: 12px; padding-top: 8px; }"
        "QGroupBox::title { color: %2; subcontrol-origin: margin; left: 12px; }"
        "QToolTip { background-color: %3; color: %2; border: 1px solid %4; border-radius: 4px; padding: 4px 8px; }"
    )
    .arg(tm->backgroundColor())
    .arg(tm->primaryTextColor())
    .arg(tm->cardColor())
    .arg(tm->borderColor())
    .arg(tm->accentColor())
    .arg(tm->accentColor());

    // Guard: only apply the global stylesheet if this hub is still alive.
    // QPointer becomes null when the hub is destroyed (deleteLater processed),
    // preventing a stale timer from re-applying the dark stylesheet after logout.
    QTimer::singleShot(0, qApp, [globalSS, self = QPointer<StudentLearningHub>(this)]() {
        if (self) qApp->setStyleSheet(globalSS);
    });

    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    // SIDEBAR — re-apply since it has explicit per-widget stylesheets set at
    // construction time (those override the global stylesheet above).
    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

    // 1. Sidebar frame background
    m_sidebar->setStyleSheet(QString(
        "QFrame { background-color: %1; border-right: 1px solid %2; }")
        .arg(HUB_SIDEBAR(), HUB_BORDER()));

    // 2. Main stack background
    m_stack->setStyleSheet(QString("background: %1;").arg(HUB_BG()));

    // 3. Step score label
    if (m_stepScoreLbl)
        m_stepScoreLbl->setStyleSheet(QString(
            "font-size: 11px; font-weight: bold; color: %1; background: transparent;")
            .arg(HUB_TEAL()));

    // 4. Sidebar labels — update all color values in their existing stylesheets
    for (QLabel *lbl : m_sidebar->findChildren<QLabel*>()) {
        QString cur = lbl->styleSheet();
        if (cur.contains("#14B8A6") || cur.contains("font-size: 32px") || cur.isEmpty())
            continue;
        lbl->setStyleSheet(cur.replace(
            QRegularExpression("color:\\s*#[0-9A-Fa-f]{3,6}"),
            QString("color: %1").arg(HUB_TEXT())));
    }

    // 5. Nav buttons (inactive + active state)
    if (m_activeMainBtn)
        setActiveBtn(m_activeMainBtn);

    // 6. Theory sub-nav buttons
    for (QPushButton *btn : m_theoryNavBtns) {
        btn->setStyleSheet(QString(
            "QPushButton { background: transparent; color: %1; "
            "border: none; border-left: 3px solid transparent; "
            "text-align: left; padding-left: 18px; "
            "font-size: 13px; font-weight: 500; border-radius: 0; }"
            "QPushButton:hover { background: rgba(128,128,128,0.08); color: %2; }")
            .arg(HUB_MUTED(), HUB_TEXT()));
    }
    if (m_activeTheoryIdx >= 0 && m_activeTheoryIdx < m_theoryNavBtns.size())
        setActiveTheoryBtn(m_activeTheoryIdx);

    // 7. Theme toggle button label + style
    if (m_themeBtn) {
        m_themeBtn->setText(isDark
            ? QString::fromUtf8("  \xe2\x98\x80\xef\xb8\x8f  Light mode")
            : QString::fromUtf8("  \xf0\x9f\x8c\x99  Dark mode"));
        m_themeBtn->setStyleSheet(QString(
            "QPushButton { background: transparent; color: %1; "
            "border: none; border-left: 3px solid transparent; "
            "text-align: left; padding-left: 16px; "
            "font-size: 13px; font-weight: 500; border-radius: 0; }"
            "QPushButton:hover { background: rgba(128,128,128,0.08); color: %2; }")
            .arg(HUB_MUTED(), HUB_TEXT()));
    }

    // 8. Parking page background (static BG color doesn't respond to global SS)
    if (m_parkingPage) {
        m_parkingPage->setStyleSheet(QString("background: %1;")
            .arg(isDark ? "#1a2332" : "#f0f2f5"));
    }

    // 9. Settings button style
    if (m_settingsBtn) {
        m_settingsBtn->setStyleSheet(QString(
            "QPushButton { background: transparent; color: %1; "
            "border: none; border-left: 3px solid transparent; "
            "text-align: left; padding-left: 16px; "
            "font-size: 13px; font-weight: 500; border-radius: 0; }"
            "QPushButton:hover { background: rgba(128,128,128,0.08); color: %2; }")
            .arg(HUB_MUTED(), HUB_TEXT()));
    }
}

// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
//  showSettings() — navigate to the settings page (index 5)
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
void StudentLearningHub::showSettings()
{
    // Collapse all accordions
    if (m_theorySubNav)   m_theorySubNav->setVisible(false);
    if (m_circuitSubNav)  m_circuitSubNav->setVisible(false);
    if (m_sessionsSubNav) m_sessionsSubNav->setVisible(false);
    if (m_parkingSubNav)  m_parkingSubNav->setVisible(false);

    // Reset nav button texts
    m_theoryBtn->setText(QString::fromUtf8("  \xf0\x9f\x93\x96  1 \xc2\xb7 Code / Theory  \xe2\x96\xb8"));
    m_circuitBtn->setText(QString::fromUtf8("  \xf0\x9f\x9a\xa6  2 \xc2\xb7 Circuit  \xe2\x96\xb8"));
    m_parkingBtn->setText(QString::fromUtf8("  \xf0\x9f\x85\xbf\xef\xb8\x8f  3 \xc2\xb7 Parking  \xe2\x96\xb8"));
    m_sessionsBtn->setText(QString::fromUtf8("  \xf0\x9f\x93\x85  4 \xc2\xb7 Sessions  \xe2\x96\xb8"));

    setActiveBtn(m_settingsBtn);
    m_stack->setCurrentIndex(5);
}

// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
//  createSettingsPage() — builds the Settings page widget (matches instructor style)
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
QWidget* StudentLearningHub::createSettingsPage()
{
    // â”€â”€ Fetch student data once â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    QString firstName, lastName, email, phone, cin;
    {
        QSqlQuery q(QSqlDatabase::database());
        q.prepare("SELECT first_name, last_name, email, phone, cin FROM STUDENTS WHERE id = :id");
        q.bindValue(":id", m_studentId);
        if (q.exec() && q.next()) {
            firstName = q.value(0).toString();
            lastName  = q.value(1).toString();
            email     = q.value(2).toString();
            phone     = q.value(3).toString();
            cin       = q.value(4).toString();
        }
    }
    QString fullName = (firstName + " " + lastName).trimmed();
    if (fullName.isEmpty()) fullName = "Student";

    // â”€â”€ Root page â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    QWidget *page = new QWidget(this);
    page->setObjectName("settingsPage");
    page->setStyleSheet("QWidget#settingsPage { background: #F1F5F9; }");

    QHBoxLayout *rootL = new QHBoxLayout(page);
    rootL->setContentsMargins(0, 0, 0, 0);
    rootL->setSpacing(0);

    // ── LEFT NAV PANEL ────────────────────────────────────────────────────────
    QWidget *navPanel = new QWidget();
    navPanel->setFixedWidth(220);
    navPanel->setStyleSheet("background:white; border-right:1px solid #E2E8F0;");
    QVBoxLayout *navL = new QVBoxLayout(navPanel);
    navL->setContentsMargins(0, 16, 0, 24);
    navL->setSpacing(2);

    QPushButton *backBtn = new QPushButton(QString::fromUtf8("  \xe2\x86\x90  Back"));
    backBtn->setFixedHeight(38);
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setStyleSheet(
        "QPushButton { background:transparent; color:#6B7280; border:none;"
        " text-align:left; padding-left:18px; font-size:13px; border-radius:0; }"
        "QPushButton:hover { background:#F1F5F9; color:#111827; }");
    connect(backBtn, &QPushButton::clicked, this, [this]() {
        setActiveBtn(m_theoryBtn);
        m_stack->setCurrentIndex(0);
    });
    navL->addWidget(backBtn);
    QFrame *backDiv = new QFrame();
    backDiv->setFrameShape(QFrame::HLine);
    backDiv->setStyleSheet("background:#E2E8F0; border:none;");
    backDiv->setFixedHeight(1);
    navL->addWidget(backDiv);
    navL->addSpacing(10);
    QLabel *settTitle = new QLabel(QString::fromUtf8("  \xe2\x9a\x99\xef\xb8\x8f  Settings"));
    settTitle->setStyleSheet("font-size:16px; font-weight:bold; color:#111827; padding:0 20px 14px 20px; background:transparent;");
    navL->addWidget(settTitle);

    struct NavItem { QString icon; QString label; };
    QList<NavItem> navItems = {
        { QString::fromUtf8("\xf0\x9f\x91\xa4"), "Profile"    },
        { QString::fromUtf8("\xf0\x9f\x8e\xa8"), "Appearance" },
        { QString::fromUtf8("\xf0\x9f\x94\x92"), "Privacy"    },
        { QString::fromUtf8("\xf0\x9f\x8f\xab"), "About"      },
    };
    QList<QPushButton*> navBtns;
    auto navActiveSS = [](bool on) -> QString {
        return on
            ? "QPushButton { background:#F0FDFA; color:#0F766E; border:none;"
              " border-left:3px solid #14B8A6; text-align:left;"
              " padding:12px 20px; font-size:13px; font-weight:700; border-radius:0; }"
            : "QPushButton { background:transparent; color:#374151; border:none;"
              " border-left:3px solid transparent; text-align:left;"
              " padding:12px 20px; font-size:13px; font-weight:500; border-radius:0; }"
              "QPushButton:hover { background:#F8FAFC; color:#111827; }";
    };
    for (auto &item : navItems) {
        QPushButton *btn = new QPushButton(QString("  %1   %2").arg(item.icon, item.label));
        btn->setFixedHeight(46); btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(navActiveSS(false));
        navL->addWidget(btn); navBtns.append(btn);
    }
    navL->addStretch();

    // ── RIGHT CONTENT STACK ───────────────────────────────────────────────────
    QStackedWidget *stack = new QStackedWidget();
    stack->setStyleSheet("background:#F1F5F9;");
    auto scrollPage = [&](QWidget *content) -> QWidget* {
        QScrollArea *sa = new QScrollArea();
        sa->setWidgetResizable(true); sa->setFrameShape(QFrame::NoFrame);
        sa->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        sa->setStyleSheet(
            "QScrollArea{border:none; background:#F1F5F9;}"
            "QScrollBar:vertical{width:6px; background:transparent;}"
            "QScrollBar::handle:vertical{background:#CBD5E1; border-radius:3px; min-height:30px;}"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical{height:0;}");
        content->setStyleSheet("background:#F1F5F9;"); sa->setWidget(content); return sa;
    };
    auto makeCard = [](QWidget *parent, const QString &icon, const QString &title, const QString &sub) -> QFrame* {
        QFrame *c = new QFrame(parent);
        c->setStyleSheet("QFrame{background:white;border-radius:14px;border:1px solid #E2E8F0;}");
        auto *sh = new QGraphicsDropShadowEffect(c);
        sh->setBlurRadius(16); sh->setColor(QColor(0,0,0,10)); sh->setOffset(0,3); c->setGraphicsEffect(sh);
        QVBoxLayout *l = new QVBoxLayout(c); l->setContentsMargins(24,20,24,20); l->setSpacing(14);
        QHBoxLayout *hdr = new QHBoxLayout(); hdr->setSpacing(12);
        QLabel *ic = new QLabel(icon,c); ic->setStyleSheet("font-size:22px;background:transparent;border:none;");
        hdr->addWidget(ic);
        QVBoxLayout *tc = new QVBoxLayout(); tc->setSpacing(2);
        QLabel *tl = new QLabel(title,c); tl->setStyleSheet("font-size:16px;font-weight:bold;color:#111827;background:transparent;border:none;");
        QLabel *sl = new QLabel(sub,c); sl->setStyleSheet("font-size:11px;color:#9CA3AF;background:transparent;border:none;");
        tc->addWidget(tl); tc->addWidget(sl); hdr->addLayout(tc,1); l->addLayout(hdr);
        QFrame *div = new QFrame(c); div->setFrameShape(QFrame::HLine);
        div->setStyleSheet("QFrame{background:#F1F5F9;border:none;}"); div->setFixedHeight(1); l->addWidget(div);
        return c;
    };
    auto makeField = [](QFrame *card, QVBoxLayout *l, const QString &label, const QString &val, bool ro=false){
        QLabel *lb = new QLabel(label,card);
        lb->setStyleSheet("font-size:11px;font-weight:600;color:#374151;background:transparent;border:none;");
        l->addWidget(lb);
        QLineEdit *ed = new QLineEdit(val,card); ed->setReadOnly(ro); ed->setFixedHeight(38);
        ed->setStyleSheet(
            "QLineEdit{background:#F9FAFB;border:1px solid #E5E7EB;border-radius:8px;padding:0 12px;font-size:12px;color:#111827;}"
            "QLineEdit:focus{border-color:#14B8A6;background:white;}"
            "QLineEdit:read-only{color:#9CA3AF;}");
        l->addWidget(ed);
    };

    // PAGE 0 : PROFILE
    {
        QWidget *pg = new QWidget(); QVBoxLayout *pgl = new QVBoxLayout(pg);
        pgl->setContentsMargins(0,0,0,0); pgl->setSpacing(0); pgl->setAlignment(Qt::AlignTop);
        QWidget *hero = new QWidget(pg);
        hero->setStyleSheet("background:white; border-bottom:1px solid #E2E8F0;");
        QHBoxLayout *heroL = new QHBoxLayout(hero);
        heroL->setContentsMargins(36,32,36,28); heroL->setSpacing(24);
        const int AV = 90;
        QLabel *av = new QLabel(hero); av->setFixedSize(AV,AV); av->setAlignment(Qt::AlignCenter);
        {
            bool loaded = false;
            if (!cin.isEmpty()) {
                QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/student_photos";
                for (const QString &ext : QStringList{"jpg","jpeg","png","bmp","webp"}) {
                    QString path = dir + "/" + cin + "." + ext;
                    if (QFile::exists(path)) {
                        QPixmap px(path);
                        if (!px.isNull()) {
                            QPixmap circle(AV,AV); circle.fill(Qt::transparent);
                            QPainter pp(&circle); pp.setRenderHint(QPainter::Antialiasing);
                            pp.setBrush(QBrush(px.scaled(AV,AV,Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation)));
                            pp.setPen(Qt::NoPen); pp.drawEllipse(0,0,AV,AV);
                            av->setPixmap(circle); av->setStyleSheet("border-radius:45px; border:3px solid #14B8A6;");
                            loaded = true; break;
                        }
                    }
                }
            }
            if (!loaded) {
                QPixmap pm(AV,AV); pm.fill(Qt::transparent);
                QPainter pp(&pm); pp.setRenderHint(QPainter::Antialiasing);
                pp.setBrush(QColor("#14B8A6")); pp.setPen(Qt::NoPen); pp.drawEllipse(0,0,AV,AV);
                pp.setPen(Qt::white);
                QFont f; f.setPointSize(30); f.setBold(true); pp.setFont(f);
                pp.drawText(QRect(0,0,AV,AV), Qt::AlignCenter, fullName.isEmpty() ? "?" : fullName.left(1).toUpper());
                av->setPixmap(pm); av->setStyleSheet("border-radius:45px; border:3px solid #14B8A6;");
            }
        }
        heroL->addWidget(av);
        QVBoxLayout *heroText = new QVBoxLayout(); heroText->setSpacing(6);
        QLabel *nameHero = new QLabel(fullName, hero);
        nameHero->setStyleSheet("font-size:26px; font-weight:bold; color:#111827; background:transparent; border:none;");
        QLabel *roleHero = new QLabel(QString::fromUtf8("\xf0\x9f\x8e\x93  Student"), hero);
        roleHero->setStyleSheet("font-size:14px; color:#14B8A6; background:transparent; border:none;");
        QLabel *emailHero = new QLabel(email.isEmpty() ? "" : "  " + email, hero);
        emailHero->setStyleSheet("font-size:13px; color:#6B7280; background:transparent; border:none;");
        heroText->addWidget(nameHero); heroText->addWidget(roleHero);
        if (!email.isEmpty()) heroText->addWidget(emailHero);
        heroL->addLayout(heroText, 1); pgl->addWidget(hero);
        QWidget *fieldsArea = new QWidget(pg); fieldsArea->setStyleSheet("background:#F1F5F9;");
        QVBoxLayout *fal = new QVBoxLayout(fieldsArea); fal->setContentsMargins(32,24,32,24); fal->setSpacing(20);
        QFrame *card = new QFrame(fieldsArea);
        card->setStyleSheet("QFrame{background:white;border-radius:14px;border:1px solid #E2E8F0;}");
        auto *sh2 = new QGraphicsDropShadowEffect(card);
        sh2->setBlurRadius(14); sh2->setColor(QColor(0,0,0,8)); sh2->setOffset(0,2); card->setGraphicsEffect(sh2);
        QVBoxLayout *cl = new QVBoxLayout(card); cl->setContentsMargins(24,20,24,20); cl->setSpacing(14);
        QLabel *fieldsTitle = new QLabel("Personal Information", card);
        fieldsTitle->setStyleSheet("font-size:14px;font-weight:bold;color:#374151;background:transparent;border:none;");
        cl->addWidget(fieldsTitle);
        QFrame *div0 = new QFrame(card); div0->setFrameShape(QFrame::HLine);
        div0->setStyleSheet("QFrame{background:#F1F5F9;border:none;}"); div0->setFixedHeight(1); cl->addWidget(div0);
        makeField(card,cl,"Full Name", fullName.isEmpty()?"—":fullName, true);
        makeField(card,cl,"Email",     email.isEmpty()?"—":email,       true);
        makeField(card,cl,"Phone",     phone.isEmpty()?"—":phone,       true);
        makeField(card,cl,"CIN",       cin.isEmpty()?"—":cin,           true);
        fal->addWidget(card);
        QFrame *langCard = new QFrame(fieldsArea);
        langCard->setStyleSheet("QFrame{background:white;border-radius:14px;border:1px solid #E2E8F0;}");
        auto *sh3 = new QGraphicsDropShadowEffect(langCard);
        sh3->setBlurRadius(14); sh3->setColor(QColor(0,0,0,8)); sh3->setOffset(0,2); langCard->setGraphicsEffect(sh3);
        QVBoxLayout *lcl = new QVBoxLayout(langCard); lcl->setContentsMargins(24,20,24,20); lcl->setSpacing(12);
        QLabel *langTitle = new QLabel("Language", langCard);
        langTitle->setStyleSheet("font-size:14px;font-weight:bold;color:#374151;background:transparent;border:none;");
        lcl->addWidget(langTitle);
        QFrame *div1 = new QFrame(langCard); div1->setFrameShape(QFrame::HLine);
        div1->setStyleSheet("QFrame{background:#F1F5F9;border:none;}"); div1->setFixedHeight(1); lcl->addWidget(div1);
        QHBoxLayout *lr = new QHBoxLayout(); lr->setSpacing(10);
        struct { const char *flag; const char *name; } langs[3]={
            {"\xf0\x9f\x87\xac\xf0\x9f\x87\xa7","English"},
            {"\xf0\x9f\x87\xab\xf0\x9f\x87\xb7","Fran\xc3\xa7""ais"},
            {"\xf0\x9f\x87\xb9\xf0\x9f\x87\xb3","\xd8\xb9\xd8\xb1\xd8\xa8\xd9\x8a"}
        };
        QSettings prefs("Wino","StudentApp");
        int savedLang = prefs.value(QString("lang_%1").arg(m_studentId),0).toInt();
        QList<QPushButton*> lbs;
        auto langSS=[](bool a)->QString{ return a
            ? "QPushButton{background:#14B8A6;color:white;border:2px solid #14B8A6;border-radius:10px;font-size:13px;font-weight:bold;}"
            : "QPushButton{background:#F9FAFB;color:#374151;border:2px solid #E5E7EB;border-radius:10px;font-size:13px;}"
              "QPushButton:hover{background:#F0FDFA;border-color:#14B8A6;}"; };
        for(int li=0;li<3;li++){
            QPushButton *lb=new QPushButton(QString("%1  %2").arg(QString::fromUtf8(langs[li].flag),
                                      QString::fromUtf8(langs[li].name)),langCard);
            lb->setFixedHeight(40); lb->setCheckable(true); lb->setChecked(li==savedLang);
            lb->setStyleSheet(langSS(li==savedLang)); lbs.append(lb); lr->addWidget(lb);
        }
        for(int li=0;li<3;li++){
            connect(lbs[li],&QPushButton::clicked,this,[lbs,li,sid=m_studentId,langSS](bool){
                QSettings ps("Wino","StudentApp"); ps.setValue(QString("lang_%1").arg(sid),li);
                for(int j=0;j<lbs.size();j++){lbs[j]->setChecked(j==li);lbs[j]->setStyleSheet(langSS(j==li));}
            });
        }
        lcl->addLayout(lr); fal->addWidget(langCard); fal->addStretch();
        pgl->addWidget(fieldsArea, 1); stack->addWidget(scrollPage(pg));
    }

    // PAGE 1 : APPEARANCE
    {
        QWidget *pg = new QWidget(); QVBoxLayout *pgl = new QVBoxLayout(pg);
        pgl->setContentsMargins(32,32,32,32); pgl->setSpacing(20); pgl->setAlignment(Qt::AlignTop);
        QFrame *card = makeCard(pg,QString::fromUtf8("\xf0\x9f\x8e\xa8"),"Appearance","Customize the interface");
        QVBoxLayout *cl = qobject_cast<QVBoxLayout*>(card->layout());
        QLabel *tl = new QLabel("Theme",card);
        tl->setStyleSheet("font-size:12px;font-weight:600;color:#374151;background:transparent;border:none;");
        cl->addWidget(tl);
        QHBoxLayout *tr = new QHBoxLayout(); tr->setSpacing(12);
        QPushButton *clairBtn=new QPushButton(QString::fromUtf8("\xe2\x98\x80\xef\xb8\x8f  Light"),card);
        QPushButton *sombreBtn=new QPushButton(QString::fromUtf8("\xf0\x9f\x8c\x99  Dark"),card);
        for(auto *b:{clairBtn,sombreBtn}){ b->setFixedHeight(48); b->setCheckable(true); }
        auto thSS=[](bool light,bool active)->QString{
            if(light) return active
                ? "QPushButton{background:white;color:#111827;border:2px solid #14B8A6;border-radius:12px;font-size:14px;font-weight:bold;}"
                : "QPushButton{background:#F9FAFB;color:#374151;border:2px solid #E5E7EB;border-radius:12px;font-size:14px;}"
                  "QPushButton:hover{border-color:#14B8A6;}";
            else return active
                ? "QPushButton{background:#1F2937;color:white;border:2px solid #14B8A6;border-radius:12px;font-size:14px;font-weight:bold;}"
                : "QPushButton{background:#F9FAFB;color:#374151;border:2px solid #E5E7EB;border-radius:12px;font-size:14px;}"
                  "QPushButton:hover{border-color:#14B8A6;}";
        };
        bool isDark = ThemeManager::instance()->currentTheme()==ThemeManager::Dark;
        clairBtn->setChecked(!isDark); clairBtn->setStyleSheet(thSS(true,!isDark));
        sombreBtn->setChecked(isDark);  sombreBtn->setStyleSheet(thSS(false,isDark));
        connect(clairBtn,&QPushButton::clicked,this,[=](bool){ ThemeManager::instance()->setTheme(ThemeManager::Light); });
        connect(sombreBtn,&QPushButton::clicked,this,[=](bool){ ThemeManager::instance()->setTheme(ThemeManager::Dark); });
        connect(ThemeManager::instance(),&ThemeManager::themeChanged,this,
            [=](ThemeManager::Theme t){
                bool d=t==ThemeManager::Dark;
                clairBtn->setChecked(!d); clairBtn->setStyleSheet(thSS(true,!d));
                sombreBtn->setChecked(d);  sombreBtn->setStyleSheet(thSS(false,d));
            });
        tr->addWidget(clairBtn,1); tr->addWidget(sombreBtn,1); cl->addLayout(tr);
        QLabel *vl=new QLabel(QString::fromUtf8("Sound Volume"),card);
        vl->setStyleSheet("font-size:12px;font-weight:600;color:#374151;background:transparent;border:none;");
        cl->addWidget(vl);
        QHBoxLayout *vr=new QHBoxLayout(); vr->setSpacing(10);
        QLabel *mic=new QLabel(QString::fromUtf8("\xf0\x9f\x94\x87"),card);
        mic->setStyleSheet("font-size:16px;background:transparent;border:none;");
        QSlider *vs=new QSlider(Qt::Horizontal,card); vs->setRange(0,100); vs->setValue(70);
        vs->setStyleSheet("QSlider::groove:horizontal{height:5px;background:#E2E8F0;border-radius:3px;}"
            "QSlider::handle:horizontal{width:18px;height:18px;background:#14B8A6;border-radius:9px;margin:-7px 0;}"
            "QSlider::sub-page:horizontal{background:#14B8A6;border-radius:3px;}");
        QLabel *vp=new QLabel("70%",card); vp->setFixedWidth(36);
        vp->setStyleSheet("font-size:12px;color:#6B7280;background:transparent;border:none;");
        connect(vs,&QSlider::valueChanged,this,[vp](int v){vp->setText(QString("%1%").arg(v));});
        QLabel *mac=new QLabel(QString::fromUtf8("\xf0\x9f\x94\x8a"),card);
        mac->setStyleSheet("font-size:16px;background:transparent;border:none;");
        vr->addWidget(mic); vr->addWidget(vs,1); vr->addWidget(vp); vr->addWidget(mac);
        cl->addLayout(vr); pgl->addWidget(card); pgl->addStretch(); stack->addWidget(scrollPage(pg));
    }

    // PAGE 2 : PRIVACY
    {
        QWidget *pg = new QWidget(); QVBoxLayout *pgl = new QVBoxLayout(pg);
        pgl->setContentsMargins(32,32,32,32); pgl->setSpacing(20); pgl->setAlignment(Qt::AlignTop);
        QFrame *card = makeCard(pg,QString::fromUtf8("\xf0\x9f\x94\x92"),"Privacy","Privacy and data settings");
        QVBoxLayout *cl = qobject_cast<QVBoxLayout*>(card->layout());
        struct { const char* icon; const char* text; bool danger; } items[]={
            {"\xf0\x9f\x94\xa7","Change password",false},
            {"\xf0\x9f\x92\xbe","Export my data",false},
            {"\xf0\x9f\x94\x84","Reset settings",false},
            {"\xf0\x9f\x97\x91","Delete my account",true}
        };
        for(auto &it:items){
            QPushButton *btn=new QPushButton(
                QString("  %1   %2").arg(QString::fromUtf8(it.icon),it.text),card);
            btn->setFixedHeight(44); btn->setCursor(Qt::PointingHandCursor);
            btn->setStyleSheet(it.danger
                ? "QPushButton{background:#FEF2F2;color:#EF4444;border:1px solid #FEE2E2;"
                  "border-radius:10px;text-align:left;padding-left:16px;font-size:13px;}"
                  "QPushButton:hover{background:#FEE2E2;}"
                : "QPushButton{background:#F9FAFB;color:#374151;border:1px solid #E5E7EB;"
                  "border-radius:10px;text-align:left;padding-left:16px;font-size:13px;}"
                  "QPushButton:hover{background:#F3F4F6;}");
            cl->addWidget(btn);
        }
        pgl->addWidget(card); pgl->addStretch(); stack->addWidget(scrollPage(pg));
    }

    // PAGE 3 : ABOUT
    {
        QWidget *pg = new QWidget(); QVBoxLayout *pgl = new QVBoxLayout(pg);
        pgl->setContentsMargins(32,32,32,32); pgl->setSpacing(20); pgl->setAlignment(Qt::AlignTop);
        QFrame *card = makeCard(pg,QString::fromUtf8("\xf0\x9f\x8f\xab"),"About Wino","Application information");
        QVBoxLayout *cl = qobject_cast<QVBoxLayout*>(card->layout());
        auto row=[&](const QString &k,const QString &v){
            QHBoxLayout *r=new QHBoxLayout();
            QLabel *kl=new QLabel(k,card); kl->setStyleSheet("font-size:13px;color:#6B7280;background:transparent;border:none;");
            QLabel *vl=new QLabel(v,card); vl->setStyleSheet("font-size:13px;font-weight:600;color:#111827;background:transparent;border:none;");
            vl->setAlignment(Qt::AlignRight); r->addWidget(kl,1); r->addWidget(vl); cl->addLayout(r);
            QFrame *d=new QFrame(card); d->setFrameShape(QFrame::HLine);
            d->setStyleSheet("QFrame{background:#F1F5F9;border:none;}"); d->setFixedHeight(1); cl->addWidget(d);
        };
        row("Application","Wino \xe2\x80\x94 Smart Driving School");
        row("Version","5.0.0  (Build 2026.02)");
        row("Framework","Qt 6 / C++17");
        row(QString::fromUtf8("Developer"),QString::fromUtf8("\xc3\x89quipe Wino"));
        row("Licence","Owner");
        QLabel *footer=new QLabel(QString::fromUtf8("Made with \xe2\x9d\xa4 in Tunisia \xf0\x9f\x87\xb9\xf0\x9f\x87\xb3\n"
                              "\xc2\xa9 2026 Wino. All rights reserved."),card);
        footer->setAlignment(Qt::AlignCenter);
        footer->setStyleSheet("font-size:11px;color:#9CA3AF;background:transparent;border:none;margin-top:8px;");
        cl->addWidget(footer); pgl->addWidget(card); pgl->addStretch(); stack->addWidget(scrollPage(pg));
    }

    // Wire nav buttons
    auto activate = [=](int idx) {
        for (int i = 0; i < navBtns.size(); i++)
            navBtns[i]->setStyleSheet(navActiveSS(i == idx));
        stack->setCurrentIndex(idx);
    };
    for (int i = 0; i < navBtns.size(); i++)
        connect(navBtns[i], &QPushButton::clicked, this, [=]{ activate(i); });
    activate(0);

    rootL->addWidget(navPanel);
    rootL->addWidget(stack, 1);
    return page;
}
