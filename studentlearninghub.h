#ifndef STUDENTLEARNINGHUB_H
#define STUDENTLEARNINGHUB_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QVBoxLayout>
#include <QProgressBar>
#include <QResizeEvent>
#include <QShowEvent>
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

signals:
    void logoutRequested();

private slots:
    void showTheory();
    void showCircuit();
    void showSessions();
    void showParking();
    void showSettings();
    void toggleAssistant();
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
    QPushButton    *m_settingsBtn   = nullptr;   // settings button at bottom of sidebar
    QPushButton    *m_logoutBtn     = nullptr;   // logout button at bottom of sidebar
    QStackedWidget *m_stack;
    QPushButton    *m_theoryBtn;
    QPushButton    *m_circuitBtn;
    QPushButton    *m_sessionsBtn;
    QPushButton    *m_parkingBtn    = nullptr;
    QLabel         *m_stepScoreLbl  = nullptr;

    // ── Pages (lazy-loaded) ──────────────────────────────────────────────────
    SmartDriveWindow     *m_theoryPage    = nullptr;
    QWidget              *m_circuitPage   = nullptr;
    WinoStudentDashboard *m_sessionsPage  = nullptr;
    ParkingWidget        *m_parkingPage   = nullptr;

    // ── Floating assistant ────────────────────────────────────────────────────
    QPushButton          *m_fabBtn        = nullptr;
    QFrame               *m_chatPanel     = nullptr;
    bool                  m_chatOpen      = false;

    // ── Theory sub-navigation ─────────────────────────────────────────────────
    QWidget             *m_theorySubNav   = nullptr;
    QList<QPushButton*>  m_theoryNavBtns;
    int                  m_activeTheoryIdx = 0;

    // ── Circuit sub-navigation ────────────────────────────────────────────────
    QWidget             *m_circuitSubNav  = nullptr;

    // ── Sessions sub-navigation ───────────────────────────────────────────────
    QWidget             *m_sessionsSubNav = nullptr;

    // ── Parking sub-navigation ────────────────────────────────────────────────
    QWidget             *m_parkingSubNav  = nullptr;

    // ── Helpers ───────────────────────────────────────────────────────────────
    void        loadStudentStep();
    void        syncScoresToProgress();   // reads TACHE+Circuit scores → WINO_PROGRESS
    void        updateScoreLabel(int activeStep);
    void        setActiveBtn(QPushButton *active);
    void        setActiveTheoryBtn(int index);
    QPushButton* makeStepNavBtn(QWidget *parent, int stepNum,
                                const QString &icon, const QString &label,
                                bool locked);
    void        createFloatingAssistant(QWidget *container);
    void        repositionFloatingAssistant();
    QWidget*    createSettingsPage();

protected:
    void resizeEvent(QResizeEvent *e) override;
    void showEvent(QShowEvent *e) override;
};

#endif // STUDENTLEARNINGHUB_H
