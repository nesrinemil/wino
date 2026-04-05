#include "parkingdbmanager.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QCoreApplication>
#include <QSqlRecord>
#include <QMessageBox>

ParkingDBManager& ParkingDBManager::instance()
{
    static ParkingDBManager inst;
    return inst;
}

ParkingDBManager::ParkingDBManager(QObject *parent) : QObject(parent) {}

ParkingDBManager::~ParkingDBManager() { close(); }

// ═══════════════════════════════════════════════════════════════
// CONNEXION ORACLE VIA ODBC  (Partie IV de l'atelier)
// ═══════════════════════════════════════════════════════════════
bool ParkingDBManager::initialize(const QString &dsn, const QString &user, const QString &password)
{
    // Use the existing default connection if already open (shared with the main app)
    QSqlDatabase defaultDb = QSqlDatabase::database();
    if (defaultDb.isOpen()) {
        // Reuse the already-open default connection — no separate connection needed
        m_db = defaultDb;
        qDebug() << "[ParkingDB] Reusing existing default DB connection.";
        createTables();
        insertDefaultData();
        return true;
    }

    // Étape 1 : Utiliser le driver QODBC pour Oracle (named connection to avoid clash)
    if (QSqlDatabase::contains("parking_connection"))
        m_db = QSqlDatabase::database("parking_connection");
    else
        m_db = QSqlDatabase::addDatabase("QODBC", "parking_connection");

    // Étape 2 : Essayer avec connection string DSN (SQLDriverConnect)
    //  Ceci est plus fiable que setUserName/setPassword (SQLConnect)
    QString connStr = QString("DSN=%1;UID=%2;PWD=%3;").arg(dsn, user, password);
    m_db.setDatabaseName(connStr);

    if (!m_db.open()) {
        qDebug() << "[DB] DSN connection string echoue:" << m_db.lastError().text();

        // Essai 2 : driver Oracle in XE (enregistré sur ce système)
        connStr = QString(
            "DRIVER={Oracle in XE};"
            "DBQ=XE;"
            "UID=%1;"
            "PWD=%2;"
        ).arg(user, password);
        m_db.setDatabaseName(connStr);

        if (!m_db.open()) {
            qDebug() << "[DB] Driver Oracle in XE echoue:" << m_db.lastError().text();

            // Essai 3 : chemin driver explicite
            connStr = QString(
                "DRIVER={C:\\oraclexe\\app\\oracle\\product\\11.2.0\\server\\BIN\\SQORA32.DLL};"
                "DBQ=XE;"
                "UID=%1;"
                "PWD=%2;"
            ).arg(user, password);
            m_db.setDatabaseName(connStr);

            if (!m_db.open()) {
                qDebug() << "[DB] Toutes les tentatives echouees:" << m_db.lastError().text();
                emit databaseError("Cannot open database: " + m_db.lastError().text());
                return false;
            }
        }
    }

    qDebug() << "[DB] Connexion Oracle reussie!";

    createTables();
    insertDefaultData();
    return true;
}

bool ParkingDBManager::isConnected() const
{
    return m_db.isOpen();
}

void ParkingDBManager::close()
{
    // Only close if this is our own named connection, not the shared default
    if (m_db.isOpen() && m_db.connectionName() == "parking_connection")
        m_db.close();
}

QString ParkingDBManager::getLastError() const
{
    return m_db.lastError().text();
}

// ═══════════════════════════════════════════════════════════════
// CRÉATION DES TABLES (syntaxe Oracle)
// ═══════════════════════════════════════════════════════════════
void ParkingDBManager::createTables()
{
    QSqlQuery q(m_db);

    // Helper : créer une séquence si elle n'existe pas
    auto createSeqIfNotExists = [&](const QString &seqName) {
        q.prepare("SELECT COUNT(*) FROM user_sequences WHERE sequence_name = ?");
        q.addBindValue(seqName.toUpper());
        if (q.exec() && q.next() && q.value(0).toInt() == 0) {
            q.exec("CREATE SEQUENCE " + seqName + " START WITH 1 INCREMENT BY 1");
        }
    };

    // Helper : créer une table si elle n'existe pas
    auto tableExists = [&](const QString &tableName) -> bool {
        q.prepare("SELECT COUNT(*) FROM user_tables WHERE table_name = ?");
        q.addBindValue(tableName.toUpper());
        return (q.exec() && q.next() && q.value(0).toInt() > 0);
    };

    // ── Tables are created by the external SQL schema (hnnlasrdb.sql) ──
    // ── Just verify core tables exist ──
    if (!tableExists("ADMIN"))               qDebug() << "[DB] WARNING: table ADMIN missing";
    if (!tableExists("DRIVING_SCHOOLS"))     qDebug() << "[DB] WARNING: table DRIVING_SCHOOLS missing";
    if (!tableExists("CARS"))                qDebug() << "[DB] WARNING: table CARS missing";
    if (!tableExists("STUDENTS"))            qDebug() << "[DB] WARNING: table STUDENTS missing";
    if (!tableExists("PARKING_SESSIONS"))    qDebug() << "[DB] WARNING: table PARKING_SESSIONS missing";
    if (!tableExists("PARKING_SENSOR_LOGS")) qDebug() << "[DB] WARNING: table PARKING_SENSOR_LOGS missing";
    if (!tableExists("PARKING_MANEUVER_STEPS")) qDebug() << "[DB] WARNING: table PARKING_MANEUVER_STEPS missing";
    if (!tableExists("PARKING_EXAM_RESULTS"))   qDebug() << "[DB] WARNING: table PARKING_EXAM_RESULTS missing";
    if (!tableExists("PARKING_STUDENT_STATS"))  qDebug() << "[DB] WARNING: table PARKING_STUDENT_STATS missing";
    if (!tableExists("EXAM_REQUEST"))           qDebug() << "[DB] WARNING: table EXAM_REQUEST missing";

    // ── INSTRUCTORS : create table + sequence if missing ──
    if (!tableExists("INSTRUCTORS")) {
        qDebug() << "[DB] Creating INSTRUCTORS table...";
        q.exec("CREATE TABLE INSTRUCTORS ("
               "id NUMBER(10) PRIMARY KEY, "
               "driving_school_id NUMBER(10), "
               "last_name VARCHAR2(60) NOT NULL, "
               "first_name VARCHAR2(60) NOT NULL, "
               "phone VARCHAR2(20), "
               "email VARCHAR2(100), "
               "hire_date DATE DEFAULT SYSDATE, "
               "is_active NUMBER(1) DEFAULT 1 CHECK (is_active IN (0,1)))");
        if (q.lastError().isValid())
            qDebug() << "[DB] INSTRUCTORS create error:" << q.lastError().text();
    }
    createSeqIfNotExists("instructors_seq");
    createSeqIfNotExists("cars_seq");
    createSeqIfNotExists("students_seq");
    createSeqIfNotExists("SEQ_EXAM_REQUEST_ID");

    // ── PARKING_MANEUVER_STEPS : ajouter colonne youtube_url si manquante ──
    {
        q.prepare("SELECT COUNT(*) FROM user_tab_columns "
                  "WHERE table_name='PARKING_MANEUVER_STEPS' AND column_name='YOUTUBE_URL'");
        if (q.exec() && q.next() && q.value(0).toInt() == 0) {
            qDebug() << "[DB] Adding youtube_url column to PARKING_MANEUVER_STEPS...";
            q.exec("ALTER TABLE PARKING_MANEUVER_STEPS ADD youtube_url VARCHAR2(500)");
            if (q.lastError().isValid())
                qDebug() << "[DB] ALTER PARKING_MANEUVER_STEPS error:" << q.lastError().text();
        }
    }

    // ── PARKING_STUDENT_STATS : ajouter colonne total_cost si manquante ──
    {
        q.prepare("SELECT COUNT(*) FROM user_tab_columns "
                  "WHERE table_name='PARKING_STUDENT_STATS' AND column_name='TOTAL_COST'");
        if (q.exec() && q.next() && q.value(0).toInt() == 0) {
            qDebug() << "[DB] Adding total_cost column to PARKING_STUDENT_STATS...";
            q.exec("ALTER TABLE PARKING_STUDENT_STATS ADD total_cost NUMBER(10,2) DEFAULT 0");
            if (q.lastError().isValid())
                qDebug() << "[DB] ALTER PARKING_STUDENT_STATS error:" << q.lastError().text();
        }
    }

    // ── PARKING_EXAM_SLOTS : create if missing ──
    if (!tableExists("PARKING_EXAM_SLOTS")) {
        q.exec("CREATE TABLE PARKING_EXAM_SLOTS ("
               "SLOT_ID       NUMBER(10,0) PRIMARY KEY,"
               "INSTRUCTOR_ID NUMBER(10,0) NOT NULL,"
               "EXAM_DATE     DATE NOT NULL,"
               "TIME_SLOT     VARCHAR2(20) NOT NULL,"
               "STATUS        VARCHAR2(20) DEFAULT 'AVAILABLE',"
               "BOOKED_BY     NUMBER(10,0),"
               "CAR_ID        NUMBER(10,0),"
               "NOTES         VARCHAR2(500),"
               "CREATED_AT    DATE DEFAULT SYSDATE"
               ")");
        if (q.lastError().isValid())
            qDebug() << "[DB] PARKING_EXAM_SLOTS create error:" << q.lastError().text();
        // Sequence may already exist from a previous partial run, ignore error
        q.exec("CREATE SEQUENCE SEQ_EXAM_SLOT_ID MINVALUE 1 START WITH 1 INCREMENT BY 1 NOCACHE");
        qDebug() << "[DB] Created PARKING_EXAM_SLOTS table";
    }

    qDebug() << "[DB] Tables Oracle verifiees/creees";
}

