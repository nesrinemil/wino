#include "instructordashboard.h"
#include "ui_instructordashboard.h"
#include "mainwindow.h"
#include "database.h"
#include "C:/Users/hboug/Downloads/final/SmartDrivingSchool/SmartDrivingSchool/circuitdashboard.h"
#include "C:/Users/hboug/Downloads/final/SmartDrivingSchool/SmartDrivingSchool/circuitdb.h"
#include "wino/wino_instructordashboard.h"
#include "wino/wino_bootstrap.h"
#include "wino/adminwidget.h"
#include "parking/parkingdbmanager.h"
#include <QtWidgets>
#include <QSettings>
#include <QSqlQuery>
#include <QSqlError>
#include <QFileDialog>
#include <QDate>
#include <QFileInfo>
#include <QPixmap>
#include <QPainter>
#include <QStandardPaths>
#include <QProcess>
#include <QTemporaryFile>
#include <QRegularExpression>
#include <QTimer>

InstructorDashboard::InstructorDashboard(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::InstructorDashboard),
    schoolId(3)
{
    ui->setupUi(this);
    setWindowTitle("Wino - Module 2 - Instructor Space");

    connect(ui->logoutButton, &QPushButton::clicked, this, &InstructorDashboard::onLogoutClicked);
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &InstructorDashboard::switchTab);

    // ── Circuit Analysis tab (lazy-loaded on first switch) ───────────────────
    m_circuitTab = new QWidget();
    m_circuitTab->setLayout(new QVBoxLayout(m_circuitTab));
    ui->tabWidget->addTab(m_circuitTab, "⚡ Circuit");

    // ── WINO Sessions tab (lazy-loaded on first switch) ───────────────────────
    m_winoTabWidget = new QWidget();
    m_winoTabWidget->setLayout(new QVBoxLayout(m_winoTabWidget));
    ui->tabWidget->addTab(m_winoTabWidget, "📅 Sessions");

    // ── Administration tab (lazy-loaded on first switch) ──────────────────────
    m_adminTabWidget = new QWidget();
    m_adminTabWidget->setLayout(new QVBoxLayout(m_adminTabWidget));
    ui->tabWidget->addTab(m_adminTabWidget, QString::fromUtf8("\xf0\x9f\x85\xbf\xef\xb8\x8f Parking"));

    // ── Hide the QTabWidget tab bar (we use a custom sidebar instead) ─────────
    ui->tabWidget->tabBar()->hide();
    ui->tabWidget->setDocumentMode(true);
    ui->tabWidget->setStyleSheet(
        "QTabWidget::pane { border: none; background: transparent; }");

    // ── Detach contentWidget and wrap it in a horizontal splitter ─────────────
    // Main vertical layout: [headerWidget, contentWidget]
    QLayout *rootVLayout = ui->centralwidget->layout();
    rootVLayout->removeWidget(ui->contentWidget);

    // Horizontal container: [sidebar | contentWidget]
    QWidget *hContainer = new QWidget(ui->centralwidget);
    hContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QHBoxLayout *hLayout = new QHBoxLayout(hContainer);
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->setSpacing(0);

    // ── Build the sidebar ──────────────────────────────────────────────────────
    QFrame *instrSidebar = new QFrame(hContainer);
    instrSidebar->setFixedWidth(230);
    instrSidebar->setObjectName("instrSidebar");
    instrSidebar->setStyleSheet(
        "QFrame#instrSidebar { background: #111827; border-right: 1px solid #1E293B; }"
        "QFrame#instrSidebar * { background: transparent; }");

    QVBoxLayout *sideLayout = new QVBoxLayout(instrSidebar);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    sideLayout->setSpacing(0);

    // Brand header
    QLabel *sideLogo = new QLabel(QString::fromUtf8("🏫"), instrSidebar);
    sideLogo->setAlignment(Qt::AlignCenter);
    sideLogo->setStyleSheet("font-size: 32px; padding: 20px 0 4px 0;");

    QLabel *sideBrand = new QLabel("SmartDrive", instrSidebar);
    sideBrand->setAlignment(Qt::AlignCenter);
    sideBrand->setStyleSheet(
        "font-size: 16px; font-weight: bold; color: #14B8A6; padding-bottom: 2px;");

    QLabel *sideSub = new QLabel("Instructor Space", instrSidebar);
    sideSub->setAlignment(Qt::AlignCenter);
    sideSub->setStyleSheet("font-size: 11px; color: #64748B; padding-bottom: 12px;");

    sideLayout->addWidget(sideLogo);
    sideLayout->addWidget(sideBrand);
    sideLayout->addWidget(sideSub);

    // Helper: horizontal divider
    auto addSideDivider = [&]() {
        QFrame *d = new QFrame(instrSidebar);
        d->setFrameShape(QFrame::HLine);
        d->setFixedHeight(1);
        d->setStyleSheet("background: #1E293B; margin: 0;");
        sideLayout->addWidget(d);
    };
    addSideDivider();
    sideLayout->addSpacing(10);

    // Nav button factory (matches student area style exactly)
    auto makeNavBtn = [&](const QString &icon, const QString &text) -> QPushButton* {
        QPushButton *btn = new QPushButton(instrSidebar);
        btn->setFixedHeight(48);
        btn->setText(QString("  %1   %2").arg(icon, text));
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QPushButton { background: transparent; color: #64748B; "
            "border: none; border-left: 3px solid transparent; "
            "text-align: left; padding-left: 16px; "
            "font-size: 14px; font-weight: 500; border-radius: 0; }"
            "QPushButton:hover { background: rgba(255,255,255,0.05); color: #E2E8F0; }");
        return btn;
    };

    // Section label: MANAGEMENT
    QLabel *mgmtLbl = new QLabel("  MANAGEMENT", instrSidebar);
    mgmtLbl->setStyleSheet(
        "font-size: 10px; font-weight: bold; color: #64748B; "
        "letter-spacing: 2px; padding: 0 20px 8px 20px;");
    sideLayout->addWidget(mgmtLbl);

    m_navRequests = makeNavBtn(QString::fromUtf8("📝"), "Requests");
    m_navStudents = makeNavBtn(QString::fromUtf8("👥"), "Students");
    m_navVehicles = makeNavBtn(QString::fromUtf8("🚗"), "Vehicles");
    sideLayout->addWidget(m_navRequests);
    sideLayout->addWidget(m_navStudents);
    sideLayout->addWidget(m_navVehicles);

    addSideDivider();
    sideLayout->addSpacing(8);

    // Section label: TRAINING
    QLabel *trainingLbl = new QLabel("  TRAINING", instrSidebar);
    trainingLbl->setStyleSheet(
        "font-size: 10px; font-weight: bold; color: #64748B; "
        "letter-spacing: 2px; padding: 0 20px 8px 20px;");
    sideLayout->addWidget(trainingLbl);

    m_navCircuit  = makeNavBtn(QString::fromUtf8("⚡"), "Circuit");
    m_navSessions = makeNavBtn(QString::fromUtf8("📅"), "Sessions");
    m_navParking  = makeNavBtn(QString::fromUtf8("🅿️"), "Parking");
    sideLayout->addWidget(m_navCircuit);
    sideLayout->addWidget(m_navSessions);
    sideLayout->addWidget(m_navParking);

    sideLayout->addStretch();
    addSideDivider();

    // ── Theme toggle (matches student area bottom button) ─────────────────────
    bool initDark = ThemeManager::instance()->currentTheme() == ThemeManager::Dark;
    m_themeBtn = new QPushButton(instrSidebar);
    m_themeBtn->setFixedHeight(48);
    m_themeBtn->setText(initDark ? QString::fromUtf8("  ☀️   Light Mode")
                                 : QString::fromUtf8("  🌙   Dark Mode"));
    m_themeBtn->setCursor(Qt::PointingHandCursor);
    sideLayout->addWidget(m_themeBtn);

    addSideDivider();

    // Logout at the bottom
    QPushButton *logoutSideBtn = new QPushButton(instrSidebar);
    logoutSideBtn->setFixedHeight(48);
    logoutSideBtn->setText(QString::fromUtf8("  🚪   Logout"));
    logoutSideBtn->setCursor(Qt::PointingHandCursor);
    logoutSideBtn->setStyleSheet(
        "QPushButton { background: transparent; color: #EF4444; "
        "border: none; border-left: 3px solid transparent; "
        "text-align: left; padding-left: 16px; "
        "font-size: 14px; font-weight: 500; border-radius: 0; }"
        "QPushButton:hover { background: rgba(239,68,68,0.08); }");
    connect(logoutSideBtn, &QPushButton::clicked,
            this, &InstructorDashboard::onLogoutClicked);
    sideLayout->addWidget(logoutSideBtn);

    // ── Store sidebar reference (for theme updates) ────────────────────────────
    m_sidebar = instrSidebar;

    // ── Apply initial theme + connect theme changes ───────────────────────────
    updateSidebarTheme();
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &InstructorDashboard::updateSidebarTheme);
    connect(m_themeBtn, &QPushButton::clicked, this, [this]() {
        ThemeManager::instance()->toggleTheme();
    });

    // ── Wire nav buttons to tab indices ───────────────────────────────────────
    connect(m_navRequests, &QPushButton::clicked, this, [this]() {
        ui->tabWidget->setCurrentIndex(0); setActiveNavBtn(m_navRequests); });
    connect(m_navStudents, &QPushButton::clicked, this, [this]() {
        ui->tabWidget->setCurrentIndex(1); setActiveNavBtn(m_navStudents); });
    connect(m_navVehicles, &QPushButton::clicked, this, [this]() {
        ui->tabWidget->setCurrentIndex(2); setActiveNavBtn(m_navVehicles); });
    connect(m_navCircuit,  &QPushButton::clicked, this, [this]() {
        ui->tabWidget->setCurrentIndex(3); setActiveNavBtn(m_navCircuit); });
    connect(m_navSessions, &QPushButton::clicked, this, [this]() {
        ui->tabWidget->setCurrentIndex(4); setActiveNavBtn(m_navSessions); });
    connect(m_navParking,  &QPushButton::clicked, this, [this]() {
        ui->tabWidget->setCurrentIndex(5); setActiveNavBtn(m_navParking); });

    // Highlight Requests as default active page
    setActiveNavBtn(m_navRequests);

    // ── Assemble horizontal container ─────────────────────────────────────────
    ui->contentWidget->setParent(hContainer);
    hLayout->addWidget(instrSidebar);
    hLayout->addWidget(ui->contentWidget, 1);
    rootVLayout->addWidget(hContainer);

    // loadData() is called by init() after login sets schoolId + instructorId
}

InstructorDashboard::~InstructorDashboard()
{
    delete ui;
}

void InstructorDashboard::loadData()
{
    QSqlQuery query;
    
    // Helper: build WHERE clause that filters by instructor_id when known
    // instructorId == 0 means no match found → show all students for the school
    auto bindInstr = [this](QSqlQuery &q) {
        q.addBindValue(schoolId);
        if (instructorId > 0) q.addBindValue(instructorId);
    };
    QString instrFilter = (instructorId > 0)
        ? " AND instructor_id = ?"
        : "";

    // ── Update sidebar nav button labels with live counts ────────────────────
    query.prepare("SELECT COUNT(*) FROM students WHERE school_id = ?" + instrFilter + " AND status = 'pending'");
    bindInstr(query);
    if (query.exec() && query.next()) {
        int n = query.value(0).toInt();
        if (m_navRequests)
            m_navRequests->setText(QString("  📝   Requests  (%1)").arg(n));
    }

    query.prepare("SELECT COUNT(*) FROM students WHERE school_id = ?" + instrFilter + " AND status = 'approved'");
    bindInstr(query);
    if (query.exec() && query.next()) {
        int n = query.value(0).toInt();
        if (m_navStudents)
            m_navStudents->setText(QString("  👥   Students  (%1)").arg(n));
    }

    query.prepare("SELECT COUNT(*) FROM cars WHERE driving_school_id = ?");
    query.addBindValue(schoolId);
    if (query.exec() && query.next()) {
        int n = query.value(0).toInt();
        if (m_navVehicles)
            m_navVehicles->setText(QString("  🚗   Vehicles  (%1)").arg(n));
    }
    
    // Load pending requests
    QLayout* oldLayout = ui->requestsContainer->layout();
    if (oldLayout) {
        QLayoutItem* item;
        while ((item = oldLayout->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        delete oldLayout;
    }
    
    QVBoxLayout *requestsLayout = new QVBoxLayout(ui->requestsContainer);
    requestsLayout->setSpacing(15);
    requestsLayout->setContentsMargins(24, 24, 24, 24);
    
    query.prepare("SELECT id, name, email, phone, birth_date, requested_date FROM students WHERE school_id = ?" + instrFilter + " AND status = 'pending' ORDER BY requested_date DESC");
    bindInstr(query);
    
    if (query.exec()) {
        while (query.next()) {
            QWidget *card = new QWidget();
            setupStudentCard(card, query.value(0).toInt(), query.value(1).toString(),
                           query.value(2).toString(), query.value(3).toString(),
                           query.value(4).toString(), query.value(5).toString());
            requestsLayout->addWidget(card);
        }
    }
    requestsLayout->addStretch();
    
    // Load approved students
    oldLayout = ui->studentsContainer->layout();
    if (oldLayout) {
        QLayoutItem* item;
        while ((item = oldLayout->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        delete oldLayout;
    }
    
    QVBoxLayout *studentsLayout = new QVBoxLayout(ui->studentsContainer);
    studentsLayout->setSpacing(15);
    studentsLayout->setContentsMargins(24, 24, 24, 24);
    
    query.prepare("SELECT id, name, email, phone, cin FROM students WHERE school_id = ?" + instrFilter + " AND status = 'approved'");
    bindInstr(query);

    if (query.exec()) {
        while (query.next()) {
            QWidget *card = new QWidget();
            setupApprovedStudentCard(card,
                                     query.value(0).toInt(),
                                     query.value(1).toString(),
                                     query.value(2).toString(),
                                     query.value(3).toString(),
                                     query.value(4).toString());
            studentsLayout->addWidget(card);
        }
    }
    studentsLayout->addStretch();
    
    // Load vehicles
    oldLayout = ui->vehiclesContainer->layout();
    if (oldLayout) {
        QLayoutItem* item;
        while ((item = oldLayout->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        delete oldLayout;
    }
    
    QVBoxLayout *vehiclesLayout = new QVBoxLayout(ui->vehiclesContainer);
    vehiclesLayout->setSpacing(15);
    vehiclesLayout->setContentsMargins(24, 24, 24, 24);
    
    // Add "Add Vehicle" button at top
    QPushButton *addVehicleBtn = new QPushButton("+ Add Vehicle");
    addVehicleBtn->setStyleSheet("background-color: #14B8A6; color: white; border: none; border-radius: 8px; padding: 12px 24px; font-size: 14px; font-weight: bold;");
    addVehicleBtn->setMinimumHeight(50);
    addVehicleBtn->setMaximumWidth(150);
    connect(addVehicleBtn, &QPushButton::clicked, this, &InstructorDashboard::onAddVehicle);
    vehiclesLayout->addWidget(addVehicleBtn, 0, Qt::AlignRight);
    
    query.prepare("SELECT id, brand, model, year, plate_number, transmission, CASE WHEN is_active=1 THEN 'active' ELSE 'inactive' END, image_path FROM cars WHERE driving_school_id = ?");
    query.addBindValue(schoolId);
    
    if (query.exec()) {
        while (query.next()) {
            QWidget *card = new QWidget();
            setupVehicleCard(card, query.value(0).toInt(), query.value(1).toString(),
                           query.value(2).toString(), query.value(3).toInt(),
                           query.value(4).toString(), query.value(5).toString(),
                           query.value(6).toString(), query.value(7).toString());
            vehiclesLayout->addWidget(card);
        }
    }
    vehiclesLayout->addStretch();
}

void InstructorDashboard::setupStudentCard(QWidget *card, int studentId, const QString &name, const QString &email,
                                           const QString &phone, const QString &birthDate, const QString &requestedDate)
{
    Q_UNUSED(birthDate);
    static int studCardCounter = 0;
    card->setObjectName(QString("studCard%1").arg(studCardCounter++));
    card->setStyleSheet("QWidget#" + card->objectName() + " { background-color: white; border-radius: 12px; border: 1px solid #E5E7EB; }");
    card->setMinimumHeight(120);

    QVBoxLayout *mainLayout = new QVBoxLayout(card);
    mainLayout->setContentsMargins(20, 15, 20, 15);
    mainLayout->setSpacing(10);
    
    QHBoxLayout *topLayout = new QHBoxLayout();
    QLabel *nameLabel = new QLabel(name);
    nameLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #1F1827;");
    topLayout->addWidget(nameLabel);
    topLayout->addStretch();
    
    QPushButton *acceptBtn = new QPushButton("✓ Accept");
    acceptBtn->setStyleSheet("background-color: #10B981; color: white; border: none; border-radius: 5px; padding: 8px 15px; font-weight: bold;");
    connect(acceptBtn, &QPushButton::clicked, [this, studentId]() { onAcceptStudent(studentId); });
    topLayout->addWidget(acceptBtn);
    
    QPushButton *rejectBtn = new QPushButton("✗ Reject");
    rejectBtn->setStyleSheet("background-color: #EF4444; color: white; border: none; border-radius: 5px; padding: 8px 15px; font-weight: bold;");
    connect(rejectBtn, &QPushButton::clicked, [this, studentId]() { onRejectStudent(studentId); });
    topLayout->addWidget(rejectBtn);
    
    mainLayout->addLayout(topLayout);
    
    QHBoxLayout *infoLayout = new QHBoxLayout();
    QLabel *emailLabel = new QLabel("📧 " + email);
    emailLabel->setStyleSheet("font-size: 13px; color: #6B7280;");
    infoLayout->addWidget(emailLabel);
    
    QLabel *phoneLabel = new QLabel("📞 " + phone);
    phoneLabel->setStyleSheet("font-size: 13px; color: #6B7280;");
    infoLayout->addWidget(phoneLabel);
    
    infoLayout->addStretch();
    mainLayout->addLayout(infoLayout);
    
    QLabel *dateLabel = new QLabel("Requested on: " + requestedDate);
    dateLabel->setStyleSheet("font-size: 12px; color: #9CA3AF;");
    mainLayout->addWidget(dateLabel);
}

void InstructorDashboard::setupApprovedStudentCard(QWidget *card, int studentId, const QString &name,
                                                   const QString &email, const QString &phone,
                                                   const QString &cin)
{
    Q_UNUSED(studentId);
    static int approvedCardCounter = 0;
    card->setObjectName(QString("approvedCard%1").arg(approvedCardCounter++));
    card->setStyleSheet("QWidget#" + card->objectName() + " { background-color: white; border-radius: 12px; border: 1px solid #E5E7EB; }");
    card->setMinimumHeight(90);

    QHBoxLayout *layout = new QHBoxLayout(card);
    layout->setContentsMargins(20, 15, 20, 15);
    layout->setSpacing(15);

    // ── Avatar: try to load student photo, fall back to teal circle ──────────
    QLabel *avatar = new QLabel();
    avatar->setFixedSize(50, 50);
    avatar->setAlignment(Qt::AlignCenter);

    // Look up photo file: AppData/student_photos/<cin>.<ext>
    bool photoLoaded = false;
    if (!cin.isEmpty()) {
        QString photosDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                            + "/student_photos";
        for (const QString &ext : {"jpg","jpeg","png","bmp","webp"}) {
            QString path = photosDir + "/" + cin + "." + ext;
            if (QFile::exists(path)) {
                QPixmap px(path);
                if (!px.isNull()) {
                    // Circular crop
                    QPixmap circle(50, 50);
                    circle.fill(Qt::transparent);
                    QPainter painter(&circle);
                    painter.setRenderHint(QPainter::Antialiasing);
                    painter.setBrush(QBrush(px.scaled(50, 50, Qt::KeepAspectRatioByExpanding,
                                                       Qt::SmoothTransformation)));
                    painter.setPen(Qt::NoPen);
                    painter.drawEllipse(0, 0, 50, 50);
                    painter.end();
                    avatar->setPixmap(circle);
                    photoLoaded = true;
                    break;
                }
            }
        }
    }
    if (!photoLoaded) {
        // Fallback: teal circle with initial letter
        QString initial = name.isEmpty() ? QString("?") : QString(name.at(0).toUpper());
        avatar->setText(initial);
        avatar->setStyleSheet(
            "background-color: #14B8A6; border-radius: 25px;"
            "color: white; font-size: 20px; font-weight: bold;"
            "min-width:50px; max-width:50px; min-height:50px; max-height:50px;");
    }
    layout->addWidget(avatar);
    
    // Info section
    QVBoxLayout *infoLayout = new QVBoxLayout();
    QLabel *nameLabel = new QLabel(name);
    nameLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #1F1827;");
    
    QHBoxLayout *contactLayout = new QHBoxLayout();
    QLabel *emailLabel = new QLabel("📧 " + email);
    emailLabel->setStyleSheet("font-size: 13px; color: #6B7280;");
    
    QLabel *phoneLabel = new QLabel("📞 " + phone);
    phoneLabel->setStyleSheet("font-size: 13px; color: #6B7280;");
    
    contactLayout->addWidget(emailLabel);
    contactLayout->addWidget(phoneLabel);
    contactLayout->addStretch();
    
    infoLayout->addWidget(nameLabel);
    infoLayout->addLayout(contactLayout);
    
    layout->addLayout(infoLayout, 1);
    
    // Active badge
    QLabel *statusBadge = new QLabel("Active");
    statusBadge->setStyleSheet("background-color: #D1FAE5; color: #065F46; border-radius: 5px; padding: 6px 12px; font-size: 12px; font-weight: bold;");
    statusBadge->setAlignment(Qt::AlignCenter);
    statusBadge->setMaximumWidth(80);
    statusBadge->setMaximumHeight(30);
    layout->addWidget(statusBadge);
}

void InstructorDashboard::setupVehicleCard(QWidget *card, int vehicleId, const QString &brand, const QString &model,
                                           int year, const QString &plateNumber, const QString &transmission, const QString &status, const QString &photoPath)
{
    // vehicleId is used for edit/delete actions below
    static int vehCardCounter = 0;
    card->setObjectName(QString("vehCard%1").arg(vehCardCounter++));
    card->setStyleSheet("QWidget#" + card->objectName() + " { background-color: white; border-radius: 12px; border: 1px solid #E5E7EB; }");
    card->setMinimumHeight(80);

    QHBoxLayout *layout = new QHBoxLayout(card);
    layout->setContentsMargins(20, 15, 20, 15);
    layout->setSpacing(15);
    
    // Car icon square — show photo thumbnail if available
    QLabel *icon = new QLabel();
    icon->setFixedSize(50, 50);
    icon->setAlignment(Qt::AlignCenter);
    if (!photoPath.isEmpty()) {
        QPixmap px(photoPath);
        if (!px.isNull()) {
            icon->setPixmap(px.scaled(50, 50, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
            icon->setStyleSheet("border-radius: 8px; background-color: #E5E7EB;");
        } else {
            icon->setStyleSheet("background-color: #14B8A6; border-radius: 8px;");
            icon->setText("🚗");
        }
    } else {
        icon->setStyleSheet("background-color: #14B8A6; border-radius: 8px;");
        icon->setText("🚗");
    }
    layout->addWidget(icon);
    
    // Info section
    QVBoxLayout *infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(5);
    
    QLabel *nameLabel = new QLabel(QString("%1 %2 (%3)").arg(brand).arg(model).arg(year));
    nameLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #1F1827;");
    
    QHBoxLayout *detailsLayout = new QHBoxLayout();
    detailsLayout->setSpacing(15);
    
    QLabel *plateLabel = new QLabel("🔖 " + plateNumber);
    plateLabel->setStyleSheet("font-size: 13px; color: #6B7280;");
    
    QLabel *transLabel = new QLabel("⚙️ " + transmission);
    transLabel->setStyleSheet("font-size: 13px; color: #6B7280;");
    
    detailsLayout->addWidget(plateLabel);
    detailsLayout->addWidget(transLabel);
    detailsLayout->addStretch();
    
    infoLayout->addWidget(nameLabel);
    infoLayout->addLayout(detailsLayout);
    
    layout->addLayout(infoLayout, 1);
    
    // Status badge
    QLabel *statusBadge = new QLabel();
    if (status.toLower() == "active") {
        statusBadge->setStyleSheet("background-color: #D1FAE5; color: #065F46; border-radius: 5px; padding: 6px 12px; font-size: 12px; font-weight: bold;");
        statusBadge->setText("Active");
    } else if (status.toLower() == "maintenance") {
        statusBadge->setStyleSheet("background-color: #FEF3C7; color: #92400E; border-radius: 5px; padding: 6px 12px; font-size: 12px; font-weight: bold;");
        statusBadge->setText("Maintenance");
    } else {
        statusBadge->setStyleSheet("background-color: #E5E7EB; color: #374151; border-radius: 5px; padding: 6px 12px; font-size: 12px; font-weight: bold;");
        statusBadge->setText(status);
    }
    statusBadge->setAlignment(Qt::AlignCenter);
    statusBadge->setMinimumWidth(100);
    statusBadge->setMaximumWidth(120);
    statusBadge->setMaximumHeight(30);
    layout->addWidget(statusBadge);
    
    // Edit button
    QPushButton *editBtn = new QPushButton("✏️");
    editBtn->setStyleSheet("background-color: transparent; border: none; font-size: 18px; padding: 5px;");
    editBtn->setMaximumSize(35, 35);
    editBtn->setCursor(Qt::PointingHandCursor);
    editBtn->setToolTip("Edit Vehicle");
    layout->addWidget(editBtn);

    // Delete button
    QPushButton *deleteBtn = new QPushButton("🗑️");
    deleteBtn->setStyleSheet("background-color: transparent; border: none; font-size: 18px; padding: 5px;");
    deleteBtn->setMaximumSize(35, 35);
    deleteBtn->setCursor(Qt::PointingHandCursor);
    deleteBtn->setToolTip("Delete Vehicle");
    layout->addWidget(deleteBtn);

    // ── Connect EDIT ──────────────────────────────────────────────────────────
    connect(editBtn, &QPushButton::clicked, [=]() {
        QDialog *dlg = new QDialog(this);
        dlg->setWindowTitle("Edit Vehicle");
        dlg->setFixedSize(460, 460);
        dlg->setStyleSheet(
            "QDialog { background-color: #F8FAFC; }"

            // ── Header ──────────────────────────────────
            "QFrame#editHeader {"
            "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
            "    stop:0 #0F766E, stop:1 #14B8A6);"
            "  border-radius: 0px; padding: 0px; }"
            "QLabel#editHeaderTitle {"
            "  color: white; font-size: 17px; font-weight: 800; }"
            "QLabel#editHeaderSub {"
            "  color: rgba(255,255,255,0.75); font-size: 11px; }"

            // ── Field labels ─────────────────────────────
            "QLabel#fieldLbl {"
            "  color: #64748B; font-size: 11px;"
            "  font-weight: 700; letter-spacing: 0.5px; }"

            // ── Inputs ───────────────────────────────────
            "QLineEdit {"
            "  background: white;"
            "  border: 1.5px solid #E2E8F0;"
            "  border-radius: 10px;"
            "  padding: 10px 14px;"
            "  font-size: 13px;"
            "  color: #0F172A; }"
            "QLineEdit:focus {"
            "  border: 1.5px solid #14B8A6;"
            "  background: #F0FDFA; }"

            // ── ComboBox ─────────────────────────────────
            "QComboBox {"
            "  background: white;"
            "  border: 1.5px solid #E2E8F0;"
            "  border-radius: 10px;"
            "  padding: 10px 14px;"
            "  font-size: 13px;"
            "  color: #0F172A; }"
            "QComboBox:focus { border: 1.5px solid #14B8A6; background: #F0FDFA; }"
            "QComboBox::drop-down { border: none; width: 30px; }"
            "QComboBox::down-arrow { width: 12px; }"
            "QComboBox QAbstractItemView {"
            "  background: white; border: 1px solid #E2E8F0;"
            "  border-radius: 8px; color: #0F172A;"
            "  selection-background-color: #CCFBF1; }"

            // ── Buttons ──────────────────────────────────
            "QPushButton#cancelBtn {"
            "  background: white; border: 1.5px solid #E2E8F0;"
            "  border-radius: 10px; padding: 10px 0px;"
            "  font-size: 13px; font-weight: 600; color: #374151; }"
            "QPushButton#cancelBtn:hover { background: #F1F5F9; }"
            "QPushButton#saveBtn {"
            "  background: #14B8A6; border: none;"
            "  border-radius: 10px; padding: 10px 0px;"
            "  font-size: 13px; font-weight: 700; color: white; }"
            "QPushButton#saveBtn:hover { background: #0D9488; }"
        );

        QVBoxLayout *vl = new QVBoxLayout(dlg);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setSpacing(0);

        // ── Gradient header ───────────────────────────────────────────────────
        QFrame *hdr = new QFrame();
        hdr->setObjectName("editHeader");
        hdr->setFixedHeight(72);
        QVBoxLayout *hdrL = new QVBoxLayout(hdr);
        hdrL->setContentsMargins(24, 12, 24, 12);
        hdrL->setSpacing(2);
        QLabel *hdrTitle = new QLabel("✏️  Edit Vehicle");
        hdrTitle->setObjectName("editHeaderTitle");
        QLabel *hdrSub   = new QLabel(QString("%1 %2  ·  %3").arg(brand).arg(model).arg(plateNumber));
        hdrSub->setObjectName("editHeaderSub");
        hdrL->addWidget(hdrTitle);
        hdrL->addWidget(hdrSub);
        vl->addWidget(hdr);

        // ── Form body ─────────────────────────────────────────────────────────
        QWidget *body = new QWidget();
        body->setStyleSheet("background: #F8FAFC;");
        QVBoxLayout *bl = new QVBoxLayout(body);
        bl->setContentsMargins(24, 20, 24, 20);
        bl->setSpacing(14);

        auto makeRow = [&](const QString &lbl, QWidget *w) {
            QVBoxLayout *col = new QVBoxLayout();
            col->setSpacing(5);
            QLabel *l = new QLabel(lbl.toUpper());
            l->setObjectName("fieldLbl");
            col->addWidget(l);
            col->addWidget(w);
            bl->addLayout(col);
        };

        // ── Brand + Model row ─────────────────────────────────────────────────
        QHBoxLayout *bmRow = new QHBoxLayout();
        bmRow->setSpacing(12);
        QLineEdit *brandEdit = new QLineEdit(brand);
        brandEdit->setPlaceholderText("e.g. Renault");
        QLineEdit *modelEdit = new QLineEdit(model);
        modelEdit->setPlaceholderText("e.g. Clio");

        QVBoxLayout *brandCol = new QVBoxLayout();
        brandCol->setSpacing(5);
        QLabel *brandLbl = new QLabel("BRAND"); brandLbl->setObjectName("fieldLbl");
        brandCol->addWidget(brandLbl); brandCol->addWidget(brandEdit);

        QVBoxLayout *modelCol = new QVBoxLayout();
        modelCol->setSpacing(5);
        QLabel *modelLbl = new QLabel("MODEL"); modelLbl->setObjectName("fieldLbl");
        modelCol->addWidget(modelLbl); modelCol->addWidget(modelEdit);

        bmRow->addLayout(brandCol);
        bmRow->addLayout(modelCol);
        bl->addLayout(bmRow);

        QLineEdit *plateEdit = new QLineEdit(plateNumber);
        plateEdit->setPlaceholderText("e.g. 123 TUN 4567");
        makeRow("Plate Number", plateEdit);

        QComboBox *statusBox = new QComboBox();
        statusBox->addItems({"active", "maintenance", "inactive"});
        statusBox->setCurrentText(status.toLower());
        makeRow("Status", statusBox);

        vl->addWidget(body, 1);

        // ── Buttons ───────────────────────────────────────────────────────────
        QWidget *footer = new QWidget();
        footer->setStyleSheet("background: white; border-top: 1px solid #E2E8F0;");
        footer->setFixedHeight(64);
        QHBoxLayout *btns = new QHBoxLayout(footer);
        btns->setContentsMargins(24, 12, 24, 12);
        btns->setSpacing(12);

        QPushButton *cancel = new QPushButton("Cancel");
        cancel->setObjectName("cancelBtn");
        cancel->setCursor(Qt::PointingHandCursor);
        QPushButton *save   = new QPushButton("💾  Save Changes");
        save->setObjectName("saveBtn");
        save->setCursor(Qt::PointingHandCursor);
        btns->addWidget(cancel);
        btns->addWidget(save);
        vl->addWidget(footer);

        connect(cancel, &QPushButton::clicked, dlg, &QDialog::reject);
        connect(save, &QPushButton::clicked, [=]() {
            QSqlQuery q;
            q.prepare("UPDATE cars SET brand=?, model=?, plate_number=?, is_active=CASE WHEN ?='active' THEN 1 ELSE 0 END WHERE id=?");
            q.addBindValue(brandEdit->text().trimmed());
            q.addBindValue(modelEdit->text().trimmed());
            q.addBindValue(plateEdit->text().trimmed());
            q.addBindValue(statusBox->currentText());
            q.addBindValue(vehicleId);
            if (q.exec()) {
                QMessageBox::information(dlg, "Success", "Vehicle updated successfully!");
                dlg->accept();
                loadData();
            } else {
                QMessageBox::critical(dlg, "Error", "Failed to update:\n" + q.lastError().text());
            }
        });
        dlg->exec();
    });

    // ── Connect DELETE ────────────────────────────────────────────────────────
    connect(deleteBtn, &QPushButton::clicked, [=]() {
        int ret = QMessageBox::warning(this, "Delete Vehicle",
            QString("Are you sure you want to delete\n<b>%1 %2</b>?")
                .arg(brand).arg(model),
            QMessageBox::Yes | QMessageBox::Cancel);
        if (ret != QMessageBox::Yes) return;

        QSqlQuery q;
        q.prepare("DELETE FROM cars WHERE id = ?");
        q.addBindValue(vehicleId);
        if (q.exec()) {
            QMessageBox::information(this, "Deleted", "Vehicle removed successfully.");
            loadData();
        } else {
            QMessageBox::critical(this, "Error", "Failed to delete:\n" + q.lastError().text());
        }
    });
}

void InstructorDashboard::switchTab(int index)
{
    // ── Circuit tab (index 3) ─────────────────────────────────────────────────
    if (index == 3) {
        if (!m_circuitDashboard) {
            OracleConnectionParams p;
            p.drivingSchoolId = schoolId;
            p.instructorId    = instructorId;
            if (!CircuitDB::instance()->initialize(p)) {
                QMessageBox::critical(this, "Circuit DB Error",
                    CircuitDB::instance()->lastError());
                return;
            }
            {
                QSettings cs("SmartDrivingSchool", "AnalysisApp");
                cs.setValue("dbDrivingSchoolId", schoolId);
                cs.setValue("dbInstructorId",    instructorId > 0 ? instructorId : 1);
                cs.sync();
            }
            // Force embedded behavior — no separate window
            m_circuitDashboard = new CircuitDashboard(m_circuitTab);
            m_circuitDashboard->setWindowFlags(Qt::Widget);
            connect(m_circuitDashboard, &QObject::destroyed, this, [this]() {
                m_circuitDashboard = nullptr;
            });
            // Sync to current theme immediately after creation
            m_circuitIsDark = true;   // CircuitDashboard always starts dark
            bool curDark = ThemeManager::instance()->currentTheme() == ThemeManager::Dark;
            applyThemeToCircuit(curDark);

            // Wrap in a scroll area so content is scrollable
            QScrollArea *sa = new QScrollArea(m_circuitTab);
            sa->setWidget(m_circuitDashboard);
            sa->setWidgetResizable(true);
            sa->setFrameShape(QFrame::NoFrame);
            sa->setStyleSheet("QScrollArea { background: transparent; border: none; }");
            m_circuitTab->layout()->addWidget(sa);
        }
    }

    // ── Sessions tab (index 4) ────────────────────────────────────────────────
    if (index == 4) {
        qApp->setProperty("currentUserId", instructorId > 0 ? instructorId : 1);

        if (!m_winoTab) {
            WinoBootstrap::bootstrap();
            // WinoInstructorDashboard is now a QWidget — embeds directly
            m_winoTab = new WinoInstructorDashboard(m_winoTabWidget);
            connect(m_winoTab, &QObject::destroyed, this, [this]() {
                m_winoTab = nullptr;
            });
            // Hide the inner header (← Back, title, D17 badge, theme toggle)
            // Navigation is handled by the outer sidebar
            if (QWidget *innerHdr = m_winoTab->findChild<QWidget*>("header"))
                innerHdr->hide();
            // Wrap in a scroll area
            QScrollArea *sa = new QScrollArea(m_winoTabWidget);
            sa->setWidget(m_winoTab);
            sa->setWidgetResizable(true);
            sa->setFrameShape(QFrame::NoFrame);
            sa->setStyleSheet("QScrollArea { background: transparent; border: none; }");
            m_winoTabWidget->layout()->addWidget(sa);
        }
    }

    // ── Parking / Administration tab (index 5) ────────────────────────────────
    if (index == 5) {
        if (!m_adminTab) {
            ParkingDBManager::instance().initialize("", "", "");
            // AdminWidget is already a QWidget — embeds directly
            m_adminTab = new AdminWidget(
                QString("Instructor"),
                QString("Moniteur"),
                instructorId,
                m_adminTabWidget
            );
            connect(m_adminTab, &QObject::destroyed, this, [this]() {
                m_adminTab = nullptr;
            });
            // Wrap in a scroll area
            QScrollArea *sa = new QScrollArea(m_adminTabWidget);
            sa->setWidget(m_adminTab);
            sa->setWidgetResizable(true);
            sa->setFrameShape(QFrame::NoFrame);
            sa->setStyleSheet("QScrollArea { background: transparent; border: none; }");
            m_adminTabWidget->layout()->addWidget(sa);
        }
    }
}

// ══════════════════════════════════════════════════════════════
//  Sidebar: full light/dark theme update  (matches student area)
// ══════════════════════════════════════════════════════════════
void InstructorDashboard::updateSidebarTheme()
{
    ThemeManager *tm = ThemeManager::instance();
    bool isDark = (tm->currentTheme() == ThemeManager::Dark);

    // ── Build global stylesheet ───────────────────────────────────────────────
    QString globalSS = QString(
        "QMainWindow, QDialog { background-color: %1; }"
        "QWidget   { background-color: %1; color: %2; }"
        "QFrame    { background-color: %3; color: %2; }"
        "QLabel    { color: %2; background: transparent; }"
        "QScrollArea { background-color: %1; border: none; }"
        "QScrollBar:vertical { background: %4; width: 8px; border-radius: 4px; }"
        "QScrollBar::handle:vertical { background: %5; border-radius: 4px; min-height: 30px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { border: none; background: none; }"
        "QPushButton { background-color: %3; color: %2; border: 1px solid %4; border-radius: 6px; padding: 6px 14px; }"
        "QPushButton:hover { background-color: %4; }"
        "QLineEdit, QTextEdit, QPlainTextEdit { background-color: %3; color: %2; border: 1px solid %4; border-radius: 6px; padding: 4px 8px; }"
        "QComboBox { background-color: %3; color: %2; border: 1px solid %4; border-radius: 6px; padding: 4px 8px; }"
        "QComboBox QAbstractItemView { background-color: %3; color: %2; selection-background-color: %5; }"
        "QTableWidget, QListWidget, QTreeWidget { background-color: %3; color: %2; gridline-color: %4; border: 1px solid %4; }"
        "QTableWidget::item:selected, QListWidget::item:selected { background-color: %5; color: white; }"
        "QHeaderView::section { background-color: %4; color: %2; border: none; padding: 6px; }"
        "QTabBar::tab { background-color: %3; color: %2; border: 1px solid %4; padding: 8px 16px; }"
        "QTabBar::tab:selected { background-color: %1; color: %6; border-bottom: 2px solid %6; }"
        "QGroupBox { color: %2; border: 1px solid %4; border-radius: 8px; margin-top: 12px; padding-top: 8px; }"
        "QGroupBox::title { color: %2; subcontrol-origin: margin; left: 12px; }"
        "QToolTip { background-color: %3; color: %2; border: 1px solid %4; border-radius: 4px; padding: 4px 8px; }"
    )
    .arg(tm->backgroundColor())
    .arg(tm->primaryTextColor())
    .arg(tm->cardColor())
    .arg(tm->borderColor())
    .arg(tm->accentColor())
    .arg(tm->accentColor());

    // ── All style application (runs at mid-point of the fade) ─────────────────
    auto applyAll = [this, isDark, globalSS]() {
        qApp->setStyleSheet(globalSS);
        applyThemeToCircuit(isDark);

        if (!m_sidebar) return;

        const QString sidebarBg = isDark ? "#111827" : "#FFFFFF";
        const QString borderClr = isDark ? "#1E293B" : "#E2E8F0";
        const QString mutedClr  = isDark ? "#64748B" : "#6B7280";
        const QString textClr   = isDark ? "#E2E8F0" : "#111827";
        const QString hoverBg   = isDark ? "rgba(255,255,255,0.05)" : "rgba(0,0,0,0.04)";
        const QString contentBg = isDark ? "#0F172A" : "#F1F5F9";

        m_sidebar->setStyleSheet(QString(
            "QFrame#instrSidebar { background: %1; border-right: 1px solid %2; }"
            "QFrame#instrSidebar * { background: transparent; }")
            .arg(sidebarBg, borderClr));

        if (ui->contentWidget)
            ui->contentWidget->setStyleSheet(
                QString("background-color: %1;").arg(contentBg));

        for (QLabel *lbl : m_sidebar->findChildren<QLabel*>()) {
            QString ss = lbl->styleSheet();
            if (ss.contains("#14B8A6")) continue;
            if (ss.contains("font-size: 32px")) continue;
            lbl->setStyleSheet(ss.isEmpty()
                ? QString("color: %1;").arg(mutedClr)
                : ss.replace(QRegularExpression("color:\\s*#[0-9A-Fa-f]{3,6}"),
                             QString("color: %1").arg(mutedClr)));
        }

        const QString inactiveSS = QString(
            "QPushButton { background: transparent; color: %1; "
            "border: none; border-left: 3px solid transparent; "
            "text-align: left; padding-left: 16px; "
            "font-size: 14px; font-weight: 500; border-radius: 0; }"
            "QPushButton:hover { background: %2; color: %3; }")
            .arg(mutedClr, hoverBg, textClr);

        for (QPushButton *btn : QList<QPushButton*>{
                m_navRequests, m_navStudents, m_navVehicles,
                m_navCircuit,  m_navSessions, m_navParking}) {
            if (!btn) continue;
            if (!btn->styleSheet().contains("#14B8A6"))
                btn->setStyleSheet(inactiveSS);
        }

        if (m_themeBtn) {
            m_themeBtn->setText(isDark ? QString::fromUtf8("  ☀️   Light Mode")
                                       : QString::fromUtf8("  🌙   Dark Mode"));
            m_themeBtn->setStyleSheet(QString(
                "QPushButton { background: transparent; color: %1; "
                "border: none; border-left: 3px solid transparent; "
                "text-align: left; padding-left: 16px; "
                "font-size: 13px; font-weight: 500; border-radius: 0; }"
                "QPushButton:hover { background: %2; color: %3; }")
                .arg(mutedClr, hoverBg, textClr));
        }
    };

    // Apply everything immediately — instant, clean, no flicker
    applyAll();
}

// ══════════════════════════════════════════════════════════════
//  Sidebar: highlight the active nav button (student-area style)
// ══════════════════════════════════════════════════════════════
void InstructorDashboard::setActiveNavBtn(QPushButton *active)
{
    const QList<QPushButton*> all = {
        m_navRequests, m_navStudents, m_navVehicles,
        m_navCircuit, m_navSessions, m_navParking
    };
    for (QPushButton *btn : all) {
        if (!btn) continue;
        if (btn == active) {
            btn->setStyleSheet(
                "QPushButton { background: rgba(20,184,166,0.15); color: #14B8A6; "
                "border-left: 3px solid #14B8A6; text-align: left; padding-left: 13px; "
                "font-size: 14px; font-weight: 600; border-radius: 0; "
                "border-top: none; border-right: none; border-bottom: none; }");
        } else {
            bool dk = ThemeManager::instance()->currentTheme() == ThemeManager::Dark;
            const QString mc = dk ? "#64748B" : "#6B7280";
            const QString tc = dk ? "#E2E8F0" : "#111827";
            const QString hb = dk ? "rgba(255,255,255,0.05)" : "rgba(0,0,0,0.04)";
            btn->setStyleSheet(QString(
                "QPushButton { background: transparent; color: %1; "
                "border: none; border-left: 3px solid transparent; "
                "text-align: left; padding-left: 16px; "
                "font-size: 14px; font-weight: 500; border-radius: 0; }"
                "QPushButton:hover { background: %3; color: %2; }")
                .arg(mc, tc, hb));
        }
    }
}

// ══════════════════════════════════════════════════════════════
//  Circuit theme: string-replace hardcoded dark colors in every
//  child widget's stylesheet (CircuitDashboard has no ThemeManager)
// ══════════════════════════════════════════════════════════════
void InstructorDashboard::applyThemeToCircuit(bool isDark)
{
    if (!m_circuitDashboard) return;
    if (m_circuitIsDark == isDark) return;   // already correct state, skip

    // Delegate to CircuitDashboard's own applyTheme() which handles both
    // QCustomPlot graph recoloring AND widget stylesheet replacement.
    m_circuitDashboard->applyTheme(isDark);

    m_circuitIsDark = isDark;
}

// ══════════════════════════════════════════════════════════════
//  Enrollment confirmation email  (curl.exe — no OpenSSL needed)
// ══════════════════════════════════════════════════════════════
static void sendEnrollmentEmail(const QString &toEmail,
                                const QString &studentName,
                                const QString &schoolName,
                                const QString &instructorName,
                                QObject *parent)
{
    const QString fromEmail   = QStringLiteral("benaliwajih2@gmail.com");
    const QString appPassword = QString("urrx lftw jafm pfcu").remove(' ');

    // ── Plain-text fallback ──
    QByteArray plain =
        "Dear " + studentName.toUtf8() + ",\r\n\r\n"
        "Congratulations! Your enrollment request has been ACCEPTED.\r\n\r\n"
        "School    : " + schoolName.toUtf8() + "\r\n"
        "Instructor: " + instructorName.toUtf8() + "\r\n\r\n"
        "You can now log in to the Driving School App and start your learning journey.\r\n\r\n"
        "Best regards,\r\nDriving School App\r\n";

    // ── HTML body ──
    QByteArray studentNameHtml = studentName.toHtmlEscaped().toUtf8();
    QByteArray schoolNameHtml  = schoolName.toHtmlEscaped().toUtf8();
    QByteArray instrNameHtml   = instructorName.toHtmlEscaped().toUtf8();

    QByteArray html =
        "<!DOCTYPE html>"
        "<html lang='en'><head><meta charset='UTF-8'/></head>"
        "<body style='margin:0;padding:0;background:#e0f5f1;"
        "font-family:Segoe UI,Arial,sans-serif;'>"

        // outer wrapper
        "<table width='100%' cellpadding='0' cellspacing='0' border='0'"
        "  style='background:#e0f5f1;padding:40px 0;'>"
        "<tr><td align='center'>"

        // card
        "<table width='520' cellpadding='0' cellspacing='0' border='0'"
        "  style='background:#ffffff;border-radius:20px;"
        "  box-shadow:0 8px 40px rgba(0,0,0,.12);overflow:hidden;'>"

        // header
        "<tr><td style='background:linear-gradient(135deg,#14B8A6 0%,#0d9488 100%);"
        "  padding:36px 40px 28px;text-align:center;'>"
        "  <div style='display:inline-block;background:rgba(255,255,255,.18);"
        "    border-radius:50%;width:72px;height:72px;line-height:72px;"
        "    font-size:36px;margin-bottom:14px;'>&#x1F393;</div>"
        "  <h1 style='margin:0;color:#ffffff;font-size:24px;font-weight:800;'>"
        "    Enrollment Confirmed!</h1>"
        "  <p style='margin:6px 0 0;color:rgba(255,255,255,.80);font-size:14px;'>"
        "    Driving School App</p>"
        "</td></tr>"

        // body
        "<tr><td style='padding:40px 48px 32px;'>"
        "  <p style='margin:0 0 8px;color:#374151;font-size:15px;'>Dear "
        + studentNameHtml +
        ",</p>"
        "  <p style='margin:0 0 28px;color:#374151;font-size:15px;line-height:1.6;'>"
        "    <strong>Congratulations!</strong> Your enrollment request has been "
        "    <span style='color:#10B981;font-weight:700;'>accepted</span>. "
        "    You can now start your driving training journey."
        "  </p>"

        // info box
        "  <table width='100%' cellpadding='0' cellspacing='0'"
        "    style='background:#F0FDF9;border:2px solid #99F6E4;"
        "    border-radius:14px;margin:0 0 28px;'>"
        "  <tr><td style='padding:24px 28px;'>"
        "    <p style='margin:0 0 10px;color:#0f766e;font-size:12px;"
        "      font-weight:700;letter-spacing:2px;text-transform:uppercase;'>"
        "      Your Details</p>"
        "    <table width='100%'>"
        "    <tr><td style='color:#374151;font-size:14px;padding:4px 0;"
        "      font-weight:600;width:130px;'>&#x1F3EB;&nbsp; School</td>"
        "        <td style='color:#374151;font-size:14px;padding:4px 0;'>"
        + schoolNameHtml +
        "</td></tr>"
        "    <tr><td style='color:#374151;font-size:14px;padding:4px 0;"
        "      font-weight:600;'>&#x1F9D1;&#x200D;&#x1F3EB;&nbsp; Instructor</td>"
        "        <td style='color:#374151;font-size:14px;padding:4px 0;'>"
        + instrNameHtml +
        "</td></tr>"
        "    </table>"
        "  </td></tr></table>"

        // next steps
        "  <table width='100%' cellpadding='0' cellspacing='0'><tr>"
        "    <td style='background:#FEF3C7;border-left:4px solid #F59E0B;"
        "      border-radius:0 8px 8px 0;padding:12px 16px;'>"
        "      <p style='margin:0;color:#92400E;font-size:13px;font-weight:600;'>"
        "        &#x1F4F1;&nbsp; Log in to the app to start your Code, Circuit"
        "        &amp; Parking training."
        "      </p>"
        "    </td>"
        "  </tr></table>"

        "  <p style='margin:28px 0 0;color:#9CA3AF;font-size:12px;line-height:1.6;'>"
        "    If you have any questions, please contact your driving school directly."
        "  </p>"
        "</td></tr>"

        // footer
        "<tr><td style='background:#F9FAFB;border-top:1px solid #E5E7EB;"
        "  padding:20px 48px;text-align:center;'>"
        "  <p style='margin:0;color:#9CA3AF;font-size:12px;'>"
        "    &copy; 2026 Driving School App &nbsp;&bull;&nbsp; All rights reserved"
        "  </p>"
        "</td></tr>"

        "</table></td></tr></table>"
        "</body></html>";

    const QByteArray boundary = "----DS_ENROLL_MIME_7a2f";
    QByteArray body =
        "From: Driving School <" + fromEmail.toLatin1() + ">\r\n"
        "To: " + toEmail.toLatin1() + "\r\n"
        "Subject: =?UTF-8?Q?Enrollment_Confirmed_=E2=80=94_Driving_School_App?=\r\n"
        "MIME-Version: 1.0\r\n"
        "Content-Type: multipart/alternative; boundary=\"" + boundary + "\"\r\n"
        "\r\n"
        "--" + boundary + "\r\n"
        "Content-Type: text/plain; charset=UTF-8\r\n"
        "Content-Transfer-Encoding: 8bit\r\n"
        "\r\n" + plain +
        "\r\n--" + boundary + "\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Transfer-Encoding: 8bit\r\n"
        "\r\n" + html +
        "\r\n--" + boundary + "--\r\n";

    // Write to temp file
    QTemporaryFile *tmp = new QTemporaryFile(parent);
    tmp->setAutoRemove(true);
    if (!tmp->open()) return;
    tmp->write(body);
    tmp->flush();

    QStringList args;
    args << "--url"  << "smtps://smtp.gmail.com:465"
         << "--insecure"
         << "--user" << (fromEmail + ":" + appPassword)
         << "--mail-from" << fromEmail
         << "--mail-rcpt"  << toEmail
         << "--upload-file" << tmp->fileName()
         << "--silent";

    QProcess *proc = new QProcess(parent);
    QObject::connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                     parent, [proc, tmp](int exitCode, QProcess::ExitStatus) {
        if (exitCode != 0)
            qDebug() << "[Enrollment email] curl error:" << proc->readAll();
        else
            qDebug() << "[Enrollment email] sent successfully.";
        proc->deleteLater();
        tmp->close();
        tmp->deleteLater();
    });
    proc->start("C:/Windows/System32/curl.exe", args);
}

void InstructorDashboard::onAcceptStudent(int studentId)
{
    QSqlQuery query;
    query.prepare("UPDATE students SET status = 'approved', instructor_id = ? WHERE id = ?");
    query.addBindValue(instructorId > 0 ? instructorId : QVariant(QMetaType(QMetaType::Int)));
    query.addBindValue(studentId);

    if (!query.exec()) {
        QMessageBox::critical(this, "Error", "Failed to approve:\n" + query.lastError().text());
        return;
    }

    // Fetch student details to sync into Circuit STUDENTS table + send email
    QSqlQuery fetch;
    fetch.prepare("SELECT name, email, phone FROM students WHERE id = ?");
    fetch.addBindValue(studentId);
    if (fetch.exec() && fetch.next()) {
        QString fullName  = fetch.value(0).toString().trimmed();
        int sp = fullName.indexOf(' ');
        QString firstName = (sp > 0) ? fullName.left(sp)   : fullName;
        QString lastName  = (sp > 0) ? fullName.mid(sp + 1): QString();
        QString email     = fetch.value(1).toString();
        QString phone     = fetch.value(2).toString();

        // Ensure CircuitDB is open before syncing
        if (!CircuitDB::instance()->isOpen()) {
            OracleConnectionParams p;
            p.drivingSchoolId = schoolId;
            p.instructorId    = instructorId;
            CircuitDB::instance()->initialize(p);
        }
        CircuitDB::instance()->syncStudent(studentId, firstName, lastName, email, phone, schoolId);

        // ── Fetch school name & instructor name for the email ──
        QString schoolName = "Driving School";
        QSqlQuery sq;
        sq.prepare("SELECT name FROM driving_schools WHERE id = ?");
        sq.addBindValue(schoolId);
        if (sq.exec() && sq.next()) schoolName = sq.value(0).toString();

        QString instrName = "Your Instructor";
        QSqlQuery iq;
        iq.prepare("SELECT full_name FROM instructors WHERE id = ?");
        iq.addBindValue(instructorId);
        if (iq.exec() && iq.next()) instrName = iq.value(0).toString();

        // ── Send enrollment confirmation email (async, non-blocking) ──
        if (!email.isEmpty())
            sendEnrollmentEmail(email, fullName, schoolName, instrName, this);
    }

    QMessageBox::information(this, "Success",
        QString::fromUtf8("Student request approved!\n"
                          "A confirmation email has been sent to the student."));
    loadData();
}

void InstructorDashboard::onRejectStudent(int studentId)
{
    QSqlQuery query;
    query.prepare("UPDATE students SET status = 'rejected' WHERE id = ?");
    query.addBindValue(studentId);
    
    if (query.exec()) {
        QMessageBox::information(this, "Success", "Student request rejected!");
        loadData();
    }
}

void InstructorDashboard::onAddVehicle()
{
    QDialog *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle("Add Vehicle");
    dialog->setMinimumWidth(500);
    dialog->setMaximumWidth(520);
    dialog->setStyleSheet(
        /* ── base ── */
        "QDialog { background-color: #F8FAFC; }"
        /* ── header panel ── */
        "QFrame#headerFrame {"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "    stop:0 #0F766E, stop:1 #14B8A6);"
        "  border-radius: 0px; }"
        /* ── section cards ── */
        "QFrame#sectionCard {"
        "  background: white; border-radius: 12px;"
        "  border: 1px solid #E2E8F0; }"
        /* ── labels ── */
        "QLabel#fieldLabel { color:#64748B; font-size:11px; font-weight:600; letter-spacing:0.5px; }"
        /* ── inputs ── */
        "QLineEdit, QSpinBox {"
        "  border: 1.5px solid #E2E8F0; border-radius: 8px;"
        "  padding: 10px 14px; font-size: 13px;"
        "  background: white; color: #0F172A; }"
        "QLineEdit:focus, QSpinBox:focus { border: 1.5px solid #14B8A6; background: #F0FDFA; }"
        "QLineEdit::placeholder { color: #CBD5E1; }"
        /* ── combobox ── */
        "QComboBox {"
        "  border: 1.5px solid #E2E8F0; border-radius: 8px;"
        "  padding: 10px 14px; font-size: 13px;"
        "  background: white; color: #0F172A;"
        "  selection-background-color: #CCFBF1; }"
        "QComboBox:focus { border: 1.5px solid #14B8A6; background: #F0FDFA; }"
        "QComboBox::drop-down { border: none; width: 30px; }"
        "QComboBox::down-arrow { image: none; width: 0px; }"
        "QComboBox QAbstractItemView {"
        "  background: white; border: 1px solid #E2E8F0; border-radius: 8px;"
        "  color: #0F172A; selection-background-color: #CCFBF1;"
        "  selection-color: #0F766E; padding: 4px; outline: none; }"
        /* ── transmission toggle buttons ── */
        "QPushButton#transBtn {"
        "  background: white; color: #64748B; border: 1.5px solid #E2E8F0;"
        "  border-radius: 8px; padding: 10px 0; font-size: 13px; font-weight: 500; }"
        "QPushButton#transBtn:checked, QPushButton#transBtn:hover {"
        "  background: #CCFBF1; color: #0F766E; border: 1.5px solid #14B8A6; font-weight: 600; }"
        /* ── photo zone ── */
        "QFrame#photoZone {"
        "  background: #F8FAFC; border: 2px dashed #CBD5E1; border-radius: 12px; }"
        "QFrame#photoZone:hover { border-color: #14B8A6; background: #F0FDFA; }"
        /* ── choose photo btn ── */
        "QPushButton#photoPickBtn {"
        "  background: white; color: #0F766E; border: 1.5px solid #14B8A6;"
        "  border-radius: 8px; padding: 9px 20px; font-size: 12px; font-weight: 600; }"
        "QPushButton#photoPickBtn:hover { background: #CCFBF1; }"
        /* ── footer buttons ── */
        "QPushButton#cancelBtn {"
        "  background: white; color: #64748B; border: 1.5px solid #E2E8F0;"
        "  border-radius: 10px; padding: 12px; font-size: 13px; font-weight: 500; }"
        "QPushButton#cancelBtn:hover { background: #F1F5F9; border-color: #CBD5E1; }"
        "QPushButton#saveBtn {"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #0F766E,stop:1 #14B8A6);"
        "  color: white; border: none; border-radius: 10px;"
        "  padding: 12px; font-size: 14px; font-weight: 700; }"
        "QPushButton#saveBtn:hover {"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #0D6560,stop:1 #0F9488); }"
        "QPushButton#saveBtn:pressed { background: #0D6560; }");

    QVBoxLayout *root = new QVBoxLayout(dialog);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Gradient header ────────────────────────────────────────────────────
    QFrame *headerFrame = new QFrame();
    headerFrame->setObjectName("headerFrame");
    headerFrame->setFixedHeight(80);
    QHBoxLayout *headerLayout = new QHBoxLayout(headerFrame);
    headerLayout->setContentsMargins(24, 0, 24, 0);
    headerLayout->setSpacing(14);

    QLabel *carIcon = new QLabel("🚗");
    carIcon->setStyleSheet("font-size: 32px; background: transparent;");
    QVBoxLayout *headerText = new QVBoxLayout();
    headerText->setSpacing(2);
    QLabel *titleLbl = new QLabel("New Vehicle");
    titleLbl->setStyleSheet("font-size:20px; font-weight:800; color:white; background:transparent;");
    QLabel *subLbl  = new QLabel("Fill in the details to register a vehicle");
    subLbl->setStyleSheet("font-size:11px; color:rgba(255,255,255,0.75); background:transparent;");
    headerText->addWidget(titleLbl);
    headerText->addWidget(subLbl);
    headerLayout->addWidget(carIcon);
    headerLayout->addLayout(headerText);
    headerLayout->addStretch();
    root->addWidget(headerFrame);

    // ── Scrollable content area ────────────────────────────────────────────
    QWidget *content = new QWidget();
    content->setStyleSheet("background: #F8FAFC;");
    QVBoxLayout *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(20, 16, 20, 20);
    contentLayout->setSpacing(12);
    root->addWidget(content);

    // ── helper: field label ────────────────────────────────────────────────
    auto makeLabel = [](const QString &t) {
        QLabel *l = new QLabel(t.toUpper());
        l->setObjectName("fieldLabel");
        l->setStyleSheet("color:#64748B; font-size:11px; font-weight:600; letter-spacing:0.5px;");
        return l;
    };

    // ── PHOTO SECTION CARD ─────────────────────────────────────────────────
    QFrame *photoCard = new QFrame(); photoCard->setObjectName("sectionCard");
    QVBoxLayout *photoCardL = new QVBoxLayout(photoCard);
    photoCardL->setContentsMargins(16, 14, 16, 14); photoCardL->setSpacing(10);

    QLabel *photoTitle = new QLabel("📷  Vehicle Photo");
    photoTitle->setStyleSheet("font-size:13px; font-weight:700; color:#1E293B;");
    photoCardL->addWidget(photoTitle);

    // Photo drop zone
    QFrame *photoZone = new QFrame(); photoZone->setObjectName("photoZone");
    photoZone->setFixedHeight(130);
    QVBoxLayout *photoZoneL = new QVBoxLayout(photoZone);
    photoZoneL->setAlignment(Qt::AlignCenter); photoZoneL->setSpacing(4);

    QLabel *photoPreview = new QLabel();
    photoPreview->setAlignment(Qt::AlignCenter);
    photoPreview->setStyleSheet("background: transparent; color:#94A3B8; font-size:13px;");
    photoPreview->setText("🚙");
    QLabel *photoHint = new QLabel("No photo selected");
    photoHint->setAlignment(Qt::AlignCenter);
    photoHint->setStyleSheet("background:transparent; color:#94A3B8; font-size:12px;");
    photoZoneL->addWidget(photoPreview);
    photoZoneL->addWidget(photoHint);
    photoCardL->addWidget(photoZone);

    QPushButton *photoBtn = new QPushButton("  Choose Photo");
    photoBtn->setObjectName("photoPickBtn");
    photoBtn->setMinimumHeight(38);
    photoCardL->addWidget(photoBtn, 0, Qt::AlignRight);
    contentLayout->addWidget(photoCard);

    QString photoPath;
    QObject::connect(photoBtn, &QPushButton::clicked, [&]() {
        QString file = QFileDialog::getOpenFileName(
            dialog, "Select Vehicle Photo", "",
            "Images (*.png *.jpg *.jpeg *.bmp *.webp)");
        if (!file.isEmpty()) {
            photoPath = file;
            QPixmap px(file);
            if (!px.isNull()) {
                photoPreview->setPixmap(px.scaled(220, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                photoHint->hide();
                photoZone->setStyleSheet(
                    "QFrame#photoZone { background:#F0FDFA; border:2px solid #14B8A6; border-radius:12px; }");
            } else {
                photoPreview->setText("📎  " + QFileInfo(file).fileName());
            }
            photoBtn->setText("  Change Photo");
        }
    });

    // ── DETAILS SECTION CARD ───────────────────────────────────────────────
    QFrame *detailCard = new QFrame(); detailCard->setObjectName("sectionCard");
    QVBoxLayout *detailCardL = new QVBoxLayout(detailCard);
    detailCardL->setContentsMargins(16, 14, 16, 16); detailCardL->setSpacing(10);

    QLabel *detailTitle = new QLabel("🔑  Vehicle Details");
    detailTitle->setStyleSheet("font-size:13px; font-weight:700; color:#1E293B;");
    detailCardL->addWidget(detailTitle);

    // Brand & Model
    QHBoxLayout *row1 = new QHBoxLayout(); row1->setSpacing(12);
    QVBoxLayout *brandCol = new QVBoxLayout(); brandCol->setSpacing(5);
    brandCol->addWidget(makeLabel("Brand *"));
    QLineEdit *brandEdit = new QLineEdit();
    brandEdit->setPlaceholderText("e.g. Renault");
    brandEdit->setMinimumHeight(42);
    brandCol->addWidget(brandEdit);

    QVBoxLayout *modelCol = new QVBoxLayout(); modelCol->setSpacing(5);
    modelCol->addWidget(makeLabel("Model *"));
    QLineEdit *modelEdit = new QLineEdit();
    modelEdit->setPlaceholderText("e.g. Clio");
    modelEdit->setMinimumHeight(42);
    modelCol->addWidget(modelEdit);

    row1->addLayout(brandCol);
    row1->addLayout(modelCol);
    detailCardL->addLayout(row1);

    // Year & Plate
    QHBoxLayout *row2 = new QHBoxLayout(); row2->setSpacing(12);
    QVBoxLayout *yearCol = new QVBoxLayout(); yearCol->setSpacing(5);
    yearCol->addWidget(makeLabel("Year *"));
    QSpinBox *yearSpin = new QSpinBox();
    yearSpin->setRange(1990, 2030);
    yearSpin->setValue(QDate::currentDate().year());
    yearSpin->setMinimumHeight(42);
    yearSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    yearCol->addWidget(yearSpin);

    QVBoxLayout *plateCol = new QVBoxLayout(); plateCol->setSpacing(5);
    plateCol->addWidget(makeLabel("Plate Number *"));
    QLineEdit *plateEdit = new QLineEdit();
    plateEdit->setPlaceholderText("e.g. 123 TUN 4567");
    plateEdit->setMinimumHeight(42);
    plateCol->addWidget(plateEdit);

    row2->addLayout(yearCol, 1);
    row2->addLayout(plateCol, 2);
    detailCardL->addLayout(row2);

    // Transmission — toggle buttons
    QVBoxLayout *transCol = new QVBoxLayout(); transCol->setSpacing(6);
    transCol->addWidget(makeLabel("Transmission"));
    QHBoxLayout *transRow = new QHBoxLayout(); transRow->setSpacing(8);
    QPushButton *manualBtn = new QPushButton("⚙️  Manual");
    manualBtn->setObjectName("transBtn");
    manualBtn->setMinimumHeight(42);
    manualBtn->setCheckable(true);
    manualBtn->setChecked(true);
    manualBtn->setStyleSheet(
        "QPushButton { background:#CCFBF1; color:#0F766E; border:1.5px solid #14B8A6;"
        "border-radius:8px; padding:10px; font-size:13px; font-weight:600; }"
        "QPushButton:!checked { background:white; color:#64748B; border:1.5px solid #E2E8F0; font-weight:500; }"
        "QPushButton:!checked:hover { background:#F0FDFA; border-color:#14B8A6; color:#0F766E; }");
    QPushButton *autoBtn = new QPushButton("🤖  Automatic");
    autoBtn->setObjectName("transBtn");
    autoBtn->setMinimumHeight(42);
    autoBtn->setCheckable(true);
    autoBtn->setChecked(false);
    autoBtn->setStyleSheet(
        "QPushButton { background:white; color:#64748B; border:1.5px solid #E2E8F0;"
        "border-radius:8px; padding:10px; font-size:13px; font-weight:500; }"
        "QPushButton:checked { background:#CCFBF1; color:#0F766E; border:1.5px solid #14B8A6; font-weight:600; }"
        "QPushButton:hover { background:#F0FDFA; border-color:#14B8A6; color:#0F766E; }");

    QObject::connect(manualBtn, &QPushButton::clicked, [=]() {
        manualBtn->setChecked(true); autoBtn->setChecked(false); });
    QObject::connect(autoBtn, &QPushButton::clicked, [=]() {
        autoBtn->setChecked(true); manualBtn->setChecked(false); });

    transRow->addWidget(manualBtn);
    transRow->addWidget(autoBtn);
    transCol->addLayout(transRow);
    detailCardL->addLayout(transCol);

    contentLayout->addWidget(detailCard);

    // ── ACTION BUTTONS ─────────────────────────────────────────────────────
    QHBoxLayout *btns = new QHBoxLayout(); btns->setSpacing(10);
    QPushButton *cancelBtn = new QPushButton("Cancel");
    cancelBtn->setObjectName("cancelBtn");
    cancelBtn->setMinimumHeight(46);
    QPushButton *saveBtn = new QPushButton("🚗   Add Vehicle");
    saveBtn->setObjectName("saveBtn");
    saveBtn->setMinimumHeight(46);
    btns->addWidget(cancelBtn, 1);
    btns->addWidget(saveBtn, 2);
    contentLayout->addLayout(btns);

    // hidden QComboBox just to keep compatibility with existing save logic
    QComboBox *transCombo = new QComboBox(dialog);
    transCombo->addItems({"Manual", "Automatic"});
    transCombo->hide();
    QObject::connect(manualBtn, &QPushButton::clicked, [transCombo](){ transCombo->setCurrentIndex(0); });
    QObject::connect(autoBtn,   &QPushButton::clicked, [transCombo](){ transCombo->setCurrentIndex(1); });

    QObject::connect(cancelBtn, &QPushButton::clicked, dialog, &QDialog::reject);

    QObject::connect(saveBtn, &QPushButton::clicked, [&]() {
        QString brand = brandEdit->text().trimmed();
        QString model = modelEdit->text().trimmed();
        QString plate = plateEdit->text().trimmed();

        if (brand.isEmpty() || model.isEmpty() || plate.isEmpty()) {
            QMessageBox::warning(dialog, "Validation", "Brand, Model and Plate Number are required.");
            return;
        }

        QSqlQuery q;
        q.prepare("INSERT INTO cars (driving_school_id, brand, model, year, plate_number, transmission, is_active, image_path) "
                  "VALUES (?, ?, ?, ?, ?, ?, 1, ?)");
        q.addBindValue(schoolId);
        q.addBindValue(brand);
        q.addBindValue(model);
        q.addBindValue(yearSpin->value());
        q.addBindValue(plate);
        q.addBindValue(transCombo->currentText());
        q.addBindValue(photoPath);

        if (!q.exec()) {
            QMessageBox::critical(dialog, "Error",
                "Failed to add vehicle:\n" + q.lastError().text());
            return;
        }

        QMessageBox::information(dialog, "Success", "Vehicle added successfully!");
        dialog->accept();
    });

    dialog->exec();
    loadData();
}

void InstructorDashboard::onLogoutClicked()
{
    // Reset global stylesheet so the login page renders cleanly
    qApp->setStyleSheet("");

    MainWindow *loginWindow = new MainWindow();
    loginWindow->show();
    loginWindow->showMaximized();
    this->hide();
    this->deleteLater();
}
