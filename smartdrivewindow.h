#ifndef SMARTDRIVEWINDOW_H
#define SMARTDRIVEWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>

// TACHE module includes (referenced from ../hous/)
#include "../hous/databasemanager.h"
#include "../hous/loadingscreen.h"
#include "../hous/codelearningmodule.h"
#include "../hous/sidebar.h"

class SmartDriveWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit SmartDriveWindow(int studentId, QWidget *parent = nullptr);
    ~SmartDriveWindow();

    // Called by StudentLearningHub sidebar to navigate TACHE content
    void navigateToIndex(int index);
    // Called by StudentLearningHub reset button
    void resetProgress();

private slots:
    void onLoadingComplete();
    void onSidebarNavigation(int index);
    void onDarkModeToggled(bool isDark);

private:
    DatabaseManager      *m_db;
    QStackedWidget       *m_outerStack;
    LoadingScreen        *m_loadingScreen;
    CodeLearningModule   *m_learningModule;
    Sidebar              *m_sidebar;
    QWidget              *m_appWidget;
    bool                  m_isDarkMode;
    int                   m_studentId;

    void loadStylesheet(bool dark = true);
};

#endif // SMARTDRIVEWINDOW_H
