#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "database.h"
#include "admindashboard.h"
#include "instructordashboard.h"
#include "studentdashboard.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProgressDialog>
#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QFile>
#include <QRegularExpression>
#include <QDate>
#include <QSslSocket>
#include <QThread>
#include <QProcess>
#include <QTemporaryFile>
#include <QEventLoop>
#include <QRandomGenerator>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QGraphicsOpacityEffect>
#include <QGraphicsDropShadowEffect>
#include <QVideoSink>
#include <QVideoFrame>
#include <QSvgRenderer>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QFrame>
#include <QPushButton>
#include <QHBoxLayout>
#include <QCamera>
#include <QMediaCaptureSession>
#include <QImageCapture>
#include <QMediaDevices>
#include <QStandardPaths>
#include <QUuid>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QBuffer>
#include <QMessageBox>
#include <QHttpMultiPart>
#include <QCryptographicHash>
#include <QtMath>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <functional>
#include "studentlearninghub.h"

// ── OCR.space API configuration ──
// Free key: register at https://ocr.space/ocrapi/freekey  (25 000 req/month)
// Place key in ocrspace_key.txt next to DrivingSchoolApp.exe
// OR set environment variable OCRSPACE_API_KEY
// The built-in "helloworld" key works for basic testing (limited requests).
static const QString OCRSPACE_API_URL = QStringLiteral("https://api.ocr.space/parse/image");

static QString getOcrSpaceApiKey()
{
    // 1. Environment variable
    QString key = qEnvironmentVariable("OCRSPACE_API_KEY").trimmed();
    if (!key.isEmpty()) return key;

    // 2. ocrspace_key.txt next to the executable
    const QStringList dirs = {
        QCoreApplication::applicationDirPath(),
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation),
        QDir::homePath()
    };
    for (const QString &dir : dirs) {
        QFile f(dir + "/ocrspace_key.txt");
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            key = QString::fromUtf8(f.readLine()).trimmed();
            f.close();
            if (!key.isEmpty() && !key.startsWith("PASTE"))
                return key;
        }
    }

    // 3. Built-in free test key (limited but always available)
    return QStringLiteral("helloworld");
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    ui = new Ui::MainWindow;
    ui->setupUi(this);

    // Initialize network manager for API calls
    networkManager = new QNetworkAccessManager(this);

    // ── Login page (index 0) ──
    connect(ui->btnSignIn, &QPushButton::clicked, this, [this]() {
        const QString email = ui->editSignEmail->text().trimmed();
        const QString password = ui->editSignPassword->text();

        ui->editSignEmail->setStyleSheet("");
        ui->editSignPassword->setStyleSheet("");

        static const QRegularExpression emailRx(R"([^@\s]+@[^@\s]+\.[^@\s]{2,})");

        if (email.isEmpty()) {
            showToast("⚠  Please enter your email address.");
            ui->editSignEmail->setStyleSheet("border: 2px solid #DC2626; background: #FEF2F2;");
            shakeWidget(ui->editSignEmail);
            return;
        }

        if (!emailRx.match(email).hasMatch()) {
            showToast("⚠  Please enter a valid email address.");
            ui->editSignEmail->setStyleSheet("border: 2px solid #DC2626; background: #FEF2F2;");
            shakeWidget(ui->editSignEmail);
            return;
        }

        if (password.isEmpty()) {
            showToast("⚠  Please enter your password.");
            ui->editSignPassword->setStyleSheet("border: 2px solid #DC2626; background: #FEF2F2;");
            shakeWidget(ui->editSignPassword);
            return;
        }

        // ── Try Oracle login for all 3 roles ──
        if (!tryLoginAllRoles(email, password)) {
            showToast("Invalid email or password.");
            ui->editSignPassword->clear();
            shakeWidget(ui->editSignPassword);
            return;
        }
        ui->editSignPassword->clear();
    });

    connect(ui->btnForgot, &QPushButton::clicked, this, [this]() { goToStep(1); });
    connect(ui->btnCreateAccount, &QPushButton::clicked, this, [this]() { goToStep(2); });

    // ── Forgot password page (index 1) ──
    connect(ui->btnSendReset, &QPushButton::clicked, this, [this]() {
        QString email = ui->editForgotEmail->text().trimmed();

        ui->editForgotEmail->setStyleSheet("");

        // Validate: empty
        if (email.isEmpty()) {
            showToast("\u26A0\u00A0 Please enter your email address.");
            ui->editForgotEmail->setStyleSheet("border: 2px solid #DC2626; background: #FEF2F2;");
            shakeWidget(ui->editForgotEmail);
            return;
        }

        // Validate: format
        static const QRegularExpression emailRx(
            R"([^@\s]+@[^@\s]+\.[^@\s]{2,})");
        if (!emailRx.match(email).hasMatch()) {
            showToast("\u26A0\u00A0 Please enter a valid email address.");
            ui->editForgotEmail->setStyleSheet("border: 2px solid #DC2626; background: #FEF2F2;");
            shakeWidget(ui->editForgotEmail);
            return;
        }

        // Generate 6-digit code
        m_verificationCode = QString::number(
            QRandomGenerator::global()->bounded(100000, 1000000));

        // Disable button while sending
        ui->btnSendReset->setEnabled(false);
        ui->btnSendReset->setText("Sending\u2026");
        ui->widgetVerify->setVisible(false);

        sendVerificationEmail(email, m_verificationCode,
            [this](bool success, const QString &errorMsg) {
            ui->btnSendReset->setEnabled(true);
            ui->btnSendReset->setText("Resend Code");

            if (!success) {
                showToast("\u26A0\u00A0 Failed to send email: " + errorMsg);
                return;
            }

            // Slide in verification section
            ui->editVerifyCode->clear();
            ui->editVerifyCode->setStyleSheet("");
            showWidget(ui->widgetVerify);
        });
    });

    connect(ui->btnVerifyCode, &QPushButton::clicked, this, [this]() {
        QString entered = ui->editVerifyCode->text().trimmed();

        ui->editVerifyCode->setStyleSheet("");

        if (entered.isEmpty()) {
            showToast("\u26A0\u00A0 Please enter the verification code.");
            ui->editVerifyCode->setStyleSheet("border: 2px solid #DC2626; background: #FEF2F2;");
            shakeWidget(ui->editVerifyCode);
            return;
        }

        if (entered != m_verificationCode) {
            showToast("\u26A0\u00A0 Incorrect code. Please try again.");
            ui->editVerifyCode->setStyleSheet("border: 2px solid #DC2626; background: #FEF2F2;");
            shakeWidget(ui->editVerifyCode);
            return;
        }

        // Success — store email, go to new-password page (index 6)
        m_resetEmail = ui->editForgotEmail->text().trimmed();
        m_verificationCode.clear();
        ui->widgetVerify->setVisible(false);
        ui->editVerifyCode->clear();
        ui->editForgotEmail->clear();
        ui->editForgotEmail->setStyleSheet("");
        ui->btnSendReset->setText("Resend Code");
        if (m_editNewPass)    m_editNewPass->clear();
        if (m_editConfirmPass) m_editConfirmPass->clear();
        goToStep(6);   // New-password form
    });

    connect(ui->btnBackToLogin, &QPushButton::clicked, this, [this]() { goToStep(0); });

    // ── Registration Step 1 → Step 2 (index 2 → 3) ──
    connect(ui->btnNext1, &QPushButton::clicked, this, [this]() {
        // First Name
        if (ui->editFirstName->text().trimmed().isEmpty()) {
            showToast("\u26A0  First Name is required.");
            shakeWidget(ui->editFirstName); return;
        }
        // Last Name
        if (ui->editLastName->text().trimmed().isEmpty()) {
            showToast("\u26A0  Last Name is required.");
            shakeWidget(ui->editLastName); return;
        }
        // Date of Birth — reject default date (1 Jan 2000 or year < 1900)
        QDate dob = ui->editDOB->date();
        if (!dob.isValid() || dob.year() < 1900 || dob >= QDate::currentDate()) {
            showToast("\u26A0  Please enter a valid Date of Birth.");
            shakeWidget(ui->editDOB); return;
        }
        // Phone
        if (ui->editPhone->text().trimmed().isEmpty()) {
            showToast("\u26A0  Phone Number is required.");
            shakeWidget(ui->editPhone); return;
        }
        // CIN
        if (ui->editCIN->text().trimmed().isEmpty()) {
            showToast("\u26A0  CIN (National ID) is required.");
            shakeWidget(ui->editCIN); return;
        }
        // Address
        if (ui->editAddress->text().trimmed().isEmpty()) {
            showToast("\u26A0  Address is required.");
            shakeWidget(ui->editAddress); return;
        }
        // City
        if (ui->editCity->text().trimmed().isEmpty()) {
            showToast("\u26A0  City is required.");
            shakeWidget(ui->editCity); return;
        }
        goToStep(3);
    });
    connect(ui->btnCancel, &QPushButton::clicked, this, [this]() { goToStep(0); });

    // ── Photo preview helper ──────────────────────────────────────────────────
    auto applyPhotoPreview = [this](const QString &path) {
        if (path.isEmpty()) return;
        QPixmap px(path);
        if (px.isNull()) return;
        m_studentPhotoPath = path;
        // Circular crop
        QPixmap sized = px.scaled(100, 100, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        QPixmap circle(100, 100);
        circle.fill(Qt::transparent);
        QPainter painter(&circle);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(QBrush(sized));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(0, 0, 100, 100);
        painter.end();
        ui->lblPhotoPreview->setPixmap(circle);
        ui->btnRemovePhoto->setVisible(true);
        ui->chkPhoto->setChecked(true);
    };

    // ── Upload Photo ──────────────────────────────────────────────────────────
    connect(ui->btnUploadPhoto, &QPushButton::clicked, this, [this, applyPhotoPreview]() {
        QString path = QFileDialog::getOpenFileName(
            this, "Select Profile Photo", QString(),
            "Images (*.png *.jpg *.jpeg *.bmp *.webp)");
        if (!path.isEmpty()) applyPhotoPreview(path);
    });

    // ── Take Photo with camera ────────────────────────────────────────────────
    connect(ui->btnTakePhotoReg, &QPushButton::clicked, this, [this, applyPhotoPreview]() {
        openCameraCapture("Look at the camera and smile 😊", "",
            [this, applyPhotoPreview](const QString &savedPath) {
                if (!savedPath.isEmpty()) applyPhotoPreview(savedPath);
            });
    });

    // ── Remove Photo ──────────────────────────────────────────────────────────
    connect(ui->btnRemovePhoto, &QPushButton::clicked, this, [this]() {
        m_studentPhotoPath.clear();
        ui->lblPhotoPreview->setPixmap(QPixmap());
        ui->btnRemovePhoto->setVisible(false);
        ui->chkPhoto->setChecked(false);
    });

    // ── Style the preview label placeholder ──────────────────────────────────
    ui->lblPhotoPreview->setStyleSheet(
        "QLabel {"
        "  background: rgba(255,255,255,0.15);"
        "  border: 2px dashed rgba(255,255,255,0.4);"
        "  border-radius: 50px;"
        "  color: rgba(255,255,255,0.6);"
        "  font-size: 28px;"
        "}");
    ui->lblPhotoPreview->setText("👤");
    ui->lblPhotoPreview->setAlignment(Qt::AlignCenter);

    // Style photo buttons
    QString photoBtn =
        "QPushButton {"
        "  background: rgba(255,255,255,0.18);"
        "  border: 1px solid rgba(255,255,255,0.35);"
        "  border-radius: 8px;"
        "  color: white;"
        "  padding: 8px 14px;"
        "  font-size: 13px;"
        "}"
        "QPushButton:hover { background: rgba(255,255,255,0.30); }";
    ui->btnUploadPhoto->setStyleSheet(photoBtn);
    ui->btnTakePhotoReg->setStyleSheet(photoBtn);
    ui->btnRemovePhoto->setStyleSheet(
        "QPushButton {"
        "  background: rgba(220,38,38,0.25);"
        "  border: 1px solid rgba(220,38,38,0.5);"
        "  border-radius: 8px;"
        "  color: #fca5a5;"
        "  padding: 6px 12px;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover { background: rgba(220,38,38,0.45); }");

    // ── Automate button: OCR scan of Tunisian CIN card ──
    connect(ui->btnAutomate, &QPushButton::clicked, this, [this]() {
        // 1. Capture FRONT of CIN with camera
        openCameraCapture("Front of CIN card", ":/assets/frontcin.svg",
            [this](const QString &frontPath) {
            if (frontPath.isEmpty()) return;

            // 2. Capture BACK of CIN with camera
            openCameraCapture("Back of CIN card", ":/assets/backcin.svg",
                [this, frontPath](const QString &backPath) {
                if (backPath.isEmpty()) return;
                startOcrScan(frontPath, backPath);
            });
        });
    });

    // ── Registration Step 2 (index 3) ──
    connect(ui->btnNext2, &QPushButton::clicked, this, [this]() {
        // Email
        QString email = ui->editEmail->text().trimmed();
        if (email.isEmpty()) {
            showToast("\u26A0  Email address is required.");
            shakeWidget(ui->editEmail); return;
        }
        static const QRegularExpression emailRx(R"([^@\s]+@[^@\s]+\.[^@\s]{2,})");
        if (!emailRx.match(email).hasMatch()) {
            showToast("\u26A0  Please enter a valid email address.");
            shakeWidget(ui->editEmail); return;
        }
        // Password
        QString pwd = ui->editPassword->text();
        if (pwd.isEmpty()) {
            showToast("\u26A0  Password is required.");
            shakeWidget(ui->editPassword); return;
        }
        if (pwd.length() < 8) {
            showToast("\u26A0  Password must be at least 8 characters.");
            shakeWidget(ui->editPassword); return;
        }
        // Confirm password
        if (ui->editConfirm->text() != pwd) {
            showToast("\u26A0  Passwords do not match.");
            shakeWidget(ui->editConfirm); return;
        }
        goToStep(4);
    });
    connect(ui->btnPrev2, &QPushButton::clicked, this, [this]() { goToStep(2); });

    // ── Registration Step 3 (index 4) ──
    connect(ui->btnNext3, &QPushButton::clicked, this, [this]() {
        // At least one course level must be selected
        if (!ui->rbRules->isChecked() && !ui->rbPractical->isChecked() &&
            !ui->rbParking->isChecked()) {
            showToast("\u26A0  Please select a Driving Course Level."); return;
        }
        // CIN document checkbox
        if (!ui->chkCIN->isChecked()) {
            showToast("\u26A0  Please confirm you have your CIN (National ID Card).");
            shakeWidget(ui->chkCIN); return;
        }
        // Photo checkbox
        if (!ui->chkPhoto->isChecked()) {
            showToast("\u26A0  Please confirm you have your face photo.");
            shakeWidget(ui->chkPhoto); return;
        }
        // Theory exam required for Practical / Parking
        if ((ui->rbPractical->isChecked() || ui->rbParking->isChecked()) &&
            !ui->chkTheoryExam->isChecked()) {
            showToast("\u26A0  You must confirm you passed the theory exam (Code).");
            shakeWidget(ui->chkTheoryExam); return;
        }
        goToStep(5);
    });
    connect(ui->btnPrev3, &QPushButton::clicked, this, [this]() { goToStep(3); });

    // ── Registration Step 4 (index 5) ──
    connect(ui->btnPrev4, &QPushButton::clicked, this, [this]() { goToStep(4); });
    connect(ui->btnScanFingerprint, &QPushButton::clicked, this, [this]() {
        m_fingerprintDone = true;
        ui->labelFingerprintStatus->setText("\u2713  Fingerprint registered successfully!");
        ui->btnScanFingerprint->setEnabled(false);
    });
    connect(ui->btnComplete, &QPushButton::clicked, this, [this]() {
        if (!m_fingerprintDone) {
            showToast("\u26A0  Please scan your fingerprint before completing registration.");
            shakeWidget(ui->btnScanFingerprint); return;
        }

        QString errorMessage;
        if (!insertStudentFromForm(&errorMessage)) {
            showToast("Registration failed: " + errorMessage);
            return;
        }

        // Get info from form to open Student Dashboard directly
        QString firstName   = ui->editFirstName->text().trimmed();
        QString lastName    = ui->editLastName->text().trimmed();
        QString email       = ui->editEmail->text().trimmed();
        bool    hasLicense  = false; // new student has no license yet
        QString fullName    = firstName + " " + lastName;

        // Get the newly created student ID from Oracle
        int newStudentId = 0;
        {
            QSqlQuery idQ;
            idQ.prepare("SELECT id FROM STUDENTS WHERE LOWER(email)=LOWER(?) AND student_status='ACTIVE'");
            idQ.addBindValue(email);
            if (idQ.exec() && idQ.next())
                newStudentId = idQ.value(0).toInt();
        }

        m_fingerprintDone = false;
        clearRegistrationForm();

        // Open Student Dashboard directly — no need to log in again
        StudentDashboard *w = new StudentDashboard();
        w->setStudentId(newStudentId);
        w->setStudentInfo(fullName, email, hasLicense);
        w->showMaximized();
        this->hide();
    });

    // Start on login page
    ui->stackedWidget->setCurrentIndex(0);

    // Build the new-password page (index 6) programmatically
    buildResetPasswordPage();

    // ── Required documents: hidden by default, shown for Practical / Parking ──
    ui->groupRequiredDocs->setVisible(false);
    connect(ui->rbRules, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) ui->groupRequiredDocs->setVisible(false);
    });
    connect(ui->rbPractical, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) ui->groupRequiredDocs->setVisible(true);
    });
    connect(ui->rbParking, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) ui->groupRequiredDocs->setVisible(true);
    });

    // ── Background car animation ──
    initCars();

    animTimer = new QTimer(this);
    connect(animTimer, &QTimer::timeout, this, &MainWindow::updateAnimation);
    animTimer->start(16); // ~60 FPS
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::goToStep(int step)
{
    if (ui && ui->stackedWidget)
        ui->stackedWidget->setCurrentIndex(step);
}

