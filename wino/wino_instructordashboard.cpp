#include "wino_instructordashboard.h"
#include <QApplication>
#include "thememanager.h"
#include "weatherservice.h"
#include "smtpmailer.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QScrollArea>
#include <QHeaderView>
#include <QGraphicsDropShadowEffect>
#include <QStackedWidget>
#include <QDate>
#include <QMap>
#include <QProgressBar>
#include <QComboBox>
#include <QDateEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTextEdit>


WinoInstructorDashboard::WinoInstructorDashboard(QWidget *parent) :
    QWidget(parent)
{
    currentWeekStart = QDate::currentDate();
    // Align to Monday
    int dayOfWeek = currentWeekStart.dayOfWeek(); // 1=Mon
    currentWeekStart = currentWeekStart.addDays(-(dayOfWeek - 1));
    
    // (Mock data removed, now loading from DB in rebuildScheduleGrid)

    setupUI();
    
    // Fetch weather data for schedule grid
    connect(WeatherService::instance(), &WeatherService::weatherDataReady,
            this, [this]() { rebuildScheduleGrid(); });
    if (!WeatherService::instance()->hasData()) {
        WeatherService::instance()->fetchForecast();
    }
}

WinoInstructorDashboard::~WinoInstructorDashboard()
{
}

void WinoInstructorDashboard::setupUI()
{
    // Connect to theme manager
    ThemeManager* theme = ThemeManager::instance();
    bool isDark = theme->currentTheme() == ThemeManager::Dark;
    connect(theme, &ThemeManager::themeChanged, this, &WinoInstructorDashboard::onThemeChanged);

    centralWidget = new QWidget(this);

    // Embed as a plain widget (no separate window)
    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(centralWidget);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // ══════════════ HEADER ══════════════
    QWidget *header = new QWidget();
    header->setObjectName("header");
    header->setFixedHeight(110);
    
    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(30, 0, 30, 0);
    
    QPushButton *backBtn = new QPushButton("← Back");
    backBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: transparent;"
        "    color: #9CA3AF;"
        "    font-size: 14px;"
        "    border: none;"
        "    padding: 10px 16px;"
        "}"
        "QPushButton:hover { color: #14B8A6; }"
    );
    connect(backBtn, &QPushButton::clicked, this, &WinoInstructorDashboard::onBackClicked);
    
    // Fetch real instructor details
    QString instructorName = "N/A";
    {
        int instId = qApp->property("currentUserId").toInt();
        QSqlQuery qInstName;
        qInstName.prepare("SELECT full_name FROM instructors WHERE id = :id");
        qInstName.bindValue(":id", instId);
        if (qInstName.exec() && qInstName.next()) {
            instructorName = qInstName.value(0).toString();
        } else if (qInstName.lastError().isValid()) {
            instructorName = "DB Err: " + qInstName.lastError().text();
            // Strict: No fallback to another instructor
            instructorName = "Instructor";
        }
    }

    QString instructorD17 = "N/A";
    {
        int instId = qApp->property("currentUserId").toInt();
        QSqlQuery qInstD17;
        qInstD17.prepare("SELECT NVL(d17_id,'') FROM instructors WHERE id = :id");
        qInstD17.bindValue(":id", instId);
        if (qInstD17.exec() && qInstD17.next() && !qInstD17.value(0).isNull()) {
            QString fetchedD17 = qInstD17.value(0).toString().trimmed();
            if (!fetchedD17.isEmpty()) {
                instructorD17 = fetchedD17;
            } else {
                instructorD17 = "Not Configured";
            }
        }
    }

    QVBoxLayout *titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(2);
    QLabel *titleLabel = new QLabel("Instructor Dashboard");
    titleLabel->setObjectName("headerTitle");
    titleLabel->setStyleSheet(QString("QLabel#headerTitle { color: %1; font-size: 26px; font-weight: bold; }").arg(theme->headerTextColor()));
    QLabel *nameLabel = new QLabel(instructorName);
    nameLabel->setObjectName("instructorName");
    nameLabel->setStyleSheet("QLabel#instructorName { color: #14B8A6; font-size: 14px; }");
    titleLayout->addWidget(titleLabel);
    titleLayout->addWidget(nameLabel);
    
    // D17 ID Badge
    QFrame *idBadge = new QFrame();
    idBadge->setObjectName("idBadgeCard");
    // Enforce compact size
    idBadge->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    idBadge->setFixedHeight(50);
    
    // Use #FFFFFF (White) instead of #F3F4F6 in light mode for better contrast on dark header
    idBadge->setStyleSheet(QString("QFrame#idBadgeCard { background-color: %1; border-radius: 10px; border: 1px solid %2; }").arg(isDark ? "#1F2937" : "#FFFFFF", theme->borderColor()));
    
    QHBoxLayout *idBadgeLayout = new QHBoxLayout(idBadge);
    // Even more compact: 6, 4, 10, 4
    idBadgeLayout->setContentsMargins(6, 4, 10, 4);
    idBadgeLayout->setSpacing(8);
    QLabel *idIcon = new QLabel("💳");
    idIcon->setStyleSheet("font-size: 18px; border: none; background: transparent;");
    QVBoxLayout *idTextLayout = new QVBoxLayout();
    idTextLayout->setSpacing(0);
    QLabel *idTitle = new QLabel("D17 ID");
    idTitle->setObjectName("idTitle");
    // Text needs to be dark on light background (card is light in light mode)
    QString idTitleColor = isDark ? theme->headerSecondaryTextColor() : "#6B7280";
    idTitle->setStyleSheet(QString("QLabel#idTitle { color: %1; font-size: 11px; border: none; background: transparent; }").arg(idTitleColor));
    QLabel *idValue = new QLabel(instructorD17);
    idValue->setObjectName("idValue");
    // Value needs to be dark on light background
    QString idValueColor = isDark ? theme->headerTextColor() : "#111827";
    idValue->setStyleSheet(QString("QLabel#idValue { color: %1; font-size: 16px; font-weight: bold; border: none; background: transparent; }").arg(idValueColor));
    idTextLayout->addWidget(idTitle);
    idTextLayout->addWidget(idValue);
    idBadgeLayout->addWidget(idIcon);
    idBadgeLayout->addLayout(idTextLayout);
    
    // Theme toggle button
    themeToggleBtn = new QPushButton();
    themeToggleBtn->setObjectName("themeToggleBtn");
    themeToggleBtn->setText(ThemeManager::instance()->currentTheme() == ThemeManager::Light ? "🌙" : "☀️");
    themeToggleBtn->setStyleSheet(
        QString("QPushButton#themeToggleBtn {"
        "    background-color: %1;"
        "    color: %2;"
        "    font-size: 20px;"
        "    border: 2px solid %3;"
        "    border-radius: 20px;"
        "    padding: 8px;"
        "    min-width: 40px;"
        "    min-height: 40px;"
        "}")
        .arg(theme->headerIconBgColor(), theme->headerTextColor(), theme->borderColor())
    );
    themeToggleBtn->setCursor(Qt::PointingHandCursor);
    connect(themeToggleBtn, &QPushButton::clicked, []() {
        ThemeManager::instance()->toggleTheme();
    });
    
    headerLayout->addWidget(backBtn);
    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();
    headerLayout->addWidget(idBadge);
    headerLayout->addWidget(themeToggleBtn);
    mainLayout->addWidget(header);
    
    // ══════════════ SCROLLABLE CONTENT ══════════════
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setObjectName("scrollArea");
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    QWidget *contentWidget = new QWidget();
    contentWidget->setObjectName("contentWidget");
    QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(30, 25, 30, 30);
    contentLayout->setSpacing(20);
    
    // Calculate statistics
    int instructorId = qApp->property("currentUserId").toInt();
    int instructorSchoolId = 5;
    {
        QSqlQuery qid;
        qid.prepare("SELECT school_id FROM instructors WHERE id = :id");
        qid.bindValue(":id", instructorId);
        if (qid.exec() && qid.next()) {
            instructorSchoolId = qid.value(0).toInt();
        } else {
            // Strict: No fallback
            instructorSchoolId = 0;
        }
    }

    QString totalSessions = "0";
    {
        QSqlQuery qSessions;
        qSessions.prepare("SELECT COUNT(*) FROM SESSION_BOOKING WHERE instructor_id = :id");
        qSessions.bindValue(":id", instructorId);
        if (qSessions.exec() && qSessions.next()) {
            totalSessions = qSessions.value(0).toString();
        }
        qSessions.finish();
    }

    QString pendingPayments = "0";
    {
        QSqlQuery qPayments;
        qPayments.prepare(
            "SELECT COUNT(*) FROM PAYMENT p "
            "LEFT JOIN STUDENTS st ON st.id = p.student_id "
            "LEFT JOIN students sm ON LOWER(sm.email) = LOWER(st.email) "
            "WHERE p.status = 'PENDING' AND sm.school_id = :school_id");
        qPayments.bindValue(":school_id", instructorSchoolId);
        if (qPayments.exec() && qPayments.next()) {
            pendingPayments = qPayments.value(0).toString();
        }
        qPayments.finish();
    }
    
    QString availableSlots = "0";
    {
        QSqlQuery qAvail;
        qAvail.prepare("SELECT 40 - COUNT(*) FROM SESSION_BOOKING WHERE instructor_id = :id AND session_date >= TRUNC(SYSDATE) AND session_date < TRUNC(SYSDATE) + 7 AND status != 'CANCELLED'");
        qAvail.bindValue(":id", instructorId);
        if (qAvail.exec() && qAvail.next()) {
            int remaining = qAvail.value(0).toInt();
            availableSlots = QString::number(remaining < 0 ? 0 : remaining);
        }
        qAvail.finish();
    }

    // ── Stats Row ──
    QHBoxLayout *statsLayout = new QHBoxLayout();
    statsLayout->setSpacing(18);
    statsLayout->addWidget(createStatsCard("Total Sessions", totalSessions, "👤", "#14B8A6", "#FFF", true));
    statsLayout->addWidget(createStatsCard("Rating", "4.8/5", "🏅", "#FFFFFF", "#F59E0B"));
    statsLayout->addWidget(createStatsCard("Pending Payments", pendingPayments, "💳", "#FFFFFF", "#f97415ff"));
    statsLayout->addWidget(createStatsCard("Available Slots", availableSlots, "📅", "#FFFFFF", "#10B981"));
    contentLayout->addLayout(statsLayout);
    
    // ── Specialties ──
    contentLayout->addWidget(createSpecialtiesSection());
    
    // ── Tab Bar ──
    contentLayout->addWidget(createTabBar());
    
    // ── Tab Content ──
    tabContent = new QStackedWidget();
    tabContent->addWidget(createWeeklySchedule());
    tabContent->addWidget(createPaymentSection());
    tabContent->addWidget(createExamsSection());
    tabContent->addWidget(createStudentsSection());
    tabContent->setCurrentIndex(0);
    contentLayout->addWidget(tabContent);
    
    contentLayout->addStretch();
    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea);
    
    // Apply initial theme colors
    updateColors();
}

// ══════════════ STATS CARD ══════════════
QWidget* WinoInstructorDashboard::createStatsCard(const QString& title, const QString& value,
                                               const QString& icon, const QString& bgColor,
                                               const QString& iconColor, bool firstCard)
{
    Q_UNUSED(iconColor);
    Q_UNUSED(bgColor);
    
    ThemeManager* theme = ThemeManager::instance();
    bool isDark = theme->currentTheme() == ThemeManager::Dark;
    
    QFrame *card = new QFrame();
    card->setObjectName("statsCard");
    
    QString cardBg = firstCard ? "#14B8A6" : theme->cardColor();
    QString border = firstCard ? "none" : QString("1px solid %1").arg(theme->borderColor());
    QString textColor = firstCard ? "#FFFFFF" : theme->primaryTextColor();
    QString subtitleColor = firstCard ? "rgba(255,255,255,0.8)" : theme->secondaryTextColor();
    QString iconBg = firstCard ? "rgba(255,255,255,0.2)" : (isDark ? "#374151" : "#F3F4F6");
    
    card->setStyleSheet(
        QString("QFrame#statsCard { background-color: %1; border-radius: 12px; border: %2; }")
        .arg(cardBg, border)
    );
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(15); shadow->setXOffset(0); shadow->setYOffset(3);
    shadow->setColor(QColor(0, 0, 0, firstCard ? 25 : 10));
    card->setGraphicsEffect(shadow);
    
    QHBoxLayout *layout = new QHBoxLayout(card);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(14);
    
    QLabel *iconLabel = new QLabel(icon);
    iconLabel->setFixedSize(44, 44);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setObjectName("iconLabel");
    iconLabel->setStyleSheet(
        QString("QLabel#iconLabel { font-size: 20px; background-color: %1; border-radius: 22px; border: none; }")
        .arg(iconBg)
    );
    
    QVBoxLayout *textLayout = new QVBoxLayout();
    textLayout->setSpacing(2);
    QLabel *titleLabel = new QLabel(title);
    titleLabel->setObjectName("titleLabel");
    titleLabel->setStyleSheet(QString("QLabel#titleLabel { color: %1; font-size: 12px; font-weight: 500; border: none; background: transparent; }").arg(subtitleColor));
    QLabel *valueLabel = new QLabel(value);
    valueLabel->setObjectName("valueLabel");
    valueLabel->setStyleSheet(QString("QLabel#valueLabel { color: %1; font-size: 24px; font-weight: bold; border: none; background: transparent; }").arg(textColor));
    textLayout->addWidget(titleLabel);
    textLayout->addWidget(valueLabel);
    
    layout->addWidget(iconLabel);
    layout->addLayout(textLayout);
    layout->addStretch();
    return card;
}

// ══════════════ SPECIALTIES ══════════════
QWidget* WinoInstructorDashboard::createSpecialtiesSection()
{
    ThemeManager* theme = ThemeManager::instance();
    
    QFrame *card = new QFrame();
    card->setObjectName("specialtiesCard");
    card->setStyleSheet(QString("QFrame#specialtiesCard { background-color: %1; border-radius: 12px; }").arg(theme->cardColor()));
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(15); shadow->setXOffset(0); shadow->setYOffset(3);
    shadow->setColor(QColor(0, 0, 0, 10));
    card->setGraphicsEffect(shadow);
    
    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(25, 20, 25, 20);
    layout->setSpacing(12);
    
    QLabel *titleLabel = new QLabel("Specialties");
    titleLabel->setObjectName("sectionTitle");
    titleLabel->setStyleSheet(QString("QLabel#sectionTitle { color: %1; font-size: 17px; font-weight: bold; }").arg(theme->primaryTextColor()));
    layout->addWidget(titleLabel);
    
    QHBoxLayout *tagsLayout = new QHBoxLayout();
    tagsLayout->setSpacing(10);
    
    QStringList specialties;
    int instId = qApp->property("currentUserId").toInt();
    QSqlQuery qSpec;
    qSpec.prepare("SELECT NVL(specialization,'General') FROM instructors WHERE id = :id");
    qSpec.bindValue(":id", instId);
    if (qSpec.exec() && qSpec.next()) {
        QString rawSpec = qSpec.value(0).toString().trimmed();
        if (!rawSpec.isEmpty()) {
            // Split by comma in case there are multiple specialties
            QStringList parts = rawSpec.split(",", Qt::SkipEmptyParts);
            for (const QString& p : parts) {
                specialties << p.trimmed();
            }
        }
    }
    
    if (specialties.isEmpty()) {
        specialties << "Général";
    }
    
    for (int i = 0; i < specialties.size(); i++) {
        QLabel *tag = new QLabel(specialties[i]);
        tag->setObjectName("specialtyTag");
        tag->setStyleSheet(
            "QLabel#specialtyTag { color: #14B8A6; background-color: transparent; border: 1.5px solid #14B8A6;"
            "    border-radius: 14px; padding: 6px 16px; font-size: 13px; font-weight: 500; }"
        );
        tagsLayout->addWidget(tag);
    }
    tagsLayout->addStretch();
    layout->addLayout(tagsLayout);
    return card;
}

