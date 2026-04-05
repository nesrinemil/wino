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
    ui->tabWidget->addTab(m_adminTabWidget, QString::fromUtf8("📊 Administration"));

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

    // Get counts
    query.prepare("SELECT COUNT(*) FROM students WHERE school_id = ?" + instrFilter + " AND status = 'pending'");
    bindInstr(query);
    if (query.exec() && query.next()) {
        ui->requestsCountLabel->setText(QString::number(query.value(0).toInt()));
    }

    query.prepare("SELECT COUNT(*) FROM students WHERE school_id = ?" + instrFilter + " AND status = 'approved'");
    bindInstr(query);
    if (query.exec() && query.next()) {
        ui->studentsCountLabel->setText(QString::number(query.value(0).toInt()));
    }
    
    query.prepare("SELECT COUNT(*) FROM cars WHERE driving_school_id = ?");
    query.addBindValue(schoolId);
    if (query.exec() && query.next()) {
        int vehicleCount = query.value(0).toInt();
        ui->vehiclesCountLabel->setText(QString::number(vehicleCount));
        ui->tabWidget->setTabText(2, QString("Vehicles (%1)").arg(vehicleCount));
    }
    
    // Update tab titles
    query.prepare("SELECT COUNT(*) FROM students WHERE school_id = ?" + instrFilter + " AND status = 'pending'");
    bindInstr(query);
    if (query.exec() && query.next()) {
        ui->tabWidget->setTabText(0, QString("Requests (%1)").arg(query.value(0).toInt()));
    }

    query.prepare("SELECT COUNT(*) FROM students WHERE school_id = ?" + instrFilter + " AND status = 'approved'");
    bindInstr(query);
    if (query.exec() && query.next()) {
        ui->tabWidget->setTabText(1, QString("Students (%1)").arg(query.value(0).toInt()));
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
    requestsLayout->setContentsMargins(0, 0, 0, 0);
    
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
    studentsLayout->setContentsMargins(0, 0, 0, 0);
    
    query.prepare("SELECT id, name, email, phone FROM students WHERE school_id = ?" + instrFilter + " AND status = 'approved'");
    bindInstr(query);
    
    if (query.exec()) {
        while (query.next()) {
            QWidget *card = new QWidget();
            setupApprovedStudentCard(card, query.value(0).toInt(), query.value(1).toString(),
                                    query.value(2).toString(), query.value(3).toString());
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
    vehiclesLayout->setContentsMargins(0, 0, 0, 0);
    
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
                                                   const QString &email, const QString &phone)
{
    Q_UNUSED(studentId);
    static int approvedCardCounter = 0;
    card->setObjectName(QString("approvedCard%1").arg(approvedCardCounter++));
    card->setStyleSheet("QWidget#" + card->objectName() + " { background-color: white; border-radius: 12px; border: 1px solid #E5E7EB; }");
    card->setMinimumHeight(90);

    QHBoxLayout *layout = new QHBoxLayout(card);
    layout->setContentsMargins(20, 15, 20, 15);
    layout->setSpacing(15);
    
    // Avatar circle
    QLabel *avatar = new QLabel();
    avatar->setStyleSheet("background-color: #14B8A6; border-radius: 25px; min-width: 50px; max-width: 50px; min-height: 50px; max-height: 50px;");
    avatar->setAlignment(Qt::AlignCenter);
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
    // ── Circuit tab (index 3) ──────────────────────────────────────────────────
    if (index == 3) {
        if (!m_circuitDashboard) {
            // First visit — create the widget
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
            m_circuitDashboard = new CircuitDashboard(m_circuitTab);
            // Auto-reset pointer if the widget is ever destroyed
            connect(m_circuitDashboard, &QObject::destroyed, this, [this]() {
                m_circuitDashboard = nullptr;
            });
            m_circuitTab->layout()->addWidget(m_circuitDashboard);
        }
        // Always make visible — handles the case where it was hidden/closed
        m_circuitDashboard->show();
        m_circuitDashboard->raise();
    }

    // ── Sessions tab (index 4) ─────────────────────────────────────────────────
    if (index == 4) {
        qApp->setProperty("currentUserId", instructorId > 0 ? instructorId : 1);

        if (!m_winoTab) {
            WinoBootstrap::bootstrap();
            m_winoTab = new WinoInstructorDashboard(m_winoTabWidget);
            // Auto-reset pointer if the widget is ever destroyed
            connect(m_winoTab, &QObject::destroyed, this, [this]() {
                m_winoTab = nullptr;
            });
            m_winoTabWidget->layout()->addWidget(m_winoTab);
        }
        // Always make visible
        m_winoTab->show();
        m_winoTab->raise();
    }

    // ── Administration tab (index 5) ──────────────────────────────────────────
    if (index == 5) {
        if (!m_adminTab) {
            ParkingDBManager::instance().initialize("", "", "");
            m_adminTab = new AdminWidget(
                QString("Instructor"),
                QString("Moniteur"),
                m_adminTabWidget
            );
            connect(m_adminTab, &QObject::destroyed, this, [this]() {
                m_adminTab = nullptr;
            });
            m_adminTabWidget->layout()->addWidget(m_adminTab);
        }
        m_adminTab->show();
        m_adminTab->raise();
    }

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

    // Fetch student details to sync into Circuit STUDENTS table
    QSqlQuery fetch;
    fetch.prepare("SELECT name, email, phone FROM students WHERE id = ?");
    fetch.addBindValue(studentId);
    if (fetch.exec() && fetch.next()) {
        QString fullName = fetch.value(0).toString().trimmed();
        int sp = fullName.indexOf(' ');
        QString firstName = (sp > 0) ? fullName.left(sp) : fullName;
        QString lastName  = (sp > 0) ? fullName.mid(sp + 1) : QString();
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
    }

    QMessageBox::information(this, "Success", "Student request approved!");
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
    MainWindow *loginWindow = new MainWindow();
    loginWindow->showMaximized();
    this->close();
}
