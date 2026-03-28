#include "mainwindow.h"
#include "database.h"
#include <QApplication>
#include <QMessageBox>
#include <QStyle>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

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