void ParkingDBManager::insertDefaultData()
{
    QSqlQuery q(m_db);

    // Check if maneuver steps already exist (inserted by SQL schema)
    bool hasManeuverSteps = false;
    q.exec("SELECT COUNT(*) FROM PARKING_MANEUVER_STEPS WHERE maneuver_type='creneau'");
    if (q.next() && q.value(0).toInt() > 0) {
        hasManeuverSteps = true;
        qDebug() << "[DB] Maneuver steps already present — skipping step inserts";
    }

    if (!hasManeuverSteps) {
    // ── Maneuver steps (PARKING_MANEUVER_STEPS) ──
    struct Step { QString type; int num; QString nom; QString cond; QString msg; int delai; int stop; };
    QList<Step> steps = {
        {"creneau", 1, "Start",
         "demarrage",
         "The car is starting. Slowly reverse.", 0, 0},
        {"creneau", 2, "Obstacle on right",
         "capteur_droit=obstacle",
         "Turn the wheel slightly to the left.", 0, 0},
        {"creneau", 3, "Obstacle on left",
         "capteur_gauche=obstacle",
         "Turn the wheel slightly to the right.", 0, 0},
        {"creneau", 4, "Capteur gauche - capteur 1",
         "capteur_gauche=capteur1",
         "Turn the wheel fully to the left.", 0, 0},
        {"creneau", 5, "Capteur droit - capteur 2 (Stop)",
         "capteur_droit=capteur2",
         "Stop! Pull over.", 0, 1},
        {"creneau", 6, "After 5s - Steer right",
         "delai_5s_apres_stop",
         "Turn the wheel fully to the right.", 5000, 0},
        {"creneau", 7, "Capteur rear right (Stop)",
         "capteur_arriere_droit=capteur",
         "Stop! Pull over.", 0, 1},
        {"creneau", 8, "After 3s - Steer left",
         "delai_3s_apres_stop",
         "Turn the wheel fully to the left.", 3000, 0},
        {"creneau", 9, "Both rear sensors obstacle (Stop)",
         "capteur_arr_gauche=obstacle AND capteur_arr_droit=obstacle",
         "Stop! Obstacle detected at rear on both sides.", 0, 1},
        {"creneau", 10, "Final phase - Passed!",
         "capteur_arr_gauche=capteur AND capteur_arr_droit=capteur",
         "Stop! Congratulations, you passed your exam!", 0, 1},
    };

    for (const auto &s : steps) {
        q.prepare("INSERT INTO PARKING_MANEUVER_STEPS "
                  "(step_id,maneuver_type,step_number,step_name,sensor_condition,guidance_message,delay_before_ms,is_stop_required) "
                  "VALUES (seq_parking_maneuver_step_id.NEXTVAL,?,?,?,?,?,?,?)");
        q.addBindValue(s.type);  q.addBindValue(s.num);
        q.addBindValue(s.nom);   q.addBindValue(s.cond);
        q.addBindValue(s.msg);   q.addBindValue(s.delai);
        q.addBindValue(s.stop);
        if (!q.exec())
            qDebug() << "[DB] Insert maneuver step error:" << q.lastError().text();
    }
    } // end if (!hasManeuverSteps)

    // ── Ensure cars_seq exists ──
    {
        QSqlQuery sq(m_db);
        sq.prepare("SELECT COUNT(*) FROM user_sequences WHERE sequence_name = 'CARS_SEQ'");
        if (sq.exec() && sq.next() && sq.value(0).toInt() == 0) {
            sq.exec("CREATE SEQUENCE cars_seq START WITH 100 INCREMENT BY 1");
            qDebug() << "[DB] Created cars_seq sequence";
        }
    }

    // ── Default parking vehicles in CARS (need a driving school first) ──
    // Check if we have at least one driving school
    q.exec("SELECT MIN(id) FROM DRIVING_SCHOOLS");
    int schoolId = 1;
    if (q.next() && !q.value(0).isNull()) {
        schoolId = q.value(0).toInt();
    }

    // Check how many parking vehicles exist
    int parkingCount = 0;
    if (q.exec("SELECT COUNT(*) FROM CARS WHERE is_parking_vehicle=1") && q.next()) {
        parkingCount = q.value(0).toInt();
    } else {
        qDebug() << "[DB] CARS query failed:" << q.lastError().text();
        // Try without is_parking_vehicle filter (column may not exist)
        if (q.exec("SELECT COUNT(*) FROM CARS WHERE is_active=1") && q.next())
            parkingCount = q.value(0).toInt();
        else
            qDebug() << "[DB] CARS count fallback also failed:" << q.lastError().text();
    }
    qDebug() << "[DB] Parking vehicles found:" << parkingCount;

    if (parkingCount == 0) {
        qDebug() << "[DB] No parking vehicles — inserting 4 defaults...";

        auto insertCar = [&](const QString &brand, const QString &model, int year,
                             const QString &plate, const QString &assist,
                             const QString &port, double fee) -> bool {
            q.prepare("INSERT INTO CARS (id,driving_school_id,brand,model,year,plate_number,transmission,"
                      "is_active,is_parking_vehicle,assistance_type,sensor_count,serial_port,baud_rate,session_fee) VALUES "
                      "(cars_seq.NEXTVAL,?,?,?,?,?,?,1,1,?,5,?,9600,?)");
            q.addBindValue(schoolId); q.addBindValue(brand); q.addBindValue(model);
            q.addBindValue(year); q.addBindValue(plate); q.addBindValue("Manual");
            q.addBindValue(assist); q.addBindValue(port); q.addBindValue(fee);
            if (!q.exec()) {
                qDebug() << "[DB] Insert car" << plate << "error:" << q.lastError().text();
                return false;
            }
            return true;
        };

        insertCar("Renault", "Clio V", 2024, "215 TU 8842",
                  "Aide parallel parking + Camera recul", "COM3", 15.0);
        insertCar("Dacia", "Sandero Stepway", 2023, "220 TU 1123",
                  "Radar recul + Aide parking", "COM4", 12.0);
        insertCar("Peugeot", "308", 2023, "221 TU 3344",
                  "Park Assist + Camera 360", "COM5", 20.0);
        insertCar("Citroen", "C3", 2024, "219 TU 7788",
                  "Camera recul + Aide parallel parking", "COM6", 13.0);

        // Verify insertion worked
        if (q.exec("SELECT COUNT(*) FROM CARS WHERE is_parking_vehicle=1") && q.next())
            qDebug() << "[DB] After insert, parking vehicles:" << q.value(0).toInt();
    }

    // ── Ensure at least one default instructor exists ──
    int instrCount = 0;
    if (q.exec("SELECT COUNT(*) FROM INSTRUCTORS") && q.next())
        instrCount = q.value(0).toInt();
    if (instrCount == 0) {
        qDebug() << "[DB] No instructors — inserting default moniteur via addMoniteur()...";
        int defId = addMoniteur("Admin", "Moniteur", "+216 00 000 000", "admin@wino.tn");
        if (defId < 0)
            qDebug() << "[DB] Default instructor insert failed (see addMoniteur error above)";
        else
            qDebug() << "[DB] Default instructor inserted with id:" << defId;
    }

    qDebug() << "[DB] Default data checked/inserted";
}

// ═══════════════════════════════════════════════════════════════
// INSTRUCTORS (MONITEURS)
// ═══════════════════════════════════════════════════════════════

// Detect actual column names in INSTRUCTORS table (handles both old/new schemas)
static QStringList s_instrCols;
static bool s_instrColsLoaded = false;

static void detectInstructorColumns(QSqlDatabase &db)
{
    if (s_instrColsLoaded) return;
    s_instrColsLoaded = true;
    QSqlQuery q(db);
    // Oracle: get column names from user_tab_columns
    q.exec("SELECT column_name FROM user_tab_columns WHERE table_name='INSTRUCTORS' ORDER BY column_id");
    while (q.next())
        s_instrCols << q.value(0).toString().toUpper();
    qDebug() << "[DB] INSTRUCTORS columns detected:" << s_instrCols;
}

static bool instrHasCol(const QString &col) { return s_instrCols.contains(col.toUpper()); }

int ParkingDBManager::addMoniteur(const QString &nom, const QString &prenom,
                                  const QString &tel, const QString &email)
{
    if (!isConnected()) return -1;
    QSqlQuery q(m_db);

    int schoolId = 1;
    QSqlQuery sq("SELECT MIN(id) FROM DRIVING_SCHOOLS", m_db);
    if (sq.next() && !sq.value(0).isNull()) schoolId = sq.value(0).toInt();

    // INSTRUCTORS table uses FULL_NAME (single column), PHONE, EMAIL, INSTRUCTOR_STATUS
    QString fullName = (prenom.trimmed() + " " + nom.trimmed()).trimmed();
    QString finalEmail = email.isEmpty()
        ? QString("moniteur_%1@wino.tn").arg(QDateTime::currentMSecsSinceEpoch()) : email;

    q.prepare("INSERT INTO INSTRUCTORS "
              "(driving_school_id, full_name, phone, email, password_hash, instructor_status) "
              "VALUES (?, ?, ?, ?, 'default', 'ACTIVE')");
    q.addBindValue(schoolId);
    q.addBindValue(fullName);
    q.addBindValue(tel);
    q.addBindValue(finalEmail);

    if (q.exec()) {
        QSqlQuery idQ("SELECT instructors_seq.CURRVAL FROM DUAL", m_db);
        if (idQ.next()) return idQ.value(0).toInt();
    }
    qDebug() << "[DB] addMoniteur error:" << q.lastError().text();
    emit databaseError("addMoniteur: " + q.lastError().text());
    return -1;
}

QVariantMap ParkingDBManager::getMoniteur(int id)
{
    QVariantMap map;
    if (!isConnected()) return map;
    QSqlQuery q(m_db);
    // Alias full_name as prenom so UI ("%1 %2").arg(prenom, nom) shows full name correctly
    q.prepare("SELECT id, full_name AS prenom, '' AS nom, "
              "NVL(phone,'') AS telephone, NVL(email,'') AS email "
              "FROM INSTRUCTORS WHERE id=?");
    q.addBindValue(id);
    if (q.exec() && q.next()) {
        for (int i = 0; i < q.record().count(); i++)
            map[q.record().fieldName(i).toLower()] = q.value(i);
    }
    return map;
}

QList<QVariantMap> ParkingDBManager::getAllMoniteurs()
{
    QList<QVariantMap> list;
    if (!isConnected()) return list;
    QSqlQuery q(m_db);
    // Alias full_name as prenom so UI ("%1 %2").arg(prenom, nom) shows full name correctly
    q.exec("SELECT id, full_name AS prenom, '' AS nom, "
           "NVL(phone,'') AS telephone, NVL(email,'') AS email "
           "FROM INSTRUCTORS "
           "WHERE UPPER(NVL(instructor_status,'ACTIVE')) != 'INACTIVE' "
           "ORDER BY full_name");
    while (q.next()) {
        QVariantMap map;
        for (int i = 0; i < q.record().count(); i++)
            map[q.record().fieldName(i).toLower()] = q.value(i);
        list.append(map);
    }
    if (list.isEmpty())
        qDebug() << "[DB] getAllMoniteurs: 0 results, last error:" << q.lastError().text();
    return list;
}

