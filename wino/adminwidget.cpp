#include "adminwidget.h"
#include "parkingdbmanager.h"
#include <QGraphicsDropShadowEffect>
#include <QHeaderView>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QDateTime>
#include <QPainter>
#include <QScrollBar>

// ═══════════════════════════════════════════════════════════════
//  MODERN ADMIN DESIGN SYSTEM
// ═══════════════════════════════════════════════════════════════

static const QString BG      = "#f0f2f5";
static const QString CARD    = "#ffffff";
static const QString TXT     = "#2d3436";
static const QString DIM     = "#636e72";
static const QString BORDER  = "#e8e8e8";
static const QString GREEN   = "#00b894";
static const QString BLUE    = "#0984e3";
static const QString PURPLE  = "#6c5ce7";
static const QString RED     = "#d63031";
static const QString ORANGE  = "#e17055";
static const QString YELLOW  = "#fdcb6e";

static const QString SCROLL_SS =
    "QScrollArea{border:none;background:%1;}"
    "QScrollBar:vertical{width:6px;background:transparent;}"
    "QScrollBar::handle:vertical{background:#ccc;border-radius:3px;min-height:30px;}"
    "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}";

static QString tableCSS() {
    return QString(
        "QTableWidget{background:white;border:1px solid %1;border-radius:12px;"
        "gridline-color:transparent;font-size:12px;color:%2;"
        "alternate-background-color:#f8fafb;selection-background-color:#e8f8f5;selection-color:%2;}"
        "QTableWidget::item{padding:10px 14px;border-bottom:1px solid #f0f2f5;color:%2;}"
        "QTableWidget::item:selected{background:#e8f8f5;color:%2;}"
        "QTableWidget::item:alternate{background:#f8fafb;color:%2;}"
        "QHeaderView::section{background:#f5f6fa;color:%3;font-weight:bold;"
        "font-size:10px;letter-spacing:1px;padding:12px 14px;border:none;"
        "border-bottom:2px solid %1;}"
    ).arg(BORDER, TXT, DIM);
}

static QString btnCSS(const QString &bg, const QString &hover) {
    return QString(
        "QPushButton{background:%1;color:white;border:none;border-radius:10px;"
        "padding:10px 20px;font-size:12px;font-weight:bold;letter-spacing:0.5px;}"
        "QPushButton:hover{background:%2;}").arg(bg, hover);
}

static QPushButton* makeActionBtn(const QString &text, const QString &color,
    const QString &lightBg, QWidget *parent)
{
    QPushButton *btn = new QPushButton(text, parent);
    btn->setFlat(true);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedSize(32, 28);
    btn->setStyleSheet(QString(
        "QPushButton{background-color:%1;color:%2;border:none;border-radius:6px;"
        "font-size:15px;padding:0px;}"
        "QPushButton:hover{background-color:%2;color:white;border-radius:6px;}"
        "QPushButton:pressed{background-color:%2;color:white;}").arg(lightBg, color));
    return btn;
}

// Keep smallBtn for backward compat
static QString smallBtn(const QString &color) {
    QString lightBg = "#f0f2f5";
    if (color == "#0984e3") lightBg = "#e8f4fd";
    else if (color == "#e17055") lightBg = "#fef0ec";
    else if (color == "#d63031") lightBg = "#fde8e8";
    else if (color == "#00b894") lightBg = "#e8f8f5";
    return QString(
        "QPushButton{background-color:%1;color:%2;border:none;border-radius:4px;"
        "padding:2px 10px;font-size:10px;font-weight:bold;}"
        "QPushButton:hover{background-color:%2;color:white;}"
        "QPushButton:pressed{background-color:%2;color:white;}").arg(lightBg, color);
}

static QString inputCSS() {
    return QString(
        "QLineEdit{background-color:white;border:1.5px solid %1;border-radius:10px;"
        "padding:10px 14px;font-size:13px;color:%2;}"
        "QLineEdit:focus{border-color:%3;background-color:white;}"
        "QLineEdit::placeholder{color:#b2bec3;}").arg(BORDER, TXT, GREEN);
}

static QGraphicsDropShadowEffect* shadow(QWidget *p, int blur=16, int alpha=15) {
    auto *s = new QGraphicsDropShadowEffect(p);
    s->setBlurRadius(blur); s->setColor(QColor(0,0,0,alpha)); s->setOffset(0,4);
    return s;
}

// ═══════════════════════════════════════════════════════════════
//  CONSTRUCTOR
// ═══════════════════════════════════════════════════════════════

AdminWidget::AdminWidget(const QString &userName, const QString &userRole, QWidget *parent)
    : QWidget(parent), m_userName(userName), m_userRole(userRole),
      m_isMoniteur(userRole == "Moniteur"),
      m_kpiEleves(nullptr), m_kpiVehiculesSub(nullptr), m_kpiSessions(nullptr),
      m_kpiTaux(nullptr), m_kpiRevenu(nullptr),
      m_kpiMoniteurs(nullptr), m_kpiMoniteursSub(nullptr),
      m_timelineLayout(nullptr), m_topStudentsLayout(nullptr), m_fleetLayout(nullptr),
      m_examSlotsTable(nullptr), m_maneuverStepsTable(nullptr), m_examResultsTable(nullptr)
{
    setupUI();
    refreshAll();
}

void AdminWidget::setupUI()
{
    QVBoxLayout *main = new QVBoxLayout(this);
    main->setContentsMargins(0,0,0,0);
    main->setSpacing(0);

    main->addWidget(createBanner());
    main->addWidget(createKPIRow());
    main->addWidget(createNavBar());

    // Pages
    m_pages = new QStackedWidget(this);
    m_pages->addWidget(createDashboardPage());      // 0
    m_pages->addWidget(createMoniteursPage());       // 1
    m_pages->addWidget(createElevesPage());          // 2
    m_pages->addWidget(createVehiculesPage());       // 3
    m_pages->addWidget(createSessionsPage());        // 4
    m_pages->addWidget(createReservationsPage());    // 5
    m_pages->addWidget(createVideosPage());          // 6
    m_pages->addWidget(createExamSessionsPage());    // 7
    m_pages->addWidget(createManeuverStepsPage());   // 8
    m_pages->addWidget(createExamResultsPage());     // 9
    main->addWidget(m_pages, 1);
}

// ═══════════════════════════════════════════════════════════════
//  BANNER
// ═══════════════════════════════════════════════════════════════

QWidget* AdminWidget::createBanner()
{
    QFrame *b = new QFrame(this);
    b->setFixedHeight(80);
    b->setStyleSheet(
        "QFrame{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,"
        "stop:0 #2d3436,stop:0.5 #636e72,stop:1 #2d3436);border:none;}");

    QHBoxLayout *bl = new QHBoxLayout(b);
    bl->setContentsMargins(32,0,32,0);

    QLabel *icon = new QLabel(QString::fromUtf8("🛡️"), b);
    icon->setStyleSheet("QLabel{font-size:28px;background:transparent;border:none;}");
    bl->addWidget(icon);

    QVBoxLayout *titleCol = new QVBoxLayout();
    titleCol->setSpacing(2);
    QLabel *t = new QLabel("ADMINISTRATION", b);
    t->setStyleSheet("QLabel{font-size:18px;font-weight:bold;color:white;"
        "letter-spacing:4px;background:transparent;border:none;}");
    titleCol->addWidget(t);
    QLabel *sub = new QLabel(QString::fromUtf8("Control Panel · Wino Driving School"), b);
    sub->setStyleSheet("QLabel{font-size:11px;color:rgba(255,255,255,0.7);"
        "background:transparent;border:none;}");
    titleCol->addWidget(sub);
    bl->addLayout(titleCol);
    bl->addStretch();

    // Search
    m_searchEdit = new QLineEdit(b);
    m_searchEdit->setPlaceholderText(QString::fromUtf8("🔍  Global search..."));
    m_searchEdit->setFixedWidth(280);
    m_searchEdit->setFixedHeight(36);
    m_searchEdit->setStyleSheet(
        "QLineEdit{background:rgba(255,255,255,0.15);border:1px solid rgba(255,255,255,0.25);"
        "border-radius:18px;padding:0 16px;font-size:12px;color:white;}"
        "QLineEdit:focus{background:rgba(255,255,255,0.25);border-color:rgba(255,255,255,0.5);}"
        "QLineEdit::placeholder{color:rgba(255,255,255,0.5);}");
    connect(m_searchEdit, &QLineEdit::textChanged, this, &AdminWidget::onGlobalSearch);
    bl->addWidget(m_searchEdit);

    bl->addSpacing(16);

    QLabel *user = new QLabel(QString::fromUtf8("👤 %1").arg(m_userName), b);
    user->setStyleSheet("QLabel{font-size:11px;color:rgba(255,255,255,0.85);"
        "background:rgba(255,255,255,0.1);border-radius:14px;padding:6px 14px;border:none;}");
    bl->addWidget(user);

    return b;
}

// ═══════════════════════════════════════════════════════════════
//  KPI CARDS ROW
// ═══════════════════════════════════════════════════════════════

QFrame* AdminWidget::createKPICard(const QString &icon, const QString &label,
                                    const QString &gradient, QLabel **valRef, QLabel **subRef)
{
    QFrame *card = new QFrame(this);
    static int kid = 0;
    QString oid = QString("kpi%1").arg(kid++);
    card->setObjectName(oid);
    card->setStyleSheet(QString(
        "QFrame#%1{background:white;border-radius:14px;border:1px solid %2;}")
        .arg(oid, BORDER));
    card->setGraphicsEffect(shadow(card));
    card->setFixedHeight(90);

    QHBoxLayout *hl = new QHBoxLayout(card);
    hl->setContentsMargins(16,12,16,12);
    hl->setSpacing(14);

    // Colored icon circle
    QLabel *ic = new QLabel(icon, card);
    ic->setFixedSize(48,48);
    ic->setAlignment(Qt::AlignCenter);
    ic->setStyleSheet(QString(
        "QLabel{font-size:20px;background:qlineargradient(x1:0,y1:0,x2:1,y2:1,%1);"
        "border-radius:24px;border:none;}").arg(gradient));
    hl->addWidget(ic);

    QVBoxLayout *vl = new QVBoxLayout();
    vl->setSpacing(2);

    QLabel *val = new QLabel("—", card);
    val->setStyleSheet(QString("QLabel{font-size:22px;font-weight:bold;color:%1;"
        "background:transparent;border:none;}").arg(TXT));
    vl->addWidget(val);

    QLabel *lbl = new QLabel(label, card);
    lbl->setStyleSheet(QString("QLabel{font-size:10px;color:%1;font-weight:600;"
        "letter-spacing:0.5px;background:transparent;border:none;}").arg(DIM));
    vl->addWidget(lbl);

    QLabel *s = new QLabel("", card);
    s->setStyleSheet(QString("QLabel{font-size:9px;color:%1;background:transparent;border:none;}").arg(DIM));
    vl->addWidget(s);

    hl->addLayout(vl, 1);

    *valRef = val;
    *subRef = s;
    return card;
}

QWidget* AdminWidget::createKPIRow()
{
    QWidget *w = new QWidget(this);
    w->setObjectName("kpiRow");
    w->setStyleSheet(QString("QWidget#kpiRow{background:%1;}").arg(BG));
    w->setFixedHeight(110);

    QHBoxLayout *hl = new QHBoxLayout(w);
    hl->setContentsMargins(28,10,28,10);
    hl->setSpacing(14);
    hl->addWidget(createKPICard(QString::fromUtf8("\xf0\x9f\x91\xa5"), QString::fromUtf8("Active Students"),
        "stop:0 #00b894,stop:1 #55efc4", &m_kpiEleves, &m_kpiElevesSub));
    hl->addWidget(createKPICard(QString::fromUtf8("🧑‍🏫"), "Moniteurs",
        "stop:0 #00cec9,stop:1 #81ecec", &m_kpiMoniteurs, &m_kpiMoniteursSub));
    hl->addWidget(createKPICard(QString::fromUtf8("\xf0\x9f\x9a\x97"), "Fleet",
        "stop:0 #0984e3,stop:1 #74b9ff", &m_kpiVehicules, &m_kpiVehiculesSub));
    hl->addWidget(createKPICard(QString::fromUtf8("📋"), "Sessions",
        "stop:0 #6c5ce7,stop:1 #a29bfe", &m_kpiSessions, &m_kpiSessionsSub));
    hl->addWidget(createKPICard(QString::fromUtf8("🏆"), QString::fromUtf8("Success Rate"),
        "stop:0 #fdcb6e,stop:1 #ffeaa7", &m_kpiTaux, &m_kpiTauxSub));
    hl->addWidget(createKPICard(QString::fromUtf8("💰"), "Revenue",
        "stop:0 #e17055,stop:1 #fab1a0", &m_kpiRevenu, &m_kpiRevenuSub));

    return w;
}

// ═══════════════════════════════════════════════════════════════
//  NAVIGATION BAR (pill tabs)
// ═══════════════════════════════════════════════════════════════

QWidget* AdminWidget::createNavBar()
{
    QWidget *bar = new QWidget(this);
    bar->setObjectName("navBar");
    bar->setStyleSheet(QString("QWidget#navBar{background:%1;border-bottom:1px solid %2;}").arg(BG, BORDER));
    bar->setFixedHeight(50);

    QHBoxLayout *hl = new QHBoxLayout(bar);
    hl->setContentsMargins(28,8,28,8);
    hl->setSpacing(8);

    QStringList tabs = {
        QString::fromUtf8("📊 Dashboard"),
        QString::fromUtf8("🧑‍🏫 Moniteurs"),
        QString::fromUtf8("\xf0\x9f\x91\xa5 Students"),
        QString::fromUtf8("🚗 Vehicles"),
        QString::fromUtf8("📋 Sessions"),
        QString::fromUtf8("📅 Reservations"),
        QString::fromUtf8("\xf0\x9f\x8e\xac Videos"),
        QString::fromUtf8("🗓 Exam Sessions"),
        QString::fromUtf8("\xf0\x9f\x85\xbf\xef\xb8\x8f Steps"),
        QString::fromUtf8("\xf0\x9f\x8f\x86 Exam Results")
    };

    for (int i = 0; i < tabs.size(); i++) {
        QPushButton *btn = new QPushButton(tabs[i], bar);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setCheckable(true);
        btn->setAutoExclusive(false); // Bug 3 fix: on gère l'état manuellement
        btn->setChecked(i == 0);
        btn->setStyleSheet(
            "QPushButton{background:white;color:" + DIM + ";border:1px solid " + BORDER + ";"
            "border-radius:12px;padding:6px 16px;font-size:11px;font-weight:bold;}"
            "QPushButton:hover{background:#e8f8f5;border-color:" + GREEN + ";color:" + GREEN + ";}"
            "QPushButton:checked{background:" + GREEN + ";color:white;border-color:" + GREEN + ";}");
        connect(btn, &QPushButton::clicked, this, [this, i]() {
            navigateTo(i); // navigateTo gère le checked de tous les boutons
        });
        hl->addWidget(btn);
        m_navBtns.append(btn);
    }

    hl->addStretch();

    QPushButton *refreshBtn = new QPushButton(QString::fromUtf8("🔄"), bar);
    refreshBtn->setFixedSize(36,36);
    refreshBtn->setCursor(Qt::PointingHandCursor);
    refreshBtn->setToolTip("Refresh all data");
    refreshBtn->setStyleSheet(
        "QPushButton{background:white;border:1px solid " + BORDER + ";border-radius:18px;font-size:16px;}"
        "QPushButton:hover{background:#e8f8f5;border-color:" + GREEN + ";}");
    connect(refreshBtn, &QPushButton::clicked, this, &AdminWidget::refreshAll);
    hl->addWidget(refreshBtn);

    return bar;
}

