#include "bookingsession.h"
#include "thememanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QDateTime>
#include <QScrollArea>
#include <QGraphicsDropShadowEffect>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QVariant>
#include <QDebug>
#include <QApplication>

BookingSession::BookingSession(QWidget *parent) :
    QWidget(parent)
{
    // Connect to theme manager
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, &BookingSession::onThemeChanged);
    
    setupUI();
}

BookingSession::~BookingSession()
{
}

void BookingSession::setupUI()
{
    ThemeManager* theme = ThemeManager::instance();
    bool isDark = theme->currentTheme() == ThemeManager::Dark;
    
    // embedded widget — no window title / modal needed
    
    // Main dialog widget
    this->setObjectName("BookingSessionDialog");
    
    QPalette pal = palette();
    this->setPalette(pal);
    this->setAutoFillBackground(true);
    
    dialogWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Scroll area
    QString scrollBg = theme->backgroundColor();
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setStyleSheet(
        QString("QScrollArea { border: none; background-color: %1; }"
        "QScrollBar:vertical { background-color: %1; width: 10px; }"
        "QScrollBar::handle:vertical { background-color: %2; border-radius: 5px; }")
        .arg(scrollBg, isDark ? "#475569" : "#9CA3AF")
    );
    
    QWidget *contentWidget = new QWidget();
    contentWidget->setStyleSheet(QString("QWidget { background-color: %1; }").arg(scrollBg));
    QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(40, 30, 40, 30);
    contentLayout->setSpacing(25);
    
    // Current stage info card
    QFrame *stageInfo = new QFrame();
    stageInfo->setStyleSheet(
        QString("QFrame {"
        "    background-color: %1;"
        "    border-radius: 12px;"
        "    border-left: 4px solid #3B82F6;"
        "}").arg(theme->cardColor())
    );
    
    QGraphicsDropShadowEffect *stageShadow = new QGraphicsDropShadowEffect();
    stageShadow->setBlurRadius(15);
    stageShadow->setXOffset(0);
    stageShadow->setYOffset(3);
    stageShadow->setColor(QColor(0, 0, 0, 10));
    stageInfo->setGraphicsEffect(stageShadow);
    
    QVBoxLayout *stageLayout = new QVBoxLayout(stageInfo);
    stageLayout->setContentsMargins(25, 20, 25, 20);
    stageLayout->setSpacing(8);
    
    // Fetch student progress
    int studentId = qApp->property("currentUserId").toInt();
    if (studentId == 0) return; // Strict: no fallback
    
    int currentStep = 1;
    QSqlQuery qProg;
    qProg.prepare("SELECT current_step FROM WINO_PROGRESS WHERE user_id = :uid");
    qProg.bindValue(":uid", studentId);
    if(qProg.exec() && qProg.next()){
        currentStep = qProg.value(0).toInt();
    }
    
    QString stageName = "Code";
    if (currentStep == 2) stageName = "Circuit";
    else if (currentStep >= 3) stageName = "Parking";

    QLabel *stageTitle = new QLabel(QString("ℹ️ Current Stage: %1").arg(stageName));
    stageTitle->setStyleSheet(
        "QLabel {"
        "    font-weight: bold;"
        "    color: #1E40AF;"
        "    font-size: 15px;"
        "    line-height: 1.5;"
        "    padding: 3px 0;"
        "}"
    );
    
    QLabel *stageNote = new QLabel("Only sessions for your current stage are available");
    stageNote->setStyleSheet(
        "QLabel {"
        "    color: #3B82F6;"
        "    font-size: 13px;"
        "    line-height: 1.5;"
        "    padding: 3px 0;"
        "}"
    );
    
    stageLayout->addWidget(stageTitle);
    stageLayout->addWidget(stageNote);
    
    contentLayout->addWidget(stageInfo);
    
    // Session Type Section
    QLabel *typeLabel = new QLabel("Session Type");
    typeLabel->setStyleSheet(
        QString("QLabel {"
        "    font-size: 16px;"
        "    font-weight: bold;"
        "    color: %1;"
        "    margin-top: 10px;"
        "    line-height: 1.5;"
        "    padding: 5px 0;"
        "}").arg(theme->primaryTextColor())
    );
    contentLayout->addWidget(typeLabel);
    
    QHBoxLayout *typeLayout = new QHBoxLayout();
    typeLayout->setSpacing(18);
    
    typeLayout->addWidget(createSessionTypeButton("📖 Code", 
        currentStep > 1 ? "Already completed" : "Current stage", 
        currentStep == 1));
        
    typeLayout->addWidget(createSessionTypeButton("🚗 Circuit", 
        currentStep > 2 ? "Already completed" : (currentStep == 2 ? "Current stage" : "Complete Code first"), 
        currentStep == 2));
        
    typeLayout->addWidget(createSessionTypeButton("🅿️ Parking", 
        currentStep >= 3 ? "Current stage" : "Complete Circuit first", 
        currentStep == 3));
    
    contentLayout->addLayout(typeLayout);
    
    // Date & Time Section - In modern card
    QFrame *dateTimeCard = new QFrame();
    dateTimeCard->setStyleSheet(
        QString("QFrame {"
        "    background-color: %1;"
        "    border-radius: 12px;"
        "}").arg(theme->cardColor())
    );
    
    QGraphicsDropShadowEffect *dtShadow = new QGraphicsDropShadowEffect();
    dtShadow->setBlurRadius(15);
    dtShadow->setXOffset(0);
    dtShadow->setYOffset(3);
    dtShadow->setColor(QColor(0, 0, 0, 10));
    dateTimeCard->setGraphicsEffect(dtShadow);
    
    QVBoxLayout *dtCardLayout = new QVBoxLayout(dateTimeCard);
    dtCardLayout->setContentsMargins(25, 25, 25, 25);
    dtCardLayout->setSpacing(20);
    
    QLabel *dtTitle = new QLabel("📅 Schedule Details");
    dtTitle->setStyleSheet(
        QString("QLabel {"
        "    font-size: 16px;"
        "    font-weight: bold;"
        "    color: %1;"
        "    line-height: 1.5;"
        "    padding: 5px 0;"
        "}").arg(theme->primaryTextColor())
    );
    dtCardLayout->addWidget(dtTitle);
    
    QHBoxLayout *dateTimeLayout = new QHBoxLayout();
    dateTimeLayout->setSpacing(20);
    
    // Date
    QVBoxLayout *dateLayout = new QVBoxLayout();
    dateLayout->setSpacing(10);
    QString labelColor = theme->secondaryTextColor();
    QString inputBg = isDark ? "#1E293B" : "#F9FAFB";
    QString inputColor = theme->primaryTextColor();
    QString inputBorder = theme->borderColor();
    QString inputFocusBg = theme->cardColor();
    
    QLabel *dateLabel = new QLabel("Date");
    dateLabel->setStyleSheet(
        QString("QLabel {"
        "    font-size: 14px;"
        "    font-weight: 600;"
        "    color: %1;"
        "    line-height: 1.5;"
        "    padding: 3px 0;"
        "}").arg(labelColor)
    );
    
    QDateEdit *dateEdit = new QDateEdit();
    dateEdit->setObjectName("dateEdit");
    dateEdit->setDate(QDate::currentDate().addDays(2));
    dateEdit->setMinimumDate(QDate::currentDate());
    dateEdit->setCalendarPopup(true);
    dateEdit->setMinimumHeight(48);
    dateEdit->setStyleSheet(
        QString("QDateEdit {"
        "    padding: 12px 16px;"
        "    border: 2px solid %1;"
        "    border-radius: 10px;"
        "    font-size: 14px;"
        "    background-color: %2;"
        "    color: %3;"
        "    line-height: 1.5;"
        "}"
        "QDateEdit:focus {"
        "    border-color: #14B8A6;"
        "    background-color: %4;"
        "}"
        "QDateEdit::drop-down { border: none; padding-right: 12px; }")
        .arg(inputBorder, inputBg, inputColor, inputFocusBg)
    );
    
    dateLayout->addWidget(dateLabel);
    dateLayout->addWidget(dateEdit);
    
    // Time
    QVBoxLayout *timeLayout = new QVBoxLayout();
    timeLayout->setSpacing(10);
    QLabel *timeLabel = new QLabel("Time");
    timeLabel->setStyleSheet(
        QString("QLabel {"
        "    font-size: 14px;"
        "    font-weight: 600;"
        "    color: %1;"
        "    line-height: 1.5;"
        "    padding: 3px 0;"
        "}").arg(labelColor)
    );
    
    // Hidden combo — only used as value store (onProceedToPayment reads from it)
    QComboBox *timeCombo = new QComboBox(this);
    timeCombo->setObjectName("timeCombo");
    timeCombo->hide();

    // Visual time-slot chips panel
    QWidget *timeSlotsWidget = new QWidget();
    timeSlotsWidget->setObjectName("timeSlotsWidget");
    timeSlotsWidget->setMinimumHeight(48);
    timeSlotsWidget->setStyleSheet("background:transparent;");
    QHBoxLayout *slotsLayout = new QHBoxLayout(timeSlotsWidget);
    slotsLayout->setContentsMargins(0, 4, 0, 4);
    slotsLayout->setSpacing(10);
    QLabel *initLbl = new QLabel(QString::fromUtf8("  \xe2\x8f\xb3  Loading slots\xe2\x80\xa6"));
    initLbl->setStyleSheet(QString("color:%1;font-size:13px;background:transparent;").arg(isDark ? "#94A3B8" : "#6B7280"));
    slotsLayout->addWidget(initLbl);
    slotsLayout->addStretch();

    timeLayout->addWidget(timeLabel);
    timeLayout->addWidget(timeSlotsWidget);
    
    dateTimeLayout->addLayout(dateLayout, 1);
    dateTimeLayout->addLayout(timeLayout, 1);
    
    dtCardLayout->addLayout(dateTimeLayout);
    
    contentLayout->addWidget(dateTimeCard);
    
    // Duration & Instructor Card
    QFrame *diCard = new QFrame();
    diCard->setStyleSheet(
        QString("QFrame {"
        "    background-color: %1;"
        "    border-radius: 12px;"
        "}").arg(theme->cardColor())
    );
    
    QGraphicsDropShadowEffect *diShadow = new QGraphicsDropShadowEffect();
    diShadow->setBlurRadius(15);
    diShadow->setXOffset(0);
    diShadow->setYOffset(3);
    diShadow->setColor(QColor(0, 0, 0, 10));
    diCard->setGraphicsEffect(diShadow);
    
    QVBoxLayout *diCardLayout = new QVBoxLayout(diCard);
    diCardLayout->setContentsMargins(25, 25, 25, 25);
    diCardLayout->setSpacing(20);
    
    QLabel *diTitle = new QLabel("⚙️ Session Configuration");
    diTitle->setStyleSheet(
        QString("QLabel {"
        "    font-size: 16px;"
        "    font-weight: bold;"
        "    color: %1;"
        "    line-height: 1.5;"
        "    padding: 5px 0;"
        "}").arg(theme->primaryTextColor())
    );
    diCardLayout->addWidget(diTitle);
    
    QHBoxLayout *durationInstructorLayout = new QHBoxLayout();
    durationInstructorLayout->setSpacing(20);
    
    // Duration
    QVBoxLayout *durationLayout = new QVBoxLayout();
    durationLayout->setSpacing(10);
    QLabel *durationLabel = new QLabel("Duration");
    durationLabel->setStyleSheet(
        QString("QLabel {"
        "    font-size: 14px;"
        "    font-weight: 600;"
        "    color: %1;"
        "    line-height: 1.5;"
        "    padding: 3px 0;"
        "}").arg(labelColor)
    );
    
    // comboStyle — used by instructorCombo below
    QString comboStyle = QString(
        "QComboBox {"
        "    padding: 12px 16px;"
        "    border: 2px solid %1;"
        "    border-radius: 10px;"
        "    font-size: 14px;"
        "    background-color: %2;"
        "    color: %3;"
        "    line-height: 1.5;"
        "}"
        "QComboBox:focus { border-color: #14B8A6; background-color: %4; }"
        "QComboBox::drop-down { border: none; padding-right: 12px; }"
        "QComboBox QAbstractItemView {"
        "    border: 1px solid %1;"
        "    background-color: %4;"
        "    color: %3;"
        "    selection-background-color: %5;"
        "    selection-color: %3;"
        "    padding: 5px;"
        "}"
        "QComboBox:disabled { color: #6B7280; background-color: %2; border-color: %1; }"
    ).arg(inputBorder, inputBg, inputColor, inputFocusBg, isDark ? "#134E4A" : "#ECFDF5");

    // Hidden combo — keeps currentIndexChanged signal wiring intact
    QComboBox *durationCombo = new QComboBox(this);
    durationCombo->setObjectName("durationCombo");
    durationCombo->addItems({"60 minutes", "90 minutes", "120 minutes"});
    durationCombo->hide();

    // Duration chip buttons (60 / 90 / 120 min)
    QWidget *durChipsWidget = new QWidget();
    durChipsWidget->setMinimumHeight(48);
    durChipsWidget->setStyleSheet("background:transparent;");
    QHBoxLayout *durChipsLay = new QHBoxLayout(durChipsWidget);
    durChipsLay->setContentsMargins(0, 4, 0, 4);
    durChipsLay->setSpacing(10);

    struct DurOpt { QString label; int idx; };
    const QList<DurOpt> durOpts = {
        { "60 min",  0 },
        { "90 min",  1 },
        { "120 min", 2 },
    };

    QString durChipSS = isDark
        ? "QPushButton{background:#1E293B;color:#5EEAD4;"
          "border:2px solid #14B8A6;border-radius:20px;"
          "padding:7px 20px;font-size:14px;font-weight:600;}"
          "QPushButton:checked{background:#14B8A6;color:white;}"
          "QPushButton:hover:!checked{background:#0F3A36;}"
        : "QPushButton{background:white;color:#14B8A6;"
          "border:2px solid #14B8A6;border-radius:20px;"
          "padding:7px 20px;font-size:14px;font-weight:600;}"
          "QPushButton:checked{background:#14B8A6;color:white;}"
          "QPushButton:hover:!checked{background:#E6FFFA;}";

    for (const DurOpt &opt : durOpts) {
        QPushButton *chip = new QPushButton(opt.label, durChipsWidget);
        chip->setCheckable(true);
        chip->setChecked(opt.idx == 0);   // 60 min selected by default
        chip->setFixedHeight(40);
        chip->setCursor(Qt::PointingHandCursor);
        chip->setStyleSheet(durChipSS);

        int chipIdx = opt.idx;
        connect(chip, &QPushButton::clicked, this, [chip, chipIdx, durChipsWidget, durationCombo]() {
            for (QPushButton *btn : durChipsWidget->findChildren<QPushButton*>())
                btn->setChecked(btn == chip);
            durationCombo->setCurrentIndex(chipIdx);  // fires currentIndexChanged
        });
        durChipsLay->addWidget(chip);
    }
    durChipsLay->addStretch();

    durationLayout->addWidget(durationLabel);
    durationLayout->addWidget(durChipsWidget);
    
    // Instructor
    QVBoxLayout *instructorLayout = new QVBoxLayout();
    instructorLayout->setSpacing(10);
    QLabel *instructorLabel = new QLabel("Instructor", this);
    instructorLabel->hide();
    instructorLabel->setStyleSheet(
        QString("QLabel {"
        "    font-size: 14px;"
        "    font-weight: 600;"
        "    color: %1;"
        "    line-height: 1.5;"
        "    padding: 3px 0;"
        "}").arg(labelColor)
    );
    
    QComboBox *instructorCombo = new QComboBox(this);
    instructorCombo->hide();
    instructorCombo->setObjectName("instructorCombo");
    
    // Fetch EXACT Instructor assigned to this student from DB
    QSqlQuery queryInst;
    int firstInstructorId = 0;
    
    queryInst.prepare(
        "SELECT i.id, i.full_name "
        "FROM INSTRUCTORS i "
        "JOIN STUDENTS s ON s.instructor_id = i.id "
        "WHERE s.id = :student_id"
    );
    queryInst.bindValue(":student_id", studentId);
    if (queryInst.exec()) {
        while (queryInst.next()) {
            int id = queryInst.value(0).toInt();
            QString name = queryInst.value(1).toString();
            instructorCombo->addItem(name + " ⭐4.8", QVariant(id));
            if (firstInstructorId == 0) firstInstructorId = id;
        }
    }
    
    if (firstInstructorId == 0) {
        // Fallback UI (but booking might fail without real instructor)
        instructorCombo->addItems({"Mohamed Karim ⭐4.8", "Fatma Mansour ⭐4.9", "Ali Gharbi ⭐4.7"});
        instructorCombo->setItemData(0, QVariant(1)); // Danger: assuming ID 1 exists
        instructorCombo->setItemData(1, QVariant(2));
        instructorCombo->setItemData(2, QVariant(3));
    }
    
    // Disable it so the student cannot change their assigned instructor
    instructorCombo->setEnabled(false);
    
    instructorCombo->setMinimumHeight(48);
    instructorCombo->setStyleSheet(comboStyle);
    instructorLayout->addWidget(instructorLabel);
    instructorLayout->addWidget(instructorCombo);
    
    durationInstructorLayout->addLayout(durationLayout, 1);
    if (currentStep > 1) {
        instructorLabel->show();
        instructorCombo->show();
        durationInstructorLayout->addLayout(instructorLayout, 1);
    } else {
        // For Code sessions, we might still need an instructor_id in the DB.
        // We ensure instructorId 1 (typically a generic staff or the first instructor) is used silently.
        if (instructorCombo->count() > 0) {
            instructorCombo->setCurrentIndex(0); 
        }
        
        QLabel *codeInfo = new QLabel("ℹ️ The premises are open for theory sessions\n"
                                      "from Monday to Saturday, 08:00 to 18:00.\n"
                                      "(No dedicated instructor required)");
        codeInfo->setStyleSheet(
            "QLabel {"
            "    color: #0369A1;"
            "    font-size: 13px;"
            "    font-weight: bold;"
            "    background-color: #E0F2FE;"
            "    padding: 12px;"
            "    border-radius: 8px;"
            "    border: 1px solid #BAE6FD;"
            "}"
        );
        codeInfo->setAlignment(Qt::AlignCenter);
        durationInstructorLayout->addWidget(codeInfo, 1);
    }
    
    diCardLayout->addLayout(durationInstructorLayout);
    
    contentLayout->addWidget(diCard);
    
    // Cost Summary Card
    QFrame *costFrame = new QFrame();
    costFrame->setStyleSheet(
        QString("QFrame {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 %1, stop:1 %2);"
        "    border-radius: 12px;"
        "    border: 2px solid #14B8A6;"
        "}").arg(isDark ? "#134E4A" : "#ECFDF5", isDark ? "#115E59" : "#D1FAE5")
    );
    
    QGraphicsDropShadowEffect *costShadow = new QGraphicsDropShadowEffect();
    costShadow->setBlurRadius(15);
    costShadow->setXOffset(0);
    costShadow->setYOffset(3);
    costShadow->setColor(QColor(20, 184, 166, 30));
    costFrame->setGraphicsEffect(costShadow);
    
    QHBoxLayout *costLayout = new QHBoxLayout(costFrame);
    costLayout->setContentsMargins(30, 25, 30, 25);
    
    QLabel *costLabel = new QLabel("💰 Session Cost:");
    costLabel->setStyleSheet(
        "QLabel {"
        "    font-size: 16px;"
        "    color: #065F46;"
        "    font-weight: bold;"
        "    line-height: 1.5;"
        "    padding: 3px 0;"
        "}"
    );
    
    double baseRate = (currentStep == 1) ? 20.0 : 40.0;
    QLabel *costValue = new QLabel(QString::number(baseRate, 'f', 0) + " TND");
    costValue->setObjectName("costValue");
    costValue->setStyleSheet(
        "QLabel {"
        "    font-size: 32px;"
        "    color: #047857;"
        "    font-weight: bold;"
        "    line-height: 1.3;"
        "    padding: 5px 0;"
        "}"
    );
    
    costLayout->addWidget(costLabel);
    costLayout->addStretch();
    costLayout->addWidget(costValue);
    
    contentLayout->addWidget(costFrame);

    // ── Confirm Booking button (inside content, no separate footer bar) ────────
    QPushButton *proceedBtn = new QPushButton(QString::fromUtf8("  \xe2\x9c\x94  Confirm Booking"));
    proceedBtn->setFixedHeight(52);
    proceedBtn->setCursor(Qt::PointingHandCursor);
    proceedBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #14B8A6;"
        "    color: #ffffff;"
        "    font-size: 16px;"
        "    font-weight: bold;"
        "    border: none;"
        "    border-radius: 12px;"
        "    padding: 0 40px;"
        "}"
        "QPushButton:hover { background-color: #0d9488; }"
        "QPushButton:pressed { background-color: #0f766e; }");
    connect(proceedBtn, &QPushButton::clicked, this, &BookingSession::onProceedToPayment);

    // Right-align the button
    QHBoxLayout *confirmRow = new QHBoxLayout();
    confirmRow->setContentsMargins(0, 12, 0, 8);
    confirmRow->addStretch();
    confirmRow->addWidget(proceedBtn);
    contentLayout->addLayout(confirmRow);

    contentLayout->addStretch();

    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea);
    
    // Connect signals to update availability
    connect(dateEdit, &QDateEdit::dateChanged, this, &BookingSession::updateAvailableTimes);
    connect(instructorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BookingSession::updateAvailableTimes);
    connect(durationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BookingSession::updateAvailableTimes);
    
    connect(durationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [=]() {
        int minSelected = durationCombo->currentText().split(" ").first().toInt();
        double hourlyRate = (currentStep == 1) ? 20.0 : 40.0;
        
        if (minSelected == 60) costValue->setText(QString::number(hourlyRate, 'f', 0) + " TND");
        else if (minSelected == 90) costValue->setText(QString::number(hourlyRate * 1.5, 'f', 0) + " TND");
        else if (minSelected == 120) costValue->setText(QString::number(hourlyRate * 2.0, 'f', 0) + " TND");
        else costValue->setText(QString::number((minSelected / 60.0) * hourlyRate, 'f', 0) + " TND");
    });
    
    // Apply initial theme colors
    updateColors();
    
    // Initial population
    updateAvailableTimes();
}

