-- ============================================================
-- Run as smart_driving user - Tables, Sequences & Data
-- Oracle 11g compatible
-- ============================================================

-- ============================================================
-- SEQUENCES (replace IDENTITY for Oracle 11g)
-- ============================================================
CREATE SEQUENCE seq_driving_schools START WITH 1 INCREMENT BY 1 NOCACHE NOCYCLE;
CREATE SEQUENCE seq_students        START WITH 1 INCREMENT BY 1 NOCACHE NOCYCLE;
CREATE SEQUENCE seq_vehicles        START WITH 1 INCREMENT BY 1 NOCACHE NOCYCLE;
CREATE SEQUENCE seq_instructors     START WITH 1 INCREMENT BY 1 NOCACHE NOCYCLE;

-- ============================================================
-- TABLE: driving_schools
-- ============================================================
CREATE TABLE driving_schools (
    id              NUMBER          PRIMARY KEY,
    name            VARCHAR2(200)   NOT NULL,
    students_count  NUMBER          DEFAULT 0,
    vehicles_count  NUMBER          DEFAULT 0,
    status          VARCHAR2(20)    DEFAULT 'pending'
                        CHECK (status IN ('pending','active','suspended')),
    rating          NUMBER(3,1)     DEFAULT 0,
    created_at      DATE            DEFAULT SYSDATE
);

CREATE OR REPLACE TRIGGER trg_driving_schools_id
BEFORE INSERT ON driving_schools FOR EACH ROW
BEGIN
    IF :NEW.id IS NULL THEN
        SELECT seq_driving_schools.NEXTVAL INTO :NEW.id FROM dual;
    END IF;
END;
/

-- ============================================================
-- TABLE: students
-- ============================================================
CREATE TABLE students (
    id              NUMBER          PRIMARY KEY,
    name            VARCHAR2(200)   NOT NULL,
    email           VARCHAR2(200),
    phone           VARCHAR2(30),
    birth_date      VARCHAR2(20),
    requested_date  VARCHAR2(30)    DEFAULT TO_CHAR(SYSDATE,'YYYY-MM-DD'),
    school_id       NUMBER          REFERENCES driving_schools(id) ON DELETE CASCADE,
    status          VARCHAR2(20)    DEFAULT 'pending'
                        CHECK (status IN ('pending','approved','rejected'))
);

CREATE OR REPLACE TRIGGER trg_students_id
BEFORE INSERT ON students FOR EACH ROW
BEGIN
    IF :NEW.id IS NULL THEN
        SELECT seq_students.NEXTVAL INTO :NEW.id FROM dual;
    END IF;
END;
/

-- ============================================================
-- TABLE: vehicles
-- ============================================================
CREATE TABLE vehicles (
    id              NUMBER          PRIMARY KEY,
    brand           VARCHAR2(100),
    model           VARCHAR2(100),
    year            NUMBER(4),
    plate_number    VARCHAR2(50),
    transmission    VARCHAR2(30)    DEFAULT 'Manual',
    status          VARCHAR2(30)    DEFAULT 'active'
                        CHECK (status IN ('active','maintenance','inactive')),
    photo_path      VARCHAR2(500),
    school_id       NUMBER          REFERENCES driving_schools(id) ON DELETE CASCADE
);

CREATE OR REPLACE TRIGGER trg_vehicles_id
BEFORE INSERT ON vehicles FOR EACH ROW
BEGIN
    IF :NEW.id IS NULL THEN
        SELECT seq_vehicles.NEXTVAL INTO :NEW.id FROM dual;
    END IF;
END;
/

-- ============================================================
-- TABLE: instructors
-- ============================================================
CREATE TABLE instructors (
    id              NUMBER          PRIMARY KEY,
    full_name       VARCHAR2(200)   NOT NULL,
    email           VARCHAR2(200),
    phone           VARCHAR2(30),
    role            VARCHAR2(20)    DEFAULT 'instructor'
                        CHECK (role IN ('instructor','admin')),
    available       NUMBER(1)       DEFAULT 1
                        CHECK (available IN (0,1)),
    school_id       NUMBER          REFERENCES driving_schools(id) ON DELETE CASCADE
);

