#ifndef INSTRUCTORDASHBOARD_H
#define INSTRUCTORDASHBOARD_H

#include <QtWidgets>
#include <QMainWindow>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QFileDialog>
#include <QDate>
#include <QPixmap>

namespace Ui {
class InstructorDashboard;
}

class CircuitDashboard;
class WinoInstructorDashboard;
class AdminWidget;

class InstructorDashboard : public QMainWindow
{
    Q_OBJECT

public:
    explicit InstructorDashboard(QWidget *parent = nullptr);
    ~InstructorDashboard();

    // Called by login to set the correct school and instructor
    void setSchoolId(int id) { schoolId = id; }
    void setInstructorId(int id) { instructorId = id; }
    void init() { loadData(); }

private slots:
    void loadData();
    void switchTab(int index);
    void onAcceptStudent(int studentId);
    void onRejectStudent(int studentId);
    void onAddVehicle();
    void onLogoutClicked();

private:
    Ui::InstructorDashboard *ui;
    int schoolId;
    int instructorId = 0;  // 0 = show all students for the school (fallback)
    CircuitDashboard        *m_circuitDashboard  = nullptr;
    QWidget                 *m_circuitTab        = nullptr;
    WinoInstructorDashboard *m_winoTab           = nullptr;
    QWidget                 *m_winoTabWidget     = nullptr;
    AdminWidget             *m_adminTab          = nullptr;
    QWidget                 *m_adminTabWidget    = nullptr;
    void setupStudentCard(QWidget *card, int studentId, const QString &name, const QString &email,
                         const QString &phone, const QString &birthDate, const QString &requestedDate);
    void setupApprovedStudentCard(QWidget *card, int studentId, const QString &name,
                                 const QString &email, const QString &phone);
    void setupVehicleCard(QWidget *card, int vehicleId, const QString &brand, const QString &model,
                         int year, const QString &plateNumber, const QString &transmission, const QString &status, const QString &photoPath = "");
};

#endif // INSTRUCTORDASHBOARD_H
