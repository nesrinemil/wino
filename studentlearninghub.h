#ifndef STUDENTLEARNINGHUB_H
#define STUDENTLEARNINGHUB_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QVBoxLayout>
#include "smartdrivewindow.h"
#include "studentdashboard.h"

// Forward declare to avoid include order issues
class StudentPortal;

// ─────────────────────────────────────────────────────────────────────────────
// StudentLearningHub
// Main student window after approval.
// Sidebar: Theory (TACHE SmartDriveWindow) | Circuit (StudentPortal)
// ─────────────────────────────────────────────────────────────────────────────
class StudentLearningHub : public QMainWindow
{
    Q_OBJECT

public:
    explicit StudentLearningHub(int studentId, QWidget *parent = nullptr);

private slots:
    void showTheory();
    void showCircuit();
    void showMySchool();

private:
    int              m_studentId;
    QStackedWidget  *m_stack;
    QPushButton     *m_theoryBtn;
    QPushButton     *m_circuitBtn;
    QPushButton     *m_mySchoolBtn;
    QPushButton     *m_logoutBtn;

    SmartDriveWindow  *m_theoryPage   = nullptr;
    QWidget           *m_circuitPage  = nullptr;
    StudentDashboard  *m_mySchoolPage = nullptr;

    // Theory sub-navigation (shown only when Theory is active)
    QWidget         *m_theorySubNav = nullptr;
    QList<QPushButton*> m_theoryNavBtns;
    int              m_activeTheoryIdx = 0;

    void setActiveBtn(QPushButton *active);
    void setActiveTheoryBtn(int index);
    QPushButton* makeNavBtn(QWidget *parent, const QString &icon, const QString &label);
    
    void updateThemeColors();
};

#endif // STUDENTLEARNINGHUB_H
