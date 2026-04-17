#include "wino_studentdashboard.h"
#include "thememanager.h"
#include "bookingsession.h"
#include "airecommendations.h"
#include "weathercalendarwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QScrollArea>
#include <QStackedWidget>
#include <QCalendarWidget>
#include <QTextCharFormat>
#include <QDialog>
#include <QTimeEdit>
#include <QDateEdit>
#include <QMessageBox>
#include <QMap>
#include <QClipboard>
#include <QTimer>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QVariant>

WinoStudentDashboard::WinoStudentDashboard(QWidget *parent) :
    QWidget(parent)
{
    // Initialize sessions data (exemple de données)
    initializeSessions();
    setupUI();
    
    // Fetch real weather data from Open-Meteo API
    connect(WeatherService::instance(), &WeatherService::weatherDataReady,
            this, &WinoStudentDashboard::onWeatherDataReady);
    WeatherService::instance()->fetchForecast();
}

WinoStudentDashboard::~WinoStudentDashboard()
{
}

void WinoStudentDashboard::initializeSessions()
{
    sessions.clear();
    
    // Fetch student progress
    int studentId = qApp->property("currentUserId").toInt();
    if (studentId == 0) return; 

    QSqlQuery query;
    query.prepare("SELECT sb.SESSION_ID, sb.SESSION_DATE, sb.START_TIME, sb.END_TIME, sb.SESSION_STEP, i.full_name "
                  "FROM SESSION_BOOKING sb "
                  "JOIN INSTRUCTORS i ON sb.INSTRUCTOR_ID = i.id "
                  "WHERE sb.STUDENT_ID = :student_id "
                  "ORDER BY sb.SESSION_DATE ASC, sb.START_TIME ASC");
    query.bindValue(":student_id", studentId);
                  
    if (query.exec()) {
        while (query.next()) {
            Session s;
            s.id = query.value(0).toInt();
            s.date = query.value(1).toDate();
            
            QString startTimeStr = query.value(2).toString();
            QString endTimeStr = query.value(3).toString();
            s.time = QTime::fromString(startTimeStr, "HH:mm");
            QTime endTime = QTime::fromString(endTimeStr, "HH:mm");
            
            // Calculate duration in minutes if possible, else default to 60
            if (s.time.isValid() && endTime.isValid()) {
                s.duration = s.time.secsTo(endTime) / 60;
            } else {
                s.duration = 60;
            }
            
            int step = query.value(4).toInt();
            if (step == 1) s.type = "Code";
            else if (step == 2) s.type = "Circuit";
            else if (step == 3) s.type = "Parking";
            else s.type = "Unknown";
            
            s.instructor = query.value(5).toString();
            
            sessions.append(s);
        }
    } else {
        qDebug() << "Failed to fetch sessions from database:" << query.lastError().text();
    }
}

void WinoStudentDashboard::setupUI()
{
    setWindowTitle("WINO - Student Dashboard");
    setMinimumSize(900, 600);
    
    // Connect to theme manager
    ThemeManager* theme = ThemeManager::instance();
    connect(theme, &ThemeManager::themeChanged, this, &WinoStudentDashboard::onThemeChanged);
    
    // ── Main stacked widget: page 0 = dashboard, page 1 = AI recommendations ──
    m_mainStack = new QStackedWidget(this);
    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(m_mainStack);

    // Page 0: dashboard container
    centralWidget = new QWidget();
    m_mainStack->addWidget(centralWidget);   // index 0

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // ── Header — use QFrame so the background rule is specific and children
    //    inherit transparency (avoids the "tab" look caused by global QWidget rule)
    QFrame *header = new QFrame();
    header->setObjectName("header");
    header->setFixedHeight(64);
    header->setStyleSheet(
        "QFrame#header { background-color: #0F172A; border-bottom: 1px solid #1E293B; }"
        "QFrame#header * { background: transparent; }");   // ← forces ALL children transparent

    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(20, 0, 20, 0);
    headerLayout->setSpacing(12);

    // ── Back button
    QPushButton *backBtn = new QPushButton(QString::fromUtf8("← Back"));
    backBtn->setObjectName("headerBackBtn");
    backBtn->setFixedSize(90, 34);
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setStyleSheet(
        "QPushButton#headerBackBtn {"
        "    background: transparent;"
        "    color: #9CA3AF;"
        "    font-size: 13px; font-weight: 500;"
        "    border: 1px solid #374151;"
        "    border-radius: 8px;"
        "    padding: 0 12px;"
        "}"
        "QPushButton#headerBackBtn:hover { color: #14B8A6; border-color: #14B8A6; }");
    connect(backBtn, &QPushButton::clicked, this, &WinoStudentDashboard::onBackClicked);

    // ── Fetch student name
    int studentId = qApp->property("currentUserId").toInt();
    if (studentId == 0) return;

    QString studentName = "Student";
    QSqlQuery queryName;
    queryName.prepare("SELECT SUBSTR(name,1,INSTR(name||' ',' ')-1), SUBSTR(name,INSTR(name||' ',' ')+1) FROM STUDENTS WHERE id = :student_id");
    queryName.bindValue(":student_id", studentId);
    if (queryName.exec() && queryName.next())
        studentName = queryName.value(0).toString() + " " + queryName.value(1).toString();

    // ── Title
    QLabel *titleLabel = new QLabel("Sessions");
    titleLabel->setObjectName("headerTitle");
    titleLabel->setStyleSheet(
        "QLabel#headerTitle {"
        "    color: #FFFFFF; font-size: 20px; font-weight: bold;"
        "    background: transparent; border: none; }");

    // ── Separator + name
    QLabel *sepLabel = new QLabel("·");
    sepLabel->setStyleSheet("color: #4B5563; font-size: 18px; background: transparent; border: none;");

    QLabel *nameLabel = new QLabel(studentName);
    nameLabel->setObjectName("studentNameLabel");
    nameLabel->setStyleSheet(
        "QLabel#studentNameLabel {"
        "    color: #9CA3AF; font-size: 14px; font-weight: 400;"
        "    background: transparent; border: none; }");

    headerLayout->addWidget(backBtn);
    headerLayout->addSpacing(12);
    headerLayout->addWidget(titleLabel);
    headerLayout->addWidget(sepLabel);
    headerLayout->addWidget(nameLabel);
    headerLayout->addStretch();

    // Theme toggle — kept as hidden member so updateColors() doesn't crash
    themeToggleBtn = new QPushButton();
    themeToggleBtn->setObjectName("themeToggleBtn");
    themeToggleBtn->setVisible(false);
    
    mainLayout->addWidget(header);
    
    // SCROLL AREA
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setObjectName("scrollArea");
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    QWidget *contentWidget = new QWidget();
    contentWidget->setObjectName("contentWidget");
    QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(28, 28, 28, 28);
    contentLayout->setSpacing(20);
    
    // Action buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->setSpacing(20);
    
    QPushButton *bookBtn = new QPushButton("📅  Book Session");
    bookBtn->setObjectName("actionButton");
    bookBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #14B8A6;"
        "    color: white;"
        "    font-size: 15px;"
        "    font-weight: bold;"
        "    border: none;"
        "    border-radius: 10px;"
        "    padding: 16px 32px;"
        "    line-height: 1.5;"
        "}"
        "QPushButton:hover { background-color: #0F9D8E; }"
        "QPushButton:pressed { background-color: #0D9488; }"
    );
    bookBtn->setCursor(Qt::PointingHandCursor);
    connect(bookBtn, &QPushButton::clicked, this, &WinoStudentDashboard::onBookSessionClicked);
    
    QPushButton *aiBtn = new QPushButton("✨  AI Recommendations");
    aiBtn->setObjectName("secondaryButton");
    bool isDark = theme->currentTheme() == ThemeManager::Dark;
    QString aiBtnBg = isDark ? "#1F2937" : "#374151";
    QString aiBtnHover = isDark ? "#374151" : "#4B5563";
    
    aiBtn->setStyleSheet(
        QString("QPushButton {"
        "    background-color: %1;"
        "    color: white;"
        "    font-size: 15px;"
        "    font-weight: bold;"
        "    border: none;"
        "    border-radius: 10px;"
        "    padding: 16px 32px;"
        "    line-height: 1.5;"
        "}"
        "QPushButton:hover { background-color: %2; }"
        "QPushButton:pressed { background-color: #111827; }").arg(aiBtnBg, aiBtnHover)
    );
    aiBtn->setCursor(Qt::PointingHandCursor);
    connect(aiBtn, &QPushButton::clicked, this, &WinoStudentDashboard::onAIRecommendationsClicked);
    
    examBtn = new QPushButton("🎓  Request Exam");
    examBtn->setObjectName("examButton");
    examBtn->setStyleSheet(
        QString("QPushButton {"
        "    background-color: %1;"
        "    color: white;"
        "    font-size: 15px;"
        "    font-weight: bold;"
        "    border: none;"
        "    border-radius: 10px;"
        "    padding: 16px 32px;"
        "    line-height: 1.5;"
        "}"
        "QPushButton:hover { background-color: %2; }"
        "QPushButton:pressed { background-color: #111827; }").arg(aiBtnBg, aiBtnHover)
    );
    examBtn->setCursor(Qt::PointingHandCursor);
    examBtn->setVisible(false); // Hidden by default, shown if score >= 70
    connect(examBtn, &QPushButton::clicked, this, &WinoStudentDashboard::onExamRequestClicked);
    
    buttonLayout->addWidget(bookBtn);
    buttonLayout->addWidget(aiBtn);
    
    contentLayout->addLayout(buttonLayout);
    
    // Info cards row
    QHBoxLayout *cardsLayout = new QHBoxLayout();
    cardsLayout->setSpacing(25);
    
    QWidget *balanceCard = createBalanceCardWithPayment();
    balanceCard->setMinimumHeight(220);
    cardsLayout->addWidget(balanceCard);
    // Real DB queries for cards
    QString scoreValue = "0%";
    QString scoreSubtitle = "No data";
    QString sessionsValue = "0/25";
    
    QSqlQuery queryProgress;
    queryProgress.prepare(
        "SELECT current_step, "
        "       NVL(code_score,0), "
        "       NVL(circuit_score,0), "
        "       NVL(parking_score,0) "
        "FROM WINO_PROGRESS WHERE user_id = :student_id");
    queryProgress.bindValue(":student_id", studentId);
    if (queryProgress.exec() && queryProgress.next()) {
        int curStep      = queryProgress.value(0).isNull() ? 1 : queryProgress.value(0).toInt();
        double codeScore = queryProgress.value(1).toDouble();
        double circScore = queryProgress.value(2).toDouble();
        double parkScore = queryProgress.value(3).toDouble();

        // Pick the score that corresponds to the active step
        double score = (curStep == 1) ? codeScore :
                       (curStep == 2) ? circScore : parkScore;

        scoreValue = QString("%1%").arg(score, 0, 'f', 1);

        // Step name + exam eligibility (≥70% required for all steps)
        QString stepName;
        bool eligible = (score >= 70.0);
        if (curStep == 1)      stepName = "Code";
        else if (curStep == 2) stepName = "Circuit";
        else                   stepName = "Parking";

        scoreSubtitle = eligible
            ? QString::fromUtf8("\u2713 Eligible for %1 Exam").arg(stepName)
            : QString::fromUtf8("\u2717 Improve your score (need \u226570%)");

        examBtn->setText(QString::fromUtf8("\U0001F393  Request %1 Exam").arg(stepName));
        examBtn->setVisible(eligible);
    }

    
    QSqlQuery querySessionCount;
    querySessionCount.prepare("SELECT COUNT(*) FROM SESSION_BOOKING WHERE STUDENT_ID = :student_id AND STATUS IN ('CONFIRMED', 'COMPLETED') AND SESSION_DATE < SYSDATE");
    querySessionCount.bindValue(":student_id", studentId);
    if (querySessionCount.exec() && querySessionCount.next()) {
        int sessionsCompleted = querySessionCount.value(0).toInt();
        sessionsValue = QString("%1/25").arg(sessionsCompleted);
    }
    
    QWidget *scoreCard = createInfoCard("Current Score", scoreValue, scoreSubtitle, "#ECFDF5");
    scoreCard->setMinimumHeight(200);
    
    QVBoxLayout *scoreLayout = qobject_cast<QVBoxLayout*>(scoreCard->layout());
    if (scoreLayout) {
        scoreLayout->addWidget(examBtn);
    }
    
    cardsLayout->addWidget(scoreCard);
    
    QWidget *sessionsCard = createInfoCard("Sessions", sessionsValue, "", "#EEF2FF");
    sessionsCard->setMinimumHeight(180);
    cardsLayout->addWidget(sessionsCard);
    
    contentLayout->addLayout(cardsLayout);
    
    // Progress section
    contentLayout->addWidget(createProgressSection());
    
    // Calendar section avec séances complètes
    contentLayout->addWidget(createCalendarSection());
    
    contentLayout->addStretch();
    
    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea);

    // Apply initial theme colors (page 0 is now ready)
    updateColors();

    // ── Page 1: AI Recommendations (embedded, no longer a dialog) ────────────
    m_aiPage = new AIRecommendations();
    m_mainStack->addWidget(m_aiPage);   // index 1

    // When the user presses "← Back to Dashboard" inside the AI page
    connect(m_aiPage, &AIRecommendations::backRequested, this, [this]() {
        m_mainStack->setCurrentIndex(0);
        updateCalendarHighlights();
        updateBalance();
    });

    // When a session is booked from the AI page, refresh the dashboard data
    connect(m_aiPage, &AIRecommendations::sessionBooked, this, [this]() {
        updateCalendarHighlights();
        updateBalance();
    });

    // ── Page 2: Dedicated Exam Page ──────────────────────────────────────────
    m_examPage = createExamPage();
    m_mainStack->addWidget(m_examPage);  // index 2
}

QWidget* WinoStudentDashboard::createInfoCard(const QString& title, const QString& value, 
                                          const QString& subtitle, const QString& /*color*/)
{
    ThemeManager* theme = ThemeManager::instance();
    
    QFrame *card = new QFrame();
    card->setObjectName("infoCard");
    card->setStyleSheet(
        QString("QFrame#infoCard {"
        "    background-color: %1;"
        "    border-radius: 12px;"
        "}").arg(theme->cardColor())
    );
    
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(20);
    shadow->setXOffset(0);
    shadow->setYOffset(4);
    shadow->setColor(QColor(0, 0, 0, 15));
    card->setGraphicsEffect(shadow);
    
    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(25, 25, 25, 25);
    layout->setSpacing(12);
    
    QLabel *titleLabel = new QLabel(title);
    titleLabel->setObjectName("cardTitle");
    titleLabel->setStyleSheet(
        QString("QLabel#cardTitle { color: %1; font-size: 13px; font-weight: 500; line-height: 1.5; padding: 3px 0; }")
        .arg(theme->secondaryTextColor())
    );
    layout->addWidget(titleLabel);
    
    QLabel *valueLabel = new QLabel(value);
    valueLabel->setObjectName("cardValue");
    QString valueColor = value.contains("-") ? (theme->currentTheme() == ThemeManager::Light ? "#DC2626" : "#F87171") : theme->primaryTextColor();
    valueLabel->setStyleSheet(
        QString("QLabel#cardValue { color: %1; font-size: 36px; font-weight: bold; line-height: 1.3; padding: 5px 0; }").arg(valueColor)
    );
    layout->addWidget(valueLabel);
    
    if (!subtitle.isEmpty()) {
        QLabel *subtitleLabel = new QLabel(subtitle);
        subtitleLabel->setObjectName("cardSubtitle");
        if (subtitle.contains("✓")) {
            QString successColor = theme->currentTheme() == ThemeManager::Light ? "#059669" : "#34D399";
            QString successBg = theme->currentTheme() == ThemeManager::Light ? "#ECFDF5" : "#134E4A";
            subtitleLabel->setStyleSheet(
                QString("QLabel#cardSubtitle {"
                "    color: %1;"
                "    font-size: 12px;"
                "    background-color: %2;"
                "    padding: 8px 14px;"
                "    border-radius: 6px;"
                "    font-weight: 600;"
                "    line-height: 1.5;"
                "}").arg(successColor, successBg)
            );
        } else {
            QString errorColor = theme->currentTheme() == ThemeManager::Light ? "#DC2626" : "#F87171";
            subtitleLabel->setStyleSheet(
                QString("QLabel#cardSubtitle {"
                "    color: %1;"
                "    font-size: 12px;"
                "    font-weight: 500;"
                "    line-height: 1.5;"
                "    padding: 3px 0;"
                "}").arg(errorColor)
            );
        }
        layout->addWidget(subtitleLabel);
    }
    
    if (title == "Sessions") {
        QProgressBar *progress = new QProgressBar();
        progress->setMaximum(25);
        int val = value.split("/").first().toInt();
        progress->setValue(val);
        progress->setTextVisible(false);
        progress->setFixedHeight(8);
        progress->setStyleSheet(
            "QProgressBar {"
            "    background-color: #E5E7EB;"
            "    border-radius: 4px;"
            "    border: none;"
            "}"
            "QProgressBar::chunk {"
            "    background-color: #14B8A6;"
            "    border-radius: 4px;"
            "}"
        );
        layout->addWidget(progress);
    }
    
    layout->addStretch();
    return card;
}

