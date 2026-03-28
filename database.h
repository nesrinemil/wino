#ifndef DATABASE_H
#define DATABASE_H

#include <QSqlDatabase>
#include <QString>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

class Database
{
public:
    static Database& instance();
    bool connect();
    QSqlDatabase getDatabase();

private:
    Database();
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    QSqlDatabase db;
};

#endif // DATABASE_H
