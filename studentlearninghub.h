#ifndef STUDENTLEARNINGHUB_H
#define STUDENTLEARNINGHUB_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QVBoxLayout>
#include <QProgressBar>
#include "wino/thememanager.h"
#include "smartdrivewindow.h"
#include "parking/parkingwidget.h"
#include "parking/parkingdbmanager.h"
#include "wino/winoassistantwidget.h"

// Forward declarations
class StudentPortal;
class WinoStudentDashboard;

// ─────────────────────────────────────────────────────────────────────────────
// StudentLearningHub
// Main student window after login.
// Sidebar: Code/Théorie (TACHE) | Circuit (StudentPortal) | Sessions (WINO)
// Steps are locked until the previous step is validated.
// Progression is stored in WINO_PROGRESS table.
// ─────────────────────────────────────────────────────────────────────────────
class StudentLearningHub : public QMainWindow
{
    Q_OBJECT

public:
    explicit StudentLearningHub(int studentId, QWidget *parent = nullptr);

private slots:
    void showTheory();
    void showCircuit();
    void showSessions();
    void showParking();
    void showAssistant();
    void applyTheme();          // called when ThemeManager emits themeChanged

private:
    // ── Identity ─────────────────────────────────────────────────────────────
    int   m_studentId;

    // ── Progression (loaded from WINO_PROGRESS) ───────────────────────────
    // 0 = Code/Théorie only accessible
    // 1 = Circuit unlocked (Code validated)
    // 2 = Sessions unlocked (Circuit validated)
    int    m_studentStep   = 0;
    double m_codeScore     = 0.0;
    double m_circuitScore  = 0.0;
    double m_parkingScore  = 0.0;

    // ── Navigation ───────────────────────────────────────────────────────────
    QFrame         *m_sidebar       = nullptr;   // kept for applyTheme()
    QPushButton    *m_activeMainBtn = nullptr;   // currently highlighted nav button
    QPushButton    *m_themeBtn      = nullptr;   // light/dark toggle in sidebar bottom
    QStackedWidget *m_stack;
    QPushButton    *m_theoryBtn;
    QPushButton    *m_circuitBtn;
    QPushButton    *m_sessionsBtn;
    QPushButton    *m_parkingBtn    = nullptr;
    QPushButton    *m_assistantBtn  = nullptr;
    QLabel         *m_stepScoreLbl  = nullptr;

    // ── Pages (lazy-loaded) ──────────────────────────────────────────────────
    SmartDriveWindow     *m_theoryPage    = nullptr;
    QWidget              *m_circuitPage   = nullptr;
    WinoStudentDashboard *m_sessionsPage  = nullptr;
    ParkingWidget        *m_parkingPage   = nullptr;
    WinoAssistantWidget  *m_assistantPage = nullptr;

    // ── Theory sub-navigation ─────────────────────────────────────────────────
    QWidget             *m_theorySubNav   = nullptr;
    QList<QPushButton*>  m_theoryNavBtns;
    int                  m_activeTheoryIdx = 0;

    // ── Helpers ───────────────────────────────────────────────────────────────
    void        loadStudentStep();
    void        syncScoresToProgress();   // reads TACHE+Circuit scores → WINO_PROGRESS
    void        updateScoreLabel(int activeStep);
    void        setActiveBtn(QPushButton *active);
    void        setActiveTheoryBtn(int index);
    QPushButton* makeStepNavBtn(QWidget *parent, int stepNum,
                                const QString &icon, const QString &label,
                                bool locked);
};

#endif // STUDENTLEARNINGHUB_H