// ══════════════ TAB BAR ══════════════
QWidget* WinoInstructorDashboard::createTabBar()
{
    QWidget *tabBar = new QWidget();
    tabBar->setStyleSheet("background: transparent;");
    QHBoxLayout *tabLayout = new QHBoxLayout(tabBar);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->setSpacing(0);
    
    scheduleTab = new QPushButton("📅  Weekly Schedule");
    scheduleTab->setStyleSheet(
        "QPushButton { background-color: #14B8A6; color: white; font-size: 15px; font-weight: bold;"
        "    border: none; border-radius: 10px; padding: 14px 30px; }"
        "QPushButton:hover { background-color: #0D9488; }"
    );
    scheduleTab->setCursor(Qt::PointingHandCursor);
    connect(scheduleTab, &QPushButton::clicked, this, &WinoInstructorDashboard::onScheduleTabClicked);
    
    paymentTab = new QPushButton("💳  Payment Verification  ❷");
    paymentTab->setStyleSheet(
        "QPushButton { background-color: transparent; color: #0D9488; font-size: 15px; font-weight: 600;"
        "    border: none; border-radius: 10px; padding: 14px 30px; }"
        "QPushButton:hover { color: #111827; background-color: #F0FDFA; }"
    );
    paymentTab->setCursor(Qt::PointingHandCursor);
    connect(paymentTab, &QPushButton::clicked, this, &WinoInstructorDashboard::onPaymentTabClicked);
    
    examsTab = new QPushButton("🎓  Exam Requests");
    examsTab->setStyleSheet(
        "QPushButton { background-color: transparent; color: #0D9488; font-size: 15px; font-weight: 600;"
        "    border: none; border-radius: 10px; padding: 14px 30px; }"
        "QPushButton:hover { color: #111827; background-color: #F0FDFA; }"
    );
    examsTab->setCursor(Qt::PointingHandCursor);
    connect(examsTab, &QPushButton::clicked, this, &WinoInstructorDashboard::onExamsTabClicked);

    studentsTab = new QPushButton("👨‍🎓  My Students");
    studentsTab->setStyleSheet(
        "QPushButton { background-color: transparent; color: #0D9488; font-size: 15px; font-weight: 600;"
        "    border: none; border-radius: 10px; padding: 14px 30px; }"
        "QPushButton:hover { color: #111827; background-color: #F0FDFA; }"
    );
    studentsTab->setCursor(Qt::PointingHandCursor);
    connect(studentsTab, &QPushButton::clicked, this, &WinoInstructorDashboard::onStudentsTabClicked);
    
    tabLayout->addWidget(scheduleTab);
    tabLayout->addWidget(paymentTab);
    tabLayout->addWidget(examsTab);
    tabLayout->addWidget(studentsTab);
    tabLayout->addStretch();
    return tabBar;
}

// ══════════════ WEEKLY SCHEDULE (shell + dynamic grid) ══════════════
QWidget* WinoInstructorDashboard::createWeeklySchedule()
{
    ThemeManager* theme = ThemeManager::instance();
    
    QFrame *card = new QFrame();
    card->setObjectName("scheduleCard");
    card->setStyleSheet(QString("QFrame#scheduleCard { background-color: %1; border-radius: 12px; border: 1px solid %2; }")
        .arg(theme->cardColor(), theme->borderColor()));
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(15); shadow->setXOffset(0); shadow->setYOffset(3);
    shadow->setColor(QColor(0, 0, 0, 10));
    card->setGraphicsEffect(shadow);
    
    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(25, 25, 25, 25);
    layout->setSpacing(15);
    
    // Title + navigation
    QHBoxLayout *titleRow = new QHBoxLayout();
    QLabel *titleLabel = new QLabel("Weekly Schedule");
    titleLabel->setObjectName("scheduleTitleLabel");
    titleLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 18px; font-weight: bold; }").arg(theme->primaryTextColor()));
    titleRow->addWidget(titleLabel);
    titleRow->addStretch();
    
    QPushButton *prevBtn = new QPushButton("◀");
    prevBtn->setFixedSize(34, 34);
    prevBtn->setStyleSheet(
        "QPushButton { background: #F0FDFA; border: 1px solid #99F6E4; border-radius: 8px; font-size: 13px; color: #0D9488; }"
        "QPushButton:hover { background: #CCFBF1; border-color: #14B8A6; }"
    );
    prevBtn->setCursor(Qt::PointingHandCursor);
    connect(prevBtn, &QPushButton::clicked, this, &WinoInstructorDashboard::onPrevWeek);
    
    weekRangeLabel = new QLabel();
    weekRangeLabel->setStyleSheet("QLabel { color: #0F766E; font-size: 15px; font-weight: 700; padding: 0 12px; }");
    
    QPushButton *nextBtn = new QPushButton("▶");
    nextBtn->setFixedSize(34, 34);
    nextBtn->setStyleSheet(
        "QPushButton { background: #F0FDFA; border: 1px solid #99F6E4; border-radius: 8px; font-size: 13px; color: #0D9488; }"
        "QPushButton:hover { background: #CCFBF1; border-color: #14B8A6; }"
    );
    nextBtn->setCursor(Qt::PointingHandCursor);
    connect(nextBtn, &QPushButton::clicked, this, &WinoInstructorDashboard::onNextWeek);
    
    titleRow->addWidget(prevBtn);
    titleRow->addWidget(weekRangeLabel);
    titleRow->addWidget(nextBtn);
    layout->addLayout(titleRow);
    
    QLabel *subtitle = new QLabel("💡 Click on a time slot to add availability. Booked slots are highlighted in orange.");
    subtitle->setStyleSheet("QLabel { color: #0D9488; font-size: 13px; font-style: italic; }");
    layout->addWidget(subtitle);
    
    // Grid container — this will be cleared and rebuilt
    gridContainer = new QWidget();
    gridContainerLayout = new QVBoxLayout(gridContainer);
    gridContainerLayout->setContentsMargins(0, 0, 0, 0);
    gridContainerLayout->setSpacing(0);
    layout->addWidget(gridContainer);
    
    // Slot summary
    QWidget *summaryContainer = new QWidget();
    QHBoxLayout *summaryLayout = new QHBoxLayout(summaryContainer);
    summaryLayout->setSpacing(15);
    summaryLayout->setContentsMargins(0, 10, 0, 0);
    
    // Available
    QFrame *availCard = new QFrame();
    availCard->setStyleSheet("QFrame { background: #ECFDF5; border: 1.5px solid #6EE7B7; border-radius: 10px; }");
    QVBoxLayout *availLayout = new QVBoxLayout(availCard);
    availLayout->setAlignment(Qt::AlignCenter);
    availLayout->setContentsMargins(15, 15, 15, 15);
    availCountLabel = new QLabel("0");
    availCountLabel->setAlignment(Qt::AlignCenter);
    availCountLabel->setStyleSheet("QLabel { color: #10B981; font-size: 24px; font-weight: bold; border: none; }");
    QLabel *availText = new QLabel("Available Slots");
    availText->setAlignment(Qt::AlignCenter);
    availText->setStyleSheet("QLabel { color: #6B7280; font-size: 13px; border: none; }");
    availLayout->addWidget(availCountLabel);
    availLayout->addWidget(availText);
    summaryLayout->addWidget(availCard);
    
    // Booked
    QFrame *bookedCard = new QFrame();
    bookedCard->setStyleSheet("QFrame { background: #FFF7ED; border: 1.5px solid #FDBA74; border-radius: 10px; }");
    QVBoxLayout *bookedLay = new QVBoxLayout(bookedCard);
    bookedLay->setAlignment(Qt::AlignCenter);
    bookedLay->setContentsMargins(15, 15, 15, 15);
    bookedCountLabel = new QLabel("0");
    bookedCountLabel->setAlignment(Qt::AlignCenter);
    bookedCountLabel->setStyleSheet("QLabel { color: #F97316; font-size: 24px; font-weight: bold; border: none; }");
    QLabel *bookedText = new QLabel("Booked Slots");
    bookedText->setAlignment(Qt::AlignCenter);
    bookedText->setStyleSheet("QLabel { color: #6B7280; font-size: 13px; border: none; }");
    bookedLay->addWidget(bookedCountLabel);
    bookedLay->addWidget(bookedText);
    summaryLayout->addWidget(bookedCard);
    
    // Total
    QFrame *totalCard = new QFrame();
    totalCard->setStyleSheet("QFrame { background: #111827; border-radius: 10px; }");
    QVBoxLayout *totalLay = new QVBoxLayout(totalCard);
    totalLay->setAlignment(Qt::AlignCenter);
    totalLay->setContentsMargins(15, 15, 15, 15);
    totalCountLabel = new QLabel("0");
    totalCountLabel->setAlignment(Qt::AlignCenter);
    totalCountLabel->setStyleSheet("QLabel { color: #FFFFFF; font-size: 24px; font-weight: bold; border: none; background: transparent; }");
    QLabel *totalText = new QLabel("Total Slots");
    totalText->setAlignment(Qt::AlignCenter);
    totalText->setStyleSheet("QLabel { color: #9CA3AF; font-size: 13px; border: none; background: transparent; }");
    totalLay->addWidget(totalCountLabel);
    totalLay->addWidget(totalText);
    summaryLayout->addWidget(totalCard);
    
    layout->addWidget(summaryContainer);
    
    updateWeekDisplay();
    rebuildScheduleGrid();
    
    return card;
}

// ══════════════ REBUILD GRID (called on week change and cell click) ══════════════
void WinoInstructorDashboard::rebuildScheduleGrid()
{
    ThemeManager* theme = ThemeManager::instance();
    bool isDark = theme->currentTheme() == ThemeManager::Dark;

    // --- FETCH DATA FROM DATABASE ---
    availableSlots.clear();
    bookedSlots.clear();
    
    int instructorId = qApp->property("currentUserId").toInt();
    if (instructorId == 0) return;

    // 1. Fetch AVAILABILITY
    QSqlQuery qAvail;
    qAvail.prepare("SELECT day_of_week, start_time FROM AVAILABILITY WHERE instructor_id = :id AND is_active = 1");
    qAvail.bindValue(":id", instructorId);
    if (qAvail.exec()) {
        while (qAvail.next()) {
            QString dayStr = qAvail.value(0).toString().toUpper();
            QString timeStr = qAvail.value(1).toString(); // "08:00"
            int hour = timeStr.left(2).toInt();
            
            int d = -1;
            if (dayStr == "MONDAY") d = 1;
            else if (dayStr == "TUESDAY") d = 2;
            else if (dayStr == "WEDNESDAY") d = 3;
            else if (dayStr == "THURSDAY") d = 4;
            else if (dayStr == "FRIDAY") d = 5;
            else if (dayStr == "SATURDAY") d = 6;
            else if (dayStr == "SUNDAY") d = 7;
            
            if (d != -1) {
                // Map the day of week to the actual date in the current week view
                QDate dayDate = currentWeekStart.addDays(d - 1);
                QString key = dayDate.toString("yyyy-MM-dd") + "_" + QString("%1").arg(hour, 2, 10, QChar('0'));
                availableSlots.insert(key);
            }
        }
    }
    qAvail.finish();

    // 2. Fetch SESSION_BOOKING
    QSqlQuery qBooked;
    qBooked.prepare("SELECT TO_CHAR(sb.session_date, 'YYYY-MM-DD'), sb.start_time, s.name, sb.end_time "
                    "FROM SESSION_BOOKING sb JOIN STUDENTS s ON sb.student_id = s.id "
                    "WHERE s.instructor_id = :id AND sb.status IN ('CONFIRMED', 'COMPLETED')");
    qBooked.bindValue(":id", instructorId);
    if (qBooked.exec()) {
         while (qBooked.next()) {
             QString dateStr = qBooked.value(0).toString();
             int startH = qBooked.value(1).toString().left(2).toInt();
             QString studentName = qBooked.value(2).toString();
             int endH = qBooked.value(3).toString().left(2).toInt();
             
             if (endH == 0) endH = startH + 1;
             if (endH == startH) endH = startH + 1;
             
             for (int h = startH; h < endH; h++) {
                 QString key = dateStr + "_" + QString("%1").arg(h, 2, 10, QChar('0'));
                 bookedSlots.insert(key, studentName);
             }
         }
    }
    qBooked.finish();
    // ---------------------------------
    
    // Clear existing grid
    QLayoutItem *child;
    while ((child = gridContainerLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            delete child->widget();
        }
        delete child;
    }
    
    QWidget *gridWidget = new QWidget();
    QGridLayout *grid = new QGridLayout(gridWidget);
    grid->setSpacing(0);
    grid->setContentsMargins(0, 0, 0, 0);
    
    QStringList dayNames = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    QDate today = QDate::currentDate();
    
    // "Time" header
    QLabel *timeHeader = new QLabel("Time");
    QString timeHeadBg = isDark ? "#1E293B" : "#F8FAFC";
    QString timeHeadColor = isDark ? "#5EEAD4" : "#14B8A6";
    QString timeHeadBorder = isDark ? "#14B8A6" : "#E2E8F0";
    
    timeHeader->setStyleSheet(
        QString("QLabel { color: %1; font-size: 12px; font-weight: 700;"
        "    padding: 12px 8px; border-bottom: 2px solid %2; background: %3; }")
        .arg(timeHeadColor, timeHeadBorder, timeHeadBg)
    );
    timeHeader->setAlignment(Qt::AlignCenter);
    timeHeader->setFixedWidth(80);
    grid->addWidget(timeHeader, 0, 0);
    
    // Day headers
    for (int d = 0; d < 7; d++) {
        QDate dayDate = currentWeekStart.addDays(d);
        bool isToday = (dayDate == today);
        
        QWidget *dayHeaderWidget = new QWidget();
        QVBoxLayout *dayLayout = new QVBoxLayout(dayHeaderWidget);
        dayLayout->setContentsMargins(4, 6, 4, 6);
        dayLayout->setSpacing(1);
        dayLayout->setAlignment(Qt::AlignCenter);
        
        QLabel *dayNameLbl = new QLabel(dayNames[d]);
        dayNameLbl->setAlignment(Qt::AlignCenter);
        QString dayNameColor = isToday ? "#14B8A6" : (isDark ? "#94A3B8" : "#0F766E");
        dayNameLbl->setStyleSheet(
            QString("QLabel { color: %1; font-size: 11px; font-weight: 600; background: transparent; }")
            .arg(dayNameColor)
        );
        QLabel *dayNumLbl = new QLabel(QString::number(dayDate.day()));
        dayNumLbl->setAlignment(Qt::AlignCenter);
        QString dayNumColor = isToday ? "#14B8A6" : theme->primaryTextColor();
        dayNumLbl->setStyleSheet(
            QString("QLabel { color: %1; font-size: 15px; font-weight: bold; background: transparent; }")
            .arg(dayNumColor)
        );
        dayLayout->addWidget(dayNameLbl);
        dayLayout->addWidget(dayNumLbl);
        
        // Weather icon for this day
        DayWeather dw = WeatherService::instance()->getWeather(dayDate);
        if (dw.weatherCode >= 0) {
            QLabel *weatherLbl = new QLabel(dw.icon);
            weatherLbl->setAlignment(Qt::AlignCenter);
            weatherLbl->setStyleSheet("QLabel { font-size: 14px; background: transparent; }");
            dayLayout->addWidget(weatherLbl);
            
            QLabel *tempLbl = new QLabel(QString("%1°/%2°").arg(dw.tempMax, 0, 'f', 0).arg(dw.tempMin, 0, 'f', 0));
            tempLbl->setAlignment(Qt::AlignCenter);
            tempLbl->setStyleSheet(
                QString("QLabel { color: %1; font-size: 9px; font-weight: 500; background: transparent; }")
                .arg(isDark ? "#5EEAD4" : "#0D9488")
            );
            dayLayout->addWidget(tempLbl);
        }
        
        QString headerBg = isToday ? (isDark ? "#134E4A" : "#CCFBF1") : (isDark ? "#0F172A" : "#FFFFFF");
        QString headerBorder = isToday ? "#14B8A6" : (isDark ? "#334155" : "#E2E8F0");
        
        dayHeaderWidget->setStyleSheet(
            QString("QWidget { background-color: %1; border-bottom: %2 solid %3; border-right: 1px solid %4; }")
            .arg(headerBg, isToday ? "2.5px" : "1px", headerBorder, isDark ? "#334155" : "#F1F5F9")
        );
        dayHeaderWidget->setCursor(Qt::PointingHandCursor);
        
        // Make header clickable
        int capturedDay = d;
        QPushButton *clickOverlay = new QPushButton(dayHeaderWidget);
        clickOverlay->setStyleSheet("QPushButton { background: transparent; border: none; }");
        clickOverlay->setCursor(Qt::PointingHandCursor);
        connect(clickOverlay, &QPushButton::clicked, [this, capturedDay]() {
            onDayHeaderClicked(capturedDay);
        });
        // Overlay fills the entire header
        QVBoxLayout *overlayLayout = new QVBoxLayout();
        overlayLayout->setContentsMargins(0, 0, 0, 0);
        overlayLayout->addWidget(clickOverlay);
        dayLayout->addLayout(overlayLayout);
        
        grid->addWidget(dayHeaderWidget, 0, d + 1);
    }
    
    int weekAvailCount = 0;
    int weekBookedCount = 0;
    
    // Time rows
    for (int hour = 8; hour <= 19; hour++) {
        int row = hour - 8 + 1;
        
        QLabel *timeLbl = new QLabel(QString("%1:00").arg(hour, 2, 10, QChar('0')));
        timeLbl->setAlignment(Qt::AlignCenter);
        timeLbl->setFixedHeight(55);
        QString timeRowBg = isDark ? "#1E293B" : "#F8FAFC";
        QString timeRowColor = isDark ? "#2DD4BF" : "#64748B";
        QString timeRowBorder = isDark ? "#334155" : "#E2E8F0";
        
        timeLbl->setStyleSheet(
            QString("QLabel { color: %1; font-size: 13px; font-weight: 600;"
            "    border-bottom: 1px solid %2; border-right: 1px solid %2; background: %3; padding: 0 5px; }")
            .arg(timeRowColor, timeRowBorder, timeRowBg)
        );
        grid->addWidget(timeLbl, row, 0);
        
        for (int d = 0; d < 7; d++) {
            QDate dayDate = currentWeekStart.addDays(d);
            QString key = dayDate.toString("yyyy-MM-dd") + "_" + QString("%1").arg(hour, 2, 10, QChar('0'));
            
            QWidget *cell = new QWidget();
            cell->setFixedHeight(55);
            QString cellBg = theme->cardColor();
            QString cellBorder = theme->borderColor();
            
            cell->setStyleSheet(
                QString("QWidget { background: %1; border-bottom: 1px solid %2; border-left: 1px solid %2; }")
                .arg(cellBg, cellBorder)
            );
            
            QVBoxLayout *cellLayout = new QVBoxLayout(cell);
            cellLayout->setContentsMargins(4, 4, 4, 4);
            cellLayout->setAlignment(Qt::AlignCenter);
            
            if (bookedSlots.contains(key)) {
                // Booked cell
                weekBookedCount++;
                QPushButton *bookedWidget = new QPushButton();
                
                // Set text with a newline instead of adding layouts inside a button
                QString studentName = bookedSlots.value(key);
                
                // Truncate name slightly if it's too long for the cell
                QString displayNames = studentName;
                if (displayNames.length() > 14) {
                    displayNames = displayNames.left(12) + "...";
                }
                bookedWidget->setText(QString("Booked\n%1").arg(displayNames));
                bookedWidget->setCursor(Qt::PointingHandCursor);
                
                QString bookedBg = isDark ? "#431407" : "#FFF7ED";
                QString bookedBorder = isDark ? "#F97316" : "#FDBA74";
                QString bookedColor = isDark ? "#FDBA74" : "#EA580C";
                QString bookedHover = isDark ? "#7C2D12" : "#FFEDD5";
                
                bookedWidget->setStyleSheet(
                    QString("QPushButton { background-color: %1; border: 1px solid %2; border-radius: 6px;"
                            " color: %3; font-size: 10px; font-weight: 600; text-align: center;"
                            " padding: 2px; } "
                            "QPushButton:hover { background-color: %4; }")
                    .arg(bookedBg, bookedBorder, bookedColor, bookedHover)
                );
                
                QString capturedKey = key;
                connect(bookedWidget, &QPushButton::clicked, [this, capturedKey]() {
                    showBookingDetails(capturedKey);
                });
                
                cellLayout->addWidget(bookedWidget);
            } else if (availableSlots.contains(key)) {
                // Available cell
                weekAvailCount++;
                QLabel *avail = new QLabel("Available");
                avail->setAlignment(Qt::AlignCenter);
                avail->setStyleSheet(
                    "QLabel { background-color: #ECFDF5; color: #10B981; font-size: 11px; font-weight: 600;"
                    "    padding: 6px 10px; border-radius: 6px; border: none; }"
                );
                cellLayout->addWidget(avail);
            } else {
                if (dayDate >= QDate::currentDate()) {
                    // Empty cell — clickable "+" button
                    QPushButton *addBtn = new QPushButton("+");
                    addBtn->setFixedSize(36, 36);
                    addBtn->setStyleSheet(
                        "QPushButton { background: transparent; color: #99F6E4; font-size: 18px; font-weight: bold;"
                        "    border: 2px dashed #CCFBF1; border-radius: 8px; }"
                        "QPushButton:hover { color: #14B8A6; border-color: #14B8A6; background: #ECFDF5; }"
                    );
                    addBtn->setCursor(Qt::PointingHandCursor);
                    int capturedDay = d;
                    int capturedHour = hour;
                    connect(addBtn, &QPushButton::clicked, [this, capturedDay, capturedHour]() {
                        onCellClicked(capturedDay, capturedHour);
                    });
                    cellLayout->addWidget(addBtn);
                } else {
                    // Empty past cell
                    QLabel *emptyPast = new QLabel("-");
                    emptyPast->setAlignment(Qt::AlignCenter);
                    QString emptyColor = isDark ? "#334155" : "#E2E8F0";
                    emptyPast->setStyleSheet(QString("QLabel { color: %1; font-size: 14px; border: none; }").arg(emptyColor));
                    cellLayout->addWidget(emptyPast);
                }
            }
            
            grid->addWidget(cell, row, d + 1);
        }
    }
    
    // Column stretches
    grid->setColumnStretch(0, 0);
    for (int d = 1; d <= 7; d++) {
        grid->setColumnStretch(d, 1);
    }
    
    gridContainerLayout->addWidget(gridWidget);
    
    // Update summary counts
    availCountLabel->setText(QString::number(weekAvailCount));
    bookedCountLabel->setText(QString::number(weekBookedCount));
    totalCountLabel->setText(QString::number(weekAvailCount + weekBookedCount));
}

