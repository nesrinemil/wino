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
        db.setHostName("127.0.0.1");
        db.setPort(1521);
        db.setDatabaseName("XE");
        db.setUserName("smart_driving");
        db.setPassword("SmartDrive2026!");
    } else if (drivers.contains("QODBC")) {
        // ODBC connection (fallback)
        db = QSqlDatabase::addDatabase("QODBC");
        db.setDatabaseName(
            "DRIVER={Oracle in XE};"
            "DBQ=127.0.0.1:1521/XE;"
            "UID=smart_driving;"
            "PWD=SmartDrive2026!;"
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

    // Ensure legacy compatibility columns exist on STUDENTS
    // ORA-01430 = column already exists → safe to ignore these failures
    {
        QSqlQuery alterQ;
        // Legacy instructor_id (may already exist from previous ALTER)
        alterQ.exec("ALTER TABLE students ADD (instructor_id NUMBER)");
        // Legacy school_id alias (application still uses school_id in some queries)
        alterQ.exec("ALTER TABLE students ADD (school_id NUMBER)");
        // Legacy name column (application stores full name here for display)
        alterQ.exec("ALTER TABLE students ADD (name VARCHAR2(200))");
        // Legacy status column for approval workflow (pending/approved/rejected)
        alterQ.exec("ALTER TABLE students ADD (status VARCHAR2(20) DEFAULT 'pending')");
        // Legacy date columns for requests
        alterQ.exec("ALTER TABLE students ADD (birth_date VARCHAR2(20))");
        alterQ.exec("ALTER TABLE students ADD (requested_date DATE)");
        // Also ensure DRIVING_SCHOOLS has legacy rating/students_count/vehicles_count columns
        alterQ.exec("ALTER TABLE driving_schools ADD (rating NUMBER(3,1) DEFAULT 4)");
        alterQ.exec("ALTER TABLE driving_schools ADD (students_count NUMBER(10) DEFAULT 0)");
        alterQ.exec("ALTER TABLE driving_schools ADD (vehicles_count NUMBER(10) DEFAULT 0)");
        // Ensure CARS has legacy status column (application reads status but new schema uses is_active)
        alterQ.exec("ALTER TABLE cars ADD (status VARCHAR2(20) DEFAULT 'active')");
        // Ensure INSTRUCTORS has legacy school_id and available columns
        alterQ.exec("ALTER TABLE instructors ADD (school_id NUMBER)");
        alterQ.exec("ALTER TABLE instructors ADD (available NUMBER(1) DEFAULT 1)");
        alterQ.exec("ALTER TABLE instructors ADD (role VARCHAR2(20) DEFAULT 'instructor')");
        // Ensure ADMIN_INSTRUCTORS has legacy school_id
        alterQ.exec("ALTER TABLE admin_instructors ADD (school_id NUMBER)");
        // Ensure ADMIN table has at least one row (id=1) for FK constraint on DRIVING_SCHOOLS
        alterQ.exec(
            "INSERT INTO ADMIN (id, full_name, email, password_hash, role, status) "
            "SELECT 1, 'Platform Admin', 'admin@wino.tn', 'default', 'SUPER_ADMIN', 'active' "
            "FROM DUAL WHERE NOT EXISTS (SELECT 1 FROM ADMIN WHERE id = 1)"
        );
    }

    return true;
}

QSqlDatabase Database::getDatabase()
{
    return db;
}
