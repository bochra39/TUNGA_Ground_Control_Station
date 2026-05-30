#include "telemetrybridge.h"
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <tf2/LinearMath/Quaternion.h>
#include <tf2/LinearMath/Matrix3x3.h>
#include <cmath>



TelemetryBridge::TelemetryBridge(QObject *parent) : QObject(parent) {
    timer = new QTimer(this);
    networkManager = new QNetworkAccessManager(this);
    
    // Başlangıç değerlerini ayarla
    telemetry.battery = 0.0; // Başlangıçta batarya yüzdesi 0
    telemetry.battery_voltage = 0.0; // Başlangıçta batarya voltajı 0
    
    // Hedef tespit değerleri başlangıçta 0
    telemetry.hedef_merkez_X = 0.0;
    telemetry.hedef_merkez_Y = 0.0;
    telemetry.hedef_genislik = 0.0;
    telemetry.hedef_yukseklik = 0.0;
    
    qDebug() << "=== TelemetryBridge Constructor ===";
    qDebug() << "Initial battery voltage:" << telemetry.battery_voltage << "V";
    qDebug() << "Initial battery percentage:" << telemetry.battery << "%";
    qDebug() << "Initial target detection values: X=" << telemetry.hedef_merkez_X << ", Y=" << telemetry.hedef_merkez_Y << ", W=" << telemetry.hedef_genislik << ", H=" << telemetry.hedef_yukseklik;
    
    // Cookie jar dışarıdan set edilecek
    connect(timer, &QTimer::timeout, this, [this]() {
        // Sadece sunucuya gönder - panelde gösterme
        if (!telemUrl.empty()) {
            sendTelemetry(telemUrl);
        }
    });
}

void TelemetryBridge::start(const std::string &serverUrl, const std::string &username, const std::string &password, const std::string &ns) {
    qDebug() << "=== TelemetryBridge::start çağrıldı ===";
    qDebug() << "Server URL:" << QString::fromStdString(serverUrl);
    qDebug() << "Username:" << QString::fromStdString(username);
    qDebug() << "Password:" << QString::fromStdString(password);
    qDebug() << "Namespace:" << QString::fromStdString(ns);
    
    // Login işlemi
    if (!login(serverUrl, username, password)) {
        qDebug() << "TelemetryBridge login başarısız!";
        emit loginFailed();
        return;
    }
    
    qDebug() << "TelemetryBridge login başarılı! Takım no:" << team_number;
    emit loginSuccess(team_number);
    
    // Telemetri URL'ini ayarla
    telemUrl = serverUrl + "/api/telemetri_gonder";
    qDebug() << "Telemetri URL:" << QString::fromStdString(telemUrl);
    
    // ROS subscriber'ları başlat
    startRosSubscribers(ns);
    
    // Timer'ı başlat (2 Hz telemetri gönder)
    timer->start(500);
    
    qDebug() << "TelemetryBridge başlatıldı - Server:" << QString::fromStdString(serverUrl);
}

void TelemetryBridge::startWithToken(const std::string &serverUrl, const std::string &token, const std::string &ns) {
    qDebug() << "=== TelemetryBridge::startWithToken çağrıldı ===";
    qDebug() << "Server URL:" << QString::fromStdString(serverUrl);
    qDebug() << "Token:" << QString::fromStdString(token);
    qDebug() << "Namespace:" << QString::fromStdString(ns);
    
    // Token kaldırıldı, cookie kullanılıyor
    
    qDebug() << "TelemetryBridge token ile başlatıldı! Takım no:" << team_number;
    emit loginSuccess(team_number);
    
    // Telemetri URL'ini ayarla
    telemUrl = serverUrl + "/api/telemetri_gonder";
    qDebug() << "Telemetri URL:" << QString::fromStdString(telemUrl);
    
    // ROS subscriber'ları başlat
    startRosSubscribers(ns);
    
    // Timer'ı başlat (2 Hz telemetri gönder)
    timer->start(500);
    
    qDebug() << "TelemetryBridge token ile başlatıldı - Server:" << QString::fromStdString(serverUrl);
}

