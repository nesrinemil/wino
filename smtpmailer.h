#ifndef SMTPMAILER_H
#define SMTPMAILER_H

#include <QObject>
#include <QString>
#include <QSslSocket>
#include <QTextStream>

class SmtpMailer : public QObject
{
    Q_OBJECT
public:
    explicit SmtpMailer(QObject *parent = nullptr);

    // Configure once at startup or pass each time
    void setCredentials(const QString &user, const QString &password);
    void setSmtpServer(const QString &host = "smtp.gmail.com", int port = 465);

    bool sendHtml(const QString &to,
                  const QString &subject,
                  const QString &htmlBody,
                  QString *errorOut = nullptr);

private:
    QString m_user;
    QString m_password;
    QString m_host;
    int     m_port;

    bool sendCommand(QSslSocket &sock, const QString &cmd, int expectedCode, QString *err);
    QString readLine(QSslSocket &sock);
};

#endif // SMTPMAILER_H