void AdminWidget::navigateTo(int page)
{
    for (int i = 0; i < m_navBtns.size(); i++) {
        // Bug 3 fix: bloquer les signaux pour éviter le double-toggle Qt
        m_navBtns[i]->blockSignals(true);
        m_navBtns[i]->setChecked(i == page);
        m_navBtns[i]->blockSignals(false);
    }
    m_pages->setCurrentIndex(page);
    
}

// ═══════════════════════════════════════════════════════════════
//  DASHBOARD PAGE
// ═══════════════════════════════════════════════════════════════

QWidget* AdminWidget::createDashboardPage()
{
    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(SCROLL_SS.arg(BG));

    QWidget *content = new QWidget();
    content->setObjectName("dashContent");
    content->setStyleSheet(QString("QWidget#dashContent{background:%1;}").arg(BG));
    QVBoxLayout *lay = new QVBoxLayout(content);
    lay->setContentsMargins(28,20,28,20);
    lay->setSpacing(18);

    // Row 1: Timeline + Top students
    QHBoxLayout *row1 = new QHBoxLayout();
    row1->setSpacing(18);
    row1->addWidget(createActivityTimeline(), 2);
    row1->addWidget(createTopStudentsCard(), 1);
    lay->addLayout(row1);

    // Row 2: Fleet overview
    lay->addWidget(createFleetOverview());

    lay->addStretch();
    scroll->setWidget(content);
    return scroll;
}

QWidget* AdminWidget::createActivityTimeline()
{
    QFrame *card = new QFrame(this);
    card->setObjectName("timelineCard");
    card->setStyleSheet(QString("QFrame#timelineCard{background:white;border-radius:14px;border:1px solid %1;}").arg(BORDER));
    card->setGraphicsEffect(shadow(card));

    QVBoxLayout *lay = new QVBoxLayout(card);
    lay->setContentsMargins(20,18,20,18);
    lay->setSpacing(12);

    QHBoxLayout *hdr = new QHBoxLayout();
    QLabel *title = new QLabel(QString::fromUtf8("⏱ Recent Activity"), card);
    title->setStyleSheet(QString("QLabel{font-size:14px;font-weight:bold;color:%1;background:transparent;border:none;}").arg(TXT));
    hdr->addWidget(title);
    hdr->addStretch();
    lay->addLayout(hdr);

    m_timelineLayout = new QVBoxLayout();
    m_timelineLayout->setSpacing(6);
    lay->addLayout(m_timelineLayout);

    QLabel *empty = new QLabel(QString::fromUtf8("No activity"), card);
    empty->setStyleSheet(QString("QLabel{font-size:11px;color:%1;padding:20px;background:transparent;border:none;}").arg(DIM));
    empty->setAlignment(Qt::AlignCenter);
    m_timelineLayout->addWidget(empty);

    lay->addStretch();
    return card;
}

QWidget* AdminWidget::createTopStudentsCard()
{
    QFrame *card = new QFrame(this);
    card->setObjectName("topCard");
    card->setStyleSheet(QString("QFrame#topCard{background:white;border-radius:14px;border:1px solid %1;}").arg(BORDER));
    card->setGraphicsEffect(shadow(card));

    QVBoxLayout *lay = new QVBoxLayout(card);
    lay->setContentsMargins(20,18,20,18);
    lay->setSpacing(12);

    QLabel *title = new QLabel(QString::fromUtf8("\xf0\x9f\x8f\x85 Top Students"), card);
    title->setStyleSheet(QString("QLabel{font-size:14px;font-weight:bold;color:%1;background:transparent;border:none;}").arg(TXT));
    lay->addWidget(title);

    m_topStudentsLayout = new QVBoxLayout();
    m_topStudentsLayout->setSpacing(8);
    lay->addLayout(m_topStudentsLayout);
    lay->addStretch();

    return card;
}

QWidget* AdminWidget::createFleetOverview()
{
    QFrame *card = new QFrame(this);
    card->setObjectName("fleetCard");
    card->setStyleSheet(QString("QFrame#fleetCard{background:white;border-radius:14px;border:1px solid %1;}").arg(BORDER));
    card->setGraphicsEffect(shadow(card));

    QVBoxLayout *lay = new QVBoxLayout(card);
    lay->setContentsMargins(20,18,20,18);
    lay->setSpacing(12);

    QLabel *title = new QLabel(QString::fromUtf8("\xf0\x9f\x9a\x97 Fleet Status"), card);
    title->setStyleSheet(QString("QLabel{font-size:14px;font-weight:bold;color:%1;background:transparent;border:none;}").arg(TXT));
    lay->addWidget(title);

    m_fleetLayout = new QVBoxLayout();
    m_fleetLayout->setSpacing(8);
    QHBoxLayout *fleetRow = new QHBoxLayout();
    fleetRow->setSpacing(12);
    m_fleetLayout->addLayout(fleetRow);
    lay->addLayout(m_fleetLayout);

    return card;
}

// ═══════════════════════════════════════════════════════════════
//  MONITEURS PAGE
// ═══════════════════════════════════════════════════════════════

QWidget* AdminWidget::createMoniteursPage()
{
    QWidget *page = new QWidget(this);
    page->setObjectName("moniteursPage");
    page->setStyleSheet(QString("QWidget#moniteursPage{background:%1;}").arg(BG));
    QVBoxLayout *lay = new QVBoxLayout(page);
    lay->setContentsMargins(28,16,28,16);
    lay->setSpacing(14);

    QHBoxLayout *toolbar = new QHBoxLayout();
    QPushButton *addBtn = new QPushButton(QString::fromUtf8("➕ New Moniteur"), page);
    addBtn->setCursor(Qt::PointingHandCursor);
    addBtn->setStyleSheet(btnCSS(GREEN, "#00a884"));
    connect(addBtn, &QPushButton::clicked, this, &AdminWidget::showAddMoniteurDialog);
    toolbar->addWidget(addBtn);
    toolbar->addStretch();
    lay->addLayout(toolbar);

    m_moniteursTable = new QTableWidget(page);
    m_moniteursTable->setColumnCount(6);
    m_moniteursTable->setHorizontalHeaderLabels({
        "ID", "Full Name", "Phone", "Email",
        "Reservations", "Actions"
    });
    m_moniteursTable->setStyleSheet(tableCSS());
    m_moniteursTable->horizontalHeader()->setStretchLastSection(true);
    m_moniteursTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_moniteursTable->verticalHeader()->setVisible(false);
    m_moniteursTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_moniteursTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_moniteursTable->setAlternatingRowColors(true);
    m_moniteursTable->setShowGrid(false);
    m_moniteursTable->verticalHeader()->setDefaultSectionSize(42);
    lay->addWidget(m_moniteursTable, 1);

    return page;
}

void AdminWidget::refreshMoniteursTable()
{
    auto moniteurs = ParkingDBManager::instance().getAllMoniteurs();
    m_moniteursTable->setRowCount(moniteurs.size());

    for (int r = 0; r < moniteurs.size(); r++) {
        const auto &m = moniteurs[r];
        int id = m["id"].toInt();

        m_moniteursTable->setItem(r, 0, new QTableWidgetItem(QString::number(id)));
        m_moniteursTable->setItem(r, 1, new QTableWidgetItem(
            QString("%1 %2").arg(m["prenom"].toString(), m["nom"].toString())));
        m_moniteursTable->setItem(r, 2, new QTableWidgetItem(m["telephone"].toString()));
        m_moniteursTable->setItem(r, 3, new QTableWidgetItem(m["email"].toString()));

        // Count reservations assigned to this moniteur
        QSqlQuery rq;
        rq.prepare("SELECT COUNT(*) FROM EXAM_REQUEST WHERE INSTRUCTOR_ID=?");
        rq.addBindValue(id);
        int resCount = 0;
        if (rq.exec() && rq.next()) resCount = rq.value(0).toInt();
        m_moniteursTable->setItem(r, 4, new QTableWidgetItem(QString::number(resCount)));

        // Actions
        QWidget *actions = new QWidget();
        actions->setStyleSheet("background:transparent;");
        QHBoxLayout *al = new QHBoxLayout(actions);
        al->setContentsMargins(4,4,4,4); al->setSpacing(6);

        QPushButton *editBtn = makeActionBtn(QString::fromUtf8("\xe2\x9c\x8f\xef\xb8\x8f"), ORANGE, "#fef0ec", actions);
        editBtn->setToolTip("Edit instructor");
        connect(editBtn, &QPushButton::clicked, this, [this, id]() { showEditMoniteurDialog(id); });
        al->addWidget(editBtn);

        QPushButton *delBtn = makeActionBtn(QString::fromUtf8("\xf0\x9f\x97\x91\xef\xb8\x8f"), RED, "#fde8e8", actions);
        delBtn->setToolTip("Delete instructor");
        connect(delBtn, &QPushButton::clicked, this, [this, id]() { deleteMoniteur(id); });
        al->addWidget(delBtn);

        m_moniteursTable->setCellWidget(r, 5, actions);
    }

    if (m_kpiMoniteurs) m_kpiMoniteurs->setText(QString::number(moniteurs.size()));
    if (m_kpiMoniteursSub) m_kpiMoniteursSub->setText(
        moniteurs.isEmpty() ? QString::fromUtf8("No instructor") :
        QString::fromUtf8("%1 active").arg(moniteurs.size()));
}

void AdminWidget::showAddMoniteurDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QString::fromUtf8("New Moniteur"));
    dlg.setFixedWidth(440);
    dlg.setStyleSheet("QDialog{background:white;border-radius:12px;}");

    QVBoxLayout *main = new QVBoxLayout(&dlg);
    main->setContentsMargins(28,24,28,24);
    main->setSpacing(16);

    QLabel *title = new QLabel(QString::fromUtf8("🧑‍🏫 Add Moniteur"), &dlg);
    title->setStyleSheet(QString("QLabel{font-size:16px;font-weight:bold;color:%1;}").arg(TXT));
    main->addWidget(title);

    QFormLayout *form = new QFormLayout();
    form->setSpacing(10);

    QLineEdit *nom = new QLineEdit(&dlg);     nom->setStyleSheet(inputCSS()); nom->setPlaceholderText("Ben Salah");
    QLineEdit *prenom = new QLineEdit(&dlg);  prenom->setStyleSheet(inputCSS()); prenom->setPlaceholderText("Mohamed");
    QLineEdit *tel = new QLineEdit(&dlg);     tel->setStyleSheet(inputCSS()); tel->setPlaceholderText("+216 XX XXX XXX");
    QLineEdit *email = new QLineEdit(&dlg);   email->setStyleSheet(inputCSS()); email->setPlaceholderText("moniteur@wino.tn");

    form->addRow("Nom *", nom);
    form->addRow(QString::fromUtf8("Prénom *"), prenom);
    form->addRow(QString::fromUtf8("Phone"), tel);
    form->addRow("Email", email);
    main->addLayout(form);

    QHBoxLayout *btns = new QHBoxLayout();
    btns->addStretch();
    QPushButton *cancelBtn = new QPushButton("Cancel", &dlg);
    cancelBtn->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:1px solid %1;"
        "border-radius:10px;padding:10px 20px;font-size:12px;font-weight:bold;}"
        "QPushButton:hover{background:#f0f0f0;}").arg(BORDER, DIM));
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    btns->addWidget(cancelBtn);

    QPushButton *saveBtn = new QPushButton(QString::fromUtf8("✅ Add"), &dlg);
    saveBtn->setStyleSheet(btnCSS(GREEN, "#00a884"));
    connect(saveBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    btns->addWidget(saveBtn);
    main->addLayout(btns);

    if (dlg.exec() == QDialog::Accepted) {
        if (nom->text().trimmed().isEmpty() || prenom->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "", QString::fromUtf8("Nom et prénom obligatoires."));
            return;
        }
        int newId = ParkingDBManager::instance().addMoniteur(
            nom->text().trimmed(), prenom->text().trimmed(),
            tel->text().trimmed(), email->text().trimmed());
        if (newId > 0) {
            
            refreshAll();
        } else {
            // Show actual SQL error from log
            QSqlQuery errQ;
            QString hint = QString::fromUtf8(
                "Failed to add moniteur.\n\n"
                "Check wino_debug.log for the exact SQL error.\n\n"
                "In SQL Developer, run:\n"
                "SELECT column_name FROM user_tab_columns\n"
                "WHERE table_name='INSTRUCTORS';");
            QMessageBox::warning(this, "Error", hint);
        }
    }
}

void AdminWidget::showEditMoniteurDialog(int id)
{
    auto m = ParkingDBManager::instance().getMoniteur(id);
    if (m.isEmpty()) return;

    QDialog dlg(this);
    dlg.setWindowTitle(QString::fromUtf8("Edit Moniteur #%1").arg(id));
    dlg.setFixedWidth(440);
    dlg.setStyleSheet("QDialog{background:white;}");

    QFormLayout *form = new QFormLayout(&dlg);
    form->setContentsMargins(28,24,28,24);
    form->setSpacing(10);

    QLineEdit *nom = new QLineEdit(m["nom"].toString(), &dlg);       nom->setStyleSheet(inputCSS());
    QLineEdit *prenom = new QLineEdit(m["prenom"].toString(), &dlg); prenom->setStyleSheet(inputCSS());
    QLineEdit *tel = new QLineEdit(m["telephone"].toString(), &dlg); tel->setStyleSheet(inputCSS());
    QLineEdit *email = new QLineEdit(m["email"].toString(), &dlg);   email->setStyleSheet(inputCSS());

    form->addRow("Nom *", nom);
    form->addRow(QString::fromUtf8("Prénom *"), prenom);
    form->addRow(QString::fromUtf8("Phone"), tel);
    form->addRow("Email", email);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
    bb->button(QDialogButtonBox::Save)->setStyleSheet(btnCSS(BLUE, "#0770c2"));
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(bb);

    if (dlg.exec() == QDialog::Accepted) {
        if (nom->text().trimmed().isEmpty() || prenom->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "", QString::fromUtf8("Nom et prénom obligatoires."));
            return;
        }
        ParkingDBManager::instance().updateMoniteur(id, nom->text().trimmed(), prenom->text().trimmed(),
            tel->text().trimmed(), email->text().trimmed());
        
        refreshAll();
    }
}

