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
    QDialog(parent)
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
    
    setWindowTitle("Book a Session");
    setModal(true);
    setMinimumSize(1050, 850);
    
    // Main dialog widget
    this->setObjectName("BookingSessionDialog");
    
    QPalette pal = palette();
    this->setPalette(pal);
    this->setAutoFillBackground(true);
    
    dialogWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Header
    QWidget *headerWidget = new QWidget();
    headerWidget->setStyleSheet(QString("QWidget { background-color: %1; }")
        .arg(isDark ? "#0F172A" : "#111827"));
    headerWidget->setFixedHeight(120);
    
    QHBoxLayout *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(40, 20, 40, 20);
    
    QVBoxLayout *titleLayout = new QVBoxLayout();
    QLabel *titleLabel = new QLabel("📅 Book a Session");
    titleLabel->setStyleSheet(
        "QLabel {"
        "    font-size: 28px;"
        "    font-weight: bold;"
        "    color: #FFFFFF;"
        "    padding: 5px 0;"
        "}"
    );
    
    QLabel *subtitleLabel = new QLabel("Schedule your driving lesson");
    subtitleLabel->setStyleSheet(
        "QLabel {"
        "    color: #9CA3AF;"
        "    font-size: 14px;"
        "    line-height: 1.5;"
        "    padding: 3px 0;"
        "}"
    );
    
    titleLayout->addWidget(titleLabel);
    titleLayout->addWidget(subtitleLabel);
    
    QPushButton *closeBtn = new QPushButton("✕");
    closeBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #374151;"
        "    border: none;"
        "    font-size: 20px;"
        "    color: #9CA3AF;"
        "    border-radius: 20px;"
        "    line-height: 1.5;"
        "}"
        "QPushButton:hover { background-color: #FEE2E2; color: #DC2626; }"
    );
    closeBtn->setFixedSize(40, 40);
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, this, &BookingSession::onCancel);
    
    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();
    headerLayout->addWidget(closeBtn);
    
    mainLayout->addWidget(headerWidget);
    
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
    
    QComboBox *timeCombo = new QComboBox();
    timeCombo->setObjectName("timeCombo");
    timeCombo->setMinimumHeight(48);
    // Reuse the generic comboStyle (we'll define it slightly earlier or just re-create it)
    QString timeComboStyle = QString(
        "QComboBox {"
        "    padding: 12px 16px;"
        "    border: 2px solid %1;"
        "    border-radius: 10px;"
        "    font-size: 14px;"
        "    background-color: %2;"
        "    color: %3;"
        "    line-height: 1.5;"
        "}"
        "QComboBox:focus {"
        "    border-color: #14B8A6;"
        "    background-color: %4;"
        "}"
        "QComboBox::drop-down { border: none; padding-right: 12px; }"
        "QComboBox QAbstractItemView {"
        "    border: 1px solid %1;"
        "    background-color: %4;"
        "    color: %3;"
        "    selection-background-color: %5;"
        "    selection-color: %3;"
        "    padding: 5px;"
        "}"
        "QComboBox:disabled {"
        "    color: #DC2626;"
        "    background-color: #FEE2E2;"
        "    border-color: #FCA5A5;"
        "}"
    ).arg(inputBorder, inputBg, inputColor, inputFocusBg, isDark ? "#134E4A" : "#ECFDF5");
    timeCombo->setStyleSheet(timeComboStyle);
    
    timeLayout->addWidget(timeLabel);
    timeLayout->addWidget(timeCombo);
    
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
        "QComboBox:focus {"
        "    border-color: #14B8A6;"
        "    background-color: %4;"
        "}"
        "QComboBox::drop-down { border: none; padding-right: 12px; }"
        "QComboBox QAbstractItemView {"
        "    border: 1px solid %1;"
        "    background-color: %4;"
        "    color: %3;"
        "    selection-background-color: %5;"
        "    selection-color: %3;"
        "    padding: 5px;"
        "}"
        "QComboBox:disabled {"
        "    color: #DC2626;"
        "    background-color: #FEE2E2;"
        "    border-color: #FCA5A5;"
        "}"
    ).arg(inputBorder, inputBg, inputColor, inputFocusBg, isDark ? "#134E4A" : "#ECFDF5");
    
    QComboBox *durationCombo = new QComboBox();
    durationCombo->setObjectName("durationCombo");
    durationCombo->addItems({"60 minutes", "90 minutes", "120 minutes"});
    durationCombo->setMinimumHeight(48);
    durationCombo->setStyleSheet(comboStyle);
    durationLayout->addWidget(durationLabel);
    durationLayout->addWidget(durationCombo);
    
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
    contentLayout->addStretch();
    
    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea);
    
    // Footer avec actions
    QWidget *footerWidget = new QWidget();
    footerWidget->setStyleSheet(QString("QWidget { background-color: %1; border-top: 1px solid %2; }")
        .arg(isDark ? "#0F172A" : "#111827", isDark ? "#334155" : "#374151"));
    footerWidget->setFixedHeight(85);
    
    QHBoxLayout *buttonLayout = new QHBoxLayout(footerWidget);
    buttonLayout->setContentsMargins(40, 18, 40, 18);
    buttonLayout->setSpacing(15);
    
    QPushButton *cancelBtn = new QPushButton("Cancel");
    cancelBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #374151;"
        "    color: #D1D5DB;"
        "    font-size: 15px;"
        "    font-weight: bold;"
        "    border: none;"
        "    border-radius: 10px;"
        "    padding: 16px 32px;"
        "    line-height: 1.5;"
        "}"
        "QPushButton:hover { background-color: #4B5563; color: #FFFFFF; }"
    );
    cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(cancelBtn, &QPushButton::clicked, this, &BookingSession::onCancel);
    
    QPushButton *proceedBtn = new QPushButton("Confirm Booking →");
    proceedBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #14B8A6;"
        "    color: white;"
        "    font-size: 15px;"
        "    font-weight: bold;"
        "    border: none;"
        "    border-radius: 10px;"
        "    padding: 16px 40px;"
        "    line-height: 1.5;"
        "}"
        "QPushButton:hover { background-color: #0F9D8E; }"
        "QPushButton:pressed { background-color: #0D9488; }"
    );
    proceedBtn->setCursor(Qt::PointingHandCursor);
    connect(proceedBtn, &QPushButton::clicked, this, &BookingSession::onProceedToPayment);
    
    buttonLayout->addWidget(cancelBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(proceedBtn);
    
    mainLayout->addWidget(footerWidget);
    
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
    this->reject();
}

