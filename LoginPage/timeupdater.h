#ifndef TIMEUPDATER_H
#define TIMEUPDATER_H

#include <QObject>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>

class TimeUpdater : public QObject {
    Q_OBJECT
public:
    TimeUpdater(QLabel* label) : m_label(label) {
        manager = new QNetworkAccessManager(this);
        timer = new QTimer(this);
        
        connect(timer, &QTimer::timeout, this, &TimeUpdater::updateTime);
        connect(manager, &QNetworkAccessManager::finished, this, &TimeUpdater::handleResponse);
        
        // Her saniye güncelle
        timer->start(1000);
        // İlk güncellemeyi hemen yap
        updateTime();
    }

private slots:
    void updateTime() {
        QNetworkRequest request(QUrl("https://sametyildirim.com/api/sunucusaati/"));
        manager->get(request);
    }

    void handleResponse(QNetworkReply* reply) {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            QJsonObject obj = doc.object();
            
            QString serverTime = obj["sunucuSaati"].toString();
            QDateTime dateTime = QDateTime::fromString(serverTime, "yyyy-MM-dd HH:mm:ss");
            
            // Saat, dakika, saniye formatında göster
            QString timeStr = dateTime.toString("HH:mm:ss");
            m_label->setText(timeStr);
        }
        reply->deleteLater();
    }

private:
    QLabel* m_label;
    QNetworkAccessManager* manager;
    QTimer* timer;
};

#endif // TIMEUPDATER_H 