bool ParkingDBManager::updateMoniteur(int id, const QString &nom, const QString &prenom,
                                      const QString &tel, const QString &email)
{
    if (!isConnected()) return false;
    QString fullName = (prenom.trimmed() + " " + nom.trimmed()).trimmed();
    QSqlQuery q(m_db);
    q.prepare("UPDATE INSTRUCTORS SET full_name=?, phone=?, email=? WHERE id=?");
    q.addBindValue(fullName); q.addBindValue(tel);
    q.addBindValue(email); q.addBindValue(id);
    return q.exec();
}

bool ParkingDBManager::deleteMoniteur(int id)
{
    if (!isConnected()) return false;
    QSqlQuery q(m_db);
    // Check if moniteur is referenced in EXAM_REQUEST
    q.prepare("SELECT COUNT(*) FROM EXAM_REQUEST WHERE INSTRUCTOR_ID=?");
    q.addBindValue(id);
    if (q.exec() && q.next() && q.value(0).toInt() > 0) {
        qDebug() << "[DB] Cannot delete moniteur" << id << "- referenced in EXAM_REQUEST";
        return false;
    }
    q.prepare("DELETE FROM INSTRUCTORS WHERE id=?");
    q.addBindValue(id);
    return q.exec();
}

// ═══════════════════════════════════════════════════════════════
// STUDENTS (formerly ELEVES)
// ═══════════════════════════════════════════════════════════════
int ParkingDBManager::addEleve(const QString &nom, const QString &prenom,
                               const QString &tel, const QString &email,
                               const QString &numPermis)
{
    if (!isConnected()) return -1;
    QSqlQuery q(m_db);
    // Need driving_school_id — use first school available
    int schoolId = 1;
    QSqlQuery sq("SELECT MIN(id) FROM DRIVING_SCHOOLS", m_db);
    if (sq.next() && !sq.value(0).isNull()) schoolId = sq.value(0).toInt();

    q.prepare("INSERT INTO STUDENTS (id,driving_school_id,last_name,first_name,date_of_birth,"
              "phone,cin,email,password_hash,license_number) "
              "VALUES (students_seq.NEXTVAL,?,?,?,SYSDATE,?,?,?,?,'default',?)");
    q.addBindValue(schoolId);
    q.addBindValue(nom); q.addBindValue(prenom);
    q.addBindValue(tel);
    q.addBindValue(QString("CIN_%1").arg(QDateTime::currentMSecsSinceEpoch()));
    q.addBindValue(email.isEmpty() ? QString("student_%1@temp.tn").arg(QDateTime::currentMSecsSinceEpoch()) : email);
    q.addBindValue(numPermis);
    if (q.exec()) {
        QSqlQuery idQ("SELECT students_seq.CURRVAL FROM DUAL", m_db);
        if (idQ.next()) {
            int newId = idQ.value(0).toInt();
            // Initialize a blank PARKING_STUDENT_STATS row so every student always has stats
            QSqlQuery sq(m_db);
            sq.prepare("INSERT INTO PARKING_STUDENT_STATS "
                       "(stat_id,student_id,total_sessions,total_successful,total_failed,"
                       "success_rate,last_updated) "
                       "VALUES (seq_parking_student_stat_id.NEXTVAL,?,0,0,0,0,SYSDATE)");
            sq.addBindValue(newId);
            if (!sq.exec())
                qDebug() << "[DB] addEleve: PARKING_STUDENT_STATS init failed:" << sq.lastError().text();
            return newId;
        }
    }
    return -1;
}

QVariantMap ParkingDBManager::getEleve(int id)
{
    QVariantMap map;
    if (!isConnected()) return map;
    QSqlQuery q(m_db);
    q.prepare("SELECT id, last_name AS nom, first_name AS prenom, "
              "phone AS telephone, email, license_number AS numero_permis, "
              "enrollment_date AS date_inscription, student_status "
              "FROM STUDENTS WHERE id=?");
    q.addBindValue(id);
    if (q.exec() && q.next()) {
        for (int i = 0; i < q.record().count(); i++)
            map[q.record().fieldName(i).toLower()] = q.value(i);
    }
    return map;
}

QList<QVariantMap> ParkingDBManager::getAllEleves()
{
    QList<QVariantMap> list;
    if (!isConnected()) return list;
    QSqlQuery q("SELECT id, last_name AS nom, first_name AS prenom, "
                "phone AS telephone, email, license_number AS numero_permis, "
                "enrollment_date AS date_inscription, student_status "
                "FROM STUDENTS WHERE student_status='ACTIVE' ORDER BY last_name", m_db);
    while (q.next()) {
        QVariantMap map;
        for (int i = 0; i < q.record().count(); i++)
            map[q.record().fieldName(i).toLower()] = q.value(i);
        list.append(map);
    }
    return list;
}

bool ParkingDBManager::updateEleve(int id, const QString &nom, const QString &prenom,
                                   const QString &tel, const QString &email,
                                   const QString &numPermis)
{
    if (!isConnected()) return false;
    QSqlQuery q(m_db);
    q.prepare("UPDATE STUDENTS SET last_name=?, first_name=?, phone=?, email=?, license_number=? WHERE id=?");
    q.addBindValue(nom); q.addBindValue(prenom); q.addBindValue(tel);
    q.addBindValue(email); q.addBindValue(numPermis); q.addBindValue(id);
    return q.exec();
}

bool ParkingDBManager::deleteEleve(int id)
{
    if (!isConnected()) return false;
    QSqlQuery q(m_db);
    q.prepare("UPDATE STUDENTS SET student_status='INACTIVE' WHERE id=?");
    q.addBindValue(id);
    return q.exec();
}

// ═══════════════════════════════════════════════════════════════
// CARS (formerly VEHICULES) — parking vehicles have is_parking_vehicle=1
// ═══════════════════════════════════════════════════════════════
int ParkingDBManager::addVehicule(const QString &immat, const QString &modele,
                                  const QString &typeAssist, const QString &portSerie,
                                  int baudRate)
{
    if (!isConnected()) return -1;
    QSqlQuery q(m_db);
    int schoolId = 1;
    QSqlQuery sq("SELECT MIN(id) FROM DRIVING_SCHOOLS", m_db);
    if (sq.next() && !sq.value(0).isNull()) schoolId = sq.value(0).toInt();

    q.prepare("INSERT INTO CARS (id,driving_school_id,brand,model,plate_number,transmission,"
              "is_active,is_parking_vehicle,assistance_type,serial_port,baud_rate) "
              "VALUES (cars_seq.NEXTVAL,?,' ',?,?,'Manual',1,1,?,?,?)");
    q.addBindValue(schoolId);
    q.addBindValue(modele); q.addBindValue(immat);
    q.addBindValue(typeAssist); q.addBindValue(portSerie); q.addBindValue(baudRate);
    if (q.exec()) {
        QSqlQuery idQ("SELECT cars_seq.CURRVAL FROM DUAL", m_db);
        if (idQ.next()) return idQ.value(0).toInt();
    }
    return -1;
}

QVariantMap ParkingDBManager::getVehicule(int id)
{
    QVariantMap map;
    if (!isConnected()) { qDebug() << "[DB] getVehicule: not connected"; return map; }
    QSqlQuery q(m_db);
    // Try with parking filter first, fallback without it
    q.prepare("SELECT id, plate_number AS immatriculation, "
              "NVL(brand,' ') || ' ' || NVL(model,' ') AS modele, "
              "assistance_type AS type_assistance, "
              "sensor_count AS nb_capteurs, "
              "serial_port AS port_serie, baud_rate, "
              "session_fee AS tarif_seance, "
              "is_active AS disponible "
              "FROM CARS WHERE id=?");
    q.addBindValue(id);
    if (!q.exec()) {
        qDebug() << "[DB] getVehicule SQL error:" << q.lastError().text();
        return map;
    }
    if (q.next()) {
        for (int i = 0; i < q.record().count(); i++) {
            QString key = q.record().fieldName(i).toLower();
            map[key] = q.value(i);
        }
        qDebug() << "[DB] getVehicule(" << id << ") found, keys:" << map.keys();
    } else {
        qDebug() << "[DB] getVehicule(" << id << ") no row found in CARS";
    }
    return map;
}

QVariantMap ParkingDBManager::getVehiculeByImmat(const QString &immat)
{
    QVariantMap map;
    if (!isConnected()) return map;
    QSqlQuery q(m_db);
    q.prepare("SELECT id, plate_number AS immatriculation, "
              "NVL(brand,' ') || ' ' || NVL(model,' ') AS modele, "
              "assistance_type AS type_assistance, "
              "sensor_count AS nb_capteurs, "
              "serial_port AS port_serie, baud_rate, "
              "session_fee AS tarif_seance, "
              "is_active AS disponible "
              "FROM CARS WHERE plate_number=? AND is_parking_vehicle=1");
    q.addBindValue(immat);
    if (q.exec() && q.next()) {
        for (int i = 0; i < q.record().count(); i++)
            map[q.record().fieldName(i).toLower()] = q.value(i);
    }
    return map;
}

QList<QVariantMap> ParkingDBManager::getVehiculesDisponibles()
{
    QList<QVariantMap> list;
    if (!isConnected()) {
        qDebug() << "[DB] getVehiculesDisponibles: NOT connected!";
        return list;
    }
    QSqlQuery q(m_db);

    // First try: parking vehicles only
    bool ok = q.exec("SELECT id, plate_number AS immatriculation, "
           "NVL(brand,' ') || ' ' || NVL(model,' ') AS modele, "
           "assistance_type AS type_assistance, "
           "sensor_count AS nb_capteurs, "
           "serial_port AS port_serie, baud_rate, "
           "session_fee AS tarif_seance, "
           "is_active AS disponible "
           "FROM CARS WHERE is_active=1 AND NVL(is_parking_vehicle,0) = 1 ORDER BY model");

    // If query failed OR returned 0 results, fall back to all active cars
    if (!ok || !q.isActive() || !q.isSelect()) {
        qDebug() << "[DB] getVehiculesDisponibles primary query failed:" << q.lastError().text();
        ok = q.exec("SELECT id, plate_number AS immatriculation, "
               "NVL(brand,' ') || ' ' || NVL(model,' ') AS modele, "
               "assistance_type AS type_assistance, "
               "sensor_count AS nb_capteurs, "
               "serial_port AS port_serie, baud_rate, "
               "session_fee AS tarif_seance, "
               "is_active AS disponible "
               "FROM CARS WHERE is_active=1 ORDER BY model");
        if (!ok)
            qDebug() << "[DB] getVehiculesDisponibles fallback also failed:" << q.lastError().text();
    }
    while (q.next()) {
        QVariantMap map;
        for (int i = 0; i < q.record().count(); i++)
            map[q.record().fieldName(i).toLower()] = q.value(i);
        list.append(map);
    }

    // Second fallback: if still empty, show ALL cars (including is_active=0)
    if (list.isEmpty()) {
        qDebug() << "[DB] getVehiculesDisponibles: no active parking cars found, showing all cars";
        if (q.exec("SELECT id, plate_number AS immatriculation, "
                   "NVL(brand,' ') || ' ' || NVL(model,' ') AS modele, "
                   "NVL(assistance_type,'Standard') AS type_assistance, "
                   "NVL(sensor_count,5) AS nb_capteurs, "
                   "NVL(serial_port,'COM3') AS port_serie, "
                   "NVL(baud_rate,9600) AS baud_rate, "
                   "NVL(session_fee,0) AS tarif_seance, "
                   "1 AS disponible "
                   "FROM CARS ORDER BY model")) {
            while (q.next()) {
                QVariantMap map;
                for (int i = 0; i < q.record().count(); i++)
                    map[q.record().fieldName(i).toLower()] = q.value(i);
                list.append(map);
            }
        }
    }

    qDebug() << "[DB] getVehiculesDisponibles: found" << list.size() << "vehicles";
    if (!list.isEmpty()) qDebug() << "[DB]   first vehicle keys:" << list.first().keys()
                                   << "id=" << list.first().value("id");
    return list;
}

