#include "studentdashboard.h"
#include "ui_studentdashboard.h"
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
#include <QFrame>
#include <QAction>
#include <QIcon>
#include <QTimer>
#include <QGraphicsDropShadowEffect>
#include <QPainter>
#include <QPainterPath>
#include <QFile>
#include <QPixmap>

StudentDashboard::StudentDashboard(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::StudentDashboard)
{
    ui->setupUi(this);
    setWindowTitle("Wino - Module 2 - Student Space");
    
    // Add search icon to search field
    QAction *searchAction = new QAction(ui->searchEdit);
    searchAction->setIcon(QIcon::fromTheme("edit-find"));
    ui->searchEdit->addAction(searchAction, QLineEdit::LeadingPosition);
    ui->searchEdit->setStyleSheet(ui->searchEdit->styleSheet() + " QLineEdit { padding-left: 35px; }");
    
    connect(ui->logoutButton, &QPushButton::clicked, this, &StudentDashboard::onLogoutClicked);
    connect(ui->searchEdit, &QLineEdit::textChanged, this, &StudentDashboard::onSearchChanged);

    // Ask driver profile first (skipped if already set from login)
    QTimer::singleShot(300, this, [this]() {
        if (m_profilePreSet)
            loadSchools();
        else
            askDriverProfile();
    });
}

StudentDashboard::~StudentDashboard()
{
    delete ui;
}

void StudentDashboard::setStudentInfo(const QString &fullName, const QString &email, bool hasDrivingLicense)
{
    m_studentName   = fullName;
    m_studentEmail  = email;
    m_isNewDriver   = !hasDrivingLicense;
    m_profilePreSet = true;
}

// ─── AI Profile Dialog ────────────────────────────────────────────────────────
void StudentDashboard::askDriverProfile()
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

void StudentDashboard::loadSchools()
{
    loadSchoolsFiltered(ui->searchEdit->text());
}

void StudentDashboard::loadSchoolsFiltered(const QString &search)
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
        "  SUM(CASE WHEN UPPER(v.transmission) = 'AUTOMATIC' THEN 1 ELSE 0 END) AS auto_cnt, "
        "  MAX(ds.logo_path) AS logo_path "
        "FROM driving_schools ds "
        "LEFT JOIN cars v ON v.driving_school_id = ds.id "
        "WHERE ds.status = 'active' ";

    if (!search.trimmed().isEmpty())
        queryStr += "AND ds.name LIKE '%" + search.trimmed() + "%' ";

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
        QString logoPath = query.value(7).toString();

        QWidget *card = new QWidget();
        setupSchoolCard(card, id, name, students, vehicles, rating, manualCnt, autoCnt, logoPath);
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

