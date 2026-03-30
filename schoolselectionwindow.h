#ifndef SCHOOLSELECTIONWINDOW_H
#define SCHOOLSELECTIONWINDOW_H

#include <QMainWindow>
#include <QSqlQuery>
#include <QMessageBox>

namespace Ui {
class SchoolSelectionWindow;
}

class SchoolSelectionWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit SchoolSelectionWindow(int studentId, const QString &fullName, const QString &email, bool hasDrivingLicense, QWidget *parent = nullptr);
    ~SchoolSelectionWindow();

signals:
    void schoolSelected(); // emitted when successful

private slots:
    void onSearchChanged(const QString &text);
    void onLogoutClicked();
    
private:
    Ui::SchoolSelectionWindow *ui;
    
    int m_studentId;
    QString m_studentName;
    QString m_studentEmail;
    bool m_isNewDriver;
    bool m_profilePreSet;

    void askDriverProfile();
    void loadSchools();
    void loadSchoolsFiltered(const QString &search);
    void setupSchoolCard(QWidget *card, int schoolId, const QString &name,
                         int students, int vehicles, double rating,
                         int manualCount, int autoCount);
    void onViewDetailsClicked(int schoolId);
};

#endif // SCHOOLSELECTIONWINDOW_H
