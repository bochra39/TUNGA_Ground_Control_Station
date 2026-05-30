#include "RosTelemetryListener.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QDebug>
#include <QRegularExpression>
#include <QRandomGenerator>
#include <QFile>
#include <QNetworkCookie>

RosTelemetryListener::RosTelemetryListener(QObject *parent)
    : QObject(parent)
    , rosProcess(nullptr)
    , networkManager(new QNetworkAccessManager(this))
    , cookieJar(new QNetworkCookieJar(this))
    , reconnectTimer(new QTimer(this))
    , telemetryTimer(nullptr)
    , isListening(false)
{
    networkManager->setCookieJar(cookieJar);
    connect(reconnectTimer, &QTimer::timeout, this, [this]() {
        if (isListening) {
            startListening();
        }
    });
}

RosTelemetryListener::~RosTelemetryListener()
{
    stopListening();
}

void RosTelemetryListener::setCookieJar(QNetworkCookieJar *jar)
{
    if (!jar) return;
    cookieJar = jar;
    networkManager->setCookieJar(cookieJar);
}

void RosTelemetryListener::setServerUrl(const QString &url)
{
    serverUrl = url;
    qDebug() << "RosTelemetryListener: Sunucu URL ayarlandı:" << serverUrl;
}

// setAuthToken kaldırıldı, cookie kullanılıyor

// setAuthCookie kaldırıldı, cookie kullanılıyor

void RosTelemetryListener::startListening()
{
    // ROS topic'lerini dinlemek için timer başlat
    // Python node'unuz zaten çalışıyor ve veri gönderiyor
    isListening = true;
    qDebug() << "ROS telemetri dinleme başlatıldı";
    
    // Telemetri verilerini periyodik olarak kontrol et
    if (!telemetryTimer) {
        telemetryTimer = new QTimer(this);
        connect(telemetryTimer, &QTimer::timeout, this, &RosTelemetryListener::checkTelemetryData);
    }
    telemetryTimer->start(1000); // Her saniye kontrol et
}

void RosTelemetryListener::stopListening()
{
    isListening = false;
    if (telemetryTimer) {
        telemetryTimer->stop();
    }
    reconnectTimer->stop();
    qDebug() << "ROS telemetri dinleme durduruldu";
}

void RosTelemetryListener::fetchTelemetry()
{
    // Sunucudan son telemetri verisini al
    qDebug() << "ROS telemetri verisi çekiliyor...";
    qDebug() << "Sunucu URL:" << serverUrl;
    
    if (serverUrl.isEmpty()) {
        emit error("Sunucu URL'i ayarlanmamış");
        return;
    }
    
    // Mevcut telemetri verisini sunucuya gönder ve yanıt al
    // Bu şekilde sunucudan son telemetri verisi alabiliriz
    QString url = serverUrl + "/api/telemetri_gonder";
    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    // Cookie otomatik olarak gönderiliyor
    
    // Boş telemetri verisi gönder (sadece son veriyi almak için)
    QJsonObject emptyTelemetry;
    QJsonDocument doc(emptyTelemetry);
    QByteArray data = doc.toJson();
    
    QNetworkReply* reply = networkManager->post(request, data);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray response = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(response);
            QJsonObject obj = doc.object();
            
            if (!obj.isEmpty()) {
                emit telemetryReceived(obj);
                qDebug() << "Telemetri verisi alındı:" << obj;
            } else {
                emit error("Sunucudan boş telemetri verisi geldi");
            }
        } else {
            emit error("Telemetri verisi alınamadı: " + reply->errorString());
        }
        reply->deleteLater();
    });
}

void RosTelemetryListener::sendTelemetryToServer(const QJsonObject &telemetry)
{
    if (serverUrl.isEmpty()) {
        emit error("Sunucu URL'i ayarlanmamış");
        return;
    }

    sendTelemetryRequest(telemetry);
}

void RosTelemetryListener::onProcessOutput()
{
    if (!rosProcess) return;

    QString output = QString::fromUtf8(rosProcess->readAllStandardOutput());
    qDebug() << "ROS çıktısı:" << output;
    
    parseTelemetryOutput(output);
}

void RosTelemetryListener::onProcessError()
{
    if (!rosProcess) return;

    QString error = QString::fromUtf8(rosProcess->readAllStandardError());
    qDebug() << "ROS hatası:" << error;
    
    if (!error.trimmed().isEmpty()) {
        emit this->error("ROS Hatası: " + error);
    }
}

