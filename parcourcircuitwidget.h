#ifndef PARCOURCIRCUITWIDGET_H
#define PARCOURCIRCUITWIDGET_H

#include <QtWidgets>
#include <QSerialPort>
#include <QTimer>
#include <QElapsedTimer>
#include <QDateTime>
#include <QtSql>
#include "dataprocessor.h"   // ProcessedData + SessionContext for CircuitDashboard bridge

// ──────────────────────────────────────────────────────────────────────────────
// CircuitTrackView — Shows the actual circuit photos, switches per step
// Photo mapping:
//   steps 0-3  → circuit1.jpeg  (overview, car at START)
//   step  4    → circuit2.jpeg  (car at traffic light / Turn Right)
//   step  5    → circuit3.jpeg  (car on Segment 6 / Doudane)
//   step  ≥6   → circuit4.jpeg  (car past barrier, session done)
// ──────────────────────────────────────────────────────────────────────────────
class CircuitTrackView : public QLabel
{
    Q_OBJECT
public:
    explicit CircuitTrackView(QWidget *parent = nullptr);

    // Update which step the session is on (0 = start, 6 = done)
    void setStep(int step, int totalSteps);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void reloadPixmap();

    int     m_step       = 0;
    int     m_totalSteps = 6;
    // Pre-loaded pixmaps (loaded once, scaled on resize)
    QPixmap m_photos[4];
    // Progress bar overlay drawn in paintEvent? → We do it via a child widget
    QLabel *m_progressBar = nullptr;
    QLabel *m_stepOverlay = nullptr;
};

// ──────────────────────────────────────────────────────────────────────────────
// ParcourCircuitWidget — full instructor circuit-session interface
// ──────────────────────────────────────────────────────────────────────────────
class ParcourCircuitWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ParcourCircuitWidget(QWidget *parent = nullptr);
    ~ParcourCircuitWidget();

    void setInstructorId(int id) { m_instructorId = id; }
    void setSchoolId(int id)     { m_schoolId     = id; }
    void setStudentId(int id, const QString &name = QString()) { m_studentId = id; m_studentName = name; }

signals:
    // Emitted at session end — bridges results to CircuitDashboard
    void sessionCompleted(const QVector<ProcessedData> &data,
                          double stressIndex,   // 0.0 – 1.0
                          double riskScore);    // 0.0 – 1.0

private slots:
    void onConnectArduino();
    void onSerialDataReady();
    void onSerialError(QSerialPort::SerialPortError error);
    void startSession();
    void endSession();
    void advanceStep();
    void onNextClicked();
    void onTimerTick();
    void loadHistory();