void TelemetryBridge::startWithCookie(const std::string &serverUrl, const std::string &ns) {
    qDebug() << "=== TelemetryBridge::startWithCookie çağrıldı ===";
    qDebug() << "Server URL:" << QString::fromStdString(serverUrl);
    qDebug() << "Namespace:" << QString::fromStdString(ns);
    
    // Cookie kullanılıyor, token gerekmez
    
    qDebug() << "TelemetryBridge cookie ile başlatıldı! Takım no:" << team_number;
    emit loginSuccess(team_number);
    
    // Telemetri URL'ini ayarla
    telemUrl = serverUrl + "/api/telemetri_gonder";
    qDebug() << "Telemetri URL:" << QString::fromStdString(telemUrl);
    
    // ROS subscriber'ları başlat
    startRosSubscribers(ns);
    
    // Timer'ı başlat (2 Hz telemetri gönder)
    timer->start(500);
    
    qDebug() << "TelemetryBridge cookie ile başlatıldı - Server:" << QString::fromStdString(serverUrl);
}

void TelemetryBridge::setCookieJar(QNetworkCookieJar* jar) {
    if (jar && networkManager) {
        networkManager->setCookieJar(jar);
        qDebug() << "TelemetryBridge cookie jar ayarlandı";
    }
}

void TelemetryBridge::startRosSubscribers(const std::string &ns) {
    qDebug() << "=== TelemetryBridge::startRosSubscribers çağrıldı ===";
    qDebug() << "Namespace:" << QString::fromStdString(ns);
    
    // ROS subscriber'ları ayarla
    QString gpsTopic = QString::fromStdString(ns + "/global_position/global");
    QString imuTopic = QString::fromStdString(ns + "/imu/data");
    QString velTopic = QString::fromStdString(ns + "/global_position/raw/gps_vel");
    QString batteryTopic = QString::fromStdString(ns + "/battery");
    QString compassTopic = QString::fromStdString(ns + "/global_position/compass_hdg");
    QString relAltTopic = QString::fromStdString(ns + "/global_position/rel_alt");
    
    qDebug() << "ROS Topics:";
    qDebug() << "  GPS:" << gpsTopic;
    qDebug() << "  IMU:" << imuTopic;
    qDebug() << "  VEL:" << velTopic;
    qDebug() << "  Battery:" << batteryTopic;
    qDebug() << "  Compass:" << compassTopic;
    qDebug() << "  Rel Alt (AGL):" << relAltTopic;
    
    sub_gps = nh.subscribe(ns + "/global_position/global", 1, &TelemetryBridge::gpsCallback, this);
    sub_imu = nh.subscribe(ns + "/imu/data", 1, &TelemetryBridge::imuCallback, this);
    sub_vel = nh.subscribe(ns + "/global_position/raw/gps_vel", 1, &TelemetryBridge::velCallback, this);
    sub_battery = nh.subscribe(ns + "/battery", 1, &TelemetryBridge::batteryCallback, this);
    sub_compass = nh.subscribe(ns + "/global_position/compass_hdg", 1, &TelemetryBridge::compassCallback, this);
    sub_rel_alt = nh.subscribe(ns + "/global_position/rel_alt", 1, &TelemetryBridge::relAltCallback, this);
    sub_time_ref = nh.subscribe(ns + "/time_reference", 1, &TelemetryBridge::timeRefCallback, this);
    
    qDebug() << "TelemetryBridge ROS subscriber'ları başlatıldı - Namespace:" << QString::fromStdString(ns);
}

void TelemetryBridge::setTeamNumber(int teamNo) {
    team_number = teamNo;
    qDebug() << "TelemetryBridge takım numarası ayarlandı:" << team_number;
}

void TelemetryBridge::setTargetCoordinates(double lat, double lon) {
    telemetry.target_lat = lat;
    telemetry.target_lon = lon;
    qDebug() << "TelemetryBridge hedef koordinatları ayarlandı:" << lat << lon;
}

void TelemetryBridge::stopTimer() {
    if (timer) {
        timer->stop();
        qDebug() << "TelemetryBridge timer durduruldu";
    }
}

TelemetryData TelemetryBridge::currentData() const {
    return telemetry;
}

