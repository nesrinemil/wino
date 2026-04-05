#ifndef SENSORMANAGER_H
#define SENSORMANAGER_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include <QStringList>

// ═══════════════════════════════════════════════════════════════
// Structure des données capteurs de la maquette
// ═══════════════════════════════════════════════════════════════
// La maquette contient :
//   - 1 capteur avant (F)
//   - 1 capteur gauche (L)
//   - 1 capteur droit (R)
//   - 1 capteur arrière gauche (RL)
//   - 1 capteur arrière droit (RR)
//
// Protocole série Arduino → Qt :
//   "S:F=0,L=0,R=1,RL=0,RR=0"          (état binaire)
//   "D:F=999,L=45,R=12,RL=999,RR=999"   (distances en cm)
//   "E:RIGHT_OBSTACLE"                   (événement)
//   "E:LEFT_SENSOR1"                     (capteur gauche → capteur 1)
//   "E:RIGHT_SENSOR2"                    (capteur droit → capteur 2)
//   "E:REAR_RIGHT_SENSOR"               (capteur arrière droit → capteur)
//   "E:BOTH_REAR_OBSTACLE"              (les 2 arrière → obstacle)
//   "E:BOTH_REAR_SENSOR"                (les 2 arrière → capteur final)
// ═══════════════════════════════════════════════════════════════

struct ParkingSensorData {
    // État binaire : 0=rien, 1=détecté
    int avant       = 0;
    int gauche      = 0;
    int droit       = 0;
    int arriereGauche = 0;
    int arriereDroit  = 0;

    // Distances en cm (-1 = pas de mesure)
    double distAvant      = -1;
    double distGauche     = -1;
    double distDroit      = -1;
    double distArrGauche  = -1;
    double distArrDroit   = -1;
};

class SensorManager : public QObject
{
    Q_OBJECT
public:
    explicit SensorManager(QObject *parent = nullptr);
    ~SensorManager();

    // Connexion
    bool connectToPort(const QString &portName, int baudRate = 9600);
    void disconnect();
    bool isConnected() const;
    QString currentPort() const;

    // Ports disponibles
    static QStringList availablePorts();

    // Données courantes
    ParkingSensorData currentData() const { return m_data; }

    // Simulation (pour tester sans maquette réelle)
    void enableSimulation(bool enable);
    void simulateEvent(const QString &event);
    void simulateSensorState(int avant, int gauche, int droit, int arrG, int arrD);

signals:
    // Données capteurs mises à jour
    void sensorDataUpdated(const ParkingSensorData &data);

    // Événements spécifiques de la maquette
    void eventReceived(const QString &event);

    // Événements métier pour la séquence de parking
    void rightObstacleDetected();
    void leftObstacleDetected();
    void leftSensor1Detected();       // capteur gauche → capteur 1
    void rightSensor2Detected();       // capteur droit → capteur 2
    void rearRightSensorDetected();    // capteur arrière droit → capteur
    void bothRearObstacleDetected();   // les 2 arrière → obstacle
    void bothRearSensorDetected();     // les 2 arrière → capteur (final)
    void frontObstacleDetected();

    // Connexion
    void connected(const QString &port);
    void disconnected();
    void connectionError(const QString &error);

private slots:
    void onDataReady();
    void onSerialError(QSerialPort::SerialPortError error);

private:
    void parseLine(const QString &line);
    void parseSensorState(const QString &payload);
    void parseDistances(const QString &payload);
    void parseEvent(const QString &event);

    QSerialPort *m_serial;
    ParkingSensorData m_data;
    QString m_buffer;
    bool m_simulation;
};

#endif // SENSORMANAGER_H
