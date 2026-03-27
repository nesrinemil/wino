-- =====================================================
-- SMART DRIVING SCHOOL - COMPLETE INTEGRATED DATABASE
-- =====================================================
-- Oracle 11g Compatible
-- Modules:
--   1. Core Tables (Admin, Driving Schools, Cars, Students, Instructors)
--   2. Learning & Progress (Theory, Code, Circuit, Parking stages)
--   3. Session Booking & Availability
--   4. Payment (D17, Cash, Facture)
--   5. Sensor Analysis (Driving sessions, ML predictions)
--   6. Parking Training (Maneuvers, Arduino sensors, Exam results)
-- =====================================================
-- Changes vs previous version:
--   + New table: ADMIN
--   + New table: PAYMENT (facture, D17)
--   + DRIVING_SCHOOLS linked to ADMIN via admin_id FK
--   + CARS absorbs PARKING_VEHICLES (parking fields added)
--   + PARKING_VEHICLES table removed
--   + PARKING_EXAM_BOOKING merged into EXAM_REQUEST
--   + PARKING_EXAM_BOOKING table removed
--   + ADMIN_INSTRUCTORS and INSTRUCTORS linked to DRIVING_SCHOOLS
--   + STUDENTS linked to DRIVING_SCHOOLS
--   + address / city / created_at columns cleaned up
-- =====================================================

-- =====================================================
-- CLEANUP (Optional - Use with caution!)
-- =====================================================
/*
-- Parking module tables
DROP TABLE PARKING_STUDENT_STATS    CASCADE CONSTRAINTS;
DROP TABLE PARKING_EXAM_RESULTS     CASCADE CONSTRAINTS;
DROP TABLE PARKING_SENSOR_LOGS      CASCADE CONSTRAINTS;
DROP TABLE PARKING_SESSIONS         CASCADE CONSTRAINTS;
DROP TABLE PARKING_MANEUVER_STEPS   CASCADE CONSTRAINTS;

-- Sensor analysis tables
DROP TABLE RECOMMENDATIONS               CASCADE CONSTRAINTS;
DROP TABLE SESSION_ANALYSIS              CASCADE CONSTRAINTS;
DROP TABLE SENSOR_DATA                   CASCADE CONSTRAINTS;
DROP TABLE SESSION_PERFORMANCE_HISTORY   CASCADE CONSTRAINTS;
DROP TABLE ANALYSIS_SETTINGS             CASCADE CONSTRAINTS;
DROP TABLE DRIVING_SESSIONS              CASCADE CONSTRAINTS;

-- Payment
DROP TABLE PAYMENT                  CASCADE CONSTRAINTS;

-- Booking tables
DROP TABLE EXAM_REQUEST             CASCADE CONSTRAINTS;
DROP TABLE AVAILABILITY             CASCADE CONSTRAINTS;
DROP TABLE SESSION_BOOKING          CASCADE CONSTRAINTS;

-- Learning tables
DROP TABLE BADGES                   CASCADE CONSTRAINTS;
DROP TABLE QUIZ_RESULTS             CASCADE CONSTRAINTS;
DROP TABLE COURSE_LINKS             CASCADE CONSTRAINTS;
DROP TABLE USER_PROGRESS            CASCADE CONSTRAINTS;

-- Core tables
DROP TABLE INSTRUCTORS              CASCADE CONSTRAINTS;
DROP TABLE ADMIN_INSTRUCTORS        CASCADE CONSTRAINTS;
DROP TABLE STUDENTS                 CASCADE CONSTRAINTS;
DROP TABLE CARS                     CASCADE CONSTRAINTS;
DROP TABLE DRIVING_SCHOOLS          CASCADE CONSTRAINTS;
DROP TABLE ADMIN                    CASCADE CONSTRAINTS;

-- Sequences
DROP SEQUENCE admin_seq;
DROP SEQUENCE driving_schools_seq;
DROP SEQUENCE cars_seq;
DROP SEQUENCE students_seq;
DROP SEQUENCE admin_instructors_seq;
DROP SEQUENCE instructors_seq;
DROP SEQUENCE quiz_results_seq;
DROP SEQUENCE badges_seq;
DROP SEQUENCE SEQ_SESSION_ID;
DROP SEQUENCE SEQ_AVAILABILITY_ID;
DROP SEQUENCE SEQ_EXAM_REQUEST_ID;
DROP SEQUENCE SEQ_PAYMENT_ID;
DROP SEQUENCE seq_driving_session_id;
DROP SEQUENCE seq_sensor_data_id;
DROP SEQUENCE seq_session_analysis_id;
DROP SEQUENCE seq_recommendation_id;
DROP SEQUENCE seq_session_performance_id;
DROP SEQUENCE seq_parking_session_id;
DROP SEQUENCE seq_parking_sensor_log_id;
DROP SEQUENCE seq_parking_maneuver_step_id;
DROP SEQUENCE seq_parking_exam_result_id;
DROP SEQUENCE seq_parking_student_stat_id;
*/

-- #####################################################
-- #                                                   #
-- #   SECTION 1: CORE TABLES                          #
-- #                                                   #
-- #####################################################

------------------------------------------------------------
-- TABLE: ADMIN
-- Super-administrators who own / manage driving schools
------------------------------------------------------------
CREATE TABLE ADMIN (
    id            NUMBER(10)    PRIMARY KEY,
    full_name     VARCHAR2(150) NOT NULL,
    email         VARCHAR2(150) UNIQUE NOT NULL,
    password_hash VARCHAR2(255) NOT NULL,
    role          VARCHAR2(20)  DEFAULT 'ADMIN',
    status        VARCHAR2(20)  DEFAULT 'active',
    CONSTRAINT chk_admin_role   CHECK (role   IN ('SUPER_ADMIN', 'ADMIN')),
    CONSTRAINT chk_admin_status CHECK (status IN ('active', 'inactive'))
);

CREATE SEQUENCE admin_seq START WITH 1 INCREMENT BY 1 NOCACHE NOCYCLE;

CREATE OR REPLACE TRIGGER admin_bi
BEFORE INSERT ON ADMIN
FOR EACH ROW
BEGIN
    IF :NEW.id IS NULL THEN
        SELECT admin_seq.NEXTVAL INTO :NEW.id FROM dual;
    END IF;
END;
/

COMMENT ON TABLE  ADMIN        IS 'Platform super-administrators managing driving schools';
COMMENT ON COLUMN ADMIN.role   IS 'SUPER_ADMIN = platform owner; ADMIN = school owner';

------------------------------------------------------------
-- TABLE: DRIVING_SCHOOLS
-- Each school belongs to one ADMIN (1,1) -> (0,N)
------------------------------------------------------------
CREATE TABLE DRIVING_SCHOOLS (
    id       NUMBER(10)    PRIMARY KEY,
    admin_id NUMBER(10)    NOT NULL,
    name     VARCHAR2(150) NOT NULL,
    phone    VARCHAR2(20),
    email    VARCHAR2(150),
    status   VARCHAR2(20)  DEFAULT 'pending',
    CONSTRAINT fk_ds_admin  FOREIGN KEY (admin_id) REFERENCES ADMIN(id),
    CONSTRAINT chk_ds_status CHECK (status IN ('active', 'pending', 'suspended'))
);

CREATE SEQUENCE driving_schools_seq START WITH 1 INCREMENT BY 1 NOCACHE NOCYCLE;

CREATE OR REPLACE TRIGGER driving_schools_bi
BEFORE INSERT ON DRIVING_SCHOOLS
FOR EACH ROW
BEGIN
    IF :NEW.id IS NULL THEN
        SELECT driving_schools_seq.NEXTVAL INTO :NEW.id FROM dual;
    END IF;
END;
/

COMMENT ON TABLE  DRIVING_SCHOOLS          IS 'Driving school master data';
COMMENT ON COLUMN DRIVING_SCHOOLS.admin_id IS 'FK -> ADMIN: each school has exactly one admin (1,1)';

------------------------------------------------------------
-- TABLE: CARS
-- Merged with former PARKING_VEHICLES.
-- When is_parking_vehicle = 1, the parking columns are active.
------------------------------------------------------------
CREATE TABLE CARS (
    id                 NUMBER(10)    PRIMARY KEY,
    driving_school_id  NUMBER(10)    NOT NULL,
    brand              VARCHAR2(100) NOT NULL,
    model              VARCHAR2(100) NOT NULL,
    year               NUMBER(4),
    plate_number       VARCHAR2(50)  UNIQUE NOT NULL,
    transmission       VARCHAR2(20)  NOT NULL,
    is_active          NUMBER(1)     DEFAULT 1,
    image_path         VARCHAR2(255),
    -- Parking training extensions (active when is_parking_vehicle = 1)
    is_parking_vehicle NUMBER(1)     DEFAULT 0,
    assistance_type    VARCHAR2(200),
    sensor_count       NUMBER(3)     DEFAULT 5,
    serial_port        VARCHAR2(20),
    baud_rate          NUMBER(10)    DEFAULT 9600,
    session_fee        NUMBER(10,2)  DEFAULT 0,
    CONSTRAINT fk_cars_school       FOREIGN KEY (driving_school_id) REFERENCES DRIVING_SCHOOLS(id) ON DELETE CASCADE,
    CONSTRAINT chk_cars_active      CHECK (is_active          IN (0, 1)),
    CONSTRAINT chk_cars_parking_veh CHECK (is_parking_vehicle IN (0, 1))
);

CREATE SEQUENCE cars_seq START WITH 1 INCREMENT BY 1 NOCACHE NOCYCLE;

CREATE OR REPLACE TRIGGER cars_bi
BEFORE INSERT ON CARS
FOR EACH ROW
BEGIN
    IF :NEW.id IS NULL THEN
        SELECT cars_seq.NEXTVAL INTO :NEW.id FROM dual;
    END IF;
END;
/

COMMENT ON TABLE  CARS                       IS 'Vehicle fleet — also covers Arduino-equipped parking-training cars';
COMMENT ON COLUMN CARS.is_parking_vehicle    IS '1 = Arduino parking car; 0 = regular driving car';
COMMENT ON COLUMN CARS.assistance_type       IS 'Parking assistance features (camera, radar…) — parking cars only';
COMMENT ON COLUMN CARS.serial_port           IS 'Arduino serial port (e.g. COM3) — parking cars only';
COMMENT ON COLUMN CARS.baud_rate             IS 'Arduino baud rate — parking cars only';
COMMENT ON COLUMN CARS.session_fee           IS 'Fee per parking training session (TND) — parking cars only';

