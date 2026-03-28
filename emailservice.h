#ifndef EMAILSERVICE_H
#define EMAILSERVICE_H

// ══════════════════════════════════════════════════════════════════
//  EmailJS Service — send emails via EmailJS REST API
//  Free plan: 200 emails/month  |  https://www.emailjs.com
//
//  HOW TO SET UP (one-time):
//  1. Create account at https://emailjs.com
//  2. Add an Email Service (Gmail / Outlook …) → copy SERVICE_ID
//  3. Create an Email Template → copy TEMPLATE_ID
//     Template variables used:
//       {{to_name}}      {{to_email}}
//       {{subject}}      {{message}}
//       {{school_name}}  {{sender_name}}
//  4. Go to Account → API Keys → copy PUBLIC_KEY
//  5. Paste all three values in emailservice.cpp
// ══════════════════════════════════════════════════════════════════

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class EmailService : public QObject
{
    Q_OBJECT

public:
    // ── Singleton ────────────────────────────────────────────────
    static EmailService &instance();

    // ── High-level senders ───────────────────────────────────────

    // 1. Student sends registration request to a school
    void sendRegistrationRequest(const QString &studentName,
                                  const QString &studentEmail,
                                  const QString &schoolName,
                                  const QString &schoolEmail);

    // 2. Instructor gets notified when a new student registers
    void sendNewStudentNotification(const QString &instructorName,
                                    const QString &instructorEmail,
                                    const QString &studentName,
                                    const QString &schoolName);

    // 3. Student receives exam result
    void sendExamResult(const QString &studentName,
                        const QString &studentEmail,
                        const QString &examType,
                        const QString &result,
                        const QString &schoolName);

    // 4. Generic email (any purpose)
    void sendEmail(const QString &toName,
                   const QString &toEmail,
                   const QString &subject,
                   const QString &message,
                   const QString &schoolName = "Wino");

signals:
    void emailSent(const QString &toEmail);
    void emailFailed(const QString &error);

private:
    explicit EmailService(QObject *parent = nullptr);
    EmailService(const EmailService &)            = delete;
    EmailService &operator=(const EmailService &) = delete;

    void postToEmailJS(const QString &toName,
                       const QString &toEmail,
                       const QString &subject,
                       const QString &message,
                       const QString &schoolName);

    QNetworkAccessManager *m_manager;

    // ── EmailJS credentials ──────────────────────────────────────
    // Replace these with your real values from emailjs.com
    static const QString SERVICE_ID;
    static const QString TEMPLATE_ID;
    static const QString PUBLIC_KEY;
};

#endif // EMAILSERVICE_H