void AdminWidget::deleteMoniteur(int id)
{
    auto m = ParkingDBManager::instance().getMoniteur(id);
    if (QMessageBox::question(this, "Delete",
            QString::fromUtf8("Delete moniteur %1 %2 ?").arg(m["prenom"].toString(), m["nom"].toString()),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        if (ParkingDBManager::instance().deleteMoniteur(id)) {
            
            refreshAll();
        } else {
            QMessageBox::warning(this, "Error",
                QString::fromUtf8("Cannot delete this moniteur.\nThey are assigned to exam reservations."));
        }
    }
}

// ═══════════════════════════════════════════════════════════════
//  ÉLÈVES PAGE
// ═══════════════════════════════════════════════════════════════

QWidget* AdminWidget::createElevesPage()
{
    QWidget *page = new QWidget(this);
    page->setObjectName("elevesPage");
    page->setStyleSheet(QString("QWidget#elevesPage{background:%1;}").arg(BG));
    QVBoxLayout *lay = new QVBoxLayout(page);
    lay->setContentsMargins(28,16,28,16);
    lay->setSpacing(14);

    QHBoxLayout *toolbar = new QHBoxLayout();
    QPushButton *addBtn = new QPushButton(QString::fromUtf8("➕ New Student"), page);
    addBtn->setCursor(Qt::PointingHandCursor);
    addBtn->setStyleSheet(btnCSS(GREEN, "#00a884"));
    connect(addBtn, &QPushButton::clicked, this, &AdminWidget::showAddEleveDialog);
    toolbar->addWidget(addBtn);
    toolbar->addStretch();
    lay->addLayout(toolbar);

    m_elevesTable = new QTableWidget(page);
    m_elevesTable->setColumnCount(8);
    m_elevesTable->setHorizontalHeaderLabels({
        "ID", "Full Name", "Phone", "License",
        "Sessions", "Success Rate", "Hours", "Actions"
    });
    m_elevesTable->setStyleSheet(tableCSS());
    m_elevesTable->horizontalHeader()->setStretchLastSection(true);
    m_elevesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_elevesTable->verticalHeader()->setVisible(false);
    m_elevesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_elevesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_elevesTable->setAlternatingRowColors(true);
    m_elevesTable->setShowGrid(false);
    m_elevesTable->verticalHeader()->setDefaultSectionSize(42);
    lay->addWidget(m_elevesTable, 1);

    return page;
}

void AdminWidget::refreshElevesTable()
{
    auto eleves = ParkingDBManager::instance().getAllEleves();
    auto &db = ParkingDBManager::instance();
    m_elevesTable->setRowCount(eleves.size());

    for (int r = 0; r < eleves.size(); r++) {
        const auto &e = eleves[r];
        int id = e["id"].toInt();
        int sessions = db.getSessionCount(id);
        double taux = db.getTauxReussite(id);
        int pracSec = db.getTotalPracticeSeconds(id);
        double hours = pracSec / 3600.0;

        m_elevesTable->setItem(r, 0, new QTableWidgetItem(QString::number(id)));
        m_elevesTable->setItem(r, 1, new QTableWidgetItem(
            QString("%1 %2").arg(e["prenom"].toString(), e["nom"].toString())));
        m_elevesTable->setItem(r, 2, new QTableWidgetItem(e["telephone"].toString()));
        m_elevesTable->setItem(r, 3, new QTableWidgetItem(e["numero_permis"].toString()));
        m_elevesTable->setItem(r, 4, new QTableWidgetItem(QString::number(sessions)));

        QTableWidgetItem *tauxItem = new QTableWidgetItem(QString("%1%").arg(taux, 0, 'f', 0));
        tauxItem->setForeground(QColor(taux >= 70 ? GREEN : (taux >= 40 ? ORANGE : RED)));
        m_elevesTable->setItem(r, 5, tauxItem);

        m_elevesTable->setItem(r, 6, new QTableWidgetItem(QString("%1h").arg(hours, 0, 'f', 1)));

        // Actions
        QWidget *actions = new QWidget();
        actions->setStyleSheet("background:transparent;");
        QHBoxLayout *al = new QHBoxLayout(actions);
        al->setContentsMargins(4,4,4,4); al->setSpacing(6);

        QPushButton *viewBtn = makeActionBtn(QString::fromUtf8("\xf0\x9f\x91\x81\xef\xb8\x8f"), BLUE, "#e8f4fd", actions);
        viewBtn->setToolTip("View student details");
        connect(viewBtn, &QPushButton::clicked, this, [this, id]() { showEleveDetailPanel(id); });
        al->addWidget(viewBtn);

        QPushButton *editBtn = makeActionBtn(QString::fromUtf8("\xe2\x9c\x8f\xef\xb8\x8f"), ORANGE, "#fef0ec", actions);
        editBtn->setToolTip("Edit student");
        connect(editBtn, &QPushButton::clicked, this, [this, id]() { showEditEleveDialog(id); });
        al->addWidget(editBtn);

        QPushButton *delBtn = makeActionBtn(QString::fromUtf8("\xf0\x9f\x97\x91\xef\xb8\x8f"), RED, "#fde8e8", actions);
        delBtn->setToolTip("Delete student");
        connect(delBtn, &QPushButton::clicked, this, [this, id]() { deleteEleve(id); });
        al->addWidget(delBtn);

        m_elevesTable->setCellWidget(r, 7, actions);
    }

    if (m_kpiEleves) m_kpiEleves->setText(QString::number(eleves.size()));
}

void AdminWidget::showAddEleveDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QString::fromUtf8("New Student"));
    dlg.setFixedWidth(440);
    dlg.setStyleSheet("QDialog{background:white;border-radius:12px;}");

    QVBoxLayout *main = new QVBoxLayout(&dlg);
    main->setContentsMargins(28,24,28,24);
    main->setSpacing(16);

    QLabel *title = new QLabel(QString::fromUtf8("👤 Add Student"), &dlg);
    title->setStyleSheet(QString("QLabel{font-size:16px;font-weight:bold;color:%1;}").arg(TXT));
    main->addWidget(title);

    QFormLayout *form = new QFormLayout();
    form->setSpacing(10);

    QLineEdit *nom = new QLineEdit(&dlg);     nom->setStyleSheet(inputCSS()); nom->setPlaceholderText("Ben Ali");
    QLineEdit *prenom = new QLineEdit(&dlg);  prenom->setStyleSheet(inputCSS()); prenom->setPlaceholderText("Ahmed");
    QLineEdit *tel = new QLineEdit(&dlg);     tel->setStyleSheet(inputCSS()); tel->setPlaceholderText("+216 XX XXX XXX");
    QLineEdit *email = new QLineEdit(&dlg);   email->setStyleSheet(inputCSS()); email->setPlaceholderText("email@example.com");
    QLineEdit *permis = new QLineEdit(&dlg);  permis->setStyleSheet(inputCSS()); permis->setPlaceholderText("TN-2026-XXXXX");

    form->addRow("Nom *", nom);
    form->addRow(QString::fromUtf8("Prénom *"), prenom);
    form->addRow(QString::fromUtf8("Phone"), tel);
    form->addRow("Email", email);
    form->addRow(QString::fromUtf8("N° License"), permis);
    main->addLayout(form);

    QHBoxLayout *btns = new QHBoxLayout();
    btns->addStretch();
    QPushButton *cancelBtn = new QPushButton("Cancel", &dlg);
    cancelBtn->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:1px solid %1;"
        "border-radius:10px;padding:10px 20px;font-size:12px;font-weight:bold;}"
        "QPushButton:hover{background:#f0f0f0;}").arg(BORDER, DIM));
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    btns->addWidget(cancelBtn);

    QPushButton *saveBtn = new QPushButton(QString::fromUtf8("✅ Add"), &dlg);
    saveBtn->setStyleSheet(btnCSS(GREEN, "#00a884"));
    connect(saveBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    btns->addWidget(saveBtn);
    main->addLayout(btns);

    if (dlg.exec() == QDialog::Accepted) {
        if (nom->text().trimmed().isEmpty() || prenom->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "", QString::fromUtf8("Nom et prénom obligatoires."));
            return;
        }
        ParkingDBManager::instance().addEleve(
            nom->text().trimmed(), prenom->text().trimmed(),
            tel->text().trimmed(), email->text().trimmed(), permis->text().trimmed());
        
        refreshAll();
    }
}

void AdminWidget::showEditEleveDialog(int id)
{
    auto e = ParkingDBManager::instance().getEleve(id);
    if (e.isEmpty()) return;

    QDialog dlg(this);
    dlg.setWindowTitle(QString::fromUtf8("Edit student #%1").arg(id));
    dlg.setFixedWidth(440);
    dlg.setStyleSheet("QDialog{background:white;}");

    QFormLayout *form = new QFormLayout(&dlg);
    form->setContentsMargins(28,24,28,24);
    form->setSpacing(10);

    QLineEdit *nom = new QLineEdit(e["nom"].toString(), &dlg);       nom->setStyleSheet(inputCSS());
    QLineEdit *prenom = new QLineEdit(e["prenom"].toString(), &dlg); prenom->setStyleSheet(inputCSS());
    QLineEdit *tel = new QLineEdit(e["telephone"].toString(), &dlg); tel->setStyleSheet(inputCSS());
    QLineEdit *email = new QLineEdit(e["email"].toString(), &dlg);   email->setStyleSheet(inputCSS());
    QLineEdit *permis = new QLineEdit(e["numero_permis"].toString(), &dlg); permis->setStyleSheet(inputCSS());

    form->addRow("Nom *", nom);
    form->addRow(QString::fromUtf8("Prénom *"), prenom);
    form->addRow(QString::fromUtf8("Phone"), tel);
    form->addRow("Email", email);
    form->addRow(QString::fromUtf8("N° License"), permis);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
    bb->button(QDialogButtonBox::Save)->setStyleSheet(btnCSS(BLUE, "#0770c2"));
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(bb);

    if (dlg.exec() == QDialog::Accepted) {
        ParkingDBManager::instance().updateEleve(id, nom->text().trimmed(), prenom->text().trimmed(),
            tel->text().trimmed(), email->text().trimmed(), permis->text().trimmed());
        
        refreshAll();
    }
}

void AdminWidget::deleteEleve(int id)
{
    auto e = ParkingDBManager::instance().getEleve(id);
    if (QMessageBox::question(this, "Delete",
            QString::fromUtf8("Delete %1 %2 ?").arg(e["prenom"].toString(), e["nom"].toString()),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        ParkingDBManager::instance().deleteEleve(id);
        
        refreshAll();
    }
}

void AdminWidget::showEleveDetailPanel(int id)
{
    auto e = ParkingDBManager::instance().getEleve(id);
    auto &db = ParkingDBManager::instance();
    if (e.isEmpty()) return;

    int total = db.getSessionCount(id);
    int success = db.getSuccessCount(id);
    double taux = db.getTauxReussite(id);
    int pracSec = db.getTotalPracticeSeconds(id);
    double hours = pracSec / 3600.0;
    double depense = db.getTotalDepense(id);

    QStringList manTypes = {"creneau", "bataille", "epi", "marche_arriere", "demi_tour"};
    QStringList manNames = {QString::fromUtf8("Slot"), "Bataille", QString::fromUtf8("Diagonal"),
                            QString::fromUtf8("Marche arr."), "U-turn"};

    QString manDetails;
    for (int i = 0; i < manTypes.size(); i++) {
        int bt = db.getMeilleurTemps(id, manTypes[i]);
        int perf = db.getPerfectRunsCount(id, manTypes[i]);
        QString btStr = bt > 0 ? QString("%1:%2").arg(bt/60).arg(bt%60, 2, 10, QChar('0')) : QString::fromUtf8("—");
        manDetails += QString("<tr><td>%1</td><td align='center'>%2</td><td align='center'>%3</td></tr>")
            .arg(manNames[i], btStr, QString::number(perf));
    }

    bool examReady = db.isExamReady(id, manTypes);

    QString html = QString::fromUtf8(
        "<div style='font-family:Segoe UI,sans-serif;'>"
        "<h2 style='color:#2d3436;margin:0;'>%1 %2</h2>"
        "<p style='color:#636e72;margin:4px 0 16px;'>📞 %3 · 📧 %4 · 🪪 %5</p>"
        "<table style='width:100%%;border-collapse:collapse;'>"
        "<tr style='background:#f8f9fa;'>"
        "<td style='padding:12px;text-align:center;border-radius:8px;'>"
        "<div style='font-size:24px;font-weight:bold;color:#00b894;'>%6</div>"
        "<div style='font-size:10px;color:#636e72;'>Sessions</div></td>"
        "<td style='padding:12px;text-align:center;'>"
        "<div style='font-size:24px;font-weight:bold;color:#6c5ce7;'>%7%%</div>"
        "<div style='font-size:10px;color:#636e72;'>Success Rate</div></td>"
        "<td style='padding:12px;text-align:center;'>"
        "<div style='font-size:24px;font-weight:bold;color:#0984e3;'>%8h</div>"
        "<div style='font-size:10px;color:#636e72;'>Pratique</div></td>"
        "<td style='padding:12px;text-align:center;'>"
        "<div style='font-size:24px;font-weight:bold;color:#e17055;'>%9 DT</div>"
        "<div style='font-size:10px;color:#636e72;'>Spent</div></td>"
        "</tr></table>"
        "<h3 style='color:#2d3436;margin:16px 0 8px;'>📊 Détail par maneuver</h3>"
        "<table style='width:100%%;font-size:12px;border-collapse:collapse;'>"
        "<tr style='background:#f8f9fa;font-weight:bold;color:#636e72;'>"
        "<td style='padding:8px;'>Maneuver</td><td align='center'>Best time</td>"
        "<td align='center'>Perfect</td></tr>%10</table>"
        "<p style='margin-top:16px;padding:10px;border-radius:8px;"
        "background:%11;color:%12;font-weight:bold;text-align:center;'>"
        "%13</p></div>")
        .arg(e["prenom"].toString(), e["nom"].toString())
        .arg(e["telephone"].toString(), e["email"].toString(), e["numero_permis"].toString())
        .arg(total).arg(taux, 0, 'f', 0).arg(hours, 0, 'f', 1).arg(depense, 0, 'f', 1)
        .arg(manDetails)
        .arg(examReady ? "#e8f8f5" : "#fef3e2")
        .arg(examReady ? "#00b894" : "#e17055")
        .arg(examReady ? QString::fromUtf8("Eligible for ATTT exam") :
                         QString::fromUtf8("🔒 Not yet eligible — keep training l'training"));

    QDialog dlg(this);
    dlg.setWindowTitle(QString::fromUtf8("Student Profile — %1 %2").arg(e["prenom"].toString(), e["nom"].toString()));
    dlg.setMinimumSize(560, 520);
    dlg.setStyleSheet("QDialog{background:white;}");

    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    lay->setContentsMargins(24,20,24,20);

    QLabel *content = new QLabel(&dlg);
    content->setTextFormat(Qt::RichText);
    content->setWordWrap(true);
    content->setText(html);
    content->setStyleSheet("QLabel{background:transparent;border:none;}");
    lay->addWidget(content);

    QPushButton *closeBtn = new QPushButton("Close", &dlg);
    closeBtn->setStyleSheet(btnCSS(BLUE, "#0770c2"));
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    lay->addWidget(closeBtn, 0, Qt::AlignRight);

    dlg.exec();
}

// ═══════════════════════════════════════════════════════════════
//  VÉHICULES PAGE
// ═══════════════════════════════════════════════════════════════

QWidget* AdminWidget::createVehiculesPage()
{
    QWidget *page = new QWidget(this);
    page->setObjectName("vehPage");
    page->setStyleSheet(QString("QWidget#vehPage{background:%1;}").arg(BG));
    QVBoxLayout *lay = new QVBoxLayout(page);
    lay->setContentsMargins(28,16,28,16);
    lay->setSpacing(14);

    QHBoxLayout *toolbar = new QHBoxLayout();
    QPushButton *addBtn = new QPushButton(QString::fromUtf8("➕ New Vehicle"), page);
    addBtn->setCursor(Qt::PointingHandCursor);
    addBtn->setStyleSheet(btnCSS(GREEN, "#00a884"));
    connect(addBtn, &QPushButton::clicked, this, &AdminWidget::showAddVehiculeDialog);
    toolbar->addWidget(addBtn);
    toolbar->addStretch();
    lay->addLayout(toolbar);

    m_vehiculesTable = new QTableWidget(page);
    m_vehiculesTable->setColumnCount(8);
    m_vehiculesTable->setHorizontalHeaderLabels({
        "ID", "Registration", QString::fromUtf8("Model"), "Assistance",
        "Port", "Rate", "Status", "Actions"
    });
    m_vehiculesTable->setStyleSheet(tableCSS());
    m_vehiculesTable->horizontalHeader()->setStretchLastSection(true);
    m_vehiculesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_vehiculesTable->verticalHeader()->setVisible(false);
    m_vehiculesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_vehiculesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_vehiculesTable->setShowGrid(false);
    m_vehiculesTable->verticalHeader()->setDefaultSectionSize(42);
    m_vehiculesTable->setAlternatingRowColors(true);
    lay->addWidget(m_vehiculesTable, 1);

    return page;
}

void AdminWidget::refreshVehiculesTable()
{
    QSqlQuery q("SELECT id, plate_number AS immatriculation, "
                "NVL(brand,' ') || ' ' || NVL(model,' ') AS modele, "
                "assistance_type AS type_assistance, "
                "sensor_count AS nb_capteurs, "
                "serial_port AS port_serie, baud_rate, "
                "session_fee AS tarif_seance, "
                "is_active AS disponible "
                "FROM CARS WHERE is_parking_vehicle=1 ORDER BY id");
    QList<QVariantMap> vehicles;
    while (q.next()) {
        QVariantMap m;
        for (int i = 0; i < q.record().count(); i++)
            m[q.record().fieldName(i).toLower()] = q.value(i);
        vehicles.append(m);
    }

    m_vehiculesTable->setRowCount(vehicles.size());
    int dispoCount = 0;

    for (int r = 0; r < vehicles.size(); r++) {
        const auto &v = vehicles[r];
        int id = v["id"].toInt();
        bool dispo = v["disponible"].toInt() == 1;
        if (dispo) dispoCount++;

        m_vehiculesTable->setItem(r, 0, new QTableWidgetItem(QString::number(id)));
        m_vehiculesTable->setItem(r, 1, new QTableWidgetItem(v["immatriculation"].toString()));
        m_vehiculesTable->setItem(r, 2, new QTableWidgetItem(v["modele"].toString()));
        m_vehiculesTable->setItem(r, 3, new QTableWidgetItem(v["type_assistance"].toString()));
        m_vehiculesTable->setItem(r, 4, new QTableWidgetItem(v["port_serie"].toString()));
        m_vehiculesTable->setItem(r, 5, new QTableWidgetItem(QString("%1 DT").arg(v["tarif_seance"].toDouble(), 0, 'f', 1)));

        QTableWidgetItem *statusItem = new QTableWidgetItem(
            dispo ? QString::fromUtf8("🟢 Available") : QString::fromUtf8("🔴 Unavailable"));
        statusItem->setForeground(QColor(dispo ? GREEN : RED));
        m_vehiculesTable->setItem(r, 6, statusItem);

        QWidget *actions = new QWidget();
        actions->setStyleSheet("background:transparent;");
        QHBoxLayout *al = new QHBoxLayout(actions);
        al->setContentsMargins(4,4,4,4); al->setSpacing(6);

        QPushButton *editBtn = makeActionBtn(QString::fromUtf8("\xe2\x9c\x8f\xef\xb8\x8f"), ORANGE, "#fef0ec", actions);
        editBtn->setToolTip("Edit vehicle");
        connect(editBtn, &QPushButton::clicked, this, [this, id]() { showEditVehiculeDialog(id); });
        al->addWidget(editBtn);

        QPushButton *togBtn = makeActionBtn(
            dispo ? QString::fromUtf8("\xf0\x9f\x94\x92") : QString::fromUtf8("\xf0\x9f\x94\x93"),
            dispo ? ORANGE : GREEN, dispo ? "#fef0ec" : "#e8f8f5", actions);
        togBtn->setToolTip(dispo ? "Disable vehicle" : "Enable vehicle");
        connect(togBtn, &QPushButton::clicked, this, [this, id, dispo]() {
            ParkingDBManager::instance().setVehiculeDisponible(id, !dispo);
            refreshAll();
        });
        al->addWidget(togBtn);

        QPushButton *delBtn = makeActionBtn(QString::fromUtf8("\xf0\x9f\x97\x91\xef\xb8\x8f"), RED, "#fde8e8", actions);
        delBtn->setToolTip("Delete vehicle");
        connect(delBtn, &QPushButton::clicked, this, [this, id]() { deleteVehicule(id); });
        al->addWidget(delBtn);

        m_vehiculesTable->setCellWidget(r, 7, actions);
    }

    if (m_kpiVehicules) {
        m_kpiVehicules->setText(QString::number(vehicles.size()));
        if (m_kpiVehiculesSub)
            m_kpiVehiculesSub->setText(QString::fromUtf8("%1 available").arg(dispoCount));
    }

    // Fleet overview on dashboard
    if (m_fleetLayout) {
        QLayoutItem *child;
        QLayout *fleetRow = nullptr;
        if (m_fleetLayout->count() > 0) {
            fleetRow = m_fleetLayout->itemAt(0)->layout();
            if (fleetRow) {
                while (fleetRow->count() > 0) {
                    QLayoutItem *item = fleetRow->takeAt(0);
                    if (item->widget()) delete item->widget();
                    delete item;
                }
            }
        }
        if (!fleetRow) return;

        for (const auto &v : vehicles) {
            bool d = v["disponible"].toInt() == 1;
            QFrame *fc = new QFrame();
            static int fid = 0;
            QString foid = QString("fc%1").arg(fid++);
            fc->setObjectName(foid);
            fc->setStyleSheet(QString("QFrame#%1{background:%2;border-radius:12px;border:1px solid %3;}")
                .arg(foid, d ? "#e8f8f5" : "#fef0f0", d ? "#b8e6d0" : "#fab1a0"));
            fc->setFixedHeight(70); fc->setMinimumWidth(160);
            QVBoxLayout *fl = new QVBoxLayout(fc);
            fl->setContentsMargins(12,8,12,8); fl->setSpacing(2);
            QLabel *fm = new QLabel(QString::fromUtf8("🚗 %1").arg(v["modele"].toString()), fc);
            fm->setStyleSheet(QString("QLabel{font-size:11px;font-weight:bold;color:%1;background:transparent;border:none;}").arg(TXT));
            fl->addWidget(fm);
            QLabel *fp = new QLabel(v["immatriculation"].toString(), fc);
            fp->setStyleSheet(QString("QLabel{font-size:10px;color:%1;background:transparent;border:none;}").arg(DIM));
            fl->addWidget(fp);
            QLabel *fs = new QLabel(d ? QString::fromUtf8("🟢 Available") : QString::fromUtf8("🔴 Unavailable"), fc);
            fs->setStyleSheet(QString("QLabel{font-size:9px;color:%1;font-weight:bold;background:transparent;border:none;}").arg(d ? GREEN : RED));
            fl->addWidget(fs);
            qobject_cast<QHBoxLayout*>(fleetRow)->addWidget(fc);
        }
        qobject_cast<QHBoxLayout*>(fleetRow)->addStretch();
    }
}

void AdminWidget::showAddVehiculeDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QString::fromUtf8("New Vehicle"));
    dlg.setFixedWidth(460);
    dlg.setStyleSheet("QDialog{background:white;}");

    QFormLayout *form = new QFormLayout(&dlg);
    form->setContentsMargins(28,24,28,24); form->setSpacing(10);

    QLineEdit *immat = new QLineEdit(&dlg);  immat->setStyleSheet(inputCSS()); immat->setPlaceholderText("XXX TU YYYY");
    QLineEdit *modele = new QLineEdit(&dlg); modele->setStyleSheet(inputCSS()); modele->setPlaceholderText("Renault Clio V");
    QLineEdit *assist = new QLineEdit(&dlg); assist->setStyleSheet(inputCSS()); assist->setPlaceholderText("Parking aid + Camera");
    QLineEdit *port = new QLineEdit(&dlg);   port->setStyleSheet(inputCSS()); port->setPlaceholderText("COM3");
    QSpinBox *baud = new QSpinBox(&dlg); baud->setRange(1200,115200); baud->setValue(9600);
    QDoubleSpinBox *tarif = new QDoubleSpinBox(&dlg); tarif->setRange(0,500); tarif->setValue(15.0); tarif->setSuffix(" DT");

    form->addRow("Registration *", immat);
    form->addRow(QString::fromUtf8("Model"), modele);
    form->addRow("Assistance", assist);
    form->addRow("Port", port);
    form->addRow("Baud", baud);
    form->addRow("Rate", tarif);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    bb->button(QDialogButtonBox::Ok)->setText("Add");
    bb->button(QDialogButtonBox::Ok)->setStyleSheet(btnCSS(GREEN, "#00a884"));
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(bb);

    if (dlg.exec() == QDialog::Accepted) {
        if (immat->text().trimmed().isEmpty() || modele->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "", QString::fromUtf8("Registration and model required.")); return;
        }
        int id = ParkingDBManager::instance().addVehicule(immat->text().trimmed(), modele->text().trimmed(),
            assist->text().trimmed(), port->text().trimmed(), baud->value());
        if (id > 0) {
            QSqlQuery q; q.prepare("UPDATE CARS SET session_fee=? WHERE id=?");
            q.addBindValue(tarif->value()); q.addBindValue(id); q.exec();
            
            refreshAll();
        }
    }
}

void AdminWidget::showEditVehiculeDialog(int id)
{
    auto v = ParkingDBManager::instance().getVehicule(id);
    if (v.isEmpty()) return;

    QDialog dlg(this);
    dlg.setWindowTitle(QString::fromUtf8("Model #%1").arg(id));
    dlg.setFixedWidth(460);
    dlg.setStyleSheet("QDialog{background:white;}");

    QFormLayout *form = new QFormLayout(&dlg);
    form->setContentsMargins(28,24,28,24); form->setSpacing(10);

    QLineEdit *immat = new QLineEdit(v["immatriculation"].toString(), &dlg); immat->setStyleSheet(inputCSS());
    QLineEdit *modele = new QLineEdit(v["modele"].toString(), &dlg);         modele->setStyleSheet(inputCSS());
    QLineEdit *assist = new QLineEdit(v["type_assistance"].toString(), &dlg); assist->setStyleSheet(inputCSS());
    QLineEdit *port = new QLineEdit(v["port_serie"].toString(), &dlg);       port->setStyleSheet(inputCSS());
    QSpinBox *baud = new QSpinBox(&dlg); baud->setRange(1200,115200); baud->setValue(v["baud_rate"].toInt());
    QDoubleSpinBox *tarif = new QDoubleSpinBox(&dlg); tarif->setRange(0,500); tarif->setValue(v["tarif_seance"].toDouble()); tarif->setSuffix(" DT");

    form->addRow("Registration", immat); form->addRow("Model", modele);
    form->addRow("Assistance", assist); form->addRow("Port", port);
    form->addRow("Baud", baud); form->addRow("Rate", tarif);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
    bb->button(QDialogButtonBox::Save)->setStyleSheet(btnCSS(BLUE, "#0770c2"));
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(bb);

    if (dlg.exec() == QDialog::Accepted) {
        ParkingDBManager::instance().updateVehicule(id, immat->text().trimmed(), modele->text().trimmed(),
            assist->text().trimmed(), port->text().trimmed(), baud->value(), tarif->value());
        
        refreshAll();
    }
}

