#ifndef ROSTELEMETRYLISTENER_H
#define ROSTELEMETRYLISTENER_H

#include <QObject>
#include <QProcess>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QJsonObject>
#include <QNetworkCookieJar>

class RosTelemetryListener : public QObject
{
    Q_OBJECT

public:
    explicit RosTelemetryListener(QObject *parent = nullptr);
    ~RosTelemetryListener();

    void setServerUrl(const QString &url);
    // setAuthToken kaldırıldı, cookie kullanılıyor
    void setCookieJar(QNetworkCookieJar *jar);
    void startListening();
    void stopListening();
    void fetchTelemetry();
    void sendTelemetryToServer(const QJsonObject &telemetry);

signals:
    void telemetryReceived(const QJsonObject &telemetry);
    void telemetrySent(const QJsonObject &response);
    void error(const QString &errorMessage);

private slots:
    void onProcessOutput();
    void onProcessError();
    void onProcessFinished(int exitCode);
    void onNetworkReply(QNetworkReply *reply);

private:
    void parseTelemetryOutput(const QString &output);
    void sendTelemetryRequest(const QJsonObject &telemetry);
    void checkTelemetryData();
    // setAuthCookie kaldırıldı, cookie kullanılıyor

    QProcess *rosProcess;
    QNetworkAccessManager *networkManager;
    QNetworkCookieJar *cookieJar;
    QTimer *reconnectTimer;
    QTimer *telemetryTimer;
    QString serverUrl;
    // authToken kaldırıldı, cookie kullanılıyor
    bool isListening;
};

#endif // ROSTELEMETRYLISTENER_H