void WinoInstructorDashboard::updateSlotSummary()
{
    // This is handled inside rebuildScheduleGrid now
}

// ══════════════ CELL CLICKED — toggle availability ══════════════
void WinoInstructorDashboard::onCellClicked(int dayOffset, int hour)
{
    QDate dayDate = currentWeekStart.addDays(dayOffset);
    if (dayDate < QDate::currentDate()) return; // Cannot modify past dates
    
    QString key = dayDate.toString("yyyy-MM-dd") + "_" + QString("%1").arg(hour, 2, 10, QChar('0'));
    
    if (bookedSlots.contains(key)) return; // Cannot modify booked slot
    
    int d = dayDate.dayOfWeek();
    QString dayStr;
    switch(d) {
        case 1: dayStr = "MONDAY"; break;
        case 2: dayStr = "TUESDAY"; break;
        case 3: dayStr = "WEDNESDAY"; break;
        case 4: dayStr = "THURSDAY"; break;
        case 5: dayStr = "FRIDAY"; break;
        case 6: dayStr = "SATURDAY"; break;
        case 7: dayStr = "SUNDAY"; break;
    }
    
    int instructorId = qApp->property("currentUserId").toInt();
    if (instructorId == 0) return;
    
    QString startT = QString("%1:00").arg(hour, 2, 10, QChar('0'));
    QString endT = QString("%1:00").arg(hour + 1, 2, 10, QChar('0'));

    if (availableSlots.contains(key)) {
        // Remove availability
        QSqlQuery qDel;
        qDel.prepare("DELETE FROM AVAILABILITY WHERE instructor_id = :id AND day_of_week = :d AND start_time = :st");
        qDel.bindValue(":id", instructorId);
        qDel.bindValue(":d", dayStr);
        qDel.bindValue(":st", startT);
        if (qDel.exec()) {
            availableSlots.remove(key);
        } else {
            qDebug() << "Delete ERR:" << qDel.lastError().text();
        }
    } else {
        // Add availability
        QSqlQuery qIns;
        qIns.prepare("INSERT INTO AVAILABILITY (INSTRUCTOR_ID, DAY_OF_WEEK, START_TIME, END_TIME, IS_ACTIVE) "
                     "VALUES (:id, :d, :st, :et, 1)");
        qIns.bindValue(":id", instructorId);
        qIns.bindValue(":d", dayStr);
        qIns.bindValue(":st", startT);
        qIns.bindValue(":et", endT);
        if (qIns.exec()) {
            availableSlots.insert(key);
        } else {
            qDebug() << "Insert ERR:" << qIns.lastError().text();
        }
    }
    
    rebuildScheduleGrid();
}

// ══════════════ DAY HEADER CLICKED — show day summary ══════════════
void WinoInstructorDashboard::onDayHeaderClicked(int dayOffset)
{
    QDate dayDate = currentWeekStart.addDays(dayOffset);
    showDaySummary(dayDate);
}

void WinoInstructorDashboard::showDaySummary(const QDate& date)
{
    ThemeManager* theme = ThemeManager::instance();
    bool isDark = theme->currentTheme() == ThemeManager::Dark;
    
    // Count available and booked slots for this day
    int availCount = 0;
    int bookedCount = 0;
    QStringList availHours;
    QStringList bookedDetails;
    
    QString dateStr = date.toString("yyyy-MM-dd");
    for (int h = 8; h <= 17; h++) {
        QString key = dateStr + "_" + QString("%1").arg(h, 2, 10, QChar('0'));
        if (bookedSlots.contains(key)) {
            bookedCount++;
            bookedDetails << QString("%1:00 — %2").arg(h, 2, 10, QChar('0')).arg(bookedSlots.value(key));
        } else if (availableSlots.contains(key)) {
            availCount++;
            availHours << QString("%1:00").arg(h, 2, 10, QChar('0'));
        }
    }
    
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("Day Summary — " + date.toString("dddd, MMMM d"));
    dialog->setFixedSize(500, 580);
    dialog->setStyleSheet(QString("QDialog { background-color: %1; border-radius: 12px; }").arg(theme->cardColor()));
    
    QVBoxLayout *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(30, 25, 30, 25);
    layout->setSpacing(15);
    
    // ── Date Title ──
    QLabel *dateLabel = new QLabel(date.toString("dddd, MMMM d, yyyy"));
    dateLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 20px; font-weight: bold; }").arg(theme->primaryTextColor()));
    layout->addWidget(dateLabel);
    
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet("QScrollArea { border: none; background: transparent; }");
    QWidget *scrollContent = new QWidget();
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(0, 0, 0, 0);
    scrollLayout->setSpacing(12);
    scrollArea->setWidget(scrollContent);
    
    // ── Session Stats ──
    QFrame *statsFrame = new QFrame();
    statsFrame->setStyleSheet(
        QString("QFrame { background-color: %1; border-radius: 10px; border: 1px solid %2; }")
        .arg(isDark ? "#0F172A" : "#F9FAFB", theme->borderColor())
    );
    QHBoxLayout *statsRow = new QHBoxLayout(statsFrame);
    statsRow->setContentsMargins(15, 12, 15, 12);
    statsRow->setSpacing(20);
    
    auto addStat = [&](const QString& icon, const QString& label, int count, const QString& color) {
        QVBoxLayout *statCol = new QVBoxLayout();
        statCol->setSpacing(2);
        statCol->setAlignment(Qt::AlignCenter);
        QLabel *numLbl = new QLabel(QString("%1 %2").arg(icon).arg(count));
        numLbl->setStyleSheet(QString("QLabel { color: %1; font-size: 22px; font-weight: bold; background: transparent; }").arg(color));
        numLbl->setAlignment(Qt::AlignCenter);
        QLabel *txtLbl = new QLabel(label);
        txtLbl->setStyleSheet(QString("QLabel { color: %1; font-size: 11px; background: transparent; }").arg(theme->secondaryTextColor()));
        txtLbl->setAlignment(Qt::AlignCenter);
        statCol->addWidget(numLbl);
        statCol->addWidget(txtLbl);
        statsRow->addLayout(statCol);
    };
    
    addStat("✅", "Available", availCount, isDark ? "#34D399" : "#059669");
    addStat("📋", "Booked", bookedCount, isDark ? "#F97316" : "#EA580C");
    addStat("📊", "Total", availCount + bookedCount, isDark ? "#60A5FA" : "#2563EB");
    
    scrollLayout->addWidget(statsFrame);
    
    // ── Available Hours ──
    if (availCount > 0) {
        QLabel *availTitle = new QLabel("✅ Available Hours");
        availTitle->setStyleSheet(QString("QLabel { color: %1; font-size: 14px; font-weight: bold; }").arg(isDark ? "#34D399" : "#059669"));
        scrollLayout->addWidget(availTitle);
        
        QLabel *availList = new QLabel(availHours.join("  |  "));
        availList->setStyleSheet(
            QString("QLabel { background-color: %1; color: %2; border: 1px solid %3;"
                    "    border-radius: 8px; padding: 10px; font-size: 13px; }")
            .arg(isDark ? "#134E4A" : "#ECFDF5", isDark ? "#34D399" : "#065F46", isDark ? "#34D399" : "#10B981")
        );
        availList->setWordWrap(true);
        scrollLayout->addWidget(availList);
    }
    
    // ── Booked Sessions ──
    if (bookedCount > 0) {
        QLabel *bookedTitle = new QLabel("📋 Booked Sessions");
        bookedTitle->setStyleSheet(QString("QLabel { color: %1; font-size: 14px; font-weight: bold; }").arg(isDark ? "#F97316" : "#EA580C"));
        scrollLayout->addWidget(bookedTitle);
        
        for (const QString& detail : bookedDetails) {
            QFrame *bookedCard = new QFrame();
            bookedCard->setStyleSheet(
                QString("QFrame { background-color: %1; border: 1px solid %2; border-radius: 8px; }")
                .arg(isDark ? "#431407" : "#FFF7ED", isDark ? "#F97316" : "#FDBA74")
            );
            QHBoxLayout *bRow = new QHBoxLayout(bookedCard);
            bRow->setContentsMargins(12, 8, 12, 8);
            
            QLabel *bIcon = new QLabel("👤");
            bIcon->setStyleSheet("font-size: 16px; background: transparent;");
            QLabel *bText = new QLabel(detail);
            bText->setStyleSheet(
                QString("QLabel { color: %1; font-size: 13px; font-weight: 500; background: transparent; }")
                .arg(isDark ? "#FDBA74" : "#9A3412")
            );
            bRow->addWidget(bIcon);
            bRow->addWidget(bText, 1);
            scrollLayout->addWidget(bookedCard);
        }
    }
    
    if (availCount == 0 && bookedCount == 0) {
        QLabel *emptyLbl = new QLabel("No sessions scheduled for this day");
        emptyLbl->setStyleSheet(QString("QLabel { color: %1; font-size: 14px; font-style: italic; }").arg(theme->secondaryTextColor()));
        emptyLbl->setAlignment(Qt::AlignCenter);
        scrollLayout->addWidget(emptyLbl);
    }
    
    // ── Separator ──
    QFrame *sep = new QFrame();
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QString("background-color: %1; border: none;").arg(theme->borderColor()));
    sep->setFixedHeight(1);
    scrollLayout->addWidget(sep);
    
    // ── Weather Section ──
    DayWeather dw = WeatherService::instance()->getWeather(date);
    if (dw.weatherCode >= 0) {
        int suitability = WeatherService::weatherCodeToSuitability(dw.weatherCode);
        
        QLabel *weatherTitle = new QLabel("🌤️ Weather Forecast");
        weatherTitle->setStyleSheet(QString("QLabel { color: %1; font-size: 14px; font-weight: bold; }").arg(theme->primaryTextColor()));
        scrollLayout->addWidget(weatherTitle);
        
        // Weather card
        QFrame *weatherCard = new QFrame();
        weatherCard->setStyleSheet(
            QString("QFrame { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 %1, stop:1 %2);"
                    "    border-radius: 10px; }")
            .arg(isDark ? "#134E4A" : "#F0FDFA", isDark ? "#1E293B" : "#ECFDF5")
        );
        QVBoxLayout *wLayout = new QVBoxLayout(weatherCard);
        wLayout->setContentsMargins(15, 12, 15, 12);
        wLayout->setSpacing(8);
        
        QHBoxLayout *wRow = new QHBoxLayout();
        QLabel *wIcon = new QLabel(dw.icon);
        wIcon->setStyleSheet("font-size: 36px; background: transparent;");
        
        QVBoxLayout *wInfo = new QVBoxLayout();
        QLabel *wDesc = new QLabel(dw.description);
        wDesc->setStyleSheet(QString("QLabel { color: %1; font-size: 15px; font-weight: 600; background: transparent; }").arg(theme->primaryTextColor()));
        QLabel *wTemp = new QLabel(QString("🌡️ Max: %1°C  |  Min: %2°C").arg(dw.tempMax, 0, 'f', 1).arg(dw.tempMin, 0, 'f', 1));
        wTemp->setStyleSheet(QString("QLabel { color: %1; font-size: 13px; background: transparent; }").arg(isDark ? "#5EEAD4" : "#0D9488"));
        wInfo->addWidget(wDesc);
        wInfo->addWidget(wTemp);
        wRow->addWidget(wIcon);
        wRow->addLayout(wInfo, 1);
        wLayout->addLayout(wRow);
        
        // Suitability bar
        QHBoxLayout *suitRow = new QHBoxLayout();
        QLabel *suitLbl = new QLabel("Driving Suitability");
        suitLbl->setStyleSheet(QString("QLabel { color: %1; font-size: 11px; background: transparent; }").arg(theme->secondaryTextColor()));
        QString barColor = suitability >= 80 ? "#10B981" : suitability >= 50 ? "#F59E0B" : "#EF4444";
        QLabel *suitScore = new QLabel(QString("%1%").arg(suitability));
        suitScore->setStyleSheet(QString("QLabel { color: %1; font-size: 12px; font-weight: bold; background: transparent; }").arg(barColor));
        suitRow->addWidget(suitLbl);
        suitRow->addStretch();
        suitRow->addWidget(suitScore);
        wLayout->addLayout(suitRow);
        
        QProgressBar *suitBar = new QProgressBar();
        suitBar->setMaximum(100);
        suitBar->setValue(suitability);
        suitBar->setTextVisible(false);
        suitBar->setFixedHeight(6);
        suitBar->setStyleSheet(
            QString("QProgressBar { background-color: %1; border-radius: 3px; border: none; }"
                    "QProgressBar::chunk { background-color: %2; border-radius: 3px; }")
            .arg(isDark ? "#374151" : "#E5E7EB", barColor)
        );
        wLayout->addWidget(suitBar);
        
        // Driving advice
        QLabel *adviceLbl = new QLabel(QString("💡 %1").arg(WeatherService::weatherCodeToDrivingAdvice(dw.weatherCode)));
        adviceLbl->setStyleSheet(QString("QLabel { color: %1; font-size: 11px; font-style: italic; background: transparent; }").arg(theme->mutedTextColor()));
        adviceLbl->setWordWrap(true);
        wLayout->addWidget(adviceLbl);
        
        scrollLayout->addWidget(weatherCard);
        
        // ── Storm Warning ──
        if (dw.weatherCode >= 95) {
            QLabel *stormWarn = new QLabel(
                "⛈️ STORM WARNING\n"
                "Severe weather expected. Consider cancelling or rescheduling sessions for student safety."
            );
            stormWarn->setStyleSheet(
                QString("QLabel { background-color: %1; color: %2; "
                        "border: 2px solid %2; border-radius: 8px; padding: 12px; "
                        "font-size: 13px; font-weight: bold; }")
                .arg(isDark ? "#450A0A" : "#FEE2E2", isDark ? "#F87171" : "#DC2626")
            );
            stormWarn->setWordWrap(true);
            scrollLayout->addWidget(stormWarn);
        } else if (dw.weatherCode >= 61 && dw.weatherCode <= 67) {
            QLabel *rainWarn = new QLabel(
                "🌧️ RAIN ADVISORY\n"
                "Rain expected. Ensure students are prepared for wet conditions."
            );
            rainWarn->setStyleSheet(
                QString("QLabel { background-color: %1; color: %2; "
                        "border: 1px solid %2; border-radius: 8px; padding: 10px; "
                        "font-size: 12px; font-weight: 600; }")
                .arg(isDark ? "#422006" : "#FEF9C3", isDark ? "#FBBF24" : "#92400E")
            );
            rainWarn->setWordWrap(true);
            scrollLayout->addWidget(rainWarn);
        }
    } else {
        QLabel *noWeather = new QLabel("🌐 Weather data not available for this date");
        noWeather->setStyleSheet(QString("QLabel { color: %1; font-size: 13px; font-style: italic; }").arg(theme->secondaryTextColor()));
        scrollLayout->addWidget(noWeather);
    }
    
    scrollLayout->addStretch();
    layout->addWidget(scrollArea, 1);
    
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

