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
#include <QSet>

AIRecommendations::AIRecommendations(QWidget *parent) :
    QWidget(parent)
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

void AIRecommendations::refreshContent()
{
    // Rebuild recommendation cards with latest data
    if (!WeatherService::instance()->hasData()) {
        WeatherService::instance()->fetchForecast(); // will trigger onWeatherDataReady when done
    } else {
        updateColors(); // data already available — rebuild immediately
    }
}

void AIRecommendations::setupUI()
{
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
    connect(backBtn, &QPushButton::clicked, this, &AIRecommendations::backRequested);
    
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
            
            // ── Collect student data ──────────────────────────────────────────────
            int studentId = qApp->property("currentUserId").toInt();
            if (studentId == 0) return;

            int schoolId = 5;
            { QSqlQuery q; q.prepare("SELECT school_id FROM STUDENTS WHERE id=:id");
              q.bindValue(":id", studentId);
              if (q.exec() && q.next() && !q.value(0).isNull()) schoolId = q.value(0).toInt(); }

            // Active learning stage
            QString activeStageStr = "Code Theory";
            { QSqlQuery q;
              q.prepare("SELECT code_status, circuit_status, parking_status FROM WINO_PROGRESS WHERE user_id=:id");
              q.bindValue(":id", studentId);
              if (q.exec() && q.next()) {
                  QString cs = q.value(0).toString(), ci = q.value(1).toString(), pk = q.value(2).toString();
                  if      (pk == "IN_PROGRESS" || pk == "ACTIVE")  activeStageStr = "Parking";
                  else if (ci == "IN_PROGRESS" || ci == "ACTIVE")  activeStageStr = "Circuit";
                  else if (cs == "IN_PROGRESS" || cs == "ACTIVE")  activeStageStr = "Code Theory";
                  else if (cs == "COMPLETED"   && ci != "COMPLETED") activeStageStr = "Circuit";
                  else if (ci == "COMPLETED"   && pk != "COMPLETED") activeStageStr = "Parking";
              } }

            bool isCircuit = (activeStageStr == "Circuit");

            // ── CIRCUIT: fetch performance + weak points from DB ────────────────
            // WeakPoint: one entry per unique category from RECOMMENDATIONS table
            struct WeakPoint {
                QString category;
                QString message;
                QString suggestion;
                QString priority;         // CRITICAL | HIGH | MEDIUM | LOW
                int     priorityOrder;    // 1..4 (1 = most urgent)
            };
            QList<WeakPoint> weakPoints;

            if (isCircuit) {
                // ── Latest session performance banner (SESSION_ANALYSIS) ──────────
                QSqlQuery qAna;
                qAna.prepare(
                    "SELECT sa.overall_performance, sa.stress_index, sa.risk_score, sa.ml_risk_level "
                    "FROM SESSION_ANALYSIS sa "
                    "JOIN DRIVING_SESSIONS ds ON sa.driving_session_id = ds.driving_session_id "
                    "WHERE ds.student_id = :sid "
                    "ORDER BY sa.analysis_date DESC"
                );
                qAna.bindValue(":sid", studentId);
                if (qAna.exec() && qAna.next()) {
                    QString overallPerf = qAna.value(0).toString();
                    double  stressIdx   = qAna.value(1).toDouble();
                    double  riskScore   = qAna.value(2).toDouble();
                    QString mlRisk      = qAna.value(3).toString();

                    QString borderCol = riskScore > 60 ? (isDark ? "#F87171" : "#EF4444")
                                      : riskScore > 35 ? (isDark ? "#FBBF24" : "#F59E0B")
                                                       : (isDark ? "#34D399" : "#10B981");

                    QFrame *perfCard = new QFrame();
                    perfCard->setStyleSheet(QString(
                        "QFrame { background-color: %1; border-radius: 12px;"
                        "         border-left: 4px solid %2; }")
                        .arg(theme->cardColor(), borderCol));
                    QHBoxLayout *pLay = new QHBoxLayout(perfCard);
                    pLay->setContentsMargins(20, 14, 20, 14); pLay->setSpacing(16);

                    QLabel *pIcon = new QLabel(riskScore > 60 ? "⚠️" : riskScore > 35 ? "📊" : "✅");
                    pIcon->setStyleSheet("font-size: 26px; border:none; background:transparent;");

                    QVBoxLayout *ptxt = new QVBoxLayout(); ptxt->setSpacing(3);
                    QLabel *pt1 = new QLabel(QString("📈 Dernière séance circuit — %1").arg(overallPerf));
                    pt1->setStyleSheet(QString("QLabel{color:%1;font-size:15px;font-weight:bold;border:none;background:transparent;}").arg(theme->primaryTextColor()));
                    QLabel *pt2 = new QLabel(QString("Stress: %1%  ·  Risque: %2%  ·  Niveau IA: %3")
                        .arg(stressIdx,0,'f',1).arg(riskScore,0,'f',1).arg(mlRisk));
                    pt2->setStyleSheet(QString("QLabel{color:%1;font-size:12px;border:none;background:transparent;}").arg(theme->secondaryTextColor()));
                    ptxt->addWidget(pt1); ptxt->addWidget(pt2);
                    pLay->addWidget(pIcon); pLay->addLayout(ptxt, 1);
                    contentLayout->addWidget(perfCard);
                }

                // ── Weak points from RECOMMENDATIONS table ────────────────────────
                QSqlQuery qRec;
                qRec.prepare(
                    "SELECT r.category, r.message, r.suggestion, r.priority "
                    "FROM RECOMMENDATIONS r "
                    "JOIN DRIVING_SESSIONS ds ON r.driving_session_id = ds.driving_session_id "
                    "WHERE ds.student_id = :sid "
                    "  AND r.priority IN ('CRITICAL','HIGH','MEDIUM') "
                    "ORDER BY CASE r.priority "
                    "           WHEN 'CRITICAL' THEN 1 WHEN 'HIGH' THEN 2 "
                    "           WHEN 'MEDIUM'   THEN 3 ELSE 4 END, "
                    "         r.created_date DESC"
                );
                qRec.bindValue(":sid", studentId);
                QSet<QString> seenCats;
                if (qRec.exec()) {
                    while (qRec.next()) {
                        QString cat = qRec.value(0).toString().trimmed();
                        if (cat.isEmpty() || seenCats.contains(cat)) continue;
                        seenCats.insert(cat);
                        WeakPoint wp;
                        wp.category      = cat;
                        wp.message       = qRec.value(1).toString();
                        wp.suggestion    = qRec.value(2).toString();
                        wp.priority      = qRec.value(3).toString();
                        wp.priorityOrder = (wp.priority == "CRITICAL") ? 1
                                         : (wp.priority == "HIGH")     ? 2
                                         : (wp.priority == "MEDIUM")   ? 3 : 4;
                        weakPoints.append(wp);
                    }
                }
            }

            // ── Section title ─────────────────────────────────────────────────────
            {
                QLabel *recsTitle = new QLabel("🎯 Recommandations personnalisées");
                recsTitle->setStyleSheet(QString(
                    "QLabel{color:%1;font-size:22px;font-weight:bold;margin-bottom:10px;}")
                    .arg(theme->primaryTextColor()));
                contentLayout->addWidget(recsTitle);

                QString sub = (isCircuit && !weakPoints.isEmpty())
                    ? QString("Séances ciblées sur vos %1 point(s) faible(s) identifié(s) — combinées avec la météo et vos créneaux préférés").arg(weakPoints.size())
                    : "Séances optimisées selon météo, créneaux favorables et disponibilités instructeur";
                QLabel *recsSub = new QLabel(sub);
                recsSub->setStyleSheet(QString(
                    "QLabel{color:%1;font-size:13px;font-style:italic;}")
                    .arg(theme->secondaryTextColor()));
                contentLayout->addWidget(recsSub);
            }

            // ── Student preferred time ────────────────────────────────────────────
            QString prefTime = "10:00:00";
            { QSqlQuery q;
              q.prepare("SELECT start_time FROM SESSION_BOOKING WHERE student_id=:id "
                        "GROUP BY start_time ORDER BY COUNT(*) DESC");
              q.bindValue(":id", studentId);
              if (q.exec() && q.next()) prefTime = q.value(0).toString(); }
            int prefHour = prefTime.left(2).toInt();

            // ── Active instructors ────────────────────────────────────────────────
            QList<int> instructors;
            { QSqlQuery q;
              q.prepare("SELECT id FROM INSTRUCTORS WHERE instructor_status='ACTIVE' AND school_id=:sid");
              q.bindValue(":sid", schoolId);
              if (q.exec()) while (q.next()) instructors.append(q.value(0).toInt());
              if (instructors.isEmpty()) instructors.append(1); }

            // ── Helper: find first available instructor slot on a given date ──────
            struct WeatherDay {
                QDate      date;
                DayWeather weather;
                int        suitability;
                QString    sessionType;
                QString    reason;
                QString    timeSlot;
                bool       isPreferredHabit;
                int        instructorId;
                int        weakPointIdx;   // index in weakPoints, -1 = general
            };

            auto dayName = [](int dow) -> QString {
                switch (dow) {
                    case 1: return "MONDAY";    case 2: return "TUESDAY";
                    case 3: return "WEDNESDAY"; case 4: return "THURSDAY";
                    case 5: return "FRIDAY";    case 6: return "SATURDAY";
                    default: return "SUNDAY";
                }
            };

            auto findSlot = [&](const QDate &date, int prefH, int &outInstId) -> QString {
                QString dStr = dayName(date.dayOfWeek());
                QList<int> hours; hours << prefH;
                for (int h = 9; h <= 16; ++h) if (h != prefH) hours << h;
                for (int h : hours) {
                    for (int instId : instructors) {
                        QSqlQuery qa;
                        qa.prepare("SELECT start_time FROM AVAILABILITY WHERE instructor_id=:id AND day_of_week=:d AND is_active=1");
                        qa.bindValue(":id", instId); qa.bindValue(":d", dStr);
                        bool avail = false;
                        if (qa.exec()) while (qa.next()) if (qa.value(0).toString().left(2).toInt() == h) avail = true;
                        if (!avail) continue;
                        QSqlQuery qb;
                        qb.prepare("SELECT start_time,end_time FROM SESSION_BOOKING "
                                   "WHERE instructor_id=:id AND TRUNC(session_date)=:sd "
                                   "AND status IN('CONFIRMED','COMPLETED')");
                        qb.bindValue(":id", instId); qb.bindValue(":sd", date);
                        bool overlap = false;
                        if (qb.exec()) while (qb.next()) {
                            int sH = qb.value(0).toString().left(2).toInt();
                            int eH = qb.value(1).toString().left(2).toInt();
                            if (h >= sH && h < eH) overlap = true;
                            if (sH == h) overlap = true;
                        }
                        if (!overlap) { outInstId = instId; return QString("%1:00").arg(h,2,10,QChar('0')); }
                    }
                }
                return {};
            };

            // ── Build recommendation day slots ────────────────────────────────────
            QList<WeatherDay> days;

            if (isCircuit && !weakPoints.isEmpty()) {
                // One best-weather slot per unique weak point
                for (int wi = 0; wi < weakPoints.size(); ++wi) {
                    WeatherDay best; best.weakPointIdx = wi; best.suitability = -1;
                    for (int i = 1; i <= 30; ++i) {
                        QDate d = QDate::currentDate().addDays(i);
                        DayWeather dw = weather->getWeather(d);
                        if (dw.weatherCode < 0) continue;
                        int suit = WeatherService::weatherCodeToSuitability(dw.weatherCode);
                        if (suit < 50 || suit <= best.suitability) continue;
                        // Don't reuse a date already assigned to another weak point
                        bool taken = false;
                        for (const WeatherDay &x : days) if (x.date == d) { taken = true; break; }
                        if (taken) continue;
                        int instId = instructors.first();
                        QString slot = findSlot(d, prefHour, instId);
                        if (slot.isEmpty()) continue;
                        best.date = d; best.weather = dw; best.suitability = suit;
                        best.sessionType = activeStageStr; best.timeSlot = slot;
                        best.isPreferredHabit = (slot.left(2).toInt() == prefHour);
                        best.instructorId = instId;
                        best.reason = QString("Focus : %1. %2")
                            .arg(weakPoints[wi].category,
                                 WeatherService::weatherCodeToDrivingAdvice(dw.weatherCode));
                    }
                    if (best.suitability >= 0) days.append(best);
                }
                // Complement with up to 2 general good-weather slots
                int extras = 0;
                for (int i = 1; i <= 30 && extras < 2; ++i) {
                    QDate d = QDate::currentDate().addDays(i);
                    DayWeather dw = weather->getWeather(d);
                    if (dw.weatherCode < 0) continue;
                    int suit = WeatherService::weatherCodeToSuitability(dw.weatherCode);
                    if (suit < 70) continue;
                    bool taken = false;
                    for (const WeatherDay &x : days) if (x.date == d) { taken = true; break; }
                    if (taken) continue;
                    int instId = instructors.first();
                    QString slot = findSlot(d, prefHour, instId);
                    if (slot.isEmpty()) continue;
                    WeatherDay wd; wd.date = d; wd.weather = dw; wd.suitability = suit;
                    wd.sessionType = activeStageStr; wd.timeSlot = slot;
                    wd.isPreferredHabit = (slot.left(2).toInt() == prefHour);
                    wd.instructorId = instId; wd.weakPointIdx = -1;
                    wd.reason = (wd.isPreferredHabit
                        ? "Correspond à vos créneaux habituels ! "
                        : QString()) + WeatherService::weatherCodeToDrivingAdvice(dw.weatherCode);
                    days.append(wd); ++extras;
                }
            } else {
                // Standard logic: Code Theory, Parking, or Circuit without prior sessions
                for (int i = 1; i <= 30; ++i) {
                    QDate d = QDate::currentDate().addDays(i);
                    DayWeather dw = weather->getWeather(d);
                    if (dw.weatherCode < 0) continue;
                    int suit = WeatherService::weatherCodeToSuitability(dw.weatherCode);
                    if (suit < 50) continue;
                    int instId = instructors.first();
                    QString slot = findSlot(d, prefHour, instId);
                    if (slot.isEmpty()) continue;
                    WeatherDay wd; wd.date = d; wd.weather = dw; wd.suitability = suit;
                    wd.sessionType = activeStageStr; wd.timeSlot = slot;
                    wd.isPreferredHabit = (slot.left(2).toInt() == prefHour);
                    wd.instructorId = instId; wd.weakPointIdx = -1;
                    wd.reason = (wd.isPreferredHabit
                        ? "Correspond à vos créneaux habituels ! "
                        : QString()) + WeatherService::weatherCodeToDrivingAdvice(dw.weatherCode);
                    days.append(wd);
                }
                std::sort(days.begin(), days.end(), [](const WeatherDay &a, const WeatherDay &b) {
                    if (a.isPreferredHabit != b.isPreferredHabit) return a.isPreferredHabit;
                    return a.suitability > b.suitability;
                });
            }

            // ── Render grid (2 columns) ───────────────────────────────────────────
            QGridLayout *gridLayout = new QGridLayout();
            gridLayout->setSpacing(20);
            gridLayout->setColumnStretch(0, 1);
            gridLayout->setColumnStretch(1, 1);

            for (int i = 0; i < days.size(); ++i) {
                const WeatherDay &wd = days[i];
                const WeakPoint  *wp = (wd.weakPointIdx >= 0 && wd.weakPointIdx < weakPoints.size())
                                       ? &weakPoints[wd.weakPointIdx] : nullptr;
                gridLayout->addWidget(
                    createWeatherRecommendationCard(
                        wd.date, wd.weather,
                        QString("%1 (%2)").arg(wd.sessionType, wd.timeSlot),
                        wd.reason, wd.instructorId,
                        wp ? wp->category   : QString(),
                        wp ? wp->message    : QString(),
                        wp ? wp->suggestion : QString(),
                        wp ? wp->priority   : QString()
                    ),
                    i / 2, i % 2
                );
            }

            if (days.isEmpty()) {
                QLabel *noRecs = new QLabel("Aucun créneau disponible avec une météo favorable dans les 30 prochains jours.");
                noRecs->setStyleSheet(QString("QLabel{color:%1;font-size:14px;}").arg(theme->secondaryTextColor()));
                gridLayout->addWidget(noRecs, 0, 0);
            }

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
                                                             const QString& sessionType, const QString& reason,
                                                             int instructorId,
                                                             const QString& weakCategory,
                                                             const QString& weakMessage,
                                                             const QString& weakSuggestion,
                                                             const QString& weakPriority)
{
    const bool hasWeakPoint = !weakCategory.isEmpty();
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
    
    // ── Weather driving advice ──
    QLabel *reasonLabel = new QLabel(QString("💡 %1").arg(reason));
    reasonLabel->setStyleSheet(QString(
        "QLabel { color: %1; font-size: 12px; border: none; font-style: italic; }")
        .arg(theme->mutedTextColor()));
    reasonLabel->setWordWrap(true);
    layout->addWidget(reasonLabel);

    // ── Circuit weak point block (only shown when linked to a recommendation) ──
    if (hasWeakPoint) {
        // Priority badge colours
        QString priBg, priColor, priLabel;
        if (weakPriority == "CRITICAL") {
            priBg    = isDark ? "#450A0A" : "#FEE2E2";
            priColor = isDark ? "#F87171" : "#DC2626";
            priLabel = "🔴 CRITIQUE";
        } else if (weakPriority == "HIGH") {
            priBg    = isDark ? "#422006" : "#FFF7ED";
            priColor = isDark ? "#FBBF24" : "#EA580C";
            priLabel = "🟠 PRIORITAIRE";
        } else {
            priBg    = isDark ? "#1E3A5F" : "#EFF6FF";
            priColor = isDark ? "#60A5FA" : "#2563EB";
            priLabel = "🔵 À AMÉLIORER";
        }

        QFrame *wpBox = new QFrame();
        wpBox->setStyleSheet(QString(
            "QFrame { background-color: %1; border: 1px solid %2;"
            "         border-left: 4px solid %2; border-radius: 8px; }")
            .arg(priBg, priColor));
        QVBoxLayout *wpLay = new QVBoxLayout(wpBox);
        wpLay->setContentsMargins(12, 10, 12, 10);
        wpLay->setSpacing(6);

        // Header row: category + priority badge
        QHBoxLayout *wpHeader = new QHBoxLayout(); wpHeader->setSpacing(8);
        QLabel *wpCat = new QLabel(QString("🎯 Point faible : %1").arg(weakCategory));
        wpCat->setStyleSheet(QString(
            "QLabel{color:%1;font-size:13px;font-weight:bold;border:none;background:transparent;}")
            .arg(priColor));
        QLabel *wpBadge = new QLabel(priLabel);
        wpBadge->setStyleSheet(QString(
            "QLabel{color:%1;font-size:10px;font-weight:bold;background-color:%2;"
            "padding:3px 8px;border-radius:6px;border:none;}")
            .arg(priColor, priBg));
        wpHeader->addWidget(wpCat, 1);
        wpHeader->addWidget(wpBadge);
        wpLay->addLayout(wpHeader);

        // Message (observation from instructor/AI)
        if (!weakMessage.isEmpty()) {
            QLabel *wpMsg = new QLabel(weakMessage);
            wpMsg->setWordWrap(true);
            wpMsg->setStyleSheet(QString(
                "QLabel{color:%1;font-size:12px;border:none;background:transparent;}")
                .arg(theme->secondaryTextColor()));
            wpLay->addWidget(wpMsg);
        }

        // Suggestion (actionable advice)
        if (!weakSuggestion.isEmpty()) {
            QFrame *sugBox = new QFrame();
            sugBox->setStyleSheet(QString(
                "QFrame{background-color:%1;border-radius:6px;border:none;}")
                .arg(isDark ? "#1E293B" : "#F9FAFB"));
            QHBoxLayout *sugLay = new QHBoxLayout(sugBox);
            sugLay->setContentsMargins(8, 6, 8, 6); sugLay->setSpacing(6);
            QLabel *bulb = new QLabel("✏️");
            bulb->setStyleSheet("font-size:13px;border:none;background:transparent;");
            QLabel *wpSug = new QLabel(weakSuggestion);
            wpSug->setWordWrap(true);
            wpSug->setStyleSheet(QString(
                "QLabel{color:%1;font-size:12px;font-style:italic;border:none;background:transparent;}")
                .arg(theme->primaryTextColor()));
            sugLay->addWidget(bulb);
            sugLay->addWidget(wpSug, 1);
            wpLay->addWidget(sugBox);
        }

        layout->addWidget(wpBox);
    }

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