void StudentDashboard::setupSchoolCard(QWidget *card, int schoolId, const QString &name,
                                        int students, int vehicles, double rating,
                                        int manualCount, int autoCount,
                                        const QString &logoPath)
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
    cardLayout->setSpacing(16);

    // ── School logo (read-only circular avatar) ───────────────────────────────
    const int LOGO_SIZE = 54;
    QLabel *logoLabel = new QLabel(card);
    logoLabel->setFixedSize(LOGO_SIZE, LOGO_SIZE);
    {
        QPixmap pix;
        bool loaded = !logoPath.isEmpty() && QFile::exists(logoPath) && pix.load(logoPath);
        if (loaded) {
            // Circular crop
            QPixmap scaled = pix.scaled(LOGO_SIZE, LOGO_SIZE,
                Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            QPixmap result(LOGO_SIZE, LOGO_SIZE);
            result.fill(Qt::transparent);
            QPainter painter(&result);
            painter.setRenderHint(QPainter::Antialiasing);
            QPainterPath path;
            path.addEllipse(0, 0, LOGO_SIZE, LOGO_SIZE);
            painter.setClipPath(path);
            int xOff = (scaled.width()  - LOGO_SIZE) / 2;
            int yOff = (scaled.height() - LOGO_SIZE) / 2;
            painter.drawPixmap(-xOff, -yOff, scaled);
            logoLabel->setPixmap(result);
            logoLabel->setStyleSheet("border-radius:27px;border:2px solid #14B8A6;");
        } else {
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
            logoLabel->setPixmap(ph);
            logoLabel->setStyleSheet("border-radius:27px;border:2px dashed #14B8A6;");
        }
    }
    cardLayout->addWidget(logoLabel);

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
        QLabel *badge = new QLabel(m_isNewDriver ? "🤖 Manual Recommended" : "🤖 Automatic Recommended");
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

void StudentDashboard::onSearchChanged(const QString &text)
{
    Q_UNUSED(text);
    loadSchoolsFiltered(ui->searchEdit->text());
}


void StudentDashboard::onViewDetailsClicked(int schoolId)
{
    QSqlQuery query(QSqlDatabase::database());
    query.prepare("SELECT name, rating FROM driving_schools WHERE id = ?");
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

    QFrame *vCard = new QFrame();
    vCard->setStyleSheet("QFrame{background:#F9FAFB;border:1.5px solid #E5E7EB;border-radius:12px;}");
    QVBoxLayout *vLayout = new QVBoxLayout(vCard);
    vLayout->setSpacing(8);
    vLayout->setContentsMargins(12, 10, 12, 10);

    struct VehicleRow { QString brand, model, plate, trans, vstatus, imagePath; int year; };
    QList<VehicleRow> allVehicles;
    {
        QSqlQuery vq(QSqlDatabase::database());
        // Note: CARS table uses is_active (1/0), not a status VARCHAR column
        vq.prepare("SELECT brand, model, year, plate_number, transmission, "
                   "CASE WHEN is_active=1 THEN 'active' ELSE 'inactive' END, "
                   "image_path "
                   "FROM cars WHERE driving_school_id = ? ORDER BY id");
        vq.addBindValue(schoolId); vq.exec();
        while (vq.next()) {
            allVehicles.append({ vq.value(0).toString(), vq.value(1).toString(),
                                 vq.value(3).toString(), vq.value(4).toString(),
                                 vq.value(5).toString(), vq.value(6).toString(),
                                 vq.value(2).toInt() });
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
        vCard->setStyleSheet("QWidget{background:transparent;border:none;}");
        QHBoxLayout *vRow = new QHBoxLayout(vCard);
        vRow->setContentsMargins(12, 10, 12, 10); vRow->setSpacing(12);
        QLabel *carIcon = new QLabel();
        carIcon->setFixedSize(64, 48);
        carIcon->setAlignment(Qt::AlignCenter);
        {
            QPixmap cpix;
            bool imgLoaded = !vr.imagePath.isEmpty()
                             && QFile::exists(vr.imagePath)
                             && cpix.load(vr.imagePath);
            if (imgLoaded) {
                carIcon->setPixmap(cpix.scaled(64, 48, Qt::KeepAspectRatio,
                                               Qt::SmoothTransformation));
                carIcon->setStyleSheet("border-radius:8px;border:1px solid #E5E7EB;");
            } else {
                carIcon->setText(vr.trans.compare("Automatic", Qt::CaseInsensitive) == 0
                                 ? "🤖" : "🚗");
                carIcon->setStyleSheet(isRec
                    ? "background:#CCFBF1;border-radius:8px;font-size:26px;"
                    : "background:#E5E7EB;border-radius:8px;font-size:26px;");
            }
        }
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
    root->addWidget(vCard);

    root->addWidget(makeSep());

    // ── INSTRUCTORS ───────────────────────────────────────────────────────
    // Both count and list queries use the same filter (role != 'admin')
    struct InstrRow { int id; QString name; QString photoPath; bool avail; };
    QList<InstrRow> instrRows;
    {
        // Instructors stored in INSTRUCTORS table
        QSqlQuery lq(QSqlDatabase::database());
        lq.prepare("SELECT id, full_name, photo_path FROM instructors "
                   "WHERE school_id = ? ORDER BY full_name");
        lq.addBindValue(schoolId); lq.exec();
        while (lq.next())
            instrRows.append({ lq.value(0).toInt(), lq.value(1).toString(),
                               lq.value(2).toString(), true /*always available*/ });
    }
    int instrTotal = instrRows.size();
    int instrAvail = 0;
    for (auto &r : instrRows) if (r.avail) instrAvail++;

    QLabel *iTitle = new QLabel(
        QString("👨‍🏫  Instructors  (%1 total, %2 available)").arg(instrTotal).arg(instrAvail));
    iTitle->setStyleSheet("font-size:15px;font-weight:bold;color:#1F1827;");
    root->addWidget(iTitle);

    QFrame *iCard = new QFrame();
    iCard->setStyleSheet("QFrame{background:#F9FAFB;border:1.5px solid #E5E7EB;border-radius:12px;}");
    QVBoxLayout *iLayout = new QVBoxLayout(iCard);
    iLayout->setSpacing(10);
    iLayout->setContentsMargins(12, 10, 12, 10);

    auto *selectedInstructorId   = new int(0);
    auto *selectedInstructorCard = new QWidget*(nullptr);

    auto normalCardStyle = [](QWidget *c) {
        c->setStyleSheet("QWidget{background:transparent;border:none;}");
    };
    auto selectedCardStyle = [](QWidget *c) {
        c->setStyleSheet("QWidget{background:transparent;border:none;}");
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

            // Avatar: show photo if available, else teal initial
            QLabel *avatar = new QLabel();
            avatar->setFixedSize(46, 46);
            avatar->setAlignment(Qt::AlignCenter);
            {
                QPixmap src;
                if (!row.photoPath.isEmpty() && QFile::exists(row.photoPath) && src.load(row.photoPath)) {
                    // Circular crop
                    QPixmap scaled = src.scaled(46, 46, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
                    QPixmap result(46, 46); result.fill(Qt::transparent);
                    QPainter pp(&result); pp.setRenderHint(QPainter::Antialiasing);
                    QPainterPath path; path.addEllipse(0, 0, 46, 46);
                    pp.setClipPath(path);
                    int xOff = (scaled.width()-46)/2, yOff = (scaled.height()-46)/2;
                    pp.drawPixmap(-xOff, -yOff, scaled);
                    avatar->setPixmap(result);
                    avatar->setStyleSheet("border-radius:23px; border:2px solid #14B8A6;");
                } else {
                    avatar->setText(row.name.isEmpty() ? "?" : QString(row.name[0].toUpper()));
                    avatar->setStyleSheet("background:#14B8A6;color:white;border-radius:23px;"
                                         "font-size:17px;font-weight:bold;");
                }
            }
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

    root->addWidget(iCard);

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
        // Also stamp school_id and name so the instructor query can find this row
        QSqlQuery rq;
        if (*selectedInstructorId > 0) {
            rq.prepare("UPDATE students SET school_id = ?, instructor_id = ?, "
                       "name = ?, status = 'pending', requested_date = SYSDATE "
                       "WHERE id = ?");
            rq.addBindValue(schoolId);
            rq.addBindValue(*selectedInstructorId);
            rq.addBindValue(m_studentName);
            rq.addBindValue(m_studentId);
        } else {
            rq.prepare("UPDATE students SET school_id = ?, "
                       "name = ?, status = 'pending', requested_date = SYSDATE "
                       "WHERE id = ?");
            rq.addBindValue(schoolId);
            rq.addBindValue(m_studentName);
            rq.addBindValue(m_studentId);
        }
        if (!rq.exec()) {
            QMessageBox::critical(dialog, "Error",
                "Could not save your request:\n" + rq.lastError().text());
            return;
        }

        // ── Send confirmation email (best-effort) ────────────────────────
        QSqlQuery eq;
        eq.prepare("SELECT email FROM driving_schools WHERE id = ?");
        eq.addBindValue(schoolId);
        QString schoolEmail;
        if (eq.exec() && eq.next()) schoolEmail = eq.value(0).toString();

        registerBtn->setText("\U0001F4E4  Sending...");
        registerBtn->setEnabled(false);

        EmailService &email = EmailService::instance();
        QObject::connect(&email, &EmailService::emailSent, dialog,
            [registerBtn, dialog](const QString &) {
                registerBtn->setText("\u2705  Request Sent!");
                registerBtn->setStyleSheet(
                    "background:#059669;color:white;border:none;"
                    "border-radius:10px;padding:13px;font-size:14px;font-weight:bold;");
                QTimer::singleShot(1800, dialog, &QDialog::accept);
            });
        QObject::connect(&email, &EmailService::emailFailed, dialog,
            [registerBtn, dialog](const QString &) {
                // DB already updated — still succeed even if email fails
                registerBtn->setText("\u2705  Request Sent!");
                registerBtn->setEnabled(false);
                QTimer::singleShot(1800, dialog, &QDialog::accept);
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

void StudentDashboard::onLogoutClicked()
{
    qApp->setStyleSheet("");

    MainWindow *loginWindow = new MainWindow();
    loginWindow->show();
    loginWindow->showMaximized();
    this->hide();
    this->deleteLater();
}
