#include "paymentverification.h"
#include "thememanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QScrollArea>
#include <QApplication>

PaymentVerification::PaymentVerification(QWidget *parent) :
    QWidget(parent)
{
    // ui->setupUi(this);  // Skip empty UI file
    setupUI();
}

PaymentVerification::~PaymentVerification()
{
}

void PaymentVerification::setupUI()
{
    ThemeManager* theme = ThemeManager::instance();
    bool isDark = theme->currentTheme() == ThemeManager::Dark;
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(40, 30, 40, 30);
    mainLayout->setSpacing(25);
    
    // Header with badge
    QHBoxLayout *headerLayout = new QHBoxLayout();
    
    QLabel *titleIcon = new QLabel("🔴");
    titleIcon->setStyleSheet("QLabel { font-size: 24px; }");
    
    QLabel *title = new QLabel("Pending Verification");
    title->setStyleSheet(QString("QLabel { font-size: 22px; font-weight: bold; color: %1; }").arg(theme->primaryTextColor()));
    
    headerLayout->addWidget(titleIcon);
    headerLayout->addWidget(title);
    headerLayout->addStretch();
    
    mainLayout->addLayout(headerLayout);
    
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet("QScrollArea { border: none; background: transparent; }");
    QWidget *scrollContent = new QWidget();
    scrollContent->setStyleSheet("QWidget { background: transparent; }");
    QVBoxLayout *paymentsLayout = new QVBoxLayout(scrollContent);
    paymentsLayout->setSpacing(20);

    // Fetch payments strictly for the current instructor's assigned students
    int instId = qApp->property("currentUserId").toInt();

    QSqlQuery q;
    q.prepare("SELECT p.payment_id, s.name as student_name, "
              "p.d17_code, TO_CHAR(p.facture_date, 'Mon DD, YYYY at HH:MI AM'), "
              "p.amount "
              "FROM PAYMENT p JOIN STUDENTS s ON p.student_id = s.id "
              "WHERE p.status = 'PENDING' AND s.instructor_id = :inst_id "
              "ORDER BY p.facture_date DESC");
    q.bindValue(":inst_id", instId);
    if (q.exec()) {
        int count = 0;
        while (q.next()) {
            int pid = q.value("payment_id").toInt();
            QString sname = q.value("student_name").toString();
            QString code = q.value("d17_code").toString();
            QString pdate = q.value(3).toString();
            QString amt = QString("%1 TND").arg(q.value("amount").toDouble(), 0, 'f', 0);
            
            QWidget *card = createPaymentCard(pid, sname, code, pdate, amt);
            paymentsLayout->addWidget(card);
            count++;
        }
        if (count == 0) {
            QLabel *empty = new QLabel("🎉 No pending payments to verify!");
            empty->setStyleSheet(QString("QLabel { color: %1; font-size: 16px; font-style: italic; }").arg(theme->secondaryTextColor()));
            paymentsLayout->addWidget(empty);
        }
    } else {
        QLabel *err = new QLabel("Database error loading payments.");
        paymentsLayout->addWidget(err);
    }
    
    paymentsLayout->addStretch();
    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea, 1);
    
    // How to verify section
    QFrame *instructionsCard = new QFrame();
    instructionsCard->setStyleSheet(
        QString("QFrame {"
        "    background-color: %1;"
        "    border-radius: 12px;"
        "    padding: 25px;"
        "}").arg(isDark ? "#1E3A5F" : "#e8f4fd")
    );
    
    QVBoxLayout *instructionsLayout = new QVBoxLayout(instructionsCard);
    
    QLabel *instructionsTitle = new QLabel("How to Verify a Payment?");
    instructionsTitle->setStyleSheet(QString("QLabel { font-size: 18px; font-weight: bold; color: %1; }").arg(theme->primaryTextColor()));
    instructionsLayout->addWidget(instructionsTitle);
    
    QStringList steps = {
        "Check your D17 app for the transaction code provided by the student",
        "Confirm that the amount and code match",
        "Click \"Verify\" to confirm the payment or \"Reject\" if the code is invalid"
    };
    
    for (int i = 0; i < steps.size(); ++i) {
        QHBoxLayout *stepLayout = new QHBoxLayout();
        
        QLabel *stepNum = new QLabel(QString::number(i + 1) + ".");
        stepNum->setFixedWidth(25);
        stepNum->setStyleSheet(QString("QLabel { font-weight: bold; color: %1; font-size: 14px; }").arg(isDark ? "#60A5FA" : "#1976d2"));
        
        QLabel *stepText = new QLabel(steps[i]);
        stepText->setStyleSheet(QString("QLabel { color: %1; font-size: 13px; }").arg(theme->primaryTextColor()));
        stepText->setWordWrap(true);
        
        stepLayout->addWidget(stepNum);
        stepLayout->addWidget(stepText, 1);
        
        instructionsLayout->addLayout(stepLayout);
    }
    
    mainLayout->addWidget(instructionsCard);
}

