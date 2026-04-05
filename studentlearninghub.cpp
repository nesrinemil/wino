#include "studentlearninghub.h"
#include "C:/Users/hboug/Downloads/final/SmartDrivingSchool/SmartDrivingSchool/studentportal.h"
#include "C:/Users/hboug/Downloads/final/SmartDrivingSchool/SmartDrivingSchool/circuitdb.h"
#include "wino/wino_studentdashboard.h"
#include "wino/wino_bootstrap.h"
#include <QHBoxLayout>
#include <QApplication>
#include <QScreen>
#include <QSqlQuery>
#include <QMessageBox>

// ── Palette ──────────────────────────────────────────────────────────────────
#define HUB_BG          "#0F172A"
#define HUB_SIDEBAR     "#111827"
#define HUB_TEAL        "#14B8A6"
#define HUB_BLUE        "#38BDF8"
#define HUB_TEXT        "#E2E8F0"
#define HUB_MUTED       "#64748B"
#define HUB_ACTIVE      "rgba(20,184,166,0.15)"
#define HUB_BORDER      "#1E293B"
#define HUB_SUB_ACTIVE  "rgba(20,184,166,0.10)"
#define HUB_LOCKED_TEXT "#475569"

// ── loadStudentStep ───────────────────────────────────────────────────────────
// Reads progression from WINO_PROGRESS. Creates a default row if none exists.
// current_step in DB: 1=Code, 2=Circuit, 3=Sessions  →  m_studentStep: 0,1,2
void StudentLearningHub::loadStudentStep()
{
    WinoBootstrap::bootstrap();

    QSqlQuery q;
    q.prepare(
        "SELECT current_step, "
        "       NVL(code_score,0), NVL(circuit_score,0), NVL(parking_score,0) "
        "FROM WINO_PROGRESS WHERE user_id = ?");
    q.addBindValue(m_studentId);

    if (q.exec() && q.next()) {
        int wStep       = q.value(0).toInt();
        m_studentStep   = qBound(0, wStep - 1, 2);
        m_codeScore     = q.value(1).toDouble();
        m_circuitScore  = q.value(2).toDouble();
        m_parkingScore  = q.value(3).toDouble();
    } else {
        // First login: create default progression row
        QSqlQuery ins;
        ins.prepare(
            "INSERT INTO WINO_PROGRESS "
            "(user_id, current_step, code_status, circuit_status, parking_status,"
            " code_score, circuit_score, parking_score, total_score) "
            "VALUES (?, 1, 'IN_PROGRESS', 'LOCKED', 'LOCKED', 0, 0, 0, 0)");
        ins.addBindValue(m_studentId);
        ins.exec();
        m_studentStep  = 0;
        m_codeScore    = 0;
        m_circuitScore = 0;
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

    // ── Circuit score: % sessions completed (max 25 sessions = 100%) ─────────
    {
        QSqlQuery q;
        q.prepare("SELECT COUNT(*) FROM DRIVING_SESSIONS WHERE student_id = ?");
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
                       (activeStep == 1) ? "Circuit" : "Sessions";
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
    QFrame *sidebar = new QFrame(root);
    sidebar->setFixedWidth(230);
    sidebar->setStyleSheet(QString(
        "QFrame { background-color: %1; border-right: 1px solid %2; }")
        .arg(HUB_SIDEBAR, HUB_BORDER));

    QVBoxLayout *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    sideLayout->setSpacing(0);

    // Helper: horizontal divider
    auto addDivider = [&]() {
        QFrame *d = new QFrame(sidebar);
        d->setFrameShape(QFrame::HLine);
        d->setStyleSheet(QString("background: %1; margin: 0;").arg(HUB_BORDER));
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
        "background: transparent; padding-bottom: 2px;").arg(HUB_TEAL));
    QLabel *platformSub = new QLabel("Learning Platform");
    platformSub->setAlignment(Qt::AlignCenter);
    platformSub->setStyleSheet(QString(
        "font-size: 11px; color: %1; background: transparent; "
        "padding-bottom: 10px;").arg(HUB_MUTED));

    sideLayout->addWidget(logo);
    sideLayout->addWidget(brand);
    sideLayout->addWidget(platformSub);
    addDivider();

    // ── MON PARCOURS ─────────────────────────────────────────────────────────
    sideLayout->addSpacing(10);

    QLabel *parcoursLbl = new QLabel(QString::fromUtf8("  🎯  MON PARCOURS"));
    parcoursLbl->setStyleSheet(QString(
        "font-size: 10px; font-weight: bold; color: %1; letter-spacing: 1px; "
        "background: transparent; padding: 0 16px 8px 16px;").arg(HUB_MUTED));
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
        { "Sessions",QString::fromUtf8("📅") },
    };

    for (int i = 0; i < 3; ++i) {
        QVBoxLayout *col = new QVBoxLayout();
        col->setSpacing(3);
        col->setAlignment(Qt::AlignHCenter);

        QLabel *circle = new QLabel();
        circle->setFixedSize(32, 32);
        circle->setAlignment(Qt::AlignCenter);

        // Sessions (i==2) is always accessible — never show lock icon
        const bool isAlwaysOpen = (i == 2);

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
                "font-size: 14px; border: 2px solid %1;").arg(HUB_BLUE));
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
            .arg((m_studentStep >= i || isAlwaysOpen) ? HUB_TEXT : HUB_MUTED));

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
        "font-size: 10px; color: %1; background: transparent;").arg(HUB_MUTED));
    trackerLay->addWidget(pctLbl);

    // Step score label (updates when navigating tabs)
    m_stepScoreLbl = new QLabel();
    m_stepScoreLbl->setAlignment(Qt::AlignCenter);
    m_stepScoreLbl->setStyleSheet(QString(
        "font-size: 11px; font-weight: bold; color: %1; "
        "background: transparent;").arg(HUB_TEAL));
    trackerLay->addWidget(m_stepScoreLbl);
    updateScoreLabel(0);

    sideLayout->addWidget(stepTracker);
    addDivider();
    sideLayout->addSpacing(8);

    // ── NAVIGATION section ────────────────────────────────────────────────────
    QLabel *navLabel = new QLabel("  NAVIGATION");
    navLabel->setStyleSheet(QString(
        "font-size: 10px; font-weight: bold; color: %1; letter-spacing: 2px; "
        "background: transparent; padding: 0 20px 8px 20px;").arg(HUB_MUTED));
    sideLayout->addWidget(navLabel);

    const bool circuitLocked = (m_studentStep < 1);

    m_theoryBtn   = makeStepNavBtn(sidebar, 1, QString::fromUtf8("📖"), "Code / Théorie", false);
    m_circuitBtn  = makeStepNavBtn(sidebar, 2, QString::fromUtf8("🚦"), "Circuit",        circuitLocked);
    m_sessionsBtn = makeStepNavBtn(sidebar, 3, QString::fromUtf8("📅"), "Sessions",       false);  // always accessible
    m_parkingBtn   = makeStepNavBtn(sidebar, 4, QString::fromUtf8("🅿️"), "Parking",        false);
    m_assistantBtn = makeStepNavBtn(sidebar, 5, QString::fromUtf8("🤖"), "Assistant Wino", false);

    sideLayout->addWidget(m_theoryBtn);
    sideLayout->addWidget(m_circuitBtn);
    sideLayout->addWidget(m_sessionsBtn);
    sideLayout->addWidget(m_parkingBtn);
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
        "background: transparent; padding: 8px 20px 6px 20px;").arg(HUB_MUTED));
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
            "QPushButton:hover { background: rgba(255,255,255,0.05); color: %2; }")
            .arg(HUB_MUTED, HUB_TEXT));
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
    resetDiv->setStyleSheet(QString("background: %1; margin: 6px 16px;").arg(HUB_BORDER));
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

    // ── Main content stack ────────────────────────────────────────────────────
    m_stack = new QStackedWidget(root);
    m_stack->setStyleSheet(QString("background: %1;").arg(HUB_BG));
    m_stack->addWidget(new QWidget());  // index 0 = theory    placeholder
    m_stack->addWidget(new QWidget());  // index 1 = circuit   placeholder
    m_stack->addWidget(new QWidget());  // index 2 = sessions  placeholder
    m_stack->addWidget(new QWidget());  // index 3 = parking   placeholder
    m_stack->addWidget(new QWidget());  // index 4 = assistant placeholder

    rootLayout->addWidget(sidebar);
    rootLayout->addWidget(m_stack, 1);

    connect(m_theoryBtn,    &QPushButton::clicked, this, &StudentLearningHub::showTheory);
    connect(m_circuitBtn,   &QPushButton::clicked, this, &StudentLearningHub::showCircuit);
    connect(m_sessionsBtn,  &QPushButton::clicked, this, &StudentLearningHub::showSessions);
    connect(m_parkingBtn,   &QPushButton::clicked, this, &StudentLearningHub::showParking);
    connect(m_assistantBtn, &QPushButton::clicked, this, &StudentLearningHub::showAssistant);

    showTheory();
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
            .arg(HUB_LOCKED_TEXT));
    } else {
        btn->setText(QString("  %1  %2 · %3").arg(icon).arg(stepNum).arg(label));
        btn->setEnabled(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString(
            "QPushButton { background: transparent; color: %1; "
            "border: none; border-left: 3px solid transparent; "
            "text-align: left; padding-left: 16px; "
            "font-size: 14px; font-weight: 500; border-radius: 0; }"
            "QPushButton:hover { background: rgba(255,255,255,0.05); color: %2; }")
            .arg(HUB_MUTED, HUB_TEXT));
    }
    return btn;
}

