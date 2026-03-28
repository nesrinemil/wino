#ifndef STUDENT_H
#define STUDENT_H

#include <QString>
#include <QDate>

class Student
{
public:
    Student();

    // Getters
    int id() const { return m_id; }
    int drivingSchoolId() const { return m_drivingSchoolId; }
    QString firstName() const { return m_firstName; }
    QString lastName() const { return m_lastName; }
    QDate dateOfBirth() const { return m_dateOfBirth; }
    QString phone() const { return m_phone; }
    QString cin() const { return m_cin; }
    QString email() const { return m_email; }
    QString passwordHash() const { return m_passwordHash; }
    bool hasCin() const { return m_hasCin; }
    bool hasFacePhoto() const { return m_hasFacePhoto; }
    QString medicalCertificatePath() const { return m_medicalCertificatePath; }
    bool isBeginner() const { return m_isBeginner; }
    bool hasDrivingLicense() const { return m_hasDrivingLicense; }
    bool hasCode() const { return m_hasCode; }
    bool hasConduite() const { return m_hasConduite; }
    QString courseLevel() const { return m_courseLevel; }
    bool fingerprintRegistered() const { return m_fingerprintRegistered; }
    QString licenseNumber() const { return m_licenseNumber; }
    QDate enrollmentDate() const { return m_enrollmentDate; }
    QString studentStatus() const { return m_studentStatus; }

    // Setters
    void setId(int id) { m_id = id; }
    void setDrivingSchoolId(int id) { m_drivingSchoolId = id; }
    void setFirstName(const QString &name) { m_firstName = name; }
    void setLastName(const QString &name) { m_lastName = name; }
    void setDateOfBirth(const QDate &date) { m_dateOfBirth = date; }
    void setPhone(const QString &phone) { m_phone = phone; }
    void setCin(const QString &cin) { m_cin = cin; }
    void setEmail(const QString &email) { m_email = email; }
    void setPasswordHash(const QString &hash) { m_passwordHash = hash; }
    void setHasCin(bool has) { m_hasCin = has; }
    void setHasFacePhoto(bool has) { m_hasFacePhoto = has; }
    void setMedicalCertificatePath(const QString &path) { m_medicalCertificatePath = path; }
    void setIsBeginner(bool is) { m_isBeginner = is; }
    void setHasDrivingLicense(bool has) { m_hasDrivingLicense = has; }
    void setHasCode(bool has) { m_hasCode = has; }
    void setHasConduite(bool has) { m_hasConduite = has; }
    void setCourseLevel(const QString &level) { m_courseLevel = level; }
    void setFingerprintRegistered(bool reg) { m_fingerprintRegistered = reg; }
    void setLicenseNumber(const QString &number) { m_licenseNumber = number; }
    void setEnrollmentDate(const QDate &date) { m_enrollmentDate = date; }
    void setStudentStatus(const QString &status) { m_studentStatus = status; }

    // Utility
    QString fullName() const { return m_firstName + " " + m_lastName; }

private:
    int m_id = 0;
    int m_drivingSchoolId = 0;
    QString m_firstName;
    QString m_lastName;
    QDate m_dateOfBirth;
    QString m_phone;
    QString m_cin;
    QString m_email;
    QString m_passwordHash;
    bool m_hasCin = false;
    bool m_hasFacePhoto = false;
    QString m_medicalCertificatePath;
    bool m_isBeginner = true;
    bool m_hasDrivingLicense = false;
    bool m_hasCode = false;
    bool m_hasConduite = false;
    QString m_courseLevel = "Theory";
    bool m_fingerprintRegistered = false;
    QString m_licenseNumber;
    QDate m_enrollmentDate;
    QString m_studentStatus = "ACTIVE";
};

#endif // STUDENT_H
