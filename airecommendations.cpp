#include "airecommendations.h"
#include "thememanager.h"

#include <QScrollArea>
#include <QProgressBar>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QMessageBox>
#include <QGridLayout>
#include <QSqlQuery>
#include <QApplication>
#include <QSqlError>
#include <QVariant>
#include <QDebug>
#include <QTime>

AIRecommendations::AIRecommendations(QWidget *parent) :
    QDialog(parent)
{
    setupUI();
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, &AIRecommendations::onThemeChanged);
    
    // Connect to weather service
    connect(WeatherService::instance(), &WeatherService::weatherDataReady,
            this, &AIRecommendations::onWeatherDataReady);
    
    updateColors();
    
    // Trigger weather fetch if not already loaded
    if (!WeatherService::instance()->hasData()) {
        WeatherService::instance()->fetchForecast();
    } else {
        onWeatherDataReady(); // data already available, build cards immediately
    }
}

AIRecommendations::~AIRecommendations()
{
}

void AIRecommendations::setupUI()
{
    setWindowTitle("AI Recommendations");
    // setWindowState(Qt::WindowMaximized); // Handled by showFullScreen() from dashboard
    setMinimumSize(1200, 800);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Header
    QWidget *header = new QWidget();
    header->setObjectName("header");
    header->setFixedHeight(120);
    
    QVBoxLayout *headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(40, 20, 40, 20);
    headerLayout->setSpacing(5);
    
    QPushButton *backBtn = new QPushButton("← Back to Dashboard");
    backBtn->setObjectName("backBtn");
    backBtn->setCursor(Qt::PointingHandCursor);
    connect(backBtn, &QPushButton::clicked, this, &AIRecommendations::close);
    
    QLabel *titleLabel = new QLabel("✨ AI Weather-Smart Recommendations");
    titleLabel->setObjectName("titleLabel");
    
    QLabel *subtitleLabel = new QLabel("Real-time weather analysis powering your driving session planning");
    subtitleLabel->setObjectName("subtitleLabel");
    
    headerLayout->addWidget(backBtn);
    headerLayout->addWidget(titleLabel);
    headerLayout->addWidget(subtitleLabel);
    
    mainLayout->addWidget(header);
    
    // Content
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setObjectName("scrollArea");
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    QWidget *contentWidget = new QWidget();
    contentWidget->setObjectName("contentWidget");
    QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(40, 40, 40, 40);
    contentLayout->setSpacing(30);
    
    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea);
}

void AIRecommendations::onThemeChanged()
{
    updateColors();
}

void AIRecommendations::onWeatherDataReady()
{
    // Rebuild the content with real weather data
    updateColors();
}