QWidget* WinoStudentDashboard::createProgressSection()
{
    ThemeManager* theme = ThemeManager::instance();
    bool isDark = theme->currentTheme() == ThemeManager::Dark;
    
    QFrame *card = new QFrame();
    card->setObjectName("progressCard");
    card->setStyleSheet(
        QString("QFrame#progressCard {"
        "    background-color: %1;"
        "    border-radius: 12px;"
        "}").arg(theme->cardColor())
    );
    
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(20);
    shadow->setXOffset(0);
    shadow->setYOffset(4);
    shadow->setColor(QColor(0, 0, 0, 15));
    card->setGraphicsEffect(shadow);
    
    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(35, 30, 35, 35);
    layout->setSpacing(25);
    
    QLabel *titleLabel = new QLabel("🎯 Learning Progress");
    titleLabel->setObjectName("progressTitle");
    titleLabel->setStyleSheet(
        QString("QLabel#progressTitle {"
        "    color: %1;"
        "    font-size: 20px;"
        "    font-weight: bold;"
        "    padding: 5px 0;"
        "}").arg(theme->primaryTextColor())
    );
    layout->addWidget(titleLabel);
    
    // Progress stages
    QHBoxLayout *stagesLayout = new QHBoxLayout();
    stagesLayout->setSpacing(20);
    
    int studentId = qApp->property("currentUserId").toInt();
    if (studentId == 0) return new QWidget();

    // Real DB queries for stages
    int codeState = 1; QString codeText = "In Progress";
    int circuitState = 0; QString circuitText = "Locked";
    int parkingState = 0; QString parkingText = "Locked";
    int currentStep = 1; // default: CODE
    QString activeScoreText = "";

    QSqlQuery qProg;
    qProg.prepare(
        "SELECT code_status, circuit_status, parking_status, current_step, "
        "       NVL(code_score,0), NVL(circuit_score,0), NVL(parking_score,0) "
        "FROM WINO_PROGRESS WHERE user_id = :student_id");
    qProg.bindValue(":student_id", studentId);
    if (qProg.exec() && qProg.next()) {
        QString cStatus    = qProg.value(0).toString();
        QString circStatus = qProg.value(1).toString();
        QString pStatus    = qProg.value(2).toString();
        if (!qProg.value(3).isNull()) currentStep = qProg.value(3).toInt();

        double codeScore = qProg.value(4).toDouble();
        double circScore = qProg.value(5).toDouble();
        double parkScore = qProg.value(6).toDouble();

        // Show score of the currently active step
        double activeScore = (currentStep == 1) ? codeScore :
                             (currentStep == 2) ? circScore : parkScore;
        activeScoreText = QString(" — Score: %1%").arg(activeScore, 0, 'f', 1);

        auto mapStatus = [](const QString &dbStatus, int &state, QString &text) {
            if (dbStatus == "COMPLETED" || dbStatus == "PASSED") { state = 2; text = "Completed"; }
            else if (dbStatus == "IN_PROGRESS" || dbStatus == "ACTIVE") { state = 1; text = "In Progress"; }
            else { state = 0; text = "Locked"; }
        };

        mapStatus(cStatus,    codeState,    codeText);
        mapStatus(circStatus, circuitState, circuitText);
        mapStatus(pStatus,    parkingState, parkingText);
    }

    // Active step badge
    QString activeStepName = (currentStep == 1) ? "Code Theory"
                           : (currentStep == 2) ? "Circuit"
                                                : "Parking";
    QString activeBadgeBg    = isDark ? "#115E59" : "#CCFBF1";
    QString activeBadgeColor = isDark ? "#2DD4BF" : "#0F766E";

    QLabel *activeBadge = new QLabel(QString::fromUtf8("\u25B6  Active Stage: %1%2").arg(activeStepName, activeScoreText));
    activeBadge->setStyleSheet(
        QString("QLabel {"
        "    background-color: %1;"
        "    color: %2;"
        "    font-size: 13px;"
        "    font-weight: 600;"
        "    padding: 6px 14px;"
        "    border-radius: 20px;"
        "    border: none;"
        "}")
        .arg(activeBadgeBg, activeBadgeColor)
    );
    QHBoxLayout *badgeRow = new QHBoxLayout();
    badgeRow->addWidget(activeBadge);
    badgeRow->addStretch();
    layout->addLayout(badgeRow);

    stagesLayout->addWidget(createStageBox("Code Theory", codeText, codeState));
    stagesLayout->addWidget(createStageBox("Circuit", circuitText, circuitState));
    stagesLayout->addWidget(createStageBox("Parking", parkingText, parkingState));
    
    layout->addLayout(stagesLayout);
    
    // ── Exam Timeline & Next Exam Banner ──
    QVBoxLayout *timelineLay = new QVBoxLayout();
    timelineLay->setSpacing(15);
    timelineLay->setContentsMargins(0, 15, 0, 0);

    // Query Upcoming Exam
    QSqlQuery qUpc;
    qUpc.prepare("SELECT EXAM_STEP, TO_CHAR(CONFIRMED_DATE, 'DD/MM/YYYY') "
                 "FROM EXAM_REQUEST WHERE STUDENT_ID = :id AND STATUS = 'APPROVED' "
                 "ORDER BY CONFIRMED_DATE ASC");
    qUpc.bindValue(":id", studentId);
    if (qUpc.exec() && qUpc.next()) {
        int eStep = qUpc.value(0).toInt();
        QString eDate = qUpc.value(1).toString();
        QString eName = (eStep == 1) ? "Code Theory" : (eStep == 2) ? "Circuit" : "Parking";
        
        QFrame *banner = new QFrame();
        banner->setStyleSheet(QString("QFrame { background-color: %1; border-left: 4px solid #F97316; border-radius: 8px; }")
            .arg(isDark ? "#2A1810" : "#FFF7ED"));
        QHBoxLayout *bLay = new QHBoxLayout(banner);
        QLabel *bIcon = new QLabel("🗓️");
        bIcon->setStyleSheet("font-size: 24px; background: transparent; border: none;");
        QLabel *bText = new QLabel(QString("<b>Next Exam:</b> %1 Scheduled on <span style='color:#EA580C;'><b>%2</b></span>").arg(eName, eDate));
        bText->setStyleSheet(QString("QLabel { color: %1; font-size: 14px; background: transparent; border: none; }")
            .arg(isDark ? "#FDBA74" : "#9A3412"));
        bLay->addWidget(bIcon);
        bLay->addWidget(bText);
        bLay->addStretch();
        timelineLay->addWidget(banner);
    }

    // Query Exam History & Sessions
    QFrame *histCard = new QFrame();
    histCard->setStyleSheet(QString("QFrame { background-color: %1; border: 1px solid %2; border-radius: 10px; padding: 10px; }")
        .arg(isDark ? "#1F2937" : "#F9FAFB", theme->borderColor()));
    QVBoxLayout *hLay = new QVBoxLayout(histCard);
    hLay->setSpacing(8);
    QLabel *hTitle = new QLabel("📈 Exam History");
    hTitle->setStyleSheet(QString("QLabel { color: %1; font-size: 14px; font-weight: bold; border: none; background: transparent; }")
        .arg(theme->primaryTextColor()));
    hLay->addWidget(hTitle);

    auto addHistItem = [&](const QString& name, int stepNum) {
        int attempts = 0;
        int passed = 0;
        QSqlQuery qAtt;
        qAtt.prepare("SELECT COUNT(*), SUM(PASSED) FROM EXAM_REQUEST WHERE STUDENT_ID = :id AND EXAM_STEP = :st AND STATUS = 'COMPLETED'");
        qAtt.bindValue(":id", studentId);
        qAtt.bindValue(":st", stepNum);
        if (qAtt.exec() && qAtt.next()) {
            attempts = qAtt.value(0).toInt();
            passed = qAtt.value(1).toInt();
        }
        
        int sessions = 0;
        QString sType = (stepNum == 1) ? "Code" : (stepNum == 2) ? "Circuit" : "Parking";
        QSqlQuery qSess;
        qSess.prepare("SELECT COUNT(*) FROM SESSION_BOOKING WHERE student_id = :id AND type = :t AND status IN ('CONFIRMED', 'COMPLETED') AND SESSION_DATE < SYSDATE");
        qSess.bindValue(":id", studentId);
        qSess.bindValue(":t", sType);
        if (qSess.exec() && qSess.next()) {
            sessions = qSess.value(0).toInt();
        }

        if (attempts > 0 || sessions > 0) {
            QString statusText;
            if (passed > 0) {
                QString ord = (attempts==1)?"st":(attempts==2)?"nd":(attempts==3)?"rd":"th";
                statusText = QString("<span style='color:%1;'>Passed on %2%3 attempt 🏆</span>").arg(isDark?"#34D399":"#10B981").arg(attempts).arg(ord);
            } else if (attempts > 0) {
                statusText = QString("<span style='color:%1;'>Failed %2 time(s)</span>").arg(isDark?"#F87171":"#EF4444").arg(attempts);
            } else {
                statusText = QString("<span style='color:%1;'>Not attempted yet</span>").arg(theme->secondaryTextColor());
            }
            
            QLabel *l = new QLabel(QString("<b>%1</b>: %2 <i>(After %3 sessions)</i>").arg(name, statusText, QString::number(sessions)));
            l->setStyleSheet(QString("QLabel { color: %1; font-size: 13px; border: none; background: transparent; }").arg(theme->primaryTextColor()));
            hLay->addWidget(l);
        }
    };

    addHistItem("Code Theory", 1);
    addHistItem("Circuit", 2);
    addHistItem("Parking", 3);

    if (hLay->count() > 1) {
        timelineLay->addWidget(histCard);
    } else {
        delete histCard;
    }

    if (timelineLay->count() > 0) {
        layout->addLayout(timelineLay);
    } else {
        delete timelineLay;
    }
    
    return card;
}

QWidget* WinoStudentDashboard::createStageBox(const QString& title, const QString& subtitle, int state)
{
    ThemeManager* theme = ThemeManager::instance();
    bool isDark = theme->currentTheme() == ThemeManager::Dark;
    
    QFrame *box = new QFrame();
    
    // Set object name based on state for updateColors to find
    if (state == 2) box->setObjectName("stageBox_Completed");
    else if (state == 1) box->setObjectName("stageBox_Current");
    else box->setObjectName("stageBox_Locked");
    
    QString bgColor, borderColor, titleColor, subtitleColor, icon;
    QString borderStyle = "solid";
    
    // State: 0 = Locked, 1 = Current, 2 = Completed
    if (state == 2) { // Completed
        bgColor = isDark ? "#134E4A" : "#ECFDF5";      // Dark Teal : Light Green
        borderColor = isDark ? "#34D399" : "#10B981";  // Light Green : Green Border
        titleColor = isDark ? "#34D399" : "#065F46";   // Light Green : Dark Green Text
        subtitleColor = isDark ? "#6EE7B7" : "#059669";
        icon = "✅";
    } else if (state == 1) { // Current
        bgColor = isDark ? "#115E59" : "#F0FDFA";      // Dark Teal : Light Teal
        borderColor = "#14B8A6";  // Teal Border (same in both)
        titleColor = theme->primaryTextColor();
        subtitleColor = isDark ? "#5EEAD4" : "#0D9488";
        icon = "🚀";
    } else { // Locked
        bgColor = isDark ? "#1E293B" : "#F9FAFB";      // Dark Grey : Light Grey
        borderColor = isDark ? "#475569" : "#E5E7EB";  // Medium Grey : Light Grey Border
        titleColor = isDark ? "#64748B" : "#9CA3AF";   // Grey Text
        subtitleColor = isDark ? "#64748B" : "#9CA3AF";
        icon = "🔒";
        borderStyle = "dashed";
    }
    
    // Apply styling
    box->setStyleSheet(
        QString("QFrame {"
                "    background-color: %1;"
                "    border: 2px %5 %2;" // border style handled here
                "    border-radius: 12px;"
                "}").arg(bgColor, borderColor, "", "", borderStyle)
    );
    
    QVBoxLayout *boxLayout = new QVBoxLayout(box);
    boxLayout->setContentsMargins(20, 20, 20, 20);
    boxLayout->setSpacing(5);
    
    // Icon
    QLabel *iconLabel = new QLabel(icon);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setStyleSheet("font-size: 24px; border: none; background: transparent;");
    boxLayout->addWidget(iconLabel);
    
    // Title
    QLabel *titleLabel = new QLabel(title);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(
        QString("QLabel {"
                "    font-size: 16px;"
                "    font-weight: bold;"
                "    color: %1;"
                "    border: none;"
                "    background: transparent;"
                "}").arg(titleColor)
    );
    boxLayout->addWidget(titleLabel);
    
    // Subtitle / Status
    QLabel *statusLabel = new QLabel(subtitle);
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setStyleSheet(
        QString("QLabel {"
                "    font-size: 13px;"
                "    font-weight: 600;"
                "    color: %1;"
                "    border: none;"
                "    background: transparent;"
                "}").arg(subtitleColor)
    );
    boxLayout->addWidget(statusLabel);
    
    return box;
}

QWidget* WinoStudentDashboard::createCalendarSection()
{
    ThemeManager* theme = ThemeManager::instance();
    
    QFrame *card = new QFrame();
    card->setObjectName("calendarCard");
    card->setStyleSheet(
        QString("QFrame {"
        "    background-color: %1;"
        "    border-radius: 12px;"
        "}").arg(theme->cardColor())
    );
    
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(20);
    shadow->setXOffset(0);
    shadow->setYOffset(4);
    shadow->setColor(QColor(0, 0, 0, 15));
    card->setGraphicsEffect(shadow);
    
    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(30, 25, 30, 30);
    layout->setSpacing(15);
    
    QLabel *titleLabel = new QLabel("📅 Your Sessions Calendar");
    titleLabel->setObjectName("calendarTitle");
    titleLabel->setStyleSheet(
        QString("QLabel {"
        "    color: %1;"
        "    font-size: 20px;"
        "    font-weight: bold;"
        "    padding: 5px 0 10px 0;"
        "}").arg(theme->primaryTextColor())
    );
    layout->addWidget(titleLabel);
    
    // Custom Calendar Widget
    calendarWidget = new WeatherCalendarWidget();
    calendarWidget->setMinimumHeight(420);
    calendarWidget->setGridVisible(true);
    calendarWidget->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    calendarWidget->setFirstDayOfWeek(Qt::Monday);
    
    // Perfect Professional Calendar Styling
    bool isDark = theme->currentTheme() == ThemeManager::Dark;
    
    QString calBg = theme->cardColor();
    QString calBorder = theme->borderColor();
    QString navBg = theme->headerColor();
    QString textPrimary = theme->primaryTextColor();
    QString textSecondary = theme->secondaryTextColor();
    QString headerBg = isDark ? "#134E4A" : "#F0FDFA";
    QString headerText = isDark ? "#5EEAD4" : "#0D9488";
    
    calendarWidget->setStyleSheet(
        QString(
        // ── Global base ──
        "QCalendarWidget {"
        "    background-color: %1;"
        "    border: 1px solid %2;"
        "    border-radius: 10px;"
        "}"
        
        // ── Navigation Bar ──
        "QCalendarWidget QWidget#qt_calendar_navigationbar {"
        "    background-color: %3;"
        "    border-top-left-radius: 10px;"
        "    border-top-right-radius: 10px;"
        "    padding: 6px 8px;"
        "    min-height: 44px;"
        "}"
        
        // ── Prev / Next / Month / Year buttons ──
        "QCalendarWidget QToolButton {"
        "    color: white;"
        "    background-color: transparent;"
        "    font-size: 15px;"
        "    font-weight: bold;"
        "    border: none;"
        "    border-radius: 6px;"
        "    padding: 6px 14px;"
        "    margin: 2px;"
        "}"
        "QCalendarWidget QToolButton:hover {"
        "    background-color: #14B8A6;"
        "    color: white;"
        "}"
        "QCalendarWidget QToolButton#qt_calendar_prevmonth,"
        "QCalendarWidget QToolButton#qt_calendar_nextmonth {"
        "    qproperty-icon: none;"
        "    font-size: 18px;"
        "    min-width: 32px;"
        "}"
        "QCalendarWidget QToolButton::menu-indicator {"
        "    image: none;"
        "}"
        
        // ── Dropdown Menus ──
        "QCalendarWidget QMenu {"
        "    background-color: %1;"
        "    color: %4;"
        "    border: 1px solid %2;"
        "    border-radius: 8px;"
        "    padding: 4px;"
        "    font-size: 13px;"
        "}"
        "QCalendarWidget QMenu::item {"
        "    padding: 6px 20px;"
        "    border-radius: 4px;"
        "}"
        "QCalendarWidget QMenu::item:selected {"
        "    background-color: #14B8A6;"
        "    color: white;"
        "}"
        
        // ── Spin Box ──
        "QCalendarWidget QSpinBox {"
        "    color: white;"
        "    background-color: transparent;"
        "    border: 1px solid #374151;"
        "    border-radius: 6px;"
        "    padding: 4px 8px;"
        "    font-size: 14px;"
        "    font-weight: bold;"
        "}"
        
        // ── Day-of-week header row (Mon, Tue …) ──
        "QCalendarWidget QHeaderView {"
        "    background-color: %5;"
        "    border: none;"
        "    border-bottom: 2px solid #14B8A6;"
        "}"
        "QCalendarWidget QHeaderView::section {"
        "    background-color: %5;"
        "    color: %6;"
        "    font-size: 13px;"
        "    font-weight: bold;"
        "    padding: 8px 0px;"
        "}"
        
        // ── Day cells (main grid) ──
        "QCalendarWidget QAbstractItemView {"
        "    background-color: %1;"
        "    alternate-background-color: %5;"
        "    selection-background-color: #14B8A6;"
        "    selection-color: white;"
        "    font-size: 14px;"
        "    outline: none;"
        "    gridline-color: %2;"
        "    border: none;"
        "}"
        "QCalendarWidget QAbstractItemView:enabled {"
        "    color: %4;"
        "}"
        "QCalendarWidget QAbstractItemView:disabled {"
        "    color: %7;"
        "}"
        
        // ── Today highlight via table view ──
        "QCalendarWidget QTableView {"
        "    outline: none;"
        "    border: none;"
        "}"
        ).arg(calBg, calBorder, navBg, textPrimary, headerBg, headerText, theme->mutedTextColor())
    );
    
    // Style the day-of-week header text format
    QTextCharFormat headerFormat;
    headerFormat.setForeground(QBrush(QColor("#0D9488")));
    headerFormat.setFontWeight(QFont::Bold);
    headerFormat.setBackground(QBrush(QColor("#F0FDFA")));
    calendarWidget->setHeaderTextFormat(headerFormat);
    
    // Style weekend dates (Sat/Sun in a subtle red-ish tone)
    QTextCharFormat weekendFormat;
    weekendFormat.setForeground(QBrush(QColor("#EF4444")));
    calendarWidget->setWeekdayTextFormat(Qt::Saturday, weekendFormat);
    calendarWidget->setWeekdayTextFormat(Qt::Sunday, weekendFormat);
    
    // Highlight dates with sessions
    updateCalendarHighlights();
    
    // Connect click signal
    connect(calendarWidget, &QCalendarWidget::clicked, this, &WinoStudentDashboard::onCalendarDateClicked);
    
    layout->addWidget(calendarWidget);
    
    return card;
}

