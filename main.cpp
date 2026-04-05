#include "mainwindow.h"
#include "database.h"
#include <QApplication>
#include <QMessageBox>
#include <QStyle>
#include <QStyleFactory>
#include <QSslConfiguration>
#include <QSslSocket>

int main(int argc, char *argv[])
{
    // Force Windows Schannel TLS backend before anything else.
    // This must be called before QApplication so the backend is selected
    // before Qt's network stack initializes.
    qputenv("QT_TLS_BACKEND", "schannel");

    QApplication a(argc, argv);

    // Select Schannel backend explicitly via Qt 6 API
    if (QSslSocket::availableBackends().contains("schannel")) {
        QSslSocket::setActiveBackend("schannel");
    }

    // Disable peer certificate verification (dev environment)
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    QSslConfiguration::setDefaultConfiguration(sslConfig);

    // Force Fusion style for consistent appearance
    a.setStyle(QStyleFactory::create("Fusion"));

    // Connect to Oracle (maryem_drive schema)
    if (!Database::instance().connect()) {
        QMessageBox::critical(nullptr, "Database Error",
            "Failed to connect to Oracle database.\n"
            "Make sure Oracle XE is running.\n\n"
            "User: maryem_drive\nDB: localhost:1521/XE");
        return -1;
    }

    // Start with Login window
    MainWindow w;
    w.showMaximized();
    return a.exec();
}