void AIRecommendations::updateColors()
{
    ThemeManager* theme = ThemeManager::instance();
    
    // 1. Update Header Styles
    QWidget* header = findChild<QWidget*>("header");
    if (header) {
        header->setStyleSheet(QString("QWidget#header { background-color: %1; border-bottom: 1px solid %2; }")
                              .arg(theme->headerColor(), theme->borderColor()));
    }
    
    QPushButton* backBtn = findChild<QPushButton*>("backBtn");
    if (backBtn) {
        backBtn->setStyleSheet(
            "QPushButton {"
            "    background-color: transparent;"
            "    color: #14B8A6;"
            "    font-size: 14px;"
            "    font-weight: 600;"
            "    border: none;"
            "    text-align: left;"
            "    padding: 0px;"
            "}"
            "QPushButton:hover { color: #0D9488; }"
        );
    }
    
    QLabel* titleLabel = findChild<QLabel*>("titleLabel");
    if (titleLabel) {
        titleLabel->setStyleSheet(
            QString("QLabel {"
            "    color: %1;"
            "    font-size: 26px;"
            "    font-weight: bold;"
            "    padding: 5px 0;"
            "}").arg(theme->primaryTextColor())
        );
    }
    
    QLabel* subtitleLabel = findChild<QLabel*>("subtitleLabel");
    if (subtitleLabel) {
        subtitleLabel->setStyleSheet(
            QString("QLabel {"
            "    color: %1;"
            "    font-size: 14px;"
            "    padding: 0px;"
            "}").arg(theme->secondaryTextColor())
        );
    }
    
    // 2. Update Scroll Area & Content Background
    QScrollArea* scrollArea = findChild<QScrollArea*>("scrollArea");
    if (scrollArea) {
        scrollArea->setStyleSheet(
            QString("QScrollArea { background-color: %1; border: none; }"
                    "QScrollBar:vertical { background-color: %1; width: 10px; }"
                    "QScrollBar::handle:vertical { background-color: %2; border-radius: 5px; }")
            .arg(theme->backgroundColor(), theme->secondaryTextColor())
        );
    }
    
    QWidget* contentWidget = findChild<QWidget*>("contentWidget");
    if (contentWidget) {
        contentWidget->setStyleSheet(QString("QWidget#contentWidget { background-color: %1; }").arg(theme->backgroundColor()));
        
        // 3. Rebuild Content
        qDeleteAll(contentWidget->children());
        QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);
        contentLayout->setContentsMargins(40, 40, 40, 40);
        contentLayout->setSpacing(30);
        
        // Weather-Powered Recommendations
        WeatherService *weather = WeatherService::instance();
        bool isDark = theme->currentTheme() == ThemeManager::Dark;
             if (weather->hasData()) {
            // ── Weather Status Banner ──
            QFrame *weatherBanner = new QFrame();
            weatherBanner->setStyleSheet(
                QString("QFrame { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
                        "stop:0 %1, stop:1 %2); border-radius: 12px; }")
                .arg(isDark ? "#134E4A" : "#F0FDFA", isDark ? "#1E293B" : "#ECFDF5")
            );
            QHBoxLayout *bannerLayout = new QHBoxLayout(weatherBanner);
            bannerLayout->setContentsMargins(25, 20, 25, 20);
            bannerLayout->setSpacing(15);
            
            QLabel *weatherIcon = new QLabel("🌍");
            weatherIcon->setStyleSheet("font-size: 32px; border: none; background: transparent;");
            
            QVBoxLayout *bannerText = new QVBoxLayout();
            bannerText->setSpacing(4);
            QLabel *bannerTitle = new QLabel("🛰️ Live Weather Integration Active");
            bannerTitle->setStyleSheet(
                QString("QLabel { color: %1; font-size: 16px; font-weight: bold; border: none; background: transparent; }")
                .arg(isDark ? "#5EEAD4" : "#0D9488")
            );
            
            DayWeather todayWeather = weather->getWeather(QDate::currentDate());
            QString todayText = "No data for today";
            if (todayWeather.weatherCode >= 0) {
                todayText = QString("Today in Tunis: %1 %2 | %3°C / %4°C")
                    .arg(todayWeather.icon, todayWeather.description)
                    .arg(todayWeather.tempMax, 0, 'f', 0)
                    .arg(todayWeather.tempMin, 0, 'f', 0);
            }
            QLabel *bannerSub = new QLabel(todayText);
            bannerSub->setStyleSheet(
                QString("QLabel { color: %1; font-size: 13px; border: none; background: transparent; }")
                .arg(theme->secondaryTextColor())
            );
            
            QLabel *creditLabel = new QLabel("<a href='https://open-meteo.com/' style='color:#14B8A6;'>Weather data by Open-Meteo.com</a>");
            creditLabel->setOpenExternalLinks(true);
            creditLabel->setStyleSheet("QLabel { font-size: 11px; border: none; background: transparent; }");
            
            bannerText->addWidget(bannerTitle);
            bannerText->addWidget(bannerSub);
            bannerText->addWidget(creditLabel);
            
            bannerLayout->addWidget(weatherIcon);
            bannerLayout->addLayout(bannerText, 1);
            contentLayout->addWidget(weatherBanner);
            
            // ── Section Title ──
            QLabel *recsTitle = new QLabel("🎯 Smart Session Recommendations");
            recsTitle->setStyleSheet(
                QString("QLabel {"
                "    color: %1;"
                "    font-size: 22px;"
                "    font-weight: bold;"
                "    margin-bottom: 15px;"
                "}").arg(theme->primaryTextColor())
            );
            contentLayout->addWidget(recsTitle);
            
            QLabel *recsSubtitle = new QLabel("Personalized sessions based on your habits, weather, and instructor availability");
            recsSubtitle->setStyleSheet(
                QString("QLabel { color: %1; font-size: 13px; font-style: italic; margin-bottom: 5px; }")
                .arg(theme->secondaryTextColor())
            );
            contentLayout->addWidget(recsSubtitle);
            
            // ── Collect Data ──
            int studentId = qApp->property("currentUserId").toInt();
            if (studentId == 0) return; // Strict: no fallback
            
            int schoolId = 5;
            QSqlQuery qsc;
            qsc.prepare("SELECT driving_school_id FROM STUDENTS WHERE id = :id");
            qsc.bindValue(":id", studentId);
            if (qsc.exec() && qsc.next() && !qsc.value(0).isNull()) {
                schoolId = qsc.value(0).toInt();
            }

            QString activeStageStr = "Code Theory"; // default
            QSqlQuery qProg;
            qProg.prepare("SELECT code_status, circuit_status, parking_status FROM USER_PROGRESS WHERE user_id = :student_id");
            qProg.bindValue(":student_id", studentId);
            if (qProg.exec() && qProg.next()) {
                QString cStatus = qProg.value(0).toString();
                QString circStatus = qProg.value(1).toString();
                QString pStatus = qProg.value(2).toString();
                
                if (pStatus == "IN_PROGRESS" || pStatus == "ACTIVE") activeStageStr = "Parking";
                else if (circStatus == "IN_PROGRESS" || circStatus == "ACTIVE") activeStageStr = "Circuit";
                else if (cStatus == "IN_PROGRESS" || cStatus == "ACTIVE") activeStageStr = "Code Theory";
                else if (cStatus == "COMPLETED" && circStatus != "COMPLETED") activeStageStr = "Circuit";
                else if (circStatus == "COMPLETED" && pStatus != "COMPLETED") activeStageStr = "Parking";
            }

            // Student Habit
            QString prefDay = "";
            QString prefTime = "10:00:00";
            QSqlQuery qPref;
            qPref.prepare("SELECT TO_CHAR(session_date, 'D') as d_num, start_time as s_time, COUNT(*) as cnt "
                          "FROM SESSION_BOOKING WHERE student_id = :id "
                          "GROUP BY TO_CHAR(session_date, 'D'), start_time ORDER BY COUNT(*) DESC");
            qPref.bindValue(":id", studentId);
            if (qPref.exec() && qPref.next()) {
                prefDay = qPref.value(0).toString(); // '1'=Sunday in Oracle depending on NLS, let's just use start_time
                prefTime = qPref.value(1).toString();
            }

            // Gather instructors
            QList<int> instructors;
            QSqlQuery qInst;
            qInst.prepare("SELECT id FROM INSTRUCTORS WHERE instructor_status = 'ACTIVE' AND driving_school_id = :school_id");
            qInst.bindValue(":school_id", schoolId);
            if (qInst.exec()) {
                while (qInst.next()) instructors.append(qInst.value(0).toInt());
            }
            if (instructors.isEmpty()) instructors.append(1); // fallback
            
            // ── Build weather-driven recommendation cards ──
            struct WeatherDay {
                QDate date;
                DayWeather weather;
                int suitability;
                QString sessionType;
                QString reason;
                QString timeSlot;
                bool isPreferredHabit;
                int instructorId;
            };
            
            QList<WeatherDay> days;
            
            for (int i = 1; i <= 30; i++) {
                QDate date = QDate::currentDate().addDays(i);
                DayWeather dw = weather->getWeather(date);
                if (dw.weatherCode < 0) continue;
                
                int suitability = WeatherService::weatherCodeToSuitability(dw.weatherCode);
                if (suitability < 50) continue; // skip bad weather days
                
                int dNum = date.dayOfWeek();
                QString dtStr;
                switch(dNum) {
                    case 1: dtStr = "MONDAY"; break;
                    case 2: dtStr = "TUESDAY"; break;
                    case 3: dtStr = "WEDNESDAY"; break;
                    case 4: dtStr = "THURSDAY"; break;
                    case 5: dtStr = "FRIDAY"; break;
                    case 6: dtStr = "SATURDAY"; break;
                    case 7: dtStr = "SUNDAY"; break;
                }
                
                QString foundTime = "";
                bool isPref = false;
                int foundInstId = instructors.isEmpty() ? 1 : instructors.first();

                // Test prefTime first
                int pd = prefTime.left(2).toInt();
                bool prefAvail = false;
                for (int instId : instructors) {
                    // check availability table
                    QSqlQuery qAva;
                    qAva.prepare("SELECT start_time FROM AVAILABILITY WHERE instructor_id = :id AND day_of_week = :d AND is_active = 1");
                    qAva.bindValue(":id", instId);
                    qAva.bindValue(":d", dtStr);
                    bool instHasAvail = false;
                    if (qAva.exec()) {
                        while (qAva.next()) {
                            if (qAva.value(0).toString().left(2).toInt() == pd) instHasAvail = true;
                        }
                    }
                    if (instHasAvail) {
                        // check overlap
                        QSqlQuery qb;
                        qb.prepare("SELECT start_time, end_time FROM SESSION_BOOKING WHERE instructor_id = :id AND TRUNC(session_date) = :sd AND status IN ('CONFIRMED', 'COMPLETED')");
                        qb.bindValue(":id", instId);
                        qb.bindValue(":sd", date);
                        bool overlap = false;
                        if (qb.exec()) {
                            while (qb.next()) {
                                int sH = qb.value(0).toString().left(2).toInt();
                                int eH = qb.value(1).toString().left(2).toInt();
                                if (pd >= sH && pd < eH) overlap = true;
                                if (sH == pd) overlap = true;
                            }
                        }
                        if (!overlap) {
                            prefAvail = true;
                            foundInstId = instId;
                            break;
                        }
                    }
                }

                if (prefAvail) {
                    foundTime = prefTime.left(5);
                    isPref = true;
                } else {
                    // find arbitrary available
                    for (int h = 9; h <= 16; h++) {
                        for (int instId : instructors) {
                            QSqlQuery qAva;
                            qAva.prepare("SELECT start_time FROM AVAILABILITY WHERE instructor_id = :id AND day_of_week = :d AND is_active = 1");
                            qAva.bindValue(":id", instId);
                            qAva.bindValue(":d", dtStr);
                            bool instHasAvail = false;
                            if (qAva.exec()) {
                                while (qAva.next()) {
                                    if (qAva.value(0).toString().left(2).toInt() == h) instHasAvail = true;
                                }
                            }
                            if (instHasAvail) {
                                QSqlQuery qb;
                                qb.prepare("SELECT start_time, end_time FROM SESSION_BOOKING WHERE instructor_id = :id AND TRUNC(session_date) = :sd AND status IN ('CONFIRMED', 'COMPLETED')");
                                qb.bindValue(":id", instId);
                                qb.bindValue(":sd", date);
                                bool overlap = false;
                                if (qb.exec()) {
                                    while(qb.next()){
                                        int sH = qb.value(0).toString().left(2).toInt();
                                        int eH = qb.value(1).toString().left(2).toInt();
                                        if (h >= sH && h < eH) overlap = true;
                                        if (sH == h) overlap = true;
                                    }
                                }
                                if (!overlap) {
                                    foundTime = QString("%1:00").arg(h, 2, 10, QChar('0'));
                                    foundInstId = instId;
                                    break;
                                }
                            }
                        }
                        if (!foundTime.isEmpty()) break;
                    }
                }
                
                if (foundTime.isEmpty()) continue; // no instructor available
                
                WeatherDay wd;
                wd.date = date;
                wd.weather = dw;
                wd.suitability = suitability;
                wd.sessionType = activeStageStr;
                wd.timeSlot = foundTime;
                wd.isPreferredHabit = isPref;
                wd.instructorId = foundInstId;
                
                if (isPref) {
                    wd.reason = "Matches your usual booking time! " + WeatherService::weatherCodeToDrivingAdvice(dw.weatherCode);
                } else {
                    wd.reason = WeatherService::weatherCodeToDrivingAdvice(dw.weatherCode);
                }
                
                days.append(wd);
                // No hard cap — show all good days within 30-day window
            }
            
            // Sort by suitability (best first)
            std::sort(days.begin(), days.end(), [](const WeatherDay &a, const WeatherDay &b) {
                if (a.isPreferredHabit != b.isPreferredHabit) return a.isPreferredHabit;
                return a.suitability > b.suitability;
            });
            
            // Create grid of cards (2 columns)
            QGridLayout *gridLayout = new QGridLayout();
            gridLayout->setSpacing(20);
            gridLayout->setColumnStretch(0, 1);
            gridLayout->setColumnStretch(1, 1);
            
            for (int i = 0; i < days.size(); i++) {
                const WeatherDay &wd = days[i];
                gridLayout->addWidget(
                    createWeatherRecommendationCard(wd.date, wd.weather, QString("%1 (%2)").arg(wd.sessionType, wd.timeSlot), wd.reason, wd.instructorId),
                    i / 2, i % 2
                );
            }
            
            if (days.isEmpty()) {
                QLabel *noRecs = new QLabel("No available slots found with good weather in the next 30 days.");
                noRecs->setStyleSheet(QString("color: %1; font-size: 14px;").arg(theme->secondaryTextColor()));
                gridLayout->addWidget(noRecs, 0, 0);
            }

            // Add grid directly to contentLayout (outer scroll area already handles page scrolling)
            contentLayout->addLayout(gridLayout);
            
        } else {
            // No weather data yet — show loading
            QLabel *loadingLabel = new QLabel("⏳ Loading weather data from Open-Meteo...");
            loadingLabel->setStyleSheet(
                QString("QLabel { color: %1; font-size: 16px; padding: 40px; }").arg(theme->secondaryTextColor())
            );
            loadingLabel->setAlignment(Qt::AlignCenter);
            contentLayout->addWidget(loadingLabel);
        }
        
        contentLayout->addStretch();
    }
}

