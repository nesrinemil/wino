#ifndef PARKINGWIDGET_H
#define PARKINGWIDGET_H

#include <QWidget>
#include "../wino/thememanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTimer>
#include <QFrame>
#include <QProgressBar>
#include <QGridLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QDialog>
#include <QPropertyAnimation>
#include <QPainter>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextEdit>
#include <QLineEdit>
#include <QProcess>
#include <QTemporaryFile>
#include <QSettings>
#include <QInputDialog>

#include "sensormanager.h"
#include "parkingdbmanager.h"

#include <QTextToSpeech>

// ═══════════════════════════════════════════════════════════
//  WeatherWidget — Real-time weather for driving conditions
// ═══════════════════════════════════════════════════════════
class WeatherWidget : public QFrame
{
    Q_OBJECT
public:
    explicit WeatherWidget(QWidget *parent = nullptr);
    void fetchWeather(const QString &city = "Tunis");
signals:
    void weatherLoaded(const QString &condition, double temp, int humidity, double wind);
private slots:
    void onWeatherReply(QNetworkReply *reply);
private:
    QNetworkAccessManager *m_nam;
    QLabel *m_iconLabel, *m_tempLabel, *m_condLabel, *m_windLabel, *m_tipLabel;
    void updateUI(const QString &icon, const QString &condition, double temp, int humidity, double windSpeed);
    QString drivingTip(const QString &condition, double windSpeed, int humidity);
};

// ═══════════════════════════════════════════════════════════
//  PerformanceTrendWidget — Line chart of session scores
// ═══════════════════════════════════════════════════════════
class PerformanceTrendWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PerformanceTrendWidget(QWidget *parent = nullptr);
    void setData(const QList<int> &scores, const QList<bool> &success);
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QList<int> m_scores;
    QList<bool> m_success;
};

// ═══════════════════════════════════════════════════════════
//  StepDiagramWidget — Bird's-eye view parking step diagram
// ═══════════════════════════════════════════════════════════
class StepDiagramWidget : public QWidget
{
    Q_OBJECT
public:
    explicit StepDiagramWidget(QWidget *parent = nullptr);
    void setStep(int maneuver, int step, int totalSteps);
protected:
    void paintEvent(QPaintEvent *) override;
private:
    int m_maneuver = 0, m_step = 0, m_totalSteps = 5;
    void drawCar(QPainter &p, double cx, double cy, double angle, double w, double h, const QColor &color, bool isMain);
    void drawParkingSlot(QPainter &p, double x, double y, double w, double h, bool occupied);
    void drawArrow(QPainter &p, double x1, double y1, double x2, double y2, const QColor &color);
};

class ParkingWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ParkingWidget(const QString &userName, const QString &userRole,
                           int userId, QWidget *parent = nullptr);
    ~ParkingWidget();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void updateSessionTimer();
    void onSensorEvent(const QString &event);
    void onDelayedAction();
    void onVehicleSelected(int vehiculeId);
    void applyTheme();    // connected to ThemeManager::themeChanged

