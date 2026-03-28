#include "database.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

Database::Database()
{
}

Database::~Database()
{
    if (db.isOpen()) {
        db.close();
    }
}

Database& Database::instance()
{
    static Database instance;
    return instance;
}

bool Database::connect()
{
    // ── Try QOCI first, fallback to QODBC ───────────────
    QStringList drivers = QSqlDatabase::drivers();
    qDebug() << "Available SQL drivers:" << drivers;

    if (drivers.contains("QOCI")) {
        // Direct OCI connection
        db = QSqlDatabase::addDatabase("QOCI");
        db.setHostName("localhost");
        db.setPort(1521);
        db.setDatabaseName("XE");
        db.setUserName("maryem_drive");
        db.setPassword("Maryem2026!");
    } else if (drivers.contains("QODBC")) {
        // ODBC connection (fallback)
        db = QSqlDatabase::addDatabase("QODBC");
        db.setDatabaseName(
            "DRIVER={Oracle in XE};"
            "DBQ=localhost:1521/XE;"
            "UID=maryem_drive;"
            "PWD=Maryem2026!;"
        );
    } else {
        qDebug() << "Error: No Oracle SQL driver found (QOCI or QODBC)";
        qDebug() << "Available drivers:" << drivers;
        return false;
    }

    if (!db.open()) {
        qDebug() << "Error: connection with Oracle database failed";
        qDebug() << db.lastError().text();
        return false;
    }

    qDebug() << "Oracle Database: connection ok";

    // Ensure instructor_id column exists on STUDENTS (ORA-01430 = column already exists → safe to ignore)
    QSqlQuery alterQ;
    alterQ.exec("ALTER TABLE students ADD (instructor_id NUMBER)");

    return true;
}

QSqlDatabase Database::getDatabase()
{
    return db;
}