------------------------------------------------------------
-- TABLE: STUDENTS
-- Linked to DRIVING_SCHOOLS (1,1) -> (0,N)
------------------------------------------------------------
CREATE TABLE STUDENTS (
    id                     NUMBER(10)    PRIMARY KEY,
    driving_school_id      NUMBER(10)    NOT NULL,
    first_name             VARCHAR2(100) NOT NULL,
    last_name              VARCHAR2(100) NOT NULL,
    date_of_birth          DATE          NOT NULL,
    phone                  VARCHAR2(20)  NOT NULL,
    cin                    VARCHAR2(20)  UNIQUE NOT NULL,
    email                  VARCHAR2(150) UNIQUE NOT NULL,
    password_hash          VARCHAR2(255) NOT NULL,
    has_cin                NUMBER(1)     DEFAULT 0,
    has_face_photo         NUMBER(1)     DEFAULT 0,
    medical_certificate_path VARCHAR2(255),
    is_beginner            NUMBER(1)     DEFAULT 1,
    has_driving_license    NUMBER(1)     DEFAULT 0,
    has_code               NUMBER(1)     DEFAULT 0,
    has_conduite           NUMBER(1)     DEFAULT 0,
    course_level           VARCHAR2(20)  DEFAULT 'Theory',
    fingerprint_registered NUMBER(1)     DEFAULT 0,
    license_number         VARCHAR2(50),
    enrollment_date        DATE          DEFAULT SYSDATE,
    student_status         VARCHAR2(20)  DEFAULT 'ACTIVE',
    CONSTRAINT fk_students_school  FOREIGN KEY (driving_school_id) REFERENCES DRIVING_SCHOOLS(id),
    CONSTRAINT chk_student_status  CHECK (student_status IN ('ACTIVE', 'INACTIVE', 'GRADUATED'))
);

CREATE SEQUENCE students_seq START WITH 1 INCREMENT BY 1 NOCACHE NOCYCLE;

CREATE OR REPLACE TRIGGER students_bi
BEFORE INSERT ON STUDENTS
FOR EACH ROW
BEGIN
    IF :NEW.id IS NULL THEN
        SELECT students_seq.NEXTVAL INTO :NEW.id FROM dual;
    END IF;
END;
/

COMMENT ON TABLE  STUDENTS                    IS 'Student master data';
COMMENT ON COLUMN STUDENTS.driving_school_id  IS 'FK -> DRIVING_SCHOOLS (1,1)';
COMMENT ON COLUMN STUDENTS.student_status     IS 'ACTIVE / INACTIVE / GRADUATED';

------------------------------------------------------------
-- TABLE: ADMIN_INSTRUCTORS
-- Linked to DRIVING_SCHOOLS (1,1) -> (0,N)
------------------------------------------------------------
CREATE TABLE ADMIN_INSTRUCTORS (
    id                NUMBER(10)    PRIMARY KEY,
    driving_school_id NUMBER(10)    NOT NULL,
    full_name         VARCHAR2(150) NOT NULL,
    email             VARCHAR2(150) UNIQUE NOT NULL,
    password_hash     VARCHAR2(255) NOT NULL,
    school_name       VARCHAR2(150) NOT NULL,
    CONSTRAINT fk_ai_school FOREIGN KEY (driving_school_id) REFERENCES DRIVING_SCHOOLS(id)
);

CREATE SEQUENCE admin_instructors_seq START WITH 1 INCREMENT BY 1 NOCACHE NOCYCLE;

CREATE OR REPLACE TRIGGER admin_instructors_bi
BEFORE INSERT ON ADMIN_INSTRUCTORS
FOR EACH ROW
BEGIN
    IF :NEW.id IS NULL THEN
        SELECT admin_instructors_seq.NEXTVAL INTO :NEW.id FROM dual;
    END IF;
END;
/

COMMENT ON TABLE  ADMIN_INSTRUCTORS                    IS 'School-level administrator / chief instructor';
COMMENT ON COLUMN ADMIN_INSTRUCTORS.driving_school_id  IS 'FK -> DRIVING_SCHOOLS (1,1)';

------------------------------------------------------------
-- TABLE: INSTRUCTORS
-- Linked to ADMIN_INSTRUCTORS (1,1) AND DRIVING_SCHOOLS (1,1)
------------------------------------------------------------
CREATE TABLE INSTRUCTORS (
    id                  NUMBER(10)    PRIMARY KEY,
    admin_instructor_id NUMBER(10)    NOT NULL,
    driving_school_id   NUMBER(10)    NOT NULL,
    full_name           VARCHAR2(150) NOT NULL,
    email               VARCHAR2(150) UNIQUE NOT NULL,
    phone               VARCHAR2(20),
    password_hash       VARCHAR2(255) NOT NULL,
    hire_date           DATE          DEFAULT SYSDATE,
    specialization      VARCHAR2(100),
    instructor_status   VARCHAR2(20)  DEFAULT 'ACTIVE',
    CONSTRAINT fk_inst_admin  FOREIGN KEY (admin_instructor_id) REFERENCES ADMIN_INSTRUCTORS(id) ON DELETE CASCADE,
    CONSTRAINT fk_inst_school FOREIGN KEY (driving_school_id)   REFERENCES DRIVING_SCHOOLS(id),
    CONSTRAINT chk_inst_status CHECK (instructor_status IN ('ACTIVE', 'INACTIVE', 'ON_LEAVE'))
);

CREATE SEQUENCE instructors_seq START WITH 1 INCREMENT BY 1 NOCACHE NOCYCLE;

CREATE OR REPLACE TRIGGER instructors_bi
BEFORE INSERT ON INSTRUCTORS
FOR EACH ROW
BEGIN
    IF :NEW.id IS NULL THEN
        SELECT instructors_seq.NEXTVAL INTO :NEW.id FROM dual;
    END IF;
END;
/

COMMENT ON TABLE  INSTRUCTORS                    IS 'Driving instructors';
COMMENT ON COLUMN INSTRUCTORS.driving_school_id  IS 'FK -> DRIVING_SCHOOLS (1,1)';
COMMENT ON COLUMN INSTRUCTORS.instructor_status  IS 'ACTIVE / INACTIVE / ON_LEAVE';

-- #####################################################
-- #                                                   #
-- #   SECTION 2: LEARNING & PROGRESS TABLES           #
-- #                                                   #
-- #####################################################

------------------------------------------------------------
-- TABLE: USER_PROGRESS
------------------------------------------------------------
CREATE TABLE USER_PROGRESS (
    user_id                   NUMBER(10) PRIMARY KEY,
    current_section           NUMBER(10) DEFAULT 0,
    streak_count              NUMBER(10) DEFAULT 0,
    total_score               NUMBER(10) DEFAULT 0,
    progress_percent          NUMBER(10) DEFAULT 0,
    last_visit_date           VARCHAR2(20),
    quizzes_passed_this_month NUMBER(10) DEFAULT 0,
    month_of_quizzes          NUMBER(10) DEFAULT 0,
    vr_unlocked               NUMBER(1)  DEFAULT 0,
    vr_points_earned          NUMBER(10) DEFAULT 0,
    -- Practical training progression CODE -> CIRCUIT -> PARKING
    current_step              NUMBER(1)  DEFAULT 1 NOT NULL,
    code_status               VARCHAR2(20) DEFAULT 'IN_PROGRESS',
    circuit_status            VARCHAR2(20) DEFAULT 'LOCKED',
    parking_status            VARCHAR2(20) DEFAULT 'LOCKED',
    code_score                NUMBER(5,2),
    circuit_score             NUMBER(5,2),
    parking_score             NUMBER(5,2),
    code_passed_date          TIMESTAMP,
    circuit_passed_date       TIMESTAMP,
    parking_passed_date       TIMESTAMP,
    is_qualified              NUMBER(1)  DEFAULT 0,
    unpaid_sessions_count     NUMBER(10) DEFAULT 0,
    CONSTRAINT chk_up_step    CHECK (current_step IN (1, 2, 3)),
    CONSTRAINT fk_up_student  FOREIGN KEY (user_id) REFERENCES STUDENTS(id)
);

COMMENT ON TABLE  USER_PROGRESS             IS 'Student learning progress (theory + practical stages)';
COMMENT ON COLUMN USER_PROGRESS.current_step IS '1=CODE  2=CIRCUIT  3=PARKING';

------------------------------------------------------------
-- TABLE: QUIZ_RESULTS
------------------------------------------------------------
CREATE TABLE QUIZ_RESULTS (
    id              NUMBER(10) PRIMARY KEY,
    user_id         NUMBER(10) NOT NULL,
    section_index   NUMBER(10),
    correct_answers NUMBER(10),
    passed          NUMBER(1),
    date_taken      VARCHAR2(20),
    CONSTRAINT fk_qr_user FOREIGN KEY (user_id) REFERENCES USER_PROGRESS(user_id)
);

CREATE SEQUENCE quiz_results_seq START WITH 1 INCREMENT BY 1 NOCACHE NOCYCLE;

CREATE OR REPLACE TRIGGER quiz_results_bi
BEFORE INSERT ON QUIZ_RESULTS
FOR EACH ROW
BEGIN
    IF :NEW.id IS NULL THEN
        SELECT quiz_results_seq.NEXTVAL INTO :NEW.id FROM dual;
    END IF;
END;
/

COMMENT ON TABLE QUIZ_RESULTS IS 'Quiz scores and pass/fail results per section';

------------------------------------------------------------
-- TABLE: BADGES
------------------------------------------------------------
CREATE TABLE BADGES (
    id          NUMBER(10)    PRIMARY KEY,
    user_id     NUMBER(10)    NOT NULL,
    badge_name  VARCHAR2(150),
    date_earned VARCHAR2(20),
    CONSTRAINT fk_badges_user FOREIGN KEY (user_id) REFERENCES USER_PROGRESS(user_id)
);

CREATE SEQUENCE badges_seq START WITH 1 INCREMENT BY 1 NOCACHE NOCYCLE;

CREATE OR REPLACE TRIGGER badges_bi
BEFORE INSERT ON BADGES
FOR EACH ROW
BEGIN
    IF :NEW.id IS NULL THEN
        SELECT badges_seq.NEXTVAL INTO :NEW.id FROM dual;
    END IF;
END;
/

COMMENT ON TABLE BADGES IS 'Achievement badges earned by students';

------------------------------------------------------------
-- TABLE: COURSE_LINKS
-- Referenced by USER_PROGRESS.current_section (0,N) -> (1,1)
------------------------------------------------------------
CREATE TABLE COURSE_LINKS (
    section_index NUMBER(10)    PRIMARY KEY,
    video_link    VARCHAR2(500)
);

COMMENT ON TABLE  COURSE_LINKS               IS 'Video links for each theory section';
COMMENT ON COLUMN COURSE_LINKS.section_index IS 'Referenced by USER_PROGRESS.current_section';