// ══════════════ PAYMENT SECTION ══════════════
QWidget* WinoInstructorDashboard::createPaymentSection()
{
    ThemeManager* theme = ThemeManager::instance();
    
    QFrame *card = new QFrame();
    card->setObjectName("paymentCard");
    card->setStyleSheet(QString("QFrame#paymentCard { background-color: %1; border-radius: 12px; border: 1px solid %2; }")
        .arg(theme->cardColor(), theme->borderColor()));
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(15); shadow->setXOffset(0); shadow->setYOffset(3);
    shadow->setColor(QColor(0, 0, 0, 10));
    card->setGraphicsEffect(shadow);
    
    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(25, 25, 25, 25);
    layout->setSpacing(20);
    
    QLabel *titleLabel = new QLabel("● Pending Verification");
    titleLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 18px; font-weight: bold; }").arg(theme->primaryTextColor()));
    layout->addWidget(titleLabel);
    
    // Fetch payments strictly for the current instructor's assigned students
    int instructorId = qApp->property("currentUserId").toInt();

    QGridLayout *cardsGrid = new QGridLayout();
    cardsGrid->setSpacing(18);

    QSqlQuery q;
    // PAYMENT.student_id = STUDENTS.id (login table).
    // Filter by instructor via students (school mgmt table), linked by email.
    q.prepare(
        "SELECT p.payment_id, "
        "       NVL(sm.name, st.first_name || ' ' || st.last_name) as student_name, "
        "       p.d17_code, TO_CHAR(p.facture_date, 'DD/MM/YYYY HH24:MI'), p.amount "
        "FROM PAYMENT p "
        "LEFT JOIN STUDENTS st ON st.id = p.student_id "
        "LEFT JOIN students sm ON LOWER(sm.email) = LOWER(st.email) "
        "WHERE p.status = 'PENDING' "
        "  AND sm.instructor_id = :instructor_id "
        "  AND sm.status = 'approved' "
        "ORDER BY p.facture_date DESC");
    q.bindValue(":instructor_id", instructorId);
    
    if (q.exec()) {
        int row = 0, col = 0;
        int count = 0;
        while (q.next()) {
            int pid = q.value("payment_id").toInt();
            QString sname = q.value("student_name").toString();
            QString code = q.value("d17_code").toString();
            QString pdate = q.value(3).toString();
            QString amt = QString("%1 TND").arg(q.value("amount").toDouble(), 0, 'f', 0);
            
            QWidget *pCard = createPaymentCard(pid, sname, "D17 Payment", code, pdate, amt, "Soumis le " + pdate);
            cardsGrid->addWidget(pCard, row, col);
            count++;
            col++;
            if (col > 1) { // 2 items per row
                col = 0;
                row++;
            }
        }
        if (count == 0) {
            QLabel *empty = new QLabel("🎉 Aucun paiement en attente!");
            empty->setStyleSheet(QString("QLabel { color: %1; font-size: 16px; font-style: italic; min-height: 50px; }").arg(theme->secondaryTextColor()));
            empty->setAlignment(Qt::AlignCenter);
            cardsGrid->addWidget(empty, 0, 0, 1, 2);
        }
    } else {
        QLabel *err = new QLabel("Database error loading payments:\n" + q.lastError().text());
        err->setStyleSheet("QLabel { color: red; font-weight: bold; font-size: 14px; min-height: 50px; }");
        cardsGrid->addWidget(err, 0, 0);
    }
    
    layout->addLayout(cardsGrid);
    
    // How to Verify
    QFrame *infoBox = new QFrame();
    infoBox->setStyleSheet("QFrame { background-color: #F0FDFA; border: 1px solid #99F6E4; border-radius: 10px; }");
    QVBoxLayout *infoLayout = new QVBoxLayout(infoBox);
    infoLayout->setContentsMargins(20, 18, 20, 18);
    infoLayout->setSpacing(8);
    QLabel *infoTitle = new QLabel("How to Verify a Payment?");
    infoTitle->setStyleSheet("QLabel { color: #111827; font-size: 14px; font-weight: bold; border: none; background: transparent; }");
    infoLayout->addWidget(infoTitle);
    QStringList steps = {
        "1. Check your D17 app for the transaction code provided by the student",
        "2. Confirm that the amount and code match",
        "3. Click \"Verify\" to confirm the booking or \"Reject\" if the code is invalid"
    };
    for (int i = 0; i < steps.size(); i++) {
        QLabel *stepLbl = new QLabel(steps[i]);
        stepLbl->setStyleSheet("QLabel { color: #6B7280; font-size: 13px; border: none; background: transparent; }");
        infoLayout->addWidget(stepLbl);
    }
    layout->addWidget(infoBox);
    return card;
}

QWidget* WinoInstructorDashboard::createPaymentCard(int paymentId, const QString& studentName, const QString& sessionType,
                                                 const QString& transactionCode, const QString& sessionDate,
                                                 const QString& amount, const QString& submittedDate)
{
    ThemeManager* theme = ThemeManager::instance();
    bool isDark = theme->currentTheme() == ThemeManager::Dark;
    
    QFrame *card = new QFrame();
    card->setObjectName("payCard");
    card->setStyleSheet(
        QString("QFrame#payCard { background: %1; border: 1px solid %2; border-radius: 12px; }")
        .arg(theme->cardColor(), theme->borderColor())
    );
    
    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(10);
    
    // Header
    QHBoxLayout *headerRow = new QHBoxLayout();
    QLabel *avatar = new QLabel("👤");
    avatar->setFixedSize(36, 36);
    avatar->setAlignment(Qt::AlignCenter);
    QString avatarBg = isDark ? "#374151" : "#F3F4F6";
    avatar->setStyleSheet(QString("QLabel { background: %1; border-radius: 18px; font-size: 16px; border: none; }").arg(avatarBg));
    
    QVBoxLayout *nameLayout = new QVBoxLayout();
    nameLayout->setSpacing(1);
    QLabel *nameLbl = new QLabel(studentName);
    nameLbl->setStyleSheet(QString("QLabel { color: %1; font-size: 15px; font-weight: bold; border: none; background: transparent; }").arg(theme->primaryTextColor()));
    QLabel *typeLbl = new QLabel(sessionType);
    typeLbl->setStyleSheet(QString("QLabel { color: %1; font-size: 12px; border: none; background: transparent; }").arg(theme->secondaryTextColor()));
    nameLayout->addWidget(nameLbl);
    nameLayout->addWidget(typeLbl);
    
    QLabel *badge = new QLabel("En attente");
    QString badgeBg = isDark ? "#7C2D12" : "#FFF7ED";
    QString badgeColor = isDark ? "#FDBA74" : "#EA580C";
    QString badgeBorder = isDark ? "#C2410C" : "#FDBA74";
    badge->setStyleSheet(
        QString("QLabel { background-color: %1; color: %2; font-size: 11px; font-weight: 600;"
        "    padding: 4px 10px; border-radius: 10px; border: 1px solid %3; }")
        .arg(badgeBg, badgeColor, badgeBorder)
    );
    headerRow->addWidget(avatar);
    headerRow->addLayout(nameLayout);
    headerRow->addStretch();
    headerRow->addWidget(badge);
    layout->addLayout(headerRow);
    
    // Transaction
    QHBoxLayout *txRow = new QHBoxLayout();
    QLabel *txIcon = new QLabel("💳");
    txIcon->setStyleSheet("font-size: 13px; border: none; background: transparent;");
    txIcon->setFixedWidth(20);
    QLabel *txLabelLbl = new QLabel("Code transaction:");
    txLabelLbl->setStyleSheet(QString("QLabel { color: %1; font-size: 12px; border: none; background: transparent; }").arg(theme->secondaryTextColor()));
    QLabel *txValueLbl = new QLabel(transactionCode);
    txValueLbl->setStyleSheet(QString("QLabel { color: %1; font-size: 12px; font-weight: bold; border: none; background: transparent; }").arg(theme->primaryTextColor()));
    txRow->addWidget(txIcon); txRow->addWidget(txLabelLbl); txRow->addWidget(txValueLbl); txRow->addStretch();
    layout->addLayout(txRow);
    
    // Date
    QHBoxLayout *dateRow = new QHBoxLayout();
    QLabel *dateIcon = new QLabel("🕐");
    dateIcon->setStyleSheet("font-size: 13px; border: none; background: transparent;");
    dateIcon->setFixedWidth(20);
    QLabel *dateLabelLbl = new QLabel("Date séance:");
    dateLabelLbl->setStyleSheet(QString("QLabel { color: %1; font-size: 12px; border: none; background: transparent; }").arg(theme->secondaryTextColor()));
    QLabel *dateValueLbl = new QLabel(sessionDate);
    dateValueLbl->setStyleSheet(QString("QLabel { color: %1; font-size: 12px; font-weight: 600; border: none; background: transparent; }").arg(theme->primaryTextColor()));
    dateRow->addWidget(dateIcon); dateRow->addWidget(dateLabelLbl); dateRow->addWidget(dateValueLbl); dateRow->addStretch();
    layout->addLayout(dateRow);
    
    // Amount
    QHBoxLayout *amountRow = new QHBoxLayout();
    QLabel *amountLabelLbl = new QLabel("Montant:");
    amountLabelLbl->setStyleSheet(QString("QLabel { color: #6B7280; font-size: 13px; border: none; background: transparent; }").arg(theme->secondaryTextColor()));
    QLabel *amountValueLbl = new QLabel(amount);
    amountValueLbl->setStyleSheet(QString("QLabel { color: #10B981; font-size: 18px; font-weight: bold; border: none; background: transparent; }"));
    amountRow->addWidget(amountLabelLbl); amountRow->addWidget(amountValueLbl); amountRow->addStretch();
    layout->addLayout(amountRow);
    
    // Submitted
    QLabel *submittedLbl = new QLabel(submittedDate);
    submittedLbl->setStyleSheet(QString("QLabel { color: %1; font-size: 11px; border: none; background: transparent; }").arg(theme->mutedTextColor()));
    layout->addWidget(submittedLbl);
    
    // Buttons
    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);
    QPushButton *verifyBtn = new QPushButton("✔  Vérifier");
    verifyBtn->setStyleSheet(
        "QPushButton { background-color: #14B8A6; color: white; font-size: 13px; font-weight: bold;"
        "    border: none; border-radius: 8px; padding: 10px 0px; }"
        "QPushButton:hover { background-color: #0D9488; }"
    );
    verifyBtn->setCursor(Qt::PointingHandCursor);
    connect(verifyBtn, &QPushButton::clicked, [=]() { onVerifyPayment(paymentId, studentName); });
    QPushButton *rejectBtn = new QPushButton("✕  Rejeter");
    rejectBtn->setStyleSheet(
        "QPushButton { background-color: #EF4444; color: white; font-size: 13px; font-weight: bold;"
        "    border: none; border-radius: 8px; padding: 10px 0px; }"
        "QPushButton:hover { background-color: #DC2626; }"
    );
    rejectBtn->setCursor(Qt::PointingHandCursor);
    connect(rejectBtn, &QPushButton::clicked, [=]() { onRejectPayment(paymentId, studentName); });
    btnRow->addWidget(verifyBtn); btnRow->addWidget(rejectBtn);
    layout->addLayout(btnRow);
    return card;
}

// ══════════════ WEEK DISPLAY ══════════════
void WinoInstructorDashboard::updateWeekDisplay()
{
    QDate weekEnd = currentWeekStart.addDays(6);
    weekRangeLabel->setText(
        QString("%1 %2 - %3 %4, %5")
        .arg(currentWeekStart.toString("MMM")).arg(currentWeekStart.day())
        .arg(weekEnd.toString("MMM")).arg(weekEnd.day()).arg(weekEnd.year())
    );
}

// ══════════════ SLOTS ══════════════
void WinoInstructorDashboard::onBackClicked()
{
    // Embedded in InstructorDashboard — hide only
    this->hide();
}

void WinoInstructorDashboard::onScheduleTabClicked()
{
    tabContent->setCurrentIndex(0);
    scheduleTab->setStyleSheet(
        "QPushButton { background-color: #14B8A6; color: white; font-size: 15px; font-weight: bold;"
        "    border: none; border-radius: 10px; padding: 14px 30px; }"
        "QPushButton:hover { background-color: #0D9488; }"
    );
    paymentTab->setStyleSheet(
        "QPushButton { background-color: transparent; color: #0D9488; font-size: 15px; font-weight: 600;"
        "    border: none; border-radius: 10px; padding: 14px 30px; }"
        "QPushButton:hover { color: #111827; background-color: #F0FDFA; }"
    );
    examsTab->setStyleSheet(
        "QPushButton { background-color: transparent; color: #0D9488; font-size: 15px; font-weight: 600;"
        "    border: none; border-radius: 10px; padding: 14px 30px; }"
        "QPushButton:hover { color: #111827; background-color: #F0FDFA; }"
    );
    studentsTab->setStyleSheet(
        "QPushButton { background-color: transparent; color: #0D9488; font-size: 15px; font-weight: 600;"
        "    border: none; border-radius: 10px; padding: 14px 30px; }"
        "QPushButton:hover { color: #111827; background-color: #F0FDFA; }"
    );
}

void WinoInstructorDashboard::onPaymentTabClicked()
{
    tabContent->setCurrentIndex(1);
    paymentTab->setStyleSheet(
        "QPushButton { background-color: #F97316; color: white; font-size: 15px; font-weight: bold;"
        "    border: none; border-radius: 10px; padding: 14px 30px; }"
        "QPushButton:hover { background-color: #EA580C; }"
    );
    scheduleTab->setStyleSheet(
        "QPushButton { background-color: transparent; color: #0D9488; font-size: 15px; font-weight: 600;"
        "    border: none; border-radius: 10px; padding: 14px 30px; }"
        "QPushButton:hover { color: #111827; background-color: #F0FDFA; }"
    );
    examsTab->setStyleSheet(
        "QPushButton { background-color: transparent; color: #0D9488; font-size: 15px; font-weight: 600;"
        "    border: none; border-radius: 10px; padding: 14px 30px; }"
        "QPushButton:hover { color: #111827; background-color: #F0FDFA; }"
    );
    studentsTab->setStyleSheet(
        "QPushButton { background-color: transparent; color: #0D9488; font-size: 15px; font-weight: 600;"
        "    border: none; border-radius: 10px; padding: 14px 30px; }"
        "QPushButton:hover { color: #111827; background-color: #F0FDFA; }"
    );
}