void RosTelemetryListener::onProcessFinished(int exitCode)
{
    qDebug() << "ROS process sonlandı, exit code:" << exitCode;
    
    if (isListening && exitCode != 0) {
        qDebug() << "ROS node beklenmedik şekilde sonlandı, yeniden başlatılıyor...";
        reconnectTimer->start(5000); // 5 saniye sonra yeniden başlat
    }
}

void RosTelemetryListener::onNetworkReply(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(response);
        QJsonObject obj = doc.object();
        
        qDebug() << "Sunucu yanıtı:" << response;
        
        // Rakip IHA telemetrisini emit et
        emit telemetrySent(obj);
    } else {
        emit error("Sunucu hatası: " + reply->errorString());
    }
    
    reply->deleteLater();
}

void RosTelemetryListener::parseTelemetryOutput(const QString &output)
{
    // ROS node'dan gelen telemetri verisini parse et
    // Bu format ROS node'unuzun çıktı formatına göre ayarlanmalı
    
    QJsonObject telemetry;
    
    // Basit JSON parse etme
    QJsonDocument doc = QJsonDocument::fromJson(output.toUtf8());
    if (!doc.isNull() && doc.isObject()) {
        telemetry = doc.object();
    } else {
        // Eğer JSON değilse, manuel parse etme
        QRegularExpression re("latitude:\\s*([\\d.-]+)");
        auto match = re.match(output);
        if (match.hasMatch()) {
            telemetry["latitude"] = match.captured(1).toDouble();
        }
        
        re = QRegularExpression("longitude:\\s*([\\d.-]+)");
        match = re.match(output);
        if (match.hasMatch()) {
            telemetry["longitude"] = match.captured(1).toDouble();
        }
        
        re = QRegularExpression("altitude:\\s*([\\d.-]+)");
        match = re.match(output);
        if (match.hasMatch()) {
            telemetry["altitude"] = match.captured(1).toDouble();
        }
        
        re = QRegularExpression("speed:\\s*([\\d.-]+)");
        match = re.match(output);
        if (match.hasMatch()) {
            telemetry["speed"] = match.captured(1).toDouble();
        }
        
        re = QRegularExpression("heading:\\s*([\\d.-]+)");
        match = re.match(output);
        if (match.hasMatch()) {
            telemetry["heading"] = match.captured(1).toDouble();
        }
        
        re = QRegularExpression("battery:\\s*([\\d.-]+)");
        match = re.match(output);
        if (match.hasMatch()) {
            telemetry["battery"] = match.captured(1).toDouble();
        }
        
        telemetry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    }
    
    if (!telemetry.isEmpty()) {
        emit telemetryReceived(telemetry);
    }
}

void RosTelemetryListener::sendTelemetryRequest(const QJsonObject &telemetry)
{
    QString url = serverUrl + "/api/telemetri_gonder";
    qDebug() << "sendTelemetryRequest çağrıldı, URL:" << url;
    qDebug() << "Cookie kullanılıyor";
    
    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    // Cookie otomatik olarak gönderiliyor
    
    QJsonDocument doc(telemetry);
    QByteArray data = doc.toJson();
    
    qDebug() << "Telemetri gönderiliyor:" << url;
    qDebug() << "Telemetri verisi:" << data;
    
    networkManager->post(request, data);
}

void RosTelemetryListener::checkTelemetryData()
{
    // Sunucudan son telemetri verilerini al
    if (serverUrl.isEmpty()) {
        qDebug() << "Sunucu URL'i ayarlanmamış";
        return;
    }
    
    // Mevcut telemetri verisini sunucuya gönder ve yanıt al
    QString url = serverUrl + "/api/telemetri_gonder";
    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    // Cookie otomatik olarak gönderiliyor
    
    // Boş telemetri verisi gönder (sadece son veriyi almak için)
    QJsonObject emptyTelemetry;
    QJsonDocument doc(emptyTelemetry);
    QByteArray data = doc.toJson();
    
    QNetworkReply* reply = networkManager->post(request, data);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray response = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(response);
            QJsonObject obj = doc.object();
            
            if (!obj.isEmpty()) {
                emit telemetryReceived(obj);
            }
        } else {
            qDebug() << "Telemetri verisi alınamadı:" << reply->errorString();
        }
        reply->deleteLater();
    });
}