-- #####################################################
-- #                                                   #
-- #   SECTION 3: SESSION BOOKING & AVAILABILITY       #
-- #                                                   #
-- #####################################################

CREATE SEQUENCE SEQ_SESSION_ID      START WITH 1 INCREMENT BY 1 NOCACHE NOCYCLE;
CREATE SEQUENCE SEQ_AVAILABILITY_ID START WITH 1 INCREMENT BY 1 NOCACHE NOCYCLE;
CREATE SEQUENCE SEQ_EXAM_REQUEST_ID START WITH 1 INCREMENT BY 1 NOCACHE NOCYCLE;

------------------------------------------------------------
-- TABLE: SESSION_BOOKING
------------------------------------------------------------
CREATE TABLE SESSION_BOOKING (
    SESSION_ID    NUMBER(10)   PRIMARY KEY,
    STUDENT_ID    NUMBER(10)   NOT NULL,
    INSTRUCTOR_ID NUMBER(10)   NOT NULL,
    SESSION_STEP  NUMBER(1)    NOT NULL,
    SESSION_DATE  DATE         NOT NULL,
    START_TIME    VARCHAR2(5)  NOT NULL,
    END_TIME      VARCHAR2(5)  NOT NULL,
    AMOUNT        NUMBER(10,3) NOT NULL,
    STATUS        VARCHAR2(20) DEFAULT 'PENDING',
    NOTES         CLOB,
    CONSTRAINT chk_sb_step   CHECK (SESSION_STEP IN (1, 2, 3)),
    CONSTRAINT chk_sb_status CHECK (STATUS IN ('PENDING', 'CONFIRMED', 'COMPLETED', 'CANCELLED')),
    CONSTRAINT fk_sb_student    FOREIGN KEY (STUDENT_ID)    REFERENCES STUDENTS(id),
    CONSTRAINT fk_sb_instructor FOREIGN KEY (INSTRUCTOR_ID) REFERENCES INSTRUCTORS(id)
);

CREATE OR REPLACE TRIGGER trg_session_id
BEFORE INSERT ON SESSION_BOOKING
FOR EACH ROW
BEGIN
    IF :NEW.SESSION_ID IS NULL THEN
        SELECT SEQ_SESSION_ID.NEXTVAL INTO :NEW.SESSION_ID FROM dual;
    END IF;
END;
/

COMMENT ON TABLE SESSION_BOOKING IS 'Scheduled driving lesson appointments';

CREATE INDEX idx_sb_student ON SESSION_BOOKING(STUDENT_ID);
CREATE INDEX idx_sb_date    ON SESSION_BOOKING(SESSION_DATE);

------------------------------------------------------------
-- TABLE: AVAILABILITY
------------------------------------------------------------
CREATE TABLE AVAILABILITY (
    AVAILABILITY_ID NUMBER(10)  PRIMARY KEY,
    INSTRUCTOR_ID   NUMBER(10)  NOT NULL,
    DAY_OF_WEEK     VARCHAR2(10) NOT NULL,
    START_TIME      VARCHAR2(5) NOT NULL,
    END_TIME        VARCHAR2(5) NOT NULL,
    IS_ACTIVE       NUMBER(1)   DEFAULT 1,
    CONSTRAINT chk_avail_day  CHECK (DAY_OF_WEEK IN ('MONDAY','TUESDAY','WEDNESDAY','THURSDAY','FRIDAY','SATURDAY','SUNDAY')),
    CONSTRAINT fk_avail_inst  FOREIGN KEY (INSTRUCTOR_ID) REFERENCES INSTRUCTORS(id)
);

CREATE OR REPLACE TRIGGER trg_availability_id
BEFORE INSERT ON AVAILABILITY
FOR EACH ROW
BEGIN
    IF :NEW.AVAILABILITY_ID IS NULL THEN
        SELECT SEQ_AVAILABILITY_ID.NEXTVAL INTO :NEW.AVAILABILITY_ID FROM dual;
    END IF;
END;
/

COMMENT ON TABLE AVAILABILITY IS 'Instructor weekly availability schedule';

CREATE INDEX idx_avail_inst ON AVAILABILITY(INSTRUCTOR_ID);

------------------------------------------------------------
-- TABLE: EXAM_REQUEST
-- Covers all 3 exam types (CODE / CIRCUIT / PARKING).
-- When EXAM_STEP = 3 (PARKING), the parking-specific columns
-- car_id, maneuver_type and time_slot become mandatory.
------------------------------------------------------------
CREATE TABLE EXAM_REQUEST (
    REQUEST_ID       NUMBER(10)    PRIMARY KEY,
    STUDENT_ID       NUMBER(10)    NOT NULL,
    INSTRUCTOR_ID    NUMBER(10)    NOT NULL,
    EXAM_STEP        NUMBER(1)     NOT NULL,
    REQUESTED_DATE   DATE,
    CONFIRMED_DATE   DATE,
    CONFIRMED_TIME   VARCHAR2(5),
    AMOUNT           NUMBER(10,3)  NOT NULL,
    D17_CODE         VARCHAR2(50),
    D17_PHONE        VARCHAR2(20),
    STATUS           VARCHAR2(20)  DEFAULT 'PENDING',
    REJECTION_REASON VARCHAR2(500),
    PASSED           NUMBER(1),
    SCORE            NUMBER(5,2),
    COMMENTS         CLOB,
    -- Parking exam specific (active when EXAM_STEP = 3)
    CAR_ID           NUMBER(10),
    MANEUVER_TYPE    VARCHAR2(30),
    TIME_SLOT        VARCHAR2(20),
    CONSTRAINT chk_er_step      CHECK (EXAM_STEP IN (1, 2, 3)),
    CONSTRAINT chk_er_status    CHECK (STATUS IN ('PENDING','APPROVED','PAID','COMPLETED','REJECTED','CANCELLED')),
    CONSTRAINT chk_er_passed    CHECK (PASSED IN (0, 1)),
    CONSTRAINT chk_er_maneuver  CHECK (MANEUVER_TYPE IS NULL OR MANEUVER_TYPE IN ('creneau','bataille','epi','marche_arriere','demi_tour')),
    CONSTRAINT fk_er_student    FOREIGN KEY (STUDENT_ID)    REFERENCES STUDENTS(id),
    CONSTRAINT fk_er_instructor FOREIGN KEY (INSTRUCTOR_ID) REFERENCES INSTRUCTORS(id),
    CONSTRAINT fk_er_car        FOREIGN KEY (CAR_ID)        REFERENCES CARS(id)
);

CREATE OR REPLACE TRIGGER trg_exam_request_id
BEFORE INSERT ON EXAM_REQUEST
FOR EACH ROW
BEGIN
    IF :NEW.REQUEST_ID IS NULL THEN
        SELECT SEQ_EXAM_REQUEST_ID.NEXTVAL INTO :NEW.REQUEST_ID FROM dual;
    END IF;
END;
/

COMMENT ON TABLE  EXAM_REQUEST              IS 'Exam booking requests (CODE / CIRCUIT / PARKING)';
COMMENT ON COLUMN EXAM_REQUEST.CAR_ID       IS 'Arduino parking car — required only when EXAM_STEP = 3';
COMMENT ON COLUMN EXAM_REQUEST.MANEUVER_TYPE IS 'Parking maneuver type — required only when EXAM_STEP = 3';
COMMENT ON COLUMN EXAM_REQUEST.TIME_SLOT    IS 'Parking exam time slot e.g. 09:00-10:00 — EXAM_STEP = 3';

CREATE INDEX idx_er_student ON EXAM_REQUEST(STUDENT_ID);
CREATE INDEX idx_er_status  ON EXAM_REQUEST(STATUS);

-- #####################################################
-- #                                                   #
-- #   SECTION 4: PAYMENT                              #
-- #                                                   #
-- #####################################################

CREATE SEQUENCE SEQ_PAYMENT_ID START WITH 1 INCREMENT BY 1 NOCACHE NOCYCLE;

------------------------------------------------------------
-- TABLE: PAYMENT
-- Covers payments for SESSION_BOOKING and EXAM_REQUEST.
-- Supports both D17 (mobile) and cash payment methods.
-- Each payment generates a unique facture (invoice).
------------------------------------------------------------
CREATE TABLE PAYMENT (
    PAYMENT_ID      NUMBER(10)    PRIMARY KEY,
    STUDENT_ID      NUMBER(10)    NOT NULL,
    SESSION_ID      NUMBER(10),
    EXAM_REQUEST_ID NUMBER(10),
    AMOUNT          NUMBER(10,3)  NOT NULL,
    PAYMENT_METHOD  VARCHAR2(20)  NOT NULL,
    D17_CODE        VARCHAR2(50),
    D17_PHONE       VARCHAR2(20),
    FACTURE_NUMBER  VARCHAR2(50)  UNIQUE,
    FACTURE_DATE    DATE          DEFAULT SYSDATE,
    STATUS          VARCHAR2(20)  DEFAULT 'PENDING',
    CONSTRAINT chk_pay_method  CHECK (PAYMENT_METHOD IN ('D17', 'CASH')),
    CONSTRAINT chk_pay_status  CHECK (STATUS IN ('PENDING', 'PAID', 'FAILED')),
    CONSTRAINT fk_pay_student  FOREIGN KEY (STUDENT_ID)      REFERENCES STUDENTS(id),
    CONSTRAINT fk_pay_session  FOREIGN KEY (SESSION_ID)      REFERENCES SESSION_BOOKING(SESSION_ID),
    CONSTRAINT fk_pay_exam     FOREIGN KEY (EXAM_REQUEST_ID) REFERENCES EXAM_REQUEST(REQUEST_ID)
);

CREATE OR REPLACE TRIGGER trg_payment_id
BEFORE INSERT ON PAYMENT
FOR EACH ROW
BEGIN
    IF :NEW.PAYMENT_ID IS NULL THEN
        SELECT SEQ_PAYMENT_ID.NEXTVAL INTO :NEW.PAYMENT_ID FROM dual;
    END IF;
    -- Auto-generate facture number if not provided
    IF :NEW.FACTURE_NUMBER IS NULL THEN
        :NEW.FACTURE_NUMBER := 'FAC-' || TO_CHAR(SYSDATE,'YYYYMMDD') || '-' || SEQ_PAYMENT_ID.CURRVAL;
    END IF;
END;
/

COMMENT ON TABLE  PAYMENT                IS 'All payments (sessions + exams) with D17 and cash support';
COMMENT ON COLUMN PAYMENT.D17_CODE       IS 'D17 transaction code — required when PAYMENT_METHOD = D17';
COMMENT ON COLUMN PAYMENT.D17_PHONE      IS 'D17 linked phone number';
COMMENT ON COLUMN PAYMENT.FACTURE_NUMBER IS 'Unique invoice reference auto-generated on insert';

CREATE INDEX idx_pay_student ON PAYMENT(STUDENT_ID);
CREATE INDEX idx_pay_status  ON PAYMENT(STATUS);

-- #####################################################
-- #                                                   #
-- #   SECTION 5: SENSOR ANALYSIS TABLES               #
-- #                                                   #
-- #####################################################

CREATE SEQUENCE seq_driving_session_id  START WITH 1 INCREMENT BY 1 NOCACHE NOCYCLE;
CREATE SEQUENCE seq_sensor_data_id      START WITH 1 INCREMENT BY 1 CACHE 1000 NOCYCLE;
CREATE SEQUENCE seq_session_analysis_id START WITH 1 INCREMENT BY 1 NOCACHE NOCYCLE;
CREATE SEQUENCE seq_recommendation_id   START WITH 1 INCREMENT BY 1 NOCACHE NOCYCLE;
CREATE SEQUENCE seq_session_performance_id START WITH 1 INCREMENT BY 1 NOCACHE NOCYCLE;

------------------------------------------------------------
-- TABLE: DRIVING_SESSIONS
------------------------------------------------------------
CREATE TABLE DRIVING_SESSIONS (
    driving_session_id NUMBER(10)    PRIMARY KEY,
    booking_id         NUMBER(10),
    student_id         NUMBER(10)    NOT NULL,
    instructor_id      NUMBER(10)    NOT NULL,
    session_date       DATE          NOT NULL,
    session_time       TIMESTAMP(6)  DEFAULT SYSTIMESTAMP,
    duration           NUMBER(10,2),
    road_type          VARCHAR2(20)  NOT NULL,
    weather            VARCHAR2(20)  NOT NULL,
    start_location     VARCHAR2(100),
    end_location       VARCHAR2(100),
    csv_file_path      VARCHAR2(500),
    notes              CLOB,
    CONSTRAINT fk_ds_booking    FOREIGN KEY (booking_id)    REFERENCES SESSION_BOOKING(SESSION_ID),
    CONSTRAINT fk_ds_student    FOREIGN KEY (student_id)    REFERENCES STUDENTS(id)    ON DELETE CASCADE,
    CONSTRAINT fk_ds_instructor FOREIGN KEY (instructor_id) REFERENCES INSTRUCTORS(id),
    CONSTRAINT chk_ds_road    CHECK (road_type IN ('neighborhood', 'normal', 'highway')),
    CONSTRAINT chk_ds_weather CHECK (weather   IN ('sunny', 'rain', 'fog'))
);

CREATE OR REPLACE TRIGGER trg_driving_session_id
BEFORE INSERT ON DRIVING_SESSIONS
FOR EACH ROW
BEGIN
    IF :NEW.driving_session_id IS NULL THEN
        SELECT seq_driving_session_id.NEXTVAL INTO :NEW.driving_session_id FROM dual;
    END IF;
END;
/

COMMENT ON TABLE  DRIVING_SESSIONS           IS 'Completed driving sessions with sensor data collection';
COMMENT ON COLUMN DRIVING_SESSIONS.booking_id IS 'Optional link to the originating SESSION_BOOKING (0,1)';

CREATE INDEX idx_ds_student    ON DRIVING_SESSIONS(student_id);
CREATE INDEX idx_ds_instructor ON DRIVING_SESSIONS(instructor_id);
CREATE INDEX idx_ds_date       ON DRIVING_SESSIONS(session_date);
CREATE INDEX idx_ds_booking    ON DRIVING_SESSIONS(booking_id);

------------------------------------------------------------
-- TABLE: SENSOR_DATA
------------------------------------------------------------
CREATE TABLE SENSOR_DATA (
    sensor_data_id     NUMBER(15)   PRIMARY KEY,
    driving_session_id NUMBER(10)   NOT NULL,
    data_timestamp     DATE         NOT NULL,
    time_offset        NUMBER(10,4),
    accel_x            NUMBER(10,4),
    accel_y            NUMBER(10,4),
    accel_z            NUMBER(10,4),
    gyro_x             NUMBER(10,4),
    gyro_y             NUMBER(10,4),
    gyro_z             NUMBER(10,4),
    gps_latitude       NUMBER(12,8),
    gps_longitude      NUMBER(12,8),
    speed              NUMBER(10,2),
    sensor_front       NUMBER(10,2),
    sensor_back_left   NUMBER(10,2),
    sensor_back_right  NUMBER(10,2),
    sensor_side_left   NUMBER(10,2),
    sensor_side_right  NUMBER(10,2),
    CONSTRAINT fk_sd_session FOREIGN KEY (driving_session_id) REFERENCES DRIVING_SESSIONS(driving_session_id) ON DELETE CASCADE
);

CREATE OR REPLACE TRIGGER trg_sensor_data_id
BEFORE INSERT ON SENSOR_DATA
FOR EACH ROW
BEGIN
    IF :NEW.sensor_data_id IS NULL THEN
        SELECT seq_sensor_data_id.NEXTVAL INTO :NEW.sensor_data_id FROM dual;
    END IF;
END;
/

COMMENT ON TABLE SENSOR_DATA IS 'Raw vehicle sensor readings (accelerometer, GPS, proximity)';

CREATE INDEX idx_sd_session   ON SENSOR_DATA(driving_session_id);
CREATE INDEX idx_sd_timestamp ON SENSOR_DATA(data_timestamp);

------------------------------------------------------------
-- TABLE: SESSION_ANALYSIS
------------------------------------------------------------
CREATE TABLE SESSION_ANALYSIS (
    analysis_id         NUMBER(10)   PRIMARY KEY,
    driving_session_id  NUMBER(10)   UNIQUE NOT NULL,
    stress_index        NUMBER(5,4)  NOT NULL,
    risk_score          NUMBER(5,4)  NOT NULL,
    overall_performance VARCHAR2(20),
    avg_acceleration    NUMBER(10,4),
    avg_speed           NUMBER(10,2),
    max_speed           NUMBER(10,2),
    avg_gyro            NUMBER(10,4),
    harsh_accel_count   NUMBER(10),
    sharp_turn_count    NUMBER(10),
    proximity_events    NUMBER(10),
    critical_events     NUMBER(10),
    ml_prediction       VARCHAR2(20),
    ml_confidence       NUMBER(5,4),
    ml_risk_level       VARCHAR2(20),
    analysis_date       DATE         DEFAULT SYSDATE,
    CONSTRAINT fk_sa_session FOREIGN KEY (driving_session_id) REFERENCES DRIVING_SESSIONS(driving_session_id) ON DELETE CASCADE,
    CONSTRAINT chk_sa_stress      CHECK (stress_index BETWEEN 0 AND 1),
    CONSTRAINT chk_sa_risk        CHECK (risk_score   BETWEEN 0 AND 1),
    CONSTRAINT chk_sa_performance CHECK (overall_performance IN ('Excellent','Good','Fair','Needs Improvement')),
    CONSTRAINT chk_sa_ml_pred     CHECK (ml_prediction       IN ('PASS','FAIL','UNKNOWN'))
);

CREATE OR REPLACE TRIGGER trg_session_analysis_id
BEFORE INSERT ON SESSION_ANALYSIS
FOR EACH ROW
BEGIN
    IF :NEW.analysis_id IS NULL THEN
        SELECT seq_session_analysis_id.NEXTVAL INTO :NEW.analysis_id FROM dual;
    END IF;
END;
/

COMMENT ON TABLE SESSION_ANALYSIS IS 'AI-calculated performance metrics and ML predictions per session';

CREATE INDEX idx_sa_session ON SESSION_ANALYSIS(driving_session_id);

------------------------------------------------------------
-- TABLE: RECOMMENDATIONS
------------------------------------------------------------
CREATE TABLE RECOMMENDATIONS (
    recommendation_id  NUMBER(10)    PRIMARY KEY,
    driving_session_id NUMBER(10)    NOT NULL,
    priority           VARCHAR2(20)  NOT NULL,
    category           VARCHAR2(50)  NOT NULL,
    message            VARCHAR2(500) NOT NULL,
    suggestion         VARCHAR2(500) NOT NULL,
    created_date       DATE          DEFAULT SYSDATE,
    CONSTRAINT fk_rec_session  FOREIGN KEY (driving_session_id) REFERENCES DRIVING_SESSIONS(driving_session_id) ON DELETE CASCADE,
    CONSTRAINT chk_rec_priority CHECK (priority IN ('CRITICAL','HIGH','MEDIUM','LOW'))
);

CREATE OR REPLACE TRIGGER trg_recommendation_id
BEFORE INSERT ON RECOMMENDATIONS
FOR EACH ROW
BEGIN
    IF :NEW.recommendation_id IS NULL THEN
        SELECT seq_recommendation_id.NEXTVAL INTO :NEW.recommendation_id FROM dual;
    END IF;
END;
/

COMMENT ON TABLE RECOMMENDATIONS IS 'AI-generated driving improvement recommendations';

CREATE INDEX idx_rec_session  ON RECOMMENDATIONS(driving_session_id);
CREATE INDEX idx_rec_priority ON RECOMMENDATIONS(priority);

------------------------------------------------------------
-- TABLE: SESSION_PERFORMANCE_HISTORY
------------------------------------------------------------
CREATE TABLE SESSION_PERFORMANCE_HISTORY (
    performance_id     NUMBER(10)   PRIMARY KEY,
    student_id         NUMBER(10)   NOT NULL,
    driving_session_id NUMBER(10)   NOT NULL,
    session_number     NUMBER(5)    NOT NULL,
    stress_index       NUMBER(5,4),
    risk_score         NUMBER(5,4),
    improvement        NUMBER(6,2),
    tracking_date      DATE         DEFAULT SYSDATE,
    CONSTRAINT fk_sph_student  FOREIGN KEY (student_id)         REFERENCES STUDENTS(id)                                  ON DELETE CASCADE,
    CONSTRAINT fk_sph_session  FOREIGN KEY (driving_session_id) REFERENCES DRIVING_SESSIONS(driving_session_id)         ON DELETE CASCADE
);

CREATE OR REPLACE TRIGGER trg_session_performance_id
BEFORE INSERT ON SESSION_PERFORMANCE_HISTORY
FOR EACH ROW
BEGIN
    IF :NEW.performance_id IS NULL THEN
        SELECT seq_session_performance_id.NEXTVAL INTO :NEW.performance_id FROM dual;
    END IF;
END;
/

COMMENT ON TABLE SESSION_PERFORMANCE_HISTORY IS 'Session-by-session performance tracking for trend analysis';

CREATE INDEX idx_sph_student    ON SESSION_PERFORMANCE_HISTORY(student_id);
CREATE INDEX idx_sph_session_num ON SESSION_PERFORMANCE_HISTORY(student_id, session_number);

------------------------------------------------------------
-- TABLE: ANALYSIS_SETTINGS
-- Autonomous configuration table — drives ML algorithm parameters.
------------------------------------------------------------
CREATE TABLE ANALYSIS_SETTINGS (
    setting_key   VARCHAR2(100) PRIMARY KEY,
    setting_value VARCHAR2(500),
    setting_type  VARCHAR2(20),
    description   VARCHAR2(200),
    last_modified DATE          DEFAULT SYSDATE,
    CONSTRAINT chk_as_type CHECK (setting_type IN ('STRING','NUMBER','BOOLEAN'))
);

COMMENT ON TABLE ANALYSIS_SETTINGS IS 'Configuration for sensor analysis algorithms — no FK (autonomous reference table)';

-- #####################################################
-- #                                                   #
-- #   SECTION 6: PARKING TRAINING TABLES              #
-- #                                                   #
-- #####################################################

CREATE SEQUENCE seq_parking_session_id       START WITH 1 INCREMENT BY 1 NOCACHE NOCYCLE;
CREATE SEQUENCE seq_parking_sensor_log_id    START WITH 1 INCREMENT BY 1 CACHE 1000 NOCYCLE;
CREATE SEQUENCE seq_parking_maneuver_step_id START WITH 1 INCREMENT BY 1 NOCACHE NOCYCLE;
CREATE SEQUENCE seq_parking_exam_result_id   START WITH 1 INCREMENT BY 1 NOCACHE NOCYCLE;
CREATE SEQUENCE seq_parking_student_stat_id  START WITH 1 INCREMENT BY 1 NOCACHE NOCYCLE;

------------------------------------------------------------
-- TABLE: PARKING_SESSIONS
-- References CARS (is_parking_vehicle = 1) instead of
-- the old PARKING_VEHICLES table.
------------------------------------------------------------
CREATE TABLE PARKING_SESSIONS (
    session_id       NUMBER(10)   PRIMARY KEY,
    student_id       NUMBER(10)   NOT NULL,
    car_id           NUMBER(10)   NOT NULL,
    maneuver_type    VARCHAR2(30) NOT NULL,
    session_start    TIMESTAMP    DEFAULT SYSTIMESTAMP,
    session_end      TIMESTAMP,
    duration_seconds NUMBER(10)   DEFAULT 0,
    is_exam_mode     NUMBER(1)    DEFAULT 0,
    is_successful    NUMBER(1)    DEFAULT 0,
    steps_completed  NUMBER(5)    DEFAULT 0,
    error_count      NUMBER(5)    DEFAULT 0,
    stall_count      NUMBER(5)    DEFAULT 0,
    obstacle_contact NUMBER(1)    DEFAULT 0,
    notes            CLOB,
    CONSTRAINT fk_ps_student  FOREIGN KEY (student_id) REFERENCES STUDENTS(id),
    CONSTRAINT fk_ps_car      FOREIGN KEY (car_id)     REFERENCES CARS(id),
    CONSTRAINT chk_ps_maneuver CHECK (maneuver_type IN ('creneau','bataille','epi','marche_arriere','demi_tour')),
    CONSTRAINT chk_ps_exam     CHECK (is_exam_mode    IN (0, 1)),
    CONSTRAINT chk_ps_success  CHECK (is_successful   IN (0, 1)),
    CONSTRAINT chk_ps_contact  CHECK (obstacle_contact IN (0, 1))
);

CREATE OR REPLACE TRIGGER trg_parking_session_id
BEFORE INSERT ON PARKING_SESSIONS
FOR EACH ROW
BEGIN
    IF :NEW.session_id IS NULL THEN
        SELECT seq_parking_session_id.NEXTVAL INTO :NEW.session_id FROM dual;
    END IF;
END;
/

COMMENT ON TABLE  PARKING_SESSIONS         IS 'Parking maneuver training and exam sessions';
COMMENT ON COLUMN PARKING_SESSIONS.car_id  IS 'FK -> CARS (is_parking_vehicle=1) replaces old PARKING_VEHICLES';

CREATE INDEX idx_ps_student  ON PARKING_SESSIONS(student_id);
CREATE INDEX idx_ps_car      ON PARKING_SESSIONS(car_id);
CREATE INDEX idx_ps_maneuver ON PARKING_SESSIONS(maneuver_type);
CREATE INDEX idx_ps_start    ON PARKING_SESSIONS(session_start);

------------------------------------------------------------
-- TABLE: PARKING_SENSOR_LOGS
------------------------------------------------------------
CREATE TABLE PARKING_SENSOR_LOGS (
    log_id                 NUMBER(15)   PRIMARY KEY,
    session_id             NUMBER(10)   NOT NULL,
    log_timestamp          TIMESTAMP    DEFAULT SYSTIMESTAMP,
    sensor_front           NUMBER(1)    DEFAULT 0,
    sensor_left            NUMBER(1)    DEFAULT 0,
    sensor_right           NUMBER(1)    DEFAULT 0,
    sensor_rear_left       NUMBER(1)    DEFAULT 0,
    sensor_rear_right      NUMBER(1)    DEFAULT 0,
    distance_front_cm      NUMBER(10,2) DEFAULT -1,
    distance_left_cm       NUMBER(10,2) DEFAULT -1,
    distance_right_cm      NUMBER(10,2) DEFAULT -1,
    distance_rear_left_cm  NUMBER(10,2) DEFAULT -1,
    distance_rear_right_cm NUMBER(10,2) DEFAULT -1,
    event_type             VARCHAR2(50),
    display_message        VARCHAR2(500),
    current_step           NUMBER(5)    DEFAULT 0,
    CONSTRAINT fk_psl_session FOREIGN KEY (session_id) REFERENCES PARKING_SESSIONS(session_id) ON DELETE CASCADE,
    CONSTRAINT chk_psl_event  CHECK (event_type IS NULL OR event_type IN ('CALAGE','CONTACT_OBSTACLE','STEP_ADVANCE','ERROR'))
);

CREATE OR REPLACE TRIGGER trg_parking_sensor_log_id
BEFORE INSERT ON PARKING_SENSOR_LOGS
FOR EACH ROW
BEGIN
    IF :NEW.log_id IS NULL THEN
        SELECT seq_parking_sensor_log_id.NEXTVAL INTO :NEW.log_id FROM dual;
    END IF;
END;
/

COMMENT ON TABLE PARKING_SENSOR_LOGS IS 'Real-time Arduino ultrasonic sensor readings during parking sessions';

CREATE INDEX idx_psl_session   ON PARKING_SENSOR_LOGS(session_id);
CREATE INDEX idx_psl_timestamp ON PARKING_SENSOR_LOGS(log_timestamp);
CREATE INDEX idx_psl_event     ON PARKING_SENSOR_LOGS(event_type);

------------------------------------------------------------
-- TABLE: PARKING_MANEUVER_STEPS
-- Autonomous reference table — no FK.
-- Referenced by PARKING_SESSIONS via maneuver_type.
------------------------------------------------------------
CREATE TABLE PARKING_MANEUVER_STEPS (
    step_id          NUMBER(10)    PRIMARY KEY,
    maneuver_type    VARCHAR2(30)  NOT NULL,
    step_number      NUMBER(5)     NOT NULL,
    step_name        VARCHAR2(200) NOT NULL,
    sensor_condition VARCHAR2(500) NOT NULL,
    guidance_message VARCHAR2(500) NOT NULL,
    audio_message    VARCHAR2(500),
    delay_before_ms  NUMBER(10)    DEFAULT 0,
    is_stop_required NUMBER(1)     DEFAULT 0,
    CONSTRAINT uq_pms_step    UNIQUE (maneuver_type, step_number),
    CONSTRAINT chk_pms_maneuver CHECK (maneuver_type IN ('creneau','bataille','epi','marche_arriere','demi_tour')),
    CONSTRAINT chk_pms_stop     CHECK (is_stop_required IN (0, 1))
);

CREATE OR REPLACE TRIGGER trg_parking_maneuver_step_id
BEFORE INSERT ON PARKING_MANEUVER_STEPS
FOR EACH ROW
BEGIN
    IF :NEW.step_id IS NULL THEN
        SELECT seq_parking_maneuver_step_id.NEXTVAL INTO :NEW.step_id FROM dual;
    END IF;
END;
/

COMMENT ON TABLE  PARKING_MANEUVER_STEPS             IS 'Step-by-step Arduino sensor-guided maneuver definitions';
COMMENT ON COLUMN PARKING_MANEUVER_STEPS.sensor_condition IS 'Arduino sensor trigger condition to advance to next step';
COMMENT ON COLUMN PARKING_MANEUVER_STEPS.is_stop_required IS '1 = student must stop before advancing';

------------------------------------------------------------
-- TABLE: PARKING_EXAM_RESULTS
-- Linked to EXAM_REQUEST when EXAM_STEP = 3.
------------------------------------------------------------
CREATE TABLE PARKING_EXAM_RESULTS (
    result_id         NUMBER(10)   PRIMARY KEY,
    student_id        NUMBER(10)   NOT NULL,
    exam_request_id   NUMBER(10),
    exam_date         TIMESTAMP    DEFAULT SYSTIMESTAMP,
    maneuver_type     VARCHAR2(30) NOT NULL,
    duration_seconds  NUMBER(10),
    is_successful     NUMBER(1)    DEFAULT 0,
    score_out_of_20   NUMBER(5,2)  DEFAULT 0,
    error_count       NUMBER(5)    DEFAULT 0,
    eliminatory_fault VARCHAR2(500),
    notes             CLOB,
    CONSTRAINT fk_per_student  FOREIGN KEY (student_id)      REFERENCES STUDENTS(id),
    CONSTRAINT fk_per_exam_req FOREIGN KEY (exam_request_id) REFERENCES EXAM_REQUEST(REQUEST_ID),
    CONSTRAINT chk_per_maneuver  CHECK (maneuver_type  IN ('creneau','bataille','epi','marche_arriere','demi_tour')),
    CONSTRAINT chk_per_success   CHECK (is_successful  IN (0, 1)),
    CONSTRAINT chk_per_score     CHECK (score_out_of_20 BETWEEN 0 AND 20)
);

CREATE OR REPLACE TRIGGER trg_parking_exam_result_id
BEFORE INSERT ON PARKING_EXAM_RESULTS
FOR EACH ROW
BEGIN
    IF :NEW.result_id IS NULL THEN
        SELECT seq_parking_exam_result_id.NEXTVAL INTO :NEW.result_id FROM dual;
    END IF;
END;
/

COMMENT ON TABLE  PARKING_EXAM_RESULTS                  IS 'Parking exam results with score/20 and eliminatory faults';
COMMENT ON COLUMN PARKING_EXAM_RESULTS.exam_request_id  IS 'FK -> EXAM_REQUEST (EXAM_STEP=3) — links result to booking';

