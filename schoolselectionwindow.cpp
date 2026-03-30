#include "schoolselectionwindow.h"
#include "ui_schoolselectionwindow.h"
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
#include <QFrame>
#include <QAction>
#include <QIcon>
#include <QTimer>
#include <QGraphicsDropShadowEffect>

SchoolSelectionWindow::SchoolSelectionWindow(int studentId, const QString &fullName, const QString &email, bool hasDrivingLicense, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::SchoolSelectionWindow),
    m_studentId(studentId),
    m_studentName(fullName),
    m_studentEmail(email),
    m_isNewDriver(!hasDrivingLicense),
    m_profilePreSet(false)
{
    ui->setupUi(this);
    setWindowTitle("Wino - Module 2 - School Selection");
    
    // Add search icon to search field
    QAction *searchAction = new QAction(ui->searchEdit);
    searchAction->setIcon(QIcon::fromTheme("edit-find"));
    ui->searchEdit->addAction(searchAction, QLineEdit::LeadingPosition);
    ui->searchEdit->setStyleSheet(ui->searchEdit->styleSheet() + " QLineEdit { padding-left: 35px; }");
    
    connect(ui->logoutButton, &QPushButton::clicked, this, &SchoolSelectionWindow::onLogoutClicked);
    connect(ui->searchEdit, &QLineEdit::textChanged, this, &SchoolSelectionWindow::onSearchChanged);

    // Ask driver profile first
    QTimer::singleShot(300, this, [this]() {
        askDriverProfile();
    });
}

SchoolSelectionWindow::~SchoolSelectionWindow()
{
    delete ui;
}

// ─── AI Profile Dialog ────────────────────────────────────────────────────────
void SchoolSelectionWindow::askDriverProfile()
{
    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle("🤖 AI Recommendation");
    dlg->setFixedSize(480, 320);
    dlg->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dlg->setStyleSheet("QDialog { background: white; border-radius: 20px; }");

    QVBoxLayout *lay = new QVBoxLayout(dlg);
    lay->setContentsMargins(36, 32, 36, 32);
    lay->setSpacing(18);

    // Header
    QLabel *icon = new QLabel("🤖");
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet("font-size: 40px;");
    lay->addWidget(icon);

    QLabel *title = new QLabel("AI School Recommendation");
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size: 18px; font-weight: 800; color: #111827;");
    lay->addWidget(title);

    QLabel *sub = new QLabel("Tell us your situation so we can recommend\nthe best driving school for you.");
    sub->setAlignment(Qt::AlignCenter);
    sub->setStyleSheet("font-size: 13px; color: #6B7280; line-height: 1.5;");
    lay->addWidget(sub);

    lay->addSpacing(6);

    // Buttons row
    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->setSpacing(14);

    // New driver button
    QPushButton *newBtn = new QPushButton("🚗  I'm a New Driver\n(No License)");
    newBtn->setFixedHeight(72);
    newBtn->setStyleSheet(
        "QPushButton { background:#F0FDF4; border:2px solid #22C55E; border-radius:14px;"
        "font-size:13px; font-weight:700; color:#15803D; padding:8px; }"
        "QPushButton:hover { background:#DCFCE7; border:2px solid #16A34A; }");

    // Has license button
    QPushButton *licBtn = new QPushButton("🏆  I Have a License\n(Upgrade Skills)");
    licBtn->setFixedHeight(72);
    licBtn->setStyleSheet(
        "QPushButton { background:#EFF6FF; border:2px solid #3B82F6; border-radius:14px;"
        "font-size:13px; font-weight:700; color:#1D4ED8; padding:8px; }"
        "QPushButton:hover { background:#DBEAFE; border:2px solid #2563EB; }");

    btnRow->addWidget(newBtn);
    btnRow->addWidget(licBtn);
    lay->addLayout(btnRow);

    // Shadow
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(dlg);
    shadow->setBlurRadius(30);
    shadow->setColor(QColor(0,0,0,60));
    shadow->setOffset(0, 8);
    dlg->setGraphicsEffect(shadow);

    connect(newBtn, &QPushButton::clicked, [=]() {
        m_isNewDriver = true;
        dlg->accept();
    });
    connect(licBtn, &QPushButton::clicked, [=]() {
        m_isNewDriver = false;
        dlg->accept();
    });

    dlg->exec();
    loadSchools();
}

