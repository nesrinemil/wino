#include "admindashboard.h"
#include "ui_admindashboard.h"
#include "mainwindow.h"
#include "database.h"
#include "emailservice.h"
#include <QSqlQuery>
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
    
    loadSchools();
}

AdminDashboard::~AdminDashboard()
{
    delete ui;
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
    
    QSqlQuery countQuery;
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
    
    QSqlQuery query("SELECT ds.id, ds.name, "
                    "(SELECT COUNT(*) FROM students s WHERE s.driving_school_id = ds.id) AS students_count, "
                    "(SELECT COUNT(*) FROM cars c WHERE c.driving_school_id = ds.id) AS vehicles_count, "
                    "ds.status FROM driving_schools ds ORDER BY ds.id DESC");
    
    while (query.next()) {
        int id = query.value(0).toInt();
        QString name = query.value(1).toString();
        int students = query.value(2).toInt();
        int vehicles = query.value(3).toInt();
        QString status = query.value(4).toString();
        
        QWidget *card = new QWidget();
        card->setObjectName(QString("schoolCard_%1").arg(id));
        card->setProperty("status", status);
        card->setProperty("schoolId", id);
        card->setProperty("schoolName", name);
        setupSchoolCard(card, id, name, students, vehicles, status);
        
        mainLayout->addWidget(card);
    }
    
    mainLayout->addStretch();
}