void WinoStudentDashboard::updateCalendarHighlights()
{
    if (!calendarWidget) return;

    // ── Reload sessions from DB each time (ensures cancels/edits are reflected) ──
    sessions.clear();
    int studentId = qApp->property("currentUserId").toInt();
    if (studentId == 0) return;

    QSqlQuery qSessions;
    qSessions.prepare(
        "SELECT sb.session_id, TO_CHAR(sb.session_date, 'YYYY-MM-DD'), sb.start_time, sb.end_time, sb.session_step, "
        "       i.full_name "
        "FROM SESSION_BOOKING sb "
        "JOIN STUDENTS st ON sb.student_id = st.id "
        "LEFT JOIN INSTRUCTORS i ON st.instructor_id = i.id "
        "WHERE sb.student_id = :sid AND TRIM(sb.status) NOT IN ('CANCELLED', 'FAILED', 'REJECTED') "
    );
    qSessions.bindValue(":sid", studentId);
    if (qSessions.exec()) {
        while (qSessions.next()) {
            Session s;
            s.id          = qSessions.value(0).toInt();
            s.date        = QDate::fromString(qSessions.value(1).toString(), "yyyy-MM-dd");
            s.time        = QTime::fromString(qSessions.value(2).toString().left(5), "HH:mm");
            s.duration    = 60;
            s.type        = qSessions.value(4).toString();
            s.instructor  = qSessions.value(5).toString();
            sessions.append(s);
        }
    }

    // Clear all previous formats first
    calendarWidget->setDateTextFormat(QDate(), QTextCharFormat());

    // Apply weather data to calendar dates (optional — only if data is available)
    WeatherService *weather = WeatherService::instance();
    if (weather && weather->hasData()) {
        QDate today = QDate::currentDate();
        for (int i = 0; i < 16; i++) {
            QDate date = today.addDays(i);
            DayWeather dw = weather->getWeather(date);
            if (dw.weatherCode >= 0) {
                QTextCharFormat weatherFormat;
                weatherFormat.setToolTip(
                    QString("%1 %2\nMax: %3°C | Min: %4°C\n%5")
                    .arg(dw.icon, dw.description)
                    .arg(dw.tempMax, 0, 'f', 0)
                    .arg(dw.tempMin, 0, 'f', 0)
                    .arg(WeatherService::weatherCodeToDrivingAdvice(dw.weatherCode))
                );
                int suitability = WeatherService::weatherCodeToSuitability(dw.weatherCode);
                if (suitability >= 80) {
                    weatherFormat.setBackground(QBrush(QColor("#ECFDF5")));
                } else if (suitability >= 50) {
                    weatherFormat.setBackground(QBrush(QColor("#FEF9C3")));
                } else {
                    weatherFormat.setBackground(QBrush(QColor("#FEE2E2")));
                }
                calendarWidget->setDateTextFormat(date, weatherFormat);
            }
        }
    }

    // Always highlight session dates — regardless of weather
    QTextCharFormat sessionFormat;
    sessionFormat.setBackground(QBrush(QColor("#14B8A6")));
    sessionFormat.setForeground(QBrush(QColor("#FFFFFF")));
    sessionFormat.setFontWeight(QFont::Bold);

    QMap<QDate, QList<Session>> sessionsByDate;
    for (const Session &session : sessions) {
        sessionsByDate[session.date].append(session);
    }

    for (auto it = sessionsByDate.begin(); it != sessionsByDate.end(); ++it) {
        QDate date = it.key();
        QList<Session> daySessions = it.value();
        
        QTextCharFormat fmt = sessionFormat;
        bool isPast = (date < QDate::currentDate());
        if (isPast) {
            bool isDark = ThemeManager::instance()->currentTheme() == ThemeManager::Dark;
            fmt.setBackground(QBrush(QColor(isDark ? "#334155" : "#9CA3AF")));
            fmt.setForeground(QBrush(QColor(isDark ? "#94A3B8" : "#F3F4F6")));
        }
        
        QString tooltipText;
        if (daySessions.size() > 1) {
            tooltipText = QString("📅 %1 Sessions:\n").arg(daySessions.size());
            for (const Session &s : daySessions) {
                if (s.type.contains("Code", Qt::CaseInsensitive)) {
                    tooltipText += QString("- %1 (%2)\n")
                                    .arg(s.type, s.time.toString("HH:mm"));
                } else {
                    tooltipText += QString("- %1 (%2) with %3\n")
                                    .arg(s.type, s.time.toString("HH:mm"), s.instructor);
                }
            }
        } else {
            const Session &s = daySessions.first();
            if (s.type.contains("Code", Qt::CaseInsensitive)) {
                tooltipText = QString("📅 Session: %1\nTime: %2\n")
                    .arg(s.type, s.time.toString("HH:mm"));
            } else {
                tooltipText = QString("📅 Session: %1\nInstructor: %2\nTime: %3\n")
                    .arg(s.type, s.instructor, s.time.toString("HH:mm"));
            }
        }
        
        if (weather && weather->hasData()) {
            DayWeather dw = weather->getWeather(date);
            if (dw.weatherCode >= 0) {
                tooltipText += QString("\n%1 %2 | %3°C/%4°C\n%5")
                    .arg(dw.icon, dw.description)
                    .arg(dw.tempMax, 0, 'f', 0)
                    .arg(dw.tempMin, 0, 'f', 0)
                    .arg(WeatherService::weatherCodeToDrivingAdvice(dw.weatherCode));
            }
        }
        
        if (isPast) {
            tooltipText += "\n\nStatut : Séance effectuée";
        }
        
        fmt.setToolTip(tooltipText.trimmed());
        calendarWidget->setDateTextFormat(date, fmt);
    }

    // ── Highlight Approved Exams ──
    QSqlQuery qUpc;
    qUpc.prepare("SELECT EXAM_STEP, TO_CHAR(CONFIRMED_DATE, 'YYYY-MM-DD') "
                 "FROM EXAM_REQUEST WHERE STUDENT_ID = :id AND STATUS = 'APPROVED'");
    qUpc.bindValue(":id", studentId);
    if (qUpc.exec()) {
        QTextCharFormat examFormat;
        examFormat.setBackground(QBrush(QColor("#F97316"))); // Orange
        examFormat.setForeground(QBrush(QColor("#FFFFFF")));
        examFormat.setFontWeight(QFont::Bold);

        while (qUpc.next()) {
            int eStep = qUpc.value(0).toInt();
            QDate eDate = QDate::fromString(qUpc.value(1).toString(), "yyyy-MM-dd");
            if (eDate.isValid()) {
                QString eName = (eStep == 1) ? "Code Theory Exam" : (eStep == 2) ? "Circuit Exam" : "Parking Exam";
                
                // Preserve existing tooltip if any, else set new
                QTextCharFormat existingFmt = calendarWidget->dateTextFormat(eDate);
                QString tip = existingFmt.toolTip();
                if (!tip.isEmpty()) tip += "\n\n";
                tip += "🎯 " + eName + " Scheduled!";
                
                examFormat.setToolTip(tip);
                calendarWidget->setDateTextFormat(eDate, examFormat);
            }
        }
    }
}

void WinoStudentDashboard::onCalendarDateClicked(const QDate &date)
{
    QList<Session*> sessionsOnDate;
    for (Session &session : sessions) {
        if (session.date == date) {
            sessionsOnDate.append(&session);
        }
    }
    
    if (sessionsOnDate.size() == 1) {
        showSessionDialog(*sessionsOnDate.first());
    } else if (sessionsOnDate.size() > 1) {
        ThemeManager* theme = ThemeManager::instance();
        QDialog *dialog = new QDialog(this);
        dialog->setWindowTitle("Sessions on " + date.toString("MMM d, yyyy"));
        dialog->setMinimumWidth(400);
        dialog->setStyleSheet(QString("QDialog { background-color: %1; border-radius: 12px; }").arg(theme->cardColor()));
        QVBoxLayout *layout = new QVBoxLayout(dialog);
        layout->setContentsMargins(20, 20, 20, 20);
        layout->setSpacing(15);
        
        QLabel *title = new QLabel(QString("You have %1 sessions on %2").arg(sessionsOnDate.size()).arg(date.toString("MMM d")));
        title->setStyleSheet(QString("QLabel { color: %1; font-size: 18px; font-weight: bold; }").arg(theme->primaryTextColor()));
        layout->addWidget(title);
        
        for (Session *s : sessionsOnDate) {
            QPushButton *btn = new QPushButton(QString("%1 at %2 - %3")
                                               .arg(s->type)
                                               .arg(s->time.toString("HH:mm"))
                                               .arg(s->instructor));
            if (theme->currentTheme() == ThemeManager::Dark) {
                btn->setStyleSheet(
                    "QPushButton {"
                    "    background-color: #1E293B;"
                    "    color: #F8FAFC;"
                    "    padding: 12px;"
                    "    border-radius: 8px;"
                    "    font-size: 14px;"
                    "    text-align: left;"
                    "    border: 1px solid #334155;"
                    "}"
                    "QPushButton:hover { background-color: #334155; }"
                );
            } else {
                btn->setStyleSheet(
                    "QPushButton {"
                    "    background-color: #F3F4F6;"
                    "    color: #1F2937;"
                    "    padding: 12px;"
                    "    border-radius: 8px;"
                    "    font-size: 14px;"
                    "    text-align: left;"
                    "    border: 1px solid #E5E7EB;"
                    "}"
                    "QPushButton:hover { background-color: #E5E7EB; }"
                );
            }
            btn->setCursor(Qt::PointingHandCursor);
            connect(btn, &QPushButton::clicked, [=]() {
                dialog->accept();
                showSessionDialog(*s);
            });
            layout->addWidget(btn);
        }
        
        QPushButton *closeBtn = new QPushButton("Close");
        closeBtn->setStyleSheet(
            "QPushButton { background-color: #14B8A6; color: white; border-radius: 8px; padding: 10px; font-weight: bold; font-size: 14px; border: none; }"
            "QPushButton:hover { background-color: #0D9488; }"
        );
        closeBtn->setCursor(Qt::PointingHandCursor);
        connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::reject);
        layout->addWidget(closeBtn);
        
        dialog->exec();
    } else {
        // Show weather detail dialog for this day
        DayWeather dw = WeatherService::instance()->getWeather(date);
        if (dw.weatherCode < 0) return; // no data
        
        ThemeManager* theme = ThemeManager::instance();
        bool isDark = theme->currentTheme() == ThemeManager::Dark;
        int suitability = WeatherService::weatherCodeToSuitability(dw.weatherCode);
        
        QDialog *dialog = new QDialog(this);
        dialog->setWindowTitle("Weather Details");
        dialog->setFixedSize(420, 420);
        dialog->setStyleSheet(QString("QDialog { background-color: %1; border-radius: 12px; }").arg(theme->cardColor()));
        
        QVBoxLayout *layout = new QVBoxLayout(dialog);
        layout->setContentsMargins(30, 25, 30, 25);
        layout->setSpacing(15);
        
        // ── Date Title ──
        QLabel *dateLabel = new QLabel(date.toString("dddd, MMMM d, yyyy"));
        dateLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 18px; font-weight: bold; }").arg(theme->primaryTextColor()));
        layout->addWidget(dateLabel);
        
        // ── Weather Icon + Description ──
        QHBoxLayout *weatherRow = new QHBoxLayout();
        QLabel *iconLbl = new QLabel(dw.icon);
        iconLbl->setStyleSheet("font-size: 48px;");
        
        QVBoxLayout *descCol = new QVBoxLayout();
        QLabel *descLbl = new QLabel(dw.description);
        descLbl->setStyleSheet(QString("QLabel { color: %1; font-size: 16px; font-weight: 600; }").arg(theme->primaryTextColor()));
        QLabel *tempLbl = new QLabel(QString("🌡️ Max: %1°C  |  Min: %2°C").arg(dw.tempMax, 0, 'f', 1).arg(dw.tempMin, 0, 'f', 1));
        tempLbl->setStyleSheet(QString("QLabel { color: %1; font-size: 14px; }").arg(isDark ? "#5EEAD4" : "#0D9488"));
        descCol->addWidget(descLbl);
        descCol->addWidget(tempLbl);
        
        weatherRow->addWidget(iconLbl);
        weatherRow->addLayout(descCol, 1);
        layout->addLayout(weatherRow);
        
        // ── Separator ──
        QFrame *sep = new QFrame();
        sep->setFrameShape(QFrame::HLine);
        sep->setStyleSheet(QString("background-color: %1; border: none;").arg(theme->borderColor()));
        sep->setFixedHeight(1);
        layout->addWidget(sep);
        
        // ── Driving Suitability ──
        QLabel *suitTitle = new QLabel("🚗 Driving Suitability");
        suitTitle->setStyleSheet(QString("QLabel { color: %1; font-size: 14px; font-weight: bold; }").arg(theme->primaryTextColor()));
        layout->addWidget(suitTitle);
        
        QHBoxLayout *suitRow = new QHBoxLayout();
        QProgressBar *suitBar = new QProgressBar();
        suitBar->setMaximum(100);
        suitBar->setValue(suitability);
        suitBar->setTextVisible(false);
        suitBar->setFixedHeight(10);
        QString barColor = suitability >= 80 ? "#10B981" : suitability >= 50 ? "#F59E0B" : "#EF4444";
        suitBar->setStyleSheet(
            QString("QProgressBar { background-color: %1; border-radius: 5px; border: none; }"
                    "QProgressBar::chunk { background-color: %2; border-radius: 5px; }")
            .arg(isDark ? "#374151" : "#E5E7EB", barColor)
        );
        QLabel *scoreLbl = new QLabel(QString("%1%").arg(suitability));
        scoreLbl->setStyleSheet(QString("QLabel { color: %1; font-size: 16px; font-weight: bold; }").arg(barColor));
        suitRow->addWidget(suitBar, 1);
        suitRow->addWidget(scoreLbl);
        layout->addLayout(suitRow);
        
        // ── Driving Advice ──
        QString advice = WeatherService::weatherCodeToDrivingAdvice(dw.weatherCode);
        QString adviceBg, adviceColor, adviceBorder;
        if (suitability >= 80) {
            adviceBg = isDark ? "#134E4A" : "#ECFDF5";
            adviceColor = isDark ? "#34D399" : "#065F46";
            adviceBorder = isDark ? "#34D399" : "#10B981";
        } else if (suitability >= 50) {
            adviceBg = isDark ? "#422006" : "#FEF9C3";
            adviceColor = isDark ? "#FBBF24" : "#92400E";
            adviceBorder = isDark ? "#FBBF24" : "#F59E0B";
        } else {
            adviceBg = isDark ? "#450A0A" : "#FEE2E2";
            adviceColor = isDark ? "#F87171" : "#991B1B";
            adviceBorder = isDark ? "#F87171" : "#EF4444";
        }
        QLabel *adviceLbl = new QLabel(QString("💡 %1").arg(advice));
        adviceLbl->setStyleSheet(
            QString("QLabel { background-color: %1; color: %2; border: 1px solid %3;"
                    "    border-radius: 8px; padding: 12px; font-size: 12px; font-weight: 500; }")
            .arg(adviceBg, adviceColor, adviceBorder)
        );
        adviceLbl->setWordWrap(true);
        layout->addWidget(adviceLbl);
        
        // ── Storm warning if applicable ──
        if (dw.weatherCode >= 95) {
            QLabel *stormWarn = new QLabel("⛈️ STORM WARNING — Severe weather expected. "
                                          "Driving sessions are strongly discouraged for safety.");
            stormWarn->setStyleSheet(
                QString("QLabel { background-color: %1; color: %2; "
                        "border: 2px solid %2; border-radius: 8px; padding: 12px; "
                        "font-size: 13px; font-weight: bold; }")
                .arg(isDark ? "#450A0A" : "#FEE2E2", isDark ? "#F87171" : "#DC2626")
            );
            stormWarn->setWordWrap(true);
            layout->addWidget(stormWarn);
        }
        
        layout->addStretch();
        
        // ── Close button ──
        QPushButton *closeBtn = new QPushButton("Close");
        closeBtn->setStyleSheet(
            "QPushButton { background-color: #14B8A6; color: white; font-weight: bold; "
            "    border-radius: 8px; padding: 10px; font-size: 13px; border: none; }"
            "QPushButton:hover { background-color: #0D9488; }"
        );
        closeBtn->setCursor(Qt::PointingHandCursor);
        connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::close);
        layout->addWidget(closeBtn);
        
        dialog->exec();
    }
}

void WinoStudentDashboard::showSessionDialog(Session &session)
{
    ThemeManager* theme = ThemeManager::instance();
    bool isDark = theme->currentTheme() == ThemeManager::Dark;
    
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("Session Details");
    dialog->setFixedSize(520, 480);
    dialog->setStyleSheet(
        QString("QDialog {"
        "    background-color: %1;"
        "    border-radius: 12px;"
        "}").arg(theme->cardColor())
    );
    
    QVBoxLayout *dialogLayout = new QVBoxLayout(dialog);
    dialogLayout->setContentsMargins(0, 0, 0, 0);
    dialogLayout->setSpacing(0);
    
    // ── Top Accent Bar ──
    QWidget *accentBar = new QWidget();
    accentBar->setFixedHeight(6);
    accentBar->setStyleSheet("background-color: #14B8A6; border-top-left-radius: 12px; border-top-right-radius: 12px;");
    dialogLayout->addWidget(accentBar);
    
    // ── Content Area ──
    QVBoxLayout *contentLayout = new QVBoxLayout();
    contentLayout->setContentsMargins(30, 25, 30, 25);
    contentLayout->setSpacing(20);
    
    // Title
    QLabel *titleLabel = new QLabel("Session Details");
    titleLabel->setStyleSheet(
        QString("QLabel {"
        "    font-size: 22px;"
        "    font-weight: bold;"
        "    color: %1;"
        "}").arg(theme->primaryTextColor())
    );
    contentLayout->addWidget(titleLabel);
    
    // ── Info Grid ──
    QFrame *infoFrame = new QFrame();
    infoFrame->setStyleSheet(
        QString("QFrame {"
        "    background-color: %1;"
        "    border: 1px solid %2;"
        "    border-radius: 10px;"
        "}").arg(isDark ? "#1E293B" : "#F9FAFB", theme->borderColor())
    );
    
    QGridLayout *infoGrid = new QGridLayout(infoFrame);
    infoGrid->setContentsMargins(20, 18, 20, 18);
    infoGrid->setVerticalSpacing(14);
    infoGrid->setHorizontalSpacing(12);
    
    // Helper lambda for info rows
    auto addInfoRow = [&](int row, const QString& icon, const QString& label, const QString& value) {
        QLabel *iconLbl = new QLabel(icon);
        iconLbl->setStyleSheet("font-size: 16px; border: none; background: transparent;");
        iconLbl->setFixedWidth(24);
        
        QLabel *labelLbl = new QLabel(label);
        labelLbl->setStyleSheet(
            QString("QLabel {"
            "    font-size: 13px;"
            "    font-weight: 600;"
            "    color: %1;"
            "    border: none;"
            "    background: transparent;"
            "}").arg(theme->secondaryTextColor())
        );
        labelLbl->setFixedWidth(90);
        
        QLabel *valueLbl = new QLabel(value);
        valueLbl->setStyleSheet(
            QString("QLabel {"
            "    font-size: 14px;"
            "    font-weight: 500;"
            "    color: %1;"
            "    border: none;"
            "    background: transparent;"
            "}").arg(theme->primaryTextColor())
        );
        
        infoGrid->addWidget(iconLbl, row, 0);
        infoGrid->addWidget(labelLbl, row, 1);
        infoGrid->addWidget(valueLbl, row, 2);
    };
    
    addInfoRow(0, "📋", "Type", session.type);
    if (!session.type.contains("Code", Qt::CaseInsensitive)) {
        addInfoRow(1, "👨‍🏫", "Instructor", session.instructor);
    }
    addInfoRow(2, "📅", "Date", session.date.toString("dddd, MMMM d, yyyy"));
    addInfoRow(3, "⏰", "Time", session.time.toString("hh:mm"));
    addInfoRow(4, "⏱️", "Duration", QString("%1 minutes").arg(session.duration));
    
    // Weather info for this session date
    DayWeather dw = WeatherService::instance()->getWeather(session.date);
    if (dw.weatherCode >= 0) {
        addInfoRow(5, dw.icon, "Weather", 
            QString("%1 | %2°C / %3°C")
            .arg(dw.description)
            .arg(dw.tempMax, 0, 'f', 0)
            .arg(dw.tempMin, 0, 'f', 0)
        );
        
        // Weather driving advice
        QLabel *adviceLabel = new QLabel(WeatherService::weatherCodeToDrivingAdvice(dw.weatherCode));
        int suitability = WeatherService::weatherCodeToSuitability(dw.weatherCode);
        QString adviceBg, adviceColor, adviceBorder;
        if (suitability >= 80) {
            adviceBg = isDark ? "#134E4A" : "#ECFDF5";
            adviceColor = isDark ? "#34D399" : "#065F46";
            adviceBorder = isDark ? "#34D399" : "#10B981";
        } else if (suitability >= 50) {
            adviceBg = isDark ? "#422006" : "#FEF9C3";
            adviceColor = isDark ? "#FBBF24" : "#92400E";
            adviceBorder = isDark ? "#FBBF24" : "#F59E0B";
        } else {
            adviceBg = isDark ? "#450A0A" : "#FEE2E2";
            adviceColor = isDark ? "#F87171" : "#991B1B";
            adviceBorder = isDark ? "#F87171" : "#EF4444";
        }
        adviceLabel->setStyleSheet(
            QString("QLabel { background-color: %1; color: %2; border: 1px solid %3;"
                    "    border-radius: 8px; padding: 10px; font-size: 12px; font-weight: 500; }")
            .arg(adviceBg, adviceColor, adviceBorder)
        );
        adviceLabel->setWordWrap(true);
        contentLayout->addWidget(adviceLabel);
    }
    
    contentLayout->addWidget(infoFrame);
    contentLayout->addStretch();
    
    // ── Separator ──
    QFrame *separator = new QFrame();
    separator->setFrameShape(QFrame::HLine);
    separator->setStyleSheet("background-color: #F3F4F6; border: none;");
    separator->setFixedHeight(1);
    contentLayout->addWidget(separator);
    
    // ── Action Buttons ──
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);
    
    if (session.date < QDate::currentDate()) {
        QLabel *pastBadge = new QLabel("Statut : Séance effectuée");
        pastBadge->setStyleSheet(
            QString("QLabel {"
            "    background-color: %1;"
            "    color: %2;"
            "    border: 1px solid %3;"
            "    border-radius: 8px;"
            "    padding: 10px 16px;"
            "    font-size: 13px;"
            "    font-weight: bold;"
            "}").arg(isDark ? "#1E293B" : "#F3F4F6", 
                     isDark ? "#94A3B8" : "#6B7280", 
                     isDark ? "#334155" : "#E5E7EB")
        );
        pastBadge->setAlignment(Qt::AlignCenter);
        btnLayout->addWidget(pastBadge);
    } else {
        QPushButton *editBtn = new QPushButton("✏️  Edit");
        editBtn->setStyleSheet(
            "QPushButton {"
            "    background-color: #14B8A6;"
            "    color: white;"
            "    font-size: 14px;"
            "    font-weight: bold;"
            "    border: none;"
            "    border-radius: 8px;"
            "    padding: 11px 22px;"
            "}"
            "QPushButton:hover { background-color: #0D9488; }"
        );
        editBtn->setCursor(Qt::PointingHandCursor);
        connect(editBtn, &QPushButton::clicked, [=, &session]() {
            dialog->accept();
            editSession(session);
        });
        
        QPushButton *cancelBtn = new QPushButton("❌  Cancel Session");
        cancelBtn->setStyleSheet(
            "QPushButton {"
            "    background-color: #EF4444;"
            "    color: white;"
            "    font-size: 14px;"
            "    font-weight: bold;"
            "    border: none;"
            "    border-radius: 8px;"
            "    padding: 11px 22px;"
            "}"
            "QPushButton:hover { background-color: #DC2626; }"
        );
        cancelBtn->setCursor(Qt::PointingHandCursor);
        connect(cancelBtn, &QPushButton::clicked, [=, &session]() {
            dialog->accept();
            deleteSession(session.id);
        });
        
        btnLayout->addWidget(editBtn);
        btnLayout->addWidget(cancelBtn);
    }
    
    QPushButton *closeBtn = new QPushButton("Close");
    closeBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #F3F4F6;"
        "    color: #6B7280;"
        "    font-size: 14px;"
        "    font-weight: 600;"
        "    border: none;"
        "    border-radius: 8px;"
        "    padding: 11px 22px;"
        "}"
        "QPushButton:hover { background-color: #E5E7EB; }"
    );
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::reject);
    
    btnLayout->addStretch();
    btnLayout->addWidget(closeBtn);
    
    contentLayout->addLayout(btnLayout);
    dialogLayout->addLayout(contentLayout);
    
    dialog->exec();
}

