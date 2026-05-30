#include "HttpIhaListener.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

HttpIhaListener::HttpIhaListener(QObject *parent)
    : QObject(parent), timer(new QTimer(this)), manager(new QNetworkAccessManager(this))
{
    connect(timer, &QTimer::timeout, this, &HttpIhaListener::fetchData);
}

void HttpIhaListener::start() {
    timer->start(1000); // her saniye
}

void HttpIhaListener::stop() {
    timer->stop();
}

void HttpIhaListener::fetchData() {
    QNetworkRequest request(QUrl("http://localhost:5000/rakip-iha"));
    QNetworkReply* reply = manager->get(request);
    connect(reply, &QNetworkReply::finished, this, &HttpIhaListener::onReplyFinished);
}

void HttpIhaListener::onReplyFinished() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    QByteArray response = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(response);
    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        if (obj.contains("telemetry") && obj["telemetry"].isArray()) {
            QJsonArray telemetryArray = obj["telemetry"].toArray();
            for (int i = 0; i < telemetryArray.size(); ++i) {
                QJsonObject telem = telemetryArray[i].toObject();
                int ihaId = i;
                double latitude = telem["iha_enlem"].toDouble();
                double longitude = telem["iha_boylam"].toDouble();
                bool isEnemy = (i != 0);
                emit ihaPositionUpdated(ihaId, latitude, longitude, isEnemy);
            }
        }
    }
    reply->deleteLater();
} 