void AdminWidget::deleteVehicule(int id)
{
    if (QMessageBox::question(this, "Delete", QString::fromUtf8("Delete ce vehicle ?"),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        if (ParkingDBManager::instance().deleteVehicule(id)) {
            
            refreshAll();
        } else {
            QMessageBox::warning(this, "Erreur", QString::fromUtf8("Impossible de supprimer cette voiture. Elle est peut-être déjà utilisée dans des sessions ou des réservations."));
        }
    }
}

// ═══════════════════════════════════════════════════════════════
//  SESSIONS PAGE
// ═══════════════════════════════════════════════════════════════

QWidget* AdminWidget::createSessionsPage()
{
    QWidget *page = new QWidget(this);
    page->setObjectName("sessPage");
    page->setStyleSheet(QString("QWidget#sessPage{background:%1;}").arg(BG));
    QVBoxLayout *lay = new QVBoxLayout(page);
    lay->setContentsMargins(28,16,28,16); lay->setSpacing(14);

    m_sessionsTable = new QTableWidget(page);
    m_sessionsTable->setColumnCount(10);
    m_sessionsTable->setHorizontalHeaderLabels({
        "ID", QString::fromUtf8("Student"), QString::fromUtf8("Vehicle"), QString::fromUtf8("Maneuver"),
        "Date", "Duration", "Result", "Errors", "Price", "Actions"
    });
    m_sessionsTable->setStyleSheet(tableCSS());
    m_sessionsTable->horizontalHeader()->setStretchLastSection(true);
    m_sessionsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_sessionsTable->verticalHeader()->setVisible(false);
    m_sessionsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_sessionsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_sessionsTable->setShowGrid(false);
    m_sessionsTable->verticalHeader()->setDefaultSectionSize(42);
    m_sessionsTable->setAlternatingRowColors(true);
    lay->addWidget(m_sessionsTable, 1);

    return page;
}

void AdminWidget::refreshSessionsTable()
{
    QSqlQuery q("SELECT * FROM ("
                "SELECT s.session_id AS id, s.student_id AS eleve_id, s.car_id AS vehicule_id, "
                "s.maneuver_type AS manoeuvre_type, s.session_start AS date_debut, "
                "s.session_end AS date_fin, s.duration_seconds AS duree_secondes, "
                "s.is_exam_mode AS mode_examen, s.is_successful AS reussi, "
                "s.error_count AS nb_erreurs, s.stall_count AS nb_calages, "
                "e.first_name AS prenom, e.last_name AS nom, "
                "NVL(c.brand,' ') || ' ' || NVL(c.model,' ') AS modele, "
                "NVL(c.session_fee, 0) AS session_fee "
                "FROM PARKING_SESSIONS s "
                "LEFT JOIN STUDENTS e ON s.student_id=e.id "
                "LEFT JOIN CARS c ON s.car_id=c.id "
                "ORDER BY s.session_start DESC) WHERE ROWNUM <= 100");

    QList<QVariantMap> sessions;
    while (q.next()) {
        QVariantMap m;
        for (int i = 0; i < q.record().count(); i++)
            m[q.record().fieldName(i).toLower()] = q.value(i);
        sessions.append(m);
    }

    m_sessionsTable->setRowCount(sessions.size());
    int totalOk = 0;

    for (int r = 0; r < sessions.size(); r++) {
        const auto &s = sessions[r];
        int id = s["id"].toInt();
        bool ok = s["reussi"].toInt() == 1;
        if (ok) totalOk++;

        m_sessionsTable->setItem(r, 0, new QTableWidgetItem(QString::number(id)));
        m_sessionsTable->setItem(r, 1, new QTableWidgetItem(
            QString("%1 %2").arg(s["prenom"].toString(), s["nom"].toString())));
        m_sessionsTable->setItem(r, 2, new QTableWidgetItem(s["modele"].toString()));
        m_sessionsTable->setItem(r, 3, new QTableWidgetItem(s["manoeuvre_type"].toString()));
        m_sessionsTable->setItem(r, 4, new QTableWidgetItem(s["date_debut"].toString().left(16)));

        int dur = s["duree_secondes"].toInt();
        m_sessionsTable->setItem(r, 5, new QTableWidgetItem(
            QString("%1:%2").arg(dur/60).arg(dur%60, 2, 10, QChar('0'))));

        QTableWidgetItem *resItem = new QTableWidgetItem(
            ok ? QString::fromUtf8("\xe2\x9c\x85 Passed") : QString::fromUtf8("\xe2\x9d\x8c Failed"));
        resItem->setForeground(QColor(ok ? GREEN : RED));
        m_sessionsTable->setItem(r, 6, resItem);
        m_sessionsTable->setItem(r, 7, new QTableWidgetItem(QString::number(s["nb_erreurs"].toInt())));

        double fee = s["session_fee"].toDouble();
        QTableWidgetItem *priceItem = new QTableWidgetItem(
            fee > 0 ? QString("%1 DT").arg(fee, 0, 'f', 1) : "-");
        priceItem->setTextAlignment(Qt::AlignCenter);
        m_sessionsTable->setItem(r, 8, priceItem);

        QWidget *actions = new QWidget();
        actions->setStyleSheet("background:transparent;");
        QHBoxLayout *al = new QHBoxLayout(actions);
        al->setContentsMargins(4,4,4,4);
        QPushButton *delBtn = makeActionBtn(QString::fromUtf8("\xf0\x9f\x97\x91\xef\xb8\x8f"), RED, "#fde8e8", actions);
        delBtn->setToolTip("Delete session");
        connect(delBtn, &QPushButton::clicked, this, [this, id]() { deleteSession(id); });
        al->addWidget(delBtn);
        m_sessionsTable->setCellWidget(r, 9, actions);
    }

    if (m_kpiSessions) {
        m_kpiSessions->setText(QString::number(sessions.size()));
        if (m_kpiSessionsSub)
            m_kpiSessionsSub->setText(QString::fromUtf8("%1 passed").arg(totalOk));
    }
    if (m_kpiTaux && !sessions.isEmpty()) {
        int pct = totalOk * 100 / sessions.size();
        m_kpiTaux->setText(QString("%1%").arg(pct));
        m_kpiTaux->setStyleSheet(QString("QLabel{font-size:22px;font-weight:bold;color:%1;"
            "background:transparent;border:none;}").arg(pct >= 70 ? GREEN : (pct >= 40 ? ORANGE : RED)));
    }

    // Timeline on dashboard
    if (m_timelineLayout) {
        while (m_timelineLayout->count() > 0) {
            QLayoutItem *it = m_timelineLayout->takeAt(0);
            if (it->widget()) delete it->widget();
            delete it;
        }

        int count = qMin(8, sessions.size());
        for (int i = 0; i < count; i++) {
            const auto &s = sessions[i];
            bool ok = s["reussi"].toInt() == 1;
            QFrame *row = new QFrame();
            row->setStyleSheet("QFrame{background:#f8f9fa;border-radius:8px;border:none;}");
            row->setFixedHeight(40);
            QHBoxLayout *rl = new QHBoxLayout(row);
            rl->setContentsMargins(12,4,12,4); rl->setSpacing(8);

            QLabel *ic = new QLabel(ok ? QString::fromUtf8("✅") : QString::fromUtf8("❌"), row);
            ic->setStyleSheet("QLabel{font-size:12px;background:transparent;border:none;}");
            rl->addWidget(ic);
            QLabel *who = new QLabel(QString("%1 %2").arg(s["prenom"].toString(), s["nom"].toString()), row);
            who->setStyleSheet(QString("QLabel{font-size:11px;font-weight:bold;color:%1;background:transparent;border:none;}").arg(TXT));
            rl->addWidget(who);
            QLabel *what = new QLabel(s["manoeuvre_type"].toString(), row);
            what->setStyleSheet(QString("QLabel{font-size:10px;color:%1;background:transparent;border:none;}").arg(DIM));
            rl->addWidget(what);
            rl->addStretch();
            QLabel *when = new QLabel(s["date_debut"].toString().left(16), row);
            when->setStyleSheet(QString("QLabel{font-size:9px;color:%1;background:transparent;border:none;}").arg(DIM));
            rl->addWidget(when);

            m_timelineLayout->addWidget(row);
        }
        if (sessions.isEmpty()) {
            QLabel *empty = new QLabel(QString::fromUtf8("No sessions recorded"));
            empty->setAlignment(Qt::AlignCenter);
            empty->setStyleSheet(QString("QLabel{font-size:11px;color:%1;padding:20px;background:transparent;border:none;}").arg(DIM));
            m_timelineLayout->addWidget(empty);
        }
    }

    // Top students on dashboard
    if (m_topStudentsLayout) {
        while (m_topStudentsLayout->count() > 0) {
            QLayoutItem *it = m_topStudentsLayout->takeAt(0);
            if (it->widget()) delete it->widget();
            delete it;
        }

        auto eleves = ParkingDBManager::instance().getAllEleves();
        struct StudentRank { int id; QString name; double taux; int sessions; };
        QList<StudentRank> ranked;
        for (const auto &e : eleves) {
            int eid = e["id"].toInt();
            int sc = ParkingDBManager::instance().getSessionCount(eid);
            if (sc == 0) continue;
            double tx = ParkingDBManager::instance().getTauxReussite(eid);
            ranked.append({eid, QString("%1 %2").arg(e["prenom"].toString(), e["nom"].toString()), tx, sc});
        }
        std::sort(ranked.begin(), ranked.end(), [](const StudentRank &a, const StudentRank &b) {
            return a.taux > b.taux;
        });

        QStringList medals = {QString::fromUtf8("🥇"), QString::fromUtf8("🥈"), QString::fromUtf8("🥉"), "4.", "5."};
        for (int i = 0; i < qMin(5, ranked.size()); i++) {
            QFrame *row = new QFrame();
            row->setStyleSheet("QFrame{background:#f8f9fa;border-radius:8px;border:none;}");
            row->setFixedHeight(36);
            QHBoxLayout *rl = new QHBoxLayout(row);
            rl->setContentsMargins(10,4,10,4); rl->setSpacing(8);
            QLabel *medal = new QLabel(medals[i], row);
            medal->setFixedWidth(24);
            medal->setStyleSheet("QLabel{font-size:14px;background:transparent;border:none;}");
            rl->addWidget(medal);
            QLabel *name = new QLabel(ranked[i].name, row);
            name->setStyleSheet(QString("QLabel{font-size:11px;font-weight:bold;color:%1;background:transparent;border:none;}").arg(TXT));
            rl->addWidget(name, 1);
            QLabel *tx = new QLabel(QString("%1%").arg(ranked[i].taux, 0, 'f', 0), row);
            tx->setStyleSheet(QString("QLabel{font-size:11px;font-weight:bold;color:%1;background:transparent;border:none;}").arg(GREEN));
            rl->addWidget(tx);
            QLabel *sc = new QLabel(QString("(%1 sess.)").arg(ranked[i].sessions), row);
            sc->setStyleSheet(QString("QLabel{font-size:9px;color:%1;background:transparent;border:none;}").arg(DIM));
            rl->addWidget(sc);
            m_topStudentsLayout->addWidget(row);
        }
        if (ranked.isEmpty()) {
            QLabel *empty = new QLabel(QString::fromUtf8("No data"));
            empty->setAlignment(Qt::AlignCenter);
            empty->setStyleSheet(QString("QLabel{font-size:11px;color:%1;padding:20px;background:transparent;border:none;}").arg(DIM));
            m_topStudentsLayout->addWidget(empty);
        }
    }
}

void AdminWidget::deleteSession(int id)
{
    if (QMessageBox::question(this, "Delete",
            QString::fromUtf8("Delete session #%1 ?").arg(id),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        ParkingDBManager::instance().deleteSession(id);
        
        refreshAll();
    }
}

// ═══════════════════════════════════════════════════════════════
//  RÉSERVATIONS PAGE
// ═══════════════════════════════════════════════════════════════

QWidget* AdminWidget::createReservationsPage()
{
    QWidget *page = new QWidget(this);
    page->setObjectName("resPage");
    page->setStyleSheet(QString("QWidget#resPage{background:%1;}").arg(BG));
    QVBoxLayout *lay = new QVBoxLayout(page);
    lay->setContentsMargins(28,16,28,16); lay->setSpacing(14);

    m_reservationsTable = new QTableWidget(page);
    m_reservationsTable->setColumnCount(8);
    m_reservationsTable->setHorizontalHeaderLabels({
        "ID", QString::fromUtf8("Student"), QString::fromUtf8("Vehicle"),
        "Date", QString::fromUtf8("Slot"), "Status", "Montant", "Actions"
    });
    m_reservationsTable->setStyleSheet(tableCSS());
    m_reservationsTable->horizontalHeader()->setStretchLastSection(true);
    m_reservationsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_reservationsTable->verticalHeader()->setVisible(false);
    m_reservationsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_reservationsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_reservationsTable->setShowGrid(false);
    m_reservationsTable->verticalHeader()->setDefaultSectionSize(42);
    m_reservationsTable->setAlternatingRowColors(true);
    lay->addWidget(m_reservationsTable, 1);

    return page;
}

void AdminWidget::refreshReservationsTable()
{
    int instrId = -1;
    if (m_userRole == "Moniteur") {
        instrId = ParkingDBManager::instance().validateMoniteurLogin(m_userName);
    }
    QString queryStr = "SELECT r.REQUEST_ID AS id, r.STUDENT_ID AS eleve_id, "
                "r.CAR_ID AS vehicule_id, "
                "TO_CHAR(r.REQUESTED_DATE,'YYYY-MM-DD') AS date_examen, "
                "r.TIME_SLOT AS creneau, r.AMOUNT AS montant_paye, "
                "r.STATUS AS statut, "
                "e.first_name AS prenom, e.last_name AS nom, "
                "NVL(c.brand,' ') || ' ' || NVL(c.model,' ') AS modele "
                "FROM EXAM_REQUEST r "
                "LEFT JOIN STUDENTS e ON r.STUDENT_ID=e.id "
                "LEFT JOIN CARS c ON r.CAR_ID=c.id "
                "WHERE r.EXAM_STEP=3 ";
    if (instrId > 0) {
        queryStr += QString(" AND r.INSTRUCTOR_ID = %1 ").arg(instrId);
    }
    queryStr += " ORDER BY r.REQUESTED_DATE DESC";

    QSqlQuery q(queryStr);

    QList<QVariantMap> res;
    while (q.next()) {
        QVariantMap m;
        for (int i = 0; i < q.record().count(); i++)
            m[q.record().fieldName(i).toLower()] = q.value(i);
        res.append(m);
    }

    m_reservationsTable->setRowCount(res.size());

    for (int r = 0; r < res.size(); r++) {
        const auto &rv = res[r];
        int id = rv["id"].toInt();
        QString statut = rv["statut"].toString();

        m_reservationsTable->setItem(r, 0, new QTableWidgetItem(QString::number(id)));
        m_reservationsTable->setItem(r, 1, new QTableWidgetItem(
            QString("%1 %2").arg(rv["prenom"].toString(), rv["nom"].toString())));
        m_reservationsTable->setItem(r, 2, new QTableWidgetItem(rv["modele"].toString()));
        m_reservationsTable->setItem(r, 3, new QTableWidgetItem(rv["date_examen"].toString()));
        m_reservationsTable->setItem(r, 4, new QTableWidgetItem(rv["creneau"].toString()));

        QString sLabel = (statut == "PENDING" || statut == "en_attente") ? QString::fromUtf8("⏳ Pending") :
                         (statut == "APPROVED" || statut == "confirmee") ? QString::fromUtf8("✅ Confirmed") :
                         (statut == "CANCELLED" || statut == "annulee") ? QString::fromUtf8("❌ Cancelled") : statut;
        QTableWidgetItem *si = new QTableWidgetItem(sLabel);
        si->setForeground(QColor((statut == "APPROVED" || statut == "confirmee") ? GREEN :
                                 (statut == "CANCELLED" || statut == "annulee") ? RED : ORANGE));
        m_reservationsTable->setItem(r, 5, si);
        m_reservationsTable->setItem(r, 6, new QTableWidgetItem(
            QString("%1 DT").arg(rv["montant_paye"].toDouble(), 0, 'f', 1)));

        QWidget *actions = new QWidget();
        actions->setStyleSheet("background:transparent;");
        QHBoxLayout *al = new QHBoxLayout(actions);
        al->setContentsMargins(4,4,4,4); al->setSpacing(6);

        if (statut == "PENDING" || statut == "en_attente") {
            QPushButton *ok = makeActionBtn(QString::fromUtf8("\xe2\x9c\x85"), GREEN, "#e8f8f5", actions);
            ok->setToolTip("Confirm reservation");
            connect(ok, &QPushButton::clicked, this, [this, id]() { updateReservationStatut(id, "APPROVED"); });
            al->addWidget(ok);
            QPushButton *no = makeActionBtn(QString::fromUtf8("\xf0\x9f\x9a\xab"), ORANGE, "#fef0ec", actions);
            no->setToolTip("Cancel reservation");
            connect(no, &QPushButton::clicked, this, [this, id]() { updateReservationStatut(id, "CANCELLED"); });
            al->addWidget(no);
        }
        QPushButton *del = makeActionBtn(QString::fromUtf8("\xf0\x9f\x97\x91\xef\xb8\x8f"), RED, "#fde8e8", actions);
        del->setToolTip("Delete reservation");
        connect(del, &QPushButton::clicked, this, [this, id]() { deleteReservation(id); });
        al->addWidget(del);
        m_reservationsTable->setCellWidget(r, 7, actions);
    }

    // Revenue KPI
    if (m_kpiRevenu) {
        double total = 0;
        QSqlQuery rq("SELECT NVL(SUM(c.session_fee),0) FROM PARKING_SESSIONS s JOIN CARS c ON s.car_id=c.id");
        if (rq.next()) total = rq.value(0).toDouble();
        m_kpiRevenu->setText(QString("%1 DT").arg(total, 0, 'f', 0));
        if (m_kpiRevenuSub)
            m_kpiRevenuSub->setText(QString::fromUtf8("Total earned"));
    }
}

void AdminWidget::updateReservationStatut(int id, const QString &status)
{
    ParkingDBManager::instance().updateReservationStatut(id, status);
    
    refreshAll();
}

void AdminWidget::deleteReservation(int id)
{
    if (QMessageBox::question(this, "Delete", QString::fromUtf8("Delete this reservation ?"),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        ParkingDBManager::instance().deleteReservation(id);
        
        refreshAll();
    }
}

// ═══════════════════════════════════════════════════════════════
//  SEARCH
// ═══════════════════════════════════════════════════════════════

void AdminWidget::onGlobalSearch(const QString &text)
{
    if (text.trimmed().isEmpty()) {
        refreshAll();
        return;
    }
    // Search in eleves table
    for (int r = 0; r < m_elevesTable->rowCount(); r++) {
        bool match = false;
        for (int c = 0; c < m_elevesTable->columnCount() - 1; c++) {
            auto *item = m_elevesTable->item(r, c);
            if (item && item->text().contains(text, Qt::CaseInsensitive)) {
                match = true; break;
            }
        }
        m_elevesTable->setRowHidden(r, !match);
    }
    // If on dashboard, switch to eleves page
    if (m_pages->currentIndex() == 0) navigateTo(2);
}

// ═══════════════════════════════════════════════════════════════
//  REFRESH ALL
// ═══════════════════════════════════════════════════════════════

void AdminWidget::refreshAll()
{
    refreshMoniteursTable();
    refreshElevesTable();
    refreshVehiculesTable();
    refreshSessionsTable();
    refreshReservationsTable();
    refreshVideosTable();
    refreshExamSessionsTable();
    refreshExamSlotsTable();
    refreshManeuverStepsTable();
    refreshExamResultsTable();
}

// ═══════════════════════════════════════════════════════════════
//  VIDEOS PAGE — Liens YouTube stockés dans PARKING_MANEUVER_STEPS
// ═══════════════════════════════════════════════════════════════

static const QStringList MANEUVER_KEYS = {
    "creneau", "bataille", "epi", "marche_arriere", "demi_tour"
};
static const QStringList MANEUVER_LABELS = {
    QString::fromUtf8("Parallel (Cr\xe9neau)"),
    QString::fromUtf8("Perpendicular (Bataille)"),
    QString::fromUtf8("Diagonal (\xc9pi)"),
    QString::fromUtf8("Reverse (Marche arri\xe8re)"),
    QString::fromUtf8("U-turn (Demi-tour)")
};

// ─────────────────────────────────────────────────────────────
//  PAGE VIDÉOS — lien youtube_url stocké dans PARKING_MANEUVER_STEPS
//  Une ligne par manœuvre, l'admin modifie uniquement le lien
// ─────────────────────────────────────────────────────────────

QWidget* AdminWidget::createVideosPage()
{
    QWidget *page = new QWidget(this);
    page->setObjectName("videosPage");
    page->setStyleSheet(QString("QWidget#videosPage{background:%1;}").arg(BG));
    QVBoxLayout *lay = new QVBoxLayout(page);
    lay->setContentsMargins(28,16,28,16);
    lay->setSpacing(14);

    QFrame *info = new QFrame(page);
    info->setStyleSheet("QFrame{background:#e8f4fd;border:1px solid #0984e3;border-radius:10px;padding:4px;}");
    QHBoxLayout *ihl = new QHBoxLayout(info);
    ihl->setContentsMargins(12,8,12,8);
    QLabel *infoLbl = new QLabel(
        QString::fromUtf8("\xf0\x9f\x8e\xac  Les liens YouTube sont stock\xe9s dans PARKING_MANEUVER_STEPS. "
                          "Cliquez \xab\xc2\xa0\xc9diter\xc2\xa0\xbb pour modifier l'URL d'une man\u0153uvre."), info);
    infoLbl->setStyleSheet("QLabel{font-size:11px;color:#0984e3;background:transparent;border:none;}");
    infoLbl->setWordWrap(true);
    ihl->addWidget(infoLbl);
    lay->addWidget(info);

    m_videosTable = new QTableWidget(page);
    m_videosTable->setColumnCount(3);
    m_videosTable->setHorizontalHeaderLabels({"Maneuver", "YouTube URL", "Action"});
    m_videosTable->setStyleSheet(tableCSS());
    m_videosTable->horizontalHeader()->setStretchLastSection(true);
    m_videosTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_videosTable->verticalHeader()->setVisible(false);
    m_videosTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_videosTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_videosTable->setAlternatingRowColors(true);
    m_videosTable->setShowGrid(false);
    m_videosTable->verticalHeader()->setDefaultSectionSize(46);
    lay->addWidget(m_videosTable, 1);

    return page;
}

void AdminWidget::refreshVideosTable()
{
    if (!m_videosTable) return;

    // Construit la liste depuis PARKING_MANEUVER_STEPS (ou fallback sur les 5 clés)
    auto dbRows = ParkingDBManager::instance().getAllManeuverVideoUrls();

    // Indexer par maneuver_type pour lookup rapide
    QMap<QString, QString> urlMap;
    for (const auto &row : dbRows)
        urlMap[row["maneuver_type"].toString()] = row["youtube_url"].toString();

    m_videosTable->setRowCount(MANEUVER_KEYS.size());

    for (int r = 0; r < MANEUVER_KEYS.size(); r++) {
        const QString &key   = MANEUVER_KEYS[r];
        const QString &label = MANEUVER_LABELS[r];
        QString url = urlMap.value(key, "");

        m_videosTable->setItem(r, 0, new QTableWidgetItem(label));
        auto *urlItem = new QTableWidgetItem(url.isEmpty() ? QString::fromUtf8("— aucun lien —") : url);
        urlItem->setForeground(QColor(url.isEmpty() ? "#b2bec3" : "#0984e3"));
        m_videosTable->setItem(r, 1, urlItem);

        QWidget *cell = new QWidget();
        cell->setStyleSheet("background:transparent;");
        QHBoxLayout *al = new QHBoxLayout(cell);
        al->setContentsMargins(4,4,4,4);
        QPushButton *editBtn = makeActionBtn(
            QString::fromUtf8("\xe2\x9c\x8f\xef\xb8\x8f Edit URL"), BLUE, "#e8f4fd", cell);
        connect(editBtn, &QPushButton::clicked, this, [this, key]() { showEditVideoDialog(key); });
        al->addWidget(editBtn);
        m_videosTable->setCellWidget(r, 2, cell);
    }
}

// showAddVideoDialog est inutilisé (on édite directement la ligne)
void AdminWidget::showAddVideoDialog() { /* non utilisé */ }

void AdminWidget::showEditVideoDialog(int /*unused*/) { /* non utilisé */ }

// Version par maneuver_type key
void AdminWidget::showEditVideoDialog(const QString &maneuverKey)
{
    int mIdx = MANEUVER_KEYS.indexOf(maneuverKey);
    QString mLabel = (mIdx >= 0) ? MANEUVER_LABELS[mIdx] : maneuverKey;

    // Récupère l'URL actuelle
    auto rows = ParkingDBManager::instance().getAllManeuverVideoUrls();
    QString currentUrl;
    for (const auto &r : rows)
        if (r["maneuver_type"].toString() == maneuverKey) { currentUrl = r["youtube_url"].toString(); break; }

    QDialog dlg(this);
    dlg.setWindowTitle(QString::fromUtf8("Edit YouTube URL"));
    dlg.setMinimumWidth(520);
    dlg.setStyleSheet("QDialog{background:#f0f2f5;}");

    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    lay->setContentsMargins(24,20,24,20);
    lay->setSpacing(16);

    QLabel *ttl = new QLabel(
        QString::fromUtf8("\xf0\x9f\x8e\xac  ") + mLabel, &dlg);
    ttl->setStyleSheet("QLabel{font-size:16px;font-weight:bold;color:#2d3436;}");
    lay->addWidget(ttl);

    QLabel *hint = new QLabel(
        QString::fromUtf8("Entrez l'URL YouTube qui sera affich\xe9""e aux \xe9l\xe8ves dans l'onglet Parking."), &dlg);
    hint->setStyleSheet("QLabel{font-size:11px;color:#636e72;}");
    hint->setWordWrap(true);
    lay->addWidget(hint);

    QLineEdit *urlEdit = new QLineEdit(currentUrl, &dlg);
    urlEdit->setPlaceholderText("https://www.youtube.com/watch?v=...");
    urlEdit->setStyleSheet(inputCSS());
    lay->addWidget(urlEdit);

    QDialogButtonBox *btns = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
    btns->button(QDialogButtonBox::Save)->setStyleSheet(btnCSS(BLUE, "#0773c5"));
    btns->button(QDialogButtonBox::Cancel)->setStyleSheet(
        "QPushButton{background:#f0f2f5;color:#636e72;border:none;border-radius:10px;padding:10px 20px;font-size:12px;}");
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btns);

    if (dlg.exec() == QDialog::Accepted) {
        bool ok = ParkingDBManager::instance().setManeuverVideoUrl(maneuverKey, urlEdit->text().trimmed());
        if (ok) {
            
            refreshVideosTable();
        } else {
            QMessageBox::warning(this, "Info",
                QString::fromUtf8("Lien sauvegard\xe9. (Si PARKING_MANEUVER_STEPS est vide, "
                                  "le lien sera charg\xe9 au prochain entra\xeenement.)"));
            refreshVideosTable();
        }
    }
}

void AdminWidget::deleteVideo(int /*unused*/) { /* non utilisé : pas de suppression, on met URL vide */ }

// ─────────────────────────────────────────────────────────────
//  PAGE EXAM SESSIONS — CRUD sur EXAM_REQUEST (vue admin)
// ─────────────────────────────────────────────────────────────

QWidget* AdminWidget::createExamSessionsPage()
{
    QWidget *page = new QWidget(this);
    page->setObjectName("examSessionsPage");
    page->setStyleSheet(QString("QWidget#examSessionsPage{background:%1;}").arg(BG));
    QVBoxLayout *lay = new QVBoxLayout(page);
    lay->setContentsMargins(28,16,28,16);
    lay->setSpacing(14);

    QFrame *info = new QFrame(page);
    info->setStyleSheet("QFrame{background:#e8f8f5;border:1px solid #00b894;border-radius:10px;padding:4px;}");
    QHBoxLayout *ihl = new QHBoxLayout(info);
    ihl->setContentsMargins(12,8,12,8);
    QLabel *infoLbl = new QLabel(
        QString::fromUtf8("🗓  Toutes les r\xe9servations d'examen (table EXAM_REQUEST). "
                          "Le moniteur peut cr\xe9er, confirmer, annuler ou supprimer."), info);
    infoLbl->setStyleSheet("QLabel{font-size:11px;color:#00b894;background:transparent;border:none;}");
    infoLbl->setWordWrap(true);
    ihl->addWidget(infoLbl);
    lay->addWidget(info);

    QHBoxLayout *toolbar = new QHBoxLayout();
    QPushButton *addBtn = new QPushButton(QString::fromUtf8("\xe2\x9e\x95 New Exam Request"), page);
    addBtn->setCursor(Qt::PointingHandCursor);
    addBtn->setStyleSheet(btnCSS(GREEN, "#00a884"));
    connect(addBtn, &QPushButton::clicked, this, &AdminWidget::showAddExamSessionDialog);
    toolbar->addWidget(addBtn);

    QPushButton *slotBtn = new QPushButton(QString::fromUtf8("\xf0\x9f\x93\x85 Create Exam Slot"), page);
    slotBtn->setCursor(Qt::PointingHandCursor);
    slotBtn->setStyleSheet(btnCSS(BLUE, "#0773c5"));
    connect(slotBtn, &QPushButton::clicked, this, &AdminWidget::addExamSlotDialog);
    toolbar->addWidget(slotBtn);
    toolbar->addStretch();
    lay->addLayout(toolbar);

    m_examSessionsTable = new QTableWidget(page);
    m_examSessionsTable->setColumnCount(8);
    m_examSessionsTable->setHorizontalHeaderLabels({
        "ID", "Student", "Instructor", "Date", "Time", "Vehicle", "Amount", "Status"
    });
    m_examSessionsTable->setStyleSheet(tableCSS());
    m_examSessionsTable->horizontalHeader()->setStretchLastSection(false);
    m_examSessionsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_examSessionsTable->verticalHeader()->setVisible(false);
    m_examSessionsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_examSessionsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_examSessionsTable->setAlternatingRowColors(true);
    m_examSessionsTable->setShowGrid(false);
    m_examSessionsTable->verticalHeader()->setDefaultSectionSize(42);
    lay->addWidget(m_examSessionsTable, 1);

    // ── Exam Slots sub-section ──
    QLabel *slotsTitle = new QLabel(QString::fromUtf8("\xf0\x9f\x93\x85  Exam Slots (instructor-created)"), page);
    slotsTitle->setStyleSheet("QLabel{font-size:13px;font-weight:bold;color:#2d3436;background:transparent;border:none;margin-top:8px;}");
    lay->addWidget(slotsTitle);

    m_examSlotsTable = new QTableWidget(page);
    m_examSlotsTable->setColumnCount(6);
    m_examSlotsTable->setHorizontalHeaderLabels({"ID","Date","Time","Status","Student","Actions"});
    m_examSlotsTable->setStyleSheet(tableCSS());
    m_examSlotsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_examSlotsTable->verticalHeader()->setVisible(false);
    m_examSlotsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_examSlotsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_examSlotsTable->setAlternatingRowColors(true);
    m_examSlotsTable->setShowGrid(false);
    m_examSlotsTable->verticalHeader()->setDefaultSectionSize(42);
    lay->addWidget(m_examSlotsTable, 1);

    return page;
}

void AdminWidget::refreshExamSessionsTable()
{
    if (!m_examSessionsTable) return;
    int instrId = -1;
    if (m_userRole == "Moniteur") {
        instrId = ParkingDBManager::instance().validateMoniteurLogin(m_userName);
    }
    auto sessions = ParkingDBManager::instance().getAllExamRequests(instrId);

    // Bug 1 fix: configurer les colonnes UNE SEULE FOIS, avant la boucle
    m_examSessionsTable->setColumnCount(9);
    m_examSessionsTable->setHorizontalHeaderLabels({
        "ID","Student","Instructor","Date","Time","Vehicle","Amount","Status","Actions"});
    m_examSessionsTable->setRowCount(sessions.size());

    for (int r = 0; r < sessions.size(); r++) {
        const auto &s = sessions[r];
        int id = s["id"].toInt();
        QString statut = s["statut"].toString();

        m_examSessionsTable->setItem(r, 0, new QTableWidgetItem(QString::number(id)));
        m_examSessionsTable->setItem(r, 1, new QTableWidgetItem(s["student_name"].toString()));
        m_examSessionsTable->setItem(r, 2, new QTableWidgetItem(s["instructor_name"].toString()));
        m_examSessionsTable->setItem(r, 3, new QTableWidgetItem(s["date_examen"].toString()));
        m_examSessionsTable->setItem(r, 4, new QTableWidgetItem(s["creneau"].toString()));
        m_examSessionsTable->setItem(r, 5, new QTableWidgetItem(s["vehicule"].toString()));
        m_examSessionsTable->setItem(r, 6, new QTableWidgetItem(
            QString("%1 DT").arg(s["montant"].toDouble(), 0, 'f', 1)));

        QString sLabel = (statut == "PENDING")   ? QString::fromUtf8("\xe2\x8f\xb3 Pending") :
                         (statut == "APPROVED")  ? QString::fromUtf8("\xe2\x9c\x85 Approved") :
                         (statut == "CANCELLED") ? QString::fromUtf8("\xe2\x9d\x8c Cancelled") : statut;
        auto *si = new QTableWidgetItem(sLabel);
        si->setForeground(QColor(statut == "APPROVED"  ? GREEN :
                                  statut == "CANCELLED" ? RED : ORANGE));
        m_examSessionsTable->setItem(r, 7, si);

        QWidget *actions = new QWidget();
        actions->setStyleSheet("background:transparent;");
        QHBoxLayout *al = new QHBoxLayout(actions);
        al->setContentsMargins(2,2,2,2); al->setSpacing(4);

        if (statut == "PENDING") {
            QPushButton *ok = makeActionBtn(QString::fromUtf8("\xe2\x9c\x85"), GREEN, "#e8f8f5", actions);
            ok->setToolTip("Approve exam request");
            connect(ok, &QPushButton::clicked, this, [this, id]() {
                ParkingDBManager::instance().updateReservationStatut(id, "APPROVED");
                
                refreshExamSessionsTable();
            });
            al->addWidget(ok);
            QPushButton *no = makeActionBtn(QString::fromUtf8("\xf0\x9f\x9a\xab"), ORANGE, "#fef0ec", actions);
            no->setToolTip("Cancel exam request");
            connect(no, &QPushButton::clicked, this, [this, id]() {
                ParkingDBManager::instance().updateReservationStatut(id, "CANCELLED");
                refreshExamSessionsTable();
            });
            al->addWidget(no);
        }
        QPushButton *del = makeActionBtn(QString::fromUtf8("\xf0\x9f\x97\x91\xef\xb8\x8f"), RED, "#fde8e8", actions);
        del->setToolTip("Delete exam request");
        connect(del, &QPushButton::clicked, this, [this, id]() { deleteExamSession(id); });
        al->addWidget(del);
        m_examSessionsTable->setCellWidget(r, 8, actions);
    }
}

void AdminWidget::showAddExamSessionDialog()
{
    // Charge la liste des élèves, moniteurs et véhicules pour les combos
    auto eleves    = ParkingDBManager::instance().getAllEleves();
    auto moniteurs = ParkingDBManager::instance().getAllMoniteurs();
    auto vehicules = ParkingDBManager::instance().getVehiculesDisponibles();

    QDialog dlg(this);
    dlg.setWindowTitle(QString::fromUtf8("New Exam Request"));
    dlg.setMinimumWidth(480);
    dlg.setStyleSheet("QDialog{background:#f0f2f5;}");

    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    lay->setContentsMargins(24,20,24,20);
    lay->setSpacing(14);

    QLabel *ttl = new QLabel(QString::fromUtf8("🗓  Nouvelle r\xe9servation d'examen"), &dlg);
    ttl->setStyleSheet("QLabel{font-size:16px;font-weight:bold;color:#2d3436;}");
    lay->addWidget(ttl);

    auto comboSS = [](){
        return QString("QComboBox{border:1.5px solid #e8e8e8;border-radius:10px;"
            "padding:8px 12px;font-size:12px;background:white;color:#2d3436;}"
            "QComboBox:focus{border-color:#00b894;}");
    };

    QFormLayout *form = new QFormLayout();
    form->setSpacing(10);

    QComboBox *eleveCombo = new QComboBox(&dlg);
    eleveCombo->setStyleSheet(comboSS());
    for (const auto &e : eleves)
        eleveCombo->addItem(
            QString("%1 %2").arg(e["prenom"].toString(), e["nom"].toString()),
            e["id"]);
    form->addRow(QString::fromUtf8("\xc9l\xe8ve :"), eleveCombo);

    QComboBox *moniteurCombo = new QComboBox(&dlg);
    moniteurCombo->setStyleSheet(comboSS());
    for (const auto &m : moniteurs)
        moniteurCombo->addItem(
            QString("%1 %2").arg(m["prenom"].toString(), m["nom"].toString()),
            m["id"]);
    form->addRow("Moniteur :", moniteurCombo);

    QComboBox *vehiculeCombo = new QComboBox(&dlg);
    vehiculeCombo->setStyleSheet(comboSS());
    for (const auto &v : vehicules)
        vehiculeCombo->addItem(
            QString("%1 — %2").arg(v["immatriculation"].toString(), v["modele"].toString()),
            v["id"]);
    form->addRow(QString::fromUtf8("V\xe9hicule :"), vehiculeCombo);

    QDateEdit *dateEdit = new QDateEdit(&dlg);
    dateEdit->setCalendarPopup(true);
    dateEdit->setMinimumDate(QDate::currentDate());
    dateEdit->setDate(QDate::currentDate().addDays(1));
    dateEdit->setDisplayFormat("yyyy-MM-dd");
    dateEdit->setStyleSheet("QDateEdit{border:1.5px solid #e8e8e8;border-radius:10px;"
        "padding:8px 12px;font-size:12px;background:white;color:#2d3436;}");
    form->addRow("Date :", dateEdit);

    QComboBox *creneauCombo = new QComboBox(&dlg);
    creneauCombo->setStyleSheet(comboSS());
    for (const QString &sl : QStringList{"08:00","09:00","10:00","11:00","14:00","15:00","16:00","17:00"})
        creneauCombo->addItem(sl);
    form->addRow(QString::fromUtf8("Cr\xe9neau :"), creneauCombo);

    QDoubleSpinBox *amountSpin = new QDoubleSpinBox(&dlg);
    amountSpin->setRange(0, 9999);
    amountSpin->setValue(0);
    amountSpin->setSuffix(" DT");
    amountSpin->setStyleSheet("QDoubleSpinBox{border:1.5px solid #e8e8e8;border-radius:10px;"
        "padding:8px 12px;font-size:12px;background:white;color:#2d3436;}");
    form->addRow("Montant :", amountSpin);

    lay->addLayout(form);

    QDialogButtonBox *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    btns->button(QDialogButtonBox::Ok)->setText(QString::fromUtf8("Cr\xe9er"));
    btns->button(QDialogButtonBox::Ok)->setStyleSheet(btnCSS(GREEN, "#00a884"));
    btns->button(QDialogButtonBox::Cancel)->setStyleSheet(
        "QPushButton{background:#f0f2f5;color:#636e72;border:none;border-radius:10px;padding:10px 20px;font-size:12px;}");
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btns);

    if (dlg.exec() == QDialog::Accepted) {
        QString dateStr = dateEdit->date().toString("yyyy-MM-dd");
        int eleveId    = eleveCombo->currentData().toInt();
        int moniteurId = moniteurCombo->currentData().toInt();
        int vehiculeId = vehiculeCombo->currentData().toInt();
        if (eleveId <= 0 || moniteurId <= 0 || vehiculeId <= 0) {
            QMessageBox::warning(this, "Error",
                QString::fromUtf8("V\xe9rifiez qu'il y a au moins un \xe9l\xe8ve, un moniteur et un v\xe9hicule."));
            return;
        }
        int newId = ParkingDBManager::instance().addExamRequestAdmin(
            eleveId, moniteurId,
            dateStr,
            creneauCombo->currentText(),
            vehiculeId,
            amountSpin->value());
        if (newId > 0) {
            
            refreshExamSessionsTable();
            refreshReservationsTable(); // sync avec l'onglet Reservations
        } else {
            QString err = ParkingDBManager::instance().getLastError();
            QMessageBox::critical(this, "Error", 
                QString::fromUtf8("Impossible de cr\xe9er la r\xe9servation. V\xe9rifiez la DB.\n\nErreur: %1").arg(err));
        }
    }
}

void AdminWidget::showEditExamSessionDialog(int id)
{
    // Réutilise updateReservationStatut via le bouton dans la table
    Q_UNUSED(id);
}

void AdminWidget::deleteExamSession(int id)
{
    if (QMessageBox::question(this, "Delete Exam Request",
            QString::fromUtf8("Supprimer cette r\xe9servation d'examen ?"),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        ParkingDBManager::instance().deleteReservation(id);
        
        refreshExamSessionsTable();
        refreshReservationsTable();
    }
}

void AdminWidget::addExamSlotDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QString::fromUtf8("Create Exam Slot"));
    dlg.setMinimumWidth(420);
    dlg.setStyleSheet("QDialog{background:#f0f2f5;}");

    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    lay->setContentsMargins(24,20,24,20);
    lay->setSpacing(14);

    QLabel *ttl = new QLabel(QString::fromUtf8("\xf0\x9f\x93\x85  New Exam Slot"), &dlg);
    ttl->setStyleSheet("QLabel{font-size:16px;font-weight:bold;color:#2d3436;}");
    lay->addWidget(ttl);

    QFormLayout *form = new QFormLayout();
    form->setSpacing(10);

    QDateEdit *dateEdit = new QDateEdit(&dlg);
    dateEdit->setCalendarPopup(true);
    dateEdit->setMinimumDate(QDate::currentDate());
    dateEdit->setDate(QDate::currentDate().addDays(1));
    dateEdit->setDisplayFormat("yyyy-MM-dd");
    dateEdit->setStyleSheet("QDateEdit{border:1.5px solid #e8e8e8;border-radius:10px;"
        "padding:8px 12px;font-size:12px;background:white;color:#2d3436;}");
    form->addRow("Date :", dateEdit);

    QComboBox *timeCombo = new QComboBox(&dlg);
    timeCombo->setStyleSheet("QComboBox{border:1.5px solid #e8e8e8;border-radius:10px;"
        "padding:8px 12px;font-size:12px;background:white;color:#2d3436;}"
        "QComboBox:focus{border-color:#0984e3;}");
    for (const QString &sl : QStringList{
            "08:00-09:00","09:00-10:00","10:00-11:00","11:00-12:00",
            "14:00-15:00","15:00-16:00","16:00-17:00","17:00-18:00"})
        timeCombo->addItem(sl);
    form->addRow("Time Slot :", timeCombo);

    QLineEdit *notesEdit = new QLineEdit(&dlg);
    notesEdit->setPlaceholderText(QString::fromUtf8("Optional notes..."));
    notesEdit->setStyleSheet("QLineEdit{border:1.5px solid #e8e8e8;border-radius:10px;"
        "padding:8px 12px;font-size:12px;background:white;color:#2d3436;}");
    form->addRow("Notes :", notesEdit);
    lay->addLayout(form);

    QDialogButtonBox *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    btns->button(QDialogButtonBox::Ok)->setText(QString::fromUtf8("Create"));
    btns->button(QDialogButtonBox::Ok)->setStyleSheet(btnCSS(BLUE, "#0773c5"));
    btns->button(QDialogButtonBox::Cancel)->setStyleSheet(
        "QPushButton{background:#f0f2f5;color:#636e72;border:none;border-radius:10px;padding:10px 20px;font-size:12px;}");
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btns);

    if (dlg.exec() == QDialog::Accepted) {
        int instrId = ParkingDBManager::instance().validateMoniteurLogin(m_userName);
        if (instrId <= 0) {
            QMessageBox::warning(this, "Error",
                QString::fromUtf8("Instructor not found in DB. Please check your login name."));
            return;
        }
        QString dateStr = dateEdit->date().toString("yyyy-MM-dd");
        QString timeSlot = timeCombo->currentText();
        QString notes = notesEdit->text().trimmed();
        int newId = ParkingDBManager::instance().addExamSlot(instrId, dateStr, timeSlot, notes);
        if (newId > 0) {
            
            refreshExamSlotsTable();
        } else {
            QMessageBox::critical(this, "Error",
                QString::fromUtf8("Failed to create exam slot. Check DB connection."));
        }
    }
}

