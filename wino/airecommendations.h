#ifndef AIRECOMMENDATIONS_H
#define AIRECOMMENDATIONS_H

#include <QWidget>
#include <QLabel>
#include <QFrame>
#include <QVBoxLayout>
#include "weatherservice.h"



class AIRecommendations : public QWidget
{
    Q_OBJECT

public:
    explicit AIRecommendations(QWidget *parent = nullptr);
    ~AIRecommendations();

    // Re-render the recommendation cards (call when navigating to the page)
    void refreshContent();

private:

    void setupUI();
    QWidget* createPerformanceOverview();
    QWidget* createRecommendationCard(const QString& title, const QString& category,
                                     const QString& description, const QString& bgColor,
                                     const QString& accentColor);
    QWidget* createActionableRecommendationCard(const QString& type, const QString& date, 
                                               const QString& time, const QString& reason, 
                                               const QString& icon, const QString& color);
    QWidget* createSkillBar(const QString& skillName, int score);
    QWidget* createWeatherRecommendationCard(const QDate& date, const DayWeather& weather,
                                             const QString& sessionType, const QString& reason,
                                             int instructorId,
                                             const QString& weakCategory   = QString(),
                                             const QString& weakMessage    = QString(),
                                             const QString& weakSuggestion = QString(),
                                             const QString& weakPriority   = QString());
    void updateColors();

public slots:
    void bookSession(const QDate& date, const QString& timeStr, const QString& sessionType, int instructorId, QWidget* cardToRemove = nullptr);

signals:
    void sessionBooked();
    void backRequested();   // emitted when the user clicks "← Back to Dashboard"

private slots:
    void onThemeChanged();
    void onWeatherDataReady();
};

#endif // AIRECOMMENDATIONS_H
