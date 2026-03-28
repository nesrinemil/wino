#ifndef STUDENTLEARNINGHUB_H
#define STUDENTLEARNINGHUB_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QVBoxLayout>
#include "smartdrivewindow.h"

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

private:
    int              m_studentId;
    QStackedWidget  *m_stack;
    QPushButton     *m_theoryBtn;
    QPushButton     *m_circuitBtn;

    SmartDriveWindow *m_theoryPage = nullptr;  // lazy-loaded
    QWidget          *m_circuitPage = nullptr; // lazy-loaded

    void buildSidebar(QWidget *parent, QVBoxLayout *layout);
    void setActiveBtn(QPushButton *active);
};

#endif // STUDENTLEARNINGHUB_H