QWidget* BookingSession::createSessionTypeButton(const QString& type, const QString& status, bool enabled)
{
    ThemeManager* theme = ThemeManager::instance();
    
    QFrame *button = new QFrame();
    
    if (enabled) {
        button->setStyleSheet(
            QString("QFrame {"
            "    border-radius: 12px;"
            "    background-color: %1;"
            "    border: 3px solid #14B8A6;"
            "}").arg(theme->cardColor())
        );
    } else {
        button->setStyleSheet(
            QString("QFrame {"
            "    border-radius: 12px;"
            "    background-color: %1;"
            "    border: 2px solid %2;"
            "    opacity: 0.7;"
            "}").arg(theme->cardColor(), theme->borderColor())
        );
    }
    
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(enabled ? 20 : 10);
    shadow->setXOffset(0);
    shadow->setYOffset(enabled ? 5 : 2);
    shadow->setColor(QColor(0, 0, 0, enabled ? 15 : 8));
    button->setGraphicsEffect(shadow);
    
    button->setMinimumHeight(130);
    button->setMinimumWidth(240);
    
    QVBoxLayout *layout = new QVBoxLayout(button);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(12);
    layout->setContentsMargins(20, 20, 20, 20);
    
    QLabel *typeLabel = new QLabel(type);
    typeLabel->setStyleSheet(
        QString("QLabel {"
                "    font-size: 17px;"
                "    font-weight: bold;"
                "    color: %1;"
                "    line-height: 1.5;"
                "    padding: 5px 0;"
                "}").arg(enabled ? "#0F766E" : "#6B7280")
    );
    typeLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(typeLabel);
    
    if (!status.isEmpty()) {
        QLabel *statusLabel = new QLabel(status);
        
        QString badgeStyle;
        if (enabled) {
            badgeStyle = 
                "QLabel {"
                "    background-color: #ECFDF5;"
                "    color: #059669;"
                "    font-size: 12px;"
                "    font-weight: 600;"
                "    padding: 6px 12px;"
                "    border-radius: 6px;"
                "    line-height: 1.5;"
                "}";
        } else {
            badgeStyle = 
                "QLabel {"
                "    background-color: #F3F4F6;"
                "    color: #6B7280;"
                "    font-size: 12px;"
                "    font-weight: 500;"
                "    padding: 6px 12px;"
                "    border-radius: 6px;"
                "    line-height: 1.5;"
                "}";
        }
        
        statusLabel->setStyleSheet(badgeStyle);
        statusLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(statusLabel);
    }
    
    return button;
}

