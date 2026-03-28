#ifndef STUDENTDASHBOARD_H
#define STUDENTDASHBOARD_H

#include <QMainWindow>

namespace Ui {
class StudentDashboard;
}

class StudentDashboard : public QMainWindow
{
    Q_OBJECT

public:
    explicit StudentDashboard(QWidget *parent = nullptr);
    ~StudentDashboard();

    // Called by login to pre-set profile so AI dialog is skipped
    void setStudentInfo(const QString &fullName, const QString &email, bool hasDrivingLicense);
    void setStudentId(int id) { m_studentId = id; }

private slots:
    void loadSchools();
    void onSearchChanged(const QString &text);
    void onViewDetailsClicked(int schoolId);
    void onLogoutClicked();

private:
    Ui::StudentDashboard *ui;
    bool m_isNewDriver = true;   // AI profile: true=new, false=has license
    bool m_profilePreSet = false; // true = came from login, skip dialog
    QString m_studentName;
    QString m_studentEmail;
    int m_studentId = 0;
    void askDriverProfile();
    void loadSchoolsFiltered(const QString &search);
    void setupSchoolCard(QWidget *card, int schoolId, const QString &name,
                        int students, int vehicles, double rating,
                        int manualCount, int autoCount);
};

#endif // STUDENTDASHBOARD_H