void WinoStudentDashboard::editSession(Session &session)
{
    ThemeManager* theme = ThemeManager::instance();
    bool isDark = theme->currentTheme() == ThemeManager::Dark;
    
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("Edit Session");
    dialog->setMinimumSize(450, 350);
    dialog->setStyleSheet(QString("QDialog { background-color: %1; }").arg(theme->cardColor()));
    
    QVBoxLayout *dialogLayout = new QVBoxLayout(dialog);
    dialogLayout->setContentsMargins(30, 30, 30, 30);
    dialogLayout->setSpacing(20);
    
    QLabel *titleLabel = new QLabel("✏️ Edit Session");
    titleLabel->setStyleSheet(
        QString("QLabel {"
        "    font-size: 22px;"
        "    font-weight: bold;"
        "    color: %1;"
        "    line-height: 1.4;"
        "    padding: 5px 0;"
        "}").arg(theme->primaryTextColor())
    );
    dialogLayout->addWidget(titleLabel);
    
    QString labelStyle = QString("QLabel { font-size: 14px; color: %1; font-weight: 500; line-height: 1.5; padding: 3px 0; }").arg(theme->primaryTextColor());
    QString inputBg = isDark ? "#1E293B" : "white";
    QString inputColor = theme->primaryTextColor();
    QString inputBorder = theme->borderColor();
    
    // Date
    QLabel *dateLabel = new QLabel("Date:");
    dateLabel->setStyleSheet(labelStyle);
    dialogLayout->addWidget(dateLabel);
    
    QDateEdit *dateEdit = new QDateEdit();
    dateEdit->setDate(session.date);
    dateEdit->setMinimumDate(QDate::currentDate());
    dateEdit->setCalendarPopup(true);
    dateEdit->setStyleSheet(
        QString("QDateEdit {"
        "    padding: 12px 15px;"
        "    border: 2px solid %1;"
        "    border-radius: 8px;"
        "    font-size: 14px;"
        "    background-color: %2;"
        "    color: %3;"
        "    line-height: 1.5;"
        "}"
        "QDateEdit:focus { border-color: #14B8A6; }")
        .arg(inputBorder, inputBg, inputColor)
    );
    dialogLayout->addWidget(dateEdit);
    
    // Time
    QLabel *timeLabel = new QLabel("Heure :");
    timeLabel->setStyleSheet(labelStyle);
    dialogLayout->addWidget(timeLabel);
    
    QComboBox *timeCombo = new QComboBox();
    timeCombo->setStyleSheet(
        QString("QComboBox {"
        "    padding: 12px 15px;"
        "    border: 2px solid %1;"
        "    border-radius: 8px;"
        "    font-size: 14px;"
        "    background-color: %2;"
        "    color: %3;"
        "    line-height: 1.5;"
        "}"
        "QComboBox::drop-down { border: none; width: 30px; }")
        .arg(inputBorder, inputBg, inputColor)
    );
    dialogLayout->addWidget(timeCombo);
    
    dialogLayout->addStretch();
    
    // ── Fetch instructor ID ──
    int instrId = 0;
    QSqlQuery qid;
    qid.prepare("SELECT instructor_id FROM SESSION_BOOKING WHERE session_id = :sid");
    qid.bindValue(":sid", session.id);
    if (qid.exec() && qid.next()) instrId = qid.value(0).toInt();

    // ── Lambda to update timeCombo based on date & logic ──
    auto updateTimes = [=, &session]() {
        timeCombo->clear();
        if (instrId <= 0) {
            timeCombo->addItem("Erreur Instructeur");
            timeCombo->setEnabled(false);
            return;
        }

        QDate d = dateEdit->date();
        int dayOfWeek = d.dayOfWeek();
        QString dayStr;
        switch(dayOfWeek) {
            case 1: dayStr = "MONDAY"; break;
            case 2: dayStr = "TUESDAY"; break;
            case 3: dayStr = "WEDNESDAY"; break;
            case 4: dayStr = "THURSDAY"; break;
            case 5: dayStr = "FRIDAY"; break;
            case 6: dayStr = "SATURDAY"; break;
            case 7: dayStr = "SUNDAY"; break;
        }

        QSqlQuery qAvail;
        qAvail.prepare("SELECT start_time, end_time FROM AVAILABILITY "
                       "WHERE instructor_id = :id AND day_of_week = :d AND is_active = 1");
        qAvail.bindValue(":id", instrId);
        qAvail.bindValue(":d", dayStr);

        QTime availStart, availEnd;
        bool hasAvail = false;
        if (qAvail.exec() && qAvail.next()) {
            availStart = QTime::fromString(qAvail.value(0).toString().left(5), "HH:mm");
            availEnd = QTime::fromString(qAvail.value(1).toString().left(5), "HH:mm");
            hasAvail = true;
        }

        if (!hasAvail || !availStart.isValid() || !availEnd.isValid()) {
            timeCombo->addItem("Indisponible ce jour");
            timeCombo->setEnabled(false);
            return;
        }

        QSet<QString> blockedSlots;
        QSqlQuery qBookings;
        qBookings.prepare("SELECT start_time, end_time FROM SESSION_BOOKING "
                          "WHERE instructor_id = :id AND session_date = :dt "
                          "AND status IN ('CONFIRMED', 'COMPLETED', 'PENDING') AND session_id != :sid");
        qBookings.bindValue(":id", instrId);
        qBookings.bindValue(":dt", d);
        qBookings.bindValue(":sid", session.id);
        if (qBookings.exec()) {
            while (qBookings.next()) {
                QTime start = QTime::fromString(qBookings.value(0).toString().left(5), "HH:mm");
                QTime end = QTime::fromString(qBookings.value(1).toString().left(5), "HH:mm");
                QTime t = start;
                while (t < end && t.isValid()) {
                    blockedSlots.insert(t.toString("HH:mm"));
                    t = t.addSecs(3600); // assume 1 hour blocks
                }
            }
        }

        int durMins = (session.duration > 0) ? session.duration : 60;
        int blocksNeeded = durMins / 60;
        bool foundValid = false;
        QTime t = availStart;

        while (t.addSecs(durMins * 60) <= availEnd) {
            bool valid = true;
            QTime checkT = t;
            for (int i = 0; i < blocksNeeded; ++i) {
                if (blockedSlots.contains(checkT.toString("HH:mm"))) {
                    valid = false;
                    break;
                }
                checkT = checkT.addSecs(3600);
            }
            if (valid) {
                timeCombo->addItem(t.toString("HH:mm"));
                foundValid = true;
            }
            t = t.addSecs(3600);
        }

        if (!foundValid) {
            timeCombo->addItem("Plus de créneaux");
            timeCombo->setEnabled(false);
        } else {
            timeCombo->setEnabled(true);
            int idx = timeCombo->findText(session.time.toString("HH:mm"));
            if (idx >= 0) timeCombo->setCurrentIndex(idx);
        }
    };

    QObject::connect(dateEdit, &QDateEdit::dateChanged, updateTimes);
    updateTimes(); // run immediately

    // Boutons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(12);
    
    QPushButton *saveBtn = new QPushButton("Save Changes");
    saveBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #14B8A6;"
        "    color: white;"
        "    font-size: 14px;"
        "    font-weight: bold;"
        "    border: none;"
        "    border-radius: 8px;"
        "    padding: 12px 30px;"
        "    line-height: 1.5;"
        "}"
        "QPushButton:hover { background-color: #0F9D8E; }"
    );
    saveBtn->setCursor(Qt::PointingHandCursor);
    connect(saveBtn, &QPushButton::clicked, [=, &session]() {
        if (!timeCombo->isEnabled() || timeCombo->count() == 0 || timeCombo->currentText().contains("disp") || timeCombo->currentText().contains("créneaux")) {
            QMessageBox::warning(dialog, "Erreur", "Séléction de l'heure invalide.");
            return;
        }

        QDate newDate = dateEdit->date();
        QTime newTime = QTime::fromString(timeCombo->currentText(), "HH:mm");
        int durMins = (session.duration > 0) ? session.duration : 60;
        QTime newEnd = newTime.addSecs(durMins * 60);
        
        // Update DB
        QSqlQuery qUpdate;
        qUpdate.prepare("UPDATE SESSION_BOOKING SET session_date = :dt, start_time = :st, end_time = :et WHERE session_id = :sid");
        qUpdate.bindValue(":dt", newDate);
        qUpdate.bindValue(":st", newTime.toString("HH:mm"));
        qUpdate.bindValue(":et", newEnd.toString("HH:mm"));
        qUpdate.bindValue(":sid", session.id);
        
        if (!qUpdate.exec()) {
            QMessageBox::critical(dialog, "Erreur DB",
                "Impossible de mettre à jour la séance : " + qUpdate.lastError().text());
            return;
        }
        
        // Update in-memory
        session.date = newDate;
        session.time = newTime;
        
        updateCalendarHighlights();
        QMessageBox::information(dialog, "Succès", "Séance mise à jour !");
        dialog->accept();
    });
    
    QPushButton *cancelBtn = new QPushButton("Cancel");
    cancelBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #F3F4F6;"
        "    color: #6B7280;"
        "    font-size: 14px;"
        "    font-weight: bold;"
        "    border: none;"
        "    border-radius: 8px;"
        "    padding: 12px 30px;"
        "    line-height: 1.5;"
        "}"
        "QPushButton:hover { background-color: #E5E7EB; }"
    );
    cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(cancelBtn, &QPushButton::clicked, dialog, &QDialog::reject);
    
    btnLayout->addWidget(saveBtn);
    btnLayout->addWidget(cancelBtn);
    
    dialogLayout->addLayout(btnLayout);
    
    dialog->exec();
}

void WinoStudentDashboard::deleteSession(int sessionId)
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Annuler la séance",
                                  "Êtes-vous sûr de vouloir annuler cette séance ?",
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        // 1. Update DB — set status to CANCELLED
        QSqlQuery qCancel;
        qCancel.prepare("UPDATE SESSION_BOOKING SET status = 'CANCELLED' WHERE session_id = :sid");
        qCancel.bindValue(":sid", sessionId);

        if (!qCancel.exec()) {
            QMessageBox::critical(this, "Erreur DB",
                "Impossible d'annuler la séance : " + qCancel.lastError().text());
            return;
        }

        // 2. Remove from memory list
        for (int i = 0; i < sessions.size(); i++) {
            if (sessions[i].id == sessionId) {
                sessions.removeAt(i);
                break;
            }
        }

        // 3. Refresh calendar
        calendarWidget->setDateTextFormat(QDate(), QTextCharFormat());
        updateCalendarHighlights();

        QMessageBox::information(this, "Succès",
            "Séance annulée. Le montant de cette séance sera retiré de votre crédit.");
    }
}

QWidget* WinoStudentDashboard::createBalanceCardWithPayment()
{
    ThemeManager* theme = ThemeManager::instance();
    bool isDark = theme->currentTheme() == ThemeManager::Dark;

    QFrame *card = new QFrame();
    card->setObjectName("balanceCard");
    card->setStyleSheet(
        QString("QFrame#balanceCard {"
        "    background-color: %1;"
        "    border-radius: 12px;"
        "}").arg(theme->cardColor())
    );
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(20);
    shadow->setXOffset(0);
    shadow->setYOffset(4);
    shadow->setColor(QColor(0, 0, 0, 15));
    card->setGraphicsEffect(shadow);

    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(25, 25, 25, 25);
    layout->setSpacing(15);

    QLabel *titleLabel = new QLabel("Account Balance");
    titleLabel->setObjectName("balanceTitle");
    titleLabel->setStyleSheet(
        QString("QLabel#balanceTitle { color: %1; font-size: 13px; font-weight: 500; line-height: 1.5; padding: 3px 0; }")
        .arg(theme->secondaryTextColor())
    );
    layout->addWidget(titleLabel);

    // ── Fetch real credit from DB ──────────────────────────────
    // Credit = total session cost for sessions whose date has passed (not yet fully paid)
    int studentId = qApp->property("currentUserId").toInt();
    if (studentId == 0) return new QWidget();

    double totalDue = 0.0;
    QSqlQuery qCredit;
    qCredit.prepare(
        "SELECT COALESCE(SUM(AMOUNT), 0) FROM SESSION_BOOKING "
        "WHERE STUDENT_ID = :sid AND STATUS IN ('CONFIRMED', 'COMPLETED')");
    qCredit.bindValue(":sid", studentId);
    if (qCredit.exec() && qCredit.next()) {
        totalDue = qCredit.value(0).toDouble();
    }

    QSqlQuery qExamFee;
    qExamFee.prepare(
        "SELECT COALESCE(SUM(AMOUNT), 0) FROM EXAM_REQUEST "
        "WHERE STUDENT_ID = :sid AND STATUS IN ('APPROVED', 'COMPLETED')");
    qExamFee.bindValue(":sid", studentId);
    if (qExamFee.exec() && qExamFee.next()) {
        totalDue += qExamFee.value(0).toDouble();
    }

    double totalPaid = 0.0;
    QSqlQuery qPaid;
    qPaid.prepare(
        "SELECT COALESCE(SUM(AMOUNT), 0) FROM PAYMENT "
        "WHERE STUDENT_ID = :sid AND STATUS IN ('CONFIRMED', 'PENDING', 'PAID', 'VALID', 'SUCCESS', 'ACCEPTED', 'VERIFIED', 'COMPLETED')");
    qPaid.bindValue(":sid", studentId);
    if (qPaid.exec() && qPaid.next())
        totalPaid = qPaid.value(0).toDouble();

    double credit = totalDue - totalPaid; // outstanding balance (positive = student owes money)
    // ───────────────────────────────────────────────────────────

    // credit > 0  → student owes money   → red,  show as "- X TND"
    // credit < 0  → student has credit   → green, show as "+ X TND"
    // credit == 0 → fully settled        → green, show as "0 TND"
    QString creditText;
    if (qAbs(credit) < 0.01)
        creditText = "0 TND";
    else if (credit > 0)
        creditText = QString("- %1 TND").arg(QString::number(credit, 'f', 0));
    else
        creditText = QString("+ %1 TND").arg(QString::number(-credit, 'f', 0));

    QLabel *balanceLabel = new QLabel(creditText);
    balanceLabel->setObjectName("balanceValue");
    QString balanceColor = (credit > 0.01) ? (isDark ? "#F87171" : "#DC2626") : (isDark ? "#34D399" : "#059669");
    balanceLabel->setStyleSheet(
        QString("QLabel#balanceValue { color: %1; font-size: 36px; font-weight: bold; line-height: 1.3; padding: 5px 0; }")
        .arg(balanceColor)
    );
    layout->addWidget(balanceLabel);
    
    // ── Pre-create all status components for dynamic updates ────────────────
    
    // Warning label
    QLabel *warningLabel = new QLabel("");
    warningLabel->setObjectName("balanceWarningLabel");
    warningLabel->setWordWrap(true);
    layout->addWidget(warningLabel);

    // D17 Pay button
    QPushButton *payBtn = new QPushButton("💳 Pay with D17");
    payBtn->setObjectName("payWithD17Btn");
    payBtn->setMinimumWidth(200);
    payBtn->setMinimumHeight(44); // Reduced slightly for better fit
    payBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #14B8A6;"
        "    color: #FFFFFF;"
        "    font-size: 15px;"
        "    font-weight: bold;"
        "    border: none;"
        "    border-radius: 8px;"
        "    padding: 10px 24px;"
        "    margin-top: 5px;"
        "}"
        "QPushButton:hover { background-color: #0F9D8E; }"
    );
    payBtn->setCursor(Qt::PointingHandCursor);
    connect(payBtn, &QPushButton::clicked, this, &WinoStudentDashboard::onPayWithD17Clicked);
    layout->addWidget(payBtn);

    // OK Label
    QLabel *okLabel = new QLabel("✅ Aucun crédit en attente");
    okLabel->setObjectName("balanceOkLabel");
    layout->addWidget(okLabel);

    // ── Initial Visibility & Content State ─────────────────────────────────
    if (credit > 0) {
        okLabel->hide();
        QString warnText = (credit >= 200)
            ? "⚠️ Credit élevé! Paiement requis avant nouvelle réservation."
            : QString("⚠️ Vous avez un crédit de %1 TND.").arg(credit, 0, 'f', 0);
        
        warningLabel->setText(warnText);
        QString warnBg    = isDark ? "#451a1a" : "#FEE2E2";
        QString warnColor = isDark ? "#F87171" : "#DC2626";
        warningLabel->setStyleSheet(
            QString("QLabel { color: %1; font-size: 12px; font-weight: 500; "
                    "background-color: %2; padding: 8px 12px; border-radius: 6px; }")
            .arg(warnColor, warnBg));
        warningLabel->show();
        payBtn->show();
    } else {
        warningLabel->hide();
        payBtn->hide();
        okLabel->setStyleSheet(
            QString("QLabel { color: %1; font-size: 13px; font-weight: 500;"
                    " background-color: %2; padding: 8px 12px; border-radius: 6px; }")
            .arg(isDark ? "#34D399" : "#059669", isDark ? "#134E4A" : "#ECFDF5"));
        okLabel->show();
    }

    // History button — always shown
    QPushButton *histBtn = new QPushButton("📋  Voir l'historique de paiement");
    histBtn->setMinimumWidth(200);
    histBtn->setMinimumHeight(44);
    histBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: transparent;"
        "    color: #0D9488;"
        "    font-size: 14px;"
        "    font-weight: bold;"
        "    border: 2px solid #0D9488;"
        "    border-radius: 8px;"
        "    padding: 8px 24px;"
        "}"
        "QPushButton:hover { background-color: #F0FDFA; }"
    );
    histBtn->setCursor(Qt::PointingHandCursor);
    connect(histBtn, &QPushButton::clicked, this, &WinoStudentDashboard::onPaymentHistoryClicked);
    layout->addWidget(histBtn);

    layout->addStretch();
    return card;
}

