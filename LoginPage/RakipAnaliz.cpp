// RakipAnaliz.cpp
#include "RakipAnaliz.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QtMath>
#include <algorithm>
#include <limits>

RakipAnaliz::RakipAnaliz(QObject* parent) : QObject(parent), net_(new QNetworkAccessManager(this)) {
    connect(&timer_, &QTimer::timeout, this, &RakipAnaliz::onTick);
    timer_.setInterval(1000); // 1 saniye
    qDebug() << "[RakipAnaliz] oluşturuldu. Timer interval:" << timer_.interval() << "ms";
}

RakipAnaliz::~RakipAnaliz() {}

void RakipAnaliz::setServerUrl(const QString& url) {
    serverUrl_ = url;
    qDebug() << "[RakipAnaliz] serverUrl set edildi.";
}

void RakipAnaliz::setCookieJar(QNetworkCookieJar* jar) {
    if (jar && net_) {
        net_->setCookieJar(jar);
        qDebug() << "[RakipAnaliz] Cookie jar set edildi";
    } else {
        qDebug() << "[RakipAnaliz] Cookie jar set EDEMİYOR (net/param null)";
    }
}

void RakipAnaliz::start() {
    qDebug() << "[RakipAnaliz] start() çağrıldı. interval=" << timer_.interval();
    timer_.start();
}
void RakipAnaliz::stop()  {
    qDebug() << "[RakipAnaliz] stop() çağrıldı.";
    timer_.stop();
}

void RakipAnaliz::onTick() {
    // Şimdilik placeholder: gerçek analiz entegrasyonu burada yapılacak
    emit analysisReady(QString::fromUtf8("Rakip Analizi Başlatılıyor..."));
}

