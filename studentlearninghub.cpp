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

// ── Palette (theme-aware) ─────────────────────────────────────────────────────
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

// ── loadStudentStep ───────────────────────────────────────────────────────────
// Reads progression from WINO_PROGRESS, bootstrapping from course_level when
// the row doesn't yet exist OR when the stored step is behind the enrolled level.
//
// course_level (STUDENTS table) → initial WINO_PROGRESS state:
//   "Theory"    → step 1, Code=IN_PROGRESS, Circuit=LOCKED,     Parking=LOCKED
//   "Practical" → step 2, Code=COMPLETED,   Circuit=IN_PROGRESS, Parking=LOCKED
//   "Parking"   → step 3, Code=COMPLETED,   Circuit=COMPLETED,   Parking=IN_PROGRESS
void StudentLearningHub::loadStudentStep()
{
    WinoBootstrap::bootstrap();

    // ── 1. Read the student's registered course level ─────────────────────────
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

    // ── 2. Check for an existing WINO_PROGRESS row ───────────────────────────
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

        // ── 3. Correct the row if the stored step is behind course level ──────
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
        // ── 4. First ever login: insert the correct initial row ───────────────
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

// ── syncScoresToProgress ──────────────────────────────────────────────────────
// Reads live scores from TACHE (USER_PROGRESS) and Circuit modules,
// writes them into WINO_PROGRESS so the Sessions dashboard shows correct values.
void StudentLearningHub::syncScoresToProgress()
{
    // ── Code score: read from TACHE USER_PROGRESS.progress_percent ───────────
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

    // ── Circuit score: % of analysed sessions (max 25 = 100%) ───────────────
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

// ── updateScoreLabel ──────────────────────────────────────────────────────────
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

// ── Constructor ───────────────────────────────────────────────────────────────
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

    // ── Left sidebar ─────────────────────────────────────────────────────────
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

    // ── Brand header ──────────────────────────────────────────────────────────
    QLabel *logo = new QLabel(QString::fromUtf8("🚗"));
    logo->setAlignment(Qt::AlignCenter);
    logo->setStyleSheet("font-size: 32px; padding: 18px 0 4px 0; background: transparent;");
    QLabel *brand = new QLabel("SmartDrive");
    brand->setAlignment(Qt::AlignCenter);
    brand->setStyleSheet(QString(
        "font-size: 16px; font-weight: bold; color: %1; "
        "background: transparent; padding-bottom: 2px;").arg(HUB_TEAL()));
    QLabel *platformSub = new QLabel("Learning Platform");
    platformSub->setAlignment(Qt::AlignCenter);
    platformSub->setStyleSheet(QString(
        "font-size: 11px; color: %1; background: transparent; "
        "padding-bottom: 10px;").arg(HUB_MUTED()));

    sideLayout->addWidget(logo);
    sideLayout->addWidget(brand);
    sideLayout->addWidget(platformSub);
    addDivider();

    // ── MON PARCOURS ─────────────────────────────────────────────────────────
    sideLayout->addSpacing(10);

    QLabel *parcoursLbl = new QLabel(QString::fromUtf8("  🎯  MON PARCOURS"));
    parcoursLbl->setStyleSheet(QString(
        "font-size: 10px; font-weight: bold; color: %1; letter-spacing: 1px; "
        "background: transparent; padding: 0 16px 8px 16px;").arg(HUB_MUTED()));
    sideLayout->addWidget(parcoursLbl);

    QWidget *stepTracker = new QWidget(sidebar);
    stepTracker->setStyleSheet("background: transparent;");
    QVBoxLayout *trackerLay = new QVBoxLayout(stepTracker);
    trackerLay->setContentsMargins(12, 0, 12, 8);
    trackerLay->setSpacing(5);

    // ── Step circles ─────────────────────────────────────────────────────────
    QHBoxLayout *circlesRow = new QHBoxLayout();
    circlesRow->setSpacing(0);
    circlesRow->setContentsMargins(0, 0, 0, 0);

    struct StepDef { QString name; QString icon; };
    const QList<StepDef> stepDefs = {
        { "Code",    QString::fromUtf8("📖") },
        { "Circuit", QString::fromUtf8("🚦") },
        { "Parking", QString::fromUtf8("🅿️") },
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
            circle->setText(QString::fromUtf8("✓"));
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
            circle->setText(QString::fromUtf8("🔒"));
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
    QLabel *pctLbl = new QLabel(QString("Progression : %1%").arg(pct));
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

    // ── NAVIGATION section ────────────────────────────────────────────────────
    QLabel *navLabel = new QLabel("  NAVIGATION");
    navLabel->setStyleSheet(QString(
        "font-size: 10px; font-weight: bold; color: %1; letter-spacing: 2px; "
        "background: transparent; padding: 0 20px 8px 20px;").arg(HUB_MUTED()));
    sideLayout->addWidget(navLabel);

    const bool circuitLocked = false;  // unlocked for all students

    m_theoryBtn   = makeStepNavBtn(sidebar, 1, QString::fromUtf8("📖"), "Code / Théorie", false);
    m_circuitBtn  = makeStepNavBtn(sidebar, 2, QString::fromUtf8("🚦"), "Circuit",        circuitLocked);
    m_parkingBtn   = makeStepNavBtn(sidebar, 3, QString::fromUtf8("🅿️"), "Parking",        false);
    m_sessionsBtn = makeStepNavBtn(sidebar, 4, QString::fromUtf8("📅"), "Sessions",       false);  // always accessible
    m_assistantBtn = makeStepNavBtn(sidebar, 5, QString::fromUtf8("🤖"), "Assistant Wino", false);

    sideLayout->addWidget(m_theoryBtn);
    sideLayout->addWidget(m_circuitBtn);
    sideLayout->addWidget(m_parkingBtn);
    sideLayout->addWidget(m_sessionsBtn);
    sideLayout->addWidget(m_assistantBtn);

    // ── THEORY sub-navigation (shown when Theory is active) ───────────────────
    m_theorySubNav = new QWidget(sidebar);
    m_theorySubNav->setStyleSheet("background: transparent;");
    QVBoxLayout *subLayout = new QVBoxLayout(m_theorySubNav);
    subLayout->setContentsMargins(0, 4, 0, 0);
    subLayout->setSpacing(0);

    QLabel *learnLabel = new QLabel("  LEARNING");
    learnLabel->setStyleSheet(QString(
        "font-size: 10px; font-weight: bold; color: %1; letter-spacing: 2px; "
        "background: transparent; padding: 8px 20px 6px 20px;").arg(HUB_MUTED()));
    subLayout->addWidget(learnLabel);

    struct SubItem { QString icon; QString label; int idx; };
    const QList<SubItem> subItems = {
        { QString::fromUtf8("📋"), "Dashboard",     0 },
        { QString::fromUtf8("📚"), "Courses",        1 },
        { QString::fromUtf8("📝"), "Quizzes",        2 },
        { QString::fromUtf8("🥽"), "VR Simulation",  3 },
        { QString::fromUtf8("🏆"), "Badges",         4 },
        { QString::fromUtf8("📷"), "Sign Scanner",   5 },
    };

    for (const auto &item : subItems) {
        QPushButton *btn = new QPushButton(m_theorySubNav);
        btn->setText(QString("    %1  %2").arg(item.icon, item.label));
        btn->setFixedHeight(40);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString(
            "QPushButton { background: transparent; color: %1; "
            "border: none; border-left: 3px solid transparent; "
            "text-align: left; padding-left: 18px; "
            "font-size: 13px; font-weight: 500; border-radius: 0; }"
            "QPushButton:hover { background: rgba(128,128,128,0.08); color: %2; }")
            .arg(HUB_MUTED(), HUB_TEXT()));
        subLayout->addWidget(btn);
        m_theoryNavBtns.append(btn);

        int idx = item.idx;
        connect(btn, &QPushButton::clicked, this, [this, idx]() {
            setActiveTheoryBtn(idx);
            if (m_theoryPage) m_theoryPage->navigateToIndex(idx);
        });
    }

    // Reset progress button
    QFrame *resetDiv = new QFrame(m_theorySubNav);
    resetDiv->setFixedHeight(1);
    resetDiv->setStyleSheet(QString("background: %1; margin: 6px 16px;").arg(HUB_BORDER()));
    subLayout->addWidget(resetDiv);

    QPushButton *resetBtn = new QPushButton(m_theorySubNav);
    resetBtn->setText(QString::fromUtf8("    🔄  Reset Progress"));
    resetBtn->setFixedHeight(40);
    resetBtn->setCursor(Qt::PointingHandCursor);
    resetBtn->setStyleSheet(
        "QPushButton { background: transparent; color: #EF4444; "
        "border: none; border-left: 3px solid transparent; "
        "text-align: left; padding-left: 18px; "
        "font-size: 13px; font-weight: 500; border-radius: 0; }"
        "QPushButton:hover { background: rgba(239,68,68,0.10); }");
    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        if (m_theoryPage) m_theoryPage->resetProgress();
    });
    subLayout->addWidget(resetBtn);

    sideLayout->addWidget(m_theorySubNav);
    sideLayout->addStretch();

    // ── Theme toggle (bottom of sidebar) ─────────────────────────────────────
    addDivider();
    {
        bool isDark = ThemeManager::instance()->currentTheme() == ThemeManager::Dark;

        m_themeBtn = new QPushButton(sidebar);
        m_themeBtn->setFixedHeight(44);
        m_themeBtn->setCursor(Qt::PointingHandCursor);
        m_themeBtn->setToolTip("Basculer entre mode clair / sombre");
        m_themeBtn->setText(isDark
            ? QString::fromUtf8("  ☀️  Mode clair")
            : QString::fromUtf8("  🌙  Mode sombre"));
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
        sideLayout->addSpacing(8);
    }

    // ── Main content stack ────────────────────────────────────────────────────
    m_stack = new QStackedWidget(root);
    m_stack->setStyleSheet(QString("background: %1;").arg(HUB_BG()));
    m_stack->addWidget(new QWidget());  // index 0 = theory    placeholder
    m_stack->addWidget(new QWidget());  // index 1 = circuit   placeholder
    m_stack->addWidget(new QWidget());  // index 2 = parking   placeholder
    m_stack->addWidget(new QWidget());  // index 3 = sessions  placeholder
    m_stack->addWidget(new QWidget());  // index 4 = assistant placeholder

    rootLayout->addWidget(sidebar);
    rootLayout->addWidget(m_stack, 1);

    connect(m_theoryBtn,    &QPushButton::clicked, this, &StudentLearningHub::showTheory);
    connect(m_circuitBtn,   &QPushButton::clicked, this, &StudentLearningHub::showCircuit);
    connect(m_sessionsBtn,  &QPushButton::clicked, this, &StudentLearningHub::showSessions);
    connect(m_parkingBtn,   &QPushButton::clicked, this, &StudentLearningHub::showParking);
    connect(m_assistantBtn, &QPushButton::clicked, this, &StudentLearningHub::showAssistant);

    // ── Theme: redraw sidebar whenever the user toggles light/dark ───────────
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &StudentLearningHub::applyTheme);

    // ── Apply saved theme immediately on startup ─────────────────────────────
    // Ensures the app opens in the correct light/dark state from QSettings.
    applyTheme();

    // ── Navigate directly to the student's current step on login ─────────────
    // m_studentStep: 0=Code, 1=Circuit, 2=Sessions/Parking
    if (m_studentStep >= 2)
        showParking();     // Parking course level → land on Parking
    else if (m_studentStep == 1)
        showCircuit();     // Practical course level → land on Circuit
    else
        showTheory();      // Theory (default) → land on Code
}

