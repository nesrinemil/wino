#include "sensormanager.h"
#include <QDebug>
#include <QDateTime>

SensorManager::SensorManager(QObject *parent)
    : QObject(parent), m_simulation(false)
{
    m_serial = new QSerialPort(this);
    connect(m_serial, &QSerialPort::readyRead, this, &SensorManager::onDataReady);
    connect(m_serial, &QSerialPort::errorOccurred, this, &SensorManager::onSerialError);
}

SensorManager::~SensorManager()
{
    disconnect();
}

// ═══════════════════════════════════════════════════════════════
// CONNEXION SÉRIE
// ═══════════════════════════════════════════════════════════════
bool SensorManager::connectToPort(const QString &portName, int baudRate)
{
    if (m_serial->isOpen()) m_serial->close();

    m_serial->setPortName(portName);
    m_serial->setBaudRate(baudRate);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    if (m_serial->open(QIODevice::ReadOnly)) {
        qDebug() << "[SENSOR] Connecté au port" << portName << "à" << baudRate << "baud";
        emit connected(portName);
        return true;
    } else {
        QString err = m_serial->errorString();
        qDebug() << "[SENSOR] Erreur connexion:" << err;
        emit connectionError(err);
        return false;
    }
}

void SensorManager::disconnect()
{
    if (m_serial->isOpen()) {
        m_serial->close();
        qDebug() << "[SENSOR] Déconnecté";
        emit disconnected();
    }
}

bool SensorManager::isConnected() const
{
    return m_serial->isOpen();
}

QString SensorManager::currentPort() const
{
    return m_serial->portName();
}

QStringList SensorManager::availablePorts()
{
    QStringList list;
    for (const auto &info : QSerialPortInfo::availablePorts()) {
        list.append(info.portName() + " - " + info.description());
    }
    return list;
}

// ═══════════════════════════════════════════════════════════════
// RÉCEPTION DONNÉES
// ═══════════════════════════════════════════════════════════════
void SensorManager::onDataReady()
{
    m_buffer += QString::fromUtf8(m_serial->readAll());

    // Traiter ligne par ligne
    while (m_buffer.contains('\n')) {
        int idx = m_buffer.indexOf('\n');
        QString line = m_buffer.left(idx).trimmed();
        m_buffer = m_buffer.mid(idx + 1);
        if (!line.isEmpty()) {
            parseLine(line);
        }
    }
}

void SensorManager::onSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) return;

    // Qt can emit 2+ different error codes for the same OS error (e.g. "Access is denied").
    // Use message + timestamp deduplication: suppress if same message within 3 s.
    static qint64  lastShownMs = 0;
    static QString lastMsg;
    qint64  now = QDateTime::currentMSecsSinceEpoch();
    QString msg = m_serial->errorString();

    if (msg == lastMsg && (now - lastShownMs) < 3000) return;
    lastShownMs = now;
    lastMsg     = msg;

    qDebug() << "[SENSOR] Erreur série:" << msg;
    emit connectionError(msg);
}

// ═══════════════════════════════════════════════════════════════
// PARSING DU PROTOCOLE
// ═══════════════════════════════════════════════════════════════
//
// Format attendu depuis l'Arduino :
//   "S:F=0,L=0,R=1,RL=0,RR=0"     → état capteurs (binaire)
//   "D:F=150,L=45,R=12,RL=200,RR=30" → distances cm
//   "E:RIGHT_OBSTACLE"              → événement
//
// ── Arduino raw sensor line: "F:56.3 L:16.0 R:263.9 BR:172.7 BL:-1.0 θ:0.0°" ──
static bool parseArduinoRawSensors(const QString &line, ParkingSensorData &data)
{
    if (!line.startsWith("F:") || !line.contains(" L:") || !line.contains(" R:"))
        return false;
    auto extractVal = [&](const QString &key) -> double {
        int idx = line.indexOf(key);
        if (idx < 0) return -1.0;
        int start = idx + key.length();
        int end = start;
        while (end < line.length() && (line[end].isDigit() || line[end] == '.' || line[end] == '-'))
            end++;
        bool ok;
        double v = line.mid(start, end - start).toDouble(&ok);
        return ok ? v : -1.0;
    };
    data.distAvant     = extractVal("F:");
    data.distGauche    = extractVal(" L:");
    data.distDroit     = extractVal(" R:");
    data.distArrDroit  = extractVal("BR:");
    data.distArrGauche = extractVal("BL:");
    return true;
}