// MainPageWidget'ten gelen ham JSON'u tüket ve basit bir özet üret
void RakipAnaliz::consumeRivalsJson(const QString& rawJson) {
    QJsonParseError err{};
    const auto doc = QJsonDocument::fromJson(rawJson.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError) {
        emit analysisReady(QString::fromUtf8("Rakip analizi: JSON hatası"));
        return;
    }
    const QJsonValue root = doc.isObject() ? QJsonValue(doc.object()) : QJsonValue(doc.array());
    // Beklenen alan: konumBilgileri[] veya telemetry[]
    QJsonArray arr;
    if (root.isObject()) {
        const auto obj = root.toObject();
        if (obj.value("konumBilgileri").isArray()) arr = obj.value("konumBilgileri").toArray();
        else if (obj.value("telemetry").isArray()) arr = obj.value("telemetry").toArray();
    } else if (root.isArray()) {
        arr = root.toArray();
    }

    // Çekirdek metrik akümülatörleri
    struct Row { int id; double score; double yawVar; double altVar; double spdVar; double routeExcess; bool headOn; bool closing; };
    QVector<Row> rows;

    // Önce tüm takımları gez ve state güncelle
    for (const auto& v : arr) {
        if (!v.isObject()) continue;
        const auto o = v.toObject();
        const int team = o.value("takim_numarasi").toInt(-1);
        if (team < 0) continue;

        TeamState st = teams_.value(team);
        st.teamId = team;

        const double lat = o.value("iha_enlem").toDouble(std::numeric_limits<double>::quiet_NaN());
        const double lon = o.value("iha_boylam").toDouble(std::numeric_limits<double>::quiet_NaN());
        const double yaw = o.value("iha_yonelme").toDouble(std::numeric_limits<double>::quiet_NaN());
        const double alt = o.value("iha_irtifa").toDouble(std::numeric_limits<double>::quiet_NaN());
        double spd = std::numeric_limits<double>::quiet_NaN();
        if (o.contains("iha_hizi")) {
            const QJsonValue v = o.value("iha_hizi");
            if (v.isDouble()) spd = v.toDouble();
            else if (v.isString()) {
                bool ok=false; const double tmp = v.toString().toDouble(&ok); if (ok) spd = tmp;
            }
        } else if (o.contains("iha_hiz")) {
            const QJsonValue v = o.value("iha_hiz");
            if (v.isDouble()) spd = v.toDouble();
            else if (v.isString()) {
                bool ok=false; const double tmp = v.toString().toDouble(&ok); if (ok) spd = tmp;
            }
        }

        // Yaw diff
        if (!std::isnan(yaw)) {
            pushBounded(st.yawHist, yaw, 10);
            if (!st.yawHist.empty()) {
                const double prev = st.yawHist.size() > 1 ? st.yawHist[st.yawHist.size()-2] : yaw;
                double diff = qAbs(yaw - prev); if (diff > 180.0) diff = 360.0 - diff;
                pushBounded(st.yawDiffHist, diff, 10);
            }
            st.lastYaw = yaw;
        }
        // Alt diff
        if (!std::isnan(alt)) {
            pushBounded(st.altHist, alt, 10);
            if (st.altHist.size() > 1) pushBounded(st.altDiffHist, qAbs(st.altHist.back() - st.altHist[st.altHist.size()-2]), 10);
            st.lastAlt = alt;
        }
        // Speed diff
        if (!std::isnan(spd)) {
            // önceki hızı tut
            st.prevSpeed = st.lastSpeed;
            st.lastSpeed = spd;
            st.speedSampleCount++;
            pushBounded(st.speedHist, spd, 10);
            if (!std::isnan(st.prevSpeed)) {
                pushBounded(st.speedDiffHist, qAbs(st.lastSpeed - st.prevSpeed), 10);
            }
        }
        // Rota karması (excess): toplam poligon - düz mesafe
        if (!std::isnan(lat) && !std::isnan(lon)) {
            pushBounded(st.routeHist, QPointF(lon, lat), 10);
        }

        teams_.insert(team, st);
    }

    // Head-on/closing temel tespiti için ayrık tarama (MY_TEAM bilinmiyor; basit head-on seti çıkaralım)
    const double HEADON_ANGLE = 12.0;
    const double RANGE_HEADON_M = 80.0;
    const double ALT_THR = 20.0;

    // Skor hesapla ve tabloyu doldur
    for (auto it = teams_.begin(); it != teams_.end(); ++it) {
        const int id = it.key();
        const TeamState& st = it.value();

        auto meanOf = [](const std::deque<double>& dq){
            if (dq.empty()) return 0.0; double s=0; for (double v: dq) s+=v; return s/dq.size();
        };
        const double yawVar = meanOf(st.yawDiffHist);
        const double altVar = meanOf(st.altDiffHist);
        const double spdVar = meanOf(st.speedDiffHist);

        // route excess
        double total = 0.0;
        for (int i=1;i<st.routeHist.size();++i) {
            const QPointF a = st.routeHist[i-1], b = st.routeHist[i];
            total += haversineMeters(a.y(), a.x(), b.y(), b.x());
        }
        double straight = 0.0;
        if (st.routeHist.size() >= 2) {
            straight = haversineMeters(st.routeHist.front().y(), st.routeHist.front().x(), st.routeHist.back().y(), st.routeHist.back().x());
        }
        const double routeExcess = std::max(0.0, total - straight);

        // Basit skor: düşük varyanslar + düşük excess daha iyi
        double sc = 10.0 - (yawVar/20.0) - (altVar/10.0) - (spdVar/5.0) - (routeExcess/150.0);
        sc = clamp(sc, 0.0, 10.0);

        Row row{ id, sc, yawVar, altVar, spdVar, routeExcess, false, false };
        rows.push_back(row);
    }

    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b){ return a.score > b.score; });

    // Çıktıyı üret
    QString out;
    out += QString::fromUtf8("Takımlar:\n");
    for (int i=0;i<rows.size();++i) {
        const Row& r = rows[i];
        const TeamState st2 = teams_.value(r.id);
        const bool hasSpdVar = (st2.speedDiffHist.size() >= 1);
        // En son delta değerini al (ortalama yerine)
        const double lastSpdDelta = hasSpdVar ? st2.speedDiffHist.back() : 0.0;
        const QString spdDeltaStr = hasSpdVar ? QString::number(lastSpdDelta, 'f', 1) : QString::fromUtf8("N/A");
        const QString lastSpdStr = std::isnan(st2.lastSpeed) ? QString::fromUtf8("—") : QString::number(st2.lastSpeed, 'f', 1);
        out += QString::fromUtf8("%1) Takım %2  | Skor: %3  | hiz=%4 m/s  | yawΔ≈%5  altΔ≈%6  excess≈%7m\n")
                   .arg(i+1)
                   .arg(r.id)
                   .arg(QString::number(r.score, 'f', 2))
                   .arg(lastSpdStr)
                   .arg(QString::number(r.yawVar, 'f', 1))
                   .arg(QString::number(r.altVar, 'f', 1))
                   .arg(QString::number(r.routeExcess, 'f', 0));
    }

    if (rows.isEmpty()) {
        out = QString::fromUtf8("Rakip analizi: geçerli veri yok");
    }

    emit analysisReady(out);
}

// ==== Yardımcılar ====
double RakipAnaliz::clamp(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
double RakipAnaliz::deg2rad(double d){ return d * M_PI / 180.0; }
double RakipAnaliz::rad2deg(double r){ return r * 180.0 / M_PI; }
double RakipAnaliz::haversineMeters(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371000.0;
    const double dlat = deg2rad(lat2 - lat1);
    const double dlon = deg2rad(lon2 - lon1);
    const double a = qSin(dlat/2)*qSin(dlat/2) + qCos(deg2rad(lat1))*qCos(deg2rad(lat2))*qSin(dlon/2)*qSin(dlon/2);
    const double c = 2 * qAtan2(qSqrt(a), qSqrt(1-a));
    return R * c;
}
double RakipAnaliz::bearingDeg(double lat1, double lon1, double lat2, double lon2) {
    const double y = qSin(deg2rad(lon2 - lon1)) * qCos(deg2rad(lat2));
    const double x = qCos(deg2rad(lat1))*qSin(deg2rad(lat2)) - qSin(deg2rad(lat1))*qCos(deg2rad(lat2))*qCos(deg2rad(lon2 - lon1));
    double th = qAtan2(y, x) * 180.0 / M_PI;
    if (th < 0) th += 360.0; return th;
}
double RakipAnaliz::angleDiff(double a, double b) {
    double d = fmod((a - b + 180.0), 360.0); if (d < 0) d += 360.0; return qAbs(d - 180.0);
}