bool TelemetryBridge::login(const std::string &url, const std::string &username, const std::string &password) {
    qDebug() << "=== TelemetryBridge::login çağrıldı ===";
    qDebug() << "URL:" << QString::fromStdString(url);
    qDebug() << "Username:" << QString::fromStdString(username);
    
    QNetworkRequest request;
    
    // URL'ye http:// protokolünü ekle
    QString urlStr = QString::fromStdString(url);
    if (!urlStr.startsWith("http://") && !urlStr.startsWith("https://")) {
        urlStr = "http://" + urlStr;
    }
    QString fullUrl = urlStr + "/api/giris";
    request.setUrl(QUrl(fullUrl));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    qDebug() << "Full URL:" << fullUrl;
    
    QJsonObject loginData;
    loginData["kadi"] = QString::fromStdString(username);
    loginData["sifre"] = QString::fromStdString(password);
    
    QJsonDocument doc(loginData);
    QByteArray data = doc.toJson();
    
    qDebug() << "Login data:" << QString::fromUtf8(data);
    
    QNetworkReply *reply = networkManager->post(request, data);
    
    // Senkron bekleme (basitlik için)
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    
    qDebug() << "Network reply error:" << reply->error();
    qDebug() << "Network reply error string:" << reply->errorString();
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response = reply->readAll();
        qDebug() << "Response:" << QString::fromUtf8(response);
        
        QJsonDocument responseDoc = QJsonDocument::fromJson(response);
        QJsonObject responseObj = responseDoc.object();
        
        qDebug() << "Response object keys:" << responseObj.keys();
        
        if (responseObj.contains("message")) {
            QString message = responseObj["message"].toString();
            qDebug() << "Message:" << message;
            qDebug() << "Contains 'başarılı':" << message.contains("başarılı");
            
            if (message.contains("başarılı")) {
                team_number = responseObj["takim_numarasi"].toInt();
                qDebug() << "Team number:" << team_number;
                qDebug() << "Cookie kullanılıyor, token gerekmez";
                
                reply->deleteLater();
                return true;
            }
        }
    }
    
    qDebug() << "Login failed!";
    reply->deleteLater();
    return false;
}

