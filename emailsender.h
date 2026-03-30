#ifndef EMAILSENDER_H
#define EMAILSENDER_H

#include <QString>
#include <QObject>

// ─────────────────────────────────────────────────────────────────────────────
// Mailjet Free API — https://www.mailjet.com  (6,000 emails/month free)
//
// HOW TO GET YOUR FREE API KEYS (2 minutes):
//  1. Go to https://app.mailjet.com/signup
//  2. Create a free account  (no credit card)
//  3. Go to Account → API Keys
//  4. Copy "API Key" → paste in MAILJET_API_KEY  below
//  5. Copy "Secret Key" → paste in MAILJET_API_SECRET below
//  6. Set MAILJET_SENDER_EMAIL to the email you verified on Mailjet
// ─────────────────────────────────────────────────────────────────────────────
#define MAILJET_API_KEY     "bae6a31a2fb9868c08e77fafe31ecbc7"
#define MAILJET_API_SECRET  "45e4a1da489ed8769ea84aa5b9ff340b"
#define MAILJET_SENDER_EMAIL "nesrine.miled@esprim.tn"
#define MAILJET_SENDER_NAME  "WINO Driving School"

class EmailSender
{
public:
    // Sends an HTML email via Mailjet API.
    // Returns true on success; errorOut receives the error message on failure.
    static bool sendHtml(const QString &toEmail,
                         const QString &toName,
                         const QString &subject,
                         const QString &htmlBody,
                         QString *errorOut = nullptr);
};

#endif // EMAILSENDER_H
