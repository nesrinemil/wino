#include "studentlearninghub.h"
#include "studentportal.h"
#include "circuitdb.h"
#include <QHBoxLayout>
#include <QApplication>
#include <QScreen>
#include "thememanager.h"

// ── Palette ──────────────────────────────────────────────────────────────────
#define HUB_BG      (ThemeManager::instance()->currentTheme() == ThemeManager::Light ? "#F3F4F6" : "#0F172A")
#define HUB_SIDEBAR (ThemeManager::instance()->currentTheme() == ThemeManager::Light ? "#FFFFFF" : "#111827")
#define HUB_TEAL    "#14B8A6"
#define HUB_TEXT    (ThemeManager::instance()->currentTheme() == ThemeManager::Light ? "#1F2937" : "#E2E8F0")
#define HUB_MUTED   (ThemeManager::instance()->currentTheme() == ThemeManager::Light ? "#6B7280" : "#64748B")
#define HUB_ACTIVE  "rgba(20,184,166,0.15)"
#define HUB_BORDER  (ThemeManager::instance()->currentTheme() == ThemeManager::Light ? "#E5E7EB" : "#1E293B")
#define HUB_SUB_ACTIVE "rgba(20,184,166,0.10)"
StudentLearningHub::StudentLearningHub(int studentId, QWidget *parent)
    : QMainWindow(parent)
    , m_studentId(studentId)
{
    setWindowTitle("Student Learning Hub");
    setMinimumSize(1100, 780);
    showMaximized();

    // ── Root layout ──────────────────────────────────────────────────────────
    QWidget *root = new QWidget(this);
    QHBoxLayout *rootLayout = new QHBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    setCentralWidget(root);

    // ── Left sidebar ─────────────────────────────────────────────────────────
    QFrame *sidebar = new QFrame(root);
    sidebar->setObjectName("hubSidebar");
    sidebar->setFixedWidth(220);
    sidebar->setStyleSheet(QString(
        "QFrame { background-color: %1; border-right: 1px solid %2; }")
        .arg(HUB_SIDEBAR, HUB_BORDER));

    QVBoxLayout *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    sideLayout->setSpacing(0);

    // Brand header (centered, same style as SmartDrive sidebar)
    QLabel *logo = new QLabel(QString::fromUtf8("🚗"));
    logo->setAlignment(Qt::AlignCenter);
    logo->setStyleSheet("font-size: 32px; padding: 20px 0 4px 0; background: transparent;");
    QLabel *brand = new QLabel("SmartDrive");
    brand->setObjectName("brandLabel");
    brand->setAlignment(Qt::AlignCenter);
    brand->setStyleSheet(QString(
        "font-size: 16px; font-weight: bold; color: %1; "
        "background: transparent; padding-bottom: 4px;").arg(HUB_TEAL));
    QLabel *sub = new QLabel("Learning Platform");
    sub->setObjectName("subLabel");
    sub->setAlignment(Qt::AlignCenter);
    sub->setStyleSheet(QString(
        "font-size: 11px; color: %1; background: transparent; "
        "padding-bottom: 16px;").arg(HUB_MUTED));

    sideLayout->addWidget(logo);
    sideLayout->addWidget(brand);
    sideLayout->addWidget(sub);

    // Divider
    QFrame *div = new QFrame(sidebar);
    div->setObjectName("divLine");
    div->setFrameShape(QFrame::HLine);
    div->setStyleSheet(QString("background: %1; margin: 0 0;").arg(HUB_BORDER));
    div->setFixedHeight(1);
    sideLayout->addWidget(div);
    sideLayout->addSpacing(12);

    // ── MODULES section label ─────────────────────────────────────────────────
    QLabel *modLabel = new QLabel("  MODULES");
    modLabel->setObjectName("modLabel");
    modLabel->setStyleSheet(QString(
        "font-size: 10px; font-weight: bold; color: %1; letter-spacing: 2px; "
        "background: transparent; padding: 0 20px 8px 20px;").arg(HUB_MUTED));
    sideLayout->addWidget(modLabel);

    // Module buttons (Theory / Circuit)
    m_theoryBtn  = makeNavBtn(sidebar, QString::fromUtf8("📖"), "Theory");
    m_circuitBtn = makeNavBtn(sidebar, QString::fromUtf8("🚦"), "Circuit");
    m_mySchoolBtn = makeNavBtn(sidebar, QString::fromUtf8("🎓"), "My School");

    sideLayout->addWidget(m_theoryBtn);
    sideLayout->addWidget(m_circuitBtn);
    sideLayout->addWidget(m_mySchoolBtn);

    // ── THEORY sub-navigation (hidden when Circuit is active) ─────────────────
    m_theorySubNav = new QWidget(sidebar);
    m_theorySubNav->setStyleSheet("background: transparent;");
    QVBoxLayout *subLayout = new QVBoxLayout(m_theorySubNav);
    subLayout->setContentsMargins(0, 4, 0, 0);
    subLayout->setSpacing(0);

    // Sub-section label
    QLabel *learnLabel = new QLabel("  LEARNING");
    learnLabel->setObjectName("learnLabel");
    learnLabel->setStyleSheet(QString(
        "font-size: 10px; font-weight: bold; color: %1; letter-spacing: 2px; "
        "background: transparent; padding: 8px 20px 6px 20px;").arg(HUB_MUTED));
    subLayout->addWidget(learnLabel);

    // TACHE navigation items (mirror of sidebar.cpp items)
    struct SubItem { QString icon; QString label; int idx; };
    QList<SubItem> subItems = {
        { QString::fromUtf8("📋"), "Dashboard",    0 },
        { QString::fromUtf8("📚"), "Courses",       1 },
        { QString::fromUtf8("📝"), "Quizzes",       2 },
        { QString::fromUtf8("🥽"), "VR Simulation", 3 },
        { QString::fromUtf8("🏆"), "Badges",        4 },
        { QString::fromUtf8("📷"), "Sign Scanner",  5 },
    };

    for (const auto &item : subItems) {
        QPushButton *btn = new QPushButton(m_theorySubNav);
        btn->setText(QString("    %1  %2").arg(item.icon, item.label));
        btn->setFixedHeight(44);
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
            if (m_theoryPage)
                m_theoryPage->navigateToIndex(idx);
        });
    }

    // Reset progress button
    QFrame *resetDiv = new QFrame(m_theorySubNav);
    resetDiv->setFixedHeight(1);
    resetDiv->setStyleSheet(QString("background: %1; margin: 6px 16px;").arg(HUB_BORDER));
    subLayout->addWidget(resetDiv);

    QPushButton *resetBtn = new QPushButton(m_theorySubNav);
    resetBtn->setObjectName("resetBtn");
    resetBtn->setText(QString::fromUtf8("    🔄  Reset Progress"));
    resetBtn->setFixedHeight(44);
    resetBtn->setCursor(Qt::PointingHandCursor);
    resetBtn->setStyleSheet(QString(
        "QPushButton { background: transparent; color: #EF4444; "
        "border: none; border-left: 3px solid transparent; "
        "text-align: left; padding-left: 18px; "
        "font-size: 13px; font-weight: 500; border-radius: 0; }"
        "QPushButton:hover { background: rgba(239,68,68,0.10); }"));
    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        if (m_theoryPage) m_theoryPage->resetProgress();
    });
    subLayout->addWidget(resetBtn);

    sideLayout->addWidget(m_theorySubNav);
    sideLayout->addStretch();

    // ── Logout Button ──────────────────────────────────────────────────────────
    m_logoutBtn = makeNavBtn(sidebar, QString::fromUtf8("🚪"), "Logout");
    m_logoutBtn->setStyleSheet(QString(
        "QPushButton { background: transparent; color: #EF4444; "
        "border: none; border-left: 3px solid transparent; "
        "text-align: left; padding-left: 16px; "
        "font-size: 14px; font-weight: 500; border-radius: 0; }"
        "QPushButton:hover { background: rgba(239,68,68,0.10); }"));
    sideLayout->addWidget(m_logoutBtn);
    
    connect(m_logoutBtn, &QPushButton::clicked, this, [this]() {
        for (QWidget *widget : qApp->topLevelWidgets()) {
            if (widget->inherits("MainWindow")) {
                widget->showNormal();
                break;
            }
        }
        this->close();
    });

    // ── Main content stack ───────────────────────────────────────────────────
    m_stack = new QStackedWidget(root);
    m_stack->setStyleSheet(QString("QStackedWidget { background: %1; }").arg(HUB_BG));

    QWidget *theoryPlaceholder  = new QWidget();
    QWidget *circuitPlaceholder = new QWidget();
    QWidget *schoolPlaceholder  = new QWidget();
    m_stack->addWidget(theoryPlaceholder);   // index 0 = theory
    m_stack->addWidget(circuitPlaceholder);  // index 1 = circuit
    m_stack->addWidget(schoolPlaceholder);   // index 2 = my school

    rootLayout->addWidget(sidebar);
    rootLayout->addWidget(m_stack, 1);

    connect(m_theoryBtn,   &QPushButton::clicked, this, &StudentLearningHub::showTheory);
    connect(m_circuitBtn,  &QPushButton::clicked, this, &StudentLearningHub::showCircuit);
    connect(m_mySchoolBtn, &QPushButton::clicked, this, &StudentLearningHub::showMySchool);

    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, &StudentLearningHub::updateThemeColors);

    showTheory();
}

