#ifndef WINO_BOOTSTRAP_H
#define WINO_BOOTSTRAP_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDebug>

class WinoBootstrap
{
public:
    static bool bootstrap()
    {
        QSqlDatabase db = QSqlDatabase::database();
        if (!db.isOpen()) return false;

        auto tryExec = [&](const QString &sql) {
            QSqlQuery q(db);
            if (!q.exec(sql)) {
                QString err = q.lastError().text();
                if (!err.contains("00955") && !err.contains("already exists") &&
                    !err.contains("01430") && !err.contains("duplicate column"))
                    qDebug() << "[WinoBootstrap]" << err.left(120);
            }
        };

        // ── Add missing columns to instructors table (single source of truth) ──
        tryExec("ALTER TABLE instructors ADD (d17_id VARCHAR2(50) DEFAULT '')");
        tryExec("ALTER TABLE instructors ADD (specialization VARCHAR2(100) DEFAULT 'General')");
        tryExec("ALTER TABLE instructors ADD (instructor_status VARCHAR2(20) DEFAULT 'ACTIVE')");
        tryExec("ALTER TABLE instructors ADD (password_hash VARCHAR2(200) DEFAULT '')");

        // ── Sequences ─────────────────────────────────────────────────────────
        tryExec("CREATE SEQUENCE seq_session_booking_id  START WITH 1 INCREMENT BY 1 NOCACHE");
        tryExec("CREATE SEQUENCE seq_exam_request_id     START WITH 1 INCREMENT BY 1 NOCACHE");
        tryExec("CREATE SEQUENCE seq_payment_id          START WITH 1 INCREMENT BY 1 NOCACHE");
        tryExec("CREATE SEQUENCE seq_availability_id     START WITH 1 INCREMENT BY 1 NOCACHE");
        tryExec("CREATE SEQUENCE seq_wino_progress_id    START WITH 1 INCREMENT BY 1 NOCACHE");

        // ── SESSION_BOOKING ───────────────────────────────────────────────────
        tryExec(
            "CREATE TABLE SESSION_BOOKING ("
            "  session_id     NUMBER PRIMARY KEY,"
            "  student_id     NUMBER NOT NULL,"
            "  instructor_id  NUMBER NOT NULL,"
            "  session_step   NUMBER(1) DEFAULT 1,"
            "  session_date   DATE DEFAULT SYSDATE,"
            "  start_time     VARCHAR2(10),"
            "  end_time       VARCHAR2(10),"
            "  amount         NUMBER(10,2) DEFAULT 0,"
            "  status         VARCHAR2(20) DEFAULT 'CONFIRMED'"
            ")");

        // ── EXAM_REQUEST ──────────────────────────────────────────────────────
        tryExec(
            "CREATE TABLE EXAM_REQUEST ("
            "  request_id       NUMBER PRIMARY KEY,"
            "  student_id       NUMBER NOT NULL,"
            "  instructor_id    NUMBER NOT NULL,"
            "  exam_step        NUMBER(1) DEFAULT 1,"
            "  requested_date   DATE DEFAULT SYSDATE,"
            "  confirmed_date   DATE,"
            "  amount           NUMBER(10,2) DEFAULT 0,"
            "  status           VARCHAR2(20) DEFAULT 'PENDING',"
            "  passed           NUMBER(1) DEFAULT 0,"
            "  rejection_reason VARCHAR2(500),"
            "  comments         VARCHAR2(500),"
            "  exam_location    VARCHAR2(200),"
            "  examiner_name    VARCHAR2(200)"
            ")");

        // ── PAYMENT ───────────────────────────────────────────────────────────
        tryExec(
            "CREATE TABLE PAYMENT ("
            "  payment_id   NUMBER PRIMARY KEY,"
            "  student_id   NUMBER NOT NULL,"
            "  d17_code     VARCHAR2(50),"
            "  facture_date DATE DEFAULT SYSDATE,"
            "  amount       NUMBER(10,2) DEFAULT 0,"
            "  status       VARCHAR2(20) DEFAULT 'PENDING'"
            ")");

        // ── AVAILABILITY ──────────────────────────────────────────────────────
        tryExec(
            "CREATE TABLE AVAILABILITY ("
            "  availability_id NUMBER PRIMARY KEY,"
            "  instructor_id   NUMBER NOT NULL,"
            "  day_of_week     NUMBER(1),"
            "  start_time      VARCHAR2(10),"
            "  end_time        VARCHAR2(10),"
            "  is_active       NUMBER(1) DEFAULT 1"
            ")");

        // ── WINO_PROGRESS (student driving progression) ───────────────────────
        tryExec(
            "CREATE TABLE WINO_PROGRESS ("
            "  user_id         NUMBER PRIMARY KEY,"
            "  current_step    NUMBER(1) DEFAULT 1,"
            "  code_status     VARCHAR2(20) DEFAULT 'IN_PROGRESS',"
            "  circuit_status  VARCHAR2(20) DEFAULT 'LOCKED',"
            "  parking_status  VARCHAR2(20) DEFAULT 'LOCKED',"
            "  code_score      NUMBER(10,2) DEFAULT 0,"
            "  circuit_score   NUMBER(10,2) DEFAULT 0,"
            "  parking_score   NUMBER(10,2) DEFAULT 0,"
            "  total_score     NUMBER(10,2) DEFAULT 0"
            ")");
        // Add score columns if table was created before this version
        tryExec("ALTER TABLE WINO_PROGRESS ADD (circuit_score NUMBER(10,2) DEFAULT 0)");
        tryExec("ALTER TABLE WINO_PROGRESS ADD (parking_score NUMBER(10,2) DEFAULT 0)");

        // ── CARS: add parking-module columns ──────────────────────────────────
        tryExec("ALTER TABLE CARS ADD (IS_PARKING_VEHICLE NUMBER(1,0) DEFAULT 0)");
        tryExec("ALTER TABLE CARS ADD (ASSISTANCE_TYPE VARCHAR2(200))");
        tryExec("ALTER TABLE CARS ADD (SENSOR_COUNT NUMBER(3,0) DEFAULT 5)");
        tryExec("ALTER TABLE CARS ADD (SERIAL_PORT VARCHAR2(20))");
        tryExec("ALTER TABLE CARS ADD (BAUD_RATE NUMBER(10,0) DEFAULT 9600)");
        tryExec("ALTER TABLE CARS ADD (SESSION_FEE NUMBER(10,2) DEFAULT 0)");

        // ── EXAM_REQUEST: migrate VEHICULE_ID → CAR_ID ────────────────────────
        // Drop old FK constraint if it exists (ignore errors — may not exist)
        tryExec("ALTER TABLE EXAM_REQUEST DROP CONSTRAINT fk_er_vehicule");
        // Add CAR_ID column (ignore if it already exists — ORA-01430)
        tryExec("ALTER TABLE EXAM_REQUEST ADD (CAR_ID NUMBER(10,0))");
        // Add new FK to CARS (ignore if already exists)
        tryExec("ALTER TABLE EXAM_REQUEST ADD CONSTRAINT fk_er_car "
                "FOREIGN KEY (CAR_ID) REFERENCES CARS(ID)");

        // ── PARKING_SESSIONS: migrate vehicule_id → CAR_ID ───────────────────
        tryExec("ALTER TABLE PARKING_SESSIONS DROP CONSTRAINT fk_ps_vehicule");
        tryExec("ALTER TABLE PARKING_SESSIONS ADD (CAR_ID NUMBER(10,0))");
        tryExec("ALTER TABLE PARKING_SESSIONS ADD CONSTRAINT fk_ps_car "
                "FOREIGN KEY (CAR_ID) REFERENCES CARS(ID)");

        // ── SEQ_EXAM_SLOT_ID sequence ─────────────────────────────────────────
        tryExec("CREATE SEQUENCE SEQ_EXAM_SLOT_ID MINVALUE 1 START WITH 1 INCREMENT BY 1 NOCACHE");

        // ── PARKING_EXAM_SLOTS table ──────────────────────────────────────────
        tryExec(
            "CREATE TABLE PARKING_EXAM_SLOTS ("
            "  SLOT_ID       NUMBER(10,0) PRIMARY KEY,"
            "  INSTRUCTOR_ID NUMBER(10,0) NOT NULL,"
            "  EXAM_DATE     DATE NOT NULL,"
            "  TIME_SLOT     VARCHAR2(20) NOT NULL,"
            "  STATUS        VARCHAR2(20) DEFAULT 'AVAILABLE',"
            "  BOOKED_BY     NUMBER(10,0),"
            "  CAR_ID        NUMBER(10,0),"
            "  NOTES         VARCHAR2(500),"
            "  CREATED_AT    DATE DEFAULT SYSDATE"
            ")");

        // ── Trigger: auto-assign SLOT_ID from sequence ────────────────────────
        tryExec(
            "CREATE OR REPLACE TRIGGER trg_exam_slot_id "
            "BEFORE INSERT ON PARKING_EXAM_SLOTS "
            "FOR EACH ROW "
            "BEGIN "
            "  IF :NEW.SLOT_ID IS NULL THEN "
            "    SELECT SEQ_EXAM_SLOT_ID.NEXTVAL INTO :NEW.SLOT_ID FROM DUAL; "
            "  END IF; "
            "END;");

        // ── V_PARKING_SESSION_DETAIL view ─────────────────────────────────────
        tryExec(
            "CREATE OR REPLACE FORCE VIEW V_PARKING_SESSION_DETAIL AS "
            "SELECT "
            "  ps.session_id, "
            "  s.first_name || ' ' || s.last_name AS student_name, "
            "  c.model AS vehicle_model, c.plate_number, "
            "  ps.maneuver_type, ps.session_start, "
            "  ps.duration_seconds, "
            "  ROUND(ps.duration_seconds/60.0,1) AS duration_minutes, "
            "  ps.is_exam_mode, ps.is_successful, ps.steps_completed, "
            "  ps.error_count, ps.stall_count, ps.obstacle_contact "
            "FROM PARKING_SESSIONS ps "
            "JOIN STUDENTS s ON ps.student_id = s.id "
            "JOIN CARS     c ON ps.car_id     = c.id");

        // ── V_PARKING_STUDENT_SUMMARY view ────────────────────────────────────
        tryExec(
            "CREATE OR REPLACE FORCE VIEW V_PARKING_STUDENT_SUMMARY AS "
            "SELECT "
            "  s.id AS student_id, "
            "  s.first_name || ' ' || s.last_name AS student_name, "
            "  s.email, s.phone, "
            "  COUNT(ps.session_id) AS total_sessions, "
            "  SUM(CASE WHEN ps.is_successful = 1 THEN 1 ELSE 0 END) AS total_successful, "
            "  CASE WHEN COUNT(ps.session_id) > 0 "
            "       THEN ROUND(100.0 * SUM(CASE WHEN ps.is_successful=1 THEN 1 ELSE 0 END) "
            "                  / COUNT(ps.session_id), 1) "
            "       ELSE 0 END AS success_rate, "
            "  NVL(SUM(ps.duration_seconds),0) AS total_seconds, "
            "  ROUND(NVL(SUM(ps.duration_seconds),0)/3600.0,1) AS total_hours, "
            "  NVL(SUM(c.session_fee),0) AS total_spent, "
            "  CASE WHEN NVL(SUM(ps.duration_seconds),0) >= 28800 THEN 'YES' ELSE 'NO' END "
            "       AS eligible_by_hours, "
            "  MAX(ps.session_start) AS last_session_date "
            "FROM STUDENTS s "
            "LEFT JOIN PARKING_SESSIONS ps ON s.id = ps.student_id "
            "LEFT JOIN CARS c              ON ps.car_id = c.id "
            "GROUP BY s.id, s.first_name, s.last_name, s.email, s.phone");

        // ── Mark all existing active cars as parking vehicles ─────────────────
        // Ensures the parking module can find vehicles already in the CARS table
        tryExec("UPDATE CARS SET IS_PARKING_VEHICLE = 1 WHERE IS_PARKING_VEHICLE IS NULL OR IS_PARKING_VEHICLE = 0");

        // ── Ensure cars_seq sequence exists for parking insertDefaultData() ──
        tryExec("CREATE SEQUENCE cars_seq START WITH 100 INCREMENT BY 1 NOCACHE NOCYCLE");

        db.commit();
        qDebug() << "[WinoBootstrap] Tables ready.";
        return true;
    }
};

#endif // WINO_BOOTSTRAP_H
