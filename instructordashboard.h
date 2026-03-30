#ifndef INSTRUCTORDASHBOARD_H
#define INSTRUCTORDASHBOARD_H

#include <QMainWindow>
#include <QWidget>
#include <QLabel>
#include <QTableWidget>
#include <QPushButton>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QStackedWidget>
#include <QDate>
#include <QSet>
#include <QMap>
#include <QVBoxLayout>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QMap>
#include <QVBoxLayout>
#include <QRadioButton>
#include <QDoubleSpinBox>
#include <QApplication>
#include "weatherservice.h"

class InstructorDashboard : public QMainWindow
{
    Q_OBJECT

public:
    explicit InstructorDashboard(QWidget *parent = nullptr);
    ~InstructorDashboard();

    // Compatibility methods for mainwindow.cpp integration
    void setSchoolId(int /*schoolId*/) {}
    void setInstructorId(int id) { qApp->setProperty("currentUserId", id); }
    void init() {}

private slots:
    void onBackClicked();
    void onScheduleTabClicked();
    void onPaymentTabClicked();
    void onExamsTabClicked();
    void onPrevWeek();
    void onNextWeek();
    void onCellClicked(int dayOffset, int hour);
    void onDayHeaderClicked(int dayOffset);
    void onVerifyPayment(int paymentId, const QString& studentName);
    void onRejectPayment(int paymentId, const QString& studentName);
    void onAcceptExamRequest(int requestId);
    void onRejectExamRequest(int requestId);
    void onRecordExamResult(int requestId);
    void onThemeChanged();
    void showBookingDetails(const QString& key);
    void onStudentsTabClicked();
    void showPendingRequests();

private:
    void setupUI();
    void updateWeekDisplay();
    void rebuildScheduleGrid();
    void updateSlotSummary();
    void updateColors();
    void showDaySummary(const QDate& date);

    // UI Helpers
    QWidget* createStatsCard(const QString& title, const QString& value, 
                             const QString& icon, const QString& bgColor,
                             const QString& iconColor, bool firstCard = false);
    QWidget* createSpecialtiesSection();
    QWidget* createTabBar();
    QWidget* createWeeklySchedule();
    QWidget* createPaymentSection();
    QWidget* createExamsSection();
    QWidget* createStudentsSection();
    QWidget* createPaymentCard(int paymentId, const QString& studentName, const QString& sessionType,
                               const QString& transactionCode, const QString& sessionDate,
                               const QString& amount, const QString& submittedDate);

    // Members
    QStackedWidget *tabContent;
    QPushButton *scheduleTab;
    QPushButton *paymentTab;
    QPushButton *examsTab;
    QPushButton *studentsTab;
    QPushButton *circuitTabBtn;
    QLabel *weekRangeLabel;
    QDate currentWeekStart;
    QPushButton* themeToggleBtn;
    QWidget* centralWidget;
    
    // Schedule grid container
    QWidget *gridContainer;
    QVBoxLayout *gridContainerLayout;
    
    // Slot summary labels
    QLabel *availCountLabel;
    QLabel *bookedCountLabel;
    QLabel *totalCountLabel;
    
    // Data: available slots stored as "YYYY-MM-DD_HH" keys
    QSet<QString> availableSlots;
    
    // Booked slots: key -> student name
    QMap<QString, QString> bookedSlots;
};

#endif // INSTRUCTORDASHBOARD_H