QWidget* AIRecommendations::createWeatherRecommendationCard(const QDate& date, const DayWeather& weather,
                                                             const QString& sessionType, const QString& reason, int instructorId)
{
    ThemeManager* theme = ThemeManager::instance();
    bool isDark = theme->currentTheme() == ThemeManager::Dark;
    int suitability = WeatherService::weatherCodeToSuitability(weather.weatherCode);
    
    QFrame *card = new QFrame();
    card->setStyleSheet(
        QString("QFrame {"
        "    background-color: %1;"
        "    border-radius: 12px;"
        "}").arg(theme->cardColor())
    );
    
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(15);
    shadow->setXOffset(0);
    shadow->setYOffset(3);
    shadow->setColor(QColor(0, 0, 0, 10));
    card->setGraphicsEffect(shadow);
    
    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(25, 25, 25, 25);
    layout->setSpacing(12);
    
    // ── Header: Weather icon + date + suitability badge ──
    QHBoxLayout *headerLayout = new QHBoxLayout();
    
    // Weather icon (large)
    QLabel *iconLabel = new QLabel(weather.icon);
    iconLabel->setStyleSheet("font-size: 36px; border: none; background: transparent;");
    
    QVBoxLayout *dateLayout = new QVBoxLayout();
    dateLayout->setSpacing(2);
    
    QLabel *dayLabel = new QLabel(date.toString("dddd"));
    dayLabel->setStyleSheet(
        QString("QLabel { color: %1; font-size: 18px; font-weight: bold; border: none; }")
        .arg(theme->primaryTextColor())
    );
    
    QLabel *fullDateLabel = new QLabel(date.toString("MMMM d, yyyy"));
    fullDateLabel->setStyleSheet(
        QString("QLabel { color: %1; font-size: 13px; border: none; }")
        .arg(theme->secondaryTextColor())
    );
    
    dateLayout->addWidget(dayLabel);
    dateLayout->addWidget(fullDateLabel);
    
    // Suitability badge
    QString badgeText, badgeBg, badgeColor;
    if (suitability >= 80) {
        badgeText = "★ IDEAL";
        badgeBg = isDark ? "#134E4A" : "#ECFDF5";
        badgeColor = isDark ? "#34D399" : "#059669";
    } else if (suitability >= 60) {
        badgeText = "GOOD";
        badgeBg = isDark ? "#1E3A5F" : "#EFF6FF";
        badgeColor = isDark ? "#60A5FA" : "#2563EB";
    } else if (suitability >= 40) {
        badgeText = "CAUTION";
        badgeBg = isDark ? "#422006" : "#FEF9C3";
        badgeColor = isDark ? "#FBBF24" : "#D97706";
    } else {
        badgeText = "⚠ AVOID";
        badgeBg = isDark ? "#450A0A" : "#FEE2E2";
        badgeColor = isDark ? "#F87171" : "#DC2626";
    }
    
    QLabel *badge = new QLabel(badgeText);
    badge->setStyleSheet(
        QString("QLabel { background-color: %1; color: %2; font-size: 11px; font-weight: bold;"
                "    padding: 4px 10px; border-radius: 8px; border: none; }")
        .arg(badgeBg, badgeColor)
    );
    
    headerLayout->addWidget(iconLabel);
    headerLayout->addLayout(dateLayout, 1);
    headerLayout->addWidget(badge);
    layout->addLayout(headerLayout);
    
    // ── Divider ──
    QFrame *line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet(QString("background-color: %1; border: none;").arg(theme->borderColor()));
    line->setFixedHeight(1);
    layout->addWidget(line);
    
    // ── Weather details ──
    QHBoxLayout *detailsRow = new QHBoxLayout();
    detailsRow->setSpacing(20);
    
    QLabel *weatherDesc = new QLabel(QString("🌡️ %1").arg(weather.description));
    weatherDesc->setStyleSheet(
        QString("QLabel { color: %1; font-size: 14px; font-weight: 500; border: none; }")
        .arg(theme->primaryTextColor())
    );
    
    QLabel *tempLabel = new QLabel(QString("↑ %1°C  ↓ %2°C")
        .arg(weather.tempMax, 0, 'f', 0)
        .arg(weather.tempMin, 0, 'f', 0));
    tempLabel->setStyleSheet(
        QString("QLabel { color: %1; font-size: 14px; font-weight: 600; border: none; }")
        .arg(isDark ? "#5EEAD4" : "#0D9488")
    );
    
    QLabel *typeLabel = new QLabel(QString("🚗 %1").arg(sessionType));
    typeLabel->setStyleSheet(
        QString("QLabel { color: %1; font-size: 13px; border: none; }").arg(theme->secondaryTextColor())
    );
    
    detailsRow->addWidget(weatherDesc);
    detailsRow->addWidget(tempLabel);
    detailsRow->addStretch();
    detailsRow->addWidget(typeLabel);
    layout->addLayout(detailsRow);
    
    // ── Suitability bar ──
    QHBoxLayout *suitRow = new QHBoxLayout();
    QLabel *suitLabel = new QLabel("Driving Suitability");
    suitLabel->setStyleSheet(
        QString("QLabel { color: %1; font-size: 12px; border: none; }").arg(theme->secondaryTextColor())
    );
    QLabel *suitScore = new QLabel(QString("%1%").arg(suitability));
    QString suitScoreColor = suitability >= 80 ? (isDark ? "#34D399" : "#059669") :
                             suitability >= 50 ? (isDark ? "#FBBF24" : "#D97706") :
                                                 (isDark ? "#F87171" : "#DC2626");
    suitScore->setStyleSheet(
        QString("QLabel { color: %1; font-size: 12px; font-weight: bold; border: none; }").arg(suitScoreColor)
    );
    suitRow->addWidget(suitLabel);
    suitRow->addStretch();
    suitRow->addWidget(suitScore);
    layout->addLayout(suitRow);
    
    QProgressBar *suitBar = new QProgressBar();
    suitBar->setMaximum(100);
    suitBar->setValue(suitability);
    suitBar->setTextVisible(false);
    suitBar->setFixedHeight(6);
    QString barColor = suitability >= 80 ? (isDark ? "#34D399" : "#10B981") :
                       suitability >= 50 ? (isDark ? "#FBBF24" : "#F59E0B") :
                                           (isDark ? "#F87171" : "#EF4444");
    QString bgBar = isDark ? "#374151" : "#E5E7EB";
    suitBar->setStyleSheet(
        QString("QProgressBar { background-color: %2; border-radius: 3px; border: none; }"
                "QProgressBar::chunk { background-color: %1; border-radius: 3px; }")
        .arg(barColor, bgBar)
    );
    layout->addWidget(suitBar);
    
    // ── AI Driving Advice ──
    QLabel *reasonLabel = new QLabel(QString("💡 %1").arg(reason));
    reasonLabel->setStyleSheet(
        QString("QLabel { color: %1; font-size: 12px; border: none; font-style: italic; }").arg(theme->mutedTextColor())
    );
    reasonLabel->setWordWrap(true);
    layout->addWidget(reasonLabel);
    
    // ── Book button (only if suitability > 20) ──
    if (suitability > 20) {
        QPushButton *bookBtn = new QPushButton(suitability >= 60 ? "Book This Session" : "Book Anyway (Caution)");
        
        QString btnBg, btnHover;
        if (suitability >= 80) {
            btnBg = "#14B8A6"; btnHover = "#0D9488";
        } else if (suitability >= 60) {
            btnBg = "#14B8A6"; btnHover = "#0D9488";
        } else {
            btnBg = isDark ? "#92400E" : "#F59E0B"; 
            btnHover = isDark ? "#78350F" : "#D97706";
        }
        
        bookBtn->setStyleSheet(
            QString("QPushButton {"
                    "    background-color: %1;"
                    "    color: white;"
                    "    font-weight: bold;"
                    "    border-radius: 8px;"
                    "    padding: 10px;"
                    "    font-size: 13px;"
                    "    border: none;"
                    "}"
                    "QPushButton:hover {"
                    "    background-color: %2;"
                    "}").arg(btnBg, btnHover)
        );
        bookBtn->setCursor(Qt::PointingHandCursor);
        
        QString stage = sessionType.split(" (").first();
        QString timeStr = sessionType.split("(").last().remove(")");
        QFrame *cardPtr = card; // capture for lambda
        connect(bookBtn, &QPushButton::clicked, this, [=]() {
            this->bookSession(date, timeStr, stage, instructorId, cardPtr);
        });
        
        layout->addWidget(bookBtn);
    } else {
        QLabel *warnLabel = new QLabel("⛔ Session not recommended — dangerous conditions");
        warnLabel->setStyleSheet(
            QString("QLabel { color: %1; font-size: 12px; font-weight: bold; border: none; "
                    "    background-color: %2; padding: 8px; border-radius: 6px; }")
            .arg(isDark ? "#F87171" : "#DC2626", isDark ? "#450A0A" : "#FEE2E2")
        );
        warnLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(warnLabel);
    }
    
    return card;
}