void TelemetryBridge::sendTelemetry(const std::string &url) {
    QNetworkRequest request;
    
    // URL'ye http:// protokolünü ekle
    QString urlStr = QString::fromStdString(url);
    if (!urlStr.startsWith("http://") && !urlStr.startsWith("https://")) {
        urlStr = "http://" + urlStr;
    }
    request.setUrl(QUrl(urlStr));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    // Cookie otomatik olarak gönderiliyor
    qDebug() << "Cookie otomatik olarak gönderiliyor";
    
    // GPS saatini al ve sunucu ofsetini uygula (+1s dahil offset parametresinde)
    QDateTime currentTime = has_time_ref ? last_time_ref_dt : QDateTime::currentDateTime();
    if (server_offset_ms != 0) {
        currentTime = currentTime.addMSecs(server_offset_ms);
    }
    QJsonObject gpsTime;
    gpsTime["saat"] = currentTime.time().hour();
    gpsTime["dakika"] = currentTime.time().minute();
    gpsTime["saniye"] = currentTime.time().second();
    gpsTime["milisaniye"] = currentTime.time().msec();
    
    // Manuel JSON oluştur - gps_saati en altta olsun
    
    // Veri kontrolü
    if (team_number <= 0) {
        qDebug() << "HATA: Takım numarası geçersiz:" << team_number;
        return;
    }
    
    if (telemetry.lat == 0.0 && telemetry.lon == 0.0) {
        qDebug() << "HATA: GPS koordinatları geçersiz:" << telemetry.lat << telemetry.lon;
        return;
    }
    
    QString jsonString = "{\n";
    jsonString += "    \"takim_numarasi\": " + QString::number(team_number) + ",\n";
    jsonString += "    \"iha_enlem\": " + QString::number(telemetry.lat, 'f', 7) + ",\n";
    jsonString += "    \"iha_boylam\": " + QString::number(telemetry.lon, 'f', 7) + ",\n";
    jsonString += "    \"iha_irtifa\": " + QString::number((int)telemetry.rel_alt) + ",\n";
    jsonString += "    \"iha_dikilme\": " + QString::number((int)(-telemetry.pitch)) + ",\n";
    jsonString += "    \"iha_yonelme\": " + QString::number((int)telemetry.yaw) + ",\n";
    jsonString += "    \"iha_yatis\": " + QString::number((int)telemetry.roll) + ",\n";
    jsonString += "    \"iha_hiz\": " + QString::number((int)telemetry.speed) + ",\n";
    // Bataryayı yüzdeye çevir (22.2V-25.2V aralığına göre)
    qDebug() << "=== SendTelemetry Battery Debug ===";
    qDebug() << "Battery voltage:" << telemetry.battery_voltage << "V";
    
    int batteryPerc = static_cast<int>(std::round(telemetry.battery));
    qDebug() << "Rounded battery percentage:" << batteryPerc << "%";
    qDebug() << "JSON field: iha_batarya = " << batteryPerc;
    
    jsonString += "    \"iha_batarya\": " + QString::number(batteryPerc) + ",\n";
    // iha_otonom: Gerçek zamanlı olarak callback fonksiyonundan kontrol et
    int currentOtonom = autonomous_flag; // Varsayılan değer
    if (autonomousModeCallback) {
        currentOtonom = autonomousModeCallback() ? 1 : 0;
    }
    jsonString += "    \"iha_otonom\": " + QString::number(currentOtonom) + ",\n";
    int kilit = lock_active ? 1 : 0;
    jsonString += "    \"iha_kilitlenme\": " + QString::number(kilit) + ",\n";
    jsonString += "    \"hedef_merkez_X\": " + QString::number((int)telemetry.hedef_merkez_X) + ",\n";
    jsonString += "    \"hedef_merkez_Y\": " + QString::number((int)telemetry.hedef_merkez_Y) + ",\n";
    jsonString += "    \"hedef_genislik\": " + QString::number((int)telemetry.hedef_genislik) + ",\n";
    jsonString += "    \"hedef_yukseklik\": " + QString::number((int)telemetry.hedef_yukseklik) + ",\n";
    jsonString += "    \"gps_saati\": {\n";
    jsonString += "        \"saat\": " + QString::number(gpsTime["saat"].toInt()) + ",\n";
    jsonString += "        \"dakika\": " + QString::number(gpsTime["dakika"].toInt()) + ",\n";
    jsonString += "        \"saniye\": " + QString::number(gpsTime["saniye"].toInt()) + ",\n";
    jsonString += "        \"milisaniye\": " + QString::number(gpsTime["milisaniye"].toInt()) + "\n";
    jsonString += "    }\n";
    jsonString += "}";
    
    QByteArray data = jsonString.toUtf8();
    
    QNetworkReply *reply = networkManager->post(request, data);
    
    // Manuel timeout (5 saniye)
    QTimer::singleShot(5000, reply, [reply]() {
        if (reply->isRunning()) {
            reply->abort();
        }
    });
    
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray response = reply->readAll();
            emit telemetrySent(QString::fromUtf8(response));
        } else {
            QByteArray errorResponse = reply->readAll();
            emit telemetrySent("Hata: " + reply->errorString());
        }
        reply->deleteLater();
    });
    
    // Hata durumlarını da yakala
    #if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        connect(reply, &QNetworkReply::errorOccurred, this, [this, reply](QNetworkReply::NetworkError) {
#else
        connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::error), this, [this, reply](QNetworkReply::NetworkError) {
#endif
        emit telemetrySent("Network Error: " + reply->errorString());
    });
}

void TelemetryBridge::timeRefCallback(const sensor_msgs::TimeReference::ConstPtr &msg) {
    const ros::Time &t = msg->time_ref;
    if (t.isZero()) {
        has_time_ref = false;
        return;
    }
    qint64 msec = static_cast<qint64>(t.sec) * 1000 + static_cast<qint64>(t.nsec) / 1000000;
    last_time_ref_dt = QDateTime::fromMSecsSinceEpoch(msec);
    has_time_ref = true;
}