void BookingSession::onCancel()
{
    emit backRequested();
}

void BookingSession::updateAvailableTimes()
{
    QDateEdit* dateEdit = findChild<QDateEdit*>("dateEdit");
    QComboBox* instructorCombo = findChild<QComboBox*>("instructorCombo");
    QComboBox* durationCombo  = findChild<QComboBox*>("durationCombo");
    QComboBox* timeCombo      = findChild<QComboBox*>("timeCombo");
    QWidget*   slotsWidget    = findChild<QWidget*>("timeSlotsWidget");

    if (!dateEdit || !timeCombo || !instructorCombo || !durationCombo || !slotsWidget) return;

    // ── Clear chips and hidden combo ────────────────────────────────────
    timeCombo->clear();
    QLayout *lay = slotsWidget->layout();
    if (lay) {
        while (lay->count()) {
            QLayoutItem *item = lay->takeAt(0);
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
    }

    ThemeManager *tm = ThemeManager::instance();
    bool isDark = tm->currentTheme() == ThemeManager::Dark;

    // Helper: show a "no slots" label
    auto showNoSlots = [&](const QString &msg = {}) {
        QLabel *lbl = new QLabel(msg.isEmpty() ? "No slots available" : msg, slotsWidget);
        lbl->setStyleSheet("color:#DC2626;font-size:13px;font-weight:600;"
                           "padding:8px 0;background:transparent;");
        qobject_cast<QHBoxLayout*>(lay)->addWidget(lbl);
        qobject_cast<QHBoxLayout*>(lay)->addStretch();
    };

    QDate date     = dateEdit->date();
    int studentId  = qApp->property("currentUserId").toInt();
    if (studentId == 0) { showNoSlots(); return; }

    int currentStep = 1;
    QSqlQuery qProg(QSqlDatabase::database());
    qProg.prepare("SELECT current_step FROM WINO_PROGRESS WHERE user_id = :uid");
    qProg.bindValue(":uid", studentId);
    if (qProg.exec() && qProg.next()) currentStep = qProg.value(0).toInt();

    QVector<int> availHours;

    if (currentStep == 1) {
        // Code stage: fixed Monday–Saturday 08–17
        if (date.dayOfWeek() == 7) { showNoSlots("Closed on Sunday"); return; }
        for (int h = 8; h <= 17; h++) availHours.append(h);
    } else {
        int instId = instructorCombo->currentData().toInt();
        if (instId == 0) { showNoSlots(); return; }

        static const char* dayNames[] =
            {"", "MONDAY","TUESDAY","WEDNESDAY","THURSDAY","FRIDAY","SATURDAY","SUNDAY"};
        QString dayStr = dayNames[qBound(1, date.dayOfWeek(), 7)];

        // Instructor's availability for this day
        QSet<int> avail;
        QSqlQuery q(QSqlDatabase::database());
        q.prepare("SELECT start_time FROM AVAILABILITY "
                  "WHERE instructor_id = :id AND day_of_week = :d AND is_active = 1");
        q.bindValue(":id", instId);
        q.bindValue(":d", dayStr);
        if (q.exec()) {
            while (q.next())
                avail.insert(q.value(0).toString().left(2).toInt());
        }

        // Already-booked slots
        QSet<int> booked;
        QSqlQuery qb(QSqlDatabase::database());
        qb.prepare("SELECT start_time, end_time FROM SESSION_BOOKING "
                   "WHERE instructor_id = :id AND TRUNC(session_date) = :sd "
                   "AND status IN ('CONFIRMED','COMPLETED')");
        qb.bindValue(":id", instId);
        qb.bindValue(":sd", date);
        if (qb.exec()) {
            while (qb.next()) {
                int sh = qb.value(0).toString().left(2).toInt();
                int eh = qb.value(1).toString().left(2).toInt();
                for (int i = sh; i < eh; i++) booked.insert(i);
                if (sh == eh) booked.insert(sh);
            }
        }

        // A slot is available if the instructor marked that start hour
        // (no consecutive-block check — session duration is handled by end_time in DB)
        for (int h = 8; h <= 17; h++) {
            if (avail.contains(h) && !booked.contains(h))
                availHours.append(h);
        }
    }

    if (availHours.isEmpty()) { showNoSlots(); return; }

    // ── Build chip buttons ───────────────────────────────────────────────
    QString chipSS = isDark
        ? "QPushButton{"
          "background:#134E4A;color:#5EEAD4;"
          "border:2px solid #14B8A6;border-radius:20px;"
          "padding:7px 22px;font-size:14px;font-weight:600;}"
          "QPushButton:checked{"
          "background:#14B8A6;color:white;border-color:#14B8A6;}"
          "QPushButton:hover:!checked{background:#0F3A36;}"
        : "QPushButton{"
          "background:white;color:#14B8A6;"
          "border:2px solid #14B8A6;border-radius:20px;"
          "padding:7px 22px;font-size:14px;font-weight:600;}"
          "QPushButton:checked{"
          "background:#14B8A6;color:white;border-color:#14B8A6;}"
          "QPushButton:hover:!checked{background:#E6FFFA;}";

    QHBoxLayout *slotsLay = qobject_cast<QHBoxLayout*>(lay);
    bool firstChip = true;
    for (int h : availHours) {
        QString label = QString("%1:00").arg(h, 2, 10, QChar('0'));
        timeCombo->addItem(label);

        QPushButton *chip = new QPushButton(label, slotsWidget);
        chip->setCheckable(true);
        chip->setChecked(firstChip);
        chip->setFixedHeight(40);
        chip->setCursor(Qt::PointingHandCursor);
        chip->setStyleSheet(chipSS);

        connect(chip, &QPushButton::clicked, this, [this, chip, label, slotsWidget]() {
            // mutual exclusion
            for (QPushButton *btn : slotsWidget->findChildren<QPushButton*>())
                btn->setChecked(btn == chip);
            // sync hidden combo
            if (QComboBox *tc = findChild<QComboBox*>("timeCombo"))
                tc->setCurrentText(label);
        });

        slotsLay->addWidget(chip);
        firstChip = false;
    }
    slotsLay->addStretch();

    // Pre-select first slot in hidden combo
    if (timeCombo->count() > 0) timeCombo->setCurrentIndex(0);
}

void BookingSession::onProceedToPayment()
{
    // Retrieve input values using findChild
    QDateEdit* dateEdit = findChild<QDateEdit*>("dateEdit");
    QComboBox* durationCombo = findChild<QComboBox*>("durationCombo");
    QComboBox* instructorCombo = findChild<QComboBox*>("instructorCombo");
    QComboBox* timeCombo = findChild<QComboBox*>("timeCombo");
    
    if (!dateEdit || !timeCombo || !durationCombo || !instructorCombo) {
        QMessageBox::critical(this, "Error", "Could not read booking details form!");
        return;
    }
    
    QDate date = dateEdit->date();
    QString timeStr = timeCombo->currentText();
    if (timeStr.isEmpty() || timeCombo->count() == 0) {
        QMessageBox::warning(this, "Unavailable", "Please select a valid time slot.");
        return;
    }
    QTime startTime = QTime::fromString(timeStr, "HH:mm");
    int minSelected = durationCombo->currentText().split(" ").first().toInt();
    QTime endTime = startTime.addSecs(minSelected * 60);
    int instructorId = instructorCombo->currentData().toInt();
    
    // Safety fallback
    if (instructorId == 0) instructorId = 1; 
    
    int studentId = qApp->property("currentUserId").toInt();
    if (studentId == 0) return;
    
    int currentStep = 1;
    QSqlQuery qProg;
    qProg.prepare("SELECT current_step FROM WINO_PROGRESS WHERE user_id = :uid");
    qProg.bindValue(":uid", studentId);
    if(qProg.exec() && qProg.next()){
        currentStep = qProg.value(0).toInt();
    }
    
    // Calculate amount based on duration and step
    double hourlyRate = (currentStep == 1) ? 20.0 : 40.0;
    double amount = (minSelected / 60.0) * hourlyRate;
    if (studentId == 0) return; // Strict: no fallback
    
    // Check if student credit exceeds 200 DT
    double totalDue = 0.0;
    QSqlQuery qCredit;
    qCredit.prepare(
        "SELECT COALESCE(SUM(AMOUNT), 0) FROM SESSION_BOOKING "
        "WHERE STUDENT_ID = :sid AND STATUS IN ('CONFIRMED', 'COMPLETED')");
    qCredit.bindValue(":sid", studentId);
    if (qCredit.exec() && qCredit.next()) {
        totalDue = qCredit.value(0).toDouble();
    }

    QSqlQuery qExamFee;
    qExamFee.prepare(
        "SELECT COALESCE(SUM(AMOUNT), 0) FROM EXAM_REQUEST "
        "WHERE STUDENT_ID = :sid AND STATUS IN ('APPROVED', 'COMPLETED')");
    qExamFee.bindValue(":sid", studentId);
    if (qExamFee.exec() && qExamFee.next()) {
        totalDue += qExamFee.value(0).toDouble();
    }

    double totalPaid = 0.0;
    QSqlQuery qPaid;
    qPaid.prepare(
        "SELECT COALESCE(SUM(AMOUNT), 0) FROM PAYMENT "
        "WHERE STUDENT_ID = :sid AND STATUS IN ('CONFIRMED', 'PENDING', 'PAID', 'VALID', 'SUCCESS', 'ACCEPTED', 'VERIFIED', 'COMPLETED')");
    qPaid.bindValue(":sid", studentId);
    if (qPaid.exec() && qPaid.next())
        totalPaid = qPaid.value(0).toDouble();

    double currentDebt = totalDue - totalPaid;
    if (currentDebt > 200) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Access Denied");
        msgBox.setText("⚠️ Credit limit reached");
        msgBox.setInformativeText(QString("Sorry, you cannot book a new session. Your current outstanding balance is <b>%1 TND</b>, which exceeds the allowed limit of 200 TND.<br><br>Please settle your balance by making a payment from your dashboard in order to make new bookings.").arg(currentDebt, 0, 'f', 0));
        msgBox.setIcon(QMessageBox::Warning);
        ThemeManager* theme = ThemeManager::instance();
        msgBox.setStyleSheet(
            QString("QMessageBox { background-color: %1; }"
            "QLabel { color: %2; font-size: 14px; line-height: 1.5; }"
            "QPushButton { background-color: #14B8A6; color: white; font-weight: bold; border: none; border-radius: 6px; padding: 10px 20px; }"
            "QPushButton:hover { background-color: #0F9D8E; }")
            .arg(theme->cardColor(), theme->primaryTextColor())
        );
        msgBox.exec();
        return;
    }

    // Step naturally cascades to session booking query from previous fetch

    // Insert into DB
    QSqlQuery insertQuery;
    insertQuery.prepare("INSERT INTO SESSION_BOOKING (STUDENT_ID, INSTRUCTOR_ID, SESSION_STEP, SESSION_DATE, START_TIME, END_TIME, AMOUNT, STATUS) "
                        "VALUES (:student, :instructor, :step, :date, :start, :end, :amount, 'CONFIRMED')");
    insertQuery.bindValue(":student", studentId); 
    insertQuery.bindValue(":instructor", instructorId);
    insertQuery.bindValue(":step", currentStep);
    insertQuery.bindValue(":date", date);
    insertQuery.bindValue(":start", startTime.toString("HH:mm"));
    insertQuery.bindValue(":end", endTime.toString("HH:mm"));
    insertQuery.bindValue(":amount", amount);
    
    if (!insertQuery.exec()) {
        QMessageBox::critical(this, "Database Error", "Failed to book session: " + insertQuery.lastError().text());
        return;
    }

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Booking Confirmed");
    msgBox.setText("✓ Your session has been booked successfully!");
    if (currentStep == 1) {
        msgBox.setInformativeText("Your Code session has been scheduled. You can manage it from your dashboard calendar.");
    } else {
        msgBox.setInformativeText("You can manage this session from your dashboard calendar.");
    }
    msgBox.setIcon(QMessageBox::Information);
    ThemeManager* theme = ThemeManager::instance();
    msgBox.setStyleSheet(
        QString("QMessageBox {"
        "    background-color: %1;"
        "}"
        "QLabel {"
        "    color: %2;"
        "    font-size: 14px;"
        "    line-height: 1.5;"
        "    padding: 3px 0;"
        "}"
        "QPushButton {"
        "    background-color: #14B8A6;"
        "    color: white;"
        "    font-weight: bold;"
        "    border: none;"
        "    border-radius: 6px;"
        "    padding: 10px 24px;"
        "    min-width: 80px;"
        "    line-height: 1.5;"
        "}"
        "QPushButton:hover {"
        "    background-color: #0F9D8E;"
        "}").arg(theme->cardColor(), theme->primaryTextColor())
    );
    msgBox.exec();
    
    emit sessionBooked(); // notify dashboard to refresh calendar
    emit backRequested(); // return to dashboard after booking
}

void BookingSession::updateColors()
{
    ThemeManager* theme = ThemeManager::instance();
    
    // Update dialog background
    this->setStyleSheet(
        QString("QDialog#BookingSessionDialog { background-color: %1; }").arg(theme->backgroundColor())
    );
}

void BookingSession::onThemeChanged()
{
    updateColors();
}