QString MainWindow::hashPassword(const QString &password) const
{
    const QByteArray digest = QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(digest.toHex());
}

// ── Oracle: check ADMIN, INSTRUCTORS, then STUDENTS ───────────────────
bool MainWindow::tryLoginAllRoles(const QString &email, const QString &password)
{
    const QString hash = hashPassword(password);

    // 1. Check ADMIN table
    {
        QSqlQuery q;
        q.prepare("SELECT id, full_name FROM ADMIN "
                  "WHERE LOWER(email)=LOWER(?) AND password_hash=? AND status='active'");
        q.addBindValue(email.trimmed());
        q.addBindValue(hash);
        if (q.exec() && q.next()) {
            QString name = q.value(1).toString();
            // Open dashboard FIRST, then hide login — avoids blocking dialog issue
            AdminDashboard *w = new AdminDashboard();
            w->showMaximized();
            this->hide();
            QMessageBox::information(w, "✅ Welcome", "Welcome, " + name + "!\nLogged in as Administrator.");
            return true;
        }
    }

    // 2. Check INSTRUCTORS table (single source of truth for all instructors)
    {
        QSqlQuery q;
        q.prepare("SELECT id, full_name, school_id FROM INSTRUCTORS "
                  "WHERE LOWER(email)=LOWER(?) AND password_hash=?");
        q.addBindValue(email.trimmed());
        q.addBindValue(hash);
        if (q.exec() && q.next()) {
            int     instrId  = q.value(0).toInt();
            QString name     = q.value(1).toString();
            int     schoolId = q.value(2).toInt();
            InstructorDashboard *w = new InstructorDashboard();
            w->setSchoolId(schoolId);
            w->setInstructorId(instrId);
            w->init();
            w->showMaximized();
            this->hide();
            QMessageBox::information(w, "✅ Welcome", "Welcome, " + name + "!\nLogged in as Instructor.");
            return true;
        }
    }

    // 3. Check STUDENTS table
    {
        QSqlQuery q;
        q.prepare("SELECT id, first_name, last_name, has_driving_license, status FROM STUDENTS "
                  "WHERE LOWER(email)=LOWER(?) AND password_hash=? AND student_status='ACTIVE'");
        q.addBindValue(email.trimmed());
        q.addBindValue(hash);
        if (q.exec() && q.next()) {
            int     id          = q.value(0).toInt();
            QString firstName   = q.value(1).toString();
            QString lastName    = q.value(2).toString();
            bool    hasLicense  = q.value(3).toInt() == 1;
            QString reqStatus   = q.value(4).toString(); // pending / approved / rejected / null
            QString fullName    = firstName + " " + lastName;

            // ── APPROVED → open SmartDrive learning module (integrated) ──
            if (reqStatus == "approved") {
                StudentLearningHub *hub = new StudentLearningHub(id);
                hub->setAttribute(Qt::WA_DeleteOnClose);
                hub->showMaximized();
                this->hide();
                return true;
            }

            // ── PENDING → tell student to wait ────────────────────────────
            if (reqStatus == "pending") {
                StudentDashboard *w = new StudentDashboard();
                w->setStudentId(id);
                w->setStudentInfo(fullName, email, hasLicense);
                w->showMaximized();
                this->hide();
                QMessageBox::information(w, "⏳ Pending Approval",
                    "Your registration request has been sent.\n"
                    "Please wait for your instructor to accept you.\n\n"
                    "You will be able to access the learning app once approved.");
                return true;
            }

            // ── REJECTED → let student choose another school ──────────────
            if (reqStatus == "rejected") {
                StudentDashboard *w = new StudentDashboard();
                w->setStudentId(id);
                w->setStudentInfo(fullName, email, hasLicense);
                w->showMaximized();
                this->hide();
                QMessageBox::warning(w, "❌ Request Rejected",
                    "Your registration request was rejected by the instructor.\n"
                    "You can choose a different school and send a new request.");
                return true;
            }

            // ── NOT YET SENT → show school selection dashboard ────────────
            StudentDashboard *w = new StudentDashboard();
            w->setStudentId(id);
            w->setStudentInfo(fullName, email, hasLicense);
            w->showMaximized();
            this->hide();
            return true;
        }
    }

    return false;
}

bool MainWindow::insertStudentFromForm(QString *errorMessage)
{
    const QString email    = ui->editEmail->text().trimmed();
    const QString password = ui->editPassword->text();
    const QString cin      = ui->editCIN->text().trimmed();

    if (email.isEmpty()) {
        if (errorMessage) *errorMessage = "Email is required.";
        return false;
    }
    if (password.isEmpty()) {
        if (errorMessage) *errorMessage = "Password is required.";
        return false;
    }

    // Check email uniqueness in Oracle
    {
        QSqlQuery ck;
        ck.prepare("SELECT COUNT(*) FROM STUDENTS WHERE LOWER(email)=LOWER(?)");
        ck.addBindValue(email);
        if (ck.exec() && ck.next() && ck.value(0).toInt() > 0) {
            if (errorMessage) *errorMessage = "An account with this email already exists.";
            return false;
        }
    }
    // Check CIN uniqueness
    {
        QSqlQuery ck;
        ck.prepare("SELECT COUNT(*) FROM STUDENTS WHERE cin=?");
        ck.addBindValue(cin);
        if (ck.exec() && ck.next() && ck.value(0).toInt() > 0) {
            if (errorMessage) *errorMessage = "A student with this CIN already exists.";
            return false;
        }
    }

    QString courseLevel = ui->rbRules->isChecked()    ? "Theory"
                        : ui->rbPractical->isChecked() ? "Practical" : "Parking";
    bool hasDrivingLicense = false; // student is registering fresh
    bool hasCode           = ui->chkTheoryExam->isChecked();
    bool isBeginner        = !hasCode;

    // Format date as DD/MM/YYYY for Oracle TO_DATE
    QString dobStr = ui->editDOB->date().toString("dd/MM/yyyy");

    QSqlQuery q;
    q.prepare(
        "INSERT INTO STUDENTS "
        "(first_name, last_name, date_of_birth, phone, cin, email, password_hash, "
        " has_cin, has_face_photo, is_beginner, has_driving_license, has_code, "
        " course_level, fingerprint_registered, student_status, enrollment_date) "
        "VALUES (?,?,TO_DATE(?,'DD/MM/YYYY'),?,?,?,?,?,?,?,?,?,?,?,?,SYSDATE)"
    );
    q.addBindValue(ui->editFirstName->text().trimmed());
    q.addBindValue(ui->editLastName->text().trimmed());
    q.addBindValue(dobStr);
    q.addBindValue(ui->editPhone->text().trimmed());
    q.addBindValue(cin);
    q.addBindValue(email);
    q.addBindValue(hashPassword(password));
    q.addBindValue(ui->chkCIN->isChecked()   ? 1 : 0);
    q.addBindValue(ui->chkPhoto->isChecked() ? 1 : 0);
    q.addBindValue(isBeginner        ? 1 : 0);
    q.addBindValue(hasDrivingLicense ? 1 : 0);
    q.addBindValue(hasCode           ? 1 : 0);
    q.addBindValue(courseLevel);
    q.addBindValue(m_fingerprintDone ? 1 : 0);
    q.addBindValue("ACTIVE");

    if (!q.exec()) {
        if (errorMessage)
            *errorMessage = q.lastError().text();
        return false;
    }

    // ── Copy student photo to app data folder ─────────────────────────────────
    if (!m_studentPhotoPath.isEmpty()) {
        QString photosDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                            + "/student_photos";
        QDir().mkpath(photosDir);
        QString cinVal = ui->editCIN->text().trimmed();
        QString ext    = QFileInfo(m_studentPhotoPath).suffix().toLower();
        if (ext.isEmpty()) ext = "jpg";
        QString destPath = photosDir + "/" + cinVal + "." + ext;
        // Remove old file if it exists then copy
        QFile::remove(destPath);
        QFile::copy(m_studentPhotoPath, destPath);
    }

    return true;
}