CREATE OR REPLACE TRIGGER trg_instructors_id
BEFORE INSERT ON instructors FOR EACH ROW
BEGIN
    IF :NEW.id IS NULL THEN
        SELECT seq_instructors.NEXTVAL INTO :NEW.id FROM dual;
    END IF;
END;
/

-- ============================================================
-- SAMPLE DATA
-- ============================================================

-- Driving Schools
INSERT INTO driving_schools (name, students_count, vehicles_count, status, rating)
VALUES ('AutoDrive Monastir', 24, 6, 'active', 4.5);

INSERT INTO driving_schools (name, students_count, vehicles_count, status, rating)
VALUES ('SafeRoad Academy', 15, 4, 'active', 4.2);

INSERT INTO driving_schools (name, students_count, vehicles_count, status, rating)
VALUES ('Speed and Control', 8, 3, 'pending', 0);

INSERT INTO driving_schools (name, students_count, vehicles_count, status, rating)
VALUES ('ProDrive Center', 30, 8, 'suspended', 3.8);

-- Students (school_id=3 = used by InstructorDashboard)
INSERT INTO students (name, email, phone, birth_date, requested_date, school_id, status)
VALUES ('Ahmed Ben Ali', 'ahmed@email.com', '+216 22 111 222', '1998-05-10', '2026-03-20', 3, 'pending');

INSERT INTO students (name, email, phone, birth_date, requested_date, school_id, status)
VALUES ('Sara Trabelsi', 'sara@email.com', '+216 55 333 444', '2000-08-15', '2026-03-22', 3, 'pending');

INSERT INTO students (name, email, phone, birth_date, requested_date, school_id, status)
VALUES ('Mohamed Chaari', 'med@email.com', '+216 99 555 666', '1997-12-01', '2026-03-18', 3, 'approved');

INSERT INTO students (name, email, phone, birth_date, requested_date, school_id, status)
VALUES ('Fatma Ksouri', 'fatma@email.com', '+216 20 777 888', '2001-03-25', '2026-03-15', 3, 'approved');

-- Vehicles (school_id=3)
INSERT INTO vehicles (brand, model, year, plate_number, transmission, status, school_id)
VALUES ('Peugeot', '208', 2022, '123 TUN 4567', 'Manual', 'active', 3);

INSERT INTO vehicles (brand, model, year, plate_number, transmission, status, school_id)
VALUES ('Renault', 'Clio', 2021, '456 TUN 7890', 'Automatic', 'active', 3);

INSERT INTO vehicles (brand, model, year, plate_number, transmission, status, school_id)
VALUES ('Volkswagen', 'Polo', 2020, '789 TUN 1234', 'Manual', 'maintenance', 3);

INSERT INTO vehicles (brand, model, year, plate_number, transmission, status, school_id)
VALUES ('Toyota', 'Yaris', 2023, '321 TUN 6543', 'Automatic', 'active', 1);

-- Instructors (school_id=3)
INSERT INTO instructors (full_name, email, phone, role, available, school_id)
VALUES ('Karim Mansouri', 'karim@autodrive.tn', '+216 22 000 111', 'admin', 1, 3);

INSERT INTO instructors (full_name, email, phone, role, available, school_id)
VALUES ('Leila Gharbi', 'leila@autodrive.tn', '+216 55 000 222', 'instructor', 1, 3);

INSERT INTO instructors (full_name, email, phone, role, available, school_id)
VALUES ('Nabil Ferjani', 'nabil@autodrive.tn', '+216 98 000 333', 'instructor', 0, 3);

COMMIT;

-- ============================================================
-- VERIFY - show row counts
-- ============================================================
PROMPT === Row counts ===
SELECT 'driving_schools' AS table_name, COUNT(*) AS total_rows FROM driving_schools
UNION ALL
SELECT 'students',    COUNT(*) FROM students
UNION ALL
SELECT 'vehicles',    COUNT(*) FROM vehicles
UNION ALL
SELECT 'instructors', COUNT(*) FROM instructors;

PROMPT === driving_schools ===
SELECT id, name, status, rating FROM driving_schools;

PROMPT === Done! smart_driving workspace is ready. ===
EXIT;
