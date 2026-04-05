#ifndef RADARWIDGET_H
#define RADARWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QScrollArea>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QComboBox>
#include <QLineEdit>
#include <QProgressBar>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QTextEdit>

#include "locationmapdialog.h"

#include <QGeoPositionInfoSource>
#include <QGeoPositionInfo>
#include <QGeoCoordinate>

// ─── Marker structs ────────────────────────────────────────────
struct AlertMarker {
    double lat, lng;
    QString type, street;
    int confidence;
    QColor color;
};

struct JamSegment {
    double lat, lng;
    int level, speed;
    QString street;
    QColor color;
};

// ─── RadarMapWidget — TileMap + overlay ────────────────────────
class RadarMapWidget : public TileMapWidget
{
    Q_OBJECT
public:
    explicit RadarMapWidget(QWidget *parent = nullptr);
    void clearMarkers();
    void addAlertMarker(const AlertMarker &m);
    void addJamMarker(const JamSegment &j);
    void setInfoText(const QString &text);

    // GPS student position
    void setGpsPosition(double lat, double lng, bool visible);
    void setGpsAccuracy(double meters);   // draw precision circle
    void addGpsTrailPoint(double lat, double lng);
    void clearGpsTrail();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QList<AlertMarker> m_alerts;
    QList<JamSegment> m_jams;
    QString m_infoText;
    bool m_gpsVisible = false;
    double m_gpsLat = 0, m_gpsLng = 0;
    double m_gpsAccuracyM = -1;           // accuracy circle radius in metres (-1 = hide)
    QList<QPointF> m_gpsTrail;
};

// ─── RadarWidget — main page ───────────────────────────────────
class RadarWidget : public QWidget
{
    Q_OBJECT
public:
    explicit RadarWidget(const QString &userName, const QString &userRole,
                         QWidget *parent = nullptr);
    ~RadarWidget();

private slots:
    void refreshTrafficData();
    void onTrafficDataReceived(QNetworkReply *reply);
    void onRouteDataReceived(QNetworkReply *reply);
    void searchRoute();
    void onCityChanged(int index);
    void toggleAutoRefresh();
    void zoomIn();
    void zoomOut();
    void onPulseTimer();
    void fetchGpsPosition();
    void fetchGpsViaNetwork();           // IP/WiFi fallback
    void onGeoPositionUpdated(const QGeoPositionInfo &info);
    void onGeoError(QGeoPositionInfoSource::Error error);
    void onNetworkGpsReceived(QNetworkReply *reply);
    void applyGpsPosition(double lat, double lon, double speedKmh, double accuracy, const QString &source);
    void toggleGpsTracking();
    void applyManualCoordinates();       // Level 3: user-typed coords
    void toggleAutoCenter();             // keep map locked to GPS pos

private:
    void setupUI();
    QWidget* createBanner();
    QWidget* createMapCard();
    QWidget* createAlertsPanel();
    QWidget* createTrafficJamsPanel();
    QWidget* createRoutePanel();
    QWidget* createStatsRow();
    QWidget* createGpsPanel();
    QWidget* createStatPill(const QString &icon, const QString &val,
                            const QString &label, const QString &baseColor);
    QWidget* createAlertRow(const QString &icon, const QString &type,
                            const QString &street, const QString &info,
                            int confidence, const QString &color);
    QWidget* createJamRow(const QString &street, int level, int speed,
                          int delay, double length);

    // (Legacy parse methods removed — using loadSmartTrafficData now)
    void updateStatsDisplay();
    void clearAlerts();
    void clearJams();
    void addStatusMessage(const QString &msg);
    void loadDemoData();
    void loadSmartTrafficData();
    void updateMapMarkers();

    static QGraphicsDropShadowEffect* glow(QWidget *p, const QColor &c, int blur=20);
    static QGraphicsDropShadowEffect* softShadow(QWidget *p);

    QString m_userName, m_userRole;
    QString m_orsApiKey;  // ORS API Key (free: openrouteservice.org)
    QNetworkAccessManager *m_networkManager;
    QTimer *m_refreshTimer;
    QTimer *m_pulseTimer;
    bool m_autoRefresh;
    int m_pulsePhase = 0;

    // UI
    QVBoxLayout *m_alertsLayout, *m_jamsLayout;
    QLabel *m_statusLabel, *m_lastUpdateLabel;
    QLabel *m_alertCountLabel, *m_jamCountLabel, *m_avgSpeedLabel, *m_delayLabel;
    RadarMapWidget *m_radarMap;
    QTextEdit *m_logConsole;
    QComboBox *m_cityCombo;
    QLineEdit *m_fromEdit, *m_toEdit;
    QLineEdit *m_apiKeyEdit;
    QPushButton *m_refreshBtn, *m_autoRefreshBtn;
    QLabel *m_routeDistLabel, *m_routeTimeLabel, *m_routeNameLabel;
    QVBoxLayout *m_routeResultLayout;
    QLabel *m_liveDot;

    // GPS tracking — multi-source location system
    //  Level 1: QGeoPositionInfoSource (real GPS/WiFi from OS)
    //  Level 2: Network geolocation (ipinfo.io — IP + WiFi hybrid)
    //  Level 3: Manual coordinates
    QGeoPositionInfoSource *m_geoSource;
    QNetworkAccessManager *m_gpsNam;     // for network fallback
    QTimer *m_gpsTimer;
    int  m_gpsLevel;                     // 1=Qt, 2=Network, 3=Manual
    bool m_gpsTracking = false;
    bool m_gpsFound = false;
    double m_gpsLat = 0, m_gpsLng = 0;
    double m_gpsSpeed = 0;
    double m_gpsAltitude = 0;
    double m_gpsAccuracy = 0;
    QString m_gpsCity;
    QString m_gpsSourceName;
    QLabel *m_gpsStatusLabel;
    QLabel *m_gpsCoordsLabel;
    QLabel *m_gpsSpeedLabel;
    QLabel *m_gpsAccuracyLabel;
    QLabel *m_gpsLastFixLabel;           // timestamp of last GPS update
    QPushButton *m_gpsToggleBtn;
    QPushButton *m_gpsCenterBtn;
    QPushButton *m_gpsAutoCenterBtn;     // lock map to GPS position
    QLineEdit  *m_manualLatEdit;         // Level 3 — manual lat input
    QLineEdit  *m_manualLngEdit;         // Level 3 — manual lon input
    bool  m_autoCenter = false;          // follow GPS on map
    QDateTime m_lastGpsFix;             // time of last valid fix
    QList<QPointF> m_gpsTrail;

    // Data
    int m_totalAlerts = 0, m_totalJams = 0, m_totalDelaySec = 0;
    double m_avgSpeed = 0;
    QList<AlertMarker> m_alertMarkers;
    QList<JamSegment> m_jamMarkers;

    struct CityBBox { QString name; double bottom, left, top, right; };
    QList<CityBBox> m_cities;
    int m_currentCity = 0;
};

#endif