void MainWindow::clearRegistrationForm()
{
    ui->editFirstName->clear();
    ui->editLastName->clear();
    ui->editDOB->setDate(QDate::currentDate().addYears(-18));
    ui->editPhone->clear();
    ui->editCIN->clear();
    ui->editAddress->clear();
    ui->editCity->clear();
    ui->editEmail->clear();
    ui->editPassword->clear();
    ui->editConfirm->clear();

    ui->chkCIN->setChecked(false);
    ui->chkPhoto->setChecked(false);

    // Reset photo
    m_studentPhotoPath.clear();
    ui->lblPhotoPreview->setPixmap(QPixmap());
    ui->lblPhotoPreview->setText("👤");
    ui->btnRemovePhoto->setVisible(false);
    ui->chkTheoryExam->setChecked(false);
    ui->rbRules->setChecked(true);

    ui->btnScanFingerprint->setEnabled(true);
    ui->labelFingerprintStatus->setText("Fingerprint not scanned yet.");
}

// ══════════════════════════════════════════════════════════════
// Camera capture dialog
// ══════════════════════════════════════════════════════════════

void MainWindow::openCameraCapture(const QString &instruction,
                                   const QString &frameResource,
                                   std::function<void(const QString &)> callback)
{
    // NOTE: We do NOT pre-check QMediaDevices::videoInputs() because on Windows
    // the Media Foundation backend may not have finished initialising yet, causing
    // the list to appear empty even when a camera is present.
    // Instead we create QCamera directly (Qt will use the system default device)
    // and handle any real failure via the errorOccurred signal below.

    // ── Custom view widget: paints camera frames + SVG guide overlay ──
    struct CameraView : public QWidget {
        QImage       frame;
        QSvgRenderer svgRenderer;
        bool         cardDetected = false;
        int          detectionFrames = 0;   // hysteresis counter

        CameraView(const QByteArray &svg, QWidget *parent)
            : QWidget(parent), svgRenderer(svg)
        { setMinimumHeight(380); }

        void setFrame(const QImage &img) { frame = img; update(); }

        // Render SVG at given size, tinting every non-transparent pixel to `color`
        QImage renderTinted(QSize sz, QColor color) {
            QImage img(sz, QImage::Format_ARGB32);
            img.fill(Qt::transparent);
            QPainter rp(&img);
            svgRenderer.render(&rp);
            rp.end();
            const int r = color.red(), g = color.green(), b = color.blue();
            for (int y = 0; y < img.height(); ++y) {
                auto *line = reinterpret_cast<QRgb*>(img.scanLine(y));
                for (int x = 0; x < img.width(); ++x) {
                    int a = qAlpha(line[x]);
                    if (a > 8)
                        line[x] = qRgba(r, g, b, a);
                }
            }
            return img;
        }

        // Detect a card: sample a downscaled version for speed.
        // A CIN card is a light rectangle; center avg luma must be sufficiently
        // bright AND higher than the surrounding border region.
        static bool detectCard(const QImage &src) {
            if (src.isNull()) return false;
            // Downscale to 80×60 for fast sampling
            QImage img = src.scaled(80, 60, Qt::IgnoreAspectRatio,
                                    Qt::FastTransformation)
                            .convertToFormat(QImage::Format_RGB32);
            const int w = img.width(), h = img.height();
            const int x0 = w*25/100, x1 = w*75/100;
            const int y0 = h*22/100, y1 = h*78/100;
            long long cSum = 0, cCnt = 0, bSum = 0, bCnt = 0;
            for (int y = 0; y < h; ++y) {
                const QRgb *line = reinterpret_cast<const QRgb*>(img.scanLine(y));
                for (int x = 0; x < w; ++x) {
                    int lum = qGray(line[x]);
                    if (x >= x0 && x < x1 && y >= y0 && y < y1)
                    { cSum += lum; ++cCnt; }
                    else
                    { bSum += lum; ++bCnt; }
                }
            }
            if (!cCnt || !bCnt) return false;
            double ac = double(cSum) / cCnt;
            double ab = double(bSum) / bCnt;
            // Card detected: center is bright (>100) AND brighter than border by >18
            return (ac > 100 && (ac - ab) > 18);
        }

        // Hysteresis: require 4 consecutive matching frames to flip state
        void updateDetection(const QImage &img) {
            bool d = detectCard(img);
            if (d == cardDetected) {
                detectionFrames = 0;
            } else {
                ++detectionFrames;
                if (detectionFrames >= 4) {
                    cardDetected = d;
                    detectionFrames = 0;
                    update();
                }
            }
        }

    protected:
        void paintEvent(QPaintEvent *) override {
            QPainter p(this);
            p.setRenderHint(QPainter::SmoothPixmapTransform);
            p.setRenderHint(QPainter::Antialiasing);

            // 1. Black background
            p.fillRect(rect(), Qt::black);

            // 2. Camera frame, aspect-ratio-correct, centred
            if (!frame.isNull()) {
                QSize fs = frame.size().scaled(size(), Qt::KeepAspectRatio);
                QRect dr((width()-fs.width())/2,
                         (height()-fs.height())/2,
                         fs.width(), fs.height());
                p.drawImage(dr, frame);
            }

            // 3. SVG guide-frame: pixel-tint approach, immune to SVG color format
            if (svgRenderer.isValid()) {
                int maxW = int(width()  * 1.45);
                int maxH = int(height() * 1.45);
                QSizeF svgNative = svgRenderer.defaultSize();
                QSizeF fitted   = svgNative.isValid()
                    ? svgNative.scaled(QSizeF(maxW, maxH), Qt::KeepAspectRatio)
                    : QSizeF(maxW, maxH);

                QColor tint = cardDetected
                    ? QColor(0, 230, 118)   // #00e676 green
                    : QColor(255, 255, 255); // white

                QImage tinted = renderTinted(fitted.toSize(), tint);
                int ox = (width()  - tinted.width())  / 2;
                int oy = (height() - tinted.height()) / 2;
                p.drawImage(ox, oy, tinted);
            }
        }
    };

    // ── Build dialog ──
    auto *dlg = new QDialog(this, Qt::Window);
    dlg->setWindowTitle(instruction);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setMinimumSize(720, 580);
    dlg->setStyleSheet(
        "QDialog { background:#0f172a; }"
        "QLabel#instrLabel {"
        "  background: rgba(20,184,166,0.18);"
        "  border: 1.5px solid #14B8A6;"
        "  border-radius: 10px;"
        "  color: #ffffff;"
        "  font-size: 15px; font-weight: 700;"
        "  padding: 10px 20px;"
        "}"
        "QLabel#statusLabel {"
        "  color: #94a3b8; font-size: 13px;"
        "  background: transparent;"
        "}"
        "QPushButton#btnCapture {"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "    stop:0 #14B8A6, stop:1 #0d9488);"
        "  color: white; border: none;"
        "  padding: 14px 48px; border-radius: 12px;"
        "  font-size: 16px; font-weight: 800; min-height: 50px;"
        "}"
        "QPushButton#btnCapture:hover {"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "    stop:0 #2dd4c1, stop:1 #14B8A6);"
        "}"
        "QPushButton#btnCapture:disabled {"
        "  background: #334155; color: #64748b;"
        "}"
        "QPushButton#btnCancel2 {"
        "  background: transparent; border: 1.5px solid #475569;"
        "  color: #94a3b8; padding: 14px 32px;"
        "  border-radius: 12px; font-size: 14px; font-weight: 600;"
        "}"
        "QPushButton#btnCancel2:hover { border-color:#94a3b8; color:#e2e8f0; }"
    );

    auto *root = new QVBoxLayout(dlg);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    // Instruction banner
    auto *instrLbl = new QLabel(QString::fromUtf8("\xF0\x9F\x93\xB7") + "  " + instruction);
    instrLbl->setObjectName("instrLabel");
    instrLbl->setAlignment(Qt::AlignCenter);
    root->addWidget(instrLbl);

    // Camera + overlay view — load SVG bytes from Qt resource
    QByteArray svgBytes;
    QFile svgFile(frameResource);
    if (svgFile.open(QIODevice::ReadOnly))
        svgBytes = svgFile.readAll();
    auto *camView = new CameraView(svgBytes, dlg);
    root->addWidget(camView, 1);

    // Status hint
    auto *hintLbl = new QLabel(
        "\u2197 Align the card with the guide frame, then press Capture");
    hintLbl->setObjectName("statusLabel");
    hintLbl->setAlignment(Qt::AlignCenter);
    root->addWidget(hintLbl);

    // Buttons row
    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(14);
    auto *cancelBtn  = new QPushButton("\u2715\u00A0\u00A0Cancel");
    cancelBtn->setObjectName("btnCancel2");
    auto *captureBtn = new QPushButton(
        QString::fromUtf8("\xF0\x9F\x93\xB8") + "  Capture");
    captureBtn->setObjectName("btnCapture");
    btnRow->addWidget(cancelBtn);
    btnRow->addStretch();
    btnRow->addWidget(captureBtn);
    root->addLayout(btnRow);

    // ── Camera setup via QVideoSink ──
    // QCamera() with no args uses the system default camera device.
    auto *camera  = new QCamera(dlg);
    auto *session = new QMediaCaptureSession(dlg);
    auto *sink    = new QVideoSink(dlg);
    session->setCamera(camera);
    session->setVideoSink(sink);      // feed raw frames into our custom widget

    // ── Handle camera errors (no device, privacy blocked, driver missing…) ──
    connect(camera, &QCamera::errorOccurred, dlg,
            [this, dlg, hintLbl, captureBtn, callback]
            (QCamera::Error /*err*/, const QString &errStr) {
        hintLbl->setStyleSheet("color:#f87171; font-size:13px; background:transparent;");
        hintLbl->setText("\u26A0  Camera error: " + errStr);
        captureBtn->setEnabled(false);
        QTimer::singleShot(2500, this, [this, dlg, callback, errStr]() {
            dlg->reject();
            showToast("\u26A0  Camera unavailable: " + errStr +
                      "\nCheck: Settings \u2192 Privacy \u2192 Camera \u2192 ON");
            callback("");
        });
    });

    // Push each frame into the custom view + run card detection
    connect(sink, &QVideoSink::videoFrameChanged, dlg,
            [camView, hintLbl](const QVideoFrame &vf) {
        if (!vf.isValid()) return;
        QVideoFrame copy = vf;

        // ── Method 1: toImage() — works on most backends ──
        QImage img = copy.toImage();

        // ── Method 2: map() raw bytes — fallback when toImage() returns null ──
        // Some Windows Media Foundation / FFmpeg backends return NV12/P010
        // frames that Qt's toImage() cannot decode; map() gives raw byte access.
        if (img.isNull() && copy.isValid()) {
            if (copy.map(QVideoFrame::ReadOnly)) {
                int w = copy.width(), h = copy.height();
                if (w > 0 && h > 0) {
                    int stride = copy.bytesPerLine(0);
                    const uchar *bits = copy.bits(0);
                    if (bits && stride > 0) {
                        // stride >= w*4 → packed ARGB/BGRA 32-bit
                        if (stride >= w * 4) {
                            img = QImage(bits, w, h, stride,
                                         QImage::Format_ARGB32).copy();
                        }
                        // stride >= w*3 → packed RGB888
                        else if (stride >= w * 3) {
                            img = QImage(bits, w, h, stride,
                                         QImage::Format_RGB888).copy();
                        }
                        // NV12 / YUV: use luminance plane as grayscale preview
                        else {
                            img = QImage(bits, w, h, stride,
                                         QImage::Format_Grayscale8).copy();
                        }
                    }
                }
                copy.unmap();
            }
        }

        // ── Normalise to RGB32 so QImage::save() always works ──
        if (!img.isNull()
            && img.format() != QImage::Format_RGB32
            && img.format() != QImage::Format_ARGB32) {
            img = img.convertToFormat(QImage::Format_RGB32);
        }

        if (!img.isNull()) {
            bool wasDetected = camView->cardDetected;
            camView->setFrame(img);
            camView->updateDetection(img);
            // Update hint label only when state changes
            if (camView->cardDetected != wasDetected) {
                if (camView->cardDetected)
                    hintLbl->setText("\u2705  Card detected \u2014 press Capture when ready");
                else
                    hintLbl->setText("\u2197 Align the card with the guide frame, then press Capture");
            }
        }
    });

    camera->start();

    // ── Capture button — save QVideoSink frame directly (bypasses QImageCapture) ──
    // QImageCapture::captureToFile() throws "Unsupported image format" with the
    // FFmpeg backend on some Windows setups.  We already have every live frame
    // as a QImage inside camView->frame, so we just save that with QImage::save().
    connect(captureBtn, &QPushButton::clicked, dlg, [=]() {
        // Allow capture even if frame is null — grab() fallback will be used
        captureBtn->setEnabled(false);

        auto *countdown = new QTimer(dlg);
        countdown->setInterval(1000);
        auto *remaining = new int(3);

        hintLbl->setText("\U0001F4F7  Capturing in  3\u2026");

        connect(countdown, &QTimer::timeout, dlg, [=]() {
            (*remaining)--;
            if (*remaining > 0) {
                hintLbl->setText(QString("\U0001F4F7  Capturing in  %1\u2026")
                                     .arg(*remaining));
            } else {
                countdown->stop();
                delete remaining;
                hintLbl->setText("\u23F3  Capturing\u2026");

                // ── Grab live frame and save to disk ─────────────────────
                // Method A: stored RGB32 frame from videoFrameChanged handler
                QImage snap = camView->frame;

                // Method B: pull directly from sink + map() raw bytes
                if (snap.isNull()) {
                    QVideoFrame live = sink->videoFrame();
                    if (live.isValid()) {
                        snap = live.toImage();
                        if (snap.isNull() && live.map(QVideoFrame::ReadOnly)) {
                            int w = live.width(), h = live.height();
                            int stride = live.bytesPerLine(0);
                            const uchar *bits = live.bits(0);
                            if (bits && stride > 0 && w > 0 && h > 0) {
                                if (stride >= w * 4)
                                    snap = QImage(bits, w, h, stride, QImage::Format_ARGB32).copy();
                                else if (stride >= w * 3)
                                    snap = QImage(bits, w, h, stride, QImage::Format_RGB888).copy();
                                else
                                    snap = QImage(bits, w, h, stride, QImage::Format_Grayscale8).copy();
                            }
                            live.unmap();
                        }
                    }
                }

                // Method C: render the CameraView widget directly to a QImage.
                // This is GUARANTEED to produce a valid image because it captures
                // exactly what is painted on screen (camera frame + card guide overlay).
                // Groq Vision can read card text even with the guide overlay present.
                if (snap.isNull()) {
                    QPixmap grabbed = camView->grab();
                    if (!grabbed.isNull())
                        snap = grabbed.toImage();
                }

                // Normalise to RGB32 before saving
                if (!snap.isNull()
                    && snap.format() != QImage::Format_RGB32
                    && snap.format() != QImage::Format_ARGB32) {
                    snap = snap.convertToFormat(QImage::Format_RGB32);
                }

                // Build save path — use Desktop as final fallback if Temp fails
                auto tryDir = [](const QString &d) -> QString {
                    QDir().mkpath(d);
                    return QDir(d).isReadable() ? d : QString();
                };
                QString dir = tryDir(QStandardPaths::writableLocation(
                                         QStandardPaths::TempLocation));
                if (dir.isEmpty())
                    dir = tryDir(QStandardPaths::writableLocation(
                                     QStandardPaths::DesktopLocation));
                if (dir.isEmpty())
                    dir = QDir::homePath();

                QString base = "/cin_" + QUuid::createUuid()
                                             .toString(QUuid::WithoutBraces).left(8);

                bool saved = false;
                QString file;

                if (!snap.isNull()) {
                    // 1. Try PNG — always built into Qt, no plugin required
                    file = dir + base + ".png";
                    saved = snap.save(file, "PNG");

                    // 2. Try JPEG
                    if (!saved) {
                        file = dir + base + ".jpg";
                        saved = snap.save(file, "JPEG", 90);
                    }

                    // 3. Try BMP — simplest format, always works
                    if (!saved) {
                        file = dir + base + ".bmp";
                        saved = snap.save(file, "BMP");
                    }

                    // 4. QPixmap path as last resort
                    if (!saved) {
                        file = dir + base + ".png";
                        QPixmap pix = QPixmap::fromImage(snap);
                        if (!pix.isNull()) saved = pix.save(file, "PNG");
                    }
                }

                if (!saved) {
                    hintLbl->setText("\u26A0  Save failed — try again.");
                    captureBtn->setEnabled(true);
                    return;
                }

                hintLbl->setText("\u2713  Captured! Processing\u2026");
                camera->stop();
                dlg->accept();
                callback(file);
            }
        });
        countdown->start();
    });

    // ── Cancel button ──
    connect(cancelBtn, &QPushButton::clicked, dlg, [dlg, camera, &callback]() {
        camera->stop();
        dlg->reject();
        callback("");
    });

    // Stop camera on close
    connect(dlg, &QDialog::finished, dlg, [camera](int) {
        camera->stop();
    });

    dlg->exec();
}