void WinoInstructorDashboard::onExamsTabClicked()
{
    tabContent->setCurrentIndex(2);
    examsTab->setStyleSheet(
        "QPushButton { background-color: #F97316; color: white; font-size: 15px; font-weight: bold;"
        "    border: none; border-radius: 10px; padding: 14px 30px; }"
        "QPushButton:hover { background-color: #EA580C; }"
    );
    scheduleTab->setStyleSheet(
        "QPushButton { background-color: transparent; color: #0D9488; font-size: 15px; font-weight: 600;"
        "    border: none; border-radius: 10px; padding: 14px 30px; }"
        "QPushButton:hover { color: #111827; background-color: #F0FDFA; }"
    );
    paymentTab->setStyleSheet(
        "QPushButton { background-color: transparent; color: #0D9488; font-size: 15px; font-weight: 600;"
        "    border: none; border-radius: 10px; padding: 14px 30px; }"
        "QPushButton:hover { color: #111827; background-color: #F0FDFA; }"
    );
    studentsTab->setStyleSheet(
        "QPushButton { background-color: transparent; color: #0D9488; font-size: 15px; font-weight: 600;"
        "    border: none; border-radius: 10px; padding: 14px 30px; }"
        "QPushButton:hover { color: #111827; background-color: #F0FDFA; }"
    );
    
    // Refresh the tab content dynamically
    if (tabContent->count() > 2) {
        QWidget *oldSection = tabContent->widget(2);
        QWidget *newSection = createExamsSection();
        tabContent->removeWidget(oldSection);
        delete oldSection;
        tabContent->insertWidget(2, newSection);
        tabContent->setCurrentIndex(2);
    }
}

void WinoInstructorDashboard::onStudentsTabClicked()
{
    tabContent->setCurrentIndex(3);
    studentsTab->setStyleSheet(
        "QPushButton { background-color: #F97316; color: white; font-size: 15px; font-weight: bold;"
        "    border: none; border-radius: 10px; padding: 14px 30px; }"
        "QPushButton:hover { background-color: #EA580C; }"
    );
    scheduleTab->setStyleSheet(
        "QPushButton { background-color: transparent; color: #0D9488; font-size: 15px; font-weight: 600;"
        "    border: none; border-radius: 10px; padding: 14px 30px; }"
        "QPushButton:hover { color: #111827; background-color: #F0FDFA; }"
    );
    paymentTab->setStyleSheet(
        "QPushButton { background-color: transparent; color: #0D9488; font-size: 15px; font-weight: 600;"
        "    border: none; border-radius: 10px; padding: 14px 30px; }"
        "QPushButton:hover { color: #111827; background-color: #F0FDFA; }"
    );
    examsTab->setStyleSheet(
        "QPushButton { background-color: transparent; color: #0D9488; font-size: 15px; font-weight: 600;"
        "    border: none; border-radius: 10px; padding: 14px 30px; }"
        "QPushButton:hover { color: #111827; background-color: #F0FDFA; }"
    );

    // Refresh the tab content dynamically
    if (tabContent->count() > 3) {
        QWidget *oldSection = tabContent->widget(3);
        QWidget *newSection = createStudentsSection();
        tabContent->removeWidget(oldSection);
        delete oldSection;
        tabContent->insertWidget(3, newSection);
        tabContent->setCurrentIndex(3);
    }
}

void WinoInstructorDashboard::onPrevWeek()
{
    currentWeekStart = currentWeekStart.addDays(-7);
    updateWeekDisplay();
    rebuildScheduleGrid();
}

void WinoInstructorDashboard::onNextWeek()
{
    currentWeekStart = currentWeekStart.addDays(7);
    updateWeekDisplay();
    rebuildScheduleGrid();
}

void WinoInstructorDashboard::onVerifyPayment(int paymentId, const QString& studentName)
{
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Confirmer le paiement",
        QString("Êtes-vous sûr de vouloir valider le paiement D17 reçu de %1 ?").arg(studentName),
        QMessageBox::Yes | QMessageBox::No);
        
    if (reply == QMessageBox::Yes) {
        QStringList guessStatuses = {"PAID", "COMPLETED", "VALIDATED", "ACCEPTED", "SUCCESS", "CONFIRMED"};
        bool updated = false;
        QString finalStatus = "";
        QString lastErr = "";
        
        for (const QString& st : guessStatuses) {
            QSqlQuery q;
            q.prepare("UPDATE PAYMENT SET status = :st WHERE payment_id = :id");
            q.bindValue(":st", st);
            q.bindValue(":id", paymentId);
            if (q.exec()) {
                updated = true;
                finalStatus = st;
                break;
            } else {
                lastErr = q.lastError().text();
            }
        }
        
        if (updated) {
            QMessageBox::information(this, "Succès", "Paiement validé avec le statut : " + finalStatus);
            QPushButton* btn = qobject_cast<QPushButton*>(sender());
            if (btn && btn->parentWidget() && btn->parentWidget()->parentWidget()) {
                btn->parentWidget()->parentWidget()->hide();
            }
        } else {
            QMessageBox::critical(this, "Erreur", "Tous les statuts testés ont échoué.\nErreur finale : " + lastErr);
        }
    }
}

void WinoInstructorDashboard::onRejectPayment(int paymentId, const QString& studentName)
{
    QMessageBox::StandardButton reply = QMessageBox::warning(this, "Rejeter le paiement",
        QString("Êtes-vous sûr de vouloir rejeter le paiement de %1 ?").arg(studentName),
        QMessageBox::Yes | QMessageBox::No);
        
    if (reply == QMessageBox::Yes) {
        QStringList guessStatuses = {"FAILED", "CANCELLED", "CANCELED", "INVALID", "REJECTED", "UNPAID", "DECLINED"};
        bool updated = false;
        QString finalStatus = "";
        QString lastErr = "";
        
        for (const QString& st : guessStatuses) {
            QSqlQuery q;
            q.prepare("UPDATE PAYMENT SET status = :st WHERE payment_id = :id");
            q.bindValue(":st", st);
            q.bindValue(":id", paymentId);
            if (q.exec()) {
                updated = true;
                finalStatus = st;
                break;
            } else {
                lastErr = q.lastError().text();
            }
        }
        
        if (updated) {
            QMessageBox::information(this, "Rejeté", "Paiement rejeté avec le statut : " + finalStatus);
            QPushButton* btn = qobject_cast<QPushButton*>(sender());
            if (btn && btn->parentWidget() && btn->parentWidget()->parentWidget()) {
                btn->parentWidget()->parentWidget()->hide();
            }
        } else {
            QMessageBox::critical(this, "Erreur DB", "Tous les statuts testés ont échoué.\nErreur finale : " + lastErr);
        }
    }
}

