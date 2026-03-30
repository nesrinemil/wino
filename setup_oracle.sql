-- ============================================================
-- DrivingSchoolApp - Oracle XE 11g Workspace Setup
-- Compatible with Oracle 11g (uses sequences + triggers)
-- ============================================================

-- 1. Drop user if already exists (clean slate)
BEGIN
   EXECUTE IMMEDIATE 'DROP USER smart_driving CASCADE';
EXCEPTION
   WHEN OTHERS THEN NULL;
END;
/

-- 2. Create user / schema
CREATE USER smart_driving IDENTIFIED BY SmartDrive2026
    DEFAULT TABLESPACE USERS
    TEMPORARY TABLESPACE TEMP
    QUOTA UNLIMITED ON USERS;

-- 3. Grant privileges
GRANT CONNECT, RESOURCE TO smart_driving;
GRANT CREATE SESSION TO smart_driving;
GRANT CREATE TABLE TO smart_driving;
GRANT CREATE SEQUENCE TO smart_driving;
GRANT CREATE TRIGGER TO smart_driving;

COMMIT;
EXIT;