CREATE INDEX idx_per_student  ON PARKING_EXAM_RESULTS(student_id);
CREATE INDEX idx_per_exam_req ON PARKING_EXAM_RESULTS(exam_request_id);
CREATE INDEX idx_per_date     ON PARKING_EXAM_RESULTS(exam_date);
CREATE INDEX idx_per_maneuver ON PARKING_EXAM_RESULTS(maneuver_type);

------------------------------------------------------------
-- TABLE: PARKING_STUDENT_STATS
------------------------------------------------------------
CREATE TABLE PARKING_STUDENT_STATS (
    stat_id              NUMBER(10)   PRIMARY KEY,
    student_id           NUMBER(10)   NOT NULL UNIQUE,
    total_sessions       NUMBER(10)   DEFAULT 0,
    total_successful     NUMBER(10)   DEFAULT 0,
    total_failed         NUMBER(10)   DEFAULT 0,
    best_time_creneau    NUMBER(10),
    best_time_bataille   NUMBER(10),
    best_time_epi        NUMBER(10),
    best_time_marche_arr NUMBER(10),
    best_time_demi_tour  NUMBER(10),
    success_rate         NUMBER(5,2)  DEFAULT 0,
    last_session_date    TIMESTAMP,
    last_updated         DATE         DEFAULT SYSDATE,
    CONSTRAINT fk_pss_student FOREIGN KEY (student_id) REFERENCES STUDENTS(id),
    CONSTRAINT chk_pss_rate   CHECK (success_rate BETWEEN 0 AND 100)
);

CREATE OR REPLACE TRIGGER trg_parking_student_stat_id
BEFORE INSERT ON PARKING_STUDENT_STATS
FOR EACH ROW
BEGIN
    IF :NEW.stat_id IS NULL THEN
        SELECT seq_parking_student_stat_id.NEXTVAL INTO :NEW.stat_id FROM dual;
    END IF;
END;
/

COMMENT ON TABLE PARKING_STUDENT_STATS IS 'Aggregated parking statistics per student — (1,1) <-> (1,1) with STUDENTS';

CREATE INDEX idx_pss_student ON PARKING_STUDENT_STATS(student_id);

-- #####################################################
-- #                                                   #
-- #   SECTION 7: DEFAULT DATA                         #
-- #                                                   #
-- #####################################################

-- Analysis algorithm settings
INSERT INTO ANALYSIS_SETTINGS VALUES ('DEFAULT_ROAD_TYPE',   'neighborhood', 'STRING',  'Default road type',          SYSDATE);
INSERT INTO ANALYSIS_SETTINGS VALUES ('DEFAULT_WEATHER',     'sunny',        'STRING',  'Default weather condition',  SYSDATE);
INSERT INTO ANALYSIS_SETTINGS VALUES ('STRESS_THRESHOLD',    '0.6',          'NUMBER',  'Stress warning threshold',   SYSDATE);
INSERT INTO ANALYSIS_SETTINGS VALUES ('RISK_THRESHOLD',      '0.6',          'NUMBER',  'Risk warning threshold',     SYSDATE);
INSERT INTO ANALYSIS_SETTINGS VALUES ('ML_MODEL_ENABLED',    'true',         'BOOLEAN', 'Enable ML predictions',      SYSDATE);
INSERT INTO ANALYSIS_SETTINGS VALUES ('ACCEL_THRESHOLD',     '15.0',         'NUMBER',  'Harsh accel threshold m/s²', SYSDATE);
INSERT INTO ANALYSIS_SETTINGS VALUES ('GYRO_THRESHOLD',      '0.5',          'NUMBER',  'Gyro threshold rad/s',       SYSDATE);
INSERT INTO ANALYSIS_SETTINGS VALUES ('PROXIMITY_THRESHOLD', '10.0',         'NUMBER',  'Proximity threshold cm',     SYSDATE);

-- Arduino-equipped parking cars (formerly PARKING_VEHICLES)
-- These rows require a valid driving_school_id; insert after your school row.
-- Example: school id = 1
INSERT INTO CARS (brand, model, year, plate_number, transmission, is_active, is_parking_vehicle, assistance_type, sensor_count, serial_port, baud_rate, session_fee, driving_school_id)
VALUES ('Renault','Clio V',         2022,'215 TU 8842',1,'Manuel',  1, 1,'Aide creneau + Camera recul',       5,'COM3',9600,15.0, 1);

INSERT INTO CARS (brand, model, year, plate_number, transmission, is_active, is_parking_vehicle, assistance_type, sensor_count, serial_port, baud_rate, session_fee, driving_school_id)
VALUES ('Dacia',  'Sandero Stepway',2021,'220 TU 1123',1,'Manuel',  1, 1,'Radar recul + Aide parking',        5,'COM4',9600,12.0, 1);

INSERT INTO CARS (brand, model, year, plate_number, transmission, is_active, is_parking_vehicle, assistance_type, sensor_count, serial_port, baud_rate, session_fee, driving_school_id)
VALUES ('Peugeot','308',            2023,'221 TU 3344',1,'Automatique',0, 1,'Park Assist + Camera 360',        5,'COM5',9600,20.0, 1);

INSERT INTO CARS (brand, model, year, plate_number, transmission, is_active, is_parking_vehicle, assistance_type, sensor_count, serial_port, baud_rate, session_fee, driving_school_id)
VALUES ('Citroen','C3',             2022,'219 TU 7788',1,'Manuel',  1, 1,'Camera recul + Aide creneau',       5,'COM6',9600,13.0, 1);

-- Parking maneuver steps: Creneau (10 steps)
INSERT INTO PARKING_MANEUVER_STEPS (maneuver_type,step_number,step_name,sensor_condition,guidance_message,delay_before_ms,is_stop_required)
VALUES ('creneau',1,'Demarrage','demarrage','La voiture demarre. Avancez doucement en marche arriere.',0,0);
INSERT INTO PARKING_MANEUVER_STEPS (maneuver_type,step_number,step_name,sensor_condition,guidance_message,delay_before_ms,is_stop_required)
VALUES ('creneau',2,'Obstacle a droite','capteur_droit=obstacle','Tournez le volant un peu a gauche.',0,0);
INSERT INTO PARKING_MANEUVER_STEPS (maneuver_type,step_number,step_name,sensor_condition,guidance_message,delay_before_ms,is_stop_required)
VALUES ('creneau',3,'Obstacle a gauche','capteur_gauche=obstacle','Tournez le volant un peu a droite.',0,0);
INSERT INTO PARKING_MANEUVER_STEPS (maneuver_type,step_number,step_name,sensor_condition,guidance_message,delay_before_ms,is_stop_required)
VALUES ('creneau',4,'Capteur gauche vers capteur 1','capteur_gauche=capteur1','Tournez completement le volant a gauche.',0,0);
INSERT INTO PARKING_MANEUVER_STEPS (maneuver_type,step_number,step_name,sensor_condition,guidance_message,delay_before_ms,is_stop_required)
VALUES ('creneau',5,'Capteur droit vers capteur 2','capteur_droit=capteur2','Stop ! Arretez-vous.',0,1);
INSERT INTO PARKING_MANEUVER_STEPS (maneuver_type,step_number,step_name,sensor_condition,guidance_message,delay_before_ms,is_stop_required)
VALUES ('creneau',6,'Apres 5s - Braquer droite','delai_5s_apres_stop','Tournez completement le volant a droite.',5000,0);
INSERT INTO PARKING_MANEUVER_STEPS (maneuver_type,step_number,step_name,sensor_condition,guidance_message,delay_before_ms,is_stop_required)
VALUES ('creneau',7,'Capteur arriere droit','capteur_arriere_droit=capteur','Stop ! Arretez-vous.',0,1);
INSERT INTO PARKING_MANEUVER_STEPS (maneuver_type,step_number,step_name,sensor_condition,guidance_message,delay_before_ms,is_stop_required)
VALUES ('creneau',8,'Apres 3s - Braquer gauche','delai_3s_apres_stop','Tournez completement le volant a gauche.',3000,0);
INSERT INTO PARKING_MANEUVER_STEPS (maneuver_type,step_number,step_name,sensor_condition,guidance_message,delay_before_ms,is_stop_required)
VALUES ('creneau',9,'Deux capteurs arriere','capteur_arr_gauche=obstacle AND capteur_arr_droit=obstacle','Stop ! Obstacle detecte a l''arriere des deux cotes.',0,1);
INSERT INTO PARKING_MANEUVER_STEPS (maneuver_type,step_number,step_name,sensor_condition,guidance_message,delay_before_ms,is_stop_required)
VALUES ('creneau',10,'Phase finale','capteur_arr_gauche=capteur AND capteur_arr_droit=capteur','Stop ! Felicitations, vous avez reussi votre examen !',0,1);

COMMIT;

-- #####################################################
-- #                                                   #
-- #   SECTION 8: VIEWS                                #
-- #                                                   #
-- #####################################################

CREATE OR REPLACE VIEW V_STUDENT_PERFORMANCE_OVERVIEW AS
SELECT
    s.id AS student_id,
    s.first_name || ' ' || s.last_name AS student_name,
    s.email, s.phone, s.student_status,
    COUNT(DISTINCT ds.driving_session_id) AS total_sessions,
    ROUND(AVG(sa.stress_index) * 100, 1)                    AS avg_stress_pct,
    ROUND(AVG(sa.risk_score)   * 100, 1)                    AS avg_risk_pct,
    ROUND(AVG((sa.stress_index + sa.risk_score)/2)*100, 1)  AS avg_combined_score,
    MAX(ds.session_date)                                     AS last_session_date
FROM STUDENTS s
LEFT JOIN DRIVING_SESSIONS   ds ON s.id = ds.student_id
LEFT JOIN SESSION_ANALYSIS   sa ON ds.driving_session_id = sa.driving_session_id
GROUP BY s.id, s.first_name, s.last_name, s.email, s.phone, s.student_status;

CREATE OR REPLACE VIEW V_STUDENT_LATEST_SESSION AS
SELECT
    s.id AS student_id,
    s.first_name || ' ' || s.last_name AS student_name,
    ds.driving_session_id, ds.session_date, ds.road_type, ds.weather, ds.duration,
    sa.stress_index, ROUND(sa.stress_index*100,1) AS stress_pct,
    sa.risk_score,   ROUND(sa.risk_score*100,1)   AS risk_pct,
    sa.overall_performance, sa.ml_prediction, sa.ml_confidence, sa.ml_risk_level
