#include "roleselectionwindow.h"
#include "ui_roleselectionwindow.h"
#include "admindashboard.h"
#include "studentdashboard.h"
#include "instructordashboard.h"

RoleSelectionWindow::RoleSelectionWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::RoleSelectionWindow)
{
    ui->setupUi(this);
    setWindowTitle("Wino - Driving School Management System");
    
    // Connect buttons
    connect(ui->adminButton, &QPushButton::clicked, this, &RoleSelectionWindow::onAdminClicked);
    connect(ui->studentButton, &QPushButton::clicked, this, &RoleSelectionWindow::onStudentClicked);
    connect(ui->instructorButton, &QPushButton::clicked, this, &RoleSelectionWindow::onInstructorClicked);
}

RoleSelectionWindow::~RoleSelectionWindow()
{
    delete ui;
}

void RoleSelectionWindow::onAdminClicked()
{
    AdminDashboard *admin = new AdminDashboard();
    admin->showMaximized();
    this->hide();
}

void RoleSelectionWindow::onStudentClicked()
{
    StudentDashboard *student = new StudentDashboard();
    student->showMaximized();
    this->hide();
}

void RoleSelectionWindow::onInstructorClicked()
{
    InstructorDashboard *instructor = new InstructorDashboard();
    instructor->showMaximized();
    this->hide();
}