void AdminWidget::refreshExamSlotsTable()
{
    if (!m_examSlotsTable) return;
    int instrId = ParkingDBManager::instance().validateMoniteurLogin(m_userName);
    QList<QVariantMap> examSlotList;
    if (instrId > 0)
        examSlotList = ParkingDBManager::instance().getExamSlotsForInstructor(instrId);
    else
        examSlotList = ParkingDBManager::instance().getAvailableExamSlots();

    m_examSlotsTable->setRowCount(examSlotList.size());
    for (int r = 0; r < examSlotList.size(); r++) {
        QVariantMap slotMap = examSlotList[r];
        int slotId = slotMap["slot_id"].toInt();
        QString status = slotMap["status"].toString();

        m_examSlotsTable->setItem(r, 0, new QTableWidgetItem(QString::number(slotId)));
        m_examSlotsTable->setItem(r, 1, new QTableWidgetItem(
            slotMap.contains("exam_date_display") ? slotMap["exam_date_display"].toString() : slotMap["exam_date"].toString()));
        m_examSlotsTable->setItem(r, 2, new QTableWidgetItem(slotMap["time_slot"].toString()));

        QString sLabel = (status == "AVAILABLE") ? QString::fromUtf8("\xf0\x9f\x9f\xa2 Available") :
                         (status == "BOOKED")    ? QString::fromUtf8("\xf0\x9f\x94\xb5 Booked") :
                         (status == "CANCELLED") ? QString::fromUtf8("\xe2\x9d\x8c Cancelled") : status;
        auto *si = new QTableWidgetItem(sLabel);
        si->setForeground(QColor(status == "AVAILABLE" ? GREEN :
                                  status == "BOOKED"    ? BLUE : RED));
        m_examSlotsTable->setItem(r, 3, si);
        m_examSlotsTable->setItem(r, 4, new QTableWidgetItem(slotMap["student_name"].toString()));

        QWidget *actions = new QWidget();
        actions->setStyleSheet("background:transparent;");
        QHBoxLayout *al = new QHBoxLayout(actions);
        al->setContentsMargins(2,2,2,2); al->setSpacing(4);

        if (status == "AVAILABLE") {
            QPushButton *cancel = makeActionBtn(QString::fromUtf8("\xe2\x9d\x8c"), RED, "#fde8e8", actions);
            cancel->setToolTip("Cancel this slot");
            connect(cancel, &QPushButton::clicked, this, [this, slotId]() {
                ParkingDBManager::instance().cancelExamSlot(slotId);
                refreshExamSlotsTable();
            });
            al->addWidget(cancel);
        }
        m_examSlotsTable->setCellWidget(r, 5, actions);
    }
}

