#ifndef ROLESELECTIONWINDOW_H
#define ROLESELECTIONWINDOW_H

#include <QMainWindow>

namespace Ui {
class RoleSelectionWindow;
}

class RoleSelectionWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit RoleSelectionWindow(QWidget *parent = nullptr);
    ~RoleSelectionWindow();

private slots:
    void onAdminClicked();
    void onStudentClicked();
    void onInstructorClicked();

private:
    Ui::RoleSelectionWindow *ui;
};

#endif // ROLESELECTIONWINDOW_H
