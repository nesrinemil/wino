#include "emailservice.h"

#include <QJsonObject>
#include <QJsonDocument>
#include <QDebug>
#include <QProcess>
#include <QTemporaryFile>
#include <QDir>

// ══════════════════════════════════════════════════════════════════
//  ★  PASTE YOUR EmailJS CREDENTIALS HERE  ★
//  Get them from https://www.emailjs.com/
// ══════════════════════════════════════════════════════════════════
const QString EmailService::SERVICE_ID  = "service_p36rptr";
const QString EmailService::TEMPLATE_ID = "template_zktkmlw";
const QString EmailService::PUBLIC_KEY  = "LIrNoj7apDLfK2zFG";
// ══════════════════════════════════════════════════════════════════

// ── Singleton ─────────────────────────────────────────────────────
EmailService &EmailService::instance()
{
    static EmailService inst;
    return inst;
}

EmailService::EmailService(QObject *parent)
    : QObject(parent)
{}

// ─────────────────────────────────────────────────────────────────
//  1. Registration Request
// ─────────────────────────────────────────────────────────────────
void EmailService::sendRegistrationRequest(const QString &studentName,
                                            const QString &studentEmail,
                                            const QString &schoolName,
                                            const QString &schoolEmail)
{
    // Email to the STUDENT — confirmation
    QString studentMsg =
        "Hello " + studentName + ",\n\n"
        "Your registration request to " + schoolName + " has been submitted successfully.\n"
        "The school will review your request and contact you soon.\n\n"
        "Good luck with your driving journey! 🚗\n\n"
        "— Wino Driving School Platform";

    postToEmailJS(studentName, studentEmail,
                  "✅ Registration Request Submitted — " + schoolName,
                  studentMsg, schoolName);

    // Email to the SCHOOL — new applicant alert
    QString schoolMsg =
        "Hello " + schoolName + " Team,\n\n"
        "A new student has submitted a registration request:\n\n"
        "  Name:  " + studentName + "\n"
        "  Email: " + studentEmail + "\n\n"
        "Please log in to Wino to review and approve the request.\n\n"
        "— Wino Driving School Platform";

    postToEmailJS(schoolName, schoolEmail,
                  "📥 New Registration Request from " + studentName,
                  schoolMsg, schoolName);
}

// ─────────────────────────────────────────────────────────────────
//  2. New Student Notification → Instructor
// ─────────────────────────────────────────────────────────────────
void EmailService::sendNewStudentNotification(const QString &instructorName,
                                               const QString &instructorEmail,
                                               const QString &studentName,
                                               const QString &schoolName)
{
    QString msg =
        "Hello " + instructorName + ",\n\n"
        "A new student has been assigned to your school " + schoolName + ":\n\n"
        "  Student: " + studentName + "\n\n"
        "Please log in to Wino to view their profile and schedule their first session.\n\n"
        "— Wino Driving School Platform";

    postToEmailJS(instructorName, instructorEmail,
                  "👨‍🎓 New Student Assigned — " + studentName,
                  msg, schoolName);
}

// ─────────────────────────────────────────────────────────────────
//  3. Exam Result → Student
// ─────────────────────────────────────────────────────────────────
void EmailService::sendExamResult(const QString &studentName,
                                   const QString &studentEmail,
                                   const QString &examType,
                                   const QString &result,
                                   const QString &schoolName)
{
    bool passed = result.compare("PASSED", Qt::CaseInsensitive) == 0 ||
                  result.compare("pass",   Qt::CaseInsensitive) == 0;

    QString emoji   = passed ? "🎉" : "📚";
    QString subject = passed
        ? emoji + " Congratulations! You passed the " + examType + " exam"
        : emoji + " " + examType + " Exam Result — Keep Going!";

    QString msg = "Hello " + studentName + ",\n\n";
    if (passed) {
        msg += "Congratulations! 🎊\n"
               "You have successfully PASSED your " + examType + " exam at " + schoolName + ".\n\n"
               "Keep up the great work — you're one step closer to your driving license! 🚗\n\n";
    } else {
        msg += "Your " + examType + " exam result at " + schoolName + " is: " + result + ".\n\n"
               "Don't give up! Review the material and try again — you can do it! 💪\n\n"
               "Contact your instructor for extra support.\n\n";
    }
    msg += "— Wino Driving School Platform";

    postToEmailJS(studentName, studentEmail, subject, msg, schoolName);
}

// ─────────────────────────────────────────────────────────────────
//  4. Generic
// ─────────────────────────────────────────────────────────────────
void EmailService::sendEmail(const QString &toName,
                              const QString &toEmail,
                              const QString &subject,
                              const QString &message,
                              const QString &schoolName)
{
    postToEmailJS(toName, toEmail, subject, message, schoolName);
}

// ─────────────────────────────────────────────────────────────────
//  Core: POST to EmailJS REST API
// ─────────────────────────────────────────────────────────────────
void EmailService::postToEmailJS(const QString &toName,
                                  const QString &toEmail,
                                  const QString &subject,
                                  const QString &message,
                                  const QString &schoolName)
{
    // Build JSON body
    QJsonObject templateParams;
    templateParams["to_name"]     = toName;
    templateParams["to_email"]    = toEmail;
    templateParams["name"]        = toName;       // template uses {{name}}
    templateParams["email"]       = toEmail;      // template uses {{email}}
    templateParams["title"]       = subject;      // template uses {{title}}
    templateParams["subject"]     = subject;
    templateParams["message"]     = message;
    templateParams["school_name"] = schoolName;
    templateParams["sender_name"] = "Wino Platform";

    QJsonObject body;
    body["service_id"]       = SERVICE_ID;
    body["template_id"]      = TEMPLATE_ID;
    body["user_id"]          = PUBLIC_KEY;
    body["template_params"]  = templateParams;

    // Write JSON body to a temp file (same approach as TrafficSignWidget — uses system curl)
    QTemporaryFile *tmpFile = new QTemporaryFile(this);
    tmpFile->setFileTemplate(QDir::tempPath() + "/emailjs_XXXXXX.json");
    if (!tmpFile->open()) {
        emit emailFailed("Failed to create temp file for email request");
        return;
    }
    tmpFile->write(QJsonDocument(body).toJson(QJsonDocument::Compact));
    tmpFile->flush();
    tmpFile->close();

    QProcess *proc = new QProcess(this);
    QStringList args;
    args << "-s"
         << "-X" << "POST"
         << "https://api.emailjs.com/api/v1.0/email/send"
         << "-H" << "Content-Type: application/json"
         << "--data-binary" << ("@" + tmpFile->fileName());

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, tmpFile, toEmail](int exitCode, QProcess::ExitStatus) {
        QByteArray out  = proc->readAllStandardOutput();
        QByteArray errB = proc->readAllStandardError();
        proc->deleteLater();
        tmpFile->deleteLater();

        QString response = QString::fromUtf8(out).trimmed();
        qDebug() << "[EmailJS] Response:" << response << "stderr:" << errB;

        // EmailJS returns "OK" on success, or a JSON error object
        if (exitCode == 0 && (response == "OK" || response.isEmpty())) {
            qDebug() << "[EmailJS] ✅ Email sent to:" << toEmail;
            emit emailSent(toEmail);
        } else if (exitCode != 0) {
            QString err = QString::fromUtf8(errB).trimmed();
            qWarning() << "[EmailJS] ❌ curl error:" << err;
            emit emailFailed("curl error: " + err);
        } else {
            qWarning() << "[EmailJS] ❌ API error:" << response;
            emit emailFailed(response);
        }
    });

    proc->start("curl", args);
}