void WinoStudentDashboard::updateBalance()
{
    QWidget* balanceLabelWidget = centralWidget->findChild<QWidget*>("balanceValue");
    if (!balanceLabelWidget) return;
    QLabel* balanceLabel = qobject_cast<QLabel*>(balanceLabelWidget);
    if (!balanceLabel) return;
    
    ThemeManager* theme = ThemeManager::instance();
    bool isDark = theme->currentTheme() == ThemeManager::Dark;

    int studentId = qApp->property("currentUserId").toInt();
    if (studentId == 0) return;

    double totalDue = 0.0;
    QSqlQuery qCredit;
    qCredit.prepare(
        "SELECT COALESCE(SUM(AMOUNT), 0) FROM SESSION_BOOKING "
        "WHERE STUDENT_ID = :sid AND STATUS IN ('CONFIRMED', 'COMPLETED')");
    qCredit.bindValue(":sid", studentId);
    if (qCredit.exec() && qCredit.next()) {
        totalDue = qCredit.value(0).toDouble();
    }

    QSqlQuery qExamFee;
    qExamFee.prepare(
        "SELECT COALESCE(SUM(AMOUNT), 0) FROM EXAM_REQUEST "
        "WHERE STUDENT_ID = :sid AND STATUS IN ('APPROVED', 'COMPLETED')");
    qExamFee.bindValue(":sid", studentId);
    if (qExamFee.exec() && qExamFee.next()) {
        totalDue += qExamFee.value(0).toDouble();
    }

    double totalPaid = 0.0;
    QSqlQuery qPaid;
    qPaid.prepare(
        "SELECT COALESCE(SUM(AMOUNT), 0) FROM PAYMENT "
        "WHERE STUDENT_ID = :sid AND STATUS IN ('CONFIRMED', 'PENDING', 'PAID', 'VALID', 'SUCCESS', 'ACCEPTED', 'VERIFIED', 'COMPLETED')");
    qPaid.bindValue(":sid", studentId);
    if (qPaid.exec() && qPaid.next())
        totalPaid = qPaid.value(0).toDouble();

    double credit = totalDue - totalPaid;

    QString creditText = QString("%1 TND").arg(credit < 0 ? "+"+QString::number(-credit,'f',0) : "-"+QString::number(credit,'f',0));
    QString balanceColor = (credit > 0) ? (isDark ? "#F87171" : "#DC2626") : (isDark ? "#34D399" : "#059669");
    balanceLabel->setText(creditText);
    balanceLabel->setStyleSheet(
        QString("QLabel#balanceValue { color: %1; font-size: 36px; font-weight: bold; line-height: 1.3; padding: 5px 0; }")
        .arg(balanceColor)
    );

    // ── Update Dynamic Visibility ──
    QLabel *warningLabel = centralWidget->findChild<QLabel*>("balanceWarningLabel");
    QPushButton *payBtn  = centralWidget->findChild<QPushButton*>("payWithD17Btn");
    QLabel *okLabel      = centralWidget->findChild<QLabel*>("balanceOkLabel");

    if (warningLabel && payBtn && okLabel) {
        if (credit > 0) {
            okLabel->hide();
            QString warnText = (credit >= 200)
                ? "⚠️ Credit élevé! Paiement requis avant nouvelle réservation."
                : QString("⚠️ Vous avez un crédit de %1 TND.").arg(credit, 0, 'f', 0);
            
            warningLabel->setText(warnText);
            QString warnBg    = isDark ? "#451a1a" : "#FEE2E2";
            QString warnColor = isDark ? "#F87171" : "#DC2626";
            warningLabel->setStyleSheet(
                QString("QLabel { color: %1; font-size: 12px; font-weight: 500; "
                        "background-color: %2; padding: 8px 12px; border-radius: 6px; }")
                .arg(warnColor, warnBg));
            warningLabel->show();
            payBtn->show();
        } else {
            warningLabel->hide();
            payBtn->hide();
            okLabel->setStyleSheet(
                QString("QLabel { color: %1; font-size: 13px; font-weight: 500;"
                        " background-color: %2; padding: 8px 12px; border-radius: 6px; }")
                .arg(isDark ? "#34D399" : "#059669", isDark ? "#134E4A" : "#ECFDF5"));
            okLabel->show();
        }
    }
}


void WinoStudentDashboard::onPayWithD17Clicked()
{
    ThemeManager* theme = ThemeManager::instance();
    bool isDark = theme->currentTheme() == ThemeManager::Dark;

    // ── Fetch real data from DB ────────────────────────────────
    int studentId = qApp->property("currentUserId").toInt();
    if (studentId == 0) return;

    double totalDue = 0.0;
    QSqlQuery qCredit;
    qCredit.prepare(
        "SELECT COALESCE(SUM(AMOUNT), 0) FROM SESSION_BOOKING "
        "WHERE STUDENT_ID = :sid AND STATUS IN ('CONFIRMED', 'COMPLETED')");
    qCredit.bindValue(":sid", studentId);
    if (qCredit.exec() && qCredit.next()) {
        totalDue = qCredit.value(0).toDouble();
    }

    QSqlQuery qExamFee;
    qExamFee.prepare(
        "SELECT COALESCE(SUM(AMOUNT), 0) FROM EXAM_REQUEST "
        "WHERE STUDENT_ID = :sid AND STATUS IN ('APPROVED', 'COMPLETED')");
    qExamFee.bindValue(":sid", studentId);
    if (qExamFee.exec() && qExamFee.next()) {
        totalDue += qExamFee.value(0).toDouble();
    }

    double totalPaid = 0.0;
    QSqlQuery qPaid;
    qPaid.prepare(
        "SELECT COALESCE(SUM(AMOUNT), 0) FROM PAYMENT "
        "WHERE STUDENT_ID = :sid AND STATUS IN ('CONFIRMED', 'PENDING', 'PAID', 'VALID', 'SUCCESS', 'ACCEPTED', 'VERIFIED', 'COMPLETED')");
    qPaid.bindValue(":sid", studentId);
    if (qPaid.exec() && qPaid.next())
        totalPaid = qPaid.value(0).toDouble();

    double credit = totalDue - totalPaid;
    if (credit <= 0) {
        QMessageBox::information(this, "No Balance", "You have no outstanding credit to pay!");
        return;
    }

    // Amount student must pay so credit drops below or equal to 200 DT
    double minPayment = 10.0;
    if (credit > 200) {
        minPayment = credit - 200.0;
    }

    // Fetch instructor's D17 ID from DB
    QString d17Id = "Non configuré"; // fallback
    QSqlQuery qD17;
    qD17.prepare("SELECT i.d17_id FROM INSTRUCTORS i "
                 "JOIN STUDENTS s ON s.instructor_id = i.id "
                 "WHERE s.id = :sid");
    qD17.bindValue(":sid", studentId);
    if (qD17.exec() && qD17.next() && !qD17.value(0).isNull()) {
        QString fetched = qD17.value(0).toString().trimmed();
        if (!fetched.isEmpty()) {
            d17Id = fetched;
        }
    }
    // ── End DB fetch ──────────────────────────────────────────

    QDialog *paymentDialog = new QDialog(this);
    paymentDialog->setWindowTitle("Pay with D17");
    paymentDialog->setMinimumSize(500, 440);
    paymentDialog->setStyleSheet(QString("QDialog { background-color: %1; }").arg(theme->cardColor()));

    QVBoxLayout *dialogLayout = new QVBoxLayout(paymentDialog);
    dialogLayout->setContentsMargins(30, 30, 30, 30);
    dialogLayout->setSpacing(20);

    QLabel *titleLabel = new QLabel("💳 Pay with D17");
    titleLabel->setStyleSheet(
        QString("QLabel { font-size: 24px; font-weight: bold; color: %1; line-height: 1.4; padding: 5px 0; }")
        .arg(theme->primaryTextColor()));
    dialogLayout->addWidget(titleLabel);

    // Credit summary
    QLabel *creditSummary = new QLabel(
        QString("Crédit total: %1 TND\nMontant minimum à payer: %2 TND")
        .arg(credit, 0, 'f', 0).arg(minPayment, 0, 'f', 0));
    creditSummary->setStyleSheet(
        QString("QLabel { font-size: 15px; color: %1; font-weight: 500; line-height: 1.7; "
        "background-color: %2; padding: 12px 16px; border-radius: 8px; }")
        .arg(isDark ? "#F87171" : "#DC2626", isDark ? "#451a1a" : "#FEE2E2"));
    dialogLayout->addWidget(creditSummary);

    // Instructor D17 ID box
    QFrame *idFrame = new QFrame();
    idFrame->setStyleSheet(
        QString("QFrame { background-color: %1; border: 1px solid %2; border-radius: 10px; }")
        .arg(isDark ? "#1E293B" : "#F9FAFB", theme->borderColor()));

    QVBoxLayout *idFrameLayout = new QVBoxLayout(idFrame);
    idFrameLayout->setContentsMargins(20, 15, 20, 15);
    idFrameLayout->setSpacing(10);

    QLabel *idTitleLabel = new QLabel("Instructor D17 ID:");
    idTitleLabel->setStyleSheet(
        QString("QLabel { font-size: 13px; color: %1; font-weight: 500; line-height: 1.5; padding: 3px 0; }")
        .arg(theme->secondaryTextColor()));
    idFrameLayout->addWidget(idTitleLabel);

    QHBoxLayout *idCopyLayout = new QHBoxLayout();
    idCopyLayout->setSpacing(10);

    QLabel *idValueLabel = new QLabel(d17Id);
    idValueLabel->setStyleSheet(
        QString("QLabel { font-size: 22px; color: %1; font-weight: bold; font-family: 'Courier New', monospace; line-height: 1.4; padding: 5px 0; }")
        .arg(theme->primaryTextColor()));
    idValueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    idCopyLayout->addWidget(idValueLabel);

    QPushButton *copyBtn = new QPushButton("📋 Copy");
    copyBtn->setStyleSheet(
        "QPushButton { background-color: #14B8A6; color: white; font-size: 12px; font-weight: bold;"
        " border: none; border-radius: 6px; padding: 8px 16px; line-height: 1.5; }"
        "QPushButton:hover { background-color: #0F9D8E; }");
    copyBtn->setCursor(Qt::PointingHandCursor);
    connect(copyBtn, &QPushButton::clicked, [=]() {
        QApplication::clipboard()->setText(d17Id);
        copyBtn->setText("✓ Copied!");
        QTimer::singleShot(2000, [=]() { copyBtn->setText("📋 Copy"); });
    });
    idCopyLayout->addWidget(copyBtn);
    idCopyLayout->addStretch();

    idFrameLayout->addLayout(idCopyLayout);
    dialogLayout->addWidget(idFrame);

    // Transaction code input
    QLabel *codeLabel = new QLabel("Entrez votre code de transaction D17:");
    codeLabel->setStyleSheet(
        QString("QLabel { font-size: 14px; color: %1; font-weight: 500; line-height: 1.5; padding: 3px 0; }")
        .arg(theme->primaryTextColor()));
    dialogLayout->addWidget(codeLabel);

    QLineEdit *codeInput = new QLineEdit();
    codeInput->setPlaceholderText("D17XXXXXX...");
    codeInput->setStyleSheet(
        QString("QLineEdit { background-color: %1; border: 2px solid %2;"
        " border-radius: 8px; padding: 12px 16px; font-size: 14px; color: %3; }"
        "QLineEdit:focus { border-color: #14B8A6; }")
        .arg(isDark ? "#1E293B" : "white", theme->borderColor(), theme->primaryTextColor()));
    dialogLayout->addWidget(codeInput);

    // Amount to pay input
    QLabel *amtLabel = new QLabel(QString("Montant envoyé (min %1 TND):").arg(minPayment, 0, 'f', 0));
    amtLabel->setStyleSheet(
        QString("QLabel { font-size: 14px; color: %1; font-weight: 500; line-height: 1.5; padding: 3px 0; }")
        .arg(theme->primaryTextColor()));
    dialogLayout->addWidget(amtLabel);

    QLineEdit *amtInput = new QLineEdit();
    amtInput->setPlaceholderText(QString("%1").arg(minPayment, 0, 'f', 0));
    amtInput->setText(QString("%1").arg(minPayment, 0, 'f', 0));
    amtInput->setStyleSheet(codeInput->styleSheet());
    dialogLayout->addWidget(amtInput);

    QLabel *infoLabel = new QLabel("ℹ️ Envoyez le montant via D17, puis entrez le code de transaction ci-dessus.");
    infoLabel->setStyleSheet(
        "QLabel { font-size: 12px; color: #6B7280; padding: 10px;"
        " background-color: #F9FAFB; border-radius: 6px; line-height: 1.6; }");
    infoLabel->setWordWrap(true);
    dialogLayout->addWidget(infoLabel);

    dialogLayout->addStretch();

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(15);

    QPushButton *cancelBtn = new QPushButton("Cancel");
    cancelBtn->setStyleSheet(
        "QPushButton { background-color: #F3F4F6; color: #6B7280; font-size: 14px;"
        " font-weight: bold; border: none; border-radius: 8px; padding: 12px 30px; }"
        "QPushButton:hover { background-color: #E5E7EB; }");
    cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(cancelBtn, &QPushButton::clicked, paymentDialog, &QDialog::reject);

    QPushButton *submitBtn = new QPushButton("Submit Code");
    submitBtn->setStyleSheet(
        "QPushButton { background-color: #14B8A6; color: white; font-size: 14px;"
        " font-weight: bold; border: none; border-radius: 8px; padding: 12px 30px; }"
        "QPushButton:hover { background-color: #0F9D8E; }"
        "QPushButton:pressed { background-color: #0D9488; }");
    submitBtn->setCursor(Qt::PointingHandCursor);

    connect(submitBtn, &QPushButton::clicked, [=]() mutable {
        QString code = codeInput->text().trimmed();
        if (code.isEmpty()) {
            QMessageBox warnBox(paymentDialog);
            warnBox.setWindowTitle("Code manquant");
            warnBox.setText("Veuillez entrer votre code de transaction D17.");
            warnBox.setIcon(QMessageBox::Warning);
            warnBox.setStyleSheet("QMessageBox { background-color: white; } QLabel { color: black; font-size: 14px; } QPushButton { background-color: #14B8A6; color: white; font-weight: bold; padding: 6px 16px; border-radius: 4px; }");
            warnBox.exec();
            return;
        }
        double paidAmount = amtInput->text().toDouble();
        if (paidAmount < minPayment) {
            QMessageBox warnBox(paymentDialog);
            warnBox.setWindowTitle("Montant insuffisant");
            warnBox.setText(QString("Veuillez saisir un montant d'au moins %1 TND pour que votre crédit ne dépasse pas 200 TND.").arg(minPayment));
            warnBox.setIcon(QMessageBox::Warning);
            warnBox.setStyleSheet("QMessageBox { background-color: white; } QLabel { color: black; font-size: 14px; } QPushButton { background-color: #14B8A6; color: white; font-weight: bold; padding: 6px 16px; border-radius: 4px; }");
            warnBox.exec();
            return;
        }

        // Insert PAYMENT record with PENDING status
        QSqlQuery insertPay;
        insertPay.prepare(
            "INSERT INTO PAYMENT (STUDENT_ID, AMOUNT, PAYMENT_METHOD, D17_CODE, STATUS) "
            "VALUES (:sid, :amt, 'D17', :code, 'PENDING')");
        insertPay.bindValue(":sid", studentId);
        insertPay.bindValue(":amt", paidAmount);
        insertPay.bindValue(":code", code);

        if (!insertPay.exec()) {
            QMessageBox::critical(paymentDialog, "Erreur DB",
                "Impossible d'enregistrer le paiement: " + insertPay.lastError().text());
            return;
        }

        QMessageBox successBox(paymentDialog);
        successBox.setWindowTitle("Succès");
        successBox.setText("✓ Code envoyé avec succès!");
        successBox.setInformativeText(
            QString("Votre paiement de %1 TND est en attente de validation par l'instructeur.\n"
            "Une fois confirmé, votre crédit sera mis à jour.")
            .arg(paidAmount, 0, 'f', 0));
        successBox.setIcon(QMessageBox::Information);
        successBox.setStyleSheet(
            "QMessageBox { background-color: white; }"
            "QLabel { color: #111827; font-size: 14px; line-height: 1.5; }"
            "QPushButton { background-color: #14B8A6; color: white; font-weight: bold;"
            " border: none; border-radius: 6px; padding: 8px 20px; min-width: 80px; }");
        successBox.exec();

        paymentDialog->accept();
    });

    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(submitBtn);
    dialogLayout->addLayout(btnLayout);

    paymentDialog->exec();
}

void WinoStudentDashboard::onBackClicked()
{
    // Embedded in StudentLearningHub — hide only
    this->hide();
}

void WinoStudentDashboard::onExamRequestClicked()
{
    // Refresh exam page data and navigate to it
    refreshExamPage();
    m_mainStack->setCurrentIndex(2);
}