// ══════════════════════════════════════════════════════════════
// OCR Implementation — native C++ using Groq Vision API
// ══════════════════════════════════════════════════════════════

// (imageToBase64DataUri removed — OCR.space now uses multipart file upload)

void MainWindow::startOcrScan(const QString &frontPath, const QString &backPath)
{
    // Show progress dialog
    QProgressDialog *progress = new QProgressDialog(
        "Scanning ID card with OCR...\nThis may take a moment.",
        "Cancel", 0, 0, this);
    progress->setWindowTitle("OCR Processing");
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->show();

    QString frontPrompt = QStringLiteral(
        "You are an OCR engine. The image shows the FRONT of a Tunisian national ID card (بطاقة التعريف الوطنية).\n"
        "Read the actual text printed on the card in the image and extract these fields.\n"
        "Respond ONLY with a raw JSON object — no markdown, no code fences, no explanation.\n"
        "\n"
        "Fields to extract:\n"
        "  \"cin\"       : the 8-digit CIN number printed on the card\n"
        "  \"lastName\"  : the FAMILY/SURNAME labelled اللقب (first name line on Tunisian cards)\n"
        "  \"firstName\" : the PERSONAL/GIVEN name labelled الاسم (second name line on Tunisian cards)\n"
        "  \"dob\"       : date of birth labelled تاريخ الولادة, exactly as printed on the card\n"
        "  \"birthPlace\": place of birth labelled مكان الولادة or ب\n"
        "\n"
        "IMPORTANT:\n"
        "- Read the actual values from the image — do NOT invent or guess values.\n"
        "- اللقب is the family name (surname). الاسم is the personal first name.\n"
        "- Use the Arabic text, not the French transliteration.\n"
        "- If a field is not readable, use an empty string \"\".\n"
        "- Output format: {\"cin\":\"<value>\",\"lastName\":\"<value>\",\"firstName\":\"<value>\",\"dob\":\"<value>\",\"birthPlace\":\"<value>\"}"
    );

    QString backPrompt = QStringLiteral(
        "You are an OCR engine. The image shows the BACK of a Tunisian national ID card (بطاقة التعريف الوطنية).\n"
        "Read the actual text printed on the card in the image and extract these fields.\n"
        "Respond ONLY with a raw JSON object — no markdown, no code fences, no explanation.\n"
        "\n"
        "Fields to extract:\n"
        "  \"address\": the street / neighbourhood part of the address as printed on the card\n"
        "  \"city\"   : ONLY the city or governorate name — the last word/part of the address line\n"
        "\n"
        "IMPORTANT:\n"
        "- Read the actual values from the image — do NOT invent or guess values.\n"
        "- \"city\" must be a single city or governorate name, NOT the full address.\n"
        "- If a field is not readable, use an empty string \"\".\n"
        "- Output format: {\"address\":\"<street address as printed>\",\"city\":\"<city name only>\"}"
    );

    // Send front image request first
    sendOcrRequest(frontPath, frontPrompt,
        [this, backPath, backPrompt, progress](const QString &frontText) {

        if (progress->wasCanceled()) {
            progress->deleteLater();
            return;
        }

        if (frontText.startsWith("ERROR:")) {
            progress->close();
            progress->deleteLater();
            QMessageBox::warning(this, "OCR Error", frontText.mid(6));
            return;
        }

        progress->setLabelText("Front side done. Scanning back side...");

        // Wait 2s before back request — helloworld key rate-limits to 1 req/s
        QTimer::singleShot(2000, this, [this, backPath, backPrompt, frontText, progress]() {
        sendOcrRequest(backPath, backPrompt,
            [this, frontText, progress](const QString &backText) {

            progress->close();
            progress->deleteLater();

            if (backText.startsWith("ERROR:")) {
                QMessageBox::warning(this, "OCR Error", backText.mid(6));
                return;
            }

            processOcrResults(frontText, backText);
        }, progress);
        }); // end QTimer::singleShot
    }, progress);
}