void ParkingDBManager::setVehiculeDisponible(int id, bool dispo)
{
    if (!isConnected()) return;
    QSqlQuery q(m_db);
    q.prepare("UPDATE CARS SET is_active=? WHERE id=?");
    q.addBindValue(dispo ? 1 : 0); q.addBindValue(id);
    q.exec();
}

bool ParkingDBManager::updateVehicule(int id, const QString &immat, const QString &modele,
                                      const QString &typeAssist, const QString &portSerie,
                                      int baudRate, double tarif)
{
    if (!isConnected()) return false;
    QSqlQuery q(m_db);
    q.prepare("UPDATE CARS SET plate_number=?, model=?, assistance_type=?, "
              "serial_port=?, baud_rate=?, session_fee=? WHERE id=?");
    q.addBindValue(immat); q.addBindValue(modele); q.addBindValue(typeAssist);
    q.addBindValue(portSerie); q.addBindValue(baudRate); q.addBindValue(tarif);
    q.addBindValue(id);
    return q.exec();
}

bool ParkingDBManager::deleteVehicule(int id)
{
    if (!isConnected()) return false;
    QSqlQuery q(m_db);
    
    // 1. Delete associated sensor logs for all sessions involving this car
    q.prepare("DELETE FROM PARKING_SENSOR_LOGS WHERE session_id IN "
              "(SELECT session_id FROM PARKING_SESSIONS WHERE car_id=?)");
    q.addBindValue(id);
    q.exec();
    
    // 2. Delete all sessions involving this car
    q.prepare("DELETE FROM PARKING_SESSIONS WHERE car_id=?");
    q.addBindValue(id);
    q.exec();
    
    // 3. Nullify car_id in exam requests (since car_id is nullable)
    q.prepare("UPDATE EXAM_REQUEST SET car_id=NULL WHERE car_id=?");
    q.addBindValue(id);
    q.exec();

    // 4. Finally, completely delete the car from the database
    q.prepare("DELETE FROM CARS WHERE id=?");
    q.addBindValue(id);
    return q.exec();
}

// ═══════════════════════════════════════════════════════════════
// PARKING_SESSIONS (formerly SESSIONS)
// ═══════════════════════════════════════════════════════════════
int ParkingDBManager::startSession(int eleveId, int vehiculeId,
                                   const QString &manoeuvreType, bool modeExamen)
{
    if (!isConnected()) return -1;
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO PARKING_SESSIONS (session_id,student_id,car_id,maneuver_type,is_exam_mode) "
              "VALUES (seq_parking_session_id.NEXTVAL,?,?,?,?)");
    q.addBindValue(eleveId); q.addBindValue(vehiculeId);
    q.addBindValue(manoeuvreType); q.addBindValue(modeExamen ? 1 : 0);
    if (q.exec()) {
        QSqlQuery idQ("SELECT seq_parking_session_id.CURRVAL FROM DUAL", m_db);
        if (idQ.next()) return idQ.value(0).toInt();
    }
    return -1;
}

void ParkingDBManager::endSession(int sessionId, bool reussi, int duree, int etapes,
                                  int erreurs, int calages, bool contactObstacle)
{
    if (!isConnected()) return;
    QSqlQuery q(m_db);
    q.prepare("UPDATE PARKING_SESSIONS SET session_end=SYSTIMESTAMP, is_successful=?, duration_seconds=?,"
              "steps_completed=?, error_count=?, stall_count=?, obstacle_contact=? "
              "WHERE session_id=?");
    q.addBindValue(reussi ? 1 : 0); q.addBindValue(duree);
    q.addBindValue(etapes); q.addBindValue(erreurs);
    q.addBindValue(calages); q.addBindValue(contactObstacle ? 1 : 0);
    q.addBindValue(sessionId);
    q.exec();
}

bool ParkingDBManager::deleteSession(int sessionId)
{
    if (!isConnected()) return false;
    QSqlQuery q(m_db);
    // Delete sensor logs first (ON DELETE CASCADE handles this, but be explicit)
    q.prepare("DELETE FROM PARKING_SENSOR_LOGS WHERE session_id=?");
    q.addBindValue(sessionId);
    q.exec();
    // Delete the session
    q.prepare("DELETE FROM PARKING_SESSIONS WHERE session_id=?");
    q.addBindValue(sessionId);
    return q.exec();
}

QVariantMap ParkingDBManager::getSession(int id)
{
    QVariantMap map;
    if (!isConnected()) return map;
    QSqlQuery q(m_db);
    q.prepare("SELECT session_id AS id, student_id AS eleve_id, car_id AS vehicule_id, "
              "maneuver_type AS manoeuvre_type, session_start AS date_debut, "
              "session_end AS date_fin, duration_seconds AS duree_secondes, "
              "is_exam_mode AS mode_examen, is_successful AS reussi, "
              "steps_completed AS nb_etapes_completees, error_count AS nb_erreurs, "
              "stall_count AS nb_calages, obstacle_contact AS contact_obstacle, notes AS commentaire "
              "FROM PARKING_SESSIONS WHERE session_id=?");
    q.addBindValue(id);
    if (q.exec() && q.next()) {
        for (int i = 0; i < q.record().count(); i++)
            map[q.record().fieldName(i).toLower()] = q.value(i);
    }
    return map;
}

QList<QVariantMap> ParkingDBManager::getSessionsEleve(int eleveId, int limit)
{
    QList<QVariantMap> list;
    if (!isConnected()) return list;
    QSqlQuery q(m_db);
    // UNION both new PARKING_SESSIONS (joined to CARS) and legacy SESSIONS
    // Column aliases must match what callers expect: reussi, nb_erreurs, nb_calages, manoeuvre_type, date_debut, duree_secondes
    q.prepare("SELECT * FROM ("
              "SELECT s.session_id AS id, s.student_id AS eleve_id, s.car_id AS vehicule_id, "
              "s.maneuver_type AS manoeuvre_type, CAST(s.session_start AS DATE) AS date_debut, "
              "CAST(s.session_end AS DATE) AS date_fin, s.duration_seconds AS duree_secondes, "
              "s.is_exam_mode AS mode_examen, s.is_successful AS reussi, "
              "s.steps_completed AS nb_etapes_completees, s.error_count AS nb_erreurs, "
              "s.stall_count AS nb_calages, s.obstacle_contact AS contact_obstacle, "
              "NVL(c.brand,' ') || ' ' || NVL(c.model,' ') AS modele, c.plate_number AS immatriculation "
              "FROM PARKING_SESSIONS s JOIN CARS c ON s.car_id=c.id WHERE s.student_id=:id1 "
              "UNION ALL "
              "SELECT s2.ID AS id, s2.ELEVE_ID AS eleve_id, s2.VEHICULE_ID AS vehicule_id, "
              "s2.MANOEUVRE_TYPE AS manoeuvre_type, s2.DATE_DEBUT AS date_debut, "
              "s2.DATE_FIN AS date_fin, s2.DUREE_SECONDES AS duree_secondes, "
              "s2.MODE_EXAMEN AS mode_examen, s2.REUSSI AS reussi, "
              "s2.NB_ETAPES_COMPLETEES AS nb_etapes_completees, s2.NB_ERREURS AS nb_erreurs, "
              "s2.NB_CALAGES AS nb_calages, s2.CONTACT_OBSTACLE AS contact_obstacle, "
              "NVL(v.brand,' ') || ' ' || NVL(v.model,' ') AS modele, NVL(v.plate_number,' ') AS immatriculation "
              "FROM SESSIONS s2 LEFT JOIN CARS v ON s2.VEHICULE_ID=v.ID WHERE s2.ELEVE_ID=:id2 "
              "ORDER BY date_debut DESC"
              ") WHERE ROWNUM <= :lim");
    q.bindValue(":id1", eleveId);
    q.bindValue(":id2", eleveId);
    q.bindValue(":lim", limit);
    if (q.exec()) {
        while (q.next()) {
            QVariantMap map;
            for (int i = 0; i < q.record().count(); i++)
                map[q.record().fieldName(i).toLower()] = q.value(i);
            list.append(map);
        }
    } else {
        qDebug() << "[DB] getSessionsEleve failed:" << q.lastError().text();
    }
    return list;
}


// ═══════════════════════════════════════════════════════════════
// PARKING_SENSOR_LOGS (formerly SENSOR_LOGS)
// ═══════════════════════════════════════════════════════════════
void ParkingDBManager::logSensorEvent(int sessionId, int avant, int gauche, int droit,
                                      int arrGauche, int arrDroit,
                                      double distAvant, double distGauche,
                                      double distDroit, double distArrG, double distArrD,
                                      const QString &evenement, const QString &message,
                                      int etape)
{
    if (!isConnected()) return;
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO PARKING_SENSOR_LOGS "
              "(log_id,session_id,sensor_front,sensor_left,sensor_right,"
              "sensor_rear_left,sensor_rear_right,"
              "distance_front_cm,distance_left_cm,distance_right_cm,"
              "distance_rear_left_cm,distance_rear_right_cm,"
              "event_type,display_message,current_step) "
              "VALUES (seq_parking_sensor_log_id.NEXTVAL,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    q.addBindValue(sessionId);
    q.addBindValue(avant); q.addBindValue(gauche); q.addBindValue(droit);
    q.addBindValue(arrGauche); q.addBindValue(arrDroit);
    q.addBindValue(distAvant); q.addBindValue(distGauche); q.addBindValue(distDroit);
    q.addBindValue(distArrG); q.addBindValue(distArrD);
    q.addBindValue(evenement); q.addBindValue(message);
    q.addBindValue(etape);
    q.exec();
}

