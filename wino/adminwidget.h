#ifndef ADMINWIDGET_H
#define ADMINWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QLineEdit>
#include <QFrame>
#include <QStackedWidget>
#include <QTableWidget>
#include <QComboBox>
#include <QDialog>
#include <QMessageBox>
#include <QTimer>
#include <QPropertyAnimation>
#include <QGraphicsDropShadowEffect>
#include <QDateEdit>
#include <QSpinBox>

class AdminWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AdminWidget(const QString &userName, const QString &userRole,
                         QWidget *parent = nullptr);

public slots:
    void refreshAll();

private:
    void setupUI();
    QWidget* createBanner();
    QWidget* createKPIRow();
    QWidget* createNavBar();

    // ── KPI helpers ──
    QFrame* createKPICard(const QString &icon, const QString &label,
                          const QString &gradient, QLabel **valRef, QLabel **subRef);

    // ── Pages ──
    QWidget* createDashboardPage();
    QWidget* createElevesPage();
    QWidget* createVehiculesPage();
    QWidget* createSessionsPage();
    QWidget* createReservationsPage();

    // ── Dashboard ──
    QWidget* createActivityTimeline();
    QWidget* createTopStudentsCard();
    QWidget* createFleetOverview();

    // ── Moniteurs CRUD ──
    QWidget* createMoniteursPage();
    void refreshMoniteursTable();
    void showAddMoniteurDialog();
    void showEditMoniteurDialog(int id);
    void deleteMoniteur(int id);

    // ── Élèves CRUD ──
    void refreshElevesTable();
    void showAddEleveDialog();
    void showEditEleveDialog(int id);
    void deleteEleve(int id);
    void showEleveDetailPanel(int id);

    // ── Véhicules CRUD ──
    void refreshVehiculesTable();
    void showAddVehiculeDialog();
    void showEditVehiculeDialog(int id);
    void deleteVehicule(int id);

    // ── Sessions ──
    void refreshSessionsTable();
    void deleteSession(int id);

    // ── Réservations ──
    void refreshReservationsTable();
    void updateReservationStatut(int id, const QString &status);
    void deleteReservation(int id);

    // ── Tutoriels Vidéo CRUD (colonne dans PARKING_MANEUVER_STEPS) ──
    QWidget* createVideosPage();
    void refreshVideosTable();
    void showAddVideoDialog();
    void showEditVideoDialog(int id);                     // stub inutilisé
    void showEditVideoDialog(const QString &maneuverKey); // version réelle
    void deleteVideo(int id);                             // stub inutilisé

    // ── Sessions Examen CRUD ──
    QWidget* createExamSessionsPage();
    void refreshExamSessionsTable();
    void showAddExamSessionDialog();
    void showEditExamSessionDialog(int id);
    void deleteExamSession(int id);
    void addExamSlotDialog();
    void refreshExamSlotsTable();

    // ── Maneuver Steps CRUD (PARKING_MANEUVER_STEPS) ──
    QWidget* createManeuverStepsPage();
    void refreshManeuverStepsTable();
    void showAddManeuverStepDialog();
    void showEditManeuverStepDialog(int stepId, const QString &maneuverType);
    void deleteManeuverStep(int stepId);

    // ── Exam Results admin view (PARKING_EXAM_RESULTS) ──
    QWidget* createExamResultsPage();
    void refreshExamResultsTable();
    void deleteExamResult(int resultId);

    // ── Search ──
    void onGlobalSearch(const QString &text);

    // ── Navigation ──
    void navigateTo(int page);

    QString m_userName, m_userRole;
    bool m_isMoniteur;

    // UI
    QStackedWidget *m_pages;
    QList<QPushButton*> m_navBtns;
    QLineEdit *m_searchEdit;

    // KPI labels
    QLabel *m_kpiEleves, *m_kpiElevesSub;
    QLabel *m_kpiVehicules, *m_kpiVehiculesSub;
    QLabel *m_kpiSessions, *m_kpiSessionsSub;
    QLabel *m_kpiTaux, *m_kpiTauxSub;
    QLabel *m_kpiRevenu, *m_kpiRevenuSub;

    // Dashboard
    QVBoxLayout *m_timelineLayout;
    QVBoxLayout *m_topStudentsLayout;
    QVBoxLayout *m_fleetLayout;

    // Tables
    QTableWidget *m_moniteursTable;
    QTableWidget *m_elevesTable;
    QTableWidget *m_vehiculesTable;
    QTableWidget *m_sessionsTable;
    QTableWidget *m_reservationsTable;
    QTableWidget *m_videosTable;
    QTableWidget *m_examSessionsTable;
    QTableWidget *m_examSlotsTable;
    QTableWidget *m_maneuverStepsTable;
    QTableWidget *m_examResultsTable;

    // Moniteurs KPI
    QLabel *m_kpiMoniteurs, *m_kpiMoniteursSub;
};

#endif
