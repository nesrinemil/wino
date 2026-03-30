#ifndef WEATHERSERVICE_H
#define WEATHERSERVICE_H

#include <QObject>
#include <QDate>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>

struct DayWeather {
    double tempMax = 0;
    double tempMin = 0;
    int weatherCode = -1;
    QString description;
    QString icon;
};

class WeatherService : public QObject
{
    Q_OBJECT

public:
    static WeatherService* instance();

    void fetchForecast(double latitude = 36.80, double longitude = 10.18);
    DayWeather getWeather(const QDate &date) const;
    bool hasData() const;

    // WMO weather code helpers
    static QString weatherCodeToDescription(int code);
    static QString weatherCodeToIcon(int code);
    static QString weatherCodeToDrivingAdvice(int code);
    static int weatherCodeToSuitability(int code); // 0-100 score for driving

signals:
    void weatherDataReady();

private:
    explicit WeatherService(QObject *parent = nullptr);
    void parseResponse(const QByteArray &data);

    QNetworkAccessManager *networkManager;
    QMap<QDate, DayWeather> forecastData;
    bool dataLoaded = false;

    static WeatherService *s_instance;
};

#endif // WEATHERSERVICE_H