// ═══════════════════════════════════════════════════════════════
// PARKING_MANEUVER_STEPS (formerly ETAPES_MANOEUVRE)
// ═══════════════════════════════════════════════════════════════
QList<QVariantMap> ParkingDBManager::getEtapesManoeuvre(const QString &manoeuvreType)
{
    QList<QVariantMap> list;
    if (!isConnected()) return list;
    QSqlQuery q(m_db);
    q.prepare("SELECT step_id AS id, maneuver_type AS manoeuvre_type, "
              "step_number AS numero_etape, step_name AS nom_etape, "
              "sensor_condition AS condition_capteur, guidance_message AS message_guidance, "
              "audio_message AS message_audio, delay_before_ms AS delai_avant_ms, "
              "is_stop_required AS est_stop "
              "FROM PARKING_MANEUVER_STEPS WHERE maneuver_type=? ORDER BY step_number");
    q.addBindValue(manoeuvreType);
    if (q.exec()) {
        while (q.next()) {
            QVariantMap map;
            for (int i = 0; i < q.record().count(); i++)
                map[q.record().fieldName(i).toLower()] = q.value(i);
            list.append(map);
        }
    }
    return list;
}

// ═══════════════════════════════════════════════════════════════
// PARKING_EXAM_RESULTS (formerly RESULTATS_EXAMEN)
// ═══════════════════════════════════════════════════════════════
int ParkingDBManager::saveResultatExamen(int eleveId, const QString &manoeuvreType,
                                         int duree, bool reussi, double score,
                                         int erreurs, const QString &fauteElim,
                                         const QString &commentaire)
{
    if (!isConnected()) return -1;
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO PARKING_EXAM_RESULTS "
              "(result_id,student_id,maneuver_type,duration_seconds,is_successful,score_out_of_20,error_count,"
              "eliminatory_fault,notes) "
              "VALUES (seq_parking_exam_result_id.NEXTVAL,?,?,?,?,?,?,?,?)");
    q.addBindValue(eleveId); q.addBindValue(manoeuvreType);
    q.addBindValue(duree); q.addBindValue(reussi ? 1 : 0);
    q.addBindValue(score); q.addBindValue(erreurs);
    q.addBindValue(fauteElim.isEmpty() ? QVariant() : fauteElim);
    q.addBindValue(commentaire);
    if (q.exec()) {
        QSqlQuery idQ("SELECT seq_parking_exam_result_id.CURRVAL FROM DUAL", m_db);
        if (idQ.next()) return idQ.value(0).toInt();
    }
    return -1;
}

QList<QVariantMap> ParkingDBManager::getResultatsEleve(int eleveId, int limit)
{
    QList<QVariantMap> list;
    if (!isConnected()) return list;
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM ("
              "SELECT result_id AS id, student_id AS eleve_id, exam_date AS date_examen, "
              "maneuver_type AS manoeuvre_type, duration_seconds AS duree_secondes, "
              "is_successful AS reussi, score_out_of_20 AS score_sur_20, "
              "error_count AS nb_erreurs, eliminatory_fault AS faute_eliminatoire, "
              "notes AS commentaire "
              "FROM PARKING_EXAM_RESULTS WHERE student_id=? ORDER BY exam_date DESC"
              ") WHERE ROWNUM <= ?");
    q.addBindValue(eleveId); q.addBindValue(limit);
    if (q.exec()) {
        while (q.next()) {
            QVariantMap map;
            for (int i = 0; i < q.record().count(); i++)
                map[q.record().fieldName(i).toLower()] = q.value(i);
            list.append(map);
        }
    }
    return list;
}

// ═══════════════════════════════════════════════════════════════
// PARKING_STUDENT_STATS (formerly STATISTIQUES_ELEVE)
// ═══════════════════════════════════════════════════════════════
void ParkingDBManager::updateStatistiquesEleve(int eleveId)
{
    if (!isConnected()) return;
    QSqlQuery q(m_db);

    int total = 0, reussies = 0;
    q.prepare("SELECT COUNT(*) FROM PARKING_SESSIONS WHERE student_id=?");
    q.addBindValue(eleveId);
    if (q.exec() && q.next()) total = q.value(0).toInt();

    q.prepare("SELECT COUNT(*) FROM PARKING_SESSIONS WHERE student_id=? AND is_successful=1");
    q.addBindValue(eleveId);
    if (q.exec() && q.next()) reussies = q.value(0).toInt();

    double taux = total > 0 ? (double)reussies / total * 100.0 : 0.0;

    // Oracle : MERGE pour INSERT ou UPDATE
    q.prepare("MERGE INTO PARKING_STUDENT_STATS s "
              "USING (SELECT ? AS student_id FROM DUAL) d ON (s.student_id = d.student_id) "
              "WHEN MATCHED THEN UPDATE SET "
              "  total_sessions=?, total_successful=?, total_failed=?, "
              "  success_rate=?, last_session_date=SYSTIMESTAMP, last_updated=SYSDATE "
              "WHEN NOT MATCHED THEN INSERT "
              "  (stat_id,student_id,total_sessions,total_successful,total_failed,success_rate,last_session_date,last_updated) "
              "  VALUES (seq_parking_student_stat_id.NEXTVAL,?,?,?,?,?,SYSTIMESTAMP,SYSDATE)");
    q.addBindValue(eleveId);
    // WHEN MATCHED
    q.addBindValue(total); q.addBindValue(reussies);
    q.addBindValue(total - reussies); q.addBindValue(taux);
    // WHEN NOT MATCHED
    q.addBindValue(eleveId); q.addBindValue(total);
    q.addBindValue(reussies); q.addBindValue(total - reussies);
    q.addBindValue(taux);
    q.exec();

    // Best times
    auto updateBest = [&](const QString &man, const QString &col) {
        q.prepare(QString("UPDATE PARKING_STUDENT_STATS SET %1="
                          "(SELECT MIN(duration_seconds) FROM PARKING_SESSIONS "
                          "WHERE student_id=? AND maneuver_type=? AND is_successful=1) "
                          "WHERE student_id=?").arg(col));
        q.addBindValue(eleveId); q.addBindValue(man); q.addBindValue(eleveId);
        q.exec();
    };
    updateBest("creneau",       "best_time_creneau");
    updateBest("bataille",      "best_time_bataille");
    updateBest("epi",           "best_time_epi");
    updateBest("marche_arriere","best_time_marche_arr");
    updateBest("demi_tour",     "best_time_demi_tour");

    // Update total_cost = sum of session fees paid by this student
    q.prepare("UPDATE PARKING_STUDENT_STATS SET total_cost = "
              "(SELECT NVL(SUM(NVL(c.session_fee,0)),0) "
              " FROM PARKING_SESSIONS ps "
              " LEFT JOIN CARS c ON ps.car_id = c.id "
              " WHERE ps.student_id = ?) "
              "WHERE student_id = ?");
    q.addBindValue(eleveId); q.addBindValue(eleveId);
    if (!q.exec())
        qDebug() << "[DB] updateStatistiquesEleve total_cost error:" << q.lastError().text();
}

QVariantMap ParkingDBManager::getStatistiquesEleve(int eleveId)
{
    QVariantMap map;
    if (!isConnected()) return map;
    QSqlQuery q(m_db);
    q.prepare("SELECT stat_id AS id, student_id AS eleve_id, "
              "total_sessions, total_successful AS total_reussies, "
              "total_failed AS total_echouees, "
              "best_time_creneau AS meilleur_temps_creneau, "
              "best_time_bataille AS meilleur_temps_bataille, "
              "best_time_epi AS meilleur_temps_epi, "
              "best_time_marche_arr AS meilleur_temps_marche_ar, "
              "success_rate AS taux_reussite, "
              "last_session_date AS derniere_session "
              "FROM PARKING_STUDENT_STATS WHERE student_id=?");
    q.addBindValue(eleveId);
    if (q.exec() && q.next()) {
        for (int i = 0; i < q.record().count(); i++)
            map[q.record().fieldName(i).toLower()] = q.value(i);
    }
    return map;
}

int ParkingDBManager::getSessionCount(int eleveId)
{
    if (!isConnected()) return 0;
    QSqlQuery q(m_db);
    // UNION both new PARKING_SESSIONS and legacy SESSIONS so Smart Coach shows all data
    q.prepare("SELECT COUNT(*) FROM ("
              "SELECT session_id FROM PARKING_SESSIONS WHERE student_id=:id1 "
              "UNION ALL "
              "SELECT ID FROM SESSIONS WHERE ELEVE_ID=:id2"
              ")");
    q.bindValue(":id1", eleveId);
    q.bindValue(":id2", eleveId);
    if (q.exec() && q.next()) return q.value(0).toInt();
    qDebug() << "[DB] getSessionCount failed:" << q.lastError().text();
    return 0;
}

int ParkingDBManager::getSuccessCount(int eleveId)
{
    if (!isConnected()) return 0;
    QSqlQuery q(m_db);
    // UNION both new PARKING_SESSIONS and legacy SESSIONS
    q.prepare("SELECT COUNT(*) FROM ("
              "SELECT session_id FROM PARKING_SESSIONS WHERE student_id=:id1 AND is_successful=1 "
              "UNION ALL "
              "SELECT ID FROM SESSIONS WHERE ELEVE_ID=:id2 AND REUSSI=1"
              ")");
    q.bindValue(":id1", eleveId);
    q.bindValue(":id2", eleveId);
    if (q.exec() && q.next()) return q.value(0).toInt();
    qDebug() << "[DB] getSuccessCount failed:" << q.lastError().text();
    return 0;
}

double ParkingDBManager::getTauxReussite(int eleveId)
{
    int total = getSessionCount(eleveId);
    if (total == 0) return 0.0;
    return (double)getSuccessCount(eleveId) / total * 100.0;
}

