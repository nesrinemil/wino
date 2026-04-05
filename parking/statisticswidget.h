#ifndef STATISTICSWIDGET_H
#define STATISTICSWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>

class StatisticsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit StatisticsWidget(const QString &userName, const QString &userRole,
                              int userId, QWidget *parent = nullptr);

public slots:
    void refreshData();

private:
    void setupUI();
    QWidget* createStatCard(const QString &icon, const QString &defaultVal,
                             const QString &label, const QString &color,
                             QLabel **valRef, QLabel **trendRef);
    QWidget* createChartCard();
    QWidget* createActivityFeed();
    QWidget* createProgressCard();

    QString m_userName, m_userRole;
    bool m_isMoniteur;
    int m_eleveId;

    // Dynamic stat labels
    QLabel *m_valSessions, *m_trendSessions;
    QLabel *m_valTaux, *m_trendTaux;
    QLabel *m_valHeures, *m_trendHeures;
    QLabel *m_valBest, *m_trendBest;

    // Maneuver mastery bars
    QList<QProgressBar*> m_bars;
    QList<QLabel*> m_pcts;

    // Activity feed
    QVBoxLayout *m_activityLayout;
};

#endif // STATISTICSWIDGET_H
