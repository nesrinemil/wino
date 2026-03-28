#include "smartdrivewindow.h"
#include "../9/animatedstackedwidget.h"
#include <QApplication>
#include <QHBoxLayout>
#include <QFile>
#include <QDebug>

SmartDriveWindow::SmartDriveWindow(int studentId, QWidget *parent)
    : QMainWindow(parent)
    , m_isDarkMode(true)
    , m_studentId(studentId)
{
    setWindowTitle("SmartDrive — Driving Theory");
    resize(1280, 820);
    setMinimumSize(1050, 720);

    loadStylesheet(true);

    // Use the same DB connection as maryem (already open)
    m_db = new DatabaseManager(this);
    if (!m_db->initialize())
        qWarning() << "SmartDriveWindow: failed to attach DB";

    // Ensure a USER_PROGRESS row exists for this student
    m_db->ensureUserProgress(m_studentId);

    // Outer stack: loading screen (0) → app (1)
    m_outerStack = new QStackedWidget(this);
    setCentralWidget(m_outerStack);

    UserProgress progress = m_db->loadUserProgress(m_studentId);
    m_loadingScreen = new LoadingScreen(progress.progressPercent, this);
    m_outerStack->addWidget(m_loadingScreen);

    m_appWidget = new QWidget(this);
    QHBoxLayout *appLayout = new QHBoxLayout(m_appWidget);
    appLayout->setContentsMargins(0, 0, 0, 0);
    appLayout->setSpacing(0);

    m_sidebar = new Sidebar(this);
    appLayout->addWidget(m_sidebar);

    m_learningModule = new CodeLearningModule(m_db, m_studentId, this);
    appLayout->addWidget(m_learningModule, 1);

    m_outerStack->addWidget(m_appWidget);

    connect(m_loadingScreen, &LoadingScreen::loadingComplete,
            this, &SmartDriveWindow::onLoadingComplete);
    connect(m_sidebar, &Sidebar::navigationClicked,
            this, &SmartDriveWindow::onSidebarNavigation);
    connect(m_sidebar, &Sidebar::darkModeToggled,
            this, &SmartDriveWindow::onDarkModeToggled);
    connect(m_learningModule, &CodeLearningModule::darkModeToggled,
            this, &SmartDriveWindow::onDarkModeToggled);
    connect(m_sidebar, &Sidebar::resetProgressClicked,
            m_learningModule, &CodeLearningModule::onResetProgress);

    m_outerStack->setCurrentIndex(0);
    m_loadingScreen->startLoading();
}

SmartDriveWindow::~SmartDriveWindow() {}

void SmartDriveWindow::onLoadingComplete()
{
    m_outerStack->setCurrentIndex(1);
}

void SmartDriveWindow::onSidebarNavigation(int index)
{
    auto *stack = m_learningModule->findChild<AnimatedStackedWidget*>();
    if (!stack) return;
    switch (index) {
    case 0: case 1: case 2: stack->setCurrentIndexAnimated(0); break;
    case 3:
        stack->setCurrentIndexAnimated(
            (m_learningModule->calculateProgress() >= 100) ? 2 : 0);
        break;
    case 4: stack->setCurrentIndexAnimated(3); break;
    case 5: stack->setCurrentIndexAnimated(4); break;
    }
}

void SmartDriveWindow::onDarkModeToggled(bool isDark)
{
    if (m_isDarkMode == isDark) return;
    m_isDarkMode = isDark;
    loadStylesheet(isDark);
    m_sidebar->setDarkMode(isDark);
    m_learningModule->setDarkMode(isDark);
}

void SmartDriveWindow::loadStylesheet(bool dark)
{
    QString path = dark ? ":/resources/style.qss" : ":/resources/style_light.qss";
    QFile f(path);
    if (f.open(QFile::ReadOnly | QFile::Text)) {
        qApp->setStyleSheet(f.readAll());
        f.close();
    }
}
