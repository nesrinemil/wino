#include "admindashboard.h"
#include "ui_admindashboard.h"
#include "mainwindow.h"
#include "database.h"
#include "emailservice.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QDialog>
#include <QScrollArea>
#include <QRegularExpression>
#include <QLineEdit>
#include <QFormLayout>
#include <QStackedWidget>
#include <QCryptographicHash>
#include <QCheckBox>
#include <QSpinBox>
#include <QVector>
#include <QFileDialog>
#include <QFileInfo>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QFile>
#include <memory>

AdminDashboard::AdminDashboard(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::AdminDashboard)
{
    ui->setupUi(this);
    setWindowTitle("Wino - Module 2 - Administrator Panel");
    
    connect(ui->logoutButton, &QPushButton::clicked, this, &AdminDashboard::onLogoutClicked);
    connect(ui->addSchoolButton, &QPushButton::clicked, this, &AdminDashboard::onAddSchoolClicked);
    connect(ui->allFilterButton, &QPushButton::clicked, [this]() { filterSchools("all"); });
    connect(ui->pendingFilterButton, &QPushButton::clicked, [this]() { filterSchools("pending"); });
    connect(ui->approvedFilterButton, &QPushButton::clicked, [this]() { filterSchools("active"); });
    connect(ui->deactivatedFilterButton, &QPushButton::clicked, [this]() { filterSchools("suspended"); });
    
    ensureLogoColumn();
    ensureInstrPhotoColumn();
    loadSchools();
}

AdminDashboard::~AdminDashboard()
{
    delete ui;
}

// ── Ensure logo_path column exists (safe: ORA-01430 = already exists) ────────
void AdminDashboard::ensureLogoColumn()
{
    QSqlQuery q(QSqlDatabase::database());
    q.exec("ALTER TABLE driving_schools ADD (logo_path VARCHAR2(500))");
    // ORA-01430 (column already exists) is intentionally ignored
}

// ── Ensure photo_path column exists in INSTRUCTORS table ─────────────────────
void AdminDashboard::ensureInstrPhotoColumn()
{
    QSqlQuery q(QSqlDatabase::database());
    q.exec("ALTER TABLE instructors ADD (photo_path VARCHAR2(500))");
    // ORA-01430 (column already exists) is intentionally ignored
}

// ── Crop a pixmap into a circle of the given size ────────────────────────────
QPixmap AdminDashboard::makeCircularPixmap(const QPixmap &src, int size)
{
    QPixmap scaled = src.scaled(size, size, Qt::KeepAspectRatioByExpanding,
                                Qt::SmoothTransformation);
    QPixmap result(size, size);
    result.fill(Qt::transparent);
    QPainter p(&result);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addEllipse(0, 0, size, size);
    p.setClipPath(path);
    int xOff = (scaled.width()  - size) / 2;
    int yOff = (scaled.height() - size) / 2;
    p.drawPixmap(-xOff, -yOff, scaled);
    return result;
}

// ── Upload logo for a school — file dialog → copy → save path to Oracle ──────
void AdminDashboard::uploadLogo(int schoolId)
{
    QString path = QFileDialog::getOpenFileName(
        this,
        QString::fromUtf8("Choose school logo"),
        QString(),
        "Images (*.png *.jpg *.jpeg *.bmp *.webp)");
    if (path.isEmpty()) return;

    // Store the path directly in the database
    QSqlQuery q(QSqlDatabase::database());
    q.prepare("UPDATE driving_schools SET logo_path = :p WHERE id = :id");
    q.bindValue(":p",  path);
    q.bindValue(":id", schoolId);
    if (!q.exec()) {
        QMessageBox::warning(this, "Error",
            "Could not save logo path:\n" + q.lastError().text());
        return;
    }
    loadSchools();   // refresh the list
}

void AdminDashboard::loadSchools()
{
    QLayout* oldLayout = ui->schoolsContainer->layout();
    if (oldLayout) {
        QLayoutItem* item;
        while ((item = oldLayout->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        delete oldLayout;
    }
    
    QVBoxLayout *mainLayout = new QVBoxLayout(ui->schoolsContainer);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    QSqlQuery countQuery(QSqlDatabase::database());
    int pendingCount = 0, approvedCount = 0, totalCount = 0;

    countQuery.exec("SELECT COUNT(*) FROM driving_schools WHERE status = 'pending'");
    if (countQuery.next()) pendingCount = countQuery.value(0).toInt();

    countQuery.exec("SELECT COUNT(*) FROM driving_schools WHERE status = 'active'");
    if (countQuery.next()) approvedCount = countQuery.value(0).toInt();

    countQuery.exec("SELECT COUNT(*) FROM driving_schools");
    if (countQuery.next()) totalCount = countQuery.value(0).toInt();
    
    ui->pendingCountLabel->setText(QString::number(pendingCount));
    ui->approvedCountLabel->setText(QString::number(approvedCount));
    ui->totalCountLabel->setText(QString::number(totalCount));
    
    QSqlQuery query(QSqlDatabase::database());
    query.exec("SELECT ds.id, ds.name, "
               "(SELECT COUNT(*) FROM students s WHERE s.driving_school_id = ds.id) AS students_count, "
               "(SELECT COUNT(*) FROM cars c WHERE c.driving_school_id = ds.id) AS vehicles_count, "
               "ds.status, ds.logo_path FROM driving_schools ds ORDER BY ds.id DESC");

    while (query.next()) {
        int id = query.value(0).toInt();
        QString name     = query.value(1).toString();
        int students     = query.value(2).toInt();
        int vehicles     = query.value(3).toInt();
        QString status   = query.value(4).toString();
        QString logoPath = query.value(5).toString();

        QWidget *card = new QWidget();
        card->setObjectName(QString("schoolCard_%1").arg(id));
        card->setProperty("status", status);
        card->setProperty("schoolId", id);
        card->setProperty("schoolName", name);
        setupSchoolCard(card, id, name, students, vehicles, status, logoPath);

        mainLayout->addWidget(card);
    }
    
    mainLayout->addStretch();
}

void AdminDashboard::setupSchoolCard(QWidget *card, int schoolId, const QString &name,
                                      int students, int vehicles, const QString &status,
                                      const QString &logoPath)
{
    QString cName = card->objectName();
    card->setStyleSheet(
        "QWidget#" + cName + " { background-color: white; border-radius: 12px; border: 1px solid #E5E7EB; }"
        "QWidget#" + cName + ":hover { background-color: #F0FDFA; border: 1.5px solid #14B8A6; }"
    );
    card->setMinimumHeight(80);

    QHBoxLayout *cardLayout = new QHBoxLayout(card);
    cardLayout->setContentsMargins(16, 12, 20, 12);
    cardLayout->setSpacing(14);

    // ── Logo circle — single QPushButton with icon (click to upload) ─────────
    const int LOGO_SIZE = 54;
    auto makeLogoPixmap = [=](const QString &path) -> QPixmap {
        QPixmap pix;
        if (!path.isEmpty() && QFile::exists(path) && pix.load(path))
            return makeCircularPixmap(pix, LOGO_SIZE);
        // Initials placeholder
        QPixmap ph(LOGO_SIZE, LOGO_SIZE);
        ph.fill(Qt::transparent);
        QPainter pp(&ph);
        pp.setRenderHint(QPainter::Antialiasing);
        pp.setBrush(QColor("#e0f2f1"));
        pp.setPen(QPen(QColor("#14B8A6"), 2));
        pp.drawEllipse(1, 1, LOGO_SIZE-2, LOGO_SIZE-2);
        pp.setPen(QColor("#0d9488"));
        QFont f = pp.font(); f.setPointSize(16); f.setBold(true); pp.setFont(f);
        pp.drawText(QRect(0, 0, LOGO_SIZE, LOGO_SIZE), Qt::AlignCenter,
                    name.isEmpty() ? QString("?") : QString(name[0]).toUpper());
        return ph;
    };

    QPushButton *logoBtn = new QPushButton(card);
    logoBtn->setFixedSize(LOGO_SIZE, LOGO_SIZE);
    logoBtn->setCursor(Qt::PointingHandCursor);
    logoBtn->setToolTip(QString::fromUtf8("Click to change logo"));
    logoBtn->setIcon(QIcon(makeLogoPixmap(logoPath)));
    logoBtn->setIconSize(QSize(LOGO_SIZE, LOGO_SIZE));
    logoBtn->setStyleSheet(
        "QPushButton { border-radius: 27px; border: 2px solid #14B8A6;"
        "              background: transparent; padding: 0px; }"
        "QPushButton:hover { background: rgba(20,184,166,0.15); }");
    QObject::connect(logoBtn, &QPushButton::clicked, this, [this, schoolId]() {
        uploadLogo(schoolId);
    });
    cardLayout->addWidget(logoBtn);

    // ── School info ───────────────────────────────────────────────────────────
    QVBoxLayout *infoLayout = new QVBoxLayout();

    QLabel *nameLabel = new QLabel(name);
    nameLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #1F1827; background: transparent;");

    QLabel *locationLabel = new QLabel(QString::fromUtf8("📍 Monastir, Monastir"));
    locationLabel->setStyleSheet("font-size: 13px; color: #6B7280; background: transparent;");

    infoLayout->addWidget(nameLabel);
    infoLayout->addWidget(locationLabel);

    cardLayout->addLayout(infoLayout, 3);
    
    QLabel *studentsLabel = new QLabel(QString("👥 %1 students").arg(students));
    studentsLabel->setStyleSheet("font-size: 13px; color: #6B7280; background: transparent;");
    studentsLabel->setMinimumWidth(120);
    cardLayout->addWidget(studentsLabel);

    QLabel *vehiclesLabel = new QLabel(QString("🚗 %1 vehicles").arg(vehicles));
    vehiclesLabel->setStyleSheet("font-size: 13px; color: #6B7280; background: transparent;");
    vehiclesLabel->setMinimumWidth(120);
    cardLayout->addWidget(vehiclesLabel);
    
    // Status badge — active / pending / suspended
    QLabel *statusBadge = new QLabel();
    statusBadge->setAlignment(Qt::AlignCenter);
    statusBadge->setMinimumWidth(100);
    statusBadge->setMaximumWidth(100);
    statusBadge->setMinimumHeight(30);
    statusBadge->setMaximumHeight(30);
    
    if (status == "active") {
        statusBadge->setStyleSheet("background-color: #D1FAE5; color: #065F46; border-radius: 5px; font-size: 12px; font-weight: bold; padding: 5px;");
        statusBadge->setText("Active");
    } else if (status == "pending") {
        statusBadge->setStyleSheet("background-color: #FEF3C7; color: #92400E; border-radius: 5px; font-size: 12px; font-weight: bold; padding: 5px;");
        statusBadge->setText("Pending");
    } else {
        // suspended
        statusBadge->setStyleSheet("background-color: #FEE2E2; color: #991B1B; border-radius: 5px; font-size: 12px; font-weight: bold; padding: 5px;");
        statusBadge->setText("Suspended");
    }
    cardLayout->addWidget(statusBadge);
    
    // View details button
    QPushButton *viewBtn = new QPushButton("👁️");
    viewBtn->setStyleSheet("QPushButton { background-color: transparent; border: none; font-size: 20px; padding: 5px; } QPushButton:hover { background-color: #D1D5DB; border-radius: 5px; }");
    viewBtn->setMinimumSize(40, 40);
    viewBtn->setMaximumSize(40, 40);
    viewBtn->setToolTip("View Details");
    connect(viewBtn, &QPushButton::clicked, [this, schoolId]() { onViewClicked(schoolId); });
    cardLayout->addWidget(viewBtn);
    
    // Suspend / Activate toggle button (all schools can be toggled)
    if (status == "active" || status == "pending") {
        QPushButton *suspendBtn = new QPushButton("Suspend");
        suspendBtn->setStyleSheet("QPushButton { background-color: transparent; border: 2px solid #F59E0B; color: #B45309; border-radius: 5px; padding: 8px 15px; font-size: 13px; font-weight: bold; } QPushButton:hover { background-color: #FEF3C7; }");
        suspendBtn->setMinimumHeight(35);
        connect(suspendBtn, &QPushButton::clicked, [this, schoolId]() { onSuspendClicked(schoolId); });
        cardLayout->addWidget(suspendBtn);
    } else {
        // suspended → can be re-activated
        QPushButton *activateBtn = new QPushButton("Activate");
        activateBtn->setStyleSheet("QPushButton { background-color: transparent; border: 2px solid #10B981; color: #065F46; border-radius: 5px; padding: 8px 15px; font-size: 13px; font-weight: bold; } QPushButton:hover { background-color: #D1FAE5; }");
        activateBtn->setMinimumHeight(35);
        connect(activateBtn, &QPushButton::clicked, [this, schoolId]() { onActivateClicked(schoolId); });
        cardLayout->addWidget(activateBtn);
    }
    
    // Remove button (permanently delete)
    QPushButton *removeBtn = new QPushButton("🗑");
    removeBtn->setStyleSheet("QPushButton { background-color: transparent; border: none; color: #EF4444; font-size: 20px; padding: 5px; } QPushButton:hover { background-color: #FEE2E2; border-radius: 5px; }");
    removeBtn->setMinimumSize(40, 40);
    removeBtn->setMaximumSize(40, 40);
    removeBtn->setToolTip("Remove School");
    connect(removeBtn, &QPushButton::clicked, [this, schoolId, name]() { onRemoveClicked(schoolId, name); });
    cardLayout->addWidget(removeBtn);
}

void AdminDashboard::filterSchools(const QString &filter)
{
    QString activeStyle = "background-color: #14B8A6; color: white;";
    QString inactiveStyle = "background-color: #E5E7EB; color: #6B7280;";
    
    ui->allFilterButton->setStyleSheet(filter == "all" ? activeStyle : inactiveStyle);
    ui->pendingFilterButton->setStyleSheet(filter == "pending" ? activeStyle : inactiveStyle);
    ui->approvedFilterButton->setStyleSheet(filter == "active" ? activeStyle : inactiveStyle);
    ui->deactivatedFilterButton->setStyleSheet(filter == "suspended" ? activeStyle : inactiveStyle);
    
    QList<QWidget*> cards = ui->schoolsContainer->findChildren<QWidget*>(QRegularExpression("schoolCard_\\d+"));
    
    for (QWidget *card : cards) {
        QString status = card->property("status").toString();
        
        if (filter == "all") {
            card->setVisible(true);
        } else {
            card->setVisible(status == filter);
        }
    }
}

void AdminDashboard::onSuspendClicked(int schoolId)
{
    QSqlQuery query;
    query.prepare("UPDATE driving_schools SET status = 'suspended' WHERE id = ?");
    query.addBindValue(schoolId);
    if (query.exec()) {
        QMessageBox::information(this, "Success", "School has been suspended.");
        loadSchools();
    }
}

void AdminDashboard::onActivateClicked(int schoolId)
{
    QSqlQuery query;
    query.prepare("UPDATE driving_schools SET status = 'active' WHERE id = ?");
    query.addBindValue(schoolId);
    if (query.exec()) {
        QMessageBox::information(this, "Success", "School has been activated.");
        loadSchools();
    }
}

void AdminDashboard::onRemoveClicked(int schoolId, const QString &name)
{
    int ret = QMessageBox::warning(this, "Remove School",
        QString("Are you sure you want to permanently remove \"%1\"?\nThis cannot be undone.").arg(name),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (ret != QMessageBox::Yes) return;

    QSqlQuery q;
    q.prepare("DELETE FROM driving_schools WHERE id = ?");
    q.addBindValue(schoolId);
    q.exec();
    // Also remove linked vehicles and instructors
    QSqlQuery qv; qv.prepare("DELETE FROM vehicles WHERE school_id = ?"); qv.addBindValue(schoolId); qv.exec();
    QSqlQuery qi; qi.prepare("DELETE FROM instructors WHERE school_id = ?"); qi.addBindValue(schoolId); qi.exec();
    loadSchools();
}

void AdminDashboard::onViewClicked(int schoolId)
{
    QSqlQuery query;
    query.prepare("SELECT name, status FROM driving_schools WHERE id = ?");
    query.addBindValue(schoolId);
    if (!query.exec() || !query.next()) return;

    QString name     = query.value(0).toString();
    QString status   = query.value(1).toString();

    // ── Dialog shell ────────────────────────────────────────────────────
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("School Details — " + name);
    dialog->setMinimumWidth(520);
    dialog->setMaximumWidth(600);
    dialog->setStyleSheet(
        "QDialog { background-color: white; }"
        "QLabel  { color: #374151; font-size: 13px; }"
        "QScrollArea { border: none; background: transparent; }"
        "QWidget#scrollContents { background: transparent; }"
        "QScrollBar:vertical { border:none; background:#F3F4F6; width:8px; border-radius:4px; }"
        "QScrollBar::handle:vertical { background:#CBD5E0; border-radius:4px; }");

    QVBoxLayout *root = new QVBoxLayout(dialog);
    root->setContentsMargins(28, 24, 28, 24);
    root->setSpacing(18);

    // ── Header row: name + status badge ─────────────────────────────────
    QHBoxLayout *headerRow = new QHBoxLayout();
    QLabel *titleLbl = new QLabel(name);
    titleLbl->setStyleSheet("font-size:20px;font-weight:bold;color:#1F1827;");
    headerRow->addWidget(titleLbl, 1);

    QLabel *statusBadge = new QLabel();
    statusBadge->setAlignment(Qt::AlignCenter);
    statusBadge->setFixedSize(100, 28);
    if (status == "active") {
        statusBadge->setText("Active");
        statusBadge->setStyleSheet("background:#D1FAE5;color:#065F46;border-radius:6px;font-size:12px;font-weight:bold;");
    } else if (status == "pending") {
        statusBadge->setText("Pending");
        statusBadge->setStyleSheet("background:#FEF3C7;color:#92400E;border-radius:6px;font-size:12px;font-weight:bold;");
    } else {
        statusBadge->setText("Suspended");
        statusBadge->setStyleSheet("background:#FEE2E2;color:#991B1B;border-radius:6px;font-size:12px;font-weight:bold;");
    }
    headerRow->addWidget(statusBadge);
    root->addLayout(headerRow);

    QLabel *locationLbl = new QLabel("📍 Monastir, Monastir");
    locationLbl->setStyleSheet("color:#6B7280;font-size:13px;");
    root->addWidget(locationLbl);

    // ── Separator ────────────────────────────────────────────────────────
    auto makeSep = []() -> QFrame* {
        QFrame *f = new QFrame();
        f->setFrameShape(QFrame::HLine);
        f->setStyleSheet("background:#E5E7EB;");
        f->setFixedHeight(1);
        return f;
    };
    root->addWidget(makeSep());

    // ── VEHICLES section ─────────────────────────────────────────────────
    QLabel *vehiclesTitle = new QLabel("🚗  Vehicles");
    vehiclesTitle->setStyleSheet("font-size:15px;font-weight:bold;color:#1F1827;");
    root->addWidget(vehiclesTitle);

    QScrollArea *vScroll = new QScrollArea();
    vScroll->setWidgetResizable(true);
    vScroll->setMaximumHeight(220);
    QWidget *vContents = new QWidget();
    vContents->setObjectName("scrollContents");
    QVBoxLayout *vLayout = new QVBoxLayout(vContents);
    vLayout->setSpacing(8);
    vLayout->setContentsMargins(0, 0, 4, 0);

    // Use CARS table (Oracle) — same table as InstructorDashboard, includes image_path
    QSqlQuery vq(QSqlDatabase::database());
    vq.prepare("SELECT brand, model, year, plate_number, transmission, "
               "CASE WHEN is_active=1 THEN 'active' ELSE 'inactive' END, image_path "
               "FROM cars WHERE driving_school_id = ? ORDER BY id");
    vq.addBindValue(schoolId);
    vq.exec();

    bool hasVehicles = false;
    while (vq.next()) {
        hasVehicles = true;
        QString brand     = vq.value(0).toString();
        QString model     = vq.value(1).toString();
        int     year      = vq.value(2).toInt();
        QString plate     = vq.value(3).toString();
        QString trans     = vq.value(4).toString();
        QString vstatus   = vq.value(5).toString();
        QString imagePath = vq.value(6).toString();

        static int vCardCounter = 0;
        QWidget *vCard = new QWidget();
        vCard->setObjectName(QString("vCard%1").arg(vCardCounter++));
        vCard->setStyleSheet(
            "QWidget#" + vCard->objectName() +
            " { background:#F9FAFB; border:1px solid #E5E7EB; border-radius:10px; }");
        QHBoxLayout *vRow = new QHBoxLayout(vCard);
        vRow->setContentsMargins(12, 10, 12, 10);
        vRow->setSpacing(12);

        // Car photo (real image or emoji fallback)
        QLabel *carIcon = new QLabel();
        carIcon->setFixedSize(56, 56);
        carIcon->setAlignment(Qt::AlignCenter);
        carIcon->setStyleSheet("border-radius:8px; background:#E5E7EB;");
        bool photoLoaded = false;
        if (!imagePath.isEmpty()) {
            QPixmap px(imagePath);
            if (!px.isNull()) {
                carIcon->setPixmap(px.scaled(56, 56, Qt::KeepAspectRatioByExpanding,
                                             Qt::SmoothTransformation));
                carIcon->setStyleSheet("border-radius:8px; background:#E5E7EB;");
                photoLoaded = true;
            }
        }
        if (!photoLoaded) {
            carIcon->setText(QString::fromUtf8("\xf0\x9f\x9a\x97")); // 🚗
            carIcon->setStyleSheet("border-radius:8px; background:#14B8A6; font-size:24px;");
        }
        vRow->addWidget(carIcon);

        QVBoxLayout *vInfo = new QVBoxLayout();
        vInfo->setSpacing(2);
        QLabel *carName = new QLabel(QString("%1 %2 (%3)").arg(brand, model).arg(year));
        carName->setStyleSheet("font-weight:bold;font-size:13px;color:#1F1827;");
        QLabel *carDetail = new QLabel(QString("🪪 %1   ⚙️ %2").arg(plate, trans));
        carDetail->setStyleSheet("font-size:12px;color:#6B7280;");
        vInfo->addWidget(carName);
        vInfo->addWidget(carDetail);
        vRow->addLayout(vInfo, 1);

        QLabel *vsBadge = new QLabel(vstatus == "active" ? "Active" : vstatus == "maintenance" ? "Maintenance" : vstatus);
        vsBadge->setAlignment(Qt::AlignCenter);
        vsBadge->setFixedSize(90, 24);
        if (vstatus == "active")
            vsBadge->setStyleSheet("background:#D1FAE5;color:#065F46;border-radius:5px;font-size:11px;font-weight:bold;");
        else
            vsBadge->setStyleSheet("background:#FEF3C7;color:#92400E;border-radius:5px;font-size:11px;font-weight:bold;");
        vRow->addWidget(vsBadge);

        vLayout->addWidget(vCard);
    }

    if (!hasVehicles) {
        QLabel *noVeh = new QLabel("No vehicles registered yet.");
        noVeh->setStyleSheet("color:#9CA3AF;font-size:13px;");
        noVeh->setAlignment(Qt::AlignCenter);
        vLayout->addWidget(noVeh);
    }
    vLayout->addStretch();
    vScroll->setWidget(vContents);
    root->addWidget(vScroll);

    root->addWidget(makeSep());

    // ── INSTRUCTORS section ──────────────────────────────────────────────
    QSqlQuery iq;
    iq.prepare("SELECT COUNT(*) FROM instructors WHERE school_id = ?");
    iq.addBindValue(schoolId);
    iq.exec(); iq.next();
    int instrTotal = iq.value(0).toInt();

    iq.prepare("SELECT COUNT(*) FROM instructors WHERE school_id = ? AND available = 1");
    iq.addBindValue(schoolId);
    iq.exec(); iq.next();
    int instrAvail = iq.value(0).toInt();

    QLabel *instrTitle = new QLabel(
        QString("👨‍🏫  Instructors  (%1 total, %2 available)").arg(instrTotal).arg(instrAvail));
    instrTitle->setStyleSheet("font-size:15px;font-weight:bold;color:#1F1827;");
    root->addWidget(instrTitle);

    QScrollArea *iScroll = new QScrollArea();
    iScroll->setWidgetResizable(true);
    iScroll->setMaximumHeight(200);
    QWidget *iContents = new QWidget();
    iContents->setObjectName("scrollContents");
    QVBoxLayout *iLayout = new QVBoxLayout(iContents);
    iLayout->setSpacing(8);
    iLayout->setContentsMargins(0, 0, 4, 0);

    iq.prepare("SELECT full_name, email, phone, role, available, photo_path FROM instructors WHERE school_id = ? ORDER BY role DESC, full_name");
    iq.addBindValue(schoolId);
    iq.exec();

    bool hasInstr = false;
    while (iq.next()) {
        hasInstr = true;
        QString iname     = iq.value(0).toString();
        QString email     = iq.value(1).toString();
        QString phone     = iq.value(2).toString();
        QString role      = iq.value(3).toString();
        bool    avail     = iq.value(4).toBool();
        QString photoPath = iq.value(5).toString();

        static int iCardCounter = 0;
        QWidget *iCard = new QWidget();
        iCard->setObjectName(QString("iCard%1").arg(iCardCounter++));
        iCard->setStyleSheet(
            "QWidget#" + iCard->objectName() + " { background:#F9FAFB; border:1px solid #E5E7EB; border-radius:8px; }");
        QHBoxLayout *iRow = new QHBoxLayout(iCard);
        iRow->setContentsMargins(12, 8, 12, 8);
        iRow->setSpacing(12);

        // Avatar: show photo if available, else teal initial
        QLabel *avatar = new QLabel();
        avatar->setFixedSize(46, 46);
        avatar->setAlignment(Qt::AlignCenter);
        {
            QPixmap src;
            if (!photoPath.isEmpty() && QFile::exists(photoPath) && src.load(photoPath)) {
                avatar->setPixmap(makeCircularPixmap(src, 46));
                avatar->setStyleSheet("border-radius:23px; border:2px solid #14B8A6;");
            } else {
                avatar->setText(iname.isEmpty() ? "?" : QString(iname[0].toUpper()));
                avatar->setStyleSheet(
                    "background:#14B8A6;color:white;border-radius:23px;font-size:18px;font-weight:bold;");
            }
        }
        iRow->addWidget(avatar);

        QVBoxLayout *iInfo = new QVBoxLayout();
        iInfo->setSpacing(2);
        QLabel *iName = new QLabel(iname + (role == "admin" ? "  👑" : ""));
        iName->setStyleSheet("font-weight:bold;font-size:13px;color:#1F1827;");
        QString contactStr = (email.isEmpty() ? "" : "✉️ " + email) +
                             (phone.isEmpty() ? "" : "  📞 " + phone);
        QLabel *iContact = new QLabel(contactStr.trimmed());
        iContact->setStyleSheet("font-size:12px;color:#6B7280;");
        iInfo->addWidget(iName);
        if (!contactStr.trimmed().isEmpty()) iInfo->addWidget(iContact);
        iRow->addLayout(iInfo, 1);

        QLabel *availBadge = new QLabel(avail ? "Available" : "Unavailable");
        availBadge->setAlignment(Qt::AlignCenter);
        availBadge->setFixedSize(90, 24);
        if (avail)
            availBadge->setStyleSheet("background:#D1FAE5;color:#065F46;border-radius:5px;font-size:11px;font-weight:bold;");
        else
            availBadge->setStyleSheet("background:#E5E7EB;color:#6B7280;border-radius:5px;font-size:11px;font-weight:bold;");
        iRow->addWidget(availBadge);

        iLayout->addWidget(iCard);
    }

    if (!hasInstr) {
        QLabel *noInstr = new QLabel("No instructors registered yet.");
        noInstr->setStyleSheet("color:#9CA3AF;font-size:13px;");
        noInstr->setAlignment(Qt::AlignCenter);
        iLayout->addWidget(noInstr);
    }
    iLayout->addStretch();
    iScroll->setWidget(iContents);
    root->addWidget(iScroll);

    // ── Close button ─────────────────────────────────────────────────────
    QPushButton *closeBtn = new QPushButton("Close");
    closeBtn->setStyleSheet(
        "background:#14B8A6;color:white;border:none;border-radius:6px;"
        "padding:10px;font-size:14px;font-weight:bold;");
    closeBtn->setMinimumHeight(42);
    connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);
    root->addWidget(closeBtn);

    dialog->exec();
}

void AdminDashboard::onAddSchoolClicked()
{
    // Shared logo path (shared_ptr so both the upload button and doSubmit see the same value)
    auto logoPathPtr = std::make_shared<QString>();

    // ═══════════════════════════════════════════════════════════════════════
    //  SHARED HELPERS
    // ═══════════════════════════════════════════════════════════════════════
    QString baseStyle =
        "QDialog  { background-color: white; }"
        "QLineEdit{ border:1px solid #D1D5DB; border-radius:6px; padding:8px 10px;"
        "           font-size:13px; background:#F9FAFB; color:#1F1827; }"
        "QLineEdit:focus{ border-color:#14B8A6; background:white; }"
        "QSpinBox { border:1px solid #D1D5DB; border-radius:6px; padding:8px 10px;"
        "           font-size:13px; background:#F9FAFB; color:#1F1827; min-height:38px; }"
        "QSpinBox:focus{ border-color:#14B8A6; background:white; }"
        "QSpinBox::up-button, QSpinBox::down-button { width:24px; border:none; background:#E5E7EB; }"
        "QSpinBox::up-button:hover, QSpinBox::down-button:hover { background:#D1D5DB; }"
        "QLabel   { font-size:13px; color:#374151; }"
        "QScrollBar:vertical { border:none; background:#F3F4F6; width:8px; border-radius:4px; }"
        "QScrollBar::handle:vertical { background:#CBD5E0; border-radius:4px; }"
        "QScrollBar::handle:vertical:hover { background:#A0AEC0; }";

    auto btnPrimary = [](const QString &txt) -> QPushButton* {
        auto *b = new QPushButton(txt);
        b->setStyleSheet("background:#14B8A6;color:white;border:none;border-radius:6px;"
                         "padding:10px;font-size:14px;font-weight:bold;margin-top:8px;");
        b->setMinimumHeight(42);
        return b;
    };
    auto btnSecondary = [](const QString &txt) -> QPushButton* {
        auto *b = new QPushButton(txt);
        b->setStyleSheet("background:white;color:#374151;border:1px solid #D1D5DB;"
                         "border-radius:6px;padding:10px;font-size:14px;font-weight:bold;margin-top:8px;");
        b->setMinimumHeight(42);
        return b;
    };
    auto makeField = [](const QString &ph, bool pwd=false) -> QLineEdit* {
        auto *e = new QLineEdit();
        e->setPlaceholderText(ph);
        e->setMinimumHeight(38);
        if (pwd) e->setEchoMode(QLineEdit::Password);
        return e;
    };
    auto stepLabel = [](const QString &txt) -> QLabel* {
        auto *l = new QLabel(txt);
        l->setStyleSheet("color:#14B8A6;font-size:11px;font-weight:bold;letter-spacing:1px;");
        return l;
    };
    auto titleLabel = [](const QString &txt) -> QLabel* {
        auto *l = new QLabel(txt);
        l->setStyleSheet("font-size:19px;font-weight:bold;color:#1F1827;margin-bottom:4px;");
        return l;
    };
    auto noteLabel = [](const QString &txt) -> QLabel* {
        auto *l = new QLabel(txt);
        l->setStyleSheet("color:#9CA3AF;font-size:11px;margin-top:2px;");
        l->setWordWrap(true);
        return l;
    };
    auto sectionLabel = [](const QString &txt) -> QLabel* {
        auto *l = new QLabel(txt);
        l->setStyleSheet("font-size:12px;font-weight:bold;color:#6B7280;"
                         "text-transform:uppercase;letter-spacing:1px;margin-top:6px;");
        return l;
    };

    // ═══════════════════════════════════════════════════════════════════════
    //  DIALOG SETUP
    // ═══════════════════════════════════════════════════════════════════════
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("Add New Driving School");
    dialog->setMinimumWidth(540);
    dialog->setMaximumWidth(600);
    dialog->setStyleSheet(baseStyle);

    QVBoxLayout *rootLayout = new QVBoxLayout(dialog);
    rootLayout->setContentsMargins(32, 28, 32, 28);
    rootLayout->setSpacing(0);

    // Progress bar strip
    QWidget *progressBar = new QWidget();
    progressBar->setFixedHeight(4);
    progressBar->setStyleSheet("background:#E5E7EB; border-radius:2px;");
    QWidget *progressFill = new QWidget(progressBar);
    progressFill->setFixedHeight(4);
    progressFill->setStyleSheet("background:#14B8A6; border-radius:2px;");
    progressFill->setFixedWidth(0);
    rootLayout->addWidget(progressBar);
    rootLayout->addSpacing(20);

    QStackedWidget *stack = new QStackedWidget();
    rootLayout->addWidget(stack);

    // helper to update progress bar  (totalSteps = 2 + numInstructors)
    auto updateProgress = [progressFill, progressBar](int step, int total) {
        int w = progressBar->width();
        if (w < 10) w = 540 - 64;
        progressFill->setFixedWidth(total > 0 ? w * step / total : 0);
    };

    // ═══════════════════════════════════════════════════════════════════════
    //  PAGE 0 (stack index 0) — SCHOOL INFORMATION
    // ═══════════════════════════════════════════════════════════════════════
    QWidget *page1 = new QWidget();
    QVBoxLayout *p1 = new QVBoxLayout(page1);
    p1->setSpacing(10);

    // step label updated dynamically; create placeholder
    QLabel *step1Label = stepLabel("STEP 1  —  SCHOOL INFORMATION");
    p1->addWidget(step1Label);
    p1->addWidget(titleLabel("Driving School Details"));
    p1->addSpacing(4);

    p1->addWidget(sectionLabel("Basic Info"));
    QLineEdit *nameEdit    = makeField("School name *");
    QLineEdit *phoneEdit   = makeField("Phone number");
    QLineEdit *emailEdit   = makeField("Email address");
    p1->addWidget(nameEdit);
    QHBoxLayout *r1 = new QHBoxLayout(); r1->setSpacing(10);
    r1->addWidget(phoneEdit); r1->addWidget(emailEdit);
    p1->addLayout(r1);

    // ── Logo upload ──────────────────────────────────────────────────────
    p1->addSpacing(8);
    p1->addWidget(sectionLabel("Logo (optional)"));

    QHBoxLayout *logoRow = new QHBoxLayout();
    logoRow->setSpacing(16);

    // Circular preview
    const int PREV = 64;
    QLabel *logoPreview = new QLabel();
    logoPreview->setFixedSize(PREV, PREV);
    auto drawPlaceholder = [=]() {
        QPixmap pm(PREV, PREV); pm.fill(Qt::transparent);
        QPainter pp(&pm); pp.setRenderHint(QPainter::Antialiasing);
        pp.setBrush(QColor("#e0f2f1")); pp.setPen(QPen(QColor("#14B8A6"), 2));
        pp.drawEllipse(1, 1, PREV-2, PREV-2);
        pp.setPen(QColor("#9ca3af")); pp.setFont(QFont("Segoe UI", 22));
        pp.drawText(QRect(0,0,PREV,PREV), Qt::AlignCenter, "🖼");
        logoPreview->setPixmap(pm);
        logoPreview->setStyleSheet("border-radius:32px; border:2px dashed #14B8A6;");
    };
    drawPlaceholder();

    QVBoxLayout *logoBtnCol = new QVBoxLayout();
    logoBtnCol->setSpacing(6);

    QPushButton *uploadLogoBtn = new QPushButton(QString::fromUtf8("📁  Choose a logo"));
    uploadLogoBtn->setStyleSheet(
        "QPushButton { background:#f0fdfa; border:1.5px solid #14B8A6; color:#0d9488;"
        " border-radius:7px; padding:7px 18px; font-size:13px; font-weight:600; }"
        "QPushButton:hover { background:#ccfbf1; }");

    QLabel *logoNameLbl = new QLabel(QString::fromUtf8("No file selected"));
    logoNameLbl->setStyleSheet("color:#9ca3af; font-size:11px;");
    logoNameLbl->setWordWrap(true);

    connect(uploadLogoBtn, &QPushButton::clicked, dialog, [=]() mutable {
        QString path = QFileDialog::getOpenFileName(
            dialog,
            QString::fromUtf8("Choose logo"),
            QString(),
            "Images (*.png *.jpg *.jpeg *.bmp *.webp)");
        if (path.isEmpty()) return;
        *logoPathPtr = path;
        // Show preview
        QPixmap src(path);
        if (!src.isNull())
            logoPreview->setPixmap(makeCircularPixmap(src, PREV));
        logoPreview->setStyleSheet("border-radius:32px; border:2px solid #14B8A6;");
        logoNameLbl->setText(QFileInfo(path).fileName());
        logoNameLbl->setStyleSheet("color:#0d9488; font-size:11px;");
    });

    logoBtnCol->addWidget(uploadLogoBtn);
    logoBtnCol->addWidget(logoNameLbl);
    logoBtnCol->addStretch();

    logoRow->addWidget(logoPreview);
    logoRow->addLayout(logoBtnCol, 1);
    p1->addLayout(logoRow);

    // Address is always Monastir, Monastir

    // ── Instructor count picker ──────────────────────────────────────────
    p1->addSpacing(6);
    p1->addWidget(sectionLabel("Instructors"));

    QHBoxLayout *instrCountRow = new QHBoxLayout();
    instrCountRow->setSpacing(12);
    QLabel *instrCountLbl = new QLabel("Number of instructors to add (0 = none):");
    instrCountLbl->setStyleSheet("font-size:13px;color:#374151;");
    QSpinBox *instrCountSpin = new QSpinBox();
    instrCountSpin->setRange(0, 20);
    instrCountSpin->setValue(0);
    instrCountSpin->setMinimumWidth(80);
    instrCountRow->addWidget(instrCountLbl, 1);
    instrCountRow->addWidget(instrCountSpin);
    p1->addLayout(instrCountRow);

    p1->addWidget(noteLabel("School will be added as Active immediately."));

    QPushButton *next1Btn = btnPrimary("Next  →");
    p1->addWidget(next1Btn);
    stack->addWidget(page1);   // stack index 0

    // ═══════════════════════════════════════════════════════════════════════
    //  INSTRUCTOR PAGES (stack indices 2 … 2+N-1) — built dynamically
    //  We keep them in a vector; we rebuild when numInstructors changes.
    //  For simplicity we pre-build up to 20 pages and show/navigate them.
    // ═══════════════════════════════════════════════════════════════════════

    // Per-instructor field sets
    struct InstrFields {
        QLineEdit *nameEdit;
        QLineEdit *emailEdit;
        QLineEdit *phoneEdit;
        QLineEdit *d17Edit;   // D17 payment ID — required
        QLineEdit *pwdEdit;
        QLineEdit *cfmEdit;
        QLabel    *stepLbl;
        QPushButton *backBtn;
        QPushButton *nextBtn;  // nullptr on last page (replaced by submitBtn)
        QString   *photoPath;  // instructor photo file path
    };

    const int MAX_INSTR = 20;
    QVector<InstrFields> instrPages(MAX_INSTR);

    for (int i = 0; i < MAX_INSTR; ++i) {
        QWidget *pg = new QWidget();
        QVBoxLayout *lay = new QVBoxLayout(pg);
        lay->setSpacing(10);

        QLabel *sLbl = stepLabel(QString("STEP %1  —  INSTRUCTOR %2").arg(i + 2).arg(i + 1));
        lay->addWidget(sLbl);
        lay->addWidget(titleLabel(QString("Instructor #%1 Details").arg(i + 1)));
        lay->addSpacing(4);

        // Numbered avatar badge
        QHBoxLayout *badgeRow = new QHBoxLayout();
        QLabel *badge = new QLabel(QString::number(i + 1));
        badge->setAlignment(Qt::AlignCenter);
        badge->setFixedSize(36, 36);
        badge->setStyleSheet(
            "background:#14B8A6;color:white;border-radius:18px;"
            "font-size:15px;font-weight:bold;");
        QLabel *badgeLbl = new QLabel(QString(" Instructor %1 of N").arg(i + 1));
        badgeLbl->setStyleSheet("color:#6B7280;font-size:13px;");
        badgeRow->addWidget(badge);
        badgeRow->addWidget(badgeLbl, 1);
        lay->addLayout(badgeRow);
        lay->addSpacing(4);

        lay->addWidget(sectionLabel("Instructor Info"));

        QLineEdit *ne = makeField("Full name *");
        QLineEdit *ee = makeField("Email address *");
        QLineEdit *pe = makeField("Phone number");

        // ── D17 ID field (required for WINO payment module) ───────────────────
        QLineEdit *d17 = makeField("D17 ID *  (payment identifier)");
        d17->setPlaceholderText("e.g. D17-XXXXXXXX");
        QLabel *d17Note = noteLabel("⚠  Required — used for student payment verification in the Sessions module.");
        d17Note->setStyleSheet("color:#F59E0B; font-size:11px; padding-left:4px;");

        QLineEdit *pw = makeField("Password *", true);
        QLineEdit *cm = makeField("Confirm password *", true);

        lay->addWidget(ne);
        lay->addWidget(ee);
        lay->addWidget(pe);
        lay->addWidget(d17);
        lay->addWidget(d17Note);
        lay->addWidget(pw);
        lay->addWidget(cm);
        lay->addWidget(noteLabel("Password must be at least 6 characters."));

        // ── Photo upload ──────────────────────────────────────────────────────
        lay->addSpacing(6);
        lay->addWidget(sectionLabel("Photo (optional)"));

        QString *photoPtr = new QString();   // heap-allocated, lives with the dialog

        const int IPRV = 56;
        QLabel *photoPreview = new QLabel();
        photoPreview->setFixedSize(IPRV, IPRV);
        {
            QPixmap pm(IPRV, IPRV); pm.fill(Qt::transparent);
            QPainter pp(&pm); pp.setRenderHint(QPainter::Antialiasing);
            pp.setBrush(QColor("#e0f2f1")); pp.setPen(QPen(QColor("#14B8A6"), 2));
            pp.drawEllipse(1, 1, IPRV-2, IPRV-2);
            pp.setPen(QColor("#9ca3af")); pp.setFont(QFont("Segoe UI", 18));
            pp.drawText(QRect(0,0,IPRV,IPRV), Qt::AlignCenter, "👤");
            photoPreview->setPixmap(pm);
        }
        photoPreview->setStyleSheet("border-radius:28px; border:2px dashed #14B8A6;");

        QLabel *photoNameLbl = new QLabel(QString::fromUtf8("No photo selected"));
        photoNameLbl->setStyleSheet("color:#9ca3af; font-size:11px;");
        photoNameLbl->setWordWrap(true);

        QPushButton *uploadPhotoBtn = new QPushButton(QString::fromUtf8("📷  Upload Photo"));
        uploadPhotoBtn->setStyleSheet(
            "QPushButton { background:#f0fdfa; border:1.5px solid #14B8A6; color:#0d9488;"
            " border-radius:7px; padding:6px 16px; font-size:13px; font-weight:600; }"
            "QPushButton:hover { background:#ccfbf1; }");

        QHBoxLayout *photoRow = new QHBoxLayout();
        photoRow->setSpacing(14);
        QVBoxLayout *photoBtnCol = new QVBoxLayout();
        photoBtnCol->setSpacing(5);
        photoBtnCol->addWidget(uploadPhotoBtn);
        photoBtnCol->addWidget(photoNameLbl);
        photoBtnCol->addStretch();
        photoRow->addWidget(photoPreview);
        photoRow->addLayout(photoBtnCol, 1);
        lay->addLayout(photoRow);

        connect(uploadPhotoBtn, &QPushButton::clicked, dialog, [=]() mutable {
            QString path = QFileDialog::getOpenFileName(
                dialog,
                QString::fromUtf8("Choose instructor photo"),
                QString(),
                "Images (*.png *.jpg *.jpeg *.bmp *.webp)");
            if (path.isEmpty()) return;
            *photoPtr = path;
            QPixmap src(path);
            if (!src.isNull())
                photoPreview->setPixmap(makeCircularPixmap(src, IPRV));
            photoPreview->setStyleSheet("border-radius:28px; border:2px solid #14B8A6;");
            photoNameLbl->setText(QFileInfo(path).fileName());
            photoNameLbl->setStyleSheet("color:#0d9488; font-size:11px;");
        });

        QHBoxLayout *btns = new QHBoxLayout(); btns->setSpacing(10);
        QPushButton *bk = btnSecondary("← Back");
        QPushButton *nx = btnPrimary("Next  →");  // overridden to Submit on last page
        btns->addWidget(bk);
        btns->addWidget(nx);
        lay->addLayout(btns);

        instrPages[i] = { ne, ee, pe, d17, pw, cm, sLbl, bk, nx, photoPtr };
        stack->addWidget(pg);   // stack index = 1 + i
    }

    // ═══════════════════════════════════════════════════════════════════════
    //  HELPERS that depend on instrCount
    // ═══════════════════════════════════════════════════════════════════════

    // totalSteps = school(1) + N instructors
    // We'll compute it inside each lambda via instrCountSpin->value()

    auto totalSteps = [instrCountSpin]() { return 1 + instrCountSpin->value(); };

    // Update all step labels to reflect current N
    auto refreshLabels = [&]() {
        int n = instrCountSpin->value();
        int tot = 1 + n;
        step1Label->setText(QString("STEP 1 OF %1  —  SCHOOL INFORMATION").arg(tot));
        for (int i = 0; i < MAX_INSTR; ++i) {
            instrPages[i].stepLbl->setText(
                QString("STEP %1 OF %2  —  INSTRUCTOR %3").arg(i + 2).arg(tot).arg(i + 1));
            // Update "N" in badge label
            QWidget *pg = stack->widget(1 + i);
            QList<QLabel*> labels = pg->findChildren<QLabel*>();
            for (auto *lbl : labels) {
                if (lbl->text().startsWith(" Instructor ") && lbl->text().contains(" of ")) {
                    lbl->setText(QString(" Instructor %1 of %2").arg(i + 1).arg(n));
                    break;
                }
            }
            // Last instructor page: button says "Create School", others say "Next"
            if (n > 0) {
                if (i == n - 1) {
                    instrPages[i].nextBtn->setText("✓  Create School");
                    instrPages[i].nextBtn->setStyleSheet(
                        "background:#10B981;color:white;border:none;border-radius:6px;"
                        "padding:10px;font-size:14px;font-weight:bold;margin-top:8px;");
                } else {
                    instrPages[i].nextBtn->setText("Next  →");
                    instrPages[i].nextBtn->setStyleSheet(
                        "background:#14B8A6;color:white;border:none;border-radius:6px;"
                        "padding:10px;font-size:14px;font-weight:bold;margin-top:8px;");
                }
            }
        }
        // When n == 0, school page "Next" becomes "Create School"
        if (n == 0) {
            next1Btn->setText("✓  Create School");
            next1Btn->setStyleSheet(
                "background:#10B981;color:white;border:none;border-radius:6px;"
                "padding:10px;font-size:14px;font-weight:bold;margin-top:8px;");
        } else {
            next1Btn->setText("Next  →");
            next1Btn->setStyleSheet(
                "background:#14B8A6;color:white;border:none;border-radius:6px;"
                "padding:10px;font-size:14px;font-weight:bold;margin-top:8px;");
        }
    };

    // ── Initial label refresh ────────────────────────────────────────────
    refreshLabels();
    connect(instrCountSpin, QOverload<int>::of(&QSpinBox::valueChanged), [=](int) {
        refreshLabels();
    });

    // ═══════════════════════════════════════════════════════════════════════
    //  SUBMIT LAMBDA  (shared — called from admin page when N=0 and
    //                  from last instructor page when N>0)
    // ═══════════════════════════════════════════════════════════════════════
    auto hashPwd = [](const QString &pwd) -> QString {
        return QString(QCryptographicHash::hash(pwd.toUtf8(),
                       QCryptographicHash::Sha256).toHex());
    };

    auto doSubmit = [=, &instrPages]() {
        // ── Insert school ──────────────────────────────────────────────
        // Get next ID from sequence first (Oracle compatible)
        QSqlQuery seqQ("SELECT driving_schools_seq.NEXTVAL FROM DUAL");
        seqQ.next();
        int newSchoolId = seqQ.value(0).toInt();

        QSqlQuery sq;
        sq.prepare(
            "INSERT INTO driving_schools "
            "(id, admin_id, name, phone, email, status) "
            "VALUES (?, 1, ?, ?, ?, 'active')");
        sq.addBindValue(newSchoolId);
        sq.addBindValue(nameEdit->text().trimmed());
        sq.addBindValue(phoneEdit->text().trimmed());
        sq.addBindValue(emailEdit->text().trimmed());

        if (!sq.exec()) {
            QMessageBox::critical(dialog, "Database Error",
                "Failed to create school:\n" + sq.lastError().text());
            return;
        }

        // Save logo path if one was selected
        if (!logoPathPtr->isEmpty()) {
            QSqlQuery lq(QSqlDatabase::database());
            lq.prepare("UPDATE driving_schools SET logo_path = :p WHERE id = :id");
            lq.bindValue(":p",  *logoPathPtr);
            lq.bindValue(":id", newSchoolId);
            lq.exec();
        }

        QString schoolName = nameEdit->text().trimmed();

        // ── Insert instructors into INSTRUCTORS ────────────────────────
        int n = instrCountSpin->value();
        int insertedNormal = 0;
        for (int i = 0; i < n; ++i) {
            QSqlQuery nq;
            nq.prepare(
                "INSERT INTO instructors (school_id, full_name, email, phone, password_hash, d17_id, role, photo_path) "
                "VALUES (?, ?, ?, ?, ?, ?, 'instructor', ?)");
            nq.addBindValue(newSchoolId);
            nq.addBindValue(instrPages[i].nameEdit->text().trimmed());
            nq.addBindValue(instrPages[i].emailEdit->text().trimmed());
            nq.addBindValue(instrPages[i].phoneEdit->text().trimmed());
            nq.addBindValue(hashPwd(instrPages[i].pwdEdit->text()));
            nq.addBindValue(instrPages[i].d17Edit->text().trimmed());
            nq.addBindValue(instrPages[i].photoPath && !instrPages[i].photoPath->isEmpty()
                            ? *instrPages[i].photoPath : QVariant(QMetaType(QMetaType::QString)));
            if (nq.exec()) {
                ++insertedNormal;
            }
        }

        QString msg = "Driving school created successfully!\nStatus: Active";
        if (n > 0)
            msg += QString("\n%1 instructor(s) added.").arg(insertedNormal);

        // ── Send email notifications via EmailJS ──────────────────
        EmailService &email = EmailService::instance();

        // Notify each instructor their account is created
        for (int i = 0; i < n; ++i) {
            if (!instrPages[i].emailEdit->text().trimmed().isEmpty()) {
                email.sendNewStudentNotification(
                    instrPages[i].nameEdit->text().trimmed(),
                    instrPages[i].emailEdit->text().trimmed(),
                    "New school setup — welcome aboard!",
                    nameEdit->text().trimmed()
                );
            }
        }
        // ─────────────────────────────────────────────────────────

        QMessageBox::information(dialog, "Success", msg);
        dialog->accept();
        loadSchools();
    };

    // ═══════════════════════════════════════════════════════════════════════
    //  CONNECTIONS
    // ═══════════════════════════════════════════════════════════════════════

    // Step 1 → first instructor page (or submit if N=0)
    connect(next1Btn, &QPushButton::clicked, [=]() {
        if (nameEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(dialog, "Validation", "School name is required.");
            return;
        }
        int n = instrCountSpin->value();
        if (n == 0) {
            doSubmit();
        } else {
            stack->setCurrentIndex(1);
            updateProgress(1, totalSteps());
        }
    });

    // Wire each instructor page's back/next buttons
    for (int i = 0; i < MAX_INSTR; ++i) {
        // Back button
        connect(instrPages[i].backBtn, &QPushButton::clicked, [=, &instrPages]() {
            // Which index am i?
            int myIdx = stack->currentIndex();  // = 2 + i
            stack->setCurrentIndex(myIdx - 1);
            updateProgress(myIdx - 1, totalSteps());
        });

        // Next/Submit button
        connect(instrPages[i].nextBtn, &QPushButton::clicked, [=, &instrPages, i]() {
            int n = instrCountSpin->value();

            // Validate this instructor
            if (instrPages[i].nameEdit->text().trimmed().isEmpty()) {
                QMessageBox::warning(dialog, "Validation",
                    QString("Instructor %1: full name is required.").arg(i + 1));
                return;
            }
            if (instrPages[i].emailEdit->text().trimmed().isEmpty()) {
                QMessageBox::warning(dialog, "Validation",
                    QString("Instructor %1: email is required.").arg(i + 1));
                return;
            }
            if (instrPages[i].d17Edit->text().trimmed().isEmpty()) {
                QMessageBox::warning(dialog, "Validation",
                    QString("Instructor %1: D17 ID is required for payment processing.").arg(i + 1));
                instrPages[i].d17Edit->setFocus();
                instrPages[i].d17Edit->setStyleSheet(instrPages[i].d17Edit->styleSheet() +
                    "border: 2px solid #EF4444;");
                return;
            }
            if (instrPages[i].pwdEdit->text().length() < 6) {
                QMessageBox::warning(dialog, "Validation",
                    QString("Instructor %1: password must be at least 6 characters.").arg(i + 1));
                return;
            }
            if (instrPages[i].pwdEdit->text() != instrPages[i].cfmEdit->text()) {
                QMessageBox::warning(dialog, "Validation",
                    QString("Instructor %1: passwords do not match.").arg(i + 1));
                return;
            }

            if (i == n - 1) {
                // Last instructor — submit
                doSubmit();
            } else {
                int nextIdx = 1 + i + 1;
                stack->setCurrentIndex(nextIdx);
                updateProgress(nextIdx, totalSteps());
            }
        });
    }

    // Initial progress
    updateProgress(0, totalSteps());
    dialog->exec();
}

void AdminDashboard::onLogoutClicked()
{
    qApp->setStyleSheet("");

    MainWindow *loginWindow = new MainWindow();
    loginWindow->show();
    loginWindow->showMaximized();
    this->hide();
    this->deleteLater();
}
