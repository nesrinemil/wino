#include "emailsender.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QEventLoop>
#include <QUrl>
#include <QAuthenticator>

bool EmailSender::sendHtml(const QString &toEmail,
                            const QString &toName,
                            const QString &subject,
                            const QString &htmlBody,
                            QString *errorOut)
{
    // Build the JSON body for Mailjet v3.1 Send API
    QJsonObject from;
    from["Email"] = MAILJET_SENDER_EMAIL;
    from["Name"]  = MAILJET_SENDER_NAME;

    QJsonObject to;
    to["Email"] = toEmail;
    to["Name"]  = toName;

    QJsonObject message;
    message["From"]     = from;
    message["To"]       = QJsonArray{ to };
    message["Subject"]  = subject;
    message["HTMLPart"] = htmlBody;

    QJsonObject body;
    body["Messages"] = QJsonArray{ message };

    QByteArray jsonData = QJsonDocument(body).toJson(QJsonDocument::Compact);

    // Prepare the HTTP request
    QNetworkRequest request(QUrl("https://api.mailjet.com/v3.1/send"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // Force pre-authentication header
    QString credentials = QString("%1:%2").arg(MAILJET_API_KEY, MAILJET_API_SECRET);
    QByteArray authHeader = "Basic " + credentials.toUtf8().toBase64();
    request.setRawHeader("Authorization", authHeader);

    QNetworkAccessManager manager;

    // Handle authentication challenge explicitly if Qt intercepts 401
    QObject::connect(&manager, &QNetworkAccessManager::authenticationRequired,
                     [](QNetworkReply *, QAuthenticator *auth) {
                         auth->setUser(MAILJET_API_KEY);
                         auth->setPassword(MAILJET_API_SECRET);
                     });

    QNetworkReply *reply = manager.post(request, jsonData);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    // Read everything before the reply is deleted
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray responseData = reply->readAll();
    QNetworkReply::NetworkError netError = reply->error();
    QString errorStr = reply->errorString();
    
    reply->deleteLater();

    if (netError != QNetworkReply::NoError) {
        if (errorOut) {
            // Also include the raw response which usually contains Mailjet's error message
            *errorOut = QString("Network Error: %1\nResponse Context: %2").arg(errorStr).arg(QString(responseData));
        }
        return false;
    }

    if (statusCode == 200 || statusCode == 201) {
        return true;
    }

    // Parse error from Mailjet response
    QJsonDocument respDoc = QJsonDocument::fromJson(responseData);
    if (errorOut) {
        if (respDoc.isObject() && respDoc.object().contains("ErrorMessage"))
            *errorOut = "Mailjet API error: " + respDoc.object()["ErrorMessage"].toString();
        else
            *errorOut = "Mailjet HTTP " + QString::number(statusCode) + " error: " + QString(responseData);
    }
    return false;
}