void StudentLearningHub::updateThemeColors()
{
    // Re-apply styles to main elements using the dynamic macros
    QFrame* sidebar = findChild<QFrame*>("hubSidebar");
    if (sidebar) sidebar->setStyleSheet(QString("QFrame { background-color: %1; border-right: 1px solid %2; }").arg(HUB_SIDEBAR, HUB_BORDER));
    
    m_stack->setStyleSheet(QString("QStackedWidget { background: %1; }").arg(HUB_BG));
    
    // Borders
    for (QFrame* div : findChildren<QFrame*>("divLine")) {
        div->setStyleSheet(QString("background: %1; margin: 0 0;").arg(HUB_BORDER));
    }
    
    // Text labels
    for (QLabel* lbl : findChildren<QLabel*>()) {
        if (lbl->objectName() == "brandLabel") lbl->setStyleSheet(QString("font-size: 16px; font-weight: bold; color: %1; background: transparent; padding-bottom: 4px;").arg(HUB_TEAL));
        else if (lbl->objectName() == "subLabel") lbl->setStyleSheet(QString("font-size: 11px; color: %1; background: transparent; padding-bottom: 16px;").arg(HUB_MUTED));
        else if (lbl->objectName() == "modLabel") lbl->setStyleSheet(QString("font-size: 10px; font-weight: bold; color: %1; letter-spacing: 2px; background: transparent; padding: 0 20px 8px 20px;").arg(HUB_MUTED));
        else if (lbl->objectName() == "learnLabel") lbl->setStyleSheet(QString("font-size: 10px; font-weight: bold; color: %1; letter-spacing: 2px; background: transparent; padding: 8px 20px 6px 20px;").arg(HUB_MUTED));
    }
    
    // Buttons
    QPushButton* activeMain = nullptr;
    if (m_stack->currentIndex() == 0) activeMain = m_theoryBtn;
    else if (m_stack->currentIndex() == 1) activeMain = m_circuitBtn;
    else if (m_stack->currentIndex() == 2) activeMain = m_mySchoolBtn;
    
    if (activeMain) setActiveBtn(activeMain);
    setActiveTheoryBtn(m_activeTheoryIdx);
    
    QPushButton* resetBtn = findChild<QPushButton*>("resetBtn");
    if (resetBtn) resetBtn->setStyleSheet(QString("QPushButton { background: transparent; color: #EF4444; border: none; border-left: 3px solid transparent; text-align: left; padding-left: 18px; font-size: 13px; font-weight: 500; border-radius: 0; } QPushButton:hover { background: rgba(239,68,68,0.10); }"));

    if (m_logoutBtn) m_logoutBtn->setStyleSheet(QString("QPushButton { background: transparent; color: #EF4444; border: none; border-left: 3px solid transparent; text-align: left; padding-left: 16px; font-size: 14px; font-weight: 500; border-radius: 0; } QPushButton:hover { background: rgba(239,68,68,0.10); }"));
}

