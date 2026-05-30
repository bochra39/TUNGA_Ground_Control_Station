#include "RakipIhaPoller.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <limits>
#include <cmath>
#include <QDebug>

RakipIhaPoller::RakipIhaPoller(QObject* parent) : QObject(parent) {
    connect(&timer_, &QTimer::timeout, this, &RakipIhaPoller::tick);
}

void RakipIhaPoller::start(const QUrl& url, int periodMs) {
    url_ = url;
    timer_.start(periodMs);
    tick();
}

void RakipIhaPoller::stop() {
    timer_.stop();
    if (reply_) { reply_->abort(); reply_->deleteLater(); reply_ = nullptr; }
}

void RakipIhaPoller::tick() {
    if (reply_) return; // bir önceki bitmeden yenisini açma
    QNetworkRequest req(url_);
    req.setHeader(QNetworkRequest::UserAgentHeader, "GorevKontrolArayuzu/1.0");
    req.setRawHeader("Accept", "application/json");
    reply_ = nam_.get(req);
    connect(reply_, &QNetworkReply::finished, this, &RakipIhaPoller::onFinished);
}

QByteArray RakipIhaPoller::extractJsonBlock(const QByteArray& body) {
    // Önce saf JSON mu diye hızlı kontrol
    QJsonParseError quickErr{};
    auto quickDoc = QJsonDocument::fromJson(body, &quickErr);
    if (quickErr.error == QJsonParseError::NoError && quickDoc.isObject()) {
        return body; // zaten saf JSON
    }

    // HTML içinden {"sunucusaati": ... ,"konumBilgileri": [...] } bloğunu yakala
    // DOTALL (SingleLine) için [\\s\\S] kullanıyoruz
    QRegularExpression re("\\{[\\s\\S]*?\"sunucusaati\"[\\s\\S]*?\"konumBilgileri\"[\\s\\S]*?\\}");
    auto m = re.match(QString::fromUtf8(body));
    if (m.hasMatch()) {
        return m.captured(0).toUtf8();
    }

    // Olmadıysa mevcut basit yöntem (ilk { ... son } arası)
    int l = body.indexOf('{');
    int r = body.lastIndexOf('}');
    if (l >= 0 && r > l) return body.mid(l, r - l + 1);

    return {};
}

double RakipIhaPoller::bearingDeg(double lat1, double lon1, double lat2, double lon2) {
    const double r = M_PI / 180.0;
    double phi1 = lat1 * r, phi2 = lat2 * r;
    double deltaLon = (lon2 - lon1) * r;
    double y = sin(deltaLon) * cos(phi2);
    double x = cos(phi1)*sin(phi2) - sin(phi1)*cos(phi2)*cos(deltaLon);
    double theta = atan2(y, x) * 180.0 / M_PI;
    if (theta < 0) theta += 360.0;
    return fmod(theta, 360.0);
}

void RakipIhaPoller::onFinished() {
    QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> guard(reply_);
    reply_ = nullptr;
    if (!guard) return;

    if (guard->error() != QNetworkReply::NoError) {
        // hata olursa sessiz geç; loglamak istersen buraya yaz
        qDebug() << "RakipIhaPoller network error:" << guard->errorString();
        return;
    }

    QByteArray raw = guard->readAll();
    
    // HTML geldiğini anlamak için log
    if (raw.startsWith("<!DOCTYPE") || raw.contains("<html")) {
        qDebug() << "RakipIhaPoller: HTML geldi, gömülü JSON aranıyor…";
    }
    
    QByteArray jsonBytes = extractJsonBlock(raw);

    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qDebug() << "RakipIhaPoller JSON parse error:" << err.errorString() << "at offset:" << err.offset;
        return;
    }

    const QJsonObject root = doc.object();
    const QJsonValue vKonum = root.value(QStringLiteral("konumBilgileri"));
    if (!vKonum.isArray()) return;

    const QJsonArray arr = vKonum.toArray();
    for (const QJsonValue& v : arr) {
        if (!v.isObject()) continue;
        const QJsonObject o = v.toObject();
        const int teamNo = o.value(QStringLiteral("takim_numarasi")).toInt();
        const double lat = o.value(QStringLiteral("iha_enlem")).toDouble(std::numeric_limits<double>::quiet_NaN());
        const double lon = o.value(QStringLiteral("iha_boylam")).toDouble(std::numeric_limits<double>::quiet_NaN());
        
        if (std::isfinite(lat) && std::isfinite(lon)) {
            double yaw = o.value(QStringLiteral("iha_yonelme")).toDouble(std::numeric_limits<double>::quiet_NaN());
            
            // Fallback: yaw yoksa/bad ise bearing hesapla
            if (!std::isfinite(yaw)) {
                if (lastPos_.contains(teamNo)) {
                    const QPointF& p = lastPos_.value(teamNo);
                    yaw = bearingDeg(p.y(), p.x(), lat, lon);
                } else {
                    yaw = 0.0;
                }
            }
            lastPos_[teamNo] = QPointF(lon, lat);
            
            qDebug() << "RakipIhaPoller: Team" << teamNo << "position:" << lat << lon << "yaw:" << yaw;
            emit telemetryForTeam(teamNo, lat, lon, yaw);
        }
    }
} 