// ── setActiveBtn ──────────────────────────────────────────────────────────────
void StudentLearningHub::setActiveBtn(QPushButton *active)
{
    struct BtnInfo { QPushButton *btn; bool locked; };
    const QList<BtnInfo> btns = {
        { m_theoryBtn,   false             },
        { m_circuitBtn,  m_studentStep < 1 },
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
                .arg(HUB_ACTIVE, HUB_TEAL));
        } else {
            bi.btn->setStyleSheet(QString(
                "QPushButton { background: transparent; color: %1; "
                "border: none; border-left: 3px solid transparent; "
                "text-align: left; padding-left: 16px; "
                "font-size: 14px; font-weight: 500; border-radius: 0; }"
                "QPushButton:hover { background: rgba(255,255,255,0.05); color: %2; }")
                .arg(HUB_MUTED, HUB_TEXT));
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
                .arg(HUB_SUB_ACTIVE, HUB_TEAL));
        } else {
            m_theoryNavBtns[i]->setStyleSheet(QString(
                "QPushButton { background: transparent; color: %1; "
                "border: none; border-left: 3px solid transparent; "
                "text-align: left; padding-left: 18px; "
                "font-size: 13px; font-weight: 500; border-radius: 0; }"
                "QPushButton:hover { background: rgba(255,255,255,0.05); color: %2; }")
                .arg(HUB_MUTED, HUB_TEXT));
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
    if (m_studentStep < 1) {
        QMessageBox::information(this,
            QString::fromUtf8("🔒 Étape verrouillée"),
            "Vous devez valider l'étape Code / Théorie avant d'accéder au Circuit.\n\n"
            "Complétez les cours et les quiz de Théorie pour débloquer cette section.");
        return;
    }

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

    // Always recreate to show up-to-date scores after sync
    if (m_sessionsPage) {
        m_stack->removeWidget(m_sessionsPage);
        delete m_sessionsPage;
        m_sessionsPage = nullptr;
    }

    WinoBootstrap::bootstrap();
    qApp->setProperty("currentUserId", m_studentId);
    m_sessionsPage = new WinoStudentDashboard(this);
    m_stack->removeWidget(m_stack->widget(2));
    m_stack->insertWidget(2, m_sessionsPage);
    m_stack->setCurrentIndex(2);
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
        m_stack->removeWidget(m_stack->widget(3));
        m_stack->insertWidget(3, m_parkingPage);
    }
    m_stack->setCurrentIndex(3);
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