private:
    // ── Layout / UI ──────────────────────────────────────────────────────────
    void buildUi();
    QWidget *buildTopBar();
    QWidget *buildSessionCard();
    QWidget *buildArduinoCard();
    QWidget *buildHistoryCard();
    void     updateStepUI();
    void     appendArduinoMessage(const QString &line);
    void     showFloatingMessage(const QString &text, const QString &color);

    // ── Data ─────────────────────────────────────────────────────────────────
    void saveSessionToDb();

    // ── Members ─────────────────────────────────────────────────────────────
    int  m_instructorId = 0;
    int  m_schoolId     = 0;

    // Circuit steps definition
    struct CircuitStep {
        QString label;       // e.g. "Segment 1 — Straight 45 cm"
        QString sensorPhase; // PREP / SEG1 / TURN_L / SEG5 / LIGHT_R / FINAL / DONE
        QString hint;        // guidance text
    };
    QVector<CircuitStep> m_steps;
    int  m_currentStep  = 0;
    int  m_totalSteps   = 0;
    bool m_sessionActive      = false;
    bool m_sessionEndHandled  = false;
    bool m_advancePending     = false;
    bool m_examMode           = false;   // true = Mode Examen (saved as 'OUI' in DB)

    // Session timer
    QTimer       *m_tickTimer   = nullptr;
    QElapsedTimer m_elapsed;
    int           m_elapsedSecs = 0;

    // Arduino serial
    QSerialPort  *m_serial           = nullptr;
    QString       m_serialBuf;
    bool          m_arduinoConnected = false;
    QTimer       *m_retryTimer       = nullptr;
    QTimer       *m_modeSendTimer    = nullptr;  // keeps sending mode char until Arduino responds
    bool          m_arduinoResponded = false;    // true once any data arrives
    char          m_activeMode       = '1';      // '1' = Neighborhood, '2' = Wide Street

    // UI widgets
    CircuitTrackView *m_trackView    = nullptr;
    QLabel           *m_stepLabel    = nullptr;   // "Étape 2 / 6"
    QLabel           *m_stepName     = nullptr;   // step title
    QLabel           *m_guidanceLabel= nullptr;
    QLabel           *m_timerLabel   = nullptr;
    QPushButton      *m_startBtn     = nullptr;
    QPushButton      *m_nextBtn      = nullptr;
    QPushButton      *m_endBtn       = nullptr;
    QPushButton      *m_examBtn      = nullptr;   // Mode Examen toggle
    QTextEdit        *m_arduinoLog   = nullptr;
    QPushButton      *m_connectBtn   = nullptr;
    QLabel           *m_connStatus   = nullptr;
    QLabel           *m_lcdStatus    = nullptr;
    QTableWidget     *m_historyTable = nullptr;
    QLabel           *m_floatingMsg  = nullptr;

    // ── Circuit car live monitoring ──────────────────────────────────────────
    QLabel      *m_sensorBar     = nullptr;   // L / R / bZ live values
    QLabel      *m_alertBanner   = nullptr;   // last ERREUR / AVERT
    QLabel      *m_errorCountLbl = nullptr;   // "Fautes: 3"
    int          m_errorCount      = 0;
    bool         m_modeWaiting    = false;
    int           m_frontCloseCount = 0;
    bool          m_hadFrontClose   = false;
    QElapsedTimer m_stepElapsed;              // how long we've been on current step

    // ── Per-tick data for CircuitDashboard graphs ────────────────────────────
    QVector<ProcessedData> m_sessionData;   // one entry per Arduino sensor line

    // ── Per-session analysis tracking (reset at session start) ─────────────────
    int    m_speedErrors       = 0;   // ERREUR VITESSE
    int    m_bumpErrors        = 0;   // ERREUR dos d'âne
    int    m_curbLeftErrors    = 0;   // ERREUR trottoir gauche
    int    m_curbRightErrors   = 0;   // ERREUR trottoir droit
    int    m_curbLeftWarnings  = 0;   // AVERT proche trottoir gauche
    int    m_curbRightWarnings = 0;   // AVERT proche trottoir droit
    double m_sumBumpZ          = 0.0; // accumulated bZ for average
    double m_maxBumpZ          = 0.0; // peak bZ reading (g-force)
    int    m_bumpCount         = 0;   // number of bZ samples
    double m_sumLeft           = 0.0; // accumulated L distance
    double m_sumRight          = 0.0; // accumulated R distance
    int    m_distCount         = 0;   // number of L/R distance samples

    void showSessionAnalysis();       // show end-of-session analysis dialog

    void sendToArduino(char c);               // write single byte to serial
    void parseSensorLine(const QString &line);// parse "Mode:N | F:... | L:... | R:..."
    void handleCircuitAlert(const QString &line); // handle ERREUR/AVERT

    // ── Per-error log (saved to CIRCUIT_ERRORS at session end) ──────────────────
    struct CircuitErrorRecord {
        QString   type;        // "ERREUR" or "AVERT"
        QString   category;    // "Feu Rouge", "Vitesse", "Dos d'âne", "Trottoir Gauche", etc.
        QString   message;     // raw message from Arduino
        int       step;        // circuit step index (0-5) when it occurred
        QString   stepName;    // step label (human-readable)
        QDateTime timestamp;
    };
    QVector<CircuitErrorRecord> m_errorLog;  // accumulates during session

    // Student tracking for CIRCUIT_ERRORS
    int     m_studentId   = 0;
    QString m_studentName;

    void saveErrorsToDb(const QSqlDatabase &db, int sessionId);  // batch-insert all errors

    // ── Traffic light Arduino (second device on separate COM port) ───────────
    void onConnectLight(const QString &port); // connect/disconnect traffic light Arduino
    void onLightDataReady();                  // readyRead slot for m_serialLight
    void parseLightLine(const QString &line); // parse "Phase : ROUGE / VERT", "INFRACTION", etc.
    void updateLightIndicator();              // update the colored ⚫/🔴/🟢 widget

    QSerialPort  *m_serialLight     = nullptr; // serial port to traffic light Arduino
    QString       m_lightBuf;                  // line buffer for traffic light
    bool          m_lightConnected  = false;
    bool          m_lightIsRed      = false;   // current traffic light state
    int           m_redLightErrors  = 0;       // passages au rouge during this session

    // Traffic light UI
    QPushButton  *m_connectLightBtn = nullptr; // connect / disconnect button
    QLabel       *m_lightStatus     = nullptr; // connection status label
    QLabel       *m_lightIndicator  = nullptr; // colored circle ⚫ / 🔴 / 🟢
    QLabel       *m_redErrLbl       = nullptr; // "Passages au rouge: N"
};

#endif // PARCOURCIRCUITWIDGET_H