void SchoolSelectionWindow::loadSchools()
{
    loadSchoolsFiltered(ui->searchEdit->text());
}

void SchoolSelectionWindow::loadSchoolsFiltered(const QString &search)
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

    // ── AI Recommendation Banner ─────────────────────────────────────────────
    QFrame *aiBanner = new QFrame();
    aiBanner->setStyleSheet(
        m_isNewDriver
        ? "QFrame{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
          "stop:0 #F0FDF4,stop:1 #DCFCE7);border-radius:12px;"
          "border:1.5px solid #22C55E;}"
        : "QFrame{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
          "stop:0 #EFF6FF,stop:1 #DBEAFE);border-radius:12px;"
          "border:1.5px solid #3B82F6;}");
    aiBanner->setMinimumHeight(56);

    QHBoxLayout *bannerLay = new QHBoxLayout(aiBanner);
    bannerLay->setContentsMargins(16, 10, 16, 10);

    QLabel *bannerIcon = new QLabel("🤖");
    bannerIcon->setStyleSheet("font-size:22px;");

    QString bannerText = m_isNewDriver
        ? "<b>AI Tip:</b> As a <b>new driver</b>, schools with more <b>Manual cars</b> appear first — "
          "manual training builds stronger fundamentals."
        : "<b>AI Tip:</b> Since you have a <b>license</b>, schools with more <b>Automatic cars</b> appear first — "
          "perfect for skill upgrades.";

    QLabel *bannerLbl = new QLabel(bannerText);
    bannerLbl->setWordWrap(true);
    bannerLbl->setStyleSheet(
        m_isNewDriver
        ? "font-size:12px;color:#15803D;background:transparent;border:none;"
        : "font-size:12px;color:#1D4ED8;background:transparent;border:none;");

    bannerLay->addWidget(bannerIcon);
    bannerLay->addWidget(bannerLbl, 1);
    mainLayout->addWidget(aiBanner);
    mainLayout->addSpacing(8);

    // ── Query: include manual/auto counts per school ──────────────────────────
    QString queryStr =
        "SELECT ds.id, ds.name, ds.students_count, ds.vehicles_count, ds.rating, "
        "  SUM(CASE WHEN UPPER(v.transmission) = 'MANUAL'    THEN 1 ELSE 0 END) AS manual_cnt, "
        "  SUM(CASE WHEN UPPER(v.transmission) = 'AUTOMATIC' THEN 1 ELSE 0 END) AS auto_cnt "
        "FROM DRIVING_SCHOOLS ds "
        "LEFT JOIN CARS v ON v.driving_school_id = ds.id "
        "WHERE ds.status = 'active' ";

    if (!search.trimmed().isEmpty())
        queryStr += "AND LOWER(ds.name) LIKE LOWER('%" + search.trimmed() + "%') ";

    queryStr +=
        "GROUP BY ds.id, ds.name, ds.students_count, ds.vehicles_count, ds.rating ";

    // Sort: recommended type count DESC first, then rating
    if (m_isNewDriver)
        queryStr += "ORDER BY manual_cnt DESC, ds.rating DESC";
    else
        queryStr += "ORDER BY auto_cnt DESC, ds.rating DESC";

    QSqlQuery query(queryStr);

    bool any = false;
    while (query.next()) {
        any = true;
        int id          = query.value(0).toInt();
        QString name    = query.value(1).toString();
        int students    = query.value(2).toInt();
        int vehicles    = query.value(3).toInt();
        double rating   = query.value(4).toDouble();
        int manualCnt   = query.value(5).toInt();
        int autoCnt     = query.value(6).toInt();

        QWidget *card = new QWidget();
        setupSchoolCard(card, id, name, students, vehicles, rating, manualCnt, autoCnt);
        mainLayout->addWidget(card);
    }

    if (!any) {
        QLabel *emptyLbl = new QLabel("No driving schools found.");
        emptyLbl->setAlignment(Qt::AlignCenter);
        emptyLbl->setStyleSheet("color:#9CA3AF;font-size:15px;padding:40px;");
        mainLayout->addWidget(emptyLbl);
    }

    mainLayout->addStretch();
}