int ParkingDBManager::getMeilleurTemps(int eleveId, const QString &manoeuvre)
{
    if (!isConnected()) return -1;
    QSqlQuery q(m_db);
    // UNION both new PARKING_SESSIONS and legacy SESSIONS for best time
    q.prepare("SELECT MIN(t) FROM ("
              "SELECT duration_seconds AS t FROM PARKING_SESSIONS WHERE student_id=:id1 AND maneuver_type=:type1 AND is_successful=1 "
              "UNION ALL "
              "SELECT DUREE_SECONDES AS t FROM SESSIONS WHERE ELEVE_ID=:id2 AND MANOEUVRE_TYPE=:type2 AND REUSSI=1"
              ")");
    q.bindValue(":id1", eleveId); q.bindValue(":type1", manoeuvre);
    q.bindValue(":id2", eleveId); q.bindValue(":type2", manoeuvre);
    if (q.exec() && q.next() && !q.value(0).isNull()) return q.value(0).toInt();
    if (q.lastError().isValid()) qDebug() << "[DB] getMeilleurTemps failed:" << q.lastError().text();
    return -1;
}

// ═══════════════════════════════════════════════════════════════
// TARIFS & FINANCES
// ═══════════════════════════════════════════════════════════════

double ParkingDBManager::getVehiculeTarif(int vehiculeId)
{
    if (!isConnected()) return 15.0;
    QSqlQuery q(m_db);
    q.prepare("SELECT session_fee FROM CARS WHERE id=?");
    q.addBindValue(vehiculeId);
    if (q.exec() && q.next()) return q.value(0).toDouble();
    return 15.0;
}

double ParkingDBManager::getTotalDepense(int eleveId)
{
    if (!isConnected()) return 0.0;
    QSqlQuery q(m_db);
    q.prepare("SELECT NVL(SUM(c.session_fee), 0) FROM PARKING_SESSIONS s "
              "JOIN CARS c ON s.car_id = c.id "
              "WHERE s.student_id=?");
    q.addBindValue(eleveId);
    if (q.exec() && q.next()) return q.value(0).toDouble();
    return 0.0;
}

// ═══════════════════════════════════════════════════════════════
// EXAM ELIGIBILITY
// ═══════════════════════════════════════════════════════════════

int ParkingDBManager::getTotalPracticeSeconds(int eleveId)
{
    if (!isConnected()) return 0;
    QSqlQuery q(m_db);
    // UNION both new PARKING_SESSIONS and legacy SESSIONS for total practice time
    q.prepare("SELECT NVL(SUM(t), 0) FROM ("
              "SELECT NVL(duration_seconds, 0) AS t FROM PARKING_SESSIONS WHERE student_id=:id1 "
              "UNION ALL "
              "SELECT NVL(DUREE_SECONDES, 0) AS t FROM SESSIONS WHERE ELEVE_ID=:id2"
              ")");
    q.bindValue(":id1", eleveId);
    q.bindValue(":id2", eleveId);
    if (q.exec() && q.next()) return q.value(0).toInt();
    qDebug() << "[DB] getTotalPracticeSeconds failed:" << q.lastError().text();
    return 0;
}

int ParkingDBManager::getPerfectRunsCount(int eleveId, const QString &manoeuvreType)
{
    if (!isConnected()) return 0;
    QSqlQuery q(m_db);
    // UNION both new PARKING_SESSIONS and legacy SESSIONS for perfect runs
    q.prepare("SELECT COUNT(*) FROM ("
              "SELECT session_id FROM PARKING_SESSIONS WHERE student_id=:id1 AND maneuver_type=:type1 "
              "AND is_successful=1 AND error_count=0 AND stall_count=0 AND obstacle_contact=0 "
              "UNION ALL "
              "SELECT ID FROM SESSIONS WHERE ELEVE_ID=:id2 AND MANOEUVRE_TYPE=:type2 "
              "AND REUSSI=1 AND NB_ERREURS=0 AND NB_CALAGES=0 AND CONTACT_OBSTACLE=0"
              ")");
    q.bindValue(":id1", eleveId);
    q.bindValue(":type1", manoeuvreType);
    q.bindValue(":id2", eleveId);
    q.bindValue(":type2", manoeuvreType);
    if (q.exec() && q.next()) return q.value(0).toInt();
    qDebug() << "[DB] getPerfectRunsCount failed:" << q.lastError().text();
    return 0;
}

bool ParkingDBManager::isExamReady(int eleveId, const QStringList &manoeuvreTypes)
{
    int totalSec = getTotalPracticeSeconds(eleveId);
    if (totalSec >= 7200) return true;

    for (const auto &mt : manoeuvreTypes) {
        if (getPerfectRunsCount(eleveId, mt) < 3) return false;
    }
    return true;
}

// ═══════════════════════════════════════════════════════════════
// RÉSERVATIONS EXAMEN
// ═══════════════════════════════════════════════════════════════

int ParkingDBManager::addReservationExamen(int eleveId, int instructeurId, int vehiculeId,
                                           const QString &dateExamen,
                                           const QString &creneau,
                                           double montant)
{
    if (!isConnected()) return -1;
    QSqlQuery q(m_db);

    q.prepare("INSERT INTO EXAM_REQUEST "
              "(REQUEST_ID, STUDENT_ID, INSTRUCTOR_ID, EXAM_STEP, "
              "REQUESTED_DATE, AMOUNT, CAR_ID, TIME_SLOT, STATUS) "
              "VALUES (SEQ_EXAM_REQUEST_ID.NEXTVAL,?,?,3,TO_DATE(?,'YYYY-MM-DD'),?,?,?,'PENDING')");
    q.addBindValue(eleveId);
    q.addBindValue(instructeurId);
    q.addBindValue(dateExamen);
    q.addBindValue(montant);
    q.addBindValue(vehiculeId);
    q.addBindValue(creneau);
    if (q.exec()) {
        QSqlQuery idQ("SELECT SEQ_EXAM_REQUEST_ID.CURRVAL FROM DUAL", m_db);
        if (idQ.next()) return idQ.value(0).toInt();
    }
    return -1;
}

QList<QVariantMap> ParkingDBManager::getReservationsEleve(int eleveId)
{
    QList<QVariantMap> list;
    if (!isConnected()) return list;
    QSqlQuery q(m_db);
    q.prepare("SELECT r.REQUEST_ID AS id, r.STUDENT_ID AS eleve_id, "
              "r.CAR_ID AS vehicule_id, "
              "TO_CHAR(r.REQUESTED_DATE,'YYYY-MM-DD') AS date_examen, "
              "r.TIME_SLOT AS creneau, r.AMOUNT AS montant_paye, "
              "r.STATUS AS statut, "
              "NVL(c.brand,' ') || ' ' || NVL(c.model,' ') AS modele, c.plate_number AS immatriculation "
              "FROM EXAM_REQUEST r "
              "LEFT JOIN CARS c ON r.CAR_ID = c.id "
              "WHERE r.STUDENT_ID=? AND r.EXAM_STEP=3 ORDER BY r.REQUESTED_DATE DESC");
    q.addBindValue(eleveId);
    if (q.exec()) {
        while (q.next()) {
            QVariantMap map;
            for (int i = 0; i < q.record().count(); i++)
                map[q.record().fieldName(i).toLower()] = q.value(i);
            list.append(map);
        }
    }
    return list;
}

void ParkingDBManager::updateReservationStatut(int reservationId, const QString &statut)
{
    if (!isConnected()) return;
    QSqlQuery q(m_db);
    
    // Map old French status to new English status
    QString newStatus = statut;
    if (statut == "confirmee") newStatus = "APPROVED";
    else if (statut == "annulee") newStatus = "CANCELLED";
    else if (statut == "en_attente") newStatus = "PENDING";

    // If cancelling, free up the slot in PARKING_EXAM_SLOTS
    if (newStatus == "CANCELLED") {
        QSqlQuery sel(m_db);
        sel.prepare("SELECT STUDENT_ID, INSTRUCTOR_ID, TO_CHAR(REQUESTED_DATE,'YYYY-MM-DD'), TIME_SLOT "
                    "FROM EXAM_REQUEST WHERE REQUEST_ID=?");
        sel.addBindValue(reservationId);
        if (sel.exec() && sel.next()) {
            int sId = sel.value(0).toInt();
            int iId = sel.value(1).toInt();
            QString dateStr = sel.value(2).toString();
            QString timeSlot = sel.value(3).toString();

            QSqlQuery uslot(m_db);
            uslot.prepare("UPDATE PARKING_EXAM_SLOTS "
                          "SET STATUS='AVAILABLE', BOOKED_BY=NULL, CAR_ID=NULL"
                          "WHERE INSTRUCTOR_ID=? AND EXAM_DATE=TO_DATE(?,'YYYY-MM-DD') "
                          "AND TIME_SLOT=? AND BOOKED_BY=?");
            uslot.addBindValue(iId);
            uslot.addBindValue(dateStr);
            uslot.addBindValue(timeSlot);
            uslot.addBindValue(sId);
            uslot.exec();
        }
    }

    q.prepare("UPDATE EXAM_REQUEST SET STATUS=? WHERE REQUEST_ID=?");
    q.addBindValue(newStatus);
    q.addBindValue(reservationId);
    q.exec();
}

bool ParkingDBManager::deleteReservation(int reservationId)
{
    if (!isConnected()) return false;
    
    // First free up the associated PARKING_EXAM_SLOT
    QSqlQuery sel(m_db);
    sel.prepare("SELECT STUDENT_ID, INSTRUCTOR_ID, TO_CHAR(REQUESTED_DATE,'YYYY-MM-DD'), TIME_SLOT "
                "FROM EXAM_REQUEST WHERE REQUEST_ID=?");
    sel.addBindValue(reservationId);
    if (sel.exec() && sel.next()) {
        int sId = sel.value(0).toInt();
        int iId = sel.value(1).toInt();
        QString dateStr = sel.value(2).toString();
        QString timeSlot = sel.value(3).toString();

        QSqlQuery uslot(m_db);
        uslot.prepare("UPDATE PARKING_EXAM_SLOTS "
                      "SET STATUS='AVAILABLE', BOOKED_BY=NULL, CAR_ID=NULL"
                      "WHERE INSTRUCTOR_ID=? AND EXAM_DATE=TO_DATE(?,'YYYY-MM-DD') "
                      "AND TIME_SLOT=? AND BOOKED_BY=?");
        uslot.addBindValue(iId);
        uslot.addBindValue(dateStr);
        uslot.addBindValue(timeSlot);
        uslot.addBindValue(sId);
        uslot.exec();
    }

    // Now delete the entry
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM EXAM_REQUEST WHERE REQUEST_ID=?");
    q.addBindValue(reservationId);
    return q.exec();
}

// ═══════════════════════════════════════════════════════════════
// LOGIN VALIDATION — check user exists in Oracle before allowing entry
// ═══════════════════════════════════════════════════════════════

