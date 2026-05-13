#ifndef WINO_STUDENTDASHBOARD_H
#define WINO_STUDENTDASHBOARD_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QCalendarWidget>
#include <QFrame>
#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QScrollArea>
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
#include "airecommendations.h"
#include "bookingsession.h"
#include <QPrinter>
#include <QPainter>
#include <QFileDialog>
#include <QStackedWidget>

// Structure pour représenter une séance
struct Session {
    int id;
    QDate date;
    QTime time;
    int duration;  // en minutes
    QString type;  // "Code", "Circuit", "Parking"
    QString instructor;
};

class WinoStudentDashboard : public QWidget
{
    Q_OBJECT

public:
    explicit WinoStudentDashboard(QWidget *parent = nullptr);
    ~WinoStudentDashboard();
    void navigateToSection(int section);  // 0=Dashboard 1=Book 2=AI 3=Calendar

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
    void showExamRequestDialog(int examStep);   // kept for legacy; now routes to page
    QWidget* createExamPage();
    void refreshExamPage();

    // Membres
    QCalendarWidget *calendarWidget = nullptr;  // only set in createCalendarSection() (lazy)
    QList<Session> sessions;
    QPushButton* themeToggleBtn = nullptr;
    QPushButton* examBtn        = nullptr;
    QWidget* centralWidget      = nullptr;      // the dashboard page (stack index 0)

    // ── Embedded page navigation ─────────────────────────────────────────────
    QStackedWidget    *m_mainStack  = nullptr;  // 0=dashboard, 1=AI, 2=Exam
    QScrollArea       *m_dashScroll = nullptr;  // main dashboard scroll area
    QWidget           *m_calSection = nullptr;  // calendar section widget (for scroll-to)
    AIRecommendations *m_aiPage     = nullptr;
    QWidget           *m_examPage       = nullptr;
    QVBoxLayout       *m_examBodyLayout = nullptr; // refreshable inner layout
    QWidget           *m_examTopBar     = nullptr; // for theme updates
    QScrollArea       *m_examScroll     = nullptr; // for theme updates
    QWidget           *m_examBodyWidget = nullptr; // for theme updates
    BookingSession    *m_bookingPage    = nullptr; // embedded booking (stack index 4)
};

#endif // WINO_STUDENTDASHBOARD_H