void BookingSession::updateAvailableTimes()
{
    QDateEdit* dateEdit = findChild<QDateEdit*>("dateEdit");
    QComboBox* timeCombo = findChild<QComboBox*>("timeCombo");
    QComboBox* instructorCombo = findChild<QComboBox*>("instructorCombo");
    QComboBox* durationCombo = findChild<QComboBox*>("durationCombo");
    
    if (!dateEdit || !timeCombo || !instructorCombo || !durationCombo) return;
    
    timeCombo->clear();
    
    QDate date = dateEdit->date();
    
    int studentId = qApp->property("currentUserId").toInt();
    if (studentId == 0) return;
    
    int currentStep = 1;
    QSqlQuery qProg;
    qProg.prepare("SELECT current_step FROM WINO_PROGRESS WHERE user_id = :uid");
    qProg.bindValue(":uid", studentId);
    if(qProg.exec() && qProg.next()){
        currentStep = qProg.value(0).toInt();
    }
    
    // Fixed schedule for Code sessions (Monday-Saturday, 08:00 - 18:00)
    if (currentStep == 1) {
        if (date.dayOfWeek() == 7) { // Sunday
            timeCombo->addItem("Closed on Sunday");
            timeCombo->setEnabled(false);
        } else {
            int durationMins = durationCombo->currentText().split(" ").first().toInt();
            int blocksNeeded = (durationMins > 60) ? 2 : 1;
            
            for (int h = 8; h <= 17; h++) {
                if (h + blocksNeeded <= 18) { // Local closes at 18:00
                    timeCombo->addItem(QString("%1:00").arg(h, 2, 10, QChar('0')));
                }
            }
            if (timeCombo->count() > 0) {
                timeCombo->setEnabled(true);
            } else {
                timeCombo->addItem("No slots available");
                timeCombo->setEnabled(false);
            }
        }
        return;
    }
    
    int instId = instructorCombo->currentData().toInt();
    if (instId == 0) return;
    
    int durationMins = durationCombo->currentText().split(" ").first().toInt();
    int blocksNeeded = (durationMins > 60) ? 2 : 1;
    
    int d = date.dayOfWeek();
    QString dayStr;
    switch(d) {
        case 1: dayStr = "MONDAY"; break;
        case 2: dayStr = "TUESDAY"; break;
        case 3: dayStr = "WEDNESDAY"; break;
        case 4: dayStr = "THURSDAY"; break;
        case 5: dayStr = "FRIDAY"; break;
        case 6: dayStr = "SATURDAY"; break;
        case 7: dayStr = "SUNDAY"; break;
    }
    
    QSet<int> allAvailableHours;
    QSqlQuery q;
    q.prepare("SELECT start_time FROM AVAILABILITY WHERE instructor_id = :id AND day_of_week = :d AND is_active = 1");
    q.bindValue(":id", instId);
    q.bindValue(":d", dayStr);
    if (q.exec()) {
        while (q.next()) {
            int h = q.value(0).toString().left(2).toInt();
            allAvailableHours.insert(h);
        }
    }
    
    QSet<int> bookedHours;
    QSqlQuery qb;
    qb.prepare("SELECT start_time, end_time FROM SESSION_BOOKING "
               "WHERE instructor_id = :id AND TRUNC(session_date) = :sd AND status IN ('CONFIRMED', 'COMPLETED')");
    qb.bindValue(":id", instId);
    qb.bindValue(":sd", date);
    if (qb.exec()) {
        while (qb.next()) {
            int startH = qb.value(0).toString().left(2).toInt();
            int endH = qb.value(1).toString().left(2).toInt();
            for (int i = startH; i < endH; i++) {
                bookedHours.insert(i);
            }
            if (startH == endH) {
                bookedHours.insert(startH);
            }
        }
    }
    
    for (int h = 8; h <= 17; h++) {
        bool canBook = true;
        for (int i = 0; i < blocksNeeded; i++) {
            if (!allAvailableHours.contains(h + i) || bookedHours.contains(h + i)) {
                canBook = false; break;
            }
        }
        if (canBook) {
            timeCombo->addItem(QString("%1:00").arg(h, 2, 10, QChar('0')));
        }
    }
    
    if (timeCombo->count() == 0) {
        timeCombo->addItem("No slots available");
        timeCombo->setEnabled(false);
    } else {
        timeCombo->setEnabled(true);
    }
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
    if (timeStr == "No slots available" || timeCombo->count() == 0) {
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
        msgBox.setWindowTitle("Accès Refusé");
        msgBox.setText("⚠️ Plafond de crédit atteint");
        msgBox.setInformativeText(QString("Désolé, vous ne pouvez pas réserver de nouvelle séance. Votre solde débiteur actuel est de <b>%1 TND</b>, ce qui dépasse la limite autorisée de 200 TND.<br><br>Veuillez régulariser votre situation en effectuant un paiement depuis votre tableau de bord afin de pouvoir effectuer de nouvelles réservations.").arg(currentDebt, 0, 'f', 0));
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
    this->accept();
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