void WinoStudentDashboard::showExamRequestDialog(int examStep)
{
    ThemeManager* theme = ThemeManager::instance();
    bool isDark = theme->currentTheme() == ThemeManager::Dark;

    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("Request Exam");
    dialog->setObjectName("requestExamDialog");
    dialog->setMinimumWidth(450);
    dialog->setStyleSheet(
        QString("QDialog#requestExamDialog { background-color: %1; border-radius: 12px; }").arg(theme->cardColor())
    );

    QVBoxLayout *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(20);

    QString stepName = (examStep == 1) ? "Code" : (examStep == 2) ? "Circuit" : "Parking";
    
    QLabel *title = new QLabel(QString("Request %1 Exam").arg(stepName));
    title->setStyleSheet(
        QString("QLabel { color: %1; font-size: 22px; font-weight: bold; }").arg(theme->primaryTextColor())
    );
    layout->addWidget(title);

    QLabel *desc = new QLabel("Please specify your preferences for the exam timing. Once submitted, your instructor will review and set an exact date/time.");
    desc->setWordWrap(true);
    desc->setStyleSheet(QString("QLabel { color: %1; font-size: 14px; }").arg(theme->secondaryTextColor()));
    layout->addWidget(desc);

    // Timeframe
    QLabel *timeLabel = new QLabel("Preferred Timeframe:");
    timeLabel->setStyleSheet(QString("QLabel { color: %1; font-weight: bold; font-size: 14px; }").arg(theme->primaryTextColor()));
    layout->addWidget(timeLabel);

    QComboBox *timeCombo = new QComboBox();
    timeCombo->addItems({"This Week", "This Month", "Next Month"});
    QString inputStyle = QString(
        "QComboBox, QLineEdit {"
        "    background-color: %1;"
        "    color: %2;"
        "    border: 1px solid %3;"
        "    border-radius: 6px;"
        "    padding: 10px;"
        "    font-size: 14px;"
        "}"
    ).arg(isDark ? "#374151" : "#F3F4F6", theme->primaryTextColor(), theme->borderColor());
    timeCombo->setStyleSheet(inputStyle);
    layout->addWidget(timeCombo);

    // Unavailable dates
    QLabel *unavailLabel = new QLabel("Unavailable Dates (Optional):");
    unavailLabel->setStyleSheet(QString("QLabel { color: %1; font-weight: bold; font-size: 14px; }").arg(theme->primaryTextColor()));
    layout->addWidget(unavailLabel);

    QLineEdit *unavailInput = new QLineEdit();
    unavailInput->setPlaceholderText("e.g. 'Mondays', '24/03', 'April 5th'");
    unavailInput->setStyleSheet(inputStyle);
    layout->addWidget(unavailInput);

    // Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    
    QPushButton *cancelBtn = new QPushButton("Cancel");
    cancelBtn->setStyleSheet(
        QString("QPushButton {"
        "    background-color: transparent;"
        "    color: %1;"
        "    font-weight: bold;"
        "    padding: 10px 20px;"
        "    border-radius: 6px;"
        "}"
        "QPushButton:hover { background-color: %2; }").arg(theme->secondaryTextColor(), isDark ? "#374151" : "#F3F4F6")
    );
    connect(cancelBtn, &QPushButton::clicked, dialog, &QDialog::reject);
    
    QPushButton *submitBtn = new QPushButton("Submit Request");
    submitBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #14B8A6;"
        "    color: white;"
        "    font-weight: bold;"
        "    padding: 10px 20px;"
        "    border-radius: 6px;"
        "}"
        "QPushButton:hover { background-color: #0F9D8E; }"
    );
    
    connect(submitBtn, &QPushButton::clicked, [=]() {
        int studentId = qApp->property("currentUserId").toInt();
        if (studentId == 0) return;

        // Fetch user's natively assigned instructor
        int instructorId = 1; // Fallback
        QSqlQuery qInst;
        qInst.prepare("SELECT INSTRUCTOR_ID FROM STUDENTS WHERE ID = :sid");
        qInst.bindValue(":sid", studentId);
        if (qInst.exec() && qInst.next()) {
            instructorId = qInst.value(0).toInt();
        }

        QString timeframe = timeCombo->currentText();
        QString unavailable = unavailInput->text().trimmed();
        QString comments = QString("Timeframe: %1").arg(timeframe);
        if (!unavailable.isEmpty()) {
            comments += QString(" | Unavailable: %1").arg(unavailable);
        }

        QSqlQuery insertQuery;
        insertQuery.prepare("INSERT INTO EXAM_REQUEST (STUDENT_ID, INSTRUCTOR_ID, EXAM_STEP, REQUESTED_DATE, AMOUNT, STATUS, COMMENTS, PASSED) "
                            "VALUES (:student_id, :instructor_id, :exam_step, SYSDATE, 50.0, 'PENDING', :comments, 0)");
        insertQuery.bindValue(":student_id", studentId);
        insertQuery.bindValue(":instructor_id", instructorId);
        insertQuery.bindValue(":exam_step", examStep);
        insertQuery.bindValue(":comments", comments);

        if (insertQuery.exec()) {
            QMessageBox::information(this, "Success", "Your exam request has been successfully sent to your instructor!");
            dialog->accept();
        } else {
            QMessageBox::warning(this, "Database Error", "Failed to submit request: " + insertQuery.lastError().text());
        }
    });

    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(submitBtn);
    layout->addLayout(btnLayout);

    dialog->exec();
}

// ═════════════════════════════════════════════════════════════════════════════
//  EXAM PAGE  (page index 2 in m_mainStack)
//  Clean, professional, connected to Oracle DB
// ═════════════════════════════════════════════════════════════════════════════

QWidget* WinoStudentDashboard::createExamPage()
{
    QWidget *page = new QWidget();
    page->setObjectName("examPage");

    QVBoxLayout *root = new QVBoxLayout(page);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Top bar ─────────────────────────────────────────────────────────────
    m_examTopBar = new QWidget(page);
    m_examTopBar->setFixedHeight(56);
    m_examTopBar->setObjectName("examTopBar");

    QHBoxLayout *topL = new QHBoxLayout(m_examTopBar);
    topL->setContentsMargins(20, 0, 20, 0);

    QPushButton *backBtn = new QPushButton(QString::fromUtf8("\xe2\x86\x90  Sessions"), m_examTopBar);
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setObjectName("examBackBtn");
    connect(backBtn, &QPushButton::clicked, this, [this](){
        m_mainStack->setCurrentIndex(0);
        updateCalendarHighlights();
        updateBalance();
    });
    topL->addWidget(backBtn);
    topL->addStretch();

    QLabel *pageTitle = new QLabel(QString::fromUtf8("\xf0\x9f\x8e\x93  Examen"), m_examTopBar);
    pageTitle->setObjectName("examPageTitle");
    pageTitle->setStyleSheet("font-size:15px;font-weight:700;");
    topL->addWidget(pageTitle);
    topL->addStretch();

    QWidget *spacer = new QWidget(m_examTopBar);
    spacer->setFixedWidth(80);
    topL->addWidget(spacer);

    root->addWidget(m_examTopBar);

    // ── Scrollable body ─────────────────────────────────────────────────────
    m_examScroll = new QScrollArea(page);
    m_examScroll->setWidgetResizable(true);
    m_examScroll->setFrameShape(QFrame::NoFrame);

    m_examBodyWidget = new QWidget();
    m_examBodyLayout = new QVBoxLayout(m_examBodyWidget);
    m_examBodyLayout->setContentsMargins(32, 28, 32, 40);
    m_examBodyLayout->setSpacing(20);

    m_examScroll->setWidget(m_examBodyWidget);
    root->addWidget(m_examScroll, 1);

    // Apply initial theme
    refreshExamPage();

    // Rebuild content + styles when theme changes
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) {
        refreshExamPage();
    });

    return page;
}

// Helper that (re-)populates the exam page body with fresh DB data
void WinoStudentDashboard::refreshExamPage()
{
    if (!m_examBodyLayout) return;

    // ── Fetch current theme colors ───────────────────────────────────────────
    ThemeManager* tm = ThemeManager::instance();
    bool isDark       = tm->currentTheme() == ThemeManager::Dark;
    QString C_BG      = tm->backgroundColor();
    QString C_CARD    = tm->cardColor();
    QString C_SURFACE = tm->surfaceColor();
    QString C_TEXT    = tm->primaryTextColor();
    QString C_MUTED   = tm->secondaryTextColor();
    QString C_BORDER  = tm->borderColor();
    QString C_TEAL    = tm->accentColor();

    // ── Update shell styles (top bar + scroll area) ──────────────────────────
    if (m_examTopBar) {
        bool dark = isDark;
        m_examTopBar->setStyleSheet(QString(
            "QWidget#examTopBar{background:%1;border-bottom:1px solid %2;}")
            .arg(tm->headerColor(), C_BORDER));
        // Update back button and title inside top bar
        auto backBtns = m_examTopBar->findChildren<QPushButton*>("examBackBtn");
        for (auto *b : backBtns)
            b->setStyleSheet(QString(
                "QPushButton{background:transparent;color:%1;font-size:13px;border:none;padding:0 4px;}"
                "QPushButton:hover{color:%2;}").arg(C_MUTED, C_TEXT));
        auto titles = m_examTopBar->findChildren<QLabel*>("examPageTitle");
        for (auto *l : titles)
            l->setStyleSheet(QString("color:%1;font-size:15px;font-weight:700;").arg(C_TEXT));
    }
    if (m_examScroll) {
        m_examScroll->setStyleSheet(QString(
            "QScrollArea{background:%1;border:none;}"
            "QScrollBar:vertical{background:%1;width:8px;border-radius:4px;}"
            "QScrollBar::handle:vertical{background:%2;border-radius:4px;min-height:30px;}"
            "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{border:none;background:none;}")
            .arg(C_BG, C_BORDER));
    }
    if (m_examBodyWidget)
        m_examBodyWidget->setStyleSheet(QString("background:%1;").arg(C_BG));

    // ── Clear previous content ───────────────────────────────────────────────
    while (QLayoutItem *item = m_examBodyLayout->takeAt(0)) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    int studentId = qApp->property("currentUserId").toInt();

    // ── 1. Determine current step ────────────────────────────────────────────
    int examStep = 1;
    {
        QSqlQuery q;
        q.prepare("SELECT NVL(current_step, 1) FROM WINO_PROGRESS WHERE user_id = :sid");
        q.bindValue(":sid", studentId);
        if (q.exec() && q.next()) examStep = q.value(0).toInt();
    }
    QString stepName  = (examStep == 1) ? "Code" : (examStep == 2) ? "Circuit" : "Parking";
    QString stepIcon  = (examStep == 1)
        ? QString::fromUtf8("\xf0\x9f\x93\x96")
        : (examStep == 2) ? QString::fromUtf8("\xf0\x9f\x9a\x97")
                          : QString::fromUtf8("\xf0\x9f\x85\xbf\xef\xb8\x8f");
    QString stepColor = (examStep == 1) ? "#6366f1" : (examStep == 2) ? "#f59e0b" : "#22c55e";

    // ── 2. Header banner ─────────────────────────────────────────────────────
    {
        QFrame *hdr = new QFrame();
        hdr->setStyleSheet(QString(
            "QFrame{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
            "stop:0 %1, stop:1 %2);border-radius:16px;}")
            .arg(stepColor, stepColor + "cc"));
        QVBoxLayout *hl = new QVBoxLayout(hdr);
        hl->setContentsMargins(28, 24, 28, 24);
        hl->setSpacing(8);

        QLabel *ico = new QLabel(stepIcon);
        ico->setStyleSheet("font-size:40px;background:transparent;");

        QLabel *ttl = new QLabel(QString("Examen %1").arg(stepName));
        ttl->setStyleSheet("font-size:26px;font-weight:800;color:#ffffff;background:transparent;");

        QLabel *sub = new QLabel(
            QString::fromUtf8("Demandez votre examen et suivez son avancement en temps r"
                              "\xc3\xa9""el."));
        sub->setWordWrap(true);
        sub->setStyleSheet("font-size:13px;color:rgba(255,255,255,0.85);background:transparent;");

        hl->addWidget(ico);
        hl->addWidget(ttl);
        hl->addWidget(sub);
        m_examBodyLayout->addWidget(hdr);
    }

    // ── 3. Progress / eligibility card ──────────────────────────────────────
    {
        int sessionsDone = 0, sessionsMin = 5;
        double score = 0.0;
        {
            QSqlQuery q;
            q.prepare("SELECT COUNT(*) FROM SESSION_BOOKING "
                      "WHERE student_id=:sid AND session_step=:step AND status='COMPLETED'");
            q.bindValue(":sid",  studentId);
            q.bindValue(":step", examStep);
            if (q.exec() && q.next()) sessionsDone = q.value(0).toInt();
        }
        {
            QSqlQuery q;
            q.prepare("SELECT NVL(CASE WHEN :step=1 THEN code_score "
                      "              WHEN :step=2 THEN circuit_score "
                      "              ELSE parking_score END, 0) "
                      "FROM WINO_PROGRESS WHERE user_id=:sid");
            q.bindValue(":step", examStep);
            q.bindValue(":step", examStep);
            q.bindValue(":sid",  studentId);
            if (q.exec() && q.next()) score = q.value(0).toDouble();
        }

        bool eligible       = (sessionsDone >= sessionsMin && score >= 60.0);
        QString eligColor   = eligible ? "#22c55e" : "#ef4444";
        QString eligIcon    = eligible
            ? QString::fromUtf8("\xe2\x9c\x85")
            : QString::fromUtf8("\xe2\x9d\x8c");
        QString eligText    = eligible
            ? QString::fromUtf8("\xe2\x9c\x94  Eligible \xe2\x80\x94 vous pouvez passer l'examen")
            : QString::fromUtf8("\xe2\x9c\x96  Pas encore \xc3\xa9ligible \xe2\x80\x94 "
                                "encore %1 s\xc3\xa9ance(s) et score \xe2\x89\xa5 60%%")
              .arg(qMax(0, sessionsMin - sessionsDone));

        QFrame *prog = new QFrame();
        prog->setStyleSheet(QString(
            "QFrame{background:%1;border:1px solid %2;border-radius:16px;}").arg(C_CARD, C_BORDER));
        QVBoxLayout *pl = new QVBoxLayout(prog);
        pl->setContentsMargins(24, 20, 24, 20);
        pl->setSpacing(16);

        QLabel *progTitle = new QLabel(QString::fromUtf8("Votre progression"));
        progTitle->setStyleSheet(QString(
            "font-size:15px;font-weight:700;color:%1;background:transparent;").arg(C_TEXT));
        pl->addWidget(progTitle);

        // Three stat boxes
        QHBoxLayout *statsRow = new QHBoxLayout();
        statsRow->setSpacing(12);

        auto makeStatBox = [&](const QString &val, const QString &lbl,
                               const QString &accent) -> QFrame* {
            QFrame *c = new QFrame();
            QString bgAlpha = isDark ? accent + "25" : accent + "15";
            c->setStyleSheet(QString(
                "QFrame{background:%1;border:1px solid %2;border-radius:12px;}")
                .arg(bgAlpha, accent + "55"));
            QVBoxLayout *cl = new QVBoxLayout(c);
            cl->setContentsMargins(12, 16, 12, 16);
            cl->setSpacing(4);
            QLabel *v = new QLabel(val);
            v->setStyleSheet(QString(
                "font-size:24px;font-weight:800;color:%1;background:transparent;").arg(accent));
            v->setAlignment(Qt::AlignCenter);
            QLabel *l2 = new QLabel(lbl);
            l2->setStyleSheet(QString(
                "font-size:11px;font-weight:500;color:%1;background:transparent;").arg(C_MUTED));
            l2->setAlignment(Qt::AlignCenter);
            cl->addWidget(v);
            cl->addWidget(l2);
            return c;
        };

        statsRow->addWidget(makeStatBox(QString::number(sessionsDone),
            QString::fromUtf8("S\xc3\xa9ances effectu\xc3\xa9""es"), "#3b82f6"), 1);
        statsRow->addWidget(makeStatBox(
            QString::number(score, 'f', 0) + "%",
            QString::fromUtf8("Score actuel"), "#8b5cf6"), 1);
        statsRow->addWidget(makeStatBox(
            eligible ? "OUI" : "NON",
            QString::fromUtf8("\xc3\x89ligible"), eligColor), 1);
        pl->addLayout(statsRow);

        // Eligibility banner
        QFrame *eligBanner = new QFrame();
        eligBanner->setStyleSheet(QString(
            "QFrame{background:%1;border-radius:10px;border:none;}")
            .arg(isDark ? eligColor + "22" : eligColor + "18"));
        QHBoxLayout *el = new QHBoxLayout(eligBanner);
        el->setContentsMargins(14, 10, 14, 10);
        QLabel *eligLbl = new QLabel(eligText);
        eligLbl->setWordWrap(true);
        eligLbl->setStyleSheet(QString(
            "font-size:12px;font-weight:600;color:%1;background:transparent;").arg(eligColor));
        el->addWidget(eligLbl);
        pl->addWidget(eligBanner);

        m_examBodyLayout->addWidget(prog);
    }

    // ── 4. Existing exam request status ─────────────────────────────────────
    {
        QSqlQuery q;
        q.prepare(
            "SELECT status, requested_date, comments, passed "
            "FROM   EXAM_REQUEST "
            "WHERE  student_id=:sid AND exam_step=:step "
            "ORDER  BY requested_date DESC FETCH FIRST 1 ROW ONLY");
        q.bindValue(":sid",  studentId);
        q.bindValue(":step", examStep);

        if (q.exec() && q.next()) {
            QString status   = q.value("status").toString();
            QString reqDate  = q.value("requested_date").toDate().toString("dd/MM/yyyy");
            QString comments = q.value("comments").toString();
            bool    passed   = q.value("passed").toInt() == 1;

            QString sColor =
                (status == "APPROVED") ? "#22c55e" :
                (status == "PENDING")  ? "#f59e0b" :
                (status == "REJECTED") ? "#ef4444" : "#3b82f6";
            QString sIcon =
                (status == "APPROVED") ? QString::fromUtf8("\xe2\x9c\x85") :
                (status == "PENDING")  ? QString::fromUtf8("\xe2\x8f\xb3") :
                (status == "REJECTED") ? QString::fromUtf8("\xe2\x9d\x8c")
                                       : QString::fromUtf8("\xf0\x9f\x8e\x93");

            QFrame *reqCard = new QFrame();
            reqCard->setStyleSheet(QString(
                "QFrame{background:%1;border:2px solid %2;border-radius:16px;}")
                .arg(C_CARD, sColor));
            QVBoxLayout *rl = new QVBoxLayout(reqCard);
            rl->setContentsMargins(20, 18, 20, 18);
            rl->setSpacing(10);

            QHBoxLayout *rh = new QHBoxLayout();
            rh->setSpacing(12);
            QLabel *sIco = new QLabel(sIcon);
            sIco->setStyleSheet("font-size:26px;background:transparent;");
            QVBoxLayout *rtxt = new QVBoxLayout();
            rtxt->setSpacing(2);
            QLabel *rtitle = new QLabel(
                QString::fromUtf8("Demande d'examen \xe2\x80\x94 ") + status);
            rtitle->setStyleSheet(QString(
                "font-size:15px;font-weight:700;color:%1;background:transparent;").arg(sColor));
            QLabel *rdate = new QLabel(
                QString::fromUtf8("Soumise le ") + reqDate);
            rdate->setStyleSheet(QString(
                "font-size:12px;color:%1;background:transparent;").arg(C_MUTED));
            rtxt->addWidget(rtitle);
            rtxt->addWidget(rdate);
            rh->addWidget(sIco);
            rh->addLayout(rtxt, 1);
            rl->addLayout(rh);

            if (!comments.isEmpty()) {
                QLabel *cmt = new QLabel(comments);
                cmt->setWordWrap(true);
                cmt->setStyleSheet(QString(
                    "font-size:12px;color:%1;background:%2;"
                    "border-radius:8px;padding:8px 10px;").arg(C_MUTED, C_SURFACE));
                rl->addWidget(cmt);
            }
            if (passed) {
                QLabel *passLbl = new QLabel(
                    QString::fromUtf8("\xf0\x9f\x8f\x86  Examen r\xc3\xa9ussi !"));
                passLbl->setStyleSheet(
                    "font-size:14px;font-weight:700;color:#22c55e;background:transparent;");
                rl->addWidget(passLbl);
            }
            m_examBodyLayout->addWidget(reqCard);
        }
    }

    // ── 5. Available exam sessions ───────────────────────────────────────────
    {
        QSqlQuery q;
        q.prepare(
            "SELECT es.id, TO_CHAR(es.exam_date,'DD/MM/YYYY') AS exam_date, "
            "       es.location, es.max_students, "
            "       (SELECT COUNT(*) FROM EXAM_REGISTRATIONS er "
            "         WHERE er.exam_session_id=es.id) AS registered "
            "FROM   EXAM_SESSIONS es "
            "WHERE  es.exam_date >= TRUNC(SYSDATE) AND es.exam_type=:step_name "
            "ORDER  BY es.exam_date FETCH FIRST 5 ROWS ONLY");
        q.bindValue(":step_name", stepName.toUpper());

        if (q.exec() && q.next()) {
            QLabel *sectionLbl = new QLabel(
                QString::fromUtf8("\xf0\x9f\x93\x85  S\xc3\xa9""ances d'examen disponibles"));
            sectionLbl->setStyleSheet(QString(
                "font-size:15px;font-weight:700;color:%1;background:transparent;").arg(C_TEXT));
            m_examBodyLayout->addWidget(sectionLbl);

            do {
                int     esId      = q.value("id").toInt();
                QString date      = q.value("exam_date").toString();
                QString location  = q.value("location").toString();
                int     maxSt     = q.value("max_students").toInt();
                int     reg       = q.value("registered").toInt();
                int     remaining = qMax(0, maxSt - reg);
                bool    full      = (remaining == 0);
                QString slotClr   = full ? "#ef4444" : (remaining<=3 ? "#f59e0b" : "#22c55e");

                QFrame *slot = new QFrame();
                slot->setStyleSheet(QString(
                    "QFrame{background:%1;border:1px solid %2;border-radius:12px;}")
                    .arg(C_CARD, C_BORDER));
                QHBoxLayout *sl = new QHBoxLayout(slot);
                sl->setContentsMargins(16, 12, 16, 12);
                sl->setSpacing(12);

                QLabel *dateLbl = new QLabel(
                    QString::fromUtf8("\xf0\x9f\x93\x85  ") + date);
                dateLbl->setStyleSheet(QString(
                    "font-size:14px;font-weight:700;color:%1;background:transparent;").arg(C_TEXT));
                sl->addWidget(dateLbl);

                if (!location.isEmpty()) {
                    QLabel *locLbl = new QLabel(
                        QString::fromUtf8("\xf0\x9f\x93\x8d  ") + location);
                    locLbl->setStyleSheet(QString(
                        "font-size:12px;color:%1;background:transparent;").arg(C_MUTED));
                    sl->addWidget(locLbl);
                }
                sl->addStretch();

                QLabel *badge = new QLabel(
                    full ? QString::fromUtf8("Complet")
                         : QString("%1 place(s)").arg(remaining));
                badge->setStyleSheet(QString(
                    "font-size:11px;font-weight:700;color:%1;"
                    "background:%2;border-radius:8px;padding:4px 10px;")
                    .arg(slotClr, slotClr + (isDark ? "30" : "18")));
                sl->addWidget(badge);

                if (!full) {
                    QPushButton *bookBtn = new QPushButton(
                        QString::fromUtf8("R\xc3\xa9""server ma place"));
                    bookBtn->setCursor(Qt::PointingHandCursor);
                    bookBtn->setStyleSheet(
                        "QPushButton{background:#3b82f6;color:white;border:none;"
                        "border-radius:8px;padding:6px 14px;font-size:12px;font-weight:600;}"
                        "QPushButton:hover{background:#2563eb;}");
                    connect(bookBtn, &QPushButton::clicked, this,
                            [this, esId, studentId]() {
                        QSqlQuery ins;
                        ins.prepare(
                            "INSERT INTO EXAM_REGISTRATIONS "
                            "(student_id,exam_session_id,registered_at,status) "
                            "VALUES(:sid,:esid,SYSDATE,'REGISTERED')");
                        ins.bindValue(":sid",  studentId);
                        ins.bindValue(":esid", esId);
                        if (ins.exec()) {
                            QMessageBox::information(this,
                                QString::fromUtf8("Inscription confirm\xc3\xa9""e"),
                                QString::fromUtf8(
                                    "Votre inscription \xc3\xa0 la s\xc3\xa9""ance "
                                    "d'examen a \xc3\xa9t\xc3\xa9 confirm\xc3\xa9""e !"));
                            refreshExamPage();
                        } else {
                            QMessageBox::warning(this, "Erreur", ins.lastError().text());
                        }
                    });
                    sl->addWidget(bookBtn);
                }
                m_examBodyLayout->addWidget(slot);
            } while (q.next());
        }
    }

    // ── 6. Submit exam request form ──────────────────────────────────────────
    {
        QFrame *form = new QFrame();
        form->setStyleSheet(QString(
            "QFrame{background:%1;border:1px solid %2;border-radius:16px;}").arg(C_CARD, C_BORDER));
        QVBoxLayout *fl = new QVBoxLayout(form);
        fl->setContentsMargins(24, 22, 24, 22);
        fl->setSpacing(14);

        // Title row with icon
        QHBoxLayout *titleRow = new QHBoxLayout();
        QLabel *formIcon = new QLabel(QString::fromUtf8("\xf0\x9f\x93\x9d"));
        formIcon->setStyleSheet("font-size:22px;background:transparent;");
        QLabel *formTitle = new QLabel(
            QString::fromUtf8("Soumettre une demande d'examen"));
        formTitle->setStyleSheet(QString(
            "font-size:16px;font-weight:700;color:%1;background:transparent;").arg(C_TEXT));
        titleRow->addWidget(formIcon);
        titleRow->addWidget(formTitle, 1);
        fl->addLayout(titleRow);

        QLabel *formDesc = new QLabel(
            QString::fromUtf8(
                "Indiquez vos pr\xc3\xa9""f\xc3\xa9rences de cr\xc3\xa9""neau. "
                "Votre instructeur confirmera la date et l'heure d\xc3\xa9""finitives."));
        formDesc->setWordWrap(true);
        formDesc->setStyleSheet(QString(
            "font-size:13px;color:%1;background:transparent;").arg(C_MUTED));
        fl->addWidget(formDesc);

        // Divider
        QFrame *div = new QFrame();
        div->setFrameShape(QFrame::HLine);
        div->setStyleSheet(QString("color:%1;background:%1;").arg(C_BORDER));
        div->setFixedHeight(1);
        fl->addWidget(div);

        // Period label + combo
        QLabel *tfLbl = new QLabel(QString::fromUtf8("P\xc3\xa9riode souhait\xc3\xa9""e :"));
        tfLbl->setStyleSheet(QString(
            "font-size:13px;font-weight:600;color:%1;background:transparent;").arg(C_TEXT));
        fl->addWidget(tfLbl);

        QComboBox *tfCombo = new QComboBox();
        tfCombo->addItems({
            QString::fromUtf8("Cette semaine"),
            QString::fromUtf8("Ce mois-ci"),
            QString::fromUtf8("Le mois prochain")});
        tfCombo->setStyleSheet(QString(
            "QComboBox{background:%1;border:1px solid %2;border-radius:8px;"
            "padding:8px 12px;font-size:13px;color:%3;min-height:36px;}"
            "QComboBox::drop-down{border:none;width:24px;}"
            "QComboBox QAbstractItemView{background:%1;color:%3;border:1px solid %2;}")
            .arg(C_SURFACE, C_BORDER, C_TEXT));
        fl->addWidget(tfCombo);

        // Unavailable dates
        QLabel *unavLbl = new QLabel(
            QString::fromUtf8("Dates indisponibles (optionnel) :"));
        unavLbl->setStyleSheet(QString(
            "font-size:13px;font-weight:600;color:%1;background:transparent;").arg(C_TEXT));
        fl->addWidget(unavLbl);

        QLineEdit *unavInput = new QLineEdit();
        unavInput->setPlaceholderText(
            QString::fromUtf8("Ex: lundis, 24/05, semaine du 10 juin..."));
        unavInput->setStyleSheet(QString(
            "QLineEdit{background:%1;border:1px solid %2;border-radius:8px;"
            "padding:8px 12px;font-size:13px;color:%3;min-height:36px;}"
            "QLineEdit:focus{border-color:%4;}")
            .arg(C_SURFACE, C_BORDER, C_TEXT, C_TEAL));
        fl->addWidget(unavInput);

        // Submit button
        QPushButton *submitBtn = new QPushButton(
            QString::fromUtf8("\xf0\x9f\x93\xa4  Envoyer la demande"));
        submitBtn->setCursor(Qt::PointingHandCursor);
        submitBtn->setFixedHeight(46);
        submitBtn->setStyleSheet(
            "QPushButton{background:#14b8a6;color:white;border:none;border-radius:10px;"
            "font-size:14px;font-weight:700;}"
            "QPushButton:hover{background:#0f9d8e;}"
            "QPushButton:disabled{background:#94a3b8;}");

        connect(submitBtn, &QPushButton::clicked, this,
                [this, tfCombo, unavInput, examStep, studentId]() {
            int instructorId = 1;
            {
                QSqlQuery qi;
                qi.prepare("SELECT INSTRUCTOR_ID FROM STUDENTS WHERE ID=:sid");
                qi.bindValue(":sid", studentId);
                if (qi.exec() && qi.next()) instructorId = qi.value(0).toInt();
            }
            QString comments = QString::fromUtf8("P\xc3\xa9riode: ") + tfCombo->currentText();
            if (!unavInput->text().trimmed().isEmpty())
                comments += QString::fromUtf8(" | Indisponible: ") + unavInput->text().trimmed();

            QSqlQuery ins;
            ins.prepare(
                "INSERT INTO EXAM_REQUEST "
                "(STUDENT_ID,INSTRUCTOR_ID,EXAM_STEP,REQUESTED_DATE,AMOUNT,STATUS,COMMENTS,PASSED) "
                "VALUES(:sid,:iid,:step,SYSDATE,50.0,'PENDING',:comments,0)");
            ins.bindValue(":sid",      studentId);
            ins.bindValue(":iid",      instructorId);
            ins.bindValue(":step",     examStep);
            ins.bindValue(":comments", comments);

            if (ins.exec()) {
                QMessageBox::information(this,
                    QString::fromUtf8("Demande envoy\xc3\xa9""e"),
                    QString::fromUtf8(
                        "Votre demande a \xc3\xa9t\xc3\xa9 transmise \xc3\xa0 votre instructeur. "
                        "Il vous confirmera la date d'examen."));
                refreshExamPage();
            } else {
                QMessageBox::warning(this, "Erreur", ins.lastError().text());
            }
        });

        fl->addWidget(submitBtn);
        m_examBodyLayout->addWidget(form);
    }

    m_examBodyLayout->addStretch();
}

