#include "connection.h"
#include <QSqlDatabase>
#include <QDebug>

Connection::Connection() {}

bool Connection::createconnect()
{
    // Reuse maryem's already-open QOCI connection (default connection)
    QSqlDatabase db = QSqlDatabase::database();
    if (db.isOpen()) {
        qDebug() << "[WINO] Reusing existing QOCI connection.";
        return true;
    }
    qDebug() << "[WINO] Default DB connection not open.";
    return false;
}