void MainWindow::sendOcrRequest(const QString &imagePath, const QString &prompt,
                                 std::function<void(const QString &text)> callback,
                                 QProgressDialog *progress)
{
    Q_UNUSED(prompt); // OCR.space does not need a text prompt — it extracts text directly

    const QString apiKey = getOcrSpaceApiKey();

    // ── Prepare image bytes (JPEG, max 1024px) ──
    QImage img(imagePath);
    if (img.isNull()) {
        callback("ERROR:Could not read the captured image file.\nPath: " + imagePath);
        return;
    }
    if (img.width() > 1024 || img.height() > 1024)
        img = img.scaled(1024, 1024, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QByteArray imgBytes;
    {
        QBuffer buf(&imgBytes);
        buf.open(QIODevice::WriteOnly);
        if (!img.save(&buf, "JPEG", 85)) {
            imgBytes.clear();
            QBuffer buf2(&imgBytes);
            buf2.open(QIODevice::WriteOnly);
            img.save(&buf2, "PNG");
        }
    }
    if (imgBytes.isEmpty()) {
        callback("ERROR:Could not encode the captured image.");
        return;
    }
    QString mimeType = imgBytes.startsWith("\xFF\xD8") ? "image/jpeg" : "image/png";
    QString fileExt  = imgBytes.startsWith("\xFF\xD8") ? "card.jpg"   : "card.png";

    // Try engines in order: 3 → 2 → 1
    static const QList<int> engines = {3, 2, 1};

    auto tryEngine = [this, apiKey, imgBytes, mimeType, fileExt, callback, progress](int engineIdx) {
        auto tryEngineImpl = std::make_shared<std::function<void(int)>>();
        *tryEngineImpl = [this, apiKey, imgBytes, mimeType, fileExt,
                          callback, progress, tryEngineImpl](int idx) {

            if (idx >= engines.size()) {
                callback("ERROR:All OCR engines failed.\n"
                         "Check ocr_debug.txt next to the exe for details.");
                return;
            }
            if (progress && progress->wasCanceled()) {
                callback("ERROR:Cancelled by user.");
                return;
            }

            int engine = engines[idx];

            // ── Use multipart/form-data — avoids all URL-encoding issues ──
            auto *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

            auto addField = [&](const QString &name, const QString &value) {
                QHttpPart p;
                p.setHeader(QNetworkRequest::ContentDispositionHeader,
                            QString("form-data; name=\"%1\"").arg(name));
                p.setBody(value.toUtf8());
                multiPart->append(p);
            };

            addField("apikey",            apiKey);
            addField("OCREngine",         QString::number(engine));
            addField("detectOrientation", "true");
            addField("isOverlayRequired", "false");
            addField("scale",             "true");
            // Note: do NOT set language for engines 2/3 (only eng supported);
            // engine 1 supports Arabic via auto-detection without explicit lang param
            if (engine == 1)
                addField("language", "ara");

            // Image file part
            QHttpPart filePart;
            filePart.setHeader(QNetworkRequest::ContentTypeHeader, mimeType);
            filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                               QString("form-data; name=\"file\"; filename=\"%1\"").arg(fileExt));
            filePart.setBody(imgBytes);
            multiPart->append(filePart);

            QNetworkRequest request{QUrl(OCRSPACE_API_URL)};
            request.setTransferTimeout(60000);

            QNetworkReply *reply = networkManager->post(request, multiPart);
            multiPart->setParent(reply); // auto-delete with reply

            if (progress)
                connect(progress, &QProgressDialog::canceled, reply, &QNetworkReply::abort);

            connect(reply, &QNetworkReply::finished, this,
                [reply, callback, idx, tryEngineImpl, engine]() {
                reply->deleteLater();

                if (reply->error() == QNetworkReply::OperationCanceledError) {
                    callback("ERROR:Cancelled by user.");
                    return;
                }

                // Always read response body — HTTP errors may still have JSON
                QByteArray responseData = reply->readAll();
                int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

                // Pure network failure (no response at all)
                if (reply->error() != QNetworkReply::NoError && responseData.isEmpty()) {
                    callback("ERROR:Network error: " + reply->errorString());
                    return;
                }

                // Rate-limited (429) — retry same engine after 2 seconds
                if (httpCode == 429) {
                    QTimer::singleShot(2500, [tryEngineImpl, idx]() {
                        (*tryEngineImpl)(idx); // retry same engine
                    });
                    return;
                }

                // Debug log
                {
                    QFile dbg(QCoreApplication::applicationDirPath() + "/ocr_debug.txt");
                    if (dbg.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
                        dbg.write(("=== Engine " + QString::number(engine) + " ===\n").toUtf8());
                        dbg.write(responseData.left(3000));
                        dbg.write("\n\n");
                    }
                }

                QJsonObject obj = QJsonDocument::fromJson(responseData).object();

                if (obj["IsErroredOnProcessing"].toBool()) {
                    // Try next engine silently
                    (*tryEngineImpl)(idx + 1);
                    return;
                }

                QJsonArray results = obj["ParsedResults"].toArray();
                if (results.isEmpty()) {
                    (*tryEngineImpl)(idx + 1);
                    return;
                }

                QString text = results[0].toObject()["ParsedText"].toString().trimmed();
                if (text.isEmpty()) {
                    (*tryEngineImpl)(idx + 1);
                    return;
                }

                callback(text);
            });
        };
        (*tryEngineImpl)(engineIdx);
    };

    tryEngine(0);
}

void MainWindow::processOcrResults(const QString &frontText, const QString &backText)
{
    // ══════════════════════════════════════════════════════════════
    // OCR.space returns raw text — extract fields with regex
    // ══════════════════════════════════════════════════════════════

    // ── CIN: 8-digit number ──
    QString cin;
    {
        QRegularExpression rx("\\b(\\d{8})\\b");
        QRegularExpressionMatch m = rx.match(frontText);
        if (m.hasMatch()) cin = m.captured(1);
    }

    // ── Lines of the front card (strip blank / whitespace lines) ──
    QStringList frontLines;
    for (const QString &ln : frontText.split('\n')) {
        QString t = ln.trimmed();
        if (!t.isEmpty()) frontLines.append(t);
    }

    // Strip leading colon / Arabic comma from OCR tokens
    auto stripLeadingPunct = [](QString s) -> QString {
        while (!s.isEmpty() && (s[0] == QLatin1Char(':') || s.startsWith(QString("،"))))
            s = s.mid(1).trimmed();
        return s;
    };

    // ── Last name / first name / DOB / birth place from front card ──
    QString lastName, firstName, dob, birthPlace;
    {
        static const QStringList lastNameMarkers  = { "اللقب", "لقب" };
        static const QStringList firstNameMarkers = { "الاسم", "اسم" };
        static const QStringList dobMarkers       = { "تاريخ الولادة", "تاريخ ولادة", "الولادة" };
        static const QStringList birthMarkers     = { "مكان الولادة", "مكان ولادة", "ولد في", "ولدت في" };

        auto extractAfterMarker = [&](const QStringList &markers, const QString &text) -> QString {
            for (const QString &marker : markers) {
                int pos = text.indexOf(marker);
                if (pos >= 0) {
                    QString after = stripLeadingPunct(text.mid(pos + marker.length()).trimmed());
                    QString firstLine = stripLeadingPunct(after.section('\n', 0, 0).trimmed());
                    if (!firstLine.isEmpty()) return firstLine;
                }
            }
            return {};
        };

        lastName   = extractAfterMarker(lastNameMarkers,  frontText);
        firstName  = extractAfterMarker(firstNameMarkers, frontText);
        dob        = extractAfterMarker(dobMarkers,       frontText);
        birthPlace = extractAfterMarker(birthMarkers,     frontText);

        // ── Fallback: if labels not found, use positional lines ──
        // Front card structure (after header lines):
        // Line 0/1: الجمهورية التونسية / بطاقة التعريف الوطنية  (headers)
        // Then: CIN number, last name, first name, DOB, birth place
        if (lastName.isEmpty() || firstName.isEmpty()) {
            QStringList contentLines;
            for (const QString &ln : frontLines) {
                // Skip header lines and the CIN number line
                if (ln.contains("الجمهورية") || ln.contains("بطاقة") || ln.contains("التعريف"))
                    continue;
                if (QRegularExpression("^\\d{8}$").match(ln).hasMatch())
                    continue;
                contentLines.append(ln);
            }
            if (lastName.isEmpty()  && contentLines.size() >= 1) lastName  = contentLines[0];
            if (firstName.isEmpty() && contentLines.size() >= 2) firstName = contentLines[1];
            if (dob.isEmpty()       && contentLines.size() >= 3) {
                // DOB line likely contains digits + Arabic month name
                for (const QString &ln : contentLines) {
                    if (QRegularExpression("\\d").match(ln).hasMatch() &&
                        (ln.contains("جانفي") || ln.contains("فيفري") || ln.contains("مارس") ||
                         ln.contains("أفريل") || ln.contains("ماي")   || ln.contains("جوان") ||
                         ln.contains("جويلية")|| ln.contains("اوت")   || ln.contains("أوت")  ||
                         ln.contains("سبتمبر")|| ln.contains("اكتوبر")|| ln.contains("نوفمبر")||
                         ln.contains("ديسمبر"))) {
                        dob = ln; break;
                    }
                }
            }
        }
    }

    // ── Address and city from back card ──
    QString address, city;
    {
        // "المعنون" is OCR misread of "المعنوان"/"العنوان"
        static const QStringList addrMarkers = {
            "العنوان", "المعنون", "المعنوان", "عنوان", "المنزل", "يقيم", "تقيم"
        };

        auto extractAfterMarkerBack = [&](const QStringList &markers, const QString &text) -> QString {
            for (const QString &marker : markers) {
                int pos = text.indexOf(marker);
                if (pos >= 0) {
                    QString after = stripLeadingPunct(text.mid(pos + marker.length()).trimmed());
                    QString firstLine = stripLeadingPunct(after.section('\n', 0, 0).trimmed());
                    if (!firstLine.isEmpty()) return firstLine;
                }
            }
            return {};
        };
        address = extractAfterMarkerBack(addrMarkers, backText);

        // City: extract from back text lines — line after address or known city name
        // From debug: back card shows address line then city on next line
        if (!address.isEmpty()) {
            // Find the line after the address line
            QStringList backLines;
            for (const QString &ln : backText.split('\n')) {
                QString t = ln.trimmed();
                if (!t.isEmpty()) backLines.append(t);
            }
            int addrIdx = backLines.indexOf(address);
            if (addrIdx >= 0 && addrIdx + 1 < backLines.size())
                city = backLines[addrIdx + 1];
        }

        // Fallback: use meaningful back text lines as address
        if (address.isEmpty()) {
            QStringList backLines;
            for (const QString &ln : backText.split('\n')) {
                QString t = ln.trimmed();
                if (!t.isEmpty() && !t.contains("اسم رقيب") && !t.contains("المدير")
                    && !t.contains("تاريخ") && !t.contains("نورش") && !t.contains("شهادة"))
                    backLines.append(t);
            }
            if (!backLines.isEmpty()) address = backLines.join("، ");
        }
    }

    // ── CIN fallback (already extracted above via regex) ──

    static const QStringList tunisianCities = {
        "تونس", "صفاقس", "سوسة", "قابس", "مدنين",
        "بنزرت", "نابل", "باجة", "جندوبة", "الكاف",
        "سليانة", "القصرين", "سيدي بوزيد", "قفصة", "توزر",
        "المهدية", "المنستير", "القيروان", "زغوان", "منوبة",
        "أريانة", "بن عروس", "تطاوين", "قبلي", "دوز"
    };

    // ═══ If city looks like a full address, extract only the city part ═══
    if (!city.isEmpty()) {
        // If city contains a digit it's probably the full address — take last word/token
        bool looksLikeAddress = city.contains(QRegularExpression("\\d")) || city.length() > 30;
        if (looksLikeAddress) {
            // Try to find a known city name inside the returned string
            QString found;
            for (const QString &c : tunisianCities) {
                if (city.contains(c)) { found = c; break; }
            }
            city = found; // empty if none matched — will fall through to next block
        }
    }

    // ═══ Cross-check: if model's city is not found in the address field,
    //     prefer the city extracted directly from the address.
    //     This handles cases where the model assigns the birthPlace as the city. ═══
    if (!city.isEmpty() && !address.isEmpty() && !address.contains(city)) {
        QString foundInAddress;
        for (const QString &c : tunisianCities) {
            if (address.contains(c)) { foundInAddress = c; break; }
        }
        if (!foundInAddress.isEmpty())
            city = foundInAddress;
    }

    // ═══ If city still empty, try to extract from address ═══
    if (city.isEmpty() && !address.isEmpty()) {
        for (const QString &c : tunisianCities) {
            if (address.contains(c)) { city = c; break; }
        }
    }

    // ═══ Last resort: scan all OCR text for a known city ═══
    if (city.isEmpty()) {
        QString allText = frontText + " " + backText;
        for (const QString &c : tunisianCities) {
            if (allText.contains(c)) { city = c; break; }
        }
    }

    // ═══ Parse DOB into ISO date ═══
    if (!dob.isEmpty()) {
        QMap<QString, QString> months;
        months["جانفي"]  = "01"; months["فيفري"]  = "02"; months["مارس"]   = "03";
        months["أفريل"]  = "04"; months["افريل"]  = "04";
        months["ماي"]    = "05"; months["جوان"]   = "06";
        months["جويلية"] = "07"; months["جولية"]  = "07";
        months["اوت"]    = "08"; months["أوت"]    = "08";
        months["سبتمبر"] = "09";
        months["اكتوبر"] = "10"; months["أكتوبر"] = "10";
        months["نوفمبر"] = "11"; months["ديسمبر"] = "12";

        QRegularExpression numRx("\\d+");
        QRegularExpressionMatchIterator it = numRx.globalMatch(dob);
        QStringList numbers;
        while (it.hasNext()) numbers.append(it.next().captured());

        QString monthNum;
        for (auto mi = months.constBegin(); mi != months.constEnd(); ++mi) {
            if (dob.contains(mi.key())) {
                monthNum = mi.value();
                break;
            }
        }

        if (numbers.size() >= 2 && !monthNum.isEmpty()) {
            QString year, day;
            // 4-digit = year, 1-2 digit = day
            if (numbers[0].length() == 4) {
                year = numbers[0];
                day = numbers.last().rightJustified(2, '0');
            } else if (numbers.last().length() == 4) {
                day = numbers[0].rightJustified(2, '0');
                year = numbers.last();
            } else {
                day = numbers[0].rightJustified(2, '0');
                year = numbers.last();
                if (year.length() == 2)
                    year = (year.toInt() < 50 ? "20" : "19") + year;
            }
            dob = year + "-" + monthNum + "-" + day;
        }
    }

    // ═══ Fill form fields ═══
    if (!cin.isEmpty())       ui->editCIN->setText(cin);
    if (!firstName.isEmpty()) ui->editFirstName->setText(firstName);
    if (!lastName.isEmpty())  ui->editLastName->setText(lastName);
    if (!address.isEmpty())   ui->editAddress->setText(address);
    if (!city.isEmpty())      ui->editCity->setText(city);

    if (!dob.isEmpty()) {
        QDate date = QDate::fromString(dob, "yyyy-MM-dd");
        if (date.isValid())
            ui->editDOB->setDate(date);
    }

}

