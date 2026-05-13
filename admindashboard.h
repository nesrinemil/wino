#ifndef ADMINDASHBOARD_H
#define ADMINDASHBOARD_H

#include <QMainWindow>

namespace Ui {
class AdminDashboard;
}

class AdminDashboard : public QMainWindow
{
    Q_OBJECT

public:
    explicit AdminDashboard(QWidget *parent = nullptr);
    ~AdminDashboard();

private slots:
    void loadSchools();
    void filterSchools(const QString &filter);
    void onSuspendClicked(int schoolId);
    void onActivateClicked(int schoolId);
    void onRemoveClicked(int schoolId, const QString &name);
    void onViewClicked(int schoolId);
    void onLogoutClicked();
    void onAddSchoolClicked();

private:
    Ui::AdminDashboard *ui;
    void setupSchoolCard(QWidget *card, int schoolId, const QString &name,
                        int students, int vehicles, const QString &status,
                        const QString &logoPath = QString());
    void ensureLogoColumn();        // ALTER TABLE driving_schools ADD logo_path
    void ensureInstrPhotoColumn();  // ALTER TABLE instructors ADD photo_path
    void uploadLogo(int schoolId);
    static QPixmap makeCircularPixmap(const QPixmap &src, int size);
};

#endif // ADMINDASHBOARD_H