void SensorManager::parseLine(const QString &line)
{
    qDebug() << "[SENSOR] Reçu:" << line;

    // Always emit raw line for terminal/log display
    emit rawLineReceived(line);

    if (line.startsWith("S:")) {
        parseSensorState(line.mid(2));
        return;
    }
    if (line.startsWith("D:")) {
        parseDistances(line.mid(2));
        return;
    }
    if (line.startsWith("E:")) {
        parseEvent(line.mid(2).trimmed());
        return;
    }

    // Arduino raw sensor format
    if (parseArduinoRawSensors(line, m_data)) {
        emit sensorDataUpdated(m_data);
        return;
    }

    // HC-05 connection confirmation
    if (line.contains("HC-05 connected") || line.contains("HC-05 connecté")) {
        emit connected("HC-05");
        return;
    }

    // Arduino state-machine step transitions → auto-advance signals
    if (line.contains("TURN RIGHT + GO FORWARD")) {
        emit arduinoStepAdvance("STEP1_DONE");
    } else if (line.contains("GO STRAIGHT") && line.contains("barrier")) {
        emit arduinoStepAdvance("STEP2_DONE");
    } else if (line.contains("EXTREMITY REACHED")) {
        emit arduinoStepAdvance("STEP3_DONE");
    } else if (line.contains("POSITION OK")) {
        emit arduinoStepAdvance("STEP4_DONE");
    } else if (line.contains("PARKING COMPLETE")) {
        emit arduinoStepAdvance("PARKING_DONE");
    }
}

void SensorManager::parseSensorState(const QString &payload)
{
    // "F=0,L=0,R=1,RL=0,RR=0"
    QStringList parts = payload.split(',');
    for (const auto &p : parts) {
        QStringList kv = p.trimmed().split('=');
        if (kv.size() != 2) continue;
        int val = kv[1].toInt();
        if      (kv[0] == "F")  m_data.avant = val;
        else if (kv[0] == "L")  m_data.gauche = val;
        else if (kv[0] == "R")  m_data.droit = val;
        else if (kv[0] == "RL") m_data.arriereGauche = val;
        else if (kv[0] == "RR") m_data.arriereDroit = val;
    }
    emit sensorDataUpdated(m_data);
}

void SensorManager::parseDistances(const QString &payload)
{
    // "F=150,L=45,R=12,RL=200,RR=30"
    QStringList parts = payload.split(',');
    for (const auto &p : parts) {
        QStringList kv = p.trimmed().split('=');
        if (kv.size() != 2) continue;
        double val = kv[1].toDouble();
        if      (kv[0] == "F")  m_data.distAvant = val;
        else if (kv[0] == "L")  m_data.distGauche = val;
        else if (kv[0] == "R")  m_data.distDroit = val;
        else if (kv[0] == "RL") m_data.distArrGauche = val;
        else if (kv[0] == "RR") m_data.distArrDroit = val;
    }
    emit sensorDataUpdated(m_data);
}

void SensorManager::parseEvent(const QString &event)
{
    emit eventReceived(event);

    if (event == "RIGHT_OBSTACLE") {
        m_data.droit = 1;
        emit rightObstacleDetected();
    }
    else if (event == "LEFT_OBSTACLE") {
        m_data.gauche = 1;
        emit leftObstacleDetected();
    }
    else if (event == "LEFT_SENSOR1") {
        emit leftSensor1Detected();
    }
    else if (event == "RIGHT_SENSOR2") {
        emit rightSensor2Detected();
    }
    else if (event == "REAR_RIGHT_SENSOR") {
        m_data.arriereDroit = 1;
        emit rearRightSensorDetected();
    }
    else if (event == "BOTH_REAR_OBSTACLE") {
        m_data.arriereGauche = 1;
        m_data.arriereDroit = 1;
        emit bothRearObstacleDetected();
    }
    else if (event == "BOTH_REAR_SENSOR") {
        m_data.arriereGauche = 1;
        m_data.arriereDroit = 1;
        emit bothRearSensorDetected();
    }
    else if (event == "FRONT_OBSTACLE") {
        m_data.avant = 1;
        emit frontObstacleDetected();
    }

    emit sensorDataUpdated(m_data);
}

// ═══════════════════════════════════════════════════════════════
// SIMULATION (pour tester sans Arduino)
// ═══════════════════════════════════════════════════════════════
void SensorManager::enableSimulation(bool enable)
{
    m_simulation = enable;
    if (enable) {
        qDebug() << "[SENSOR] Mode simulation activé";
        emit connected("SIMULATION");
    }
}

void SensorManager::simulateEvent(const QString &event)
{
    if (!m_simulation) return;
    qDebug() << "[SENSOR-SIM] Événement:" << event;
    parseEvent(event);
}

void SensorManager::simulateSensorState(int avant, int gauche, int droit, int arrG, int arrD)
{
    if (!m_simulation) return;
    m_data.avant = avant;
    m_data.gauche = gauche;
    m_data.droit = droit;
    m_data.arriereGauche = arrG;
    m_data.arriereDroit = arrD;
    emit sensorDataUpdated(m_data);
}