QWidget* PaymentVerification::createPaymentCard(int paymentId, const QString& studentName,
                                                const QString& transactionCode, const QString& paymentDate,
                                                const QString& amount)
{
    ThemeManager* theme = ThemeManager::instance();
    bool isDark = theme->currentTheme() == ThemeManager::Dark;
    
    QFrame *card = new QFrame();
    card->setStyleSheet(
        QString("QFrame {"
        "    background-color: %1;"
        "    border: 1px solid %2;"
        "    border-radius: 12px;"
        "    padding: 25px;"
        "}").arg(theme->cardColor(), theme->borderColor())
    );
    
    QHBoxLayout *mainCardLayout = new QHBoxLayout(card);
    mainCardLayout->setSpacing(30);
    
    // Left side - Student info and details
    QVBoxLayout *leftLayout = new QVBoxLayout();
    leftLayout->setSpacing(15);
    
    // Student header with avatar
    QHBoxLayout *studentHeader = new QHBoxLayout();
    
    QLabel *avatar = new QLabel("👤");
    avatar->setFixedSize(50, 50);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setStyleSheet(
        QString("QLabel {"
        "    background-color: %1;"
        "    border-radius: 25px;"
        "    font-size: 24px;"
        "}").arg(isDark ? "#134E4A" : "#ECFDF5")
    );
    
    QVBoxLayout *studentInfo = new QVBoxLayout();
    
    QLabel *nameLabel = new QLabel(studentName);
    nameLabel->setStyleSheet(QString("QLabel { font-size: 18px; font-weight: bold; color: %1; }").arg(theme->primaryTextColor()));
    
    QLabel *sessionLabel = new QLabel("D17 Payment");
    sessionLabel->setStyleSheet(
        "QLabel {"
        "    color: #14B8A6;"
        "    font-size: 14px;"
        "    font-weight: 600;"
        "}"
    );
    
    studentInfo->addWidget(nameLabel);
    studentInfo->addWidget(sessionLabel);
    
    QLabel *statusBadge = new QLabel("Pending");
    statusBadge->setStyleSheet(
        QString("QLabel {"
        "    background-color: %1;"
        "    color: %2;"
        "    padding: 6px 14px;"
        "    border-radius: 12px;"
        "    font-size: 12px;"
        "    font-weight: bold;"
        "}").arg(isDark ? "#422006" : "#fff3cd", isDark ? "#FBBF24" : "#856404")
    );
    statusBadge->setFixedHeight(30);
    
    studentHeader->addWidget(avatar);
    studentHeader->addLayout(studentInfo, 1);
    studentHeader->addWidget(statusBadge);
    
    leftLayout->addLayout(studentHeader);
    
    // Transaction details
    QFrame *detailsFrame = new QFrame();
    detailsFrame->setStyleSheet(
        QString("QFrame {"
        "    background-color: %1;"
        "    border-radius: 8px;"
        "    padding: 15px;"
        "}").arg(isDark ? "#1E293B" : "#F9FAFB")
    );
    
    QVBoxLayout *detailsLayout = new QVBoxLayout(detailsFrame);
    detailsLayout->setSpacing(8);
    
    // Transaction code
    QHBoxLayout *codeLayout = new QHBoxLayout();
    QLabel *codeIcon = new QLabel("💳");
    codeIcon->setStyleSheet("QLabel { font-size: 16px; }");
    
    QLabel *codeLabel = new QLabel("Transaction Code:");
    codeLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 13px; }").arg(theme->secondaryTextColor()));
    
    QLabel *codeValue = new QLabel(transactionCode);
    codeValue->setStyleSheet(
        QString("QLabel {"
        "    color: %1;"
        "    font-size: 14px;"
        "    font-weight: bold;"
        "    font-family: 'Courier New', monospace;"
        "}").arg(theme->primaryTextColor())
    );
    
    codeLayout->addWidget(codeIcon);
    codeLayout->addWidget(codeLabel);
    codeLayout->addWidget(codeValue);
    codeLayout->addStretch();
    
    detailsLayout->addLayout(codeLayout);
    
    leftLayout->addWidget(detailsFrame);
    
    // Amount and submission info
    QHBoxLayout *amountLayout = new QHBoxLayout();
    
    QLabel *amountLabel = new QLabel("Amount:");
    amountLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 14px; }").arg(theme->secondaryTextColor()));
    
    QLabel *amountValue = new QLabel(amount);
    amountValue->setStyleSheet("QLabel { color: #14B8A6; font-size: 20px; font-weight: bold; }");
    
    amountLayout->addWidget(amountLabel);
    amountLayout->addWidget(amountValue);
    amountLayout->addStretch();
    
    leftLayout->addLayout(amountLayout);
    
    QLabel *submittedLabel = new QLabel("Submitted on " + paymentDate);
    submittedLabel->setStyleSheet(QString("QLabel { color: %1; font-size: 12px; font-style: italic; }").arg(theme->mutedTextColor()));
    leftLayout->addWidget(submittedLabel);
    
    mainCardLayout->addLayout(leftLayout, 1);
    
    // Right side - Action buttons
    QVBoxLayout *rightLayout = new QVBoxLayout();
    rightLayout->setAlignment(Qt::AlignCenter);
    rightLayout->setSpacing(12);
    
    QPushButton *verifyBtn = new QPushButton("✓ Verify");
    verifyBtn->setFixedSize(180, 45);
    verifyBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #059669;"
        "    color: white;"
        "    font-size: 15px;"
        "    font-weight: bold;"
        "    border: none;"
        "    border-radius: 8px;"
        "}"
        "QPushButton:hover { background-color: #229954; }"
    );
    verifyBtn->setCursor(Qt::PointingHandCursor);
    connect(verifyBtn, &QPushButton::clicked, [=]() { onVerifyPayment(paymentId, studentName); });
    
    QPushButton *rejectBtn = new QPushButton("✕ Reject");
    rejectBtn->setFixedSize(180, 45);
    rejectBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #DC2626;"
        "    color: white;"
        "    font-size: 15px;"
        "    font-weight: bold;"
        "    border: none;"
        "    border-radius: 8px;"
        "}"
        "QPushButton:hover { background-color: #B91C1C; }"
    );
    rejectBtn->setCursor(Qt::PointingHandCursor);
    connect(rejectBtn, &QPushButton::clicked, [=]() { onRejectPayment(paymentId, studentName); });
    
    rightLayout->addWidget(verifyBtn);
    rightLayout->addWidget(rejectBtn);
    
    mainCardLayout->addLayout(rightLayout);
    
    return card;
}

void PaymentVerification::onVerifyPayment(int paymentId, const QString& studentName)
{
    QMessageBox msgBox;
    msgBox.setWindowTitle("Confirm Verification");
    msgBox.setText(QString("Verify payment for %1?").arg(studentName));
    msgBox.setInformativeText("This will mark the D17 payment as received and update the student's credit.");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.setIcon(QMessageBox::Question);
    
    if (msgBox.exec() == QMessageBox::Yes) {
        // Update DB
        QSqlQuery q;
        q.prepare("UPDATE PAYMENT SET status = 'CONFIRMED' WHERE payment_id = :id");
        q.bindValue(":id", paymentId);
        
        if (q.exec()) {
            QMessageBox::information(this, "Payment Verified", 
                                    QString("Payment for %1 has been verified successfully!").arg(studentName));
            
            // Hide row
            QWidget *card = qobject_cast<QWidget*>(sender()->parent()->parent());
            if (card) {
                card->hide();
            }
        } else {
            QMessageBox::critical(this, "DB Error", "Failed to update payment status:\n" + q.lastError().text());
        }
    }
}

void PaymentVerification::onRejectPayment(int paymentId, const QString& studentName)
{
    QMessageBox msgBox;
    msgBox.setWindowTitle("Confirm Rejection");
    msgBox.setText(QString("Reject payment for %1?").arg(studentName));
    msgBox.setInformativeText("This will reject the D17 code. The student's balance will reflect the missing payment.");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Cancel);
    msgBox.setIcon(QMessageBox::Warning);
    
    if (msgBox.exec() == QMessageBox::Yes) {
        // Update DB
        QSqlQuery q;
        q.prepare("UPDATE PAYMENT SET status = 'REJECTED' WHERE payment_id = :id");
        q.bindValue(":id", paymentId);
        
        if (q.exec()) {
            QMessageBox::information(this, "Payment Rejected", 
                                    QString("Payment for %1 has been rejected.").arg(studentName));
            
            // Hide row
            QWidget *card = qobject_cast<QWidget*>(sender()->parent()->parent());
            if (card) {
                card->hide();
            }
        } else {
            QMessageBox::critical(this, "DB Error", "Failed to update payment status:\n" + q.lastError().text());
        }
    }
}
