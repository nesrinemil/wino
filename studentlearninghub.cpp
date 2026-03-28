#include "studentlearninghub.h"
#include "C:/Users/hboug/Downloads/final/SmartDrivingSchool/SmartDrivingSchool/studentportal.h"
#include "C:/Users/hboug/Downloads/final/SmartDrivingSchool/SmartDrivingSchool/circuitdb.h"
#include <QHBoxLayout>
#include <QApplication>
#include <QScreen>

// ── Palette ──────────────────────────────────────────────────────────────────
#define HUB_BG      "#0F172A"
#define HUB_SIDEBAR "#111827"
#define HUB_TEAL    "#14B8A6"
#define HUB_TEXT    "#E2E8F0"
#define HUB_MUTED   "#64748B"
#define HUB_ACTIVE  "rgba(20,184,166,0.15)"
#define HUB_BORDER  "#1E293B"

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
    sidebar->setFixedWidth(200);
    sidebar->setStyleSheet(QString(
        "QFrame { background-color: %1; border-right: 1px solid %2; }")
        .arg(HUB_SIDEBAR, HUB_BORDER));

    QVBoxLayout *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    sideLayout->setSpacing(0);

    // Brand header
    QLabel *logo = new QLabel("🚗");
    logo->setAlignment(Qt::AlignCenter);
    logo->setStyleSheet("font-size: 32px; padding: 20px 0 4px 0; background: transparent;");
    QLabel *brand = new QLabel("Wino");
    brand->setAlignment(Qt::AlignCenter);
    brand->setStyleSheet(QString(
        "font-size: 16px; font-weight: bold; color: %1; "
        "background: transparent; padding-bottom: 6px;").arg(HUB_TEAL));
    QLabel *sub = new QLabel("Learning Platform");
    sub->setAlignment(Qt::AlignCenter);
    sub->setStyleSheet(QString(
        "font-size: 11px; color: %1; background: transparent; "
        "padding-bottom: 20px;").arg(HUB_MUTED));

    sideLayout->addWidget(logo);
    sideLayout->addWidget(brand);
    sideLayout->addWidget(sub);

    // Divider
    QFrame *div = new QFrame(sidebar);
    div->setFrameShape(QFrame::HLine);
    div->setStyleSheet(QString("background: %1; margin: 0 16px;").arg(HUB_BORDER));
    div->setFixedHeight(1);
    sideLayout->addWidget(div);
    sideLayout->addSpacing(12);

    // ── Nav label ────────────────────────────────────────────────────────────
    QLabel *navLabel = new QLabel("MODULES");
    navLabel->setStyleSheet(QString(
        "font-size: 10px; font-weight: bold; color: %1; letter-spacing: 2px; "
        "background: transparent; padding: 0 20px 8px 20px;").arg(HUB_MUTED));
    sideLayout->addWidget(navLabel);

    // ── Nav buttons ──────────────────────────────────────────────────────────
    auto makeNavBtn = [&](const QString &icon, const QString &label) -> QPushButton* {
        QPushButton *btn = new QPushButton(sidebar);
        btn->setText(QString("  %1  %2").arg(icon, label));
        btn->setFixedHeight(48);
        btn->setCheckable(false);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString(
            "QPushButton { background: transparent; color: %1; "
            "border: none; text-align: left; padding-left: 16px; "
            "font-size: 14px; font-weight: 500; border-radius: 0; }"
            "QPushButton:hover { background: rgba(255,255,255,0.05); color: %2; }")
            .arg(HUB_MUTED, HUB_TEXT));
        return btn;
    };

    m_theoryBtn  = makeNavBtn("📖", "Theory");
    m_circuitBtn = makeNavBtn("🚦", "Circuit");

    sideLayout->addWidget(m_theoryBtn);
    sideLayout->addWidget(m_circuitBtn);
    sideLayout->addStretch();

    // ── Main content stack ───────────────────────────────────────────────────
    m_stack = new QStackedWidget(root);
    m_stack->setStyleSheet(QString("background: %1;").arg(HUB_BG));

    // Placeholder pages (lazy-loaded on first click)
    QWidget *theoryPlaceholder = new QWidget();
    QWidget *circuitPlaceholder = new QWidget();
    m_stack->addWidget(theoryPlaceholder);   // index 0 = theory
    m_stack->addWidget(circuitPlaceholder);  // index 1 = circuit

    rootLayout->addWidget(sidebar);
    rootLayout->addWidget(m_stack, 1);

    // Connect nav buttons
    connect(m_theoryBtn,  &QPushButton::clicked, this, &StudentLearningHub::showTheory);
    connect(m_circuitBtn, &QPushButton::clicked, this, &StudentLearningHub::showCircuit);

    // Start on Theory
    showTheory();
}

void StudentLearningHub::setActiveBtn(QPushButton *active)
{
    for (QPushButton *btn : {m_theoryBtn, m_circuitBtn}) {
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
                "border: none; text-align: left; padding-left: 16px; "
                "font-size: 14px; font-weight: 500; border-radius: 0; }"
                "QPushButton:hover { background: rgba(255,255,255,0.05); color: %2; }")
                .arg(HUB_MUTED, HUB_TEXT));
        }
    }
}

void StudentLearningHub::showTheory()
{
    setActiveBtn(m_theoryBtn);

    // Lazy-load SmartDriveWindow (TACHE theory module)
    if (!m_theoryPage) {
        m_theoryPage = new SmartDriveWindow(m_studentId, this);
        m_stack->removeWidget(m_stack->widget(0));
        m_stack->insertWidget(0, m_theoryPage);
    }
    m_stack->setCurrentIndex(0);
}

void StudentLearningHub::showCircuit()
{
    setActiveBtn(m_circuitBtn);

    // Lazy-load StudentPortal (circuit driving analysis)
    if (!m_circuitPage) {
        // Init CircuitDB if not already open
        if (!CircuitDB::instance()->isOpen()) {
            OracleConnectionParams p;
            p.drivingSchoolId = 1;
            p.instructorId    = 1;
            CircuitDB::instance()->initialize(p);
        }

        // Load student profile from CircuitDB
        StudentProfile sp = CircuitDB::instance()->getStudent(m_studentId);
        if (sp.id <= 0) {
            // Fallback: create a minimal profile so portal still shows
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