private:
    void setupUI();
    void autoSelectVehicle();   // skip vehicle-selection page; use circuit car
    QWidget* createBanner();
    QWidget* createVehicleSelectionPage();
    QWidget* createHomePage();
    QWidget* createSessionPage();
    QWidget* createManeuverCard(int index);
    QWidget* createStatPill(const QString &icon, const QString &val,
                            const QString &label, const QString &grad);
    QWidget* createStepCard();
    QWidget* createCarDiagram();
    QWidget* createMiniStatusCard();
    QWidget* createMistakeCard();
    QWidget* createBottomBar();
    QWidget* createAchievementsStrip();
    QWidget* createHistoryCard();
    QWidget* createRadarChart();
    QWidget* createErrorRecordingPanel();
    QWidget* createExamReadinessCard();
    QWidget* createReservationPage();
    QWidget* createFullExamCard();
    QWidget* createAnalyticsPage();
    QWidget* createWeatherCard();
    QWidget* createSmartRecommendationCard();
    QWidget* createVideoTutorialCard(int maneuverIndex);
    QWidget* createPerformanceTrendCard();

    // ── AI Assistant page ──
    QWidget* createAssistantPage();
    void sendToAI(const QString &message);
    void onAIReply();
    void addAIBubble(const QString &text, bool isUser);
    void showAIApiKeyDialog();

    void openManeuver(int idx);
    void openFullExam();
    void showChecklistThenStart();
    void startSession();
    void advanceStep();
    void prevStep();
    void endSession();
    void showResultsDialog(bool success, int score);
    void showFullExamResults();
    void showReservationDialog();
    void checkExamReadiness();
    void refreshAnalytics();               // NEW
    void showSessionReplay(int sessionId); // NEW

    void updateStepUI();
    void updateDots();
    void updateCommonMistakes();
    void updateDashboard();
    void updateCarSensorZone(QLabel *zone, QLabel *distLabel, int distCm);
    void loadStatsFromDB();
    void loadHistoryFromDB();
    void refreshHistoryUI();
    void updateLiveScore();

    void connectSensorSignals();
    void connectMaquette();
    void onConnectionChanged(bool connected);
    void handleSensorEventForManeuver(const QString &event);
    void handleCreneauSensorEvent(const QString &event);
    void handleMarcheAvantSensorEvent(const QString &event);
    void handleBatailleSensorEvent(const QString &event);
    void handleEpiSensorEvent(const QString &event);
    void handleMarcheArriereSensorEvent(const QString &event);
    void handleDemiTourSensorEvent(const QString &event);
    void onDistanceUpdated(const QString &sensor, int distCm);
    int  calculateScore(bool success);

    // ── Real Exam Mode (maquette-based) ──
    enum RealExamPhase {
        EXAM_IDLE = 0,
        EXAM_WAITING_START,           // Waiting for student to begin
        EXAM_RIGHT_OBSTACLE,          // Right sensor → obstacle → "Turn slightly LEFT"
        EXAM_LEFT_OBSTACLE,           // Left sensor → obstacle → "Turn slightly RIGHT"
        EXAM_LEFT_SENSOR1,            // Left sensor → sensor1 → "Turn FULL LEFT"
        EXAM_RIGHT_SENSOR2_STOP,      // Right sensor → sensor2 → "STOP"
        EXAM_RIGHT_SENSOR2_TURN,      // After 5s → "Turn FULL RIGHT"
        EXAM_REAR_RIGHT_STOP,         // Rear right → sensor → "STOP"
        EXAM_REAR_RIGHT_TURN,         // After delay → "Turn FULL LEFT"
        EXAM_BOTH_REAR_OBSTACLE,      // Both rear → obstacle → "STOP"
        EXAM_BOTH_REAR_FINAL,         // Both rear → sensor → "STOP + CONGRATULATIONS"
        EXAM_PASSED,                  // Exam passed
        EXAM_FAILED                   // Exam failed
    };

    void startRealExam();
    void handleRealExamEvent(const QString &event);
    void showExamInstruction(const QString &icon, const QString &title,
                             const QString &instruction, const QString &color);
    void advanceExamPhase(RealExamPhase nextPhase);

    RealExamPhase m_realExamPhase;
    QTimer *m_examDelayTimer;
    QLabel *m_examBigIcon;
    QLabel *m_examBigTitle;
    QLabel *m_examBigInstruction;
    QFrame *m_examInstructionCard;
    int m_realExamErrors;

    void initManeuverData();
    void initCommonMistakes();

    // Feedback
    void showFloatingMessage(const QString &msg, const QString &bgColor);
    void flashSensorZone(QLabel *zone, const QString &color);

    // Voice assistance
    void speak(const QString &text);
    void speakStep(int stepIndex);
    void speakSensorWarning(const QString &zone, int distCm);

    // Error Recording
    void recordCalage();
    void recordObstacleContact();
    void recordError(const QString &type);

    // ── Identity ──
    QString m_userName, m_userRole;
    bool m_isMoniteur;

    // ── Navigation ──
    QStackedWidget *m_stack;

    // ── Vehicle ──
    QString m_selectedPlate, m_selectedModel;
    double m_selectedTarif;

    // ── Tarif & Finances ──
    double m_totalDepense;

    // ── Exam Readiness ──
    bool m_examReady;
    QLabel *m_examReadyLabel;
    QPushButton *m_reservationBtn;
    QLabel *m_depenseLabel;
    QVBoxLayout *m_reservationsLayout;
    QFrame *m_newBookingForm;

    // ── Oracle Exam Booking Gate ──
    bool        m_hasExamBookingToday; // true only if EXAM_REGISTRATIONS/EXAM_SESSIONS has today
    QPushButton *m_fullExamStartBtn;   // "Start Full Exam" button (lock/unlock)
    QLabel      *m_fullExamLockLabel;  // Shown when exam is locked
    void checkOracleExamBooking();     // Queries Oracle for today's parking exam booking
    void updateFullExamCard();         // Locks/unlocks the Full Exam card based on booking

    // ── Incremental Score Sync ──
    // Computes a step-by-step parking score from per-maneuver mastery and syncs to Oracle.
    // Formula: score = sum(mastery_i * coverage_i) / 5 * 100
    //   mastery_i  = successes / attempts  (0–1)
    //   coverage_i = min(attempts, 5) / 5  (reaches 1.0 after 5 sessions per maneuver)
    void computeAndSyncParkingScore();

    // ── Trend Widget Refresh ──
    // Reloads the last 20 sessions from DB and updates the histogram.
    void refreshTrendWidget();

    // ── Theme ── (declared as private slot above)

    // ── Session state ──
    QTimer *m_sessionTimer;
    int  m_sessionSeconds;
    bool m_sessionActive, m_examMode;
    bool m_freeTrainingMode;

    // ── Full Exam Mode (4 maneuvers chained in sequence) ──
    bool m_fullExamMode;
    int  m_fullExamPhase;          // 0-3: current phase in the sequence
    QList<int>  m_fullExamScores;  // score per completed phase
    QList<bool> m_fullExamSuccess; // success per completed phase

    // ── Maneuver state ──
    int m_currentManeuver, m_currentStep, m_totalSteps;

    // ── Session page UI ──
    QLabel *m_sessionTitle, *m_timerLabel, *m_examTimerLabel;
    QPushButton *m_examToggle, *m_simToggle, *m_freeToggle;

    // Step card
    QLabel *m_stepNum, *m_stepTitle, *m_stepInstruction;
    StepDiagramWidget *m_stepDiagram;
    QLabel *m_stepTip, *m_stepSensor, *m_guidanceMsg;

    // Step dots
    QList<QWidget*> m_dots;
    QHBoxLayout *m_dotsLayout;

    // Bottom controls
    QPushButton *m_prevBtn, *m_nextBtn, *m_endBtn;
    QPushButton *m_calageBtn, *m_obstacleBtn, *m_errorBtn;

    // Car diagram sensor zones
    QLabel *m_zFront, *m_zLeft, *m_zRight, *m_zRearL, *m_zRearR;
    QLabel *m_dFront, *m_dLeft, *m_dRight, *m_dRearL, *m_dRearR;

    // Mini status
    QLabel *m_scoreLabel, *m_errorsLabel, *m_connStatus;
    QLabel *m_liveScoreLabel;

    // Mistake
    QLabel *m_mistakeTitle, *m_mistakeDesc, *m_mistakeTip;

    // ── Vehicle Selection Page ──
    QVBoxLayout *m_vehicleListLayout;
    QLabel *m_vehiclePageTitle;
    QLabel *m_selectedCarLabel;

    // ── Home page widgets ──
    QLabel *m_dashLevel;
    QProgressBar *m_dashXPBar;
    QLabel *m_dashXPLabel;
    QLabel *m_statSessions, *m_statSuccess, *m_statBest, *m_statStreak;
    QList<QProgressBar*> m_masteryBars;
    QList<QLabel*> m_masteryPcts;
    QList<QLabel*> m_achieveIcons;
    QVBoxLayout *m_historyLayout;

    // ── Floating message ──
    QLabel *m_floatingMsg;
    QTimer *m_floatingTimer;

    // ── Stats FAB (floating bubble → Coach & Stats page) ──
    QPushButton *m_statsFAB;          // Stats Floating Action Button (session page)
    QPushButton *m_vehStatsFAB;       // Stats FAB on vehicle selection page
    QWidget     *m_vehPage;           // Pointer to vehicle selection page (for repositioning)

    // ── Floating notes bubble (WhatsApp-style user notepad) ──
    QPushButton *m_notesFAB;          // Floating Action Button
    QFrame *m_notesBubble;            // Expandable note editor popup
    QTextEdit *m_notesTextEdit;       // Text area for writing
    QLabel *m_notesBubbleTitle;       // Dynamic title
    QLabel *m_notesSaveStatus;        // Auto-save indicator
    QTimer *m_notesAutoSave;          // Auto-save timer
    QWidget *m_notesBubbleParent;
    bool m_notesBubbleOpen;
    void toggleNotesBubble();
    void repositionNotesBubble();
    void saveQuickNote();
    void loadQuickNote();
    QString quickNotePath() const;

    // ── AI Assistant (Gemini via curl.exe) ──
    QScrollArea  *m_aiScrollArea;
    QWidget      *m_aiChatContent;
    QVBoxLayout  *m_aiChatLayout;
    QLineEdit    *m_aiInput;
    QString       m_aiApiKey;
    QJsonArray    m_aiHistory;      // multi-turn conversation
    QLabel       *m_aiTypingLabel;
    QPushButton  *m_aiApiKeyBtn;
    QProcess     *m_aiProc;
    QTemporaryFile *m_aiTmpPayload;
    int           m_aiPrevPage;    // page to return to on Back

    // ── Sensor / Maquette ──
    SensorManager *m_sensorMgr;
    QTimer *m_delayTimer;
    int  m_pendingStep;
    bool m_maquetteConnected, m_simulationMode;
    QFrame *m_simButtonPanel;

    // ── DB Sensor Messages ──
    QLabel *m_dbMsgLabel;
    QMap<QString, QString> m_dbSensorMessages; // event → guidance_message from DB
    QMap<QString, int>     m_eventToDbStep;    // event → DB step_number (1-based)
    QMap<QString, bool>    m_dbStopRequired;   // event → IS_STOP_REQUIRED
    void loadDbSensorMessages(const QString &maneuverType);
    QString dbMessageForEvent(const QString &event) const;
    void checkSensorErrors(const QString &event);

    // ── Database ──
    int  m_currentSessionId, m_currentEleveId, m_currentVehiculeId;
    int  m_nbErrors, m_nbCalages;
    bool m_contactObstacle;
    int  m_simEventIndex;
    QStringList m_simEvents;

    // ── Gamification ──
    int m_totalXP, m_currentLevel, m_practiceStreak;
    int m_totalSessionsCount, m_totalSuccessCount, m_bestTimeSeconds;
    QList<int> m_maneuverSuccessCounts, m_maneuverAttemptCounts;

    // ── Distance tracking ──
    int m_lastDistFront, m_lastDistLeft, m_lastDistRight;
    int m_lastDistRearL, m_lastDistRearR;

    // ── Voice Assistant (TTS) ──
    QTextToSpeech *m_tts;
    bool m_voiceEnabled;
    QPushButton *m_voiceToggle;
    QTimer *m_sensorSpeakCooldown;

    // ── Maneuver data ──
    struct ManeuverStep {
        QString title, instruction, detail, assistMsg, sensorPhase;
    };
    struct CommonMistake {
        QString mistake, consequence, tip;
    };
    QList<QList<ManeuverStep>> m_allManeuvers;
    QList<QList<CommonMistake>> m_allMistakes;
    QStringList m_maneuverNames, m_maneuverIcons, m_maneuverColors;
    QStringList m_maneuverDbKeys; // DB keys: "creneau","bataille","epi","marche_arriere","demi_tour"

    // ── Analytics & Smart Coach ──
    QVBoxLayout *m_analyticsCoachLayout;
    QVBoxLayout *m_analyticsProgressLayout;
    QVBoxLayout *m_analyticsReplayLayout;
    QVBoxLayout *m_analyticsHeatmapLayout;
    QLabel *m_predictionLabel;

    // ── Weather Integration ──
    WeatherWidget *m_weatherWidget;

    // ── Performance Trend ──
    PerformanceTrendWidget *m_trendWidget;

    // ── Smart Recommendation ──
    QLabel *m_recommendLabel;
    QPushButton *m_recommendBtn;
    int m_recommendedManeuver;

    // ── Video Tutorials ──
    QStringList m_videoUrls;
    void initVideoTutorials();
    int computeRecommendedManeuver();
    double computeExamPassProbability();
};

#endif
