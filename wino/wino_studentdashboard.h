#ifndef WINO_STUDENTDASHBOARD_H
#define WINO_STUDENTDASHBOARD_H

#include <QMainWindow>
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QCalendarWidget>
#include <QFrame>
#include <QDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QClipboard>
#include <QApplication>
#include <QTimer>
#include <QMap>
#include <QGridLayout>
#include <QDate>
#include <QTime>
#include <QList>
#include "weatherservice.h"
#include <QPrinter>
#include <QPainter>
#include <QFileDialog>

// Structure pour représenter une séance
struct Session {
    int id;
    QDate date;
    QTime time;
    int duration;  // en minutes
    QString type;  // "Code", "Circuit", "Parking"
    QString instructor;
};

class WinoStudentDashboard : public QMainWindow
{
    Q_OBJECT

public:
    explicit WinoStudentDashboard(QWidget *parent = nullptr);
    ~WinoStudentDashboard();

private slots:
    void onBackClicked();
    void onBookSessionClicked();
    void onAIRecommendationsClicked();
    void onPayWithD17Clicked();
    void onPaymentHistoryClicked();
    void onCalendarDateClicked(const QDate &date);
    void onExamRequestClicked();
    void onThemeChanged();
    void onWeatherDataReady();

private:
    void setupUI();
    void initializeSessions();
    QWidget* createInfoCard(const QString& title, const QString& value, 
                           const QString& subtitle, const QString& color);
    QWidget* createBalanceCardWithPayment();
    QWidget* createProgressSection();
    QWidget* createStageBox(const QString& title, const QString& subtitle, int state);
    QWidget* createCalendarSection();
    void updateColors();
    void updateBalance();
    
    // CRUD pour les séances
    void updateCalendarHighlights();
    void showSessionDialog(Session &session);
    void editSession(Session &session);
    void deleteSession(int sessionId);
    void showExamRequestDialog(int examStep);
    
    // Membres
    QCalendarWidget *calendarWidget;
    QList<Session> sessions;
    QPushButton* themeToggleBtn;
    QPushButton* examBtn;
    QWidget* centralWidget;
};

#endif // WINO_STUDENTDASHBOARD_H