// ══════════════════════════════════════════════════════════════
// UI animation helpers
// ══════════════════════════════════════════════════════════════

// Top-right toast notification
void MainWindow::showToast(const QString &message, bool isError)
{
    // Stack below existing toasts
    int stackOffset = 0;
    for (auto *child : findChildren<QLabel*>()) {
        if (child->property("isToast").toBool())
            stackOffset += child->height() + 8;
    }

    auto *toast = new QLabel(this);
    toast->setProperty("isToast", true);
    toast->setText(message);
    toast->setWordWrap(true);
    toast->setMaximumWidth(360);
    toast->setMinimumWidth(240);

    // Glassmorphism toast colours
    QString bg      = isError ? "rgba(220,38,38,0.88)"    : "rgba(13,148,136,0.88)";
    QString border  = isError ? "rgba(255,255,255,0.25)"  : "rgba(255,255,255,0.25)";
    QString accent  = isError ? "rgba(255,255,255,0.70)"  : "rgba(255,255,255,0.70)";
    toast->setStyleSheet(QString(
        "QLabel {"
        "  background: %1;"
        "  color: #ffffff;"
        "  border: 1px solid %2;"
        "  border-left: 4px solid %3;"
        "  border-radius: 14px;"
        "  padding: 13px 18px;"
        "  font-size: 13px;"
        "  font-weight: 700;"
        "  letter-spacing: 0.2px;"
        "}"
    ).arg(bg, border, accent));

    toast->adjustSize();
    const int margin = 16;
    const int posX   = width() - toast->sizeHint().width() - margin;
    const int posY   = margin + stackOffset;

    toast->move(width(), posY);   // start off-screen right
    toast->show();
    toast->raise();

    // Slide in
    auto *slideIn = new QPropertyAnimation(toast, "pos", toast);
    slideIn->setDuration(300);
    slideIn->setStartValue(QPoint(width(), posY));
    slideIn->setEndValue(QPoint(posX, posY));
    slideIn->setEasingCurve(QEasingCurve::OutCubic);
    slideIn->start(QAbstractAnimation::DeleteWhenStopped);

    // Auto-dismiss after 3.5 s
    QTimer::singleShot(3500, toast, [toast]() {
        if (!toast) return;
        auto *eff = new QGraphicsOpacityEffect(toast);
        toast->setGraphicsEffect(eff);
        auto *fade = new QPropertyAnimation(eff, "opacity", toast);
        fade->setDuration(400);
        fade->setStartValue(1.0);
        fade->setEndValue(0.0);
        QObject::connect(fade, &QPropertyAnimation::finished,
                         toast, &QLabel::deleteLater);
        fade->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

// Horizontal shake (invalid input feedback)
void MainWindow::shakeWidget(QWidget *w)
{
    const QRect geo = w->geometry();
    auto *anim = new QPropertyAnimation(w, "geometry", w);
    anim->setDuration(420);
    anim->setKeyValueAt(0.00, geo);
    anim->setKeyValueAt(0.15, geo.translated(9,  0));
    anim->setKeyValueAt(0.30, geo.translated(-7, 0));
    anim->setKeyValueAt(0.45, geo.translated(6,  0));
    anim->setKeyValueAt(0.60, geo.translated(-5, 0));
    anim->setKeyValueAt(0.75, geo.translated(3,  0));
    anim->setKeyValueAt(0.90, geo.translated(-2, 0));
    anim->setKeyValueAt(1.00, geo);
    anim->setEasingCurve(QEasingCurve::Linear);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

// Slide-down + fade-in for a panel widget
void MainWindow::showWidget(QWidget *w)
{
    w->setMaximumHeight(0);
    w->setVisible(true);

    auto *eff = new QGraphicsOpacityEffect(w);
    w->setGraphicsEffect(eff);
    eff->setOpacity(0.0);

    auto *hAnim = new QPropertyAnimation(w, "maximumHeight", w);
    hAnim->setDuration(380);
    hAnim->setStartValue(0);
    hAnim->setEndValue(300);
    hAnim->setEasingCurve(QEasingCurve::OutCubic);

    auto *oAnim = new QPropertyAnimation(eff, "opacity", w);
    oAnim->setDuration(380);
    oAnim->setStartValue(0.0);
    oAnim->setEndValue(1.0);
    oAnim->setEasingCurve(QEasingCurve::OutCubic);

    auto *grp = new QParallelAnimationGroup(w);
    grp->addAnimation(hAnim);
    grp->addAnimation(oAnim);
    grp->start(QAbstractAnimation::DeleteWhenStopped);
}

// Custom success dialog with fade-in and drop shadow
void MainWindow::showSuccess(const QString &title, const QString &message)
{
    QDialog *dlg = new QDialog(this, Qt::FramelessWindowHint | Qt::Dialog);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setAttribute(Qt::WA_TranslucentBackground);
    dlg->setModal(true);

    // Outer layout acts as padding for the drop-shadow
    auto *outerLayout = new QVBoxLayout(dlg);
    outerLayout->setContentsMargins(20, 20, 20, 28);

    // Card widget — white bg + rounded corners + drop shadow
    auto *card = new QWidget(dlg);
    card->setObjectName("successCard");
    card->setStyleSheet(
        "QWidget#successCard {"
        "  background: #ffffff;"
        "  border-radius: 20px;"
        "}"
    );

    auto *shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(40);
    shadow->setColor(QColor(0, 0, 0, 70));
    shadow->setOffset(0, 10);
    card->setGraphicsEffect(shadow);

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(40, 36, 40, 32);
    layout->setSpacing(10);

    // Circle icon badge
    auto *iconBadge = new QLabel("\u2713");
    iconBadge->setFixedSize(80, 80);
    iconBadge->setAlignment(Qt::AlignCenter);
    iconBadge->setStyleSheet(
        "background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
        "  stop:0 #14B8A6, stop:1 #0d9488);"
        "color: white;"
        "font-size: 40px;"
        "font-weight: 900;"
        "border-radius: 40px;"
    );

    auto *titleLbl = new QLabel(title);
    titleLbl->setAlignment(Qt::AlignCenter);
    titleLbl->setStyleSheet(
        "font-size: 20px; font-weight: 800; color: #111827;"
        "background: transparent;"
    );

    // Separator
    auto *sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("background:#E5E7EB; border:none; max-height:1px; margin: 4px 0;");

    auto *msgLbl = new QLabel(message);
    msgLbl->setAlignment(Qt::AlignCenter);
    msgLbl->setWordWrap(true);
    msgLbl->setStyleSheet(
        "font-size: 14px; color: #6B7280; background: transparent;"
    );

    auto *btn = new QPushButton("\u2713\u00A0\u00A0Continue");
    btn->setFixedHeight(46);
    btn->setStyleSheet(
        "QPushButton {"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "    stop:0 #14B8A6, stop:1 #0d9488);"
        "  color: white; padding: 0 32px; border-radius: 10px;"
        "  border: none; font-weight: 700; font-size: 15px;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "    stop:0 #2dd4c1, stop:1 #14B8A6);"
        "}"
        "QPushButton:pressed {"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "    stop:0 #0d9488, stop:1 #0a7e74);"
        "}"
    );
    connect(btn, &QPushButton::clicked, dlg, &QDialog::accept);

    layout->addWidget(iconBadge, 0, Qt::AlignHCenter);
    layout->addSpacing(4);
    layout->addWidget(titleLbl);
    layout->addWidget(sep);
    layout->addWidget(msgLbl);
    layout->addSpacing(12);
    layout->addWidget(btn);

    outerLayout->addWidget(card);

    // Fade-in the dialog
    auto *eff = new QGraphicsOpacityEffect(dlg);
    dlg->setGraphicsEffect(eff);
    eff->setOpacity(0.0);
    auto *fadeIn = new QPropertyAnimation(eff, "opacity", dlg);
    fadeIn->setDuration(240);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    fadeIn->setEasingCurve(QEasingCurve::OutCubic);
    fadeIn->start(QAbstractAnimation::DeleteWhenStopped);

    dlg->exec();
}

// ══════════════════════════════════════════════════════════════
// ══════════════════════════════════════════════════════════════
// Reset-password page  (page index 6 in QStackedWidget)
// ══════════════════════════════════════════════════════════════

void MainWindow::buildResetPasswordPage()
{
    // ── Outer page (same gradient background as other pages) ──
    QWidget *page = new QWidget();
    page->setObjectName("pageResetPassword");

    QVBoxLayout *outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addStretch(1);

    // ── Card ──────────────────────────────────────────────────
    QFrame *card = new QFrame(page);
    card->setObjectName("resetCard");
    card->setFixedWidth(480);
    card->setStyleSheet(
        "QFrame#resetCard{"
        "  background:white;"
        "  border-radius:20px;"
        "  border:none;"
        "}"
    );
    QGraphicsDropShadowEffect *sh = new QGraphicsDropShadowEffect(card);
    sh->setBlurRadius(40);
    sh->setOffset(0, 8);
    sh->setColor(QColor(0,0,0,30));
    card->setGraphicsEffect(sh);

    QVBoxLayout *cL = new QVBoxLayout(card);
    cL->setContentsMargins(48, 40, 48, 40);
    cL->setSpacing(16);

    // Icon circle
    QLabel *iconLbl = new QLabel(QString::fromUtf8("\xf0\x9f\x94\x92"), card);  // 🔒
    iconLbl->setFixedSize(72, 72);
    iconLbl->setAlignment(Qt::AlignCenter);
    iconLbl->setStyleSheet(
        "QLabel{background:#14B8A6;border-radius:36px;font-size:32px;"
        "color:white;border:none;}");
    QHBoxLayout *iconRow = new QHBoxLayout;
    iconRow->addStretch();
    iconRow->addWidget(iconLbl);
    iconRow->addStretch();
    cL->addLayout(iconRow);
    cL->addSpacing(4);

    // Title
    QLabel *title = new QLabel("Set New Password", card);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(
        "QLabel{font-size:24px;font-weight:900;color:#0f172a;"
        "background:transparent;border:none;}");
    cL->addWidget(title);

    // Subtitle
    QLabel *sub = new QLabel("Choose a strong password for your account.", card);
    sub->setAlignment(Qt::AlignCenter);
    sub->setWordWrap(true);
    sub->setStyleSheet(
        "QLabel{font-size:13px;color:#64748b;background:transparent;border:none;}");
    cL->addWidget(sub);
    cL->addSpacing(8);

    // Divider
    QFrame *div = new QFrame(card);
    div->setFrameShape(QFrame::HLine);
    div->setStyleSheet("QFrame{color:#e2e8f0;background:#e2e8f0;border:none;}");
    div->setFixedHeight(1);
    cL->addWidget(div);
    cL->addSpacing(8);

    // ── New Password field ─────────────────────────────────────
    QLabel *lbl1 = new QLabel("New Password", card);
    lbl1->setStyleSheet("QLabel{font-size:13px;font-weight:600;color:#374151;"
        "background:transparent;border:none;}");
    cL->addWidget(lbl1);

    m_editNewPass = new QLineEdit(card);
    m_editNewPass->setEchoMode(QLineEdit::Password);
    m_editNewPass->setPlaceholderText("At least 8 characters");
    m_editNewPass->setFixedHeight(44);
    m_editNewPass->setStyleSheet(
        "QLineEdit{background:#f8fafc;border:1.5px solid #e2e8f0;"
        "border-radius:10px;padding:0 14px;font-size:13px;color:#0f172a;}"
        "QLineEdit:focus{border-color:#14B8A6;background:white;}");
    cL->addWidget(m_editNewPass);

    // ── Confirm Password field ─────────────────────────────────
    QLabel *lbl2 = new QLabel("Confirm Password", card);
    lbl2->setStyleSheet("QLabel{font-size:13px;font-weight:600;color:#374151;"
        "background:transparent;border:none;}");
    cL->addWidget(lbl2);

    m_editConfirmPass = new QLineEdit(card);
    m_editConfirmPass->setEchoMode(QLineEdit::Password);
    m_editConfirmPass->setPlaceholderText("Repeat your new password");
    m_editConfirmPass->setFixedHeight(44);
    m_editConfirmPass->setStyleSheet(
        "QLineEdit{background:#f8fafc;border:1.5px solid #e2e8f0;"
        "border-radius:10px;padding:0 14px;font-size:13px;color:#0f172a;}"
        "QLineEdit:focus{border-color:#14B8A6;background:white;}");
    cL->addWidget(m_editConfirmPass);
    cL->addSpacing(8);

    // ── Save button ────────────────────────────────────────────
    QPushButton *saveBtn = new QPushButton("Save New Password", card);
    saveBtn->setFixedHeight(48);
    saveBtn->setCursor(Qt::PointingHandCursor);
    saveBtn->setStyleSheet(
        "QPushButton{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "  stop:0 #14B8A6,stop:1 #0d9488);"
        "  color:white;font-size:14px;font-weight:700;border:none;border-radius:12px;}"
        "QPushButton:hover{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "  stop:0 #0d9488,stop:1 #0f766e);}"
        "QPushButton:pressed{background:#0f766e;}"
    );
    connect(saveBtn, &QPushButton::clicked, this, [this, saveBtn]() {
        QString np = m_editNewPass->text();
        QString cp = m_editConfirmPass->text();

        m_editNewPass->setStyleSheet(
            "QLineEdit{background:#f8fafc;border:1.5px solid #e2e8f0;"
            "border-radius:10px;padding:0 14px;font-size:13px;color:#0f172a;}"
            "QLineEdit:focus{border-color:#14B8A6;background:white;}");
        m_editConfirmPass->setStyleSheet(
            "QLineEdit{background:#f8fafc;border:1.5px solid #e2e8f0;"
            "border-radius:10px;padding:0 14px;font-size:13px;color:#0f172a;}"
            "QLineEdit:focus{border-color:#14B8A6;background:white;}");

        if (np.isEmpty()) {
            showToast(QString::fromUtf8("\u26A0  Please enter a new password."));
            m_editNewPass->setStyleSheet(
                "QLineEdit{background:#FEF2F2;border:2px solid #DC2626;"
                "border-radius:10px;padding:0 14px;font-size:13px;}");
            shakeWidget(m_editNewPass);
            return;
        }
        if (np.length() < 8) {
            showToast(QString::fromUtf8("\u26A0  Password must be at least 8 characters."));
            m_editNewPass->setStyleSheet(
                "QLineEdit{background:#FEF2F2;border:2px solid #DC2626;"
                "border-radius:10px;padding:0 14px;font-size:13px;}");
            shakeWidget(m_editNewPass);
            return;
        }
        if (np != cp) {
            showToast(QString::fromUtf8("\u26A0  Passwords do not match."));
            m_editConfirmPass->setStyleSheet(
                "QLineEdit{background:#FEF2F2;border:2px solid #DC2626;"
                "border-radius:10px;padding:0 14px;font-size:13px;}");
            shakeWidget(m_editConfirmPass);
            return;
        }

        const QString hash = hashPassword(np);
        if (!updatePasswordInDB(m_resetEmail, hash)) {
            showToast(QString::fromUtf8("\u26A0  Could not update password. Email not found."));
            return;
        }

        saveBtn->setEnabled(false);
        showSuccess("Password Updated",
            "Your password has been changed successfully.\nYou can now log in with your new password.");
        m_resetEmail.clear();
        m_editNewPass->clear();
        m_editConfirmPass->clear();
        goToStep(0);
        saveBtn->setEnabled(true);
    });
    cL->addWidget(saveBtn);

    // ── Back to login link ─────────────────────────────────────
    QPushButton *backBtn = new QPushButton(QString::fromUtf8("\u2190 Back to Login"), card);
    backBtn->setFlat(true);
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setStyleSheet(
        "QPushButton{color:#64748b;font-size:12px;border:none;background:transparent;}"
        "QPushButton:hover{color:#14B8A6;}");
    connect(backBtn, &QPushButton::clicked, this, [this]() { goToStep(0); });
    QHBoxLayout *backRow = new QHBoxLayout;
    backRow->addStretch();
    backRow->addWidget(backBtn);
    backRow->addStretch();
    cL->addLayout(backRow);

    // ── Center card horizontally ──
    QHBoxLayout *hCenter = new QHBoxLayout;
    hCenter->addStretch();
    hCenter->addWidget(card);
    hCenter->addStretch();
    outer->addLayout(hCenter);
    outer->addStretch(1);

    // Add as page 6 of the stacked widget
    ui->stackedWidget->addWidget(page);
}

// ── Update password_hash in ADMIN + INSTRUCTORS + STUDENTS for given email ──
bool MainWindow::updatePasswordInDB(const QString &email, const QString &hash)
{
    bool updated = false;

    auto tryUpdate = [&](const QString &table) {
        QSqlQuery q;
        q.prepare(QString("UPDATE %1 SET password_hash=? "
                          "WHERE LOWER(email)=LOWER(?)").arg(table));
        q.addBindValue(hash);
        q.addBindValue(email.trimmed());
        if (q.exec() && q.numRowsAffected() > 0)
            updated = true;
    };

    tryUpdate("ADMIN");
    tryUpdate("INSTRUCTORS");
    tryUpdate("STUDENTS");

    return updated;
}

// Email verification via Gmail SMTP + TLS
// ══════════════════════════════════════════════════════════════

void MainWindow::sendVerificationEmail(const QString &toEmail,
                                       const QString &code,
                                       std::function<void(bool, QString)> callback)
{
    const QString fromEmail   = QStringLiteral("benaliwajih2@gmail.com");
    const QString appPassword = QString("urrx lftw jafm pfcu").remove(' ');

    // ── Build the MIME email payload ──────────────────────────────
    const QByteArray boundary = "----DS_MIME_BOUNDARY_7f3a9b";

    // Plain-text fallback
    QByteArray plain =
        "Your password reset verification code is:\r\n\r\n"
        "  " + code.toLatin1() + "\r\n\r\n"
        "This code expires in 10 minutes.\r\n"
        "If you did not request this, please ignore this email.\r\n"
        "\r\nDriving School App\r\n";

    // HTML body
    QByteArray html = QByteArray(
            "<!DOCTYPE html>"
            "<html lang=\"en\"><head><meta charset=\"UTF-8\"/>"
            "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"/>"
            "<title>Password Reset</title></head>"
            "<body style=\"margin:0;padding:0;background:#e0f5f1;"
            "font-family:'Segoe UI',Arial,sans-serif;\">"

            // ── outer wrapper ──
            "<table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\""
            "  style=\"background:#e0f5f1;padding:40px 0;\">"
            "<tr><td align=\"center\">"

            // ── card ──
            "<table width=\"520\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\""
            "  style=\"background:#ffffff;border-radius:20px;"
            "  box-shadow:0 8px 40px rgba(0,0,0,.12);overflow:hidden;\">"

            // header bar
            "<tr><td style=\"background:linear-gradient(135deg,#14B8A6 0%,#0d9488 100%);"
            "  padding:36px 40px 28px;text-align:center;\">"
            "  <div style=\"display:inline-block;background:rgba(255,255,255,.18);"
            "    border-radius:50%;width:72px;height:72px;line-height:72px;"
            "    font-size:36px;margin-bottom:14px;\">&#x1F512;</div>"
            "  <h1 style=\"margin:0;color:#ffffff;font-size:24px;font-weight:800;"
            "    letter-spacing:0.5px;\">Password Reset</h1>"
            "  <p style=\"margin:6px 0 0;color:rgba(255,255,255,.80);font-size:14px;\">"
            "    Driving School App</p>"
            "</td></tr>"

            // body
            "<tr><td style=\"padding:40px 48px 32px;\">"
            "  <p style=\"margin:0 0 8px;color:#374151;font-size:15px;\">Hello,</p>"
            "  <p style=\"margin:0 0 28px;color:#374151;font-size:15px;line-height:1.6;\">"
            "    We received a request to reset the password for your account.<br/>"
            "    Use the verification code below to continue:"
            "  </p>"

            // code box
            "  <div style=\"background:#F0FDF9;border:2px solid #99F6E4;"
            "    border-radius:14px;padding:28px 16px;text-align:center;"
            "    margin:0 0 28px;\">"
            "    <p style=\"margin:0 0 6px;color:#0f766e;font-size:12px;"
            "      font-weight:700;letter-spacing:2px;text-transform:uppercase;\">"
            "      Verification Code</p>"
            "    <p style=\"margin:0;font-size:46px;font-weight:900;letter-spacing:12px;"
            "      color:#0d9488;font-family:'Courier New',monospace;\">"
        ) + code.toLatin1() + QByteArray(
            "    </p>"
            "  </div>"

            // expiry note
            "  <table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\"><tr>"
            "    <td style=\"background:#FEF3C7;border-left:4px solid #F59E0B;"
            "      border-radius:0 8px 8px 0;padding:12px 16px;\">"
            "      <p style=\"margin:0;color:#92400E;font-size:13px;font-weight:600;\">"
            "        &#x23F0;&nbsp; This code expires in <strong>10 minutes</strong>."
            "      </p>"
            "    </td>"
            "  </tr></table>"

            "  <p style=\"margin:28px 0 0;color:#9CA3AF;font-size:12px;line-height:1.6;\">"
            "    If you didn&#39;t request this, you can safely ignore this email.<br/>"
            "    Your password will not be changed."
            "  </p>"
            "</td></tr>"

            // footer
            "<tr><td style=\"background:#F9FAFB;border-top:1px solid #E5E7EB;"
            "  padding:20px 48px;text-align:center;\">"
            "  <p style=\"margin:0;color:#9CA3AF;font-size:12px;\">"
            "    &copy; 2026 Driving School App &nbsp;&bull;&nbsp; All rights reserved"
            "  </p>"
            "</td></tr>"

            "</table>" // card
            "</td></tr></table>" // outer
            "</body></html>"
        );

    QByteArray body =
        "From: Driving School <" + fromEmail.toLatin1() + ">\r\n"
        "To: " + toEmail.toLatin1() + "\r\n"
        "Subject: Your Password Reset Code\r\n"
        "MIME-Version: 1.0\r\n"
        "Content-Type: multipart/alternative; boundary=\"" + boundary + "\"\r\n"
        "\r\n"
        "--" + boundary + "\r\n"
        "Content-Type: text/plain; charset=UTF-8\r\n"
        "Content-Transfer-Encoding: 8bit\r\n"
        "\r\n"
        + plain +
        "\r\n--" + boundary + "\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Transfer-Encoding: 8bit\r\n"
        "\r\n"
        + html +
        "\r\n--" + boundary + "--\r\n";

    // ── Write payload to a temp file and send via curl.exe ────────
    // curl uses Windows Schannel (built-in TLS) — no OpenSSL needed.
    QTemporaryFile *tmpFile = new QTemporaryFile(this);
    tmpFile->setAutoRemove(true);
    if (!tmpFile->open()) {
        callback(false, "Cannot create temp file for email.");
        return;
    }
    tmpFile->write(body);
    tmpFile->flush();

    QStringList args;
    args << "--url"  << "smtps://smtp.gmail.com:465"
         << "--insecure"                                   // skip cert validation (same machine)
         << "--user" << (fromEmail + ":" + appPassword)
         << "--mail-from" << fromEmail
         << "--mail-rcpt"  << toEmail
         << "--upload-file" << tmpFile->fileName()
         << "--silent";

    QProcess *proc = new QProcess(this);
    proc->setProcessChannelMode(QProcess::MergedChannels);

    // Run asynchronously — callback fires when curl finishes
    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, tmpFile, callback](int exitCode, QProcess::ExitStatus) {
        QString output = QString::fromUtf8(proc->readAll()).trimmed();
        proc->deleteLater();
        tmpFile->close();
        tmpFile->deleteLater();

        if (exitCode == 0) {
            callback(true, "");
        } else {
            // Provide a friendly message; include curl output for diagnosis
            QString err = output.isEmpty()
                ? QString("curl exit code %1").arg(exitCode)
                : output;
            callback(false, err);
        }
    });

    proc->start("C:/Windows/System32/curl.exe", args);
}

// ══════════════════════════════════════════════════════════════
// Background animation  —  depth-layered parallex cars
// ══════════════════════════════════════════════════════════════

/*  Three depth lanes (back→front):
    Lane 0  y≈100-200   size 0.32-0.48   speed 0.25-0.55   opacity 0.05-0.08
    Lane 1  y≈230-360   size 0.55-0.75   speed 0.55-0.95   opacity 0.07-0.11
    Lane 2  y≈390-540   size 0.82-1.10   speed 0.90-1.50   opacity 0.10-0.15  */

static qreal rnd(int lo, int hi)   // integer range → qreal
{ return lo + std::rand() % (hi - lo + 1); }

static qreal rndf(qreal lo, qreal hi)  // float range
{ return lo + (std::rand() % 1000) / 1000.0 * (hi - lo); }

void MainWindow::initCars()
{
    cars.clear();
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    // Lane definitions: {yMin, yMax, sMin, sMax, vMin, vMax, oMin, oMax, count}
    struct LaneDef { int yLo,yHi; qreal sLo,sHi,vLo,vHi,oLo,oHi; int n; };
    const LaneDef lanes[] = {
        { 100, 200, 0.32, 0.48, 0.25, 0.55, 0.05, 0.08, 5 },
        { 230, 360, 0.55, 0.75, 0.55, 0.95, 0.07, 0.11, 5 },
        { 390, 540, 0.82, 1.10, 0.90, 1.50, 0.10, 0.15, 5 },
    };

    for (const auto &l : lanes) {
        for (int i = 0; i < l.n; ++i) {
            AnimCar c;
            c.y            = rnd(l.yLo, l.yHi);
            c.size         = rndf(l.sLo, l.sHi);
            c.speed        = rndf(l.vLo, l.vHi);
            c.targetOpacity= rndf(l.oLo, l.oHi);
            c.opacity      = 0.0;                       // fades in when first seen
            c.facingRight  = (std::rand() % 2 == 0);
            c.colorIndex   = std::rand() % 3;
            c.carType      = std::rand() % 3;
            c.wheelAngle   = rndf(0, 360);
            c.bobPhase     = rndf(0, 6.28318);
            c.x            = rnd(-140, 1300);
            cars.append(c);
        }
    }
}

void MainWindow::updateAnimation()
{
    const qreal W = width();
    for (auto &c : cars) {
        // Move
        if (c.facingRight) {
            c.x += c.speed;
            if (c.x > W + 160) { c.x = -160; c.opacity = 0.0; }
        } else {
            c.x -= c.speed;
            if (c.x < -160) { c.x = W + 160; c.opacity = 0.0; }
        }
        // Fade-in
        if (c.opacity < c.targetOpacity)
            c.opacity = qMin(c.targetOpacity, c.opacity + 0.003);
        // Wheel spin: faster car → faster spin
        c.wheelAngle = std::fmod(c.wheelAngle + c.speed * 4.5, 360.0);
        // Bob
        c.bobPhase = std::fmod(c.bobPhase + 0.018, 6.28318);
    }
    update();
}

// ─── Draw one car ────────────────────────────────────────────
void MainWindow::drawCar(QPainter &p, const AnimCar &car)
{
    const qreal bob = std::sin(car.bobPhase) * 1.8; // gentle float

    p.save();
    p.translate(car.x, car.y + bob);
    p.scale(car.size, car.size);
    if (!car.facingRight) p.scale(-1, 1);

    // ── Color palette ──
    QColor base;
    switch (car.colorIndex) {
        case 0: base = QColor(20,  184, 166); break;  // teal
        case 1: base = QColor(99,  102, 241); break;  // indigo
        default:base = QColor(17,  24,  39);  break;  // near-black
    }
    base.setAlphaF(car.opacity);

    QColor dim   = base; dim.setAlphaF(car.opacity * 0.55);
    QColor glass = base; glass.setAlphaF(car.opacity * 0.38);
    QColor wheel = base; wheel.setAlphaF(car.opacity * 0.90);
    QColor rim   = base; rim.setAlphaF(car.opacity * 0.30);

    // ─── Car geometry per type ────────────────────────────────
    // All shapes: wheels at y=54 for sedan/sports, y=58 for SUV
    struct Geom {
        QRectF body;        // main lower rectangle
        QPolygonF cabin;    // cabin outline
        QPolygonF winFL;    // front window
        QPolygonF winRL;    // rear window
        qreal w1x, w2x;    // wheel X centres
        qreal wy, wr;      // wheel Y centre, radius
    } g;

    if (car.carType == 0) {       // ── SEDAN ──
        g.body  = QRectF(0, 22, 130, 32);
        g.cabin = QPolygonF({QPointF(22,22),QPointF(35,3),QPointF(95,3),QPointF(110,22)});
        g.winFL = QPolygonF({QPointF(36,20),QPointF(42,5),QPointF(64,5),QPointF(64,20)});
        g.winRL = QPolygonF({QPointF(68,20),QPointF(68,5),QPointF(90,5),QPointF(103,20)});
        g.w1x=22; g.w2x=108; g.wy=54; g.wr=13;
    } else if (car.carType == 1) { // ── SUV ──
        g.body  = QRectF(0, 24, 145, 36);
        g.cabin = QPolygonF({QPointF(18,24),QPointF(24,2),QPointF(122,2),QPointF(130,24)});
        g.winFL = QPolygonF({QPointF(28,21),QPointF(32,5),QPointF(68,5),QPointF(68,21)});
        g.winRL = QPolygonF({QPointF(74,21),QPointF(74,5),QPointF(115,5),QPointF(120,21)});
        g.w1x=24; g.w2x=120; g.wy=60; g.wr=15;
    } else {                       // ── SPORTS ──
        g.body  = QRectF(0, 28, 150, 25);
        g.cabin = QPolygonF({QPointF(35,28),QPointF(50,8),QPointF(105,8),QPointF(120,28)});
        g.winFL = QPolygonF({QPointF(52,26),QPointF(58,11),QPointF(78,11),QPointF(78,26)});
        g.winRL = QPolygonF({QPointF(82,26),QPointF(82,11),QPointF(100,11),QPointF(113,26)});
        g.w1x=28; g.w2x=122; g.wy=53; g.wr=12;
    }

    // ── Ground shadow ──
    QRadialGradient shadow(QPointF((g.w1x+g.w2x)/2.0, g.wy+g.wr+2),
                           (g.w2x-g.w1x)*0.5 + g.wr);
    QColor sh1 = QColor(0,0,0); sh1.setAlphaF(car.opacity * 0.40);
    QColor sh2 = QColor(0,0,0); sh2.setAlphaF(0);
    shadow.setColorAt(0, sh1);
    shadow.setColorAt(1, sh2);
    p.setBrush(shadow);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF((g.w1x+g.w2x)/2.0, g.wy+g.wr+4),
                  (g.w2x-g.w1x)*0.5+g.wr, 7);

    // ── Speed-blur lines (fast cars only) ──
    if (car.speed > 0.72) {
        QColor blur = base; blur.setAlphaF(car.opacity * 0.45 * (car.speed - 0.72) / 0.78);
        p.setPen(QPen(blur, 1.2));
        const qreal lineY[] = { g.body.top()+8, g.body.top()+16, g.body.top()+24 };
        const qreal lineLen[] = { 60, 40, 28 };
        for (int i = 0; i < 3; ++i) {
            // lines stream BEHIND the car (left if facing right)
            p.drawLine(QPointF(-lineLen[i]-4, lineY[i]),
                       QPointF(-10, lineY[i]));
        }
    }

    // ── Body ──
    QPainterPath bodyPath;
    bodyPath.addRoundedRect(g.body, 7, 7);
    p.fillPath(bodyPath, base);

    // ── Cabin ──
    QPainterPath cabinPath;
    cabinPath.addPolygon(g.cabin);
    cabinPath.closeSubpath();
    p.fillPath(cabinPath, dim);

    // ── Windows ──
    QPainterPath winPath;
    winPath.addPolygon(g.winFL); winPath.closeSubpath();
    winPath.addPolygon(g.winRL); winPath.closeSubpath();
    p.fillPath(winPath, glass);

    // ── Headlight glow (front = right side when facingRight) ──
    {
        qreal hx = g.body.right() - 4;
        qreal hy = g.body.top()   + g.body.height()*0.45;
        QRadialGradient hg(QPointF(hx, hy), 18);
        QColor light = QColor(255,245,180); light.setAlphaF(car.opacity * 1.1);
        QColor none  = QColor(255,245,180); none.setAlphaF(0);
        hg.setColorAt(0, light); hg.setColorAt(1, none);
        p.setBrush(hg); p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(hx, hy), 18, 10);
    }

    // ── Taillight glow (rear = left side) ──
    {
        qreal tx = g.body.left() + 4;
        qreal ty = g.body.top()  + g.body.height()*0.48;
        QRadialGradient tg(QPointF(tx, ty), 14);
        QColor red  = QColor(220,38,38); red.setAlphaF(car.opacity * 0.9);
        QColor none = QColor(220,38,38); none.setAlphaF(0);
        tg.setColorAt(0, red); tg.setColorAt(1, none);
        p.setBrush(tg); p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(tx, ty), 14, 8);
    }

    // ── Wheels ──
    const qreal wx[] = { g.w1x, g.w2x };
    for (qreal wxi : wx) {
        QPointF wc(wxi, g.wy);

        // tyre
        p.setBrush(wheel); p.setPen(Qt::NoPen);
        p.drawEllipse(wc, g.wr, g.wr);

        // rim ring
        p.setBrush(rim); p.setPen(Qt::NoPen);
        p.drawEllipse(wc, g.wr*0.62, g.wr*0.62);

        // spokes
        p.setPen(QPen(rim, 1.4));
        for (int s = 0; s < 5; ++s) {
            qreal angle = qDegreesToRadians(car.wheelAngle + s * 72.0);
            qreal sx = wc.x() + std::cos(angle) * g.wr * 0.58;
            qreal sy = wc.y() + std::sin(angle) * g.wr * 0.58;
            p.drawLine(wc, QPointF(sx, sy));
        }

        // hub dot
        p.setBrush(base); p.setPen(Qt::NoPen);
        p.drawEllipse(wc, g.wr*0.22, g.wr*0.22);
    }

    p.restore();
}

// ── Paint background + cars ──
void MainWindow::paintEvent(QPaintEvent *event)
{
    QMainWindow::paintEvent(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // Sort back→front so larger (front) cars paint over smaller (back) ones
    QVector<const AnimCar*> sorted;
    sorted.reserve(cars.size());
    for (const auto &c : cars) sorted.append(&c);
    std::sort(sorted.begin(), sorted.end(),
              [](const AnimCar *a, const AnimCar *b){ return a->size < b->size; });

    for (const AnimCar *c : sorted)
        drawCar(p, *c);
}

