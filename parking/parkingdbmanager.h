#ifndef PARKING_DATABASEMANAGER_H
#define PARKING_DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariantMap>
#include <QList>
#include <QDateTime>

class ParkingDBManager : public QObject
{
    Q_OBJECT
public:
    static ParkingDBManager& instance();
    bool initialize(const QString &dsn = "Source_Projet2A",
                    const QString &user = "bourhen",
                    const QString &password = "esprit18");
    void close();
    bool isConnected() const;

    // ── Moniteurs (Instructors) ──
    int addMoniteur(const QString &nom, const QString &prenom,
                    const QString &tel = "", const QString &email = "");
    bool updateMoniteur(int id, const QString &nom, const QString &prenom,
                        const QString &tel, const QString &email);
    bool deleteMoniteur(int id);
    QVariantMap getMoniteur(int id);
    QList<QVariantMap> getAllMoniteurs();

    // ── Élèves ──
    int addEleve(const QString &nom, const QString &prenom,
                 const QString &tel = "", const QString &email = "",
                 const QString &numPermis = "");
    bool updateEleve(int id, const QString &nom, const QString &prenom,
                     const QString &tel, const QString &email,
                     const QString &numPermis);
    bool deleteEleve(int id);
    QVariantMap getEleve(int id);
    QList<QVariantMap> getAllEleves();

    // ── Véhicules ──
    int addVehicule(const QString &immat, const QString &modele,
                    const QString &typeAssist, const QString &portSerie,
                    int baudRate = 9600);
    bool updateVehicule(int id, const QString &immat, const QString &modele,
                        const QString &typeAssist, const QString &portSerie,
                        int baudRate, double tarif);
    bool deleteVehicule(int id);
    QVariantMap getVehicule(int id);
    QVariantMap getVehiculeByImmat(const QString &immat);
    QList<QVariantMap> getVehiculesDisponibles();
    void setVehiculeDisponible(int id, bool dispo);

    // ── Sessions ──
    int startSession(int eleveId, int vehiculeId, const QString &manoeuvreType,
                     bool modeExamen = false);
    void endSession(int sessionId, bool reussi, int duree, int etapes,
                    int erreurs = 0, int calages = 0, bool contactObstacle = false);
    bool deleteSession(int sessionId);
    QVariantMap getSession(int id);
    QList<QVariantMap> getSessionsEleve(int eleveId, int limit = 20);

    // ── Sensor Logs ──
    void logSensorEvent(int sessionId, int avant, int gauche, int droit,
                        int arrGauche, int arrDroit,
                        double distAvant = -1, double distGauche = -1,
                        double distDroit = -1, double distArrG = -1, double distArrD = -1,
                        const QString &evenement = "", const QString &message = "",
                        int etape = 0);

    // ── Étapes manœuvre ──
    QList<QVariantMap> getEtapesManoeuvre(const QString &manoeuvreType);

    // ── Résultats examen ──
    int saveResultatExamen(int eleveId, const QString &manoeuvreType,
                           int duree, bool reussi, double score,
                           int erreurs = 0, const QString &fauteElim = "",
                           const QString &commentaire = "");
    QList<QVariantMap> getResultatsEleve(int eleveId, int limit = 10);

    // ── Statistiques ──
    void updateStatistiquesEleve(int eleveId);
    QVariantMap getStatistiquesEleve(int eleveId);

    // ── Utilitaires ──
    int getSessionCount(int eleveId);
    int getSuccessCount(int eleveId);
    double getTauxReussite(int eleveId);
    int getMeilleurTemps(int eleveId, const QString &manoeuvre);

    // ── Tarifs & Finances ──
    double getVehiculeTarif(int vehiculeId);
    double getTotalDepense(int eleveId);

    // ── Éligibilité Examen ──
    int getTotalPracticeSeconds(int eleveId);
    int getPerfectRunsCount(int eleveId, const QString &manoeuvreType);
    bool isExamReady(int eleveId, const QStringList &manoeuvreTypes);

    // ── Réservations Examen ──
    int addReservationExamen(int eleveId, int instructeurId, int vehiculeId,
                             const QString &dateExamen, const QString &creneau,
                             double montant = 0);
    QList<QVariantMap> getReservationsEleve(int eleveId);
    void updateReservationStatut(int reservationId, const QString &statut);
    bool deleteReservation(int reservationId);

    // ── Exam Slots (instructor-created available slots) ──
    int  addExamSlot(int instructorId, const QString &examDate,
                     const QString &timeSlot, const QString &notes = QString());
    QList<QVariantMap> getAvailableExamSlots();                   // for students
    QList<QVariantMap> getExamSlotsForInstructor(int instructorId); // instructor's own
    bool bookExamSlot(int slotId, int studentId, int vehiculeId); // student books
    bool cancelExamSlot(int slotId);                              // instructor cancels

    // ── Login validation (returns user ID, -1 if not found / DB not connected) ──
    int validateStudentLogin(const QString &fullName);
    int validateMoniteurLogin(const QString &fullName);

    // ── Tutoriels Vidéo (colonne youtube_url dans PARKING_MANEUVER_STEPS) ──
    bool setManeuverVideoUrl(const QString &maneuverType, const QString &url);
    QList<QVariantMap> getAllManeuverVideoUrls();   // retourne {maneuver_type, youtube_url} par manoeuvre

    // ── Maneuver Steps CRUD (PARKING_MANEUVER_STEPS) ──
    QList<QVariantMap> getAllManeuverSteps();
    int  addManeuverStep(const QString &maneuverType, int stepNumber,
                         const QString &stepName, const QString &sensorCondition,
                         const QString &guidanceMessage, const QString &audioMessage,
                         int delayMs, bool isStopRequired);
    bool updateManeuverStep(int stepId, int stepNumber,
                            const QString &stepName, const QString &sensorCondition,
                            const QString &guidanceMessage, const QString &audioMessage,
                            int delayMs, bool isStopRequired);
    bool deleteManeuverStep(int stepId);

    // ── Exam Results admin view (PARKING_EXAM_RESULTS) ──
    QList<QVariantMap> getAllExamResults();
    bool deleteExamResult(int resultId);

    // ── Sessions Examen (EXAM_REQUEST — vue admin complète) ──
    QList<QVariantMap> getAllExamRequests(int instructorId = -1);
    int addExamRequestAdmin(int studentId, int instructorId,
                            const QString &date, const QString &creneau,
                            int carId, double amount);

    QString getLastError() const;
    QSqlDatabase db() const { return m_db; }   // expose connection for direct queries

signals:
    void databaseError(const QString &error);

private:
    explicit ParkingDBManager(QObject *parent = nullptr);
    ~ParkingDBManager();
    ParkingDBManager(const ParkingDBManager&) = delete;
    ParkingDBManager& operator=(const ParkingDBManager&) = delete;

    void createTables();
    void insertDefaultData();
    QSqlDatabase m_db;
};

#endif // PARKING_DATABASEMANAGER_H