void AdminDashboard::setupSchoolCard(QWidget *card, int schoolId, const QString &name,
                                      int students, int vehicles, const QString &status)
{
    QString cName = card->objectName();
    card->setStyleSheet(
        "QWidget#" + cName + " { background-color: white; border-radius: 12px; border: 1px solid #E5E7EB; }"
        "QWidget#" + cName + ":hover { background-color: #F0FDFA; border: 1.5px solid #14B8A6; }"
    );
    card->setMinimumHeight(80);

    QHBoxLayout *cardLayout = new QHBoxLayout(card);
    cardLayout->setContentsMargins(20, 14, 20, 14);
    cardLayout->setSpacing(16);
    
    QVBoxLayout *infoLayout = new QVBoxLayout();
    
    QLabel *nameLabel = new QLabel(name);
    nameLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #1F1827; background: transparent;");

    QLabel *locationLabel = new QLabel("📍 Monastir, Monastir");
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
    // Also remove linked cars and instructors
    QSqlQuery qv; qv.prepare("DELETE FROM cars WHERE driving_school_id = ?"); qv.addBindValue(schoolId); qv.exec();
    QSqlQuery qi; qi.prepare("DELETE FROM admin_instructors WHERE driving_school_id = ?"); qi.addBindValue(schoolId); qi.exec();
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

    QSqlQuery vq;
    vq.prepare("SELECT brand, model, year, plate_number, transmission, "
               "CASE WHEN is_active=1 THEN 'active' ELSE 'inactive' END AS status "
               "FROM cars WHERE driving_school_id = ? ORDER BY id");
    vq.addBindValue(schoolId);
    vq.exec();

    bool hasVehicles = false;
    while (vq.next()) {
        hasVehicles = true;
        QString brand    = vq.value(0).toString();
        QString model    = vq.value(1).toString();
        int     year     = vq.value(2).toInt();
        QString plate    = vq.value(3).toString();
        QString trans    = vq.value(4).toString();
        QString vstatus  = vq.value(5).toString();

        static int vCardCounter = 0;
        QWidget *vCard = new QWidget();
        vCard->setObjectName(QString("vCard%1").arg(vCardCounter++));
        vCard->setStyleSheet(
            "QWidget#" + vCard->objectName() + " { background:#F9FAFB; border:1px solid #E5E7EB; border-radius:8px; }");
        QHBoxLayout *vRow = new QHBoxLayout(vCard);
        vRow->setContentsMargins(12, 8, 12, 8);
        vRow->setSpacing(12);

        // Car icon placeholder (grey box, simulates photo)
        QLabel *carIcon = new QLabel("🚗");
        carIcon->setFixedSize(48, 48);
        carIcon->setAlignment(Qt::AlignCenter);
        carIcon->setStyleSheet(
            "background:#E5E7EB; border-radius:6px; font-size:26px;");
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
    iq.prepare("SELECT COUNT(*) FROM admin_instructors WHERE driving_school_id = ?");
    iq.addBindValue(schoolId);
    iq.exec(); iq.next();
    int instrTotal = iq.value(0).toInt();

    iq.prepare("SELECT COUNT(*) FROM admin_instructors WHERE driving_school_id = ?");
    iq.addBindValue(schoolId);
    iq.exec(); iq.next();
    int instrAvail = iq.value(0).toInt(); // all admin_instructors considered available

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

    iq.prepare("SELECT full_name, email, school_name, 'instructor' AS role, 1 AS available "
               "FROM admin_instructors WHERE driving_school_id = ? ORDER BY full_name");
    iq.addBindValue(schoolId);
    iq.exec();

    bool hasInstr = false;
    while (iq.next()) {
        hasInstr = true;
        QString iname  = iq.value(0).toString();
        QString email  = iq.value(1).toString();
        QString phone  = iq.value(2).toString(); // school_name used as phone placeholder
        QString role   = iq.value(3).toString();
        bool    avail  = iq.value(4).toBool();

        static int iCardCounter = 0;
        QWidget *iCard = new QWidget();
        iCard->setObjectName(QString("iCard%1").arg(iCardCounter++));
        iCard->setStyleSheet(
            "QWidget#" + iCard->objectName() + " { background:#F9FAFB; border:1px solid #E5E7EB; border-radius:8px; }");
        QHBoxLayout *iRow = new QHBoxLayout(iCard);
        iRow->setContentsMargins(12, 8, 12, 8);
        iRow->setSpacing(12);

        // Avatar circle with initials
        QLabel *avatar = new QLabel(iname.isEmpty() ? "?" : QString(iname[0].toUpper()));
        avatar->setFixedSize(42, 42);
        avatar->setAlignment(Qt::AlignCenter);
        avatar->setStyleSheet(
            "background:#14B8A6;color:white;border-radius:21px;font-size:18px;font-weight:bold;");
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
    //  PAGE 1 (stack index 1) — ADMIN INSTRUCTOR
    // ═══════════════════════════════════════════════════════════════════════
    QWidget *page2 = new QWidget();
    QVBoxLayout *p2 = new QVBoxLayout(page2);
    p2->setSpacing(10);

    QLabel *step2Label = stepLabel("STEP 2  —  ADMIN INSTRUCTOR ACCOUNT");
    p2->addWidget(step2Label);
    p2->addWidget(titleLabel("Admin Instructor Credentials"));
    p2->addSpacing(4);
    p2->addWidget(noteLabel("This account will have admin-level access for the school."));
    p2->addSpacing(4);

    QLineEdit *adminNameEdit  = makeField("Full name *");
    QLineEdit *adminEmailEdit = makeField("Email address *");
    QLineEdit *adminPwdEdit   = makeField("Password *", true);
    QLineEdit *adminCfmEdit   = makeField("Confirm password *", true);
    p2->addWidget(adminNameEdit);
    p2->addWidget(adminEmailEdit);
    p2->addWidget(adminPwdEdit);
    p2->addWidget(adminCfmEdit);
    p2->addWidget(noteLabel("Password must be at least 6 characters."));

    QHBoxLayout *p2Btns = new QHBoxLayout(); p2Btns->setSpacing(10);
    QPushButton *back2Btn = btnSecondary("← Back");
    QPushButton *next2Btn = btnPrimary("Next  →");
    p2Btns->addWidget(back2Btn);
    p2Btns->addWidget(next2Btn);
    p2->addLayout(p2Btns);
    stack->addWidget(page2);   // stack index 1

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
        QLineEdit *pwdEdit;
        QLineEdit *cfmEdit;
        QLabel    *stepLbl;
        QPushButton *backBtn;
        QPushButton *nextBtn;  // nullptr on last page (replaced by submitBtn)
    };

    const int MAX_INSTR = 20;
    QVector<InstrFields> instrPages(MAX_INSTR);

    for (int i = 0; i < MAX_INSTR; ++i) {
        QWidget *pg = new QWidget();
        QVBoxLayout *lay = new QVBoxLayout(pg);
        lay->setSpacing(10);

        QLabel *sLbl = stepLabel(QString("STEP %1  —  INSTRUCTOR %2").arg(i + 3).arg(i + 1));
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
        QLineEdit *pw = makeField("Password *", true);
        QLineEdit *cm = makeField("Confirm password *", true);

        lay->addWidget(ne);
        lay->addWidget(ee);
        lay->addWidget(pe);
        lay->addWidget(pw);
        lay->addWidget(cm);
        lay->addWidget(noteLabel("Password must be at least 6 characters."));

        QHBoxLayout *btns = new QHBoxLayout(); btns->setSpacing(10);
        QPushButton *bk = btnSecondary("← Back");
        QPushButton *nx = btnPrimary("Next  →");  // overridden to Submit on last page
        btns->addWidget(bk);
        btns->addWidget(nx);
        lay->addLayout(btns);

        instrPages[i] = { ne, ee, pe, pw, cm, sLbl, bk, nx };
        stack->addWidget(pg);   // stack index = 2 + i
    }

    // ═══════════════════════════════════════════════════════════════════════
    //  HELPERS that depend on instrCount
    // ═══════════════════════════════════════════════════════════════════════

    // totalSteps = school(1) + admin(1) + N instructors
    // We'll compute it inside each lambda via instrCountSpin->value()

    auto totalSteps = [instrCountSpin]() { return 2 + instrCountSpin->value(); };

    // Update all step labels to reflect current N
    auto refreshLabels = [&]() {
        int n = instrCountSpin->value();
        int tot = 2 + n;
        step1Label->setText(QString("STEP 1 OF %1  —  SCHOOL INFORMATION").arg(tot));
        step2Label->setText(QString("STEP 2 OF %1  —  ADMIN INSTRUCTOR ACCOUNT").arg(tot));
        for (int i = 0; i < MAX_INSTR; ++i) {
            instrPages[i].stepLbl->setText(
                QString("STEP %1 OF %2  —  INSTRUCTOR %3").arg(i + 3).arg(tot).arg(i + 1));
            // Update "N" in badge label
            QWidget *pg = stack->widget(2 + i);
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
        // When n == 0, admin page "Next" becomes "Create School"
        if (n == 0) {
            next2Btn->setText("✓  Create School");
            next2Btn->setStyleSheet(
                "background:#10B981;color:white;border:none;border-radius:6px;"
                "padding:10px;font-size:14px;font-weight:bold;margin-top:8px;");
        } else {
            next2Btn->setText("Next  →");
            next2Btn->setStyleSheet(
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

        int currentAdminId = qApp->property("currentUserId").toInt();
        if (currentAdminId <= 0) currentAdminId = 1; // Fallback

        QSqlQuery sq;
        sq.prepare(
            "INSERT INTO driving_schools "
            "(id, admin_id, name, phone, email, status) "
            "VALUES (?, ?, ?, ?, ?, 'active')");
        sq.addBindValue(newSchoolId);
        sq.addBindValue(currentAdminId);
        sq.addBindValue(nameEdit->text().trimmed());
        sq.addBindValue(phoneEdit->text().trimmed());
        sq.addBindValue(emailEdit->text().trimmed());

        if (!sq.exec()) {
            QMessageBox::critical(dialog, "Database Error",
                "Failed to create school:\n" + sq.lastError().text());
            return;
        }

        QString schoolName = nameEdit->text().trimmed();

        // ── Insert admin instructor into ADMIN_INSTRUCTORS ─────────────
        QSqlQuery aq;
        aq.prepare(
            "INSERT INTO admin_instructors (driving_school_id, full_name, email, password_hash, school_name) "
            "VALUES (?, ?, ?, ?, ?)");
        aq.addBindValue(newSchoolId);
        aq.addBindValue(adminNameEdit->text().trimmed());
        aq.addBindValue(adminEmailEdit->text().trimmed());
        aq.addBindValue(hashPwd(adminPwdEdit->text()));
        aq.addBindValue(schoolName);
        if (!aq.exec()) {
            QMessageBox::critical(dialog, "Database Error",
                "School created but failed to create admin instructor:\n" + aq.lastError().text());
            dialog->accept(); loadSchools(); return;
        }

        // Retrieve the auto-generated ID of the newly inserted admin instructor
        int adminInstructorId = 1; // Fallback
        QSqlQuery getIdQ;
        getIdQ.prepare("SELECT id FROM admin_instructors WHERE email = ?");
        getIdQ.addBindValue(adminEmailEdit->text().trimmed());
        if (getIdQ.exec() && getIdQ.next()) {
            adminInstructorId = getIdQ.value(0).toInt();
        }

        // ── Insert normal instructors into INSTRUCTORS ───────────
        int n = instrCountSpin->value();
        int insertedNormal = 0;
        for (int i = 0; i < n; ++i) {
            QSqlQuery nq;
            nq.prepare(
                "INSERT INTO INSTRUCTORS (admin_instructor_id, driving_school_id, full_name, email, phone, password_hash, instructor_status) "
                "VALUES (?, ?, ?, ?, ?, ?, 'ACTIVE')");
            nq.addBindValue(adminInstructorId);
            nq.addBindValue(newSchoolId);
            nq.addBindValue(instrPages[i].nameEdit->text().trimmed());
            nq.addBindValue(instrPages[i].emailEdit->text().trimmed());
            nq.addBindValue(instrPages[i].phoneEdit->text().trimmed());
            nq.addBindValue(hashPwd(instrPages[i].pwdEdit->text()));
            if (nq.exec()) {
                ++insertedNormal;
            }
        }

        QString msg = "Driving school created successfully!\nStatus: Active\n\nAdmin instructor account created.";
        if (n > 0)
            msg += QString("\n%1 normal instructor(s) added.").arg(insertedNormal);

        // ── Send email notifications via EmailJS ──────────────────
        EmailService &email = EmailService::instance();

        // Notify the admin instructor that their account is ready
        email.sendEmail(
            adminNameEdit->text().trimmed(),
            adminEmailEdit->text().trimmed(),
            "🏫 Your School Account is Ready — " + nameEdit->text().trimmed(),
            "Hello " + adminNameEdit->text().trimmed() + ",\n\n"
            "Your driving school \"" + nameEdit->text().trimmed() + "\" has been successfully "
            "registered on the Wino platform.\n\n"
            "Your admin instructor account has been created.\n"
            "You can now log in to manage students, vehicles, and sessions.\n\n"
            "— Wino Driving School Platform",
            nameEdit->text().trimmed()
        );

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

    // Step 1 → 2
    connect(next1Btn, &QPushButton::clicked, [=]() {
        if (nameEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(dialog, "Validation", "School name is required.");
            return;
        }
        stack->setCurrentIndex(1);
        updateProgress(1, totalSteps());
    });

    // Step 2 ← back
    connect(back2Btn, &QPushButton::clicked, [=]() {
        stack->setCurrentIndex(0);
        updateProgress(0, totalSteps());
    });

    // Step 2 → (either first instructor page or submit if N=0)
    connect(next2Btn, &QPushButton::clicked, [=, &instrPages]() {
        if (adminNameEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(dialog, "Validation", "Admin instructor full name is required.");
            return;
        }
        if (adminEmailEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(dialog, "Validation", "Admin instructor email is required.");
            return;
        }
        if (adminPwdEdit->text().length() < 6) {
            QMessageBox::warning(dialog, "Validation", "Admin password must be at least 6 characters.");
            return;
        }
        if (adminPwdEdit->text() != adminCfmEdit->text()) {
            QMessageBox::warning(dialog, "Validation", "Admin passwords do not match.");
            return;
        }
        int n = instrCountSpin->value();
        if (n == 0) {
            doSubmit();
        } else {
            stack->setCurrentIndex(2);
            updateProgress(2, totalSteps());
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
        connect(instrPages[i].nextBtn, &QPushButton::clicked, [=, &instrPages]() {
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
                int nextIdx = 2 + i + 1;
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
    MainWindow *loginWindow = new MainWindow();
    loginWindow->showMaximized();
    this->close();
}
