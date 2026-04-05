#include "smtpmailer.h"
#include <QSslSocket>
#include <QByteArray>
#include <QDateTime>

SmtpMailer::SmtpMailer(QObject *parent)
    : QObject(parent), m_host("smtp.gmail.com"), m_port(465)
{}

void SmtpMailer::setCredentials(const QString &user, const QString &password)
{
    m_user = user;
    m_password = password;
}

void SmtpMailer::setSmtpServer(const QString &host, int port)
{
    m_host = host;
    m_port = port;
}

QString SmtpMailer::readLine(QSslSocket &sock)
{
    while (!sock.canReadLine() && sock.state() != QAbstractSocket::UnconnectedState)
        sock.waitForReadyRead(3000);
    return sock.readLine().trimmed();
}

bool SmtpMailer::sendCommand(QSslSocket &sock, const QString &cmd, int expectedCode, QString *err)
{
    if (!cmd.isEmpty()) {
        sock.write((cmd + "\r\n").toUtf8());
        sock.waitForBytesWritten(3000);
    }
    QString resp = readLine(sock);
    int code = resp.left(3).toInt();
    if (code != expectedCode) {
        if (err) *err = QString("SMTP Error: expected %1 got %2 (%3)").arg(expectedCode).arg(code).arg(resp);
        return false;
    }
    return true;
}

bool SmtpMailer::sendHtml(const QString &to, const QString &subject,
                           const QString &htmlBody, QString *errorOut)
{
    QSslSocket sock;
    sock.connectToHostEncrypted(m_host, m_port);
    if (!sock.waitForEncrypted(10000)) {
        if (errorOut) *errorOut = "SSL connection failed: " + sock.errorString();
        return false;
    }

    // Read greeting
    readLine(sock);

    // EHLO
    sock.write("EHLO localhost\r\n");
    sock.waitForBytesWritten(3000);
    QString line;
    do {
        line = readLine(sock);
    } while (!line.isEmpty() && line.size() > 3 && line[3] == '-');

    if (!line.startsWith("250")) {
        if (errorOut) *errorOut = "EHLO failed: " + line;
        return false;
    }

    // AUTH LOGIN
    sock.write("AUTH LOGIN\r\n");
    sock.waitForBytesWritten(3000);
    QString resp = readLine(sock); // 334 VXNlcm5hbWU6
    if (!resp.startsWith("334")) {
        if (errorOut) *errorOut = "AUTH LOGIN failed: " + resp;
        return false;
    }

    sock.write((m_user.toUtf8().toBase64() + "\r\n"));
    sock.waitForBytesWritten(3000);
    resp = readLine(sock); // 334 UGFzc3dvcmQ6
    if (!resp.startsWith("334")) {
        if (errorOut) *errorOut = "Username rejected: " + resp;
        return false;
    }

    sock.write((m_password.toUtf8().toBase64() + "\r\n"));
    sock.waitForBytesWritten(3000);
    resp = readLine(sock); // 235
    if (!resp.startsWith("235")) {
        if (errorOut) *errorOut = "Authentication failed: " + resp;
        return false;
    }

    // MAIL FROM
    if (!sendCommand(sock, QString("MAIL FROM:<%1>").arg(m_user), 250, errorOut)) return false;
    // RCPT TO
    if (!sendCommand(sock, QString("RCPT TO:<%1>").arg(to), 250, errorOut)) return false;
    // DATA
    if (!sendCommand(sock, "DATA", 354, errorOut)) return false;

    // Build the email
    QString msg;
    msg += QString("From: WINO Driving School <%1>\r\n").arg(m_user);
    msg += QString("To: <%1>\r\n").arg(to);
    msg += QString("Subject: %1\r\n").arg(subject);
    msg += "MIME-Version: 1.0\r\n";
    msg += "Content-Type: text/html; charset=UTF-8\r\n";
    msg += "Content-Transfer-Encoding: 7bit\r\n";
    msg += "\r\n";
    msg += htmlBody;
    msg += "\r\n.\r\n";

    sock.write(msg.toUtf8());
    sock.waitForBytesWritten(10000);
    resp = readLine(sock);
    if (!resp.startsWith("250")) {
        if (errorOut) *errorOut = "Message rejected: " + resp;
        return false;
    }

    sendCommand(sock, "QUIT", 221, nullptr);
    sock.disconnectFromHost();
    return true;
}