void WinoStudentDashboard::onBookSessionClicked()
{
    BookingSession *booking = new BookingSession(this);
    
    // When booking confirmed, refresh sessions list and calendar highlights
    connect(booking, &BookingSession::sessionBooked, this, [this]() {
        updateCalendarHighlights();
        updateBalance();
    });
    
    booking->show();
}

void WinoStudentDashboard::onAIRecommendationsClicked()
{
    // Switch to the embedded AI Recommendations page (stack index 1)
    m_mainStack->setCurrentIndex(1);
    // Refresh recommendation cards with the latest weather + session data
    if (m_aiPage) m_aiPage->refreshContent();
}

void WinoStudentDashboard::updateColors()
{
    ThemeManager* theme = ThemeManager::instance();
    bool isDark = theme->currentTheme() == ThemeManager::Dark;
    
    // Update header — always dark regardless of theme
    QFrame* hdrFrame = centralWidget->findChild<QFrame*>("header");
    if (hdrFrame) {
        hdrFrame->setStyleSheet(
            "QFrame#header { background-color: #0F172A; border-bottom: 1px solid #1E293B; }"
            "QFrame#header * { background: transparent; }");
    }
    
    // Update scroll area and content
    QScrollArea* scrollArea = centralWidget->findChild<QScrollArea*>("scrollArea");
    if (scrollArea) {
        scrollArea->setStyleSheet(
            QString("QScrollArea#scrollArea { background-color: %1; border: none; }"
                    "QScrollBar:vertical { background-color: %1; width: 12px; border-radius: 6px; }"
                    "QScrollBar::handle:vertical { background-color: %2; border-radius: 6px; min-height: 30px; }"
                    "QScrollBar::handle:vertical:hover { background-color: %3; }"
                    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { border: none; background: none; }")
            .arg(theme->backgroundColor(), theme->tealLighter(), theme->accentColor())
        );
    }
    
    QWidget* contentWidget = centralWidget->findChild<QWidget*>("contentWidget");
    if (contentWidget) {
        contentWidget->setStyleSheet(
            QString("QWidget#contentWidget { background-color: %1; }").arg(theme->backgroundColor())
        );
    }
    
    // Update Info Cards
    QList<QFrame*> infoCards = centralWidget->findChildren<QFrame*>("infoCard");
    for (QFrame* card : infoCards) {
        card->setStyleSheet(QString("QFrame#infoCard { background-color: %1; border-radius: 12px; }").arg(theme->cardColor()));
        
        QLabel* title = card->findChild<QLabel*>("cardTitle");
        if (title) title->setStyleSheet(QString("QLabel#cardTitle { color: %1; font-size: 13px; font-weight: 500; line-height: 1.5; padding: 3px 0; }").arg(theme->secondaryTextColor()));
        
        QLabel* value = card->findChild<QLabel*>("cardValue");
        // Maintain logic for red text if negative, but update default primary color
        if (value) {
            QString currentText = value->text();
            QString valueColor = currentText.contains("-") ? (isDark ? "#F87171" : "#DC2626") : theme->primaryTextColor();
            value->setStyleSheet(QString("QLabel#cardValue { color: %1; font-size: 36px; font-weight: bold; line-height: 1.3; padding: 5px 0; }").arg(valueColor));
        }
        
        QLabel* subtitle = card->findChild<QLabel*>("cardSubtitle");
        if (subtitle) {
            QString currentText = subtitle->text();
            if (currentText.contains("✓")) {
                QString successColor = isDark ? "#34D399" : "#059669";
                QString successBg = isDark ? "#134E4A" : "#ECFDF5";
                subtitle->setStyleSheet(QString("QLabel#cardSubtitle { color: %1; font-size: 12px; background-color: %2; padding: 8px 14px; border-radius: 6px; font-weight: 600; line-height: 1.5; }").arg(successColor, successBg));
            } else {
                QString errorColor = isDark ? "#F87171" : "#DC2626";
                subtitle->setStyleSheet(QString("QLabel#cardSubtitle { color: %1; font-size: 12px; font-weight: 500; line-height: 1.5; padding: 3px 0; }").arg(errorColor));
            }
        }
    }
    
    // Update Balance Card
    QFrame* balanceCard = centralWidget->findChild<QFrame*>("balanceCard");
    if (balanceCard) {
        balanceCard->setStyleSheet(QString("QFrame#balanceCard { background-color: %1; border-radius: 12px; }").arg(theme->cardColor()));
        
        QLabel* balTitle = balanceCard->findChild<QLabel*>("balanceTitle");
        if (balTitle) balTitle->setStyleSheet(QString("QLabel#balanceTitle { color: %1; font-size: 13px; font-weight: 500; line-height: 1.5; padding: 3px 0; }").arg(theme->secondaryTextColor()));
        
        QLabel* balValue = balanceCard->findChild<QLabel*>("balanceValue");
        if (balValue) {
            QString currentText = balValue->text();
            // "- X TND" → owes → red   |  "+ X TND" or "0 TND" → settled/credit → green
            QString valueColor = currentText.contains("- ") ? (isDark ? "#F87171" : "#DC2626") : (isDark ? "#34D399" : "#059669");
            balValue->setStyleSheet(QString("QLabel#balanceValue { color: %1; font-size: 36px; font-weight: bold; line-height: 1.3; padding: 5px 0; }").arg(valueColor));
        }

        QLabel* balWarn = balanceCard->findChild<QLabel*>("balanceWarningLabel");
        if (balWarn && !balWarn->isHidden()) {
            QString warnBg    = isDark ? "#451a1a" : "#FEE2E2";
            QString warnColor = isDark ? "#F87171" : "#DC2626";
            balWarn->setStyleSheet(QString("QLabel { color: %1; font-size: 12px; font-weight: 500; background-color: %2; padding: 8px 12px; border-radius: 6px; }").arg(warnColor, warnBg));
        }

        QLabel* balOk = balanceCard->findChild<QLabel*>("balanceOkLabel");
        if (balOk && !balOk->isHidden()) {
            balOk->setStyleSheet(QString("QLabel { color: %1; font-size: 13px; font-weight: 500; background-color: %2; padding: 8px 12px; border-radius: 6px; }").arg(isDark ? "#34D399" : "#059669", isDark ? "#134E4A" : "#ECFDF5"));
        }
    }
    
    // Update Progress Card
    QFrame* progressCard = centralWidget->findChild<QFrame*>("progressCard");
    if (progressCard) {
        progressCard->setStyleSheet(QString("QFrame#progressCard { background-color: %1; border-radius: 12px; }").arg(theme->cardColor()));
        QLabel* title = progressCard->findChild<QLabel*>("progressTitle");
        if (title) title->setStyleSheet(QString("QLabel#progressTitle { color: %1; font-size: 20px; font-weight: bold; padding: 5px 0; }").arg(theme->primaryTextColor()));
        
        // Update stage boxes within progress card
        auto updateStageBox = [&](const QString& objName, int state) {
            QFrame* stageBox = progressCard->findChild<QFrame*>(objName);
            if (!stageBox) return;
            
            QString bgColor, borderColor, titleColor, subtitleColor;
            QString borderStyle = "solid";
            
            if (state == 2) {
                bgColor = isDark ? "#134E4A" : "#ECFDF5";
                borderColor = isDark ? "#34D399" : "#10B981";
                titleColor = isDark ? "#34D399" : "#065F46";
                subtitleColor = isDark ? "#6EE7B7" : "#059669";
            } else if (state == 1) {
                bgColor = isDark ? "#115E59" : "#F0FDFA";
                borderColor = "#14B8A6";
                titleColor = theme->primaryTextColor();
                subtitleColor = isDark ? "#5EEAD4" : "#0D9488";
            } else {
                bgColor = isDark ? "#1E293B" : "#F9FAFB";
                borderColor = isDark ? "#475569" : "#E5E7EB";
                titleColor = isDark ? "#64748B" : "#9CA3AF";
                subtitleColor = isDark ? "#64748B" : "#9CA3AF";
                borderStyle = "dashed";
            }
            
            stageBox->setStyleSheet(
                QString("QFrame { background-color: %1; border: %6 %5 %2; border-radius: 12px; }")
                .arg(bgColor, borderColor, "", "", borderStyle, (state == 1) ? "3px" : "2px")
            );
            
            // Update child labels
            QList<QLabel*> labels = stageBox->findChildren<QLabel*>();
            for (int i = 0; i < labels.size(); i++) {
                if (labels[i]->styleSheet().contains("font-size: 16px")) {
                    labels[i]->setStyleSheet(QString("QLabel { font-size: 16px; font-weight: bold; color: %1; border: none; background: transparent; }").arg(titleColor));
                } else if (labels[i]->styleSheet().contains("font-size: 13px")) {
                    labels[i]->setStyleSheet(QString("QLabel { font-size: 13px; font-weight: 600; color: %1; border: none; background: transparent; }").arg(subtitleColor));
                }
            }
        };
        
        updateStageBox("stageBox_Completed", 2);
        updateStageBox("stageBox_Current", 1);
        updateStageBox("stageBox_Locked", 0);
    }
    
    // Update Calendar Card
    QFrame* calendarCard = centralWidget->findChild<QFrame*>("calendarCard");
    if (calendarCard) {
        calendarCard->setStyleSheet(QString("QFrame#calendarCard { background-color: %1; border-radius: 12px; }").arg(theme->cardColor()));
        QLabel* title = calendarCard->findChild<QLabel*>("calendarTitle");
        if (title) title->setStyleSheet(QString("QLabel#calendarTitle { color: %1; font-size: 20px; font-weight: bold; padding: 5px 0 10px 0; }").arg(theme->primaryTextColor()));
    }
    
    // Update Calendar Widget Styling
    if (calendarWidget) {
        QString calBg = theme->cardColor();
        QString calBorder = theme->borderColor();
        QString navBg = theme->headerColor();
        QString textPrimary = theme->primaryTextColor();
        QString textSecondary = theme->secondaryTextColor();
        QString headerBg = isDark ? "#134E4A" : "#F0FDFA";
        QString headerText = isDark ? "#5EEAD4" : "#0D9488";
        
        calendarWidget->setStyleSheet(
            QString(
            // ── Global base ──
            "QCalendarWidget {"
            "    background-color: %1;"
            "    border: 1px solid %2;"
            "    border-radius: 10px;"
            "}"
            
            // ── Navigation Bar ──
            "QCalendarWidget QWidget#qt_calendar_navigationbar {"
            "    background-color: %3;"
            "    border-top-left-radius: 10px;"
            "    border-top-right-radius: 10px;"
            "    padding: 6px 8px;"
            "    min-height: 44px;"
            "}"
            
            // ── Prev / Next / Month / Year buttons ──
            "QCalendarWidget QToolButton {"
            "    color: white;"
            "    background-color: transparent;"
            "    font-size: 15px;"
            "    font-weight: bold;"
            "    border: none;"
            "    border-radius: 6px;"
            "    padding: 6px 14px;"
            "    margin: 2px;"
            "}"
            "QCalendarWidget QToolButton:hover {"
            "    background-color: #14B8A6;"
            "    color: white;"
            "}"
            "QCalendarWidget QToolButton#qt_calendar_prevmonth,"
            "QCalendarWidget QToolButton#qt_calendar_nextmonth {"
            "    qproperty-icon: none;"
            "    font-size: 18px;"
            "    min-width: 32px;"
            "}"
            "QCalendarWidget QToolButton::menu-indicator {"
            "    image: none;"
            "}"
            
            // ── Dropdown Menus ──
            "QCalendarWidget QMenu {"
            "    background-color: %1;"
            "    color: %4;"
            "    border: 1px solid %2;"
            "    border-radius: 8px;"
            "    padding: 4px;"
            "    font-size: 13px;"
            "}"
            "QCalendarWidget QMenu::item {"
            "    padding: 6px 20px;"
            "    border-radius: 4px;"
            "}"
            "QCalendarWidget QMenu::item:selected {"
            "    background-color: #14B8A6;"
            "    color: white;"
            "}"
            
            // ── Spin Box ──
            "QCalendarWidget QSpinBox {"
            "    color: white;"
            "    background-color: transparent;"
            "    border: 1px solid #374151;"
            "    border-radius: 6px;"
            "    padding: 4px 8px;"
            "    font-size: 14px;"
            "    font-weight: bold;"
            "}"
            
            // ── Day-of-week header row (Mon, Tue …) ──
            "QCalendarWidget QHeaderView {"
            "    background-color: %5;"
            "    border: none;"
            "    border-bottom: 2px solid #14B8A6;"
            "}"
            "QCalendarWidget QHeaderView::section {"
            "    background-color: %5;"
            "    color: %6;"
            "    font-size: 13px;"
            "    font-weight: bold;"
            "    padding: 8px 0px;"
            "}"
            
            // ── Day cells (main grid) ──
            "QCalendarWidget QAbstractItemView {"
            "    background-color: %1;"
            "    alternate-background-color: %5;"
            "    selection-background-color: #14B8A6;"
            "    selection-color: white;"
            "    font-size: 14px;"
            "    outline: none;"
            "    gridline-color: %2;"
            "    border: none;"
            "}"
            "QCalendarWidget QAbstractItemView:enabled {"
            "    color: %4;"
            "}"
            "QCalendarWidget QAbstractItemView:disabled {"
            "    color: %7;"
            "}"
            
            // ── Today highlight via table view ──
            "QCalendarWidget QTableView {"
            "    outline: none;"
            "    border: none;"
            "}"
            ).arg(calBg, calBorder, navBg, textPrimary, headerBg, headerText, theme->mutedTextColor())
        );
        
        // Update Header Text Format for Dark Mode
        QTextCharFormat headerFormat;
        headerFormat.setForeground(QBrush(QColor(isDark ? "#5EEAD4" : "#0D9488")));
        headerFormat.setFontWeight(QFont::Bold);
        headerFormat.setBackground(QBrush(QColor(isDark ? "#134E4A" : "#F0FDFA")));
        calendarWidget->setHeaderTextFormat(headerFormat);
        
        // Weekend format (red) - remains same but good to re-apply just in case
        QTextCharFormat weekendFormat;
        weekendFormat.setForeground(QBrush(QColor(isDark ? "#F87171" : "#EF4444")));
        calendarWidget->setWeekdayTextFormat(Qt::Saturday, weekendFormat);
        calendarWidget->setWeekdayTextFormat(Qt::Sunday, weekendFormat);
        
        updateCalendarHighlights();
    }

    // ── Re-apply action button styles (global qApp stylesheet flattens them) ──
    QString darkBtnBg    = isDark ? "#1F2937" : "#374151";
    QString darkBtnHover = isDark ? "#374151" : "#4B5563";

    QPushButton *bookBtn = centralWidget->findChild<QPushButton*>("actionButton");
    if (bookBtn) {
        bookBtn->setStyleSheet(
            "QPushButton#actionButton {"
            "    background-color: #14B8A6; color: white;"
            "    font-size: 15px; font-weight: bold;"
            "    border: none; border-radius: 10px; padding: 16px 32px;"
            "}"
            "QPushButton#actionButton:hover { background-color: #0F9D8E; }"
            "QPushButton#actionButton:pressed { background-color: #0D9488; }");
    }

    QPushButton *aiBtn = centralWidget->findChild<QPushButton*>("secondaryButton");
    if (aiBtn) {
        aiBtn->setStyleSheet(
            QString("QPushButton#secondaryButton {"
            "    background-color: %1; color: white;"
            "    font-size: 15px; font-weight: bold;"
            "    border: none; border-radius: 10px; padding: 16px 32px;"
            "}"
            "QPushButton#secondaryButton:hover { background-color: %2; }"
            "QPushButton#secondaryButton:pressed { background-color: #111827; }").arg(darkBtnBg, darkBtnHover));
    }

    if (examBtn) {
        examBtn->setStyleSheet(
            QString("QPushButton#examButton {"
            "    background-color: %1; color: white;"
            "    font-size: 15px; font-weight: bold;"
            "    border: none; border-radius: 10px; padding: 16px 32px;"
            "}"
            "QPushButton#examButton:hover { background-color: %2; }"
            "QPushButton#examButton:pressed { background-color: #111827; }").arg(darkBtnBg, darkBtnHover));
    }

    // ── Header background ──
    QFrame *hdr = centralWidget->findChild<QFrame*>("header");
    if (hdr) {
        hdr->setStyleSheet(
            "QFrame#header { background-color: #0F172A; border-bottom: 1px solid #1E293B; }"
            "QFrame#header * { background: transparent; }");
    }

    // ── Scroll area & content background ──
    QScrollArea *sa = centralWidget->findChild<QScrollArea*>("scrollArea");
    if (sa) {
        sa->setStyleSheet(
            QString("QScrollArea#scrollArea { background-color: %1; border: none; }"
                    "QScrollBar:vertical { background: %1; width: 8px; border-radius: 4px; }"
                    "QScrollBar::handle:vertical { background: %2; border-radius: 4px; min-height: 30px; }"
                    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { border: none; background: none; }")
            .arg(theme->backgroundColor(), theme->tealLighter()));
    }
    QWidget *cw = centralWidget->findChild<QWidget*>("contentWidget");
    if (cw) {
        cw->setStyleSheet(
            QString("QWidget#contentWidget { background-color: %1; }").arg(theme->backgroundColor()));
    }
}


void WinoStudentDashboard::onThemeChanged()
{
    // Defer updateColors() so it runs AFTER qApp->setStyleSheet() has been applied
    // (StudentLearningHub::applyTheme() defers the global SS via singleShot(0)).
    // Using singleShot(20) guarantees our widget-level styles always win.
    QTimer::singleShot(20, this, [this]() { updateColors(); });

    // Update theme toggle button icon
    themeToggleBtn->setText(ThemeManager::instance()->currentTheme() == ThemeManager::Light ? "🌙" : "☀️");
}

void WinoStudentDashboard::onWeatherDataReady()
{
    // Re-apply calendar highlights with weather data
    updateCalendarHighlights();
}

// ══════════════════════════════════════
// PAYMENT HISTORY DIALOG
// ══════════════════════════════════════
void WinoStudentDashboard::onPaymentHistoryClicked()
{
    ThemeManager* theme = ThemeManager::instance();
    bool isDark = theme->currentTheme() == ThemeManager::Dark;

    int studentId = qApp->property("currentUserId").toInt();
    if (studentId == 0) return;

    // Fetch payment history — using real column names from the PAYMENT table
    QSqlQuery q;
    q.prepare(
        "SELECT p.payment_id, p.amount, p.status, p.d17_code, "
        "       TO_CHAR(p.facture_date, 'DD/MM/YYYY HH24:MI') "
        "FROM PAYMENT p "
        "WHERE p.student_id = :sid "
        "ORDER BY p.facture_date DESC NULLS LAST"
    );
    q.bindValue(":sid", studentId);

    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("Historique de Paiement");
    dialog->setMinimumSize(700, 500);
    dialog->setStyleSheet(QString("QDialog { background-color: %1; }").arg(theme->backgroundColor()));

    QVBoxLayout *mainLayout = new QVBoxLayout(dialog);
    mainLayout->setContentsMargins(25, 20, 25, 20);
    mainLayout->setSpacing(15);

    // Title
    QLabel *titleLbl = new QLabel("📋 Historique de vos paiements");
    titleLbl->setStyleSheet(QString("QLabel { color: %1; font-size: 20px; font-weight: bold; }").arg(theme->primaryTextColor()));
    mainLayout->addWidget(titleLbl);

    // Scroll area with cards
    QScrollArea *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }");
    QWidget *scrollContent = new QWidget();
    scrollContent->setStyleSheet("QWidget { background: transparent; }");
    QVBoxLayout *cardsLayout = new QVBoxLayout(scrollContent);
    cardsLayout->setSpacing(12);
    cardsLayout->setContentsMargins(0, 0, 0, 0);

    bool anyRecord = false;
    if (q.exec()) {
        while (q.next()) {
            anyRecord = true;
            int payId = q.value(0).toInt();
            double amount = q.value(1).toDouble();
            QString status = q.value(2).toString();
            QString txCode = q.value(3).toString();
            QString payDate = q.value(4).toString();

            bool isValidated = (status == "PAID" || status == "VALID" || status == "SUCCESS"
                               || status == "ACCEPTED" || status == "VERIFIED" || status == "CONFIRMED" || status == "COMPLETED");
            bool isRejected = (status == "FAILED" || status == "CANCELLED" || status == "REJECTED"
                              || status == "DECLINED" || status == "INVALID");

            QString cardBg = isDark ? "#1E293B" : "#FFFFFF";
            QString borderColor = isDark ? "#334155" : "#E2E8F0";
            
            QString statusColor = isValidated ? "#10B981"
                                : isRejected  ? "#EF4444"
                                              : "#F59E0B";
            QString statusIcon = isValidated ? "✅" : isRejected ? "❌" : "⏳";

            QFrame *card = new QFrame();
            card->setStyleSheet(QString("QFrame { background-color: %1; border-radius: 8px; border: 1px solid %2; }")
                                .arg(cardBg, borderColor));
            QHBoxLayout *cardLayout = new QHBoxLayout(card);
            cardLayout->setContentsMargins(15, 12, 15, 12);
            cardLayout->setSpacing(15);

            // Left info block
            QVBoxLayout *infoLayout = new QVBoxLayout();
            infoLayout->setSpacing(4);

            QLabel *dateLbl = new QLabel(payDate.isEmpty() ? "Date inconnue" : payDate);
            dateLbl->setStyleSheet(QString("QLabel { color: %1; font-size: 12px; }").arg(theme->secondaryTextColor()));

            QLabel *txLbl = new QLabel(QString("Code D17: <b>%1</b>").arg(txCode.isEmpty() ? "N/A" : txCode));
            txLbl->setStyleSheet(QString("QLabel { color: %1; font-size: 13px; }").arg(theme->primaryTextColor()));

            infoLayout->addWidget(dateLbl);
            infoLayout->addWidget(txLbl);

            // Status chip
            QLabel *statusLbl = new QLabel(statusIcon + " " + status);
            statusLbl->setStyleSheet(QString("QLabel { color: %1; font-size: 13px; font-weight: bold; background: transparent; border: none; }").arg(statusColor));
            statusLbl->setAlignment(Qt::AlignCenter);

            // Amount
            QLabel *amtLbl = new QLabel(QString("<b>%1 DT</b>").arg(amount, 0, 'f', 0));
            amtLbl->setStyleSheet(QString("QLabel { color: %1; font-size: 16px; font-weight: bold; background: transparent; border: none; }").arg(theme->primaryTextColor()));
            amtLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

            cardLayout->addLayout(infoLayout, 3);
            cardLayout->addWidget(statusLbl, 1);
            cardLayout->addWidget(amtLbl, 1);

            // Invoice download button (only for validated)
            if (isValidated) {
                QPushButton *invoiceBtn = new QPushButton("⬇ Facture PDF");
                invoiceBtn->setFixedHeight(34);
                invoiceBtn->setStyleSheet(
                    "QPushButton { background-color: #14B8A6; color: white; font-size: 12px; font-weight: 600;"
                    " border: none; border-radius: 6px; padding: 6px 14px; }"
                    "QPushButton:hover { background-color: #0F9D8E; }"
                );
                invoiceBtn->setCursor(Qt::PointingHandCursor);

                // Capture data for the lambda
                int capPayId = payId;
                double capAmt = amount;
                QString capTx = txCode;
                QString capDate = payDate;
                QString capInstr = QString(); // instructor info not available in this query
                QString capStatus = status;

                connect(invoiceBtn, &QPushButton::clicked, [=]() {
                    // Generate PDF invoice
                    QFileDialog saveDialog(dialog);
                    saveDialog.setAcceptMode(QFileDialog::AcceptSave);
                    saveDialog.setNameFilter("PDF (*.pdf)");
                    saveDialog.setDefaultSuffix("pdf");
                    saveDialog.selectFile(QString("Facture_%1.pdf").arg(capPayId));
                    if (saveDialog.exec() != QDialog::Accepted) return;

                    QString path = saveDialog.selectedFiles().first();

                    QPrinter printer(QPrinter::ScreenResolution);
                    printer.setOutputFormat(QPrinter::PdfFormat);
                    printer.setOutputFileName(path);
                    printer.setPageSize(QPageSize(QPageSize::A4));
                    printer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);

                    QPainter painter;
                    if (!painter.begin(&printer)) { QMessageBox::critical(dialog, "Erreur", "Impossible de créer le PDF."); return; }

                    // Work in logical coordinates — A4 at screen DPI ≈ 794 × 1123 px
                    QRectF page = printer.pageRect(QPrinter::DevicePixel);
                    double W = page.width();
                    double H = page.height();
                    double m = W * 0.08; // margin

                    // ── WHITE BACKGROUND ──
                    painter.fillRect(page, QColor("#FFFFFF"));

                    // ── TEAL HEADER BAND (25% height) ──
                    double hdrH = H * 0.22;
                    painter.fillRect(0, 0, W, hdrH, QColor("#0F766E"));

                    // Decorative circles
                    painter.save();
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(QColor(255, 255, 255, 18));
                    painter.drawEllipse(QPointF(W * 0.85, hdrH * 0.1), W * 0.22, W * 0.22);
                    painter.drawEllipse(QPointF(W * 0.02, hdrH * 0.85), W * 0.15, W * 0.15);
                    painter.restore();

                    // School name
                    painter.setPen(Qt::white);
                    QFont f1("Arial", 22, QFont::Bold);
                    painter.setFont(f1);
                    painter.drawText(QRectF(m, hdrH * 0.12, W - 2*m, hdrH * 0.35),
                                     Qt::AlignLeft | Qt::AlignVCenter, "AUTO-ECOLE WINO");

                    QFont f2("Arial", 13);
                    painter.setFont(f2);
                    painter.drawText(QRectF(m, hdrH * 0.47, W - 2*m, hdrH * 0.22),
                                     Qt::AlignLeft, "Recu de Paiement");

                    QFont f3("Arial", 10);
                    painter.setFont(f3);
                    painter.drawText(QRectF(m, hdrH * 0.67, W - 2*m, hdrH * 0.18),
                                     Qt::AlignLeft,
                                     QString("N Recu : PAY-%1   |   Date : %2")
                                     .arg(capPayId, 6, 10, QChar('0')).arg(capDate));

                    // ── DIVIDER ──
                    double dy = hdrH + H * 0.04;
                    painter.setPen(QPen(QColor("#14B8A6"), 1.5));
                    painter.drawLine(QPointF(m, dy), QPointF(W - m, dy));

                    // ── BODY ROWS ──
                    dy += H * 0.04;
                    QFont labelF("Arial", 10, QFont::Bold);
                    QFont valueF("Arial", 10);
                    double rowH = H * 0.055;

                    auto drawRow = [&](const QString& label, const QString& value) {
                        painter.setPen(QColor("#374151"));
                        painter.setFont(labelF);
                        painter.drawText(QRectF(m, dy, W * 0.38, rowH), Qt::AlignLeft | Qt::AlignVCenter, label);
                        painter.setFont(valueF);
                        painter.setPen(QColor("#111827"));
                        painter.drawText(QRectF(m + W * 0.40, dy, W * 0.52, rowH), Qt::AlignLeft | Qt::AlignVCenter, value);
                        // row separator
                        painter.setPen(QPen(QColor("#E5E7EB"), 0.5));
                        painter.drawLine(QPointF(m, dy + rowH), QPointF(W - m, dy + rowH));
                        dy += rowH + H * 0.01;
                    };

                    drawRow("Code de transaction :", capTx.isEmpty() ? "N/A" : capTx);
                    drawRow("Statut du paiement :", capStatus);
                    drawRow("Methode de paiement :", "D17 Digital Wallet");

                    // ── AMOUNT BOX ──
                    dy += H * 0.03;
                    double boxH = H * 0.10;
                    painter.fillRect(QRectF(m, dy, W - 2*m, boxH), QColor("#ECFDF5"));
                    painter.setPen(QPen(QColor("#A7F3D0"), 1));
                    painter.drawRect(QRectF(m, dy, W - 2*m, boxH));
                    QFont bigF("Arial", 18, QFont::Bold);
                    painter.setFont(bigF);
                    painter.setPen(QColor("#065F46"));
                    painter.drawText(QRectF(m, dy, W - 2*m, boxH), Qt::AlignCenter,
                                     QString("Montant : %1 DT").arg(capAmt, 0, 'f', 2));

                    // ── FOOTER ──
                    double footerY = H - H * 0.10;
                    painter.setPen(QPen(QColor("#D1D5DB"), 1));
                    painter.drawLine(QPointF(m, footerY), QPointF(W - m, footerY));
                    QFont footF("Arial", 8);
                    painter.setFont(footF);
                    painter.setPen(QColor("#9CA3AF"));
                    painter.drawText(QRectF(m, footerY + 8, W - 2*m, H * 0.08),
                                     Qt::AlignCenter | Qt::TextWordWrap,
                                     "Auto-Ecole WINO  |  contact@wino.tn  |  Tel: 71123456\n"
                                     "Ce document est une preuve de paiement officielle.");

                    painter.end();
                    QMessageBox::information(dialog, "Succes", QString("Facture:\n%1").arg(path));
                });

                cardLayout->addWidget(invoiceBtn);
            }

            cardsLayout->addWidget(card);
        }
    }

    if (!anyRecord) {
        QLabel *emptyLbl = new QLabel("Aucune transaction trouvée.");
        emptyLbl->setStyleSheet(QString("QLabel { color: %1; font-size: 14px; }").arg(theme->secondaryTextColor()));
        emptyLbl->setAlignment(Qt::AlignCenter);
        cardsLayout->addWidget(emptyLbl);
    }

    cardsLayout->addStretch();
    scroll->setWidget(scrollContent);
    mainLayout->addWidget(scroll);

    // Close button
    QPushButton *closeBtn = new QPushButton("Fermer");
    closeBtn->setStyleSheet(
        "QPushButton { background-color: #14B8A6; color: white; font-weight: bold; border: none;"
        " border-radius: 8px; padding: 12px 30px; }"
        "QPushButton:hover { background-color: #0F9D8E; }"
    );
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);
    mainLayout->addWidget(closeBtn, 0, Qt::AlignRight);

    dialog->exec();
}