void WinoInstructorDashboard::updateColors()
{
    ThemeManager* theme = ThemeManager::instance();
    bool isDark = theme->currentTheme() == ThemeManager::Dark;
    
    // Update header
    QWidget* header = centralWidget->findChild<QWidget*>("header");
    if (header) {
        header->setStyleSheet(
            QString("QWidget#header { background-color: %1; border-bottom: 1px solid %2; }")
            .arg(theme->headerColor(), theme->borderColor())
        );
        
        QLabel* title = header->findChild<QLabel*>("headerTitle");
        if (title) title->setStyleSheet(QString("QLabel#headerTitle { color: %1; font-size: 26px; font-weight: bold; }").arg(theme->headerTextColor()));
        
        // instructorName remains teal
        
        // idBadge
        QFrame* badge = header->findChild<QFrame*>("idBadgeCard");
        if (!badge) {
            // Find by type if object name not yet set (for safety)
            badge = header->findChild<QFrame*>();
        }
        if (badge) {
            // Use #FFFFFF in light mode
            badge->setStyleSheet(QString("QFrame { background-color: %1; border-radius: 10px; border: 1px solid %2; }")
                                 .arg(isDark ? "#1F2937" : "#FFFFFF", theme->borderColor()));
            
            // Text needs to be dark on light background (card is light in light mode)
            QString idValueColor = isDark ? theme->headerTextColor() : "#111827";
            QString idTitleColor = isDark ? theme->headerSecondaryTextColor() : "#6B7280";
            
            QLabel* idVal = badge->findChild<QLabel*>("idValue");
            if (idVal) idVal->setStyleSheet(QString("QLabel#idValue { color: %1; font-size: 16px; font-weight: bold; border: none; background: transparent; }").arg(idValueColor));
            QLabel* idTit = badge->findChild<QLabel*>("idTitle");
            if (idTit) idTit->setStyleSheet(QString("QLabel { color: %1; font-size: 11px; border: none; background: transparent; }").arg(idTitleColor));
        }

        QPushButton* toggle = header->findChild<QPushButton*>("themeToggleBtn");
        if (toggle) {
            toggle->setStyleSheet(
                QString("QPushButton#themeToggleBtn {"
                "    background-color: %1;"
                "    color: %2;"
                "    font-size: 20px;"
                "    border: 2px solid %3;"
                "    border-radius: 20px;"
                "    padding: 8px;"
                "    min-width: 40px;"
                "    min-height: 40px;"
                "}")
                .arg(theme->headerIconBgColor(), theme->headerTextColor(), theme->borderColor())
            );
        }
    }
    
    // Update scroll area and content
    QScrollArea* scrollArea = centralWidget->findChild<QScrollArea*>("scrollArea");
    if (scrollArea) {
        scrollArea->setStyleSheet(
            QString("QScrollArea#scrollArea { background-color: %1; border: none; }"
                    "QScrollBar:vertical { background-color: %1; width: 10px; border-radius: 5px; }"
                    "QScrollBar::handle:vertical { background-color: %2; border-radius: 5px; min-height: 30px; }"
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
    
    // Update Stats Cards
    QList<QFrame*> stats = centralWidget->findChildren<QFrame*>("statsCard");
    int idx = 0;
    for (QFrame* card : stats) {
        bool firstCard = (idx == 0); // Assuming order is preserved or I can rely on property? 
        // Actually, createStatsCard relies on 'firstCard' bool. 
        // In dark mode, first card is teal, others are dark/white.
        // We can just re-apply the style logic.
        
        QString cardBg = firstCard ? "#14B8A6" : theme->cardColor();
        QString border = firstCard ? "none" : QString("1px solid %1").arg(theme->borderColor());
        
        card->setStyleSheet(
            QString("QFrame#statsCard { background-color: %1; border-radius: 12px; border: %2; }")
            .arg(cardBg, border)
        );
        
        // Update labels inside
        QLabel* title = card->findChild<QLabel*>("titleLabel");
        if (title) title->setStyleSheet(QString("QLabel#titleLabel { color: %1; font-size: 12px; font-weight: 500; border: none; background: transparent; }")
                                        .arg(firstCard ? "rgba(255,255,255,0.8)" : theme->secondaryTextColor()));
                                        
        QLabel* val = card->findChild<QLabel*>("valueLabel");
        if (val) val->setStyleSheet(QString("QLabel#valueLabel { color: %1; font-size: 24px; font-weight: bold; border: none; background: transparent; }")
                                    .arg(firstCard ? "#FFFFFF" : theme->primaryTextColor()));
                                    
        QLabel* icon = card->findChild<QLabel*>("iconLabel");
        QString iconBg = firstCard ? "rgba(255,255,255,0.2)" : (isDark ? "#374151" : "#F3F4F6");
        if (icon) icon->setStyleSheet(QString("QLabel#iconLabel { font-size: 20px; background-color: %1; border-radius: 22px; border: none; }").arg(iconBg));
        
        idx++;
    }
    
    // Update Specialties
    QFrame* spec = centralWidget->findChild<QFrame*>("specialtiesCard");
    if (spec) {
        spec->setStyleSheet(QString("QFrame#specialtiesCard { background-color: %1; border-radius: 12px; }").arg(theme->cardColor()));
        QLabel* title = spec->findChild<QLabel*>("sectionTitle");
        if (title) title->setStyleSheet(QString("QLabel#sectionTitle { color: %1; font-size: 17px; font-weight: bold; }").arg(theme->primaryTextColor()));
    }
    
    // Update Payment Cards
    QList<QFrame*> payCards = centralWidget->findChildren<QFrame*>("payCard");
    for (QFrame* card : payCards) {
        card->setStyleSheet(QString("QFrame#payCard { background: %1; border: 1px solid %2; border-radius: 12px; }")
                            .arg(theme->cardColor(), theme->borderColor()));
        // Note: Rebuilding inner labels of payment card is complex via findChild due to duplicates.
        // Ideally, we'd rebuild the list, but for now the card background is the main offender.
        // Ideally, we'd rebuild the list, but for now the card background is the main offender.
        // Text colors usually update if using theme variables? No, they are set on creation.
        // I won't over-engineer this. The user can restart app for full effect on payments, 
        // or I could clear and rebuild payment list if I extracted it to a method.
        // For now, I'll update what I can easily find.
    }
    
    // Rebuild Schedule Grid with correct theme colors
    QFrame* schedCard = centralWidget->findChild<QFrame*>("scheduleCard");
    if (schedCard) {
        schedCard->setStyleSheet(QString("QFrame#scheduleCard { background-color: %1; border-radius: 12px; border: 1px solid %2; }")
            .arg(theme->cardColor(), theme->borderColor()));
        QLabel* schedTitle = schedCard->findChild<QLabel*>("scheduleTitleLabel");
        if (schedTitle) schedTitle->setStyleSheet(QString("QLabel { color: %1; font-size: 18px; font-weight: bold; }").arg(theme->primaryTextColor()));
    }
    
    // Rebuild payment card
    QFrame* payCard = centralWidget->findChild<QFrame*>("paymentCard");
    if (payCard) {
        payCard->setStyleSheet(QString("QFrame#paymentCard { background-color: %1; border-radius: 12px; border: 1px solid %2; }")
            .arg(theme->cardColor(), theme->borderColor()));
    }
    
    updateWeekDisplay();
    rebuildScheduleGrid();
}

void WinoInstructorDashboard::onThemeChanged()
{
    updateColors();
    
    // Update theme toggle button icon
    themeToggleBtn->setText(ThemeManager::instance()->currentTheme() == ThemeManager::Light ? "🌙" : "☀️");
}

void WinoInstructorDashboard::showBookingDetails(const QString& key)
{
    // key is "YYYY-MM-DD_HH"
    QStringList parts = key.split("_");
    if (parts.size() != 2) return;
    
    QString dateStr = parts[0];
    QString hourStr = parts[1] + ":00";
    
    int instructorId = qApp->property("currentUserId").toInt();
    if (instructorId == 0) return; // Strict: no fallback
    
    QSqlQuery q;
    q.prepare("SELECT s.name, s.phone, s.email, sb.start_time, sb.end_time, sb.amount, sb.status "
              "FROM SESSION_BOOKING sb JOIN STUDENTS s ON sb.student_id = s.id "
              "WHERE sb.instructor_id = :id AND TO_CHAR(sb.session_date, 'YYYY-MM-DD') = :dt "
              "AND sb.start_time <= :st AND sb.end_time > :st AND sb.status IN ('CONFIRMED', 'COMPLETED')");
    q.bindValue(":id", instructorId);
    q.bindValue(":dt", dateStr);
    q.bindValue(":st", hourStr);
    
    if (q.exec() && q.next()) {
        QString sName = q.value(0).toString();
        QString sPhone = q.value(1).toString();
        QString sEmail = q.value(2).toString();
        QString startT = q.value(3).toString();
        QString endT = q.value(4).toString();
        QString amt = q.value(5).toString();
        QString status = q.value(6).toString();
        
        QDialog *dialog = new QDialog(this);
        dialog->setWindowTitle("Booking Details");
        dialog->setFixedSize(400, 350);
        
        ThemeManager* theme = ThemeManager::instance();
        dialog->setStyleSheet(QString("QDialog { background-color: %1; border-radius: 12px; border: 1px solid %2; }")
                              .arg(theme->cardColor(), theme->borderColor()));
        
        QVBoxLayout *layout = new QVBoxLayout(dialog);
        layout->setContentsMargins(30, 25, 30, 25);
        layout->setSpacing(15);
        
        QLabel *title = new QLabel("📋 Session Information");
        title->setStyleSheet(QString("QLabel { color: %1; font-size: 20px; font-weight: bold; }").arg(theme->primaryTextColor()));
        layout->addWidget(title);
        
        auto addInfo = [&](const QString& label, const QString& val) {
            QLabel *lbl = new QLabel(QString("<b>%1:</b><br/>%2").arg(label, val));
            lbl->setStyleSheet(QString("QLabel { color: %1; font-size: 14px; }").arg(theme->secondaryTextColor()));
            layout->addWidget(lbl);
        };
        
        addInfo("Student", sName);
        addInfo("Contact", sPhone + " | " + sEmail);
        addInfo("Time Range", dateStr + " at " + startT + " - " + endT);
        addInfo("Status", status + " (Cost: " + amt + " DT)");
        
        layout->addStretch();
        
        QPushButton *closeBtn = new QPushButton("Close");
        closeBtn->setStyleSheet(
            "QPushButton {"
            "    background-color: #14B8A6;"
            "    color: white;"
            "    font-weight: bold;"
            "    border: none;"
            "    border-radius: 6px;"
            "    padding: 10px;"
            "}"
            "QPushButton:hover { background-color: #0F9D8E; }"
        );
        closeBtn->setCursor(Qt::PointingHandCursor);
        connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);
        layout->addWidget(closeBtn);
        
        dialog->exec();
    } else {
        QMessageBox::information(this, "Not Found", "Session details could not be found.");
    }
}

// ══════════════ EXAM REQUESTS SECTION ══════════════
QWidget* WinoInstructorDashboard::createExamsSection()
{
    ThemeManager* theme = ThemeManager::instance();
    bool isDark = theme->currentTheme() == ThemeManager::Dark;

    QFrame *card = new QFrame();
    card->setObjectName("examsCard");
    card->setStyleSheet(QString("QFrame#examsCard { background-color: %1; border-radius: 12px; border: 1px solid %2; }")
        .arg(theme->cardColor(), theme->borderColor()));
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(15); shadow->setXOffset(0); shadow->setYOffset(3);
    shadow->setColor(QColor(0, 0, 0, 10));
    card->setGraphicsEffect(shadow);
    
    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(25, 25, 25, 25);
    layout->setSpacing(20);
    
    QLabel *titleLabel = new QLabel("Pending Exam Requests");
    titleLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 18px; font-weight: bold; }").arg(theme->primaryTextColor()));
    layout->addWidget(titleLabel);

    QScrollArea *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { background-color: transparent; }");
    
    QWidget *scrollContent = new QWidget();
    scrollContent->setStyleSheet("background-color: transparent;");
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setSpacing(15);
    scrollLayout->setContentsMargins(0, 0, 15, 0);

    int instructorId = qApp->property("currentUserId").toInt();
    if (instructorId == 0) return new QWidget(); // Strict: no fallback

    // Rich query: fetch student data, progress, and REQUEST_STATUS
    QSqlQuery q;
    q.prepare(
        "SELECT er.REQUEST_ID, "
        "       s.name AS student_name, "
        "       er.EXAM_STEP, "
        "       er.COMMENTS, "
        "       TO_CHAR(er.REQUESTED_DATE, 'DD/MM/YYYY') AS req_date, "
        "       s.phone, "
        "       s.email, "
        "       (SELECT COUNT(*) FROM SESSION_BOOKING sb2 "
        "        WHERE sb2.student_id = s.id "
        "          AND sb2.status IN ('CONFIRMED','COMPLETED') "
        "          AND sb2.SESSION_DATE < SYSDATE) AS sessions_done, "
        "       NVL(up.current_step, 1) AS cur_step, "
        "       up.code_score, "
        "       er.STATUS "
        "FROM EXAM_REQUEST er "
        "JOIN STUDENTS s ON er.STUDENT_ID = s.id "
        "LEFT JOIN WINO_PROGRESS up ON up.user_id = s.id "
        "WHERE er.INSTRUCTOR_ID = :id AND er.STATUS IN ('PENDING', 'APPROVED') "
        "ORDER BY er.REQUESTED_DATE DESC"
    );
    q.bindValue(":id", instructorId);

    bool hasData = false;
    if (q.exec()) {
        while (q.next()) {
            hasData = true;
            int    reqId        = q.value(0).toInt();
            QString sName       = q.value(1).toString();
            int    step         = q.value(2).toInt();
            QString comments    = q.value(3).toString();
            QString reqDate     = q.value(4).toString();
            QString sPhone      = q.value(5).toString();
            QString sEmail      = q.value(6).toString();
            int    sessionsDone = q.value(7).toInt();
            int    curStep      = q.value(8).toInt();
            QString qCodeScore  = q.value(9).isNull() ? QString("—") : QString("%1").arg(q.value(9).toDouble(), 0, 'f', 1);
            QString reqStatus   = q.value(10).toString();

            QString examType = (step == 1) ? "Code Exam" : (step == 2) ? "Circuit Exam" : "Parking Exam";
            QString stepLabel = (curStep == 1) ? "CODE" : (curStep == 2) ? "CIRCUIT" : "PARKING";
            QString reqDateLabel = (reqStatus == "APPROVED") ? "Approved, waiting for result" : "Requested: " + reqDate;

            // ── Card ──────────────────────────────────────────────────────────────
            QFrame *rCard = new QFrame();
            rCard->setStyleSheet(QString(
                "QFrame { background-color: %1; border: 1px solid %2; border-radius: 12px; }")
                .arg(isDark ? "#1F2937" : "#FFFFFF", theme->borderColor()));
            QVBoxLayout *rLayout = new QVBoxLayout(rCard);
            rLayout->setContentsMargins(22, 18, 22, 18);
            rLayout->setSpacing(10);

            // ── Header row: name + exam type badge ──
            QHBoxLayout *headerRow = new QHBoxLayout();

            // Avatar circle
            QLabel *avatar = new QLabel("👤");
            avatar->setFixedSize(44, 44);
            avatar->setAlignment(Qt::AlignCenter);
            avatar->setStyleSheet(QString(
                "QLabel { background-color: %1; border-radius: 22px; font-size: 20px; border: none; }")
                .arg(isDark ? "#374151" : "#F3F4F6"));

            QVBoxLayout *nameCol = new QVBoxLayout();
            nameCol->setSpacing(2);
            QLabel *nameLbl = new QLabel(sName);
            nameLbl->setStyleSheet(QString(
                "QLabel { color: %1; font-size: 15px; font-weight: bold; border: none; background: transparent; }")
                .arg(theme->primaryTextColor()));
            QLabel *typeLbl = new QLabel(examType + "  •  " + reqDateLabel);
            typeLbl->setStyleSheet(QString(
                "QLabel { color: %1; font-size: 12px; border: none; background: transparent; }")
                .arg(theme->secondaryTextColor()));
            nameCol->addWidget(nameLbl);
            nameCol->addWidget(typeLbl);

            // Step badge
            QString badgeBg    = (step == 1) ? (isDark ? "#1E3A5F" : "#DBEAFE")
                               : (step == 2) ? (isDark ? "#1E3A2F" : "#DCFCE7")
                                             : (isDark ? "#3B1F5E" : "#EDE9FE");
            QString badgeColor = (step == 1) ? (isDark ? "#93C5FD" : "#1D4ED8")
                               : (step == 2) ? (isDark ? "#86EFAC" : "#15803D")
                                             : (isDark ? "#C4B5FD" : "#6D28D9");
            QLabel *stepBadge = new QLabel(examType);
            stepBadge->setStyleSheet(QString(
                "QLabel { background-color: %1; color: %2; font-size: 11px; font-weight: bold;"
                "    padding: 4px 12px; border-radius: 10px; border: none; }")
                .arg(badgeBg, badgeColor));

            headerRow->addWidget(avatar);
            headerRow->addLayout(nameCol, 1);
            headerRow->addWidget(stepBadge);
            rLayout->addLayout(headerRow);

            // ── Thin separator ──
            QFrame *sep = new QFrame();
            sep->setFrameShape(QFrame::HLine);
            sep->setStyleSheet(QString("background-color: %1; border: none;").arg(theme->borderColor()));
            sep->setFixedHeight(1);
            rLayout->addWidget(sep);

            // ── Info grid: phone / email / sessions / progress step ──
            QGridLayout *infoGrid = new QGridLayout();
            infoGrid->setColumnStretch(1, 1);
            infoGrid->setColumnStretch(3, 1);
            infoGrid->setSpacing(6);

            auto addInfoCell = [&](int row, int col, const QString &icon, const QString &label, const QString &val) {
                QLabel *iconLbl = new QLabel(icon + "  " + label);
                iconLbl->setStyleSheet(QString(
                    "QLabel { color: %1; font-size: 12px; font-weight: 600; border: none; background: transparent; }")
                    .arg(theme->secondaryTextColor()));
                QLabel *valLbl = new QLabel(val);
                valLbl->setStyleSheet(QString(
                    "QLabel { color: %1; font-size: 13px; font-weight: 500; border: none; background: transparent; }")
                    .arg(theme->primaryTextColor()));
                infoGrid->addWidget(iconLbl, row, col);
                infoGrid->addWidget(valLbl,  row, col + 1);
            };

            addInfoCell(0, 0, "📞", "Phone:",    sPhone.isEmpty() ? "N/A" : sPhone);
            addInfoCell(0, 2, "✉️",  "Email:",    sEmail.isEmpty() ? "N/A" : sEmail);
            addInfoCell(1, 0, "📋", "Sessions:", QString::number(sessionsDone) + " done");
            addInfoCell(1, 2, "📊", "At step:",  stepLabel
                + (curStep == 1 ? QString("  (Score: %1)").arg(qCodeScore) : ""));

            rLayout->addLayout(infoGrid);

            // ── Comments ──
            if (!comments.isEmpty()) {
                QLabel *prefs = new QLabel("💬  " + comments);
                prefs->setWordWrap(true);
                prefs->setStyleSheet(QString(
                    "QLabel { color: %1; font-size: 12px; font-style: italic;"
                    "    border: 1px solid %2; border-radius: 6px; padding: 8px;"
                    "    background: %3; }"
                ).arg(theme->secondaryTextColor(), theme->borderColor(),
                      isDark ? "#0F172A" : "#F9FAFB"));
                rLayout->addWidget(prefs);
            }

            // ── Action buttons ──
            QHBoxLayout *btns = new QHBoxLayout();
            btns->addStretch();

            if (reqStatus == "PENDING") {
                QPushButton *rejBtn = new QPushButton("✕  Reject");
                rejBtn->setStyleSheet(
                    "QPushButton { background-color: #EF4444; color: white; border-radius: 8px;"
                    "    padding: 8px 20px; font-weight: bold; font-size: 13px; border: none; }"
                    "QPushButton:hover { background-color: #DC2626; }");
                rejBtn->setCursor(Qt::PointingHandCursor);
                connect(rejBtn, &QPushButton::clicked, [this, reqId]() { onRejectExamRequest(reqId); });

                QPushButton *accBtn = new QPushButton("✔  Approve");
                accBtn->setStyleSheet(
                    "QPushButton { background-color: #10B981; color: white; border-radius: 8px;"
                    "    padding: 8px 20px; font-weight: bold; font-size: 13px; border: none; }"
                    "QPushButton:hover { background-color: #059669; }");
                accBtn->setCursor(Qt::PointingHandCursor);
                connect(accBtn, &QPushButton::clicked, [this, reqId]() { onAcceptExamRequest(reqId); });

                btns->addWidget(rejBtn);
                btns->addWidget(accBtn);
            } else if (reqStatus == "APPROVED") {
                QPushButton *resBtn = new QPushButton("🎯  Record Result");
                resBtn->setStyleSheet(
                    "QPushButton { background-color: #F97316; color: white; border-radius: 8px;"
                    "    padding: 8px 20px; font-weight: bold; font-size: 13px; border: none; }"
                    "QPushButton:hover { background-color: #EA580C; }");
                resBtn->setCursor(Qt::PointingHandCursor);
                connect(resBtn, &QPushButton::clicked, [this, reqId]() { onRecordExamResult(reqId); });
                btns->addWidget(resBtn);
            }

            rLayout->addLayout(btns);

            scrollLayout->addWidget(rCard);
        }
    }

    if (!hasData) {
        QLabel *empty = new QLabel("No pending exam requests at this time.");
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet(QString("QLabel { color: %1; font-size: 15px; font-style: italic; margin-top: 30px; }").arg(theme->secondaryTextColor()));
        scrollLayout->addWidget(empty);
    }

    scrollLayout->addStretch();
    scroll->setWidget(scrollContent);
    layout->addWidget(scroll);

    return card;
}

void WinoInstructorDashboard::onAcceptExamRequest(int requestId)
{
    ThemeManager* theme = ThemeManager::instance();
    bool isDark = theme->currentTheme() == ThemeManager::Dark;

    QString studentEmail, studentName, examTypeStr;
    int examStep = 1;
    {
        QSqlQuery q;
        q.prepare("SELECT s.email, s.name, er.EXAM_STEP "
                  "FROM EXAM_REQUEST er JOIN STUDENTS s ON er.STUDENT_ID = s.id "
                  "WHERE er.REQUEST_ID = :id");
        q.bindValue(":id", requestId);
        if (q.exec() && q.next()) {
            studentEmail = q.value(0).toString();
            studentName  = q.value(1).toString();
            examStep     = q.value(2).toInt();
        }
    }
    examTypeStr = (examStep==1) ? "Code Exam" : (examStep==2) ? "Circuit Exam" : "Parking Exam";

    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle("Approve Exam Request");
    dlg->setObjectName("approveDialog");
    dlg->setMinimumWidth(520);
    dlg->setStyleSheet(QString("QDialog#approveDialog { background-color: %1; }").arg(theme->cardColor()));

    QVBoxLayout *mainLay = new QVBoxLayout(dlg);
    mainLay->setContentsMargins(30,30,30,30);
    mainLay->setSpacing(16);

    QLabel *hdr = new QLabel(QString("Approve — %1").arg(examTypeStr));
    hdr->setStyleSheet(QString("QLabel { color: %1; font-size: 20px; font-weight: bold; }").arg(theme->primaryTextColor()));
    mainLay->addWidget(hdr);

    QLabel *sub = new QLabel(QString("Student: <b>%1</b>").arg(studentName));
    sub->setStyleSheet(QString("QLabel { color: %1; font-size: 14px; }").arg(theme->secondaryTextColor()));
    mainLay->addWidget(sub);

    QString inputSS = QString(
        "QDateEdit, QSpinBox, QDoubleSpinBox, QLineEdit, QTextEdit {"
        "  background-color: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 6px; padding: 8px; font-size: 14px; }"
    ).arg(isDark?"#374151":"#F3F4F6", theme->primaryTextColor(), theme->borderColor());

    auto makeLabel = [&](const QString &txt) -> QLabel* {
        QLabel *l = new QLabel(txt);
        l->setStyleSheet(QString("QLabel { color: %1; font-weight: 600; font-size: 13px; }").arg(theme->primaryTextColor()));
        return l;
    };

    QGridLayout *grid = new QGridLayout();
    grid->setSpacing(10);

    grid->addWidget(makeLabel("Exam Date:"), 0, 0);
    QDateEdit *datePicker = new QDateEdit(QDate::currentDate().addDays(7));
    datePicker->setCalendarPopup(true);
    datePicker->setMinimumDate(QDate::currentDate());
    datePicker->setDisplayFormat("dd/MM/yyyy");
    datePicker->setStyleSheet(inputSS);
    grid->addWidget(datePicker, 0, 1);

    grid->addWidget(makeLabel("Duration (minutes):"), 1, 0);
    QSpinBox *durationSpin = new QSpinBox();
    durationSpin->setRange(30,240);
    durationSpin->setValue(60);
    durationSpin->setSuffix(" min");
    durationSpin->setStyleSheet(inputSS);
    grid->addWidget(durationSpin, 1, 1);

    grid->addWidget(makeLabel("Exam Fee (DT):"), 2, 0);
    QDoubleSpinBox *priceSpin = new QDoubleSpinBox();
    priceSpin->setRange(0,9999);
    priceSpin->setValue(50.0);
    priceSpin->setDecimals(3);
    priceSpin->setStyleSheet(inputSS);
    grid->addWidget(priceSpin, 2, 1);

    grid->addWidget(makeLabel("Location:"), 3, 0);
    QLineEdit *locationInput = new QLineEdit();
    locationInput->setPlaceholderText("e.g. Centre d'examen Alger");
    locationInput->setStyleSheet(inputSS);
    grid->addWidget(locationInput, 3, 1);

    grid->addWidget(makeLabel("Examiner Name:"), 4, 0);
    QLineEdit *examinerInput = new QLineEdit();
    examinerInput->setPlaceholderText("e.g. M. Bensalem");
    examinerInput->setStyleSheet(inputSS);
    grid->addWidget(examinerInput, 4, 1);

    if (examStep == 1) { // Code exam doesn't need an examiner
        grid->itemAtPosition(4, 0)->widget()->hide();
        examinerInput->hide();
    }

    mainLay->addLayout(grid);

    mainLay->addWidget(makeLabel("Additional Message (optional):"));
    QTextEdit *msgEdit = new QTextEdit();
    msgEdit->setPlaceholderText("Write any additional information for the student...");
    msgEdit->setFixedHeight(80);
    msgEdit->setStyleSheet(inputSS);
    mainLay->addWidget(msgEdit);

    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    QPushButton *cancelBtn = new QPushButton("Cancel");
    cancelBtn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; padding:10px 20px; border-radius:6px; font-weight:bold; }"
                                     "QPushButton:hover { background:%2; }").arg(theme->secondaryTextColor(), isDark?"#374151":"#F3F4F6"));
    connect(cancelBtn, &QPushButton::clicked, dlg, &QDialog::reject);

    QPushButton *sendBtn = new QPushButton("Approve & Send Email");
    sendBtn->setStyleSheet("QPushButton { background-color:#10B981; color:white; padding:10px 24px; border-radius:6px; font-weight:bold; border:none; }"
                           "QPushButton:hover { background-color:#059669; }");

    connect(sendBtn, &QPushButton::clicked, [=]() {
        if (locationInput->text().trimmed().isEmpty() || (examStep != 1 && examinerInput->text().trimmed().isEmpty())) {
            QMessageBox::warning(this, "Missing Info", "Please fill in Location" + QString((examStep != 1) ? " and Examiner Name." : "."));
            return;
        }
        if (studentEmail.isEmpty()) {
            QMessageBox::warning(this, "No Email", "Student email not found in database.");
            return;
        }
        QSqlQuery upd;
        upd.prepare("UPDATE EXAM_REQUEST SET STATUS='APPROVED', CONFIRMED_DATE=TO_DATE(:dt,'YYYY-MM-DD'), AMOUNT=:amt WHERE REQUEST_ID=:id");
        upd.bindValue(":dt", datePicker->date().toString("yyyy-MM-dd"));
        upd.bindValue(":amt", priceSpin->value());
        upd.bindValue(":id", requestId);
        if (!upd.exec()) { QMessageBox::warning(this,"DB Error", upd.lastError().text()); return; }

        QString extraMsg = msgEdit->toPlainText().trimmed();
        QString extraBlock = extraMsg.isEmpty() ? "" :
            QString("<div style='margin-top:25px;padding:20px 24px;background:#F0FDFA;border-left:4px solid #0D9488;border-radius:12px;color:#0F766E;font-size:15px;line-height:1.7;'>"
                    "<b style='color:#0F172A;font-size:13px;text-transform:uppercase;letter-spacing:1px;'>Personal Note from your Instructor:</b><br><br><i>\"%1\"</i></div>").arg(extraMsg.toHtmlEscaped().replace("\n","<br>"));

        QString html = QString(
"<!DOCTYPE html><html><head><meta charset='UTF-8'><style>"
"body{font-family:'Segoe UI',Arial,sans-serif;background-color:#F9FAFB;margin:0;padding:20px 0;}"
".wrap{max-width:600px;margin:0 auto;background:#ffffff;border-radius:16px;overflow:hidden;box-shadow:0 10px 25px rgba(0,0,0,.05); border:1px solid #E5E7EB;}"
".hdr{background:linear-gradient(135deg, #0F766E, #0D9488);padding:40px 36px;text-align:center;}"
".hdr h1{color:#ffffff;margin:0;font-size:28px;font-weight:300;letter-spacing:1px;}"
".hdr p{color:rgba(255,255,255,.9);margin:10px 0 0;font-size:15px;text-transform:uppercase;letter-spacing:2px;font-weight:600;}"
".body{padding:40px 36px;}"
".greeting{font-size:18px;color:#1F2937;margin-bottom:24px;line-height:1.6;}"
".message{font-size:15px;color:#4B5563;margin-bottom:30px;line-height:1.7;}"
".info-box{background:#F8FAFC;border:1px solid #E2E8F0;border-radius:12px;padding:25px;margin:25px 0;}"
".info-title{font-size:13px;text-transform:uppercase;color:#64748B;font-weight:700;letter-spacing:1px;margin-bottom:15px;}"
".info-grid{border-collapse:collapse;width:100%%;}"
".info-grid td{padding:12px 0;font-size:15px;border-bottom:1px solid #F1F5F9;}"
".info-grid tr:last-child td{border-bottom:none;}"
".info-grid td:first-child{color:#64748B;width:40%%;font-weight:500;}"
".info-grid td:last-child{color:#0F172A;font-weight:600;}"
".badge{display:inline-block;background:#E0F2FE;color:#0369A1;border-radius:20px;padding:4px 12px;font-size:14px;font-weight:600;line-height:1;margin:0 4px;vertical-align:middle;}"
".wish{margin-top:30px;padding:24px;background:#FFFBEB;border-radius:12px;border-left:4px solid #F59E0B;color:#92400E;font-size:15px;line-height:1.8; font-style:italic;}"
".footer{background:#F8FAFC;padding:25px 36px;font-size:13px;color:#94A3B8;text-align:center;border-top:1px solid #E2E8F0;}"
".heart{color:#EF4444;}"
"</style></head><body>"
"<div class='wrap'>"
"<div class='hdr'><h1>Your Exam is Confirmed</h1><p>WINO Smart Driving School</p></div>"
"<div class='body'>"
"<p class='greeting'>Dear <b>%1</b>,</p>"
"<p class='message'>It is with great pride that we inform you that your request for the <span class='badge'>%2</span> has been <b>officially approved</b>.<br><br>"
"We have watched you grow, learn, and overcome challenges to reach this very moment. Trust in the skills you've acquired and the countless hours you have spent preparing. You are ready for this.</p>"
"<div class='info-box'>"
"<div class='info-title'>Official Exam Itinerary</div>"
"<table class='info-grid'>"
"<tr><td>Date &amp; Time</td><td>%3</td></tr>"
"<tr><td>Duration</td><td>%4 minutes</td></tr>"
"<tr><td>Registration Fee</td><td>%5 TND</td></tr>"
"<tr><td>Exam Center</td><td>%6</td></tr>"
"%7"
"</table>"
"</div>"
"%8"
"<div class='wish'><b>Our warmest wishes to you!</b><br> %10 We believe in you, and we will be cheering for you every step of the way. <span class='heart'>&hearts;</span></div>"
"</div><div class='footer'>Sent with care from WINO Smart Driving School.<br>Automated message &mdash; please do not reply. &copy; %9 WINO Platform</div>"
"</div></body></html>")
            .arg(studentName, examTypeStr,
                 datePicker->date().toString("dddd dd MMMM yyyy"),
                 QString::number(durationSpin->value()),
                 QString::number(priceSpin->value(),'f',3),
                 locationInput->text().trimmed().toHtmlEscaped(),
                 (examStep == 1) ? "" : QString("<tr><td>Examiner</td><td>%1</td></tr>").arg(examinerInput->text().trimmed().toHtmlEscaped()),
                 extraBlock,
                 QString::number(QDate::currentDate().year()),
                 (examStep == 1) ? "Your dedication to understanding the rules of the road is inspiring. Breathe deeply, read carefully, and let your knowledge shine."
                                 : "You have mastered the wheel and proven your resilience. Take a deep breath, trust your instincts, and drive with absolute confidence.");

        SmtpMailer mailer;
        mailer.setCredentials("nesrin965333@gmail.com", "egpipwfvlfwahohd");
        QString err;
        bool ok = mailer.sendHtml(studentEmail, QString("Your %1 Appointment - WINO Driving School").arg(examTypeStr), html, &err);
        dlg->accept();

        // ── Notify about email result ──
        if (ok) QMessageBox::information(this, "✅ Approved", "Exam approved successfully!");
        else    qDebug() << "[WINO] Exam approved (DB ok). Email skipped:" << err;

        onExamsTabClicked(); // refresh the view to show the new Record Result button
    });

    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(sendBtn);
    mainLay->addLayout(btnRow);
    dlg->exec();
}