// ── Helpers ──────────────────────────────────────────────────────────────────
QPushButton* StudentLearningHub::makeNavBtn(QWidget *parent,
                                            const QString &icon,
                                            const QString &label)
{
    QPushButton *btn = new QPushButton(parent);
    btn->setText(QString("  %1  %2").arg(icon, label));
    btn->setFixedHeight(48);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(QString(
        "QPushButton { background: transparent; color: %1; "
        "border: none; border-left: 3px solid transparent; "
        "text-align: left; padding-left: 16px; "
        "font-size: 14px; font-weight: 500; border-radius: 0; }"
        "QPushButton:hover { background: rgba(255,255,255,0.05); color: %2; }")
        .arg(HUB_MUTED, HUB_TEXT));
    return btn;
}

void StudentLearningHub::setActiveBtn(QPushButton *active)
{
    for (QPushButton *btn : {m_theoryBtn, m_circuitBtn, m_mySchoolBtn}) {
        if (btn == active) {
            btn->setStyleSheet(QString(
                "QPushButton { background: %1; color: %2; "
                "border-left: 3px solid %2; text-align: left; padding-left: 13px; "
                "font-size: 14px; font-weight: 600; border-radius: 0; "
                "border-top: none; border-right: none; border-bottom: none; }")
                .arg(HUB_ACTIVE, HUB_TEAL));
        } else {
            btn->setStyleSheet(QString(
                "QPushButton { background: transparent; color: %1; "
                "border: none; border-left: 3px solid transparent; "
                "text-align: left; padding-left: 16px; "
                "font-size: 14px; font-weight: 500; border-radius: 0; }"
                "QPushButton:hover { background: rgba(255,255,255,0.05); color: %2; }")
                .arg(HUB_MUTED, HUB_TEXT));
        }
    }
}

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