FROM STUDENTS s
LEFT JOIN DRIVING_SESSIONS ds ON s.id = ds.student_id
LEFT JOIN SESSION_ANALYSIS sa ON ds.driving_session_id = sa.driving_session_id
WHERE ds.session_date = (SELECT MAX(session_date) FROM DRIVING_SESSIONS WHERE student_id = s.id);

CREATE OR REPLACE VIEW V_SESSION_COMPARISON AS
SELECT
    ds.driving_session_id, ds.booking_id, ds.session_date,
    TO_CHAR(ds.session_date,'YYYY-MM-DD HH24:MI') AS formatted_date,
    s.first_name || ' ' || s.last_name AS student_name,
    i.full_name AS instructor_name,
    ds.road_type, ds.weather, ds.duration,
    ROUND(ds.duration/60,1) AS duration_minutes,
    sa.stress_index, ROUND(sa.stress_index*100,1) AS stress_pct,
    sa.risk_score,   ROUND(sa.risk_score*100,1)   AS risk_pct,
    sa.overall_performance, sa.ml_prediction,
    sa.ml_confidence, ROUND(sa.ml_confidence*100,0) AS ml_confidence_pct,
    sa.ml_risk_level
FROM DRIVING_SESSIONS ds
JOIN STUDENTS         s  ON ds.student_id         = s.id
JOIN INSTRUCTORS      i  ON ds.instructor_id       = i.id
LEFT JOIN SESSION_ANALYSIS sa ON ds.driving_session_id = sa.driving_session_id
ORDER BY ds.session_date DESC;

CREATE OR REPLACE VIEW V_PERFORMANCE_TREND AS
SELECT
    sph.student_id,
    s.first_name || ' ' || s.last_name AS student_name,
    sph.session_number,
    sph.stress_index, ROUND(sph.stress_index*100,1) AS stress_pct,
    sph.risk_score,   ROUND(sph.risk_score*100,1)   AS risk_pct,
    ROUND((sph.stress_index+sph.risk_score)/2*100,1) AS combined_score,
    sph.improvement, sph.tracking_date,
    TO_CHAR(sph.tracking_date,'YYYY-MM-DD') AS formatted_date
FROM SESSION_PERFORMANCE_HISTORY sph
JOIN STUDENTS s ON sph.student_id = s.id
ORDER BY sph.student_id, sph.session_number;

CREATE OR REPLACE VIEW V_RECOMMENDATIONS_SUMMARY AS
SELECT
    r.recommendation_id, r.driving_session_id, ds.session_date,
    s.first_name || ' ' || s.last_name AS student_name,
    r.priority, r.category, r.message, r.suggestion, r.created_date
FROM RECOMMENDATIONS r
JOIN DRIVING_SESSIONS ds ON r.driving_session_id = ds.driving_session_id
JOIN STUDENTS         s  ON ds.student_id = s.id
ORDER BY ds.session_date DESC,
    CASE r.priority WHEN 'CRITICAL' THEN 1 WHEN 'HIGH' THEN 2 WHEN 'MEDIUM' THEN 3 ELSE 4 END;

CREATE OR REPLACE VIEW V_PARKING_STUDENT_SUMMARY AS
SELECT
    s.id AS student_id,
    s.first_name || ' ' || s.last_name AS student_name,
    s.email, s.phone,
    COUNT(ps.session_id) AS total_sessions,
    SUM(CASE WHEN ps.is_successful = 1 THEN 1 ELSE 0 END) AS total_successful,
    CASE WHEN COUNT(ps.session_id) > 0
         THEN ROUND(100.0 * SUM(CASE WHEN ps.is_successful=1 THEN 1 ELSE 0 END)/COUNT(ps.session_id),1)
         ELSE 0 END AS success_rate,
    NVL(SUM(ps.duration_seconds),0) AS total_seconds,
    ROUND(NVL(SUM(ps.duration_seconds),0)/3600.0,1) AS total_hours,
    NVL(SUM(c.session_fee),0) AS total_spent,
    CASE WHEN NVL(SUM(ps.duration_seconds),0) >= 28800 THEN 'YES' ELSE 'NO' END AS eligible_by_hours,
    MAX(ps.session_start) AS last_session_date
FROM STUDENTS s
LEFT JOIN PARKING_SESSIONS ps ON s.id = ps.student_id
LEFT JOIN CARS c              ON ps.car_id = c.id
GROUP BY s.id, s.first_name, s.last_name, s.email, s.phone;

CREATE OR REPLACE VIEW V_PARKING_PERFECT_RUNS AS
SELECT
    ps.student_id,
    s.first_name || ' ' || s.last_name AS student_name,
    ps.maneuver_type,
    COUNT(*) AS perfect_count
FROM PARKING_SESSIONS ps
JOIN STUDENTS s ON ps.student_id = s.id
WHERE ps.is_successful = 1
  AND ps.error_count = 0 AND ps.stall_count = 0 AND ps.obstacle_contact = 0
GROUP BY ps.student_id, s.first_name, s.last_name, ps.maneuver_type;

CREATE OR REPLACE VIEW V_PARKING_EXAM_ELIGIBILITY AS
SELECT
    r.student_id, r.student_name, r.total_sessions, r.success_rate,
    r.total_hours, r.total_spent, r.eligible_by_hours,
    NVL(p_cr.perfect_count,0) AS perfect_creneau,
    NVL(p_ba.perfect_count,0) AS perfect_bataille,
    NVL(p_ep.perfect_count,0) AS perfect_epi,
    NVL(p_ma.perfect_count,0) AS perfect_marche_arriere,
    NVL(p_dt.perfect_count,0) AS perfect_demi_tour,
    CASE
        WHEN r.eligible_by_hours = 'YES' THEN 'ELIGIBLE'
        WHEN NVL(p_cr.perfect_count,0)>=10 AND NVL(p_ba.perfect_count,0)>=10
         AND NVL(p_ep.perfect_count,0)>=10 AND NVL(p_ma.perfect_count,0)>=10
         AND NVL(p_dt.perfect_count,0)>=10 THEN 'ELIGIBLE'
        ELSE 'NOT YET'
    END AS eligibility_status
FROM V_PARKING_STUDENT_SUMMARY r
LEFT JOIN V_PARKING_PERFECT_RUNS p_cr ON r.student_id=p_cr.student_id AND p_cr.maneuver_type='creneau'
LEFT JOIN V_PARKING_PERFECT_RUNS p_ba ON r.student_id=p_ba.student_id AND p_ba.maneuver_type='bataille'
LEFT JOIN V_PARKING_PERFECT_RUNS p_ep ON r.student_id=p_ep.student_id AND p_ep.maneuver_type='epi'
LEFT JOIN V_PARKING_PERFECT_RUNS p_ma ON r.student_id=p_ma.student_id AND p_ma.maneuver_type='marche_arriere'
LEFT JOIN V_PARKING_PERFECT_RUNS p_dt ON r.student_id=p_dt.student_id AND p_dt.maneuver_type='demi_tour';

CREATE OR REPLACE VIEW V_PARKING_SESSION_DETAIL AS
SELECT
    ps.session_id,
    s.first_name || ' ' || s.last_name AS student_name,
    c.model AS vehicle_model, c.plate_number,
    ps.maneuver_type, ps.session_start,
    ps.duration_seconds, ROUND(ps.duration_seconds/60.0,1) AS duration_minutes,
    ps.is_exam_mode, ps.is_successful, ps.steps_completed,
    ps.error_count, ps.stall_count, ps.obstacle_contact,
    (SELECT COUNT(*)      FROM PARKING_SENSOR_LOGS psl WHERE psl.session_id=ps.session_id) AS total_readings,
    (SELECT COUNT(*)      FROM PARKING_SENSOR_LOGS psl WHERE psl.session_id=ps.session_id AND psl.event_type='CALAGE')            AS stall_events,
    (SELECT COUNT(*)      FROM PARKING_SENSOR_LOGS psl WHERE psl.session_id=ps.session_id AND psl.event_type='CONTACT_OBSTACLE')  AS contact_events,
    (SELECT COUNT(*)      FROM PARKING_SENSOR_LOGS psl WHERE psl.session_id=ps.session_id AND psl.event_type='STEP_ADVANCE')      AS step_advance_events
FROM PARKING_SESSIONS ps
JOIN STUDENTS s ON ps.student_id = s.id
JOIN CARS     c ON ps.car_id     = c.id
ORDER BY ps.session_start DESC;

CREATE OR REPLACE VIEW V_PARKING_MANEUVER_PROGRESS AS
SELECT
    s.id AS student_id,
    s.first_name || ' ' || s.last_name AS student_name,
    ps.maneuver_type,
    COUNT(ps.session_id) AS total_attempts,
    SUM(CASE WHEN ps.is_successful=1 THEN 1 ELSE 0 END) AS successful_attempts,
    CASE WHEN COUNT(ps.session_id)>0
         THEN ROUND(100.0*SUM(CASE WHEN ps.is_successful=1 THEN 1 ELSE 0 END)/COUNT(ps.session_id),1)
         ELSE 0 END AS maneuver_success_rate,
    MIN(CASE WHEN ps.is_successful=1 THEN ps.duration_seconds ELSE NULL END) AS best_time_seconds,
    ROUND(AVG(ps.duration_seconds),0) AS avg_time_seconds,
    SUM(ps.error_count)  AS total_errors,
    SUM(ps.stall_count)  AS total_stalls,
    MAX(ps.session_start) AS last_attempt_date
FROM STUDENTS s
JOIN PARKING_SESSIONS ps ON s.id = ps.student_id
GROUP BY s.id, s.first_name, s.last_name, ps.maneuver_type
ORDER BY s.id, ps.maneuver_type;

CREATE OR REPLACE VIEW V_PAYMENT_SUMMARY AS
SELECT
    p.payment_id, p.facture_number, p.facture_date,
    s.first_name || ' ' || s.last_name AS student_name,
    p.amount, p.payment_method, p.status,
    p.d17_code, p.d17_phone,
    CASE WHEN p.session_id IS NOT NULL      THEN 'SESSION_BOOKING'
         WHEN p.exam_request_id IS NOT NULL THEN 'EXAM_REQUEST'
         ELSE 'DIRECT'
    END AS payment_for,
    NVL(TO_CHAR(p.session_id), TO_CHAR(p.exam_request_id)) AS reference_id
FROM PAYMENT p
JOIN STUDENTS s ON p.student_id = s.id
ORDER BY p.facture_date DESC;

-- #####################################################
-- #                                                   #
-- #   SECTION 9: STORED PROCEDURES                    #
-- #                                                   #
-- #####################################################

CREATE OR REPLACE PROCEDURE calc_student_improvement(
    p_student_id IN NUMBER, p_improvement OUT NUMBER
) AS
    v_count NUMBER; v_early_avg NUMBER; v_recent_avg NUMBER; v_compare_count NUMBER;