void SchoolSelectionWindow::setupSchoolCard(QWidget *card, int schoolId, const QString &name,
                                        int students, int vehicles, double rating,
                                        int manualCount, int autoCount)
{
    // Determine if this school is AI-recommended
    bool isRecommended = m_isNewDriver ? (manualCount >= autoCount) : (autoCount >= manualCount);

    card->setObjectName(QString("studentCard_%1").arg(schoolId));
    QString borderColor = isRecommended
        ? (m_isNewDriver ? "#22C55E" : "#3B82F6")
        : "#E5E7EB";
    QString hoverBg    = isRecommended
        ? (m_isNewDriver ? "#F0FDF4" : "#EFF6FF")
        : "#F0FDFA";
    card->setStyleSheet(
        "QWidget#" + card->objectName() + " { background:white; border-radius:14px; border:1.5px solid " + borderColor + "; }"
        "QWidget#" + card->objectName() + ":hover { background:" + hoverBg + "; border:2px solid " + borderColor + "; }"
    );
    card->setMinimumHeight(90);

    QHBoxLayout *cardLayout = new QHBoxLayout(card);
    cardLayout->setContentsMargins(20, 15, 20, 15);
    cardLayout->setSpacing(20);

    QVBoxLayout *infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(6);

    // ── Top row: name + AI badge + rating ────────────────────────────────────
    QHBoxLayout *nameRatingLayout = new QHBoxLayout();
    QLabel *nameLabel = new QLabel(name);
    nameLabel->setStyleSheet("font-size:16px;font-weight:bold;color:#1F1827;");

    nameRatingLayout->addWidget(nameLabel);
    nameRatingLayout->addSpacing(8);

    // AI Recommended badge
    if (isRecommended) {
        QLabel *badge = new QLabel(m_isNewDriver ? "🤖 Manuel Recommandé" : "🤖 Automatique Recommandé");
        badge->setStyleSheet(
            m_isNewDriver
            ? "background:#DCFCE7;color:#15803D;border-radius:8px;padding:2px 8px;font-size:11px;font-weight:700;"
            : "background:#DBEAFE;color:#1D4ED8;border-radius:8px;padding:2px 8px;font-size:11px;font-weight:700;");
        nameRatingLayout->addWidget(badge);
    }

    nameRatingLayout->addStretch();

    QLabel *ratingIcon  = new QLabel("⭐");
    ratingIcon->setStyleSheet("font-size:13px;");
    QLabel *ratingLabel = new QLabel(QString::number(rating, 'f', 1));
    ratingLabel->setStyleSheet("font-size:13px;color:#F59E0B;font-weight:bold;");
    nameRatingLayout->addWidget(ratingIcon);
    nameRatingLayout->addWidget(ratingLabel);

    // ── Second row: location ─────────────────────────────────────────────────
    QHBoxLayout *locationLayout = new QHBoxLayout();
    QLabel *locationLabel = new QLabel("📍 Monastir, Monastir");
    locationLabel->setStyleSheet("font-size:12px;color:#6B7280;");
    locationLayout->addWidget(locationLabel);
    locationLayout->addStretch();

    // ── Third row: car type pills ─────────────────────────────────────────────
    QHBoxLayout *carsRow = new QHBoxLayout();
    carsRow->setSpacing(6);

    auto makePill = [](const QString &txt, const QString &bg, const QString &fg) {
        QLabel *l = new QLabel(txt);
        l->setStyleSheet(QString("background:%1;color:%2;border-radius:6px;"
                                 "padding:2px 8px;font-size:11px;font-weight:600;").arg(bg).arg(fg));
        return l;
    };
    if (manualCount > 0)
        carsRow->addWidget(makePill(QString("🔧 %1 Manual").arg(manualCount),
            m_isNewDriver && isRecommended ? "#DCFCE7" : "#F3F4F6",
            m_isNewDriver && isRecommended ? "#15803D" : "#374151"));
    if (autoCount > 0)
        carsRow->addWidget(makePill(QString("⚡ %1 Automatic").arg(autoCount),
            !m_isNewDriver && isRecommended ? "#DBEAFE" : "#F3F4F6",
            !m_isNewDriver && isRecommended ? "#1D4ED8" : "#374151"));
    carsRow->addStretch();

    infoLayout->addLayout(nameRatingLayout);
    infoLayout->addLayout(locationLayout);
    infoLayout->addLayout(carsRow);

    cardLayout->addLayout(infoLayout, 3);
    
    // Students count
    QHBoxLayout *studentsLayout = new QHBoxLayout();
    QLabel *studentsIcon = new QLabel("👥");
    studentsIcon->setStyleSheet("font-size: 14px;");
    QLabel *studentsLabel = new QLabel(QString("%1 students").arg(students));
    studentsLabel->setStyleSheet("font-size: 13px; color: #6B7280;");
    studentsLayout->addWidget(studentsIcon);
    studentsLayout->addWidget(studentsLabel);
    studentsLayout->setSpacing(5);
    
    QWidget *studentsWidget = new QWidget();
    studentsWidget->setLayout(studentsLayout);
    studentsWidget->setStyleSheet("background: transparent;");
    studentsWidget->setMinimumWidth(120);
    cardLayout->addWidget(studentsWidget);
    
    // Vehicles count
    QHBoxLayout *vehiclesLayout = new QHBoxLayout();
    QLabel *vehiclesIcon = new QLabel("🚗");
    vehiclesIcon->setStyleSheet("font-size: 14px;");
    QLabel *vehiclesLabel = new QLabel(QString("%1 vehicles").arg(vehicles));
    vehiclesLabel->setStyleSheet("font-size: 13px; color: #6B7280;");
    vehiclesLayout->addWidget(vehiclesIcon);
    vehiclesLayout->addWidget(vehiclesLabel);
    vehiclesLayout->setSpacing(5);
    
    QWidget *vehiclesWidget = new QWidget();
    vehiclesWidget->setLayout(vehiclesLayout);
    vehiclesWidget->setStyleSheet("background: transparent;");
    vehiclesWidget->setMinimumWidth(120);
    cardLayout->addWidget(vehiclesWidget);
    
    // View Details button
    QPushButton *viewBtn = new QPushButton("View Details");
    viewBtn->setStyleSheet("background-color: #14B8A6; color: white; border: none; border-radius: 5px; padding: 10px 20px; font-size: 13px; font-weight: bold;");
    viewBtn->setMinimumWidth(120);
    viewBtn->setMinimumHeight(40);
    connect(viewBtn, &QPushButton::clicked, [this, schoolId]() { onViewDetailsClicked(schoolId); });
    cardLayout->addWidget(viewBtn);
}

