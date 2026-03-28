#include "student.h"

Student::Student()
    : m_id(0)
    , m_drivingSchoolId(0)
    , m_hasCin(false)
    , m_hasFacePhoto(false)
    , m_isBeginner(true)
    , m_hasDrivingLicense(false)
    , m_hasCode(false)
    , m_hasConduite(false)
    , m_courseLevel("Theory")
    , m_fingerprintRegistered(false)
    , m_studentStatus("ACTIVE")
{
    m_enrollmentDate = QDate::currentDate();
}