// ── makeStepNavBtn ────────────────────────────────────────────────────────────
QPushButton* StudentLearningHub::makeStepNavBtn(QWidget *parent, int stepNum,
                                                const QString &icon,
                                                const QString &label, bool locked)
{
    QPushButton *btn = new QPushButton(parent);
    btn->setFixedHeight(48);

    if (locked) {
        btn->setText(QString("  %1  %2 · %3")
            .arg(QString::fromUtf8("🔒")).arg(stepNum).arg(label));
        btn->setEnabled(false);
        btn->setCursor(Qt::ForbiddenCursor);
        btn->setToolTip("Validez l'étape précédente pour débloquer cette section.");
        btn->setStyleSheet(QString(
            "QPushButton { background: transparent; color: %1; "
            "border: none; border-left: 3px solid transparent; "
            "text-align: left; padding-left: 16px; "
            "font-size: 13px; font-weight: 400; border-radius: 0; }")
            .arg(HUB_LOCKED_TEXT()));
    } else {
        btn->setText(QString("  %1  %2 · %3").arg(icon).arg(stepNum).arg(label));
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

// ── setActiveBtn ──────────────────────────────────────────────────────────────
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
}

// ── setActiveTheoryBtn ────────────────────────────────────────────────────────
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

// ── showTheory ────────────────────────────────────────────────────────────────
void StudentLearningHub::showTheory()
{
    setActiveBtn(m_theoryBtn);
    m_theorySubNav->setVisible(true);
    updateScoreLabel(0);

    if (!m_theoryPage) {
        m_theoryPage = new SmartDriveWindow(m_studentId, this);
        m_stack->removeWidget(m_stack->widget(0));
        m_stack->insertWidget(0, m_theoryPage);
    }
    m_stack->setCurrentIndex(0);
    setActiveTheoryBtn(m_activeTheoryIdx);
}

// ── showCircuit ───────────────────────────────────────────────────────────────
void StudentLearningHub::showCircuit()
{
    setActiveBtn(m_circuitBtn);
    m_theorySubNav->setVisible(false);
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

// ── showSessions ──────────────────────────────────────────────────────────────
void StudentLearningHub::showSessions()
{
    // Sync live scores from TACHE + Circuit → WINO_PROGRESS before displaying
    syncScoresToProgress();

    setActiveBtn(m_sessionsBtn);
    m_theorySubNav->setVisible(false);
    updateScoreLabel(m_studentStep);  // show score of the student's current step

    // Always recreate to show up-to-date scores after sync.
    // IMPORTANT: do NOT call removeWidget(widget(3)) after removing m_sessionsPage —
    // once m_sessionsPage is removed, index 3 belongs to the next widget (assistant)
    // and removing it again would silently delete the assistant from the stack.
    WinoBootstrap::bootstrap();
    qApp->setProperty("currentUserId", m_studentId);

    WinoStudentDashboard *newPage = new WinoStudentDashboard(this);

    if (m_sessionsPage) {
        // Replace the existing sessions widget in-place without touching other indices
        int idx = m_stack->indexOf(m_sessionsPage);
        m_stack->removeWidget(m_sessionsPage);
        delete m_sessionsPage;
        m_sessionsPage = nullptr;
        m_stack->insertWidget(idx, newPage);
        m_stack->setCurrentIndex(idx);
    } else {
        // First time: replace the placeholder at index 3
        m_stack->removeWidget(m_stack->widget(3));
        m_stack->insertWidget(3, newPage);
        m_stack->setCurrentIndex(3);
    }
    m_sessionsPage = newPage;
}

// ── showParking ───────────────────────────────────────────────────────────────
void StudentLearningHub::showParking()
{
    setActiveBtn(m_parkingBtn);
    m_theorySubNav->setVisible(false);
    updateScoreLabel(m_studentStep);

    if (!m_parkingPage) {
        // Reuse the already-open default DB connection
        ParkingDBManager::instance().initialize("", "", "");

        m_parkingPage = new ParkingWidget(
            QString("Student"),   // userName
            QString("Eleve"),     // userRole
            m_studentId,          // userId
            this
        );
        m_stack->removeWidget(m_stack->widget(2));
        m_stack->insertWidget(2, m_parkingPage);
    }
    m_stack->setCurrentIndex(2);
}

// ── showAssistant ─────────────────────────────────────────────────────────────
void StudentLearningHub::showAssistant()
{
    setActiveBtn(m_assistantBtn);
    m_theorySubNav->setVisible(false);
    updateScoreLabel(m_studentStep);

    if (!m_assistantPage) {
        m_assistantPage = new WinoAssistantWidget(
            QString("Student"),
            QString("Eleve"),
            this
        );
        m_stack->removeWidget(m_stack->widget(4));
        m_stack->insertWidget(4, m_assistantPage);
    }
    m_stack->setCurrentIndex(4);
}

// ── applyTheme ────────────────────────────────────────────────────────────────
// Central theme dispatcher — called whenever ThemeManager emits themeChanged.
// 1. Applies a global QApplication stylesheet so every QWidget in the app responds.
// 2. Re-styles the sidebar widgets specifically (hardcoded-color elements).
// 3. WinoStudentDashboard updates itself via its own ThemeManager connection.
void StudentLearningHub::applyTheme()
{
    if (!m_sidebar) return;
    ThemeManager *tm = ThemeManager::instance();
    bool isDark = (tm->currentTheme() == ThemeManager::Dark);

    // ═══════════════════════════════════════════════════════════════════════════
    // GLOBAL stylesheet — cascades to ALL QWidget instances in the entire app.
    // Per-widget stylesheets that are set explicitly take precedence over these.
    // ═══════════════════════════════════════════════════════════════════════════
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

    QTimer::singleShot(0, qApp, [globalSS]() {
        qApp->setStyleSheet(globalSS);
    });

    // ═══════════════════════════════════════════════════════════════════════════
    // SIDEBAR — re-apply since it has explicit per-widget stylesheets set at
    // construction time (those override the global stylesheet above).
    // ═══════════════════════════════════════════════════════════════════════════

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
            ? QString::fromUtf8("  ☀️  Mode clair")
            : QString::fromUtf8("  🌙  Mode sombre"));
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
}