int ParkingDBManager::validateStudentLogin(const QString &fullName)
{
    if (!isConnected()) return -1;
    QSqlQuery q(m_db);

    // 1) Try exact full name: "Firstname Lastname"
    q.prepare("SELECT id FROM STUDENTS "
              "WHERE (UPPER(TRIM(first_name || ' ' || last_name)) = UPPER(TRIM(?)) "
              "    OR UPPER(TRIM(last_name  || ' ' || first_name)) = UPPER(TRIM(?))) "
              "AND UPPER(NVL(student_status,'ACTIVE')) != 'INACTIVE' "
              "AND ROWNUM = 1");
    q.addBindValue(fullName);
    q.addBindValue(fullName);
    if (q.exec() && q.next()) {
        int id = q.value(0).toInt();
        qDebug() << "[DB] validateStudentLogin: found id=" << id << "for" << fullName;
        return id;
    }

    // 2) Fallback: first token matches first_name (handles "Youssef" when full name in DB is "Youssef Ben Ali")
    QString firstName = fullName.split(' ', Qt::SkipEmptyParts).first();
    q.prepare("SELECT id FROM STUDENTS "
              "WHERE UPPER(TRIM(first_name)) = UPPER(TRIM(?)) "
              "AND UPPER(NVL(student_status,'ACTIVE')) != 'INACTIVE' "
              "AND ROWNUM = 1");
    q.addBindValue(firstName);
    if (q.exec() && q.next()) {
        int id = q.value(0).toInt();
        qDebug() << "[DB] validateStudentLogin (first-name fallback): found id=" << id;
        return id;
    }

    qDebug() << "[DB] validateStudentLogin: NOT FOUND for" << fullName;
    return -1;
}

int ParkingDBManager::validateMoniteurLogin(const QString &fullName)
{
    if (!isConnected()) return -1;
    QSqlQuery q(m_db);

    // INSTRUCTORS table uses FULL_NAME (single column) and INSTRUCTOR_STATUS
    // 1) Exact match on FULL_NAME
    q.prepare("SELECT id FROM INSTRUCTORS "
              "WHERE UPPER(TRIM(full_name)) = UPPER(TRIM(?)) "
              "AND UPPER(NVL(instructor_status,'ACTIVE')) != 'INACTIVE' "
              "AND ROWNUM = 1");
    q.addBindValue(fullName);
    if (q.exec() && q.next()) {
        int id = q.value(0).toInt();
        qDebug() << "[DB] validateMoniteurLogin: found id=" << id << "for" << fullName;
        return id;
    }

    // 2) Fallback: input contained in FULL_NAME (handles partial entry)
    q.prepare("SELECT id FROM INSTRUCTORS "
              "WHERE UPPER(full_name) LIKE UPPER(?) "
              "AND UPPER(NVL(instructor_status,'ACTIVE')) != 'INACTIVE' "
              "AND ROWNUM = 1");
    q.addBindValue("%" + fullName.trimmed() + "%");
    if (q.exec() && q.next()) {
        int id = q.value(0).toInt();
        qDebug() << "[DB] validateMoniteurLogin (partial): found id=" << id;
        return id;
    }

    qDebug() << "[DB] validateMoniteurLogin: NOT FOUND for" << fullName;
    return -1;
}

// ═══════════════════════════════════════════════════════════════
// TUTORIELS VIDÉO — colonne youtube_url dans PARKING_MANEUVER_STEPS
// ═══════════════════════════════════════════════════════════════

bool ParkingDBManager::setManeuverVideoUrl(const QString &maneuverType, const QString &url)
{
    if (!isConnected()) return false;
    QSqlQuery q(m_db);
    // Met à jour le lien sur toutes les étapes de la manoeuvre
    q.prepare("UPDATE PARKING_MANEUVER_STEPS SET youtube_url=? WHERE maneuver_type=?");
    q.addBindValue(url.trimmed().isEmpty() ? QVariant() : url.trimmed());
    q.addBindValue(maneuverType);
    bool ok = q.exec();
    if (!ok) qDebug() << "[DB] setManeuverVideoUrl error:" << q.lastError().text();
    return ok;
}

QList<QVariantMap> ParkingDBManager::getAllManeuverVideoUrls()
{
    QList<QVariantMap> list;
    if (!isConnected()) return list;
    QSqlQuery q(m_db);
    // Une ligne par manoeuvre (step_number minimal = étape 1)
    q.exec("SELECT maneuver_type, MIN(youtube_url) AS youtube_url "
           "FROM PARKING_MANEUVER_STEPS "
           "GROUP BY maneuver_type "
           "ORDER BY MIN(step_number)");
    while (q.next()) {
        QVariantMap m;
        m["maneuver_type"] = q.value(0);
        m["youtube_url"]   = q.value(1);
        list.append(m);
    }
    return list;
}

// ═══════════════════════════════════════════════════════════════
// SESSIONS EXAMEN — vue admin complète sur EXAM_REQUEST
// ═══════════════════════════════════════════════════════════════

QList<QVariantMap> ParkingDBManager::getAllExamRequests(int instructorId)
{
    QList<QVariantMap> list;
    if (!isConnected()) return list;
    QSqlQuery q(m_db);
    QString queryStr = "SELECT r.REQUEST_ID AS id, "
           "NVL(s.last_name,' ') || ' ' || NVL(s.first_name,' ') AS student_name, "
           "NVL(i.last_name,' ') || ' ' || NVL(i.first_name,' ') AS instructor_name, "
           "TO_CHAR(r.REQUESTED_DATE,'YYYY-MM-DD') AS date_examen, "
           "r.TIME_SLOT AS creneau, "
           "NVL(c.brand,' ') || ' ' || NVL(c.plate_number,' ') AS vehicule, "
           "NVL(r.AMOUNT,0) AS montant, "
           "r.STATUS AS statut, "
           "r.STUDENT_ID, r.INSTRUCTOR_ID, r.CAR_ID "
           "FROM EXAM_REQUEST r "
           "LEFT JOIN STUDENTS s ON r.STUDENT_ID = s.id "
           "LEFT JOIN INSTRUCTORS i ON r.INSTRUCTOR_ID = i.id "
           "LEFT JOIN CARS c ON r.CAR_ID = c.id ";

    if (instructorId > 0) {
        queryStr += QString(" WHERE r.INSTRUCTOR_ID = %1 ").arg(instructorId);
    }
    queryStr += " ORDER BY r.REQUESTED_DATE DESC";

    q.exec(queryStr);
    while (q.next()) {
        QVariantMap m;
        for (int i = 0; i < q.record().count(); i++)
            m[q.record().fieldName(i).toLower()] = q.value(i);
        list.append(m);
    }
    return list;
}

int ParkingDBManager::addExamRequestAdmin(int studentId, int instructorId,
                                         const QString &date, const QString &creneau,
                                         int carId, double amount)
{
    if (!isConnected()) return -1;
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO EXAM_REQUEST "
              "(REQUEST_ID,STUDENT_ID,INSTRUCTOR_ID,EXAM_STEP,"
              "REQUESTED_DATE,AMOUNT,CAR_ID,TIME_SLOT,STATUS) "
              "VALUES (SEQ_EXAM_REQUEST_ID.NEXTVAL,?,?,3,TO_DATE(?,'YYYY-MM-DD'),?,?,?,'PENDING')");
    q.addBindValue(studentId);
    q.addBindValue(instructorId);
    q.addBindValue(date);
    q.addBindValue(amount);
    q.addBindValue(carId);
    q.addBindValue(creneau);
    if (!q.exec()) {
        qDebug() << "[DB] addExamRequestAdmin error:" << q.lastError().text();
        return -1;
    }
    QSqlQuery idQ("SELECT SEQ_EXAM_REQUEST_ID.CURRVAL FROM DUAL", m_db);
    return (idQ.next()) ? idQ.value(0).toInt() : -1;
}

// ═══════════════════════════════════════════════════════════════
// MANEUVER STEPS CRUD — PARKING_MANEUVER_STEPS
// ═══════════════════════════════════════════════════════════════

QList<QVariantMap> ParkingDBManager::getAllManeuverSteps()
{
    QList<QVariantMap> list;
    if (!isConnected()) return list;
    QSqlQuery q(m_db);
    q.exec("SELECT step_id, maneuver_type, step_number, step_name, "
           "sensor_condition, guidance_message, audio_message, "
           "delay_before_ms, is_stop_required "
           "FROM PARKING_MANEUVER_STEPS "
           "ORDER BY maneuver_type, step_number");
    while (q.next()) {
        QVariantMap m;
        for (int i = 0; i < q.record().count(); i++)
            m[q.record().fieldName(i).toLower()] = q.value(i);
        list.append(m);
    }
    if (!q.isActive())
        qDebug() << "[DB] getAllManeuverSteps error:" << q.lastError().text();
    return list;
}

int ParkingDBManager::addManeuverStep(const QString &maneuverType, int stepNumber,
                                     const QString &stepName, const QString &sensorCondition,
                                     const QString &guidanceMessage, const QString &audioMessage,
                                     int delayMs, bool isStopRequired)
{
    if (!isConnected()) return -1;
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO PARKING_MANEUVER_STEPS "
              "(step_id, maneuver_type, step_number, step_name, "
              "sensor_condition, guidance_message, audio_message, "
              "delay_before_ms, is_stop_required) "
              "VALUES (SEQ_PARKING_MANEUVER_STEP_ID.NEXTVAL,?,?,?,?,?,?,?,?)");
    q.addBindValue(maneuverType);
    q.addBindValue(stepNumber);
    q.addBindValue(stepName.trimmed().isEmpty() ? QVariant() : stepName.trimmed());
    q.addBindValue(sensorCondition.trimmed().isEmpty() ? QVariant() : sensorCondition.trimmed());
    q.addBindValue(guidanceMessage.trimmed().isEmpty() ? QVariant() : guidanceMessage.trimmed());
    q.addBindValue(audioMessage.trimmed().isEmpty() ? QVariant() : audioMessage.trimmed());
    q.addBindValue(delayMs);
    q.addBindValue(isStopRequired ? 1 : 0);
    if (!q.exec()) {
        qDebug() << "[DB] addManeuverStep error:" << q.lastError().text();
        return -1;
    }
    QSqlQuery idQ("SELECT SEQ_PARKING_MANEUVER_STEP_ID.CURRVAL FROM DUAL", m_db);
    return (idQ.next()) ? idQ.value(0).toInt() : -1;
}