void StudentLearningHub::showTheory()
{
    setActiveBtn(m_theoryBtn);
    m_theorySubNav->setVisible(true);

    if (!m_theoryPage) {
        m_theoryPage = new SmartDriveWindow(m_studentId, this);
        m_stack->removeWidget(m_stack->widget(0));
        m_stack->insertWidget(0, m_theoryPage);
    }
    m_stack->setCurrentIndex(0);
    setActiveTheoryBtn(m_activeTheoryIdx);
}

void StudentLearningHub::showCircuit()
{
    setActiveBtn(m_circuitBtn);
    m_theorySubNav->setVisible(false);   // hide TACHE sub-nav

    if (!m_circuitPage) {
        if (!CircuitDB::instance()->isOpen()) {
            OracleConnectionParams p;
            p.drivingSchoolId = 1;
            p.instructorId    = 1;
            p.username        = "smart_driving";
            p.password        = "SmartDrive2026!";
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

void StudentLearningHub::showMySchool()
{
    setActiveBtn(m_mySchoolBtn);
    m_theorySubNav->setVisible(false); // hide Theory sub-nav

    if (!m_mySchoolPage) {
        // Pass the student ID via qApp property so StudentDashboard picks it up
        qApp->setProperty("currentUserId", m_studentId);
        qApp->setProperty("currentUserRole", QString("STUDENT"));

        m_mySchoolPage = new StudentDashboard(this);
        m_stack->removeWidget(m_stack->widget(2));
        m_stack->insertWidget(2, m_mySchoolPage);
    }
    m_stack->setCurrentIndex(2);
}