BEGIN
    SELECT COUNT(*) INTO v_count FROM SESSION_PERFORMANCE_HISTORY WHERE student_id = p_student_id;
    IF v_count < 2 THEN p_improvement := 0; RETURN; END IF;
    v_compare_count := CASE WHEN v_count >= 6 THEN 3 ELSE 1 END;
    SELECT AVG((stress_index+risk_score)/2) INTO v_early_avg
    FROM (SELECT stress_index,risk_score FROM SESSION_PERFORMANCE_HISTORY WHERE student_id=p_student_id ORDER BY session_number)
    WHERE ROWNUM <= v_compare_count;
    SELECT AVG((stress_index+risk_score)/2) INTO v_recent_avg
    FROM (SELECT stress_index,risk_score FROM SESSION_PERFORMANCE_HISTORY WHERE student_id=p_student_id ORDER BY session_number DESC)
    WHERE ROWNUM <= v_compare_count;
    IF v_early_avg > 0 THEN p_improvement := ((v_early_avg - v_recent_avg)/v_early_avg)*100;
    ELSE p_improvement := 0; END IF;
EXCEPTION WHEN OTHERS THEN p_improvement := 0;
END;
/

CREATE OR REPLACE PROCEDURE generate_recommendations(p_driving_session_id IN NUMBER) AS
    v_stress NUMBER; v_risk NUMBER; v_harsh NUMBER; v_proximity NUMBER; v_turns NUMBER;
BEGIN
    SELECT sa.stress_index,sa.risk_score,sa.harsh_accel_count,sa.proximity_events,sa.sharp_turn_count
    INTO v_stress,v_risk,v_harsh,v_proximity,v_turns
    FROM SESSION_ANALYSIS sa WHERE sa.driving_session_id = p_driving_session_id;
    IF v_risk > 0.7 THEN
        INSERT INTO RECOMMENDATIONS VALUES (seq_recommendation_id.NEXTVAL,p_driving_session_id,'CRITICAL','Safety',
            'Critical risk level ('||ROUND(v_risk*100,1)||'%)','Mandatory safety review — focus on defensive driving.',SYSDATE);
    ELSIF v_risk > 0.6 THEN
        INSERT INTO RECOMMENDATIONS VALUES (seq_recommendation_id.NEXTVAL,p_driving_session_id,'HIGH','Safety',
            'High risk score ('||ROUND(v_risk*100,1)||'%)','Additional safety training required.',SYSDATE);
    END IF;
    IF v_harsh > 15 THEN
        INSERT INTO RECOMMENDATIONS VALUES (seq_recommendation_id.NEXTVAL,p_driving_session_id,'HIGH','Technique',
            'Excessive harsh maneuvers ('||v_harsh||')','Practice smooth acceleration and braking.',SYSDATE);
    END IF;
    IF v_proximity > 50 THEN
        INSERT INTO RECOMMENDATIONS VALUES (seq_recommendation_id.NEXTVAL,p_driving_session_id,'HIGH','Spatial Awareness',
            'Frequent proximity warnings ('||v_proximity||')','Improve spatial judgment and parking skills.',SYSDATE);
    END IF;
    IF v_stress < 0.3 AND v_risk < 0.3 THEN
        INSERT INTO RECOMMENDATIONS VALUES (seq_recommendation_id.NEXTVAL,p_driving_session_id,'LOW','Performance',
            'Excellent performance','Ready for more challenging road scenarios.',SYSDATE);
    END IF;
    COMMIT;
EXCEPTION WHEN NO_DATA_FOUND THEN NULL; WHEN OTHERS THEN ROLLBACK;
END;
/

CREATE OR REPLACE PROCEDURE add_performance_entry(p_student_id IN NUMBER, p_driving_session_id IN NUMBER) AS
    v_session_number NUMBER; v_stress NUMBER; v_risk NUMBER; v_improvement NUMBER := 0;
BEGIN
    SELECT NVL(MAX(session_number),0)+1 INTO v_session_number FROM SESSION_PERFORMANCE_HISTORY WHERE student_id=p_student_id;
    SELECT stress_index,risk_score INTO v_stress,v_risk FROM SESSION_ANALYSIS WHERE driving_session_id=p_driving_session_id;
    IF v_session_number > 1 THEN calc_student_improvement(p_student_id, v_improvement); END IF;
    INSERT INTO SESSION_PERFORMANCE_HISTORY VALUES (seq_session_performance_id.NEXTVAL,p_student_id,p_driving_session_id,v_session_number,v_stress,v_risk,v_improvement,SYSDATE);
    COMMIT;
EXCEPTION WHEN OTHERS THEN ROLLBACK;
END;
/

CREATE OR REPLACE PROCEDURE refresh_parking_stats(p_student_id IN NUMBER) AS
    v_total NUMBER:=0; v_successful NUMBER:=0; v_failed NUMBER:=0; v_rate NUMBER:=0;
    v_best_cr NUMBER; v_best_ba NUMBER; v_best_ep NUMBER; v_best_ma NUMBER; v_best_dt NUMBER;
    v_last_date TIMESTAMP; v_exists NUMBER:=0;
BEGIN
    SELECT COUNT(*), SUM(CASE WHEN is_successful=1 THEN 1 ELSE 0 END),
           SUM(CASE WHEN is_successful=0 THEN 1 ELSE 0 END), MAX(session_start)
    INTO v_total,v_successful,v_failed,v_last_date
    FROM PARKING_SESSIONS WHERE student_id=p_student_id;
    IF v_total>0 THEN v_rate:=ROUND((v_successful/v_total)*100,2); END IF;
    SELECT MIN(CASE WHEN maneuver_type='creneau'       THEN duration_seconds END),
           MIN(CASE WHEN maneuver_type='bataille'      THEN duration_seconds END),
           MIN(CASE WHEN maneuver_type='epi'           THEN duration_seconds END),
           MIN(CASE WHEN maneuver_type='marche_arriere'THEN duration_seconds END),
           MIN(CASE WHEN maneuver_type='demi_tour'     THEN duration_seconds END)
    INTO v_best_cr,v_best_ba,v_best_ep,v_best_ma,v_best_dt
    FROM PARKING_SESSIONS WHERE student_id=p_student_id AND is_successful=1;
    SELECT COUNT(*) INTO v_exists FROM PARKING_STUDENT_STATS WHERE student_id=p_student_id;
    IF v_exists=0 THEN
        INSERT INTO PARKING_STUDENT_STATS (stat_id,student_id,total_sessions,total_successful,total_failed,
            best_time_creneau,best_time_bataille,best_time_epi,best_time_marche_arr,best_time_demi_tour,
            success_rate,last_session_date,last_updated)
        VALUES (seq_parking_student_stat_id.NEXTVAL,p_student_id,v_total,v_successful,v_failed,
            v_best_cr,v_best_ba,v_best_ep,v_best_ma,v_best_dt,v_rate,v_last_date,SYSDATE);
    ELSE
        UPDATE PARKING_STUDENT_STATS SET total_sessions=v_total,total_successful=v_successful,total_failed=v_failed,
            best_time_creneau=v_best_cr,best_time_bataille=v_best_ba,best_time_epi=v_best_ep,
            best_time_marche_arr=v_best_ma,best_time_demi_tour=v_best_dt,
            success_rate=v_rate,last_session_date=v_last_date,last_updated=SYSDATE
        WHERE student_id=p_student_id;
    END IF;
    COMMIT;
EXCEPTION WHEN OTHERS THEN ROLLBACK;
END;
/

CREATE OR REPLACE PROCEDURE update_parking_progress(p_student_id IN NUMBER, p_passed IN NUMBER, p_score IN NUMBER) AS
BEGIN
    IF p_passed=1 THEN
        UPDATE USER_PROGRESS SET parking_status='PASSED', parking_score=p_score,
            parking_passed_date=SYSTIMESTAMP,
            is_qualified=CASE WHEN code_status='PASSED' AND circuit_status='PASSED' THEN 1 ELSE is_qualified END
        WHERE user_id=p_student_id;
    ELSE
        UPDATE USER_PROGRESS SET parking_score=p_score WHERE user_id=p_student_id;
    END IF;
    COMMIT;
EXCEPTION WHEN OTHERS THEN ROLLBACK;
END;
/

-- #####################################################
-- #                                                   #
-- #   SECTION 10: VERIFICATION & SUMMARY              #
-- #                                                   #
-- #####################################################

SELECT table_name    FROM user_tables    ORDER BY table_name;
SELECT sequence_name FROM user_sequences ORDER BY sequence_name;
SELECT view_name     FROM user_views     ORDER BY view_name;
SELECT object_name   FROM user_objects   WHERE object_type='PROCEDURE' ORDER BY object_name;

SELECT 'ANALYSIS_SETTINGS'    AS tbl, COUNT(*) AS cnt FROM ANALYSIS_SETTINGS
UNION ALL SELECT 'CARS (parking only)', COUNT(*) FROM CARS WHERE is_parking_vehicle=1
UNION ALL SELECT 'PARKING_MANEUVER_STEPS', COUNT(*) FROM PARKING_MANEUVER_STEPS;

SELECT
    'COMPLETE INTEGRATED DATABASE READY!' AS status,
    (SELECT COUNT(*) FROM user_tables)                                     AS total_tables,
    (SELECT COUNT(*) FROM user_sequences)                                  AS total_sequences,
    (SELECT COUNT(*) FROM user_views)                                      AS total_views,
    (SELECT COUNT(*) FROM user_objects WHERE object_type='PROCEDURE')      AS total_procedures
FROM DUAL;

-- =====================================================
-- TABLE INVENTORY
-- =====================================================
-- CORE:     ADMIN, DRIVING_SCHOOLS, CARS, STUDENTS,
--           ADMIN_INSTRUCTORS, INSTRUCTORS
-- LEARNING: USER_PROGRESS, QUIZ_RESULTS, BADGES, COURSE_LINKS
-- BOOKING:  SESSION_BOOKING, AVAILABILITY, EXAM_REQUEST
-- PAYMENT:  PAYMENT
-- SENSOR:   DRIVING_SESSIONS, SENSOR_DATA, SESSION_ANALYSIS,
--           RECOMMENDATIONS, SESSION_PERFORMANCE_HISTORY, ANALYSIS_SETTINGS
-- PARKING:  PARKING_SESSIONS, PARKING_SENSOR_LOGS,
--           PARKING_MANEUVER_STEPS, PARKING_EXAM_RESULTS,
--           PARKING_STUDENT_STATS
-- =====================================================
-- TOTAL: 26 Tables | 23 Sequences | 11 Views | 5 Procedures
-- =====================================================
-- END OF COMPLETE INTEGRATED SCHEMA
-- =====================================================