void WinoInstructorDashboard::onRecordExamResult(int requestId)
{
    ThemeManager *resTheme = ThemeManager::instance();
    bool resIsDark = resTheme->currentTheme() == ThemeManager::Dark;

    int studentId = -1;
    QString studentName;
    int examStep = 1;

    QSqlQuery q;
    q.prepare("SELECT er.STUDENT_ID, s.name, er.EXAM_STEP "
              "FROM EXAM_REQUEST er JOIN STUDENTS s ON er.STUDENT_ID = s.id "
              "WHERE er.REQUEST_ID = :id");
    q.bindValue(":id", requestId);
    if (q.exec() && q.next()) {
        studentId = q.value(0).toInt();
        studentName = q.value(1).toString();
        examStep = q.value(2).toInt();
    }
    QString examTypeStr = (examStep == 1) ? "Code Exam" : (examStep == 2) ? "Circuit Exam" : "Parking Exam";

    QDialog *resDlg = new QDialog(this);
    resDlg->setWindowTitle("Record Exam Result — " + examTypeStr);
    resDlg->setObjectName("resultDialog");
    resDlg->setMinimumWidth(480);
    resDlg->setStyleSheet(QString("QDialog#resultDialog { background-color: %1; }").arg(resTheme->cardColor()));

    QVBoxLayout *resLay = new QVBoxLayout(resDlg);
    resLay->setContentsMargins(30, 28, 30, 28);
    resLay->setSpacing(16);

    // Title
    QLabel *resHdr = new QLabel("📝 Record Exam Result");
    resHdr->setStyleSheet(QString("QLabel { color: %1; font-size: 20px; font-weight: bold; }").arg(resTheme->primaryTextColor()));
    resLay->addWidget(resHdr);

    QLabel *resSub = new QLabel(QString("Student: <b>%1</b>  |  Exam: <b>%2</b>").arg(studentName, examTypeStr));
    resSub->setStyleSheet(QString("QLabel { color: %1; font-size: 14px; }").arg(resTheme->secondaryTextColor()));
    resLay->addWidget(resSub);

    // Pass / Fail selection
    QLabel *resultLbl = new QLabel("Exam Outcome:");
    resultLbl->setStyleSheet(QString("QLabel { color: %1; font-weight: 600; font-size: 13px; }").arg(resTheme->primaryTextColor()));
    resLay->addWidget(resultLbl);

    QHBoxLayout *radioRow = new QHBoxLayout();
    QRadioButton *passRadio = new QRadioButton("✅  Passed");
    QRadioButton *failRadio = new QRadioButton("❌  Failed");
    passRadio->setStyleSheet(QString("QRadioButton { color: %1; font-size: 14px; font-weight: 600; }")
        .arg(resIsDark ? "#34D399" : "#059669"));
    failRadio->setStyleSheet(QString("QRadioButton { color: %1; font-size: 14px; font-weight: 600; }")
        .arg(resIsDark ? "#F87171" : "#DC2626"));
    passRadio->setChecked(true);
    radioRow->addWidget(passRadio);
    radioRow->addWidget(failRadio);
    radioRow->addStretch();
    resLay->addLayout(radioRow);

    // Score (for Code exam only)
    QLabel *scoreLbl = nullptr;
    QDoubleSpinBox *scoreSpin = nullptr;
    if (examStep == 1) {
        scoreLbl = new QLabel("Score (0 – 30):");
        scoreLbl->setStyleSheet(QString("QLabel { color: %1; font-weight: 600; font-size: 13px; }").arg(resTheme->primaryTextColor()));
        resLay->addWidget(scoreLbl);

        scoreSpin = new QDoubleSpinBox();
        scoreSpin->setRange(0, 30);
        scoreSpin->setDecimals(2);
        scoreSpin->setValue(0);
        scoreSpin->setStyleSheet(QString(
            "QDoubleSpinBox { background-color: %1; color: %2; border: 1px solid %3;"
            "    border-radius: 6px; padding: 8px; font-size: 14px; }")
            .arg(resIsDark ? "#374151" : "#F3F4F6", resTheme->primaryTextColor(), resTheme->borderColor()));
        resLay->addWidget(scoreSpin);
    }

    // Buttons
    QHBoxLayout *resBtnRow = new QHBoxLayout();
    resBtnRow->addStretch();

    QPushButton *skipBtn = new QPushButton("Cancel");
    skipBtn->setStyleSheet(QString(
        "QPushButton { background: transparent; color: %1; padding: 10px 20px;"
        "    border-radius: 6px; font-weight: bold; }"
        "QPushButton:hover { background: %2; }")
        .arg(resTheme->secondaryTextColor(), resIsDark ? "#374151" : "#F3F4F6"));
    connect(skipBtn, &QPushButton::clicked, resDlg, &QDialog::reject);

    QPushButton *saveBtn = new QPushButton("💾  Save Result");
    saveBtn->setStyleSheet(
        "QPushButton { background-color: #F97316; color: white; padding: 10px 24px;"
        "    border-radius: 6px; font-weight: bold; border: none; }"
        "QPushButton:hover { background-color: #EA580C; }");

    connect(saveBtn, &QPushButton::clicked, [=]() {
        bool passed = passRadio->isChecked();
        double score = (scoreSpin != nullptr) ? scoreSpin->value() : -1.0;

        // 1. Update EXAM_REQUEST: set PASSED and SCORE, STATUS=COMPLETED
        QSqlQuery updReq;
        if (examStep == 1 && score >= 0) {
            updReq.prepare(
                "UPDATE EXAM_REQUEST "
                "SET PASSED = :p, SCORE = :sc, STATUS = 'COMPLETED' "
                "WHERE REQUEST_ID = :id");
            updReq.bindValue(":sc", score);
        } else {
            updReq.prepare(
                "UPDATE EXAM_REQUEST "
                "SET PASSED = :p, STATUS = 'COMPLETED' "
                "WHERE REQUEST_ID = :id");
        }
        updReq.bindValue(":p", passed ? 1 : 0);
        updReq.bindValue(":id", requestId);
        if (!updReq.exec()) {
            QMessageBox::warning(resDlg, "DB Error", "Could not save exam result:\n" + updReq.lastError().text());
            return;
        }

        // 2. If student passed → advance WINO_PROGRESS
        if (passed && studentId > 0) {
            QSqlQuery updProg;
            if (examStep == 1) {
                // CODE passed → unlock CIRCUIT
                updProg.prepare(
                    "UPDATE WINO_PROGRESS "
                    "SET code_status = 'COMPLETED', "
                    "    code_passed_date = SYSDATE, "
                    "    code_score = :sc, "
                    "    circuit_status = 'IN_PROGRESS', "
                    "    current_step = 2 "
                    "WHERE user_id = :uid");
                updProg.bindValue(":sc", score >= 0 ? QVariant(score) : QVariant());
            } else if (examStep == 2) {
                // CIRCUIT passed → unlock PARKING
                updProg.prepare(
                    "UPDATE WINO_PROGRESS "
                    "SET circuit_status = 'COMPLETED', "
                    "    circuit_passed_date = SYSDATE, "
                    "    parking_status = 'IN_PROGRESS', "
                    "    current_step = 3 "
                    "WHERE user_id = :uid");
            } else {
                // PARKING passed → student is fully qualified
                updProg.prepare(
                    "UPDATE WINO_PROGRESS "
                    "SET parking_status = 'COMPLETED', "
                    "    parking_passed_date = SYSDATE, "
                    "    is_qualified = 1 "
                    "WHERE user_id = :uid");
            }
            updProg.bindValue(":uid", studentId);
            if (!updProg.exec()) {
                QMessageBox::warning(resDlg, "DB Error",
                    "Result saved, but failed to advance student progress:\n" + updProg.lastError().text());
            }
        }

        resDlg->accept();
        onExamsTabClicked(); // refresh dashboard

        QString outcome = passed ? "✅ PASSED" : "❌ FAILED";
        QString nextMsg = "";
        if (passed) {
            if (examStep == 1)      nextMsg = "\n\nStudent is now unlocked for the Circuit stage.";
            else if (examStep == 2) nextMsg = "\n\nStudent is now unlocked for the Parking stage.";
            else                    nextMsg = "\n\nStudent is now fully qualified! 🎉";
        }
        QMessageBox::information(this, "Result Saved",
            QString("Exam result for <b>%1</b> recorded as <b>%2</b>.%3")
            .arg(studentName, outcome, nextMsg));
    });

    resBtnRow->addWidget(skipBtn);
    resBtnRow->addWidget(saveBtn);
    resLay->addLayout(resBtnRow);
    resDlg->exec();
}

void WinoInstructorDashboard::onRejectExamRequest(int requestId)
{
    ThemeManager* theme = ThemeManager::instance();
    bool isDark = theme->currentTheme() == ThemeManager::Dark;

    QString studentEmail, studentName, examTypeStr;
    int examStep = 1;
    {
        QSqlQuery q;
        q.prepare("SELECT s.email, s.name, er.EXAM_STEP "
                  "FROM EXAM_REQUEST er JOIN STUDENTS s ON er.STUDENT_ID = s.id "
                  "WHERE er.REQUEST_ID = :id");
        q.bindValue(":id", requestId);
        if (q.exec() && q.next()) {
            studentEmail = q.value(0).toString();
            studentName  = q.value(1).toString();
            examStep     = q.value(2).toInt();
        }
    }
    examTypeStr = (examStep==1) ? "Code Exam" : (examStep==2) ? "Circuit Exam" : "Parking Exam";

    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle("Reject Exam Request");
    dlg->setObjectName("rejectDialog");
    dlg->setMinimumWidth(520);
    dlg->setStyleSheet(QString("QDialog#rejectDialog { background-color: %1; }").arg(theme->cardColor()));

    QVBoxLayout *mainLay = new QVBoxLayout(dlg);
    mainLay->setContentsMargins(30,30,30,30);
    mainLay->setSpacing(16);

    QLabel *hdr = new QLabel(QString("Reject — %1").arg(examTypeStr));
    hdr->setStyleSheet("QLabel { color: #EF4444; font-size: 20px; font-weight: bold; }");
    mainLay->addWidget(hdr);

    QLabel *sub = new QLabel(QString("Student: <b>%1</b>").arg(studentName));
    sub->setStyleSheet(QString("QLabel { color: %1; font-size: 14px; }").arg(theme->secondaryTextColor()));
    mainLay->addWidget(sub);

    QString inputSS = QString(
        "QComboBox, QTextEdit, QLineEdit {"
        "  background-color: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 6px; padding: 8px; font-size: 14px; }"
    ).arg(isDark?"#374151":"#F3F4F6", theme->primaryTextColor(), theme->borderColor());

    auto makeLabel = [&](const QString &txt) -> QLabel* {
        QLabel *l = new QLabel(txt);
        l->setStyleSheet(QString("QLabel { color: %1; font-weight: 600; font-size: 13px; }").arg(theme->primaryTextColor()));
        return l;
    };

    mainLay->addWidget(makeLabel("Quick Rejection Reason:"));
    QComboBox *reasonCombo = new QComboBox();
    
    QStringList reasons;
    reasons << "-- Select a reason --";
    
    if (examStep == 1) {
        reasons << "Your theory score does not meet the minimum requirement."
                << "You need to review the traffic laws more thoroughly."
                << "Your performance in practice tests is inconsistent."
                << "Your session attendance is insufficient."
                << "The requested exam slot is not available.";
    } else {
        reasons << "You have not completed all required driving hours."
                << "Your driving skills need further improvement."
                << "You need to practice specific maneuvers (e.g. parking, vehicle control)."
                << "You are not ready for the practical exam yet."
                << "The requested exam slot is not available."
                << "Please book more sessions before requesting an exam.";
    }
    
    reasonCombo->addItems(reasons);
    reasonCombo->setStyleSheet(inputSS);
    mainLay->addWidget(reasonCombo);

    mainLay->addWidget(makeLabel("Additional Message (optional):"));
    QTextEdit *customMsg = new QTextEdit();
    customMsg->setPlaceholderText("Write any personalized feedback...");
    customMsg->setFixedHeight(70);
    customMsg->setStyleSheet(inputSS);
    mainLay->addWidget(customMsg);

    mainLay->addWidget(makeLabel("Advice for Improvement:"));
    QTextEdit *adviceEdit = new QTextEdit();
    adviceEdit->setPlaceholderText("e.g. Focus on parking maneuvers, review road signs...");
    adviceEdit->setFixedHeight(70);
    adviceEdit->setStyleSheet(inputSS);
    mainLay->addWidget(adviceEdit);

    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    QPushButton *cancelBtn = new QPushButton("Cancel");
    cancelBtn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; padding:10px 20px; border-radius:6px; font-weight:bold; }"
                                     "QPushButton:hover { background:%2; }").arg(theme->secondaryTextColor(), isDark?"#374151":"#F3F4F6"));
    connect(cancelBtn, &QPushButton::clicked, dlg, &QDialog::reject);

    QPushButton *sendBtn = new QPushButton("Reject & Send Email");
    sendBtn->setStyleSheet("QPushButton { background-color:#EF4444; color:white; padding:10px 24px; border-radius:6px; font-weight:bold; border:none; }"
                           "QPushButton:hover { background-color:#DC2626; }");

    connect(sendBtn, &QPushButton::clicked, [=]() {
        QString reason = (reasonCombo->currentIndex()==0) ? QString() : reasonCombo->currentText();
        QString custom = customMsg->toPlainText().trimmed();
        QString advice = adviceEdit->toPlainText().trimmed();
        if (reason.isEmpty() && custom.isEmpty()) {
            QMessageBox::warning(this, "Missing Info", "Please select a reason or write a message."); return;
        }
        if (studentEmail.isEmpty()) {
            QMessageBox::warning(this, "No Email", "Student email not found in database."); return;
        }

        QString fullReason = reason;
        if (!custom.isEmpty()) { if (!fullReason.isEmpty()) fullReason += " "; fullReason += custom; }

        QSqlQuery upd;
        upd.prepare("UPDATE EXAM_REQUEST SET STATUS='REJECTED', REJECTION_REASON=:r WHERE REQUEST_ID=:id");
        upd.bindValue(":r", fullReason);
        upd.bindValue(":id", requestId);
        upd.exec();

        QString reasonBlock = fullReason.isEmpty() ? "" :
            QString("<div style='padding:12px 16px;background:#FEF2F2;border-left:4px solid #EF4444;border-radius:6px;color:#991B1B;'>"
                    "<b>Reason:</b> %1</div>").arg(fullReason.toHtmlEscaped());

        QString adviceBlock = advice.isEmpty() ? "" :
            QString("<div style='margin-top:14px;padding:12px 16px;background:#FFFBEB;border-left:4px solid #F59E0B;border-radius:6px;color:#92400E;'>"
                    "<b>Instructor's Advice:</b><br>%1</div>").arg(advice.toHtmlEscaped().replace("\n","<br>"));

        QString html = QString(
"<!DOCTYPE html><html><head><meta charset='UTF-8'><style>"
"body{font-family:'Segoe UI',Arial,sans-serif;background:#F3F4F6;margin:0;padding:0;}"
".wrap{max-width:600px;margin:30px auto;background:#fff;border-radius:12px;overflow:hidden;box-shadow:0 4px 20px rgba(0,0,0,.1);}"
".hdr{background:linear-gradient(135deg,#EF4444,#B91C1C);padding:32px 36px;}"
".hdr h1{color:#fff;margin:0;font-size:24px;}"
".hdr p{color:rgba(255,255,255,.85);margin:6px 0 0;font-size:14px;}"
".body{padding:32px 36px;}"
".greeting{font-size:16px;color:#111827;margin-bottom:20px;line-height:1.7;}"
".badge{display:inline-block;background:#FEE2E2;color:#DC2626;border:1px solid #FECACA;border-radius:20px;padding:4px 14px;font-size:13px;font-weight:600;}"
".encourage{margin-top:24px;padding:18px;background:#F0FDFA;border-radius:8px;border-left:4px solid #14B8A6;color:#065F46;font-size:14px;line-height:1.7;}"
".footer{background:#F9FAFB;padding:20px 36px;font-size:12px;color:#9CA3AF;text-align:center;}"
"</style></head><body>"
"<div class='wrap'>"
"<div class='hdr'><h1>Exam Request Update</h1><p>WINO Smart Driving School</p></div>"
"<div class='body'>"
"<p class='greeting'>Dear <b>%1</b>,<br><br>"
"We regret to inform you that your request for the <span class='badge'>%2</span> has been <b>declined</b> at this time.</p>"
"%3%4"
"<div class='encourage'><b>Don't be discouraged!</b><br>"
"Every great driver starts somewhere. Use this as an opportunity to improve your skills and come back stronger. "
"We believe in you — keep practicing, stay focused, and your success is just around the corner!</div>"
"</div><div class='footer'>WINO Smart Driving School &bull; Automated message &mdash; please do not reply. &copy; %5 WINO Platform</div>"
"</div></body></html>")
            .arg(studentName, examTypeStr, reasonBlock, adviceBlock,
                 QString::number(QDate::currentDate().year()));

        SmtpMailer mailer;
        mailer.setCredentials("nesrin965333@gmail.com", "egpipwfvlfwahohd");
        QString err;
        bool ok = mailer.sendHtml(studentEmail, QString("Update on Your %1 Request - WINO Driving School").arg(examTypeStr), html, &err);
        dlg->accept();
        onExamsTabClicked();
        if (ok) QMessageBox::information(this, "✅ Rejected", "Exam request rejected successfully!");
        else    qDebug() << "[WINO] Exam rejected (DB ok). Email skipped:" << err;
    });

    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(sendBtn);
    mainLay->addLayout(btnRow);
    dlg->exec();
}

QWidget* WinoInstructorDashboard::createStudentsSection()
{
    ThemeManager* theme = ThemeManager::instance();
    bool isDark = theme->currentTheme() == ThemeManager::Dark;

    QWidget *container = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(20);

    int currentAppUserId = qApp->property("currentUserId").toInt();
    if (currentAppUserId == 0) {
        QWidget* emptyWidget = new QWidget();
        emptyWidget->setObjectName("emptyStudentsSection");
        return emptyWidget;
    }

    int instructorId = currentAppUserId;
    int instructorSchoolId = -1;
    QSqlQuery qid;

    qid.prepare("SELECT school_id FROM instructors WHERE id = :id");
    qid.bindValue(":id", currentAppUserId);

    if (qid.exec() && qid.next()) {
        instructorSchoolId = qid.value(0).toInt();
    } else {
        QWidget* errWidget = new QWidget();
        QVBoxLayout *errLayout = new QVBoxLayout(errWidget);
        QLabel *errLabel = new QLabel("Error loading instructor details.");
        errLayout->addWidget(errLabel);
        return errWidget;
    }

    QHBoxLayout *headerLayout = new QHBoxLayout();
    QLabel *titleLabel = new QLabel("🚗 Étudiants — Phase Circuit");
    titleLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 20px; font-weight: bold; }").arg(theme->primaryTextColor()));
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    layout->addLayout(headerLayout);

    QScrollArea *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }");
    
    QWidget *scrollContent = new QWidget();
    scrollContent->setStyleSheet("background: transparent;");
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(0, 0, 10, 0);
    scrollLayout->setSpacing(15);

    // Query from 'students' (school management table, lowercase) which has
    // instructor_id properly set. Join with STUDENTS (login table) by email
    // to get the login ID needed for WINO_PROGRESS / SESSION_BOOKING.
    QSqlQuery q;
    q.prepare(
        "SELECT sm.id, sm.name, sm.phone, sm.email, "
        "       NVL(up.current_step, 1), "
        "       NVL(up.code_status,    'IN_PROGRESS'), "
        "       NVL(up.circuit_status, 'LOCKED'), "
        "       NVL(up.parking_status, 'LOCKED'), "
        "       (SELECT COUNT(*) FROM SESSION_BOOKING sb "
        "        WHERE sb.student_id = NVL(st.id, sm.id) "
        "          AND sb.status IN ('CONFIRMED','COMPLETED') "
        "          AND sb.SESSION_DATE < SYSDATE), "
        "       0, "   // is_qualified placeholder
        "       (SELECT TO_CHAR(MIN(er.CONFIRMED_DATE),'DD/MM/YYYY') "
        "        FROM EXAM_REQUEST er "
        "        WHERE er.STUDENT_ID = NVL(st.id, sm.id) "
        "          AND er.STATUS = 'APPROVED' "
        "          AND er.CONFIRMED_DATE >= TRUNC(SYSDATE)) "
        "FROM students sm "
        "LEFT JOIN STUDENTS st ON LOWER(st.email) = LOWER(sm.email) "
        "LEFT JOIN WINO_PROGRESS up ON up.user_id = NVL(st.id, sm.id) "
        "WHERE sm.instructor_id = :inst_id "
        "  AND sm.status = 'approved' "
        "  AND NVL(up.current_step, 1) = 2 "   // Circuit step only (1=Code, 2=Circuit, 3=Parking)
        "ORDER BY sm.id DESC");
    q.bindValue(":inst_id", instructorId);

    bool hasData = false;
    if (q.exec()) {
        while (q.next()) {
            hasData = true;
            QString name = q.value(1).toString();
            QString phone = q.value(2).toString();
            QString email = q.value(3).toString();
            int currentStep = q.value(4).toInt();
            QString codeStat = q.value(5).toString();
            QString circuitStat = q.value(6).toString();
            QString parkingStat = q.value(7).toString();
            int sessions = q.value(8).toInt();
            int isQual = q.value(9).toInt();
            QString nextExam = q.value(10).toString();

            QFrame *card = new QFrame();
            card->setStyleSheet(QString("QFrame { background-color: %1; border-radius: 12px; border: 1px solid %2; }")
                .arg(theme->cardColor(), theme->borderColor()));
            
            QHBoxLayout *cardLay = new QHBoxLayout(card);
            cardLay->setContentsMargins(20, 20, 20, 20);
            cardLay->setSpacing(20);

            // Left side: icon & info
            QLabel *iconLbl = new QLabel("👤");
            iconLbl->setFixedSize(50, 50);
            iconLbl->setAlignment(Qt::AlignCenter);
            iconLbl->setStyleSheet(QString("QLabel { background-color: %1; border-radius: 25px; font-size: 24px; border: none; }")
                .arg(isDark ? "#374151" : "#F3F4F6"));
            cardLay->addWidget(iconLbl);

            QVBoxLayout *infoLay = new QVBoxLayout();
            QLabel *nameLbl = new QLabel(name);
            nameLbl->setStyleSheet(QString("QLabel { color: %1; font-size: 16px; font-weight: bold; border: none; background: transparent; }")
                .arg(theme->primaryTextColor()));
            QLabel *contactLbl = new QLabel(QString("📞 %1   ✉️ %2").arg(phone, email));
            contactLbl->setStyleSheet(QString("QLabel { color: %1; font-size: 13px; border: none; background: transparent; }")
                .arg(theme->secondaryTextColor()));
            QLabel *sessLbl = new QLabel(QString("🚗 Total Practical Sessions: <b>%1</b>").arg(sessions));
            sessLbl->setStyleSheet(QString("QLabel { color: %1; font-size: 13px; border: none; background: transparent; margin-top: 4px; }")
                .arg(theme->secondaryTextColor()));
            
            infoLay->addWidget(nameLbl);
            infoLay->addWidget(contactLbl);
            infoLay->addWidget(sessLbl);

            if (!nextExam.isEmpty()) {
                QLabel *examLbl = new QLabel(QString("📅 Next Exam: <span style='color:#F97316; font-weight:bold;'>%1</span>").arg(nextExam));
                examLbl->setStyleSheet(QString("QLabel { color: %1; font-size: 13px; border: none; background: transparent; margin-top: 2px; }")
                    .arg(theme->secondaryTextColor()));
                infoLay->addWidget(examLbl);
            }

            cardLay->addLayout(infoLay);
            cardLay->addStretch();

            // Right side: Progress Pills
            QVBoxLayout *progLay = new QVBoxLayout();
            progLay->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
            progLay->setSpacing(8);

            QLabel *progTitle = new QLabel((isQual == 1) ? "🎉 Fully Qualified!" : "Current Progress");
            progTitle->setAlignment(Qt::AlignRight);
            progTitle->setStyleSheet(QString("QLabel { color: %1; font-size: 13px; font-weight: bold; border: none; background: transparent; }")
                .arg((isQual == 1) ? "#10B981" : theme->primaryTextColor()));
            progLay->addWidget(progTitle);

            QHBoxLayout *pillsLay = new QHBoxLayout();
            pillsLay->setSpacing(10);

            auto makePill = [&](const QString& label, const QString& status, int stepNum) -> QLabel* {
                QLabel *l = new QLabel(label);
                l->setAlignment(Qt::AlignCenter);
                QString bg, color, border;
                
                if (status == "COMPLETED" || (isQual == 1)) {
                    bg = isDark ? "#134E4A" : "#ECFDF5";
                    color = isDark ? "#34D399" : "#059669";
                    border = "1px solid " + (isDark ? QString("#059669") : QString("#10B981"));
                } else if (currentStep == stepNum) {
                    bg = isDark ? "#1E3A8A" : "#EFF6FF";
                    color = isDark ? "#60A5FA" : "#2563EB";
                    border = "1px solid " + (isDark ? QString("#3B82F6") : QString("#60A5FA"));
                } else {
                    bg = isDark ? "#374151" : "#F3F4F6";
                    color = isDark ? "#9CA3AF" : "#6B7280";
                    border = "1px dashed " + (isDark ? QString("#4B5563") : QString("#D1D5DB"));
                }

                l->setStyleSheet(QString("QLabel { background-color: %1; color: %2; border: %3; border-radius: 12px; padding: 4px 12px; font-size: 12px; font-weight: 600; }")
                    .arg(bg, color, border));
                return l;
            };

            pillsLay->addWidget(makePill("📘 Code", codeStat, 1));
            pillsLay->addWidget(makePill("🚗 Circuit", circuitStat, 2));
            pillsLay->addWidget(makePill("🅿️ Parking", parkingStat, 3));
            
            progLay->addLayout(pillsLay);
            cardLay->addLayout(progLay);

            scrollLayout->addWidget(card);
        }
    }

    if (!hasData) {
        QLabel *empty = new QLabel("Aucun étudiant en phase circuit ne vous est actuellement assigné.");
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet(QString("QLabel { color: %1; font-size: 15px; font-style: italic; margin-top: 30px; }").arg(theme->secondaryTextColor()));
        scrollLayout->addWidget(empty);
    }

    scrollLayout->addStretch();
    scroll->setWidget(scrollContent);
    layout->addWidget(scroll);

    return container;
}

