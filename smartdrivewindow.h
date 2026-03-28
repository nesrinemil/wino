#ifndef SMARTDRIVEWINDOW_H
#define SMARTDRIVEWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>

// TACHE module includes (referenced from ../9/)
#include "../9/databasemanager.h"
#include "../9/loadingscreen.h"
#include "../9/codelearningmodule.h"
#include "../9/sidebar.h"

class SmartDriveWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit SmartDriveWindow(int studentId, QWidget *parent = nullptr);
    ~SmartDriveWindow();

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
