#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QPointer>
#include <QMap>
#include <QPointF>

class RakipIhaPoller : public QObject {
    Q_OBJECT
public:
    explicit RakipIhaPoller(QObject* parent = nullptr);

    // ör: start(QUrl("http://localhost:5000/rakip-iha"), 1000);
    void start(const QUrl& url, int periodMs = 1000);
    void stop();

signals:
    // takım_numarası 1..N → lat, lon, yaw
    void telemetryForTeam(int teamNo, double lat, double lon, double yawDeg);

private slots:
    void tick();
    void onFinished();

private:
    QUrl url_;
    QTimer timer_;
    QNetworkAccessManager nam_;
    QPointer<QNetworkReply> reply_{nullptr};
    QMap<int, QPointF> lastPos_;

    static QByteArray extractJsonBlock(const QByteArray& htmlOrJson);
    static double bearingDeg(double lat1, double lon1, double lat2, double lon2);
}; 