bool ParkingDBManager::updateManeuverStep(int stepId, int stepNumber,
                                         const QString &stepName, const QString &sensorCondition,
                                         const QString &guidanceMessage, const QString &audioMessage,
                                         int delayMs, bool isStopRequired)
{
    if (!isConnected()) return false;
    QSqlQuery q(m_db);
    q.prepare("UPDATE PARKING_MANEUVER_STEPS SET "
              "step_number=?, step_name=?, sensor_condition=?, "
              "guidance_message=?, audio_message=?, "
              "delay_before_ms=?, is_stop_required=? "
              "WHERE step_id=?");
    q.addBindValue(stepNumber);
    q.addBindValue(stepName.trimmed().isEmpty() ? QVariant() : stepName.trimmed());
    q.addBindValue(sensorCondition.trimmed().isEmpty() ? QVariant() : sensorCondition.trimmed());
    q.addBindValue(guidanceMessage.trimmed().isEmpty() ? QVariant() : guidanceMessage.trimmed());
    q.addBindValue(audioMessage.trimmed().isEmpty() ? QVariant() : audioMessage.trimmed());
    q.addBindValue(delayMs);
    q.addBindValue(isStopRequired ? 1 : 0);
    q.addBindValue(stepId);
    bool ok = q.exec();
    if (!ok) qDebug() << "[DB] updateManeuverStep error:" << q.lastError().text();
    return ok;
}

bool ParkingDBManager::deleteManeuverStep(int stepId)
{
    if (!isConnected()) return false;
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM PARKING_MANEUVER_STEPS WHERE step_id=?");
    q.addBindValue(stepId);
    bool ok = q.exec();
    if (!ok) qDebug() << "[DB] deleteManeuverStep error:" << q.lastError().text();
    return ok;
}

// ═══════════════════════════════════════════════════════════════
// EXAM RESULTS ADMIN VIEW — PARKING_EXAM_RESULTS
// ═══════════════════════════════════════════════════════════════

QList<QVariantMap> ParkingDBManager::getAllExamResults()
{
    QList<QVariantMap> list;
    if (!isConnected()) return list;
    QSqlQuery q(m_db);
    q.exec("SELECT r.result_id AS id, "
           "NVL(s.last_name,' ') || ' ' || NVL(s.first_name,' ') AS student_name, "
           "r.maneuver_type, "
           "TO_CHAR(r.exam_date,'YYYY-MM-DD HH24:MI') AS exam_date, "
           "r.duration_seconds, r.is_successful, "
           "r.score_out_of_20, r.error_count, "
           "NVL(r.eliminatory_fault,'-') AS eliminatory_fault "
           "FROM PARKING_EXAM_RESULTS r "
           "LEFT JOIN STUDENTS s ON r.student_id = s.id "
           "ORDER BY r.exam_date DESC");
    while (q.next()) {
        QVariantMap m;
        for (int i = 0; i < q.record().count(); i++)
            m[q.record().fieldName(i).toLower()] = q.value(i);
        list.append(m);
    }
    if (!q.isActive())
        qDebug() << "[DB] getAllExamResults error:" << q.lastError().text();
    return list;
}

bool ParkingDBManager::deleteExamResult(int resultId)
{
    if (!isConnected()) return false;
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM PARKING_EXAM_RESULTS WHERE result_id=?");
    q.addBindValue(resultId);
    bool ok = q.exec();
    if (!ok) qDebug() << "[DB] deleteExamResult error:" << q.lastError().text();
    return ok;
}

// ═══════════════════════════════════════════════════════════════
// PARKING_EXAM_SLOTS — instructor-created exam slots
// ═══════════════════════════════════════════════════════════════
int ParkingDBManager::addExamSlot(int instructorId, const QString &examDate,
                                  const QString &timeSlot, const QString &notes)
{
    if (!isConnected()) return -1;
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO PARKING_EXAM_SLOTS "
              "(SLOT_ID,INSTRUCTOR_ID,EXAM_DATE,TIME_SLOT,STATUS,NOTES) "
              "VALUES (SEQ_EXAM_SLOT_ID.NEXTVAL,?,TO_DATE(?,'YYYY-MM-DD'),?,'AVAILABLE',?)");
    q.addBindValue(instructorId);
    q.addBindValue(examDate);
    q.addBindValue(timeSlot);
    q.addBindValue(notes.isEmpty() ? QVariant() : notes);
    if (!q.exec()) {
        qDebug() << "[DB] addExamSlot error:" << q.lastError().text();
        return -1;
    }
    QSqlQuery id("SELECT SEQ_EXAM_SLOT_ID.CURRVAL FROM DUAL", m_db);
    return id.next() ? id.value(0).toInt() : -1;
}

QList<QVariantMap> ParkingDBManager::getAvailableExamSlots()
{
    QList<QVariantMap> list;
    if (!isConnected()) return list;
    QSqlQuery q(m_db);
    q.exec("SELECT s.SLOT_ID, s.INSTRUCTOR_ID, "
           "TO_CHAR(s.EXAM_DATE,'YYYY-MM-DD') AS exam_date, "
           "TO_CHAR(s.EXAM_DATE,'DD/MM/YYYY') AS exam_date_display, "
           "s.TIME_SLOT, s.NOTES, "
           "NVL(i.last_name||' '||i.first_name, 'Instructor') AS instructor_name "
           "FROM PARKING_EXAM_SLOTS s "
           "LEFT JOIN INSTRUCTORS i ON s.INSTRUCTOR_ID=i.id "
           "WHERE s.STATUS='AVAILABLE' AND s.BOOKED_BY IS NULL "
           "AND s.EXAM_DATE >= TRUNC(SYSDATE) "
           "ORDER BY s.EXAM_DATE, s.TIME_SLOT");
    while (q.next()) {
        QVariantMap m;
        for (int i = 0; i < q.record().count(); i++)
            m[q.record().fieldName(i).toLower()] = q.value(i);
        list.append(m);
    }
    return list;
}

QList<QVariantMap> ParkingDBManager::getExamSlotsForInstructor(int instructorId)
{
    QList<QVariantMap> list;
    if (!isConnected()) return list;
    QSqlQuery q(m_db);
    q.prepare("SELECT s.SLOT_ID, "
              "TO_CHAR(s.EXAM_DATE,'YYYY-MM-DD') AS exam_date, "
              "TO_CHAR(s.EXAM_DATE,'DD/MM/YYYY') AS exam_date_display, "
              "s.TIME_SLOT, s.STATUS, s.NOTES, "
              "NVL(st.last_name||' '||st.first_name,'-') AS student_name "
              "FROM PARKING_EXAM_SLOTS s "
              "LEFT JOIN STUDENTS st ON s.BOOKED_BY=st.id "
              "WHERE s.INSTRUCTOR_ID=? "
              "ORDER BY s.EXAM_DATE DESC, s.TIME_SLOT");
    q.addBindValue(instructorId);
    if (q.exec())
        while (q.next()) {
            QVariantMap m;
            for (int i = 0; i < q.record().count(); i++)
                m[q.record().fieldName(i).toLower()] = q.value(i);
            list.append(m);
        }
    return list;
}

bool ParkingDBManager::bookExamSlot(int slotId, int studentId, int vehiculeId)
{
    if (!isConnected()) return false;
    QSqlQuery q(m_db);
    q.prepare("UPDATE PARKING_EXAM_SLOTS SET STATUS='BOOKED', BOOKED_BY=?, CAR_ID=? "
              "WHERE SLOT_ID=? AND STATUS='AVAILABLE' AND BOOKED_BY IS NULL");
    q.addBindValue(studentId);
    q.addBindValue(vehiculeId > 0 ? vehiculeId : QVariant());
    q.addBindValue(slotId);
    if (!q.exec() || q.numRowsAffected() == 0) return false;

    // Also create an EXAM_REQUEST row for this booking
    QSqlQuery slotQ(m_db);
    slotQ.prepare("SELECT INSTRUCTOR_ID, TO_CHAR(EXAM_DATE,'YYYY-MM-DD'), TIME_SLOT "
                  "FROM PARKING_EXAM_SLOTS WHERE SLOT_ID=?");
    slotQ.addBindValue(slotId);
    if (slotQ.exec() && slotQ.next()) {
        int instrId  = slotQ.value(0).toInt();
        QString date = slotQ.value(1).toString();
        QString slot = slotQ.value(2).toString();
        QSqlQuery ins(m_db);
        ins.prepare("INSERT INTO EXAM_REQUEST "
                    "(REQUEST_ID,STUDENT_ID,INSTRUCTOR_ID,EXAM_STEP,"
                    "REQUESTED_DATE,AMOUNT,CAR_ID,TIME_SLOT,STATUS) "
                    "VALUES (SEQ_EXAM_REQUEST_ID.NEXTVAL,?,?,3,TO_DATE(?,'YYYY-MM-DD'),50,?,?,'PENDING')");
        ins.addBindValue(studentId);
        ins.addBindValue(instrId);
        ins.addBindValue(date);
        ins.addBindValue(vehiculeId > 0 ? vehiculeId : QVariant());
        ins.addBindValue(slot);
        ins.exec();
    }
    return true;
}

bool ParkingDBManager::cancelExamSlot(int slotId)
{
    if (!isConnected()) return false;
    
    // Find if a student booked this slot
    QSqlQuery sel(m_db);
    sel.prepare("SELECT BOOKED_BY, INSTRUCTOR_ID, TO_CHAR(EXAM_DATE,'YYYY-MM-DD'), TIME_SLOT "
                "FROM PARKING_EXAM_SLOTS WHERE SLOT_ID=?");
    sel.addBindValue(slotId);
    if (sel.exec() && sel.next()) {
        int sId = sel.value(0).toInt();
        int iId = sel.value(1).toInt();
        QString dateStr = sel.value(2).toString();
        QString timeSlot = sel.value(3).toString();

        if (sId > 0) {
            // Cancel the student's exam request
            QSqlQuery updReq(m_db);
            updReq.prepare("UPDATE EXAM_REQUEST SET STATUS='CANCELLED' "
                           "WHERE STUDENT_ID=? AND INSTRUCTOR_ID=? "
                           "AND REQUESTED_DATE=TO_DATE(?,'YYYY-MM-DD') AND TIME_SLOT=? AND STATUS='PENDING'");
            updReq.addBindValue(sId);
            updReq.addBindValue(iId);
            updReq.addBindValue(dateStr);
            updReq.addBindValue(timeSlot);
            updReq.exec();
        }
    }

    QSqlQuery q(m_db);
    q.prepare("UPDATE PARKING_EXAM_SLOTS SET STATUS='CANCELLED' WHERE SLOT_ID=?");
    q.addBindValue(slotId);
    return q.exec() && q.numRowsAffected() > 0;
}