void SchoolSelectionWindow::onSearchChanged(const QString &text)
{
    Q_UNUSED(text);
    loadSchoolsFiltered(ui->searchEdit->text());
}


void SchoolSelectionWindow::onViewDetailsClicked(int schoolId)
{
    QSqlQuery query;
    query.prepare("SELECT name, rating FROM DRIVING_SCHOOLS WHERE id = ?");
    query.addBindValue(schoolId);
    if (!query.exec() || !query.next()) return;

    QString name     = query.value(0).toString();
    double  rating   = query.value(1).toDouble();

    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle(name);
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
    root->setSpacing(16);

    // ── Header: name + rating ────────────────────────────────────────────
    QHBoxLayout *headerRow = new QHBoxLayout();
    QLabel *titleLbl = new QLabel(name);
    titleLbl->setStyleSheet("font-size:20px;font-weight:bold;color:#1F1827;");
    headerRow->addWidget(titleLbl, 1);

    QLabel *ratingLbl = new QLabel(QString("⭐ %1 / 5").arg(QString::number(rating, 'f', 1)));
    ratingLbl->setStyleSheet("font-size:14px;font-weight:bold;color:#F59E0B;");
    headerRow->addWidget(ratingLbl);
    root->addLayout(headerRow);

    QLabel *locationLbl = new QLabel("📍 Monastir, Monastir");
    locationLbl->setStyleSheet("color:#6B7280;font-size:13px;");
    root->addWidget(locationLbl);

    auto makeSep = []() -> QFrame* {
        QFrame *f = new QFrame();
        f->setFrameShape(QFrame::HLine);
        f->setStyleSheet("background:#E5E7EB;");
        f->setFixedHeight(1);
        return f;
    };
    root->addWidget(makeSep());

    // ── AI PROFILE BANNER — auto-detected from registration data ─────────
    bool isNewDriver = m_isNewDriver;
    QFrame *aiFrame = new QFrame();
    aiFrame->setStyleSheet(
        "QFrame { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "stop:0 #0F766E, stop:1 #0891B2); border-radius: 12px; }");
    aiFrame->setFixedHeight(64);
    QHBoxLayout *aiLayout = new QHBoxLayout(aiFrame);
    aiLayout->setContentsMargins(16, 10, 16, 10);
    aiLayout->setSpacing(12);
    QLabel *aiIcon = new QLabel("🤖");
    aiIcon->setStyleSheet("font-size:22px; background:transparent;");
    QVBoxLayout *aiText = new QVBoxLayout();
    aiText->setSpacing(2);
    QLabel *aiTitle2 = new QLabel("AI Car Recommendation");
    aiTitle2->setStyleSheet("font-size:13px; font-weight:700; color:white; background:transparent;");
    QLabel *aiSub = new QLabel(isNewDriver
        ? "Profile from your account: New Driver — Manual cars first"
        : "Profile from your account: Has License — Automatic cars first");
    aiSub->setStyleSheet("font-size:11px; color:rgba(255,255,255,0.85); background:transparent;");
    aiText->addWidget(aiTitle2);
    aiText->addWidget(aiSub);
    aiLayout->addWidget(aiIcon);
    aiLayout->addLayout(aiText, 1);
    // Profile badge (read-only, no toggle)
    QLabel *profileBadge = new QLabel(isNewDriver ? "🆕 New Driver" : "🪪 Has License");
    profileBadge->setStyleSheet(
        "background:rgba(255,255,255,0.25); color:white; border-radius:8px;"
        "padding:5px 12px; font-size:12px; font-weight:600;");
    aiLayout->addWidget(profileBadge);
    root->addWidget(aiFrame);

    // ── AI Tip label ──────────────────────────────────────────────────────
    QLabel *aiTipLbl = new QLabel(isNewDriver
        ? "💡 Showing  Manual  cars first — better for learning from scratch"
        : "💡 Showing  Automatic  cars first — smoother upgrade for licensed drivers");
    aiTipLbl->setStyleSheet(isNewDriver
        ? "background:#F0FDFA;border:1px solid #99F6E4;border-radius:8px;"
          "color:#0F766E;font-size:12px;font-weight:500;padding:7px 12px;"
        : "background:#EFF6FF;border:1px solid #BFDBFE;border-radius:8px;"
          "color:#1D4ED8;font-size:12px;font-weight:500;padding:7px 12px;");
    aiTipLbl->setWordWrap(true);
    root->addWidget(aiTipLbl);

    // ── VEHICLES ──────────────────────────────────────────────────────────
    QLabel *vTitle = new QLabel("🚗  Vehicles");
    vTitle->setStyleSheet("font-size:15px;font-weight:bold;color:#1F1827;");
    root->addWidget(vTitle);

    QScrollArea *vScroll = new QScrollArea();
    vScroll->setWidgetResizable(true);
    vScroll->setMaximumHeight(220);
    QWidget *vContents = new QWidget();
    vContents->setObjectName("scrollContents");
    QVBoxLayout *vLayout = new QVBoxLayout(vContents);
    vLayout->setSpacing(8);
    vLayout->setContentsMargins(0, 0, 4, 0);

    struct VehicleRow { QString brand, model, plate, trans, vstatus; int year; };
    QList<VehicleRow> allVehicles;
    {
        QSqlQuery vq;
        vq.prepare("SELECT brand, model, year, plate_number, transmission, status "
                   "FROM CARS WHERE driving_school_id = ? ORDER BY id");
        vq.addBindValue(schoolId); vq.exec();
        while (vq.next()) {
            allVehicles.append({ vq.value(0).toString(), vq.value(1).toString(),
                                 vq.value(3).toString(), vq.value(4).toString(),
                                 vq.value(5).toString(), vq.value(2).toInt() });
        }
    }

    // Sort: recommended type first, based on registration profile
    QString recommendedTrans = isNewDriver ? "Manual" : "Automatic";
    QList<VehicleRow> sortedVehicles;
    for (auto &v : allVehicles)
        if (v.trans.compare(recommendedTrans, Qt::CaseInsensitive) == 0) sortedVehicles.prepend(v);
        else sortedVehicles.append(v);

    if (sortedVehicles.isEmpty()) {
        QLabel *noVeh = new QLabel("No vehicles registered for this school.");
        noVeh->setStyleSheet("color:#9CA3AF;font-size:13px;");
        noVeh->setAlignment(Qt::AlignCenter);
        vLayout->addWidget(noVeh);
    }
    for (auto &vr : sortedVehicles) {
        bool isRec = vr.trans.compare(recommendedTrans, Qt::CaseInsensitive) == 0;
        QWidget *vCard = new QWidget();
        vCard->setStyleSheet(isRec
            ? "QWidget{background:#F0FDFA;border:2px solid #14B8A6;border-radius:10px;}"
            : "QWidget{background:#F9FAFB;border:1px solid #E5E7EB;border-radius:10px;}");
        QHBoxLayout *vRow = new QHBoxLayout(vCard);
        vRow->setContentsMargins(12, 10, 12, 10); vRow->setSpacing(12);
        QLabel *carIcon = new QLabel(vr.trans.compare("Automatic", Qt::CaseInsensitive) == 0 ? "🤖" : "⚙️");
        carIcon->setFixedSize(48, 48); carIcon->setAlignment(Qt::AlignCenter);
        carIcon->setStyleSheet(isRec ? "background:#CCFBF1;border-radius:8px;font-size:26px;"
                                     : "background:#E5E7EB;border-radius:8px;font-size:26px;");
        vRow->addWidget(carIcon);
        QVBoxLayout *vInfo = new QVBoxLayout(); vInfo->setSpacing(3);
        QHBoxLayout *nameRow = new QHBoxLayout(); nameRow->setSpacing(8);
        QLabel *carName = new QLabel(QString("%1 %2 (%3)").arg(vr.brand, vr.model).arg(vr.year));
        carName->setStyleSheet("font-weight:700;font-size:13px;color:#1F1827;");
        nameRow->addWidget(carName);
        if (isRec) {
            QLabel *aiBadge = new QLabel("🤖 AI Pick");
            aiBadge->setStyleSheet("background:#14B8A6;color:white;border-radius:6px;"
                                   "font-size:10px;font-weight:700;padding:2px 7px;");
            nameRow->addWidget(aiBadge);
        }
        nameRow->addStretch();
        vInfo->addLayout(nameRow);
        QLabel *carDetail = new QLabel(QString("🪪 %1   ⚙️ %2").arg(vr.plate, vr.trans));
        carDetail->setStyleSheet("font-size:12px;color:#6B7280;");
        vInfo->addWidget(carDetail);
        vRow->addLayout(vInfo, 1);
        QLabel *vsBadge = new QLabel(vr.vstatus == "active" ? "Available" : "Maintenance");
        vsBadge->setAlignment(Qt::AlignCenter); vsBadge->setFixedSize(90, 24);
        vsBadge->setStyleSheet(vr.vstatus == "active"
            ? "background:#D1FAE5;color:#065F46;border-radius:5px;font-size:11px;font-weight:bold;"
            : "background:#FEF3C7;color:#92400E;border-radius:5px;font-size:11px;font-weight:bold;");
        vRow->addWidget(vsBadge);
        vLayout->addWidget(vCard);
    }
    vLayout->addStretch();
    vScroll->setWidget(vContents);
    root->addWidget(vScroll);

    root->addWidget(makeSep());

    // ── INSTRUCTORS ───────────────────────────────────────────────────────
    struct InstrRow { int id; QString name; bool avail; };
    QList<InstrRow> instrRows;
    {
        // BUG FIX: Changed from ADMIN_INSTRUCTORS to INSTRUCTORS to prevent ORA-02291 constraint error
        QSqlQuery lq;
        lq.prepare("SELECT id, full_name FROM INSTRUCTORS "
                   "WHERE driving_school_id = ? ORDER BY full_name");
        lq.addBindValue(schoolId); lq.exec();
        while (lq.next())
            instrRows.append({ lq.value(0).toInt(), lq.value(1).toString(), true /*always available*/ });
    }
    int instrTotal = instrRows.size();
    int instrAvail = 0;
    for (auto &r : instrRows) if (r.avail) instrAvail++;

    QLabel *iTitle = new QLabel(
        QString("👨‍🏫  Instructors  (%1 total, %2 available)").arg(instrTotal).arg(instrAvail));
    iTitle->setStyleSheet("font-size:15px;font-weight:bold;color:#1F1827;");
    root->addWidget(iTitle);

    QScrollArea *iScroll = new QScrollArea();
    iScroll->setWidgetResizable(true);
    iScroll->setMaximumHeight(220);
    QWidget *iContents = new QWidget();
    iContents->setObjectName("scrollContents");
    QVBoxLayout *iLayout = new QVBoxLayout(iContents);
    iLayout->setSpacing(10);
    iLayout->setContentsMargins(0, 0, 4, 0);

    auto *selectedInstructorId   = new int(0);
    auto *selectedInstructorCard = new QWidget*(nullptr);

    auto normalCardStyle = [](QWidget *c) {
        c->setStyleSheet("QWidget{background:#F9FAFB;border:1.5px solid #E5E7EB;border-radius:10px;}");
    };
    auto selectedCardStyle = [](QWidget *c) {
        c->setStyleSheet("QWidget{background:#F0FDFA;border:2px solid #14B8A6;border-radius:10px;}");
    };

    // Auto-select if only one instructor
    if (instrRows.size() == 1)
        *selectedInstructorId = instrRows[0].id;

    if (instrRows.isEmpty()) {
        QLabel *noInstr = new QLabel("No instructors listed for this school.");
        noInstr->setStyleSheet("color:#9CA3AF;font-size:13px;");
        noInstr->setAlignment(Qt::AlignCenter);
        iLayout->addWidget(noInstr);
    } else {
        if (instrRows.size() > 1) {
            QLabel *pickHint = new QLabel("👆  Select an instructor to send your request to:");
            pickHint->setStyleSheet(
                "color:#0F766E;font-size:12px;font-weight:600;"
                "background:#F0FDFA;border:1px solid #99F6E4;border-radius:6px;padding:6px 10px;");
            iLayout->addWidget(pickHint);
        }

        // Local event-filter class for card click detection
        struct CardClickFilter : public QObject {
            std::function<void()> cb;
            CardClickFilter(QObject *p, std::function<void()> f) : QObject(p), cb(std::move(f)) {}
            bool eventFilter(QObject *, QEvent *e) override {
                if (e->type() == QEvent::MouseButtonRelease) { cb(); return false; }
                return false;
            }
        };

        for (const InstrRow &row : instrRows) {
            QWidget *iCard = new QWidget();
            iCard->setCursor(Qt::PointingHandCursor);

            bool autoSelected = (instrRows.size() == 1);
            if (autoSelected) {
                selectedCardStyle(iCard);
                *selectedInstructorCard = iCard;
            } else {
                normalCardStyle(iCard);
            }

            QHBoxLayout *iRow = new QHBoxLayout(iCard);
            iRow->setContentsMargins(14, 10, 14, 10);
            iRow->setSpacing(12);

            // Avatar circle with initial
            QLabel *avatar = new QLabel(row.name.isEmpty() ? "?" : QString(row.name[0].toUpper()));
            avatar->setFixedSize(42, 42);
            avatar->setAlignment(Qt::AlignCenter);
            avatar->setStyleSheet("background:#14B8A6;color:white;border-radius:21px;"
                                  "font-size:17px;font-weight:bold;");
            iRow->addWidget(avatar);

            // Name + role
            QVBoxLayout *iInfo = new QVBoxLayout();
            iInfo->setSpacing(2);
            QLabel *nameLbl = new QLabel(row.name);
            nameLbl->setStyleSheet("font-size:13px;font-weight:bold;color:#1F1827;");
            QLabel *roleLbl = new QLabel("Instructor");
            roleLbl->setStyleSheet("font-size:11px;color:#9CA3AF;");
            iInfo->addWidget(nameLbl);
            iInfo->addWidget(roleLbl);
            iRow->addLayout(iInfo, 1);

            // Available badge
            QLabel *availBadge = new QLabel(row.avail ? "Available" : "Unavailable");
            availBadge->setAlignment(Qt::AlignCenter);
            availBadge->setFixedSize(90, 26);
            availBadge->setStyleSheet(row.avail
                ? "background:#D1FAE5;color:#065F46;border-radius:6px;font-size:11px;font-weight:bold;"
                : "background:#E5E7EB;color:#6B7280;border-radius:6px;font-size:11px;font-weight:bold;");
            iRow->addWidget(availBadge);

            // Selection checkmark indicator
            QLabel *checkLbl = new QLabel("✓");
            checkLbl->setFixedSize(26, 26);
            checkLbl->setAlignment(Qt::AlignCenter);
            checkLbl->setStyleSheet("background:#14B8A6;color:white;border-radius:13px;"
                                    "font-size:13px;font-weight:bold;");
            checkLbl->setVisible(autoSelected);
            iRow->addWidget(checkLbl);

            iLayout->addWidget(iCard);

            // Install click handler via event filter (works reliably on all widgets)
            int iid = row.id;
            auto *cf = new CardClickFilter(iCard, [=]() mutable {
                if (*selectedInstructorCard && *selectedInstructorCard != iCard) {
                    normalCardStyle(*selectedInstructorCard);
                    // Hide previous card's checkmark
                    for (auto *lbl : (*selectedInstructorCard)->findChildren<QLabel*>())
                        if (lbl->text() == "\u2713") lbl->setVisible(false);
                }
                selectedCardStyle(iCard);
                checkLbl->setVisible(true);
                *selectedInstructorId   = iid;
                *selectedInstructorCard = iCard;
            });
            iCard->installEventFilter(cf);
        }
    }

    iLayout->addStretch();
    iScroll->setWidget(iContents);
    root->addWidget(iScroll);

    // ── Register button ───────────────────────────────────────────────────
    QPushButton *registerBtn = new QPushButton("\U0001F4E7  Send Registration Request");
    registerBtn->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "stop:0 #0F766E, stop:1 #14B8A6); color:white; border:none;"
        "border-radius:10px; padding:13px; font-size:14px; font-weight:bold; }"
        "QPushButton:hover { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "stop:0 #0D6560, stop:1 #0F9488); }"
        "QPushButton:disabled { background:#9CA3AF; }");
    registerBtn->setMinimumHeight(52);

    int numInstructors = instrRows.size();
    connect(registerBtn, &QPushButton::clicked, [=]() {
        // If school has multiple instructors, one must be selected
        if (numInstructors > 1 && *selectedInstructorId == 0) {
            QMessageBox::warning(dialog, "Select Instructor",
                "Please select an instructor before sending your request.");
            return;
        }

        // ── Save request to DB ───────────────────────────────────────────
        QSqlQuery rq;
        if (*selectedInstructorId > 0) {
            // FIX: Set driving_school_id and instructor_id explicitly, and keep student_status as PENDING
            rq.prepare("UPDATE STUDENTS SET driving_school_id = ?, instructor_id = ?, "
                       "student_status = 'PENDING' WHERE id = ?");
            rq.addBindValue(schoolId);
            rq.addBindValue(*selectedInstructorId);
            rq.addBindValue(m_studentId);
        } else {
            // Should not reach here ordinarily due to MIN() assigned early, but safety net
            rq.prepare("UPDATE STUDENTS SET driving_school_id = ?, "
                       "student_status = 'PENDING' WHERE id = ?");
            rq.addBindValue(schoolId);
            rq.addBindValue(m_studentId);
        }
        
        if (!rq.exec()) {
            QMessageBox::critical(dialog, "Error",
                "Could not save your request:\n" + rq.lastError().text());
            return;
        }

        // ── Send confirmation email (best-effort) ────────────────────────
        QSqlQuery eq;
        eq.prepare("SELECT email FROM DRIVING_SCHOOLS WHERE id = ?");
        eq.addBindValue(schoolId);
        QString schoolEmail;
        if (eq.exec() && eq.next()) schoolEmail = eq.value(0).toString();

        registerBtn->setText("\U0001F4E4  Sending...");
        registerBtn->setEnabled(false);

        EmailService &email = EmailService::instance();
        QObject::connect(&email, &EmailService::emailSent, dialog,
            [=](const QString &) {
                registerBtn->setText("\u2705  Request Sent!");
                registerBtn->setStyleSheet(
                    "background:#059669;color:white;border:none;"
                    "border-radius:10px;padding:13px;font-size:14px;font-weight:bold;");
                QTimer::singleShot(1800, [=]() { 
                    dialog->accept(); 
                    emit schoolSelected(); 
                    this->close();
                });
            });
        QObject::connect(&email, &EmailService::emailFailed, dialog,
            [=](const QString &) {
                // DB already updated — still succeed even if email fails
                registerBtn->setText("\u2705  Request Sent!");
                registerBtn->setEnabled(false);
                QTimer::singleShot(1800, [=]() { 
                    dialog->accept(); 
                    emit schoolSelected(); 
                    this->close();
                });
            });

        email.sendRegistrationRequest(
            m_studentName,
            m_studentEmail,
            name,
            schoolEmail.isEmpty() ? "school@wino.tn" : schoolEmail
        );
    });
    root->addWidget(registerBtn);

    dialog->exec();
}

void SchoolSelectionWindow::onLogoutClicked()
{
    // Need to navigate cleanly back to main window without include loops
    // In our flow, we just close and tell parent to show itself.
    this->close();
    QWidget* parentWindow = parentWidget();
    if (parentWindow) parentWindow->show();
}
