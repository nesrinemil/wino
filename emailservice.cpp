#include "emailservice.h"

#include <QNetworkRequest>
#include <QJsonObject>
#include <QJsonDocument>
#include <QUrl>
#include <QDebug>

// ══════════════════════════════════════════════════════════════════
//  ★  PASTE YOUR EmailJS CREDENTIALS HERE  ★
//  Get them from https://www.emailjs.com/
// ══════════════════════════════════════════════════════════════════
const QString EmailService::SERVICE_ID  = "service_p36rptr";
const QString EmailService::TEMPLATE_ID = "template_xdysak3";
const QString EmailService::PUBLIC_KEY  = "LIrNoj7apDLfK2zFG";
// ══════════════════════════════════════════════════════════════════

// ── Singleton ─────────────────────────────────────────────────────
EmailService &EmailService::instance()
{
    static EmailService inst;
    return inst;
}

EmailService::EmailService(QObject *parent)
    : QObject(parent),
      m_manager(new QNetworkAccessManager(this))
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
    templateParams["subject"]     = subject;
    templateParams["message"]     = message;
    templateParams["school_name"] = schoolName;
    templateParams["sender_name"] = "Wino Platform";

    QJsonObject body;
    body["service_id"]       = SERVICE_ID;
    body["template_id"]      = TEMPLATE_ID;
    body["user_id"]          = PUBLIC_KEY;
    body["template_params"]  = templateParams;

    QNetworkRequest request(QUrl("https://api.emailjs.com/api/v1.0/email/send"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = m_manager->post(
        request, QJsonDocument(body).toJson(QJsonDocument::Compact));

    // Handle response async
    connect(reply, &QNetworkReply::finished, this, [this, reply, toEmail]() {
        if (reply->error() == QNetworkReply::NoError) {
            qDebug() << "[EmailJS] ✅ Email sent to:" << toEmail;
            emit emailSent(toEmail);
        } else {
            QString err = reply->errorString();
            QString resp = QString::fromUtf8(reply->readAll());
            qDebug() << "[EmailJS] ❌ Failed to send email to:" << toEmail
                     << "Error:" << err << resp;
            emit emailFailed(err);
        }
        reply->deleteLater();
    });
}