// ═══════════════════════════════════════════════════════════════
//  MANEUVER STEPS PAGE — CRUD sur PARKING_MANEUVER_STEPS
// ═══════════════════════════════════════════════════════════════

QWidget* AdminWidget::createManeuverStepsPage()
{
    QWidget *page = new QWidget(this);
    page->setObjectName("maneuverStepsPage");
    page->setStyleSheet(QString("QWidget#maneuverStepsPage{background:%1;}").arg(BG));
    QVBoxLayout *lay = new QVBoxLayout(page);
    lay->setContentsMargins(28,16,28,16);
    lay->setSpacing(14);

    QFrame *info = new QFrame(page);
    info->setStyleSheet("QFrame{background:#fff3e0;border:1px solid #e17055;border-radius:10px;padding:4px;}");
    QHBoxLayout *ihl = new QHBoxLayout(info);
    ihl->setContentsMargins(12,8,12,8);
    QLabel *infoLbl = new QLabel(
        QString::fromUtf8("\xf0\x9f…  \xc9tapes de man\u0153uvres (PARKING_MANEUVER_STEPS). "
                          "D\xe9finissez les \xe9tapes pour chacune des 5 man\u0153uvres ATTT. "
                          "Sans \xe9tapes, le syst\xe8me de guidage ne fonctionne pas pour cette man\u0153uvre."), info);
    infoLbl->setStyleSheet("QLabel{font-size:11px;color:#e17055;background:transparent;border:none;}");
    infoLbl->setWordWrap(true);
    ihl->addWidget(infoLbl);
    lay->addWidget(info);

    QHBoxLayout *toolbar = new QHBoxLayout();
    QPushButton *addBtn = new QPushButton(QString::fromUtf8("\xe2\x9e\x95 Add Step"), page);
    addBtn->setCursor(Qt::PointingHandCursor);
    addBtn->setStyleSheet(btnCSS(ORANGE, "#c0604a"));
    connect(addBtn, &QPushButton::clicked, this, &AdminWidget::showAddManeuverStepDialog);
    toolbar->addWidget(addBtn);
    toolbar->addStretch();
    lay->addLayout(toolbar);

    m_maneuverStepsTable = new QTableWidget(page);
    m_maneuverStepsTable->setColumnCount(7);
    m_maneuverStepsTable->setHorizontalHeaderLabels({
        "ID", "Maneuver", "Step#", "Step Name", "Guidance Message", "Stop?", "Actions"
    });
    m_maneuverStepsTable->setStyleSheet(tableCSS());
    m_maneuverStepsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_maneuverStepsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_maneuverStepsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_maneuverStepsTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    m_maneuverStepsTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Fixed);
    m_maneuverStepsTable->setColumnWidth(0, 50);
    m_maneuverStepsTable->setColumnWidth(2, 60);
    m_maneuverStepsTable->setColumnWidth(5, 55);
    m_maneuverStepsTable->setColumnWidth(6, 90);
    m_maneuverStepsTable->verticalHeader()->setVisible(false);
    m_maneuverStepsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_maneuverStepsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_maneuverStepsTable->setAlternatingRowColors(true);
    m_maneuverStepsTable->setShowGrid(false);
    m_maneuverStepsTable->verticalHeader()->setDefaultSectionSize(42);
    lay->addWidget(m_maneuverStepsTable, 1);

    return page;
}

void AdminWidget::refreshManeuverStepsTable()
{
    if (!m_maneuverStepsTable) return;
    auto steps = ParkingDBManager::instance().getAllManeuverSteps();
    m_maneuverStepsTable->setRowCount(steps.size());

    for (int r = 0; r < steps.size(); r++) {
        const auto &s = steps[r];
        int stepId = s["step_id"].toInt();
        QString mType = s["maneuver_type"].toString();

        m_maneuverStepsTable->setItem(r, 0, new QTableWidgetItem(QString::number(stepId)));
        auto *mItem = new QTableWidgetItem(mType);
        mItem->setForeground(QColor(ORANGE));
        m_maneuverStepsTable->setItem(r, 1, mItem);
        m_maneuverStepsTable->setItem(r, 2, new QTableWidgetItem(s["step_number"].toString()));
        m_maneuverStepsTable->setItem(r, 3, new QTableWidgetItem(s["step_name"].toString()));
        m_maneuverStepsTable->setItem(r, 4, new QTableWidgetItem(s["guidance_message"].toString()));

        bool stop = s["is_stop_required"].toInt() == 1;
        auto *stopItem = new QTableWidgetItem(stop ? QString::fromUtf8("\xe2\x9b\x94 Yes") : "No");
        stopItem->setForeground(QColor(stop ? RED : DIM));
        m_maneuverStepsTable->setItem(r, 5, stopItem);

        QWidget *actions = new QWidget();
        actions->setStyleSheet("background:transparent;");
        QHBoxLayout *al = new QHBoxLayout(actions);
        al->setContentsMargins(2,2,2,2); al->setSpacing(4);

        QPushButton *editBtn = makeActionBtn(QString::fromUtf8("\xe2\x9c\x8f\xef\xb8\x8f"), BLUE, "#e8f4fd", actions);
        editBtn->setToolTip("Edit step");
        connect(editBtn, &QPushButton::clicked, this, [this, stepId, mType]() {
            showEditManeuverStepDialog(stepId, mType);
        });
        al->addWidget(editBtn);

        QPushButton *delBtn = makeActionBtn(QString::fromUtf8("\xf0\x9f\x97\x91\xef\xb8\x8f"), RED, "#fde8e8", actions);
        delBtn->setToolTip("Delete step");
        connect(delBtn, &QPushButton::clicked, this, [this, stepId]() {
            deleteManeuverStep(stepId);
        });
        al->addWidget(delBtn);

        m_maneuverStepsTable->setCellWidget(r, 6, actions);
    }
}