void TelemetryBridge::gpsCallback(const sensor_msgs::NavSatFix::ConstPtr &msg) {
    telemetry.lat = msg->latitude;
    telemetry.lon = msg->longitude;
    // telemetry.alt = msg->altitude;  // Artık MSL irtifayı kullanmıyoruz, AGL kullanıyoruz
    // Hedef bearing'i güncelle
    updateTargetBearing();
    
    // Harita güncellemesi için signal gönder (iha.png için)
    emit mapPositionUpdated(telemetry.lat, telemetry.lon, telemetry.yaw);
    
    // Telemetri güncellemesi için signal gönder (AGL irtifayı kullan)
    emit telemetryUpdated(telemetry.lat, telemetry.lon, telemetry.rel_alt, telemetry.roll, telemetry.pitch, telemetry.yaw, telemetry.speed, telemetry.battery);
}

void TelemetryBridge::imuCallback(const sensor_msgs::Imu::ConstPtr &msg) {
    // Quaternion'dan Euler açılarına çevir
    tf2::Quaternion q(msg->orientation.x, msg->orientation.y, msg->orientation.z, msg->orientation.w);
    tf2::Matrix3x3 m(q);
    double roll, pitch, yaw;
    m.getRPY(roll, pitch, yaw);
    
    telemetry.roll = roll * 180.0 / M_PI;  // Radyan'dan dereceye çevir
    telemetry.pitch = pitch * 180.0 / M_PI;
    
    // Yaw artık compass topic'inden alınıyor, burada hesaplamıyoruz
    
    // Telemetri güncellemesi için signal gönder (AGL irtifayı kullan)
    emit telemetryUpdated(telemetry.lat, telemetry.lon, telemetry.rel_alt, telemetry.roll, telemetry.pitch, telemetry.yaw, telemetry.speed, telemetry.battery);
}

void TelemetryBridge::velCallback(const geometry_msgs::TwistStamped::ConstPtr &msg) {
    // Hızı m/s cinsinden al
    telemetry.speed = sqrt(pow(msg->twist.linear.x, 2) + 
                          pow(msg->twist.linear.y, 2) + 
                          pow(msg->twist.linear.z, 2));
    
    // Telemetri güncellemesi için signal gönder (AGL irtifayı kullan)
    emit telemetryUpdated(telemetry.lat, telemetry.lon, telemetry.rel_alt, telemetry.roll, telemetry.pitch, telemetry.yaw, telemetry.speed, telemetry.battery);
}

void TelemetryBridge::batteryCallback(const sensor_msgs::BatteryState::ConstPtr &msg) {
    // Voltaj değerini al (V cinsinden)
    telemetry.battery_voltage = msg->voltage;
    
    // Direkt percentage değerini al (0.0-1.0 aralığında)
    if (msg->percentage > 0) {
        telemetry.battery = msg->percentage * 100.0; // Yüzdeye çevir
    } else {
        // Eğer percentage yoksa voltajdan hesapla
        telemetry.battery = batteryPercentage(telemetry.battery_voltage, 22.2, 25.2);
    }
    
    // Debug: Voltaj ve yüzde değerlerini logla
    qDebug() << "=== Battery Callback ===";
    qDebug() << "Raw voltage from topic:" << msg->voltage << "V";
    qDebug() << "Raw percentage from topic:" << msg->percentage;
    qDebug() << "Stored voltage:" << telemetry.battery_voltage << "V";
    qDebug() << "Calculated percentage:" << telemetry.battery << "%";
    qDebug() << "Topic name: /mavros/battery";
    
    // Telemetri güncellemesi için signal gönder (AGL irtifayı kullan)
    emit telemetryUpdated(telemetry.lat, telemetry.lon, telemetry.rel_alt, telemetry.roll, telemetry.pitch, telemetry.yaw, telemetry.speed, telemetry.battery);
}

void TelemetryBridge::compassCallback(const std_msgs::Float64::ConstPtr &msg) {
    // Pusula yönelimini al (derece cinsinden, 0-360)
    telemetry.yaw = msg->data;
    
    // 0-360 aralığına normalize et
    telemetry.yaw = fmod(telemetry.yaw + 360.0, 360.0);
    
    // Debug: Pusula yönelimini logla
    qDebug() << "=== Compass Callback ===";
    qDebug() << "Raw compass heading:" << msg->data << "°";
    qDebug() << "Normalized yaw:" << telemetry.yaw << "°";
    qDebug() << "Topic name: /mavros/global_position/compass_hdg";
    
    // Hedef bearing'i güncelle
    updateTargetBearing();
    
    // Harita güncellemesi için signal gönder
    emit mapPositionUpdated(telemetry.lat, telemetry.lon, telemetry.yaw);
    
    // Telemetri güncellemesi için signal gönder
    emit telemetryUpdated(telemetry.lat, telemetry.lon, telemetry.rel_alt, telemetry.roll, telemetry.pitch, telemetry.yaw, telemetry.speed, telemetry.battery);
}