QWidget* AIRecommendations::createPerformanceOverview()
{
    ThemeManager* theme = ThemeManager::instance();
    
    QFrame *card = new QFrame();
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
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(25);
    
    QLabel *titleLabel = new QLabel("📈 Performance Overview");
    titleLabel->setStyleSheet(
        QString("QLabel {"
        "    color: %1;"
        "    font-size: 20px;"
        "    font-weight: bold;"
        "    line-height: 1.4;"
        "    padding: 5px 0;"
        "}").arg(theme->primaryTextColor())
    );
    layout->addWidget(titleLabel);
    
    // Skills grid
    QGridLayout *skillsGrid = new QGridLayout();
    skillsGrid->setSpacing(15);
    
    skillsGrid->addWidget(createSkillBar("Steering Control", 85), 0, 0);
    skillsGrid->addWidget(createSkillBar("Speed Management", 78), 0, 1);
    skillsGrid->addWidget(createSkillBar("Lane Positioning", 92), 1, 0);
    skillsGrid->addWidget(createSkillBar("Parallel Parking", 62), 1, 1);
    skillsGrid->addWidget(createSkillBar("Traffic Awareness", 88), 2, 0);
    skillsGrid->addWidget(createSkillBar("Signal Usage", 95), 2, 1);
    
    layout->addLayout(skillsGrid);
    
    return card;
}

QWidget* AIRecommendations::createSkillBar(const QString& skillName, int score)
{
    ThemeManager* theme = ThemeManager::instance();
    bool isDark = theme->currentTheme() == ThemeManager::Dark;
    
    QWidget *skillWidget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(skillWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    
    QHBoxLayout *headerLayout = new QHBoxLayout();
    
    QLabel *nameLabel = new QLabel(skillName);
    nameLabel->setStyleSheet(
        QString("QLabel {"
        "    color: %1;"
        "    font-size: 14px;"
        "    font-weight: 500;"
        "    line-height: 1.5;"
        "    padding: 3px 0;"
        "}").arg(theme->secondaryTextColor())
    );
    
    QLabel *scoreLabel = new QLabel(QString::number(score) + "%");
    QString scoreColor = score >= 80 ? (isDark ? "#34D399" : "#059669") : (score >= 60 ? (isDark ? "#FBBF24" : "#F59E0B") : (isDark ? "#F87171" : "#DC2626"));
    scoreLabel->setStyleSheet(
        QString("QLabel {"
                "    color: %1;"
                "    font-size: 14px;"
                "    font-weight: bold;"
                "    line-height: 1.5;"
                "    padding: 3px 0;"
                "}").arg(scoreColor)
    );
    
    headerLayout->addWidget(nameLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(scoreLabel);
    
    layout->addLayout(headerLayout);
    
    QProgressBar *progressBar = new QProgressBar();
    progressBar->setMaximum(100);
    progressBar->setValue(score);
    progressBar->setTextVisible(false);
    progressBar->setFixedHeight(8);
    
    QString barColor = score >= 80 ? (isDark ? "#34D399" : "#10B981") : (score >= 60 ? (isDark ? "#FBBF24" : "#F59E0B") : (isDark ? "#F87171" : "#EF4444"));
    QString bgBar = isDark ? "#374151" : "#E5E7EB";
    
    progressBar->setStyleSheet(
        QString("QProgressBar {"
                "    background-color: %2;"
                "    border-radius: 4px;"
                "    border: none;"
                "}"
                "QProgressBar::chunk {"
                "    background-color: %1;"
                "    border-radius: 4px;"
                "}").arg(barColor, bgBar)
    );
    
    layout->addWidget(progressBar);
    
    return skillWidget;
}

QWidget* AIRecommendations::createRecommendationCard(const QString& title, const QString& category,
                                                     const QString& description, const QString& bgColor,
                                                     const QString& accentColor)
{
    Q_UNUSED(bgColor);
    ThemeManager* theme = ThemeManager::instance();
    
    QFrame *card = new QFrame();
    card->setStyleSheet(
        QString("QFrame {"
                "    background-color: %1;"
                "    border-radius: 12px;"
                "    border-left: 4px solid %2;"
                "}").arg(theme->cardColor(), accentColor)
    );
    
    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(25, 20, 25, 20);
    layout->setSpacing(12);
    
    QLabel *titleLabel = new QLabel(title);
    titleLabel->setStyleSheet(
        QString("QLabel { color: %1; font-size: 17px; font-weight: bold; }").arg(theme->primaryTextColor())
    );
    layout->addWidget(titleLabel);
    
    QLabel *categoryLabel = new QLabel(category);
    categoryLabel->setStyleSheet(
        QString("QLabel { color: %1; font-size: 12px; font-weight: 600; }").arg(accentColor)
    );
    layout->addWidget(categoryLabel);
    
    QLabel *descLabel = new QLabel(description);
    descLabel->setStyleSheet(
        QString("QLabel { color: %1; font-size: 14px; }").arg(theme->secondaryTextColor())
    );
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel);
    
    return card;
}

QWidget* AIRecommendations::createActionableRecommendationCard(const QString& type, const QString& date, 
                                                           const QString& time, const QString& reason, 
                                                           const QString& icon, const QString& color)
{
    Q_UNUSED(color);
    ThemeManager* theme = ThemeManager::instance();
    
    QFrame *card = new QFrame();
    card->setStyleSheet(
        QString("QFrame { background-color: %1; border-radius: 12px; }").arg(theme->cardColor())
    );
    
    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(25, 25, 25, 25);
    layout->setSpacing(15);
    
    QLabel *iconLabel = new QLabel(icon);
    iconLabel->setStyleSheet("font-size: 24px; border: none; background: transparent;");
    
    QLabel *typeLabel = new QLabel(type);
    typeLabel->setStyleSheet(
        QString("QLabel { color: %1; font-size: 18px; font-weight: bold; border: none; }").arg(theme->primaryTextColor())
    );
    
    layout->addWidget(iconLabel);
    layout->addWidget(typeLabel);
    
    QLabel *dateLabel = new QLabel(QString("📅 %1 | ⏰ %2").arg(date, time));
    dateLabel->setStyleSheet(
        QString("QLabel { color: %1; font-size: 13px; border: none; }").arg(theme->secondaryTextColor())
    );
    layout->addWidget(dateLabel);
    
    QLabel *reasonLabel = new QLabel(reason);
    reasonLabel->setStyleSheet(
        QString("QLabel { color: %1; font-size: 12px; border: none; }").arg(theme->mutedTextColor())
    );
    reasonLabel->setWordWrap(true);
    layout->addWidget(reasonLabel);
    
    return card;
}

void AIRecommendations::bookSession(const QDate& date, const QString& timeStr, const QString& sessionType, int instructorId, QWidget* cardToRemove)
{
    int studentId = qApp->property("currentUserId").toInt();
    if (studentId == 0) return; // Strict: no fallback
    
    QTime startTime = QTime::fromString(timeStr, "HH:mm");
    QTime endTime = startTime.addSecs(3600); // 1 hour by default
    double amount = 40.0; // Standard 1 hour price
    
    // Determine session_step
    int step = 1;
    if (sessionType.contains("Circuit", Qt::CaseInsensitive)) step = 2;
    else if (sessionType.contains("Parking", Qt::CaseInsensitive)) step = 3;
    
    // Check if student credit exceeds 200 DT
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

    double currentDebt = totalDue - totalPaid;
    if (currentDebt > 200) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Accès Refusé");
        msgBox.setText("⚠️ Plafond de crédit atteint");
        msgBox.setInformativeText(QString("Désolé, vous ne pouvez pas réserver de nouvelle séance. Votre solde débiteur actuel est de <b>%1 TND</b>, ce qui dépasse la limite autorisée de 200 TND.<br><br>Veuillez régulariser votre situation en effectuant un paiement depuis votre tableau de bord afin de pouvoir effectuer de nouvelles réservations.").arg(currentDebt, 0, 'f', 0));
        msgBox.setIcon(QMessageBox::Warning);
        ThemeManager* theme = ThemeManager::instance();
        msgBox.setStyleSheet(
            QString("QMessageBox { background-color: %1; }"
            "QLabel { color: %2; font-size: 14px; line-height: 1.5; }"
            "QPushButton { background-color: #14B8A6; color: white; font-weight: bold; border: none; border-radius: 6px; padding: 10px 20px; }"
            "QPushButton:hover { background-color: #0F9D8E; }")
            .arg(theme->cardColor(), theme->primaryTextColor())
        );
        msgBox.exec();
        return;
    }

    QSqlQuery insertQuery;
    insertQuery.prepare("INSERT INTO SESSION_BOOKING (STUDENT_ID, INSTRUCTOR_ID, SESSION_STEP, SESSION_DATE, START_TIME, END_TIME, AMOUNT, STATUS) "
                        "VALUES (:student, :instructor, :step, :date, :start, :end, :amount, 'CONFIRMED')");
    insertQuery.bindValue(":student", studentId); 
    insertQuery.bindValue(":instructor", instructorId);
    insertQuery.bindValue(":step", step);
    insertQuery.bindValue(":date", date);
    insertQuery.bindValue(":start", startTime.toString("HH:mm"));
    insertQuery.bindValue(":end", endTime.toString("HH:mm"));
    insertQuery.bindValue(":amount", amount);
    
    if (!insertQuery.exec()) {
        QMessageBox::critical(this, "Database Error", "Failed to book session: " + insertQuery.lastError().text());
        return;
    }

    emit sessionBooked();

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Booking Confirmed");
    msgBox.setText("✓ Your AI Recommended session has been booked successfully!");
    msgBox.setInformativeText("The session is now on your calendar and your balance has been updated.");
    msgBox.setIcon(QMessageBox::Information);
    ThemeManager* theme = ThemeManager::instance();
    msgBox.setStyleSheet(
        QString("QMessageBox {"
        "    background-color: %1;"
        "}"
        "QLabel {"
        "    color: %2;"
        "    font-size: 14px;"
        "    line-height: 1.5;"
        "    padding: 3px 0;"
        "}"
        "QPushButton {"
        "    background-color: #14B8A6;"
        "    color: white;"
        "    font-weight: bold;"
        "    border: none;"
        "    border-radius: 6px;"
        "    padding: 10px 24px;"
        "    min-width: 80px;"
        "    line-height: 1.5;"
        "}"
        "QPushButton:hover {"
        "    background-color: #0F9D8E;"
        "}").arg(theme->cardColor(), theme->primaryTextColor())
    );
    msgBox.exec();
    
    // Hide the booked card from the recommendations grid
    if (cardToRemove) {
        cardToRemove->setVisible(false);
    }
}