void AdminWidget::showAddManeuverStepDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QString::fromUtf8("Add Maneuver Step"));
    dlg.setMinimumWidth(520);
    dlg.setStyleSheet("QDialog{background:#f0f2f5;}");

    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    lay->setContentsMargins(24,20,24,20);
    lay->setSpacing(14);

    QLabel *ttl = new QLabel(QString::fromUtf8("\xf0\x9f…  Nouvelle \xe9tape de man\u0153uvre"), &dlg);
    ttl->setStyleSheet("QLabel{font-size:16px;font-weight:bold;color:#2d3436;}");
    lay->addWidget(ttl);

    auto comboSS = [](){
        return QString("QComboBox{border:1.5px solid #e8e8e8;border-radius:10px;"
            "padding:8px 12px;font-size:12px;background:white;color:#2d3436;}"
            "QComboBox:focus{border-color:#e17055;}");
    };

    QFormLayout *form = new QFormLayout();
    form->setSpacing(10);

    QComboBox *typeCombo = new QComboBox(&dlg);
    typeCombo->setStyleSheet(comboSS());
    for (int i = 0; i < MANEUVER_KEYS.size(); i++)
        typeCombo->addItem(MANEUVER_LABELS[i], MANEUVER_KEYS[i]);
    form->addRow(QString::fromUtf8("Man\u0153uvre :"), typeCombo);

    QSpinBox *stepNumSpin = new QSpinBox(&dlg);
    stepNumSpin->setRange(1, 99);
    stepNumSpin->setValue(1);
    stepNumSpin->setStyleSheet("QSpinBox{border:1.5px solid #e8e8e8;border-radius:10px;"
        "padding:8px 12px;font-size:12px;background:white;color:#2d3436;}");
    form->addRow(QString::fromUtf8("N\xb0 \xe9tape :"), stepNumSpin);

    QLineEdit *nameEdit = new QLineEdit(&dlg);
    nameEdit->setPlaceholderText(QString::fromUtf8("ex: D\xe9marrage"));
    nameEdit->setStyleSheet(inputCSS());
    form->addRow(QString::fromUtf8("Nom :"), nameEdit);

    QLineEdit *sensorEdit = new QLineEdit(&dlg);
    sensorEdit->setPlaceholderText("ex: capteur_droit=obstacle");
    sensorEdit->setStyleSheet(inputCSS());
    form->addRow("Sensor condition :", sensorEdit);

    QLineEdit *guidanceEdit = new QLineEdit(&dlg);
    guidanceEdit->setPlaceholderText(QString::fromUtf8("Message affich\xe9 \xe0 l'\xe9l\xe8ve"));
    guidanceEdit->setStyleSheet(inputCSS());
    form->addRow("Guidance message :", guidanceEdit);

    QLineEdit *audioEdit = new QLineEdit(&dlg);
    audioEdit->setPlaceholderText(QString::fromUtf8("Message audio (optionnel)"));
    audioEdit->setStyleSheet(inputCSS());
    form->addRow("Audio message :", audioEdit);

    QSpinBox *delaySpin = new QSpinBox(&dlg);
    delaySpin->setRange(0, 30000);
    delaySpin->setValue(0);
    delaySpin->setSuffix(" ms");
    delaySpin->setStyleSheet("QSpinBox{border:1.5px solid #e8e8e8;border-radius:10px;"
        "padding:8px 12px;font-size:12px;background:white;color:#2d3436;}");
    form->addRow("Delay before :", delaySpin);

    QComboBox *stopCombo = new QComboBox(&dlg);
    stopCombo->setStyleSheet(comboSS());
    stopCombo->addItem("No", 0);
    stopCombo->addItem(QString::fromUtf8("Yes — Stop required"), 1);
    form->addRow(QString::fromUtf8("Stop requis :"), stopCombo);

    lay->addLayout(form);

    QDialogButtonBox *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    btns->button(QDialogButtonBox::Ok)->setText(QString::fromUtf8("Ajouter"));
    btns->button(QDialogButtonBox::Ok)->setStyleSheet(btnCSS(ORANGE, "#c0604a"));
    btns->button(QDialogButtonBox::Cancel)->setStyleSheet(
        "QPushButton{background:#f0f2f5;color:#636e72;border:none;border-radius:10px;padding:10px 20px;font-size:12px;}");
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btns);

    if (dlg.exec() == QDialog::Accepted) {
        QString mType = typeCombo->currentData().toString();
        int newId = ParkingDBManager::instance().addManeuverStep(
            mType,
            stepNumSpin->value(),
            nameEdit->text().trimmed(),
            sensorEdit->text().trimmed(),
            guidanceEdit->text().trimmed(),
            audioEdit->text().trimmed(),
            delaySpin->value(),
            stopCombo->currentData().toInt() == 1);
        if (newId > 0) {
            
            refreshManeuverStepsTable();
        } else {
            QMessageBox::critical(this, "Error",
                QString::fromUtf8("Impossible d'ajouter l'\xe9tape. V\xe9rifiez que le num\xe9ro d'\xe9tape n'existe pas d\xe9j\xe0 pour cette man\u0153uvre."));
        }
    }
}

void AdminWidget::showEditManeuverStepDialog(int stepId, const QString &maneuverType)
{
    // Load current values from DB
    auto allSteps = ParkingDBManager::instance().getAllManeuverSteps();
    QVariantMap step;
    for (const auto &s : allSteps)
        if (s["step_id"].toInt() == stepId) { step = s; break; }
    if (step.isEmpty()) return;

    QDialog dlg(this);
    dlg.setWindowTitle(QString::fromUtf8("Edit Step — ") + maneuverType);
    dlg.setMinimumWidth(520);
    dlg.setStyleSheet("QDialog{background:#f0f2f5;}");

    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    lay->setContentsMargins(24,20,24,20);
    lay->setSpacing(14);

    QLabel *ttl = new QLabel(
        QString::fromUtf8("\xe2\x9c\x8f\xef\xb8\x8f  Modifier \xe9tape #") +
        step["step_number"].toString() + " — " + maneuverType, &dlg);
    ttl->setStyleSheet("QLabel{font-size:16px;font-weight:bold;color:#2d3436;}");
    lay->addWidget(ttl);

    QFormLayout *form = new QFormLayout();
    form->setSpacing(10);

    QSpinBox *stepNumSpin = new QSpinBox(&dlg);
    stepNumSpin->setRange(1, 99);
    stepNumSpin->setValue(step["step_number"].toInt());
    stepNumSpin->setStyleSheet("QSpinBox{border:1.5px solid #e8e8e8;border-radius:10px;"
        "padding:8px 12px;font-size:12px;background:white;color:#2d3436;}");
    form->addRow(QString::fromUtf8("N\xb0 \xe9tape :"), stepNumSpin);

    QLineEdit *nameEdit = new QLineEdit(step["step_name"].toString(), &dlg);
    nameEdit->setStyleSheet(inputCSS());
    form->addRow(QString::fromUtf8("Nom :"), nameEdit);

    QLineEdit *sensorEdit = new QLineEdit(step["sensor_condition"].toString(), &dlg);
    sensorEdit->setStyleSheet(inputCSS());
    form->addRow("Sensor condition :", sensorEdit);

    QLineEdit *guidanceEdit = new QLineEdit(step["guidance_message"].toString(), &dlg);
    guidanceEdit->setStyleSheet(inputCSS());
    form->addRow("Guidance message :", guidanceEdit);

    QLineEdit *audioEdit = new QLineEdit(step["audio_message"].toString(), &dlg);
    audioEdit->setStyleSheet(inputCSS());
    form->addRow("Audio message :", audioEdit);

    QSpinBox *delaySpin = new QSpinBox(&dlg);
    delaySpin->setRange(0, 30000);
    delaySpin->setValue(step["delay_before_ms"].toInt());
    delaySpin->setSuffix(" ms");
    delaySpin->setStyleSheet("QSpinBox{border:1.5px solid #e8e8e8;border-radius:10px;"
        "padding:8px 12px;font-size:12px;background:white;color:#2d3436;}");
    form->addRow("Delay before :", delaySpin);

    auto comboSS = [](){
        return QString("QComboBox{border:1.5px solid #e8e8e8;border-radius:10px;"
            "padding:8px 12px;font-size:12px;background:white;color:#2d3436;}"
            "QComboBox:focus{border-color:#e17055;}");
    };
    QComboBox *stopCombo = new QComboBox(&dlg);
    stopCombo->setStyleSheet(comboSS());
    stopCombo->addItem("No", 0);
    stopCombo->addItem(QString::fromUtf8("Yes — Stop required"), 1);
    stopCombo->setCurrentIndex(step["is_stop_required"].toInt() == 1 ? 1 : 0);
    form->addRow(QString::fromUtf8("Stop requis :"), stopCombo);

    lay->addLayout(form);

    QDialogButtonBox *btns = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
    btns->button(QDialogButtonBox::Save)->setStyleSheet(btnCSS(BLUE, "#0773c5"));
    btns->button(QDialogButtonBox::Cancel)->setStyleSheet(
        "QPushButton{background:#f0f2f5;color:#636e72;border:none;border-radius:10px;padding:10px 20px;font-size:12px;}");
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btns);

    if (dlg.exec() == QDialog::Accepted) {
        bool ok = ParkingDBManager::instance().updateManeuverStep(
            stepId,
            stepNumSpin->value(),
            nameEdit->text().trimmed(),
            sensorEdit->text().trimmed(),
            guidanceEdit->text().trimmed(),
            audioEdit->text().trimmed(),
            delaySpin->value(),
            stopCombo->currentData().toInt() == 1);
        if (ok) {
            
            refreshManeuverStepsTable();
        } else {
            QMessageBox::critical(this, "Error",
                QString::fromUtf8("Impossible de modifier l'\xe9tape. V\xe9rifiez qu'il n'y a pas de conflit de num\xe9ro."));
        }
    }
}

void AdminWidget::deleteManeuverStep(int stepId)
{
    if (QMessageBox::question(this, "Delete Step",
            QString::fromUtf8("Supprimer cette \xe9tape de man\u0153uvre ? "
                              "Cela peut affecter le guidage en temps r\xe9el."),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        bool ok = ParkingDBManager::instance().deleteManeuverStep(stepId);
        if (ok) {
            
            refreshManeuverStepsTable();
        } else {
            QMessageBox::critical(this, "Error", QString::fromUtf8("Impossible de supprimer l'\xe9tape."));
        }
    }
}

// ═══════════════════════════════════════════════════════════════
//  EXAM RESULTS PAGE — vue admin sur PARKING_EXAM_RESULTS
// ═══════════════════════════════════════════════════════════════

QWidget* AdminWidget::createExamResultsPage()
{
    QWidget *page = new QWidget(this);
    page->setObjectName("examResultsPage");
    page->setStyleSheet(QString("QWidget#examResultsPage{background:%1;}").arg(BG));
    QVBoxLayout *lay = new QVBoxLayout(page);
    lay->setContentsMargins(28,16,28,16);
    lay->setSpacing(14);

    QFrame *info = new QFrame(page);
    info->setStyleSheet("QFrame{background:#fef9e7;border:1px solid #fdcb6e;border-radius:10px;padding:4px;}");
    QHBoxLayout *ihl = new QHBoxLayout(info);
    ihl->setContentsMargins(12,8,12,8);
    QLabel *infoLbl = new QLabel(
        QString::fromUtf8("\xf0\x9f\x8f\x86  R\xe9sultats d'examen (PARKING_EXAM_RESULTS). "
                          "G\xe9n\xe9r\xe9s automatiquement lors des examens en mode parking. "
                          "L'admin peut consulter et supprimer des r\xe9sultats erron\xe9s."), info);
    infoLbl->setStyleSheet("QLabel{font-size:11px;color:#b7950b;background:transparent;border:none;}");
    infoLbl->setWordWrap(true);
    ihl->addWidget(infoLbl);
    lay->addWidget(info);

    m_examResultsTable = new QTableWidget(page);
    m_examResultsTable->setColumnCount(8);
    m_examResultsTable->setHorizontalHeaderLabels({
        "ID", "Student", "Maneuver", "Date", "Duration", "Score/20", "Errors", "Actions"
    });
    m_examResultsTable->setStyleSheet(tableCSS());
    m_examResultsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_examResultsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_examResultsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    m_examResultsTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    m_examResultsTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Fixed);
    m_examResultsTable->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Fixed);
    m_examResultsTable->setColumnWidth(0, 50);
    m_examResultsTable->setColumnWidth(4, 80);
    m_examResultsTable->setColumnWidth(5, 75);
    m_examResultsTable->setColumnWidth(6, 60);
    m_examResultsTable->setColumnWidth(7, 70);
    m_examResultsTable->verticalHeader()->setVisible(false);
    m_examResultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_examResultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_examResultsTable->setAlternatingRowColors(true);
    m_examResultsTable->setShowGrid(false);
    m_examResultsTable->verticalHeader()->setDefaultSectionSize(42);
    lay->addWidget(m_examResultsTable, 1);

    return page;
}

void AdminWidget::refreshExamResultsTable()
{
    if (!m_examResultsTable) return;
    auto results = ParkingDBManager::instance().getAllExamResults();
    m_examResultsTable->setRowCount(results.size());

    for (int r = 0; r < results.size(); r++) {
        const auto &res = results[r];
        int resultId = res["id"].toInt();
        bool reussi  = res["is_successful"].toInt() == 1;
        double score = res["score_out_of_20"].toDouble();

        m_examResultsTable->setItem(r, 0, new QTableWidgetItem(QString::number(resultId)));
        m_examResultsTable->setItem(r, 1, new QTableWidgetItem(res["student_name"].toString()));
        m_examResultsTable->setItem(r, 2, new QTableWidgetItem(res["maneuver_type"].toString()));
        m_examResultsTable->setItem(r, 3, new QTableWidgetItem(res["exam_date"].toString()));

        int dur = res["duration_seconds"].toInt();
        m_examResultsTable->setItem(r, 4, new QTableWidgetItem(
            QString("%1m%2s").arg(dur / 60).arg(dur % 60, 2, 10, QChar('0'))));

        auto *scoreItem = new QTableWidgetItem(QString::number(score, 'f', 1));
        scoreItem->setForeground(QColor(reussi ? GREEN : RED));
        m_examResultsTable->setItem(r, 5, scoreItem);

        m_examResultsTable->setItem(r, 6, new QTableWidgetItem(res["error_count"].toString()));

        QWidget *actions = new QWidget();
        actions->setStyleSheet("background:transparent;");
        QHBoxLayout *al = new QHBoxLayout(actions);
        al->setContentsMargins(4,4,4,4);
        QPushButton *delBtn = makeActionBtn(QString::fromUtf8("\xf0\x9f\x97\x91\xef\xb8\x8f"), RED, "#fde8e8", actions);
        delBtn->setToolTip("Delete this exam result");
        connect(delBtn, &QPushButton::clicked, this, [this, resultId]() {
            deleteExamResult(resultId);
        });
        al->addWidget(delBtn);
        m_examResultsTable->setCellWidget(r, 7, actions);
    }
}

void AdminWidget::deleteExamResult(int resultId)
{
    if (QMessageBox::question(this, "Delete Exam Result",
            QString::fromUtf8("Supprimer ce r\xe9sultat d'examen ? Cette action est irr\xe9versible."),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        bool ok = ParkingDBManager::instance().deleteExamResult(resultId);
        if (ok) {
            
            refreshExamResultsTable();
        } else {
            QMessageBox::critical(this, "Error", QString::fromUtf8("Impossible de supprimer le r\xe9sultat."));
        }
    }
}