void TelemetryBridge::relAltCallback(const std_msgs::Float64::ConstPtr &msg) {
    // AGL (Above Ground Level) irtifa bilgisini al (metre cinsinden)
    telemetry.rel_alt = msg->data;
    
    // Telemetri güncellemesi için signal gönder (AGL irtifayı kullan)
    emit telemetryUpdated(telemetry.lat, telemetry.lon, telemetry.rel_alt, telemetry.roll, telemetry.pitch, telemetry.yaw, telemetry.speed, telemetry.battery);
}

double TelemetryBridge::calculateBearing(double lat1, double lon1, double lat2, double lon2) {
    // Dereceyi radyana çevir
    lat1 = lat1 * M_PI / 180.0;
    lon1 = lon1 * M_PI / 180.0;
    lat2 = lat2 * M_PI / 180.0;
    lon2 = lon2 * M_PI / 180.0;
    
    double dLon = lon2 - lon1;
    
    double y = sin(dLon) * cos(lat2);
    double x = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(dLon);
    double bearing = atan2(y, x); // Radyan cinsinden
    bearing = fmod((bearing * 180.0 / M_PI + 360.0), 360.0); // Dereceye çevir
    
    return bearing;
}

void TelemetryBridge::updateTargetBearing() {
    if (telemetry.target_lat != 0 && telemetry.target_lon != 0) {
        // Hedef bearing açısını hesapla
        telemetry.target_bearing = calculateBearing(telemetry.lat, telemetry.lon, 
                                                   telemetry.target_lat, telemetry.target_lon);
        
        // Yaw farkını hesapla ve normalize et (-180 ile +180 arasına)
        telemetry.yaw_difference = telemetry.target_bearing - telemetry.yaw;
        
        // Yaw farkını normalize et
        if (telemetry.yaw_difference > 180) {
            telemetry.yaw_difference -= 360;
        } else if (telemetry.yaw_difference < -180) {
            telemetry.yaw_difference += 360;
        }
        
        qDebug() << "=== Target Bearing Update ===";
        qDebug() << "Current Yaw:" << telemetry.yaw;
        qDebug() << "Target Bearing:" << telemetry.target_bearing;
        qDebug() << "Yaw Difference:" << telemetry.yaw_difference;
    }
}

double TelemetryBridge::batteryPercentage(double voltage, double v_min, double v_max) {
    // Voltajı verilen aralıkta sınırla
    if (voltage > v_max) voltage = v_max;
    if (voltage < v_min) voltage = v_min;

    // Lineer oranlama
    double perc = (voltage - v_min) / (v_max - v_min) * 100.0;

    return perc;
}

void TelemetryBridge::setTargetDetection(double x, double y, double width, double height) {
    telemetry.hedef_merkez_X = x;
    telemetry.hedef_merkez_Y = y;
    telemetry.hedef_genislik = width;
    telemetry.hedef_yukseklik = height;
    
    qDebug() << "=== Target Detection Updated ===";
    qDebug() << "Center X:" << x << ", Y:" << y;
    qDebug() << "Size W:" << width << ", H:" << height;
}

void TelemetryBridge::setTargetCenter(double x, double y) {
    telemetry.hedef_merkez_X = x;
    telemetry.hedef_merkez_Y = y;
    
    qDebug() << "=== Target Center Updated ===";
    qDebug() << "Center X:" << x << ", Y:" << y;
}

void TelemetryBridge::setTargetSize(double width, double height) {
    telemetry.hedef_genislik = width;
    telemetry.hedef_yukseklik = height;
    
    qDebug() << "=== Target Size Updated ===";
    qDebug() << "Size W:" << width << ", H:" << height;
}
