#ifndef TELEMETRYBRIDGE_H
#define TELEMETRYBRIDGE_H

#include <QObject>
#include <ros/ros.h>
#include <sensor_msgs/NavSatFix.h>
#include <sensor_msgs/Imu.h>
#include <sensor_msgs/BatteryState.h>

#include <sensor_msgs/TimeReference.h>
#include <geometry_msgs/TwistStamped.h>
#include <std_msgs/Float64.h>
#include <nlohmann/json.hpp>
#include <QTimer>
#include <QNetworkAccessManager>
#include <string>

struct TelemetryData {
    double lat = 0;
    double lon = 0;
    double alt = 0;
    double rel_alt = 0;  // AGL - Above Ground Level irtifa (metre)
    double roll = 0;
    double pitch = 0;
    double yaw = 0;
    double speed = 0;
    double battery = 0;  // Batarya yüzdesi (%)
    double battery_voltage = 0;  // Batarya voltajı (V)
    // Hedef yönelme için yeni alanlar (tunga1'deki gibi)
    double target_lat = 0;  // Hedef enlem
    double target_lon = 0;  // Hedef boylam
    double target_bearing = 0; // Hedef bearing açısı
    double yaw_difference = 0; // Yaw farkı (hedef - mevcut)
    // Hedef tespit için yeni alanlar
    double hedef_merkez_X = 0;  // Hedef merkez X koordinatı
    double hedef_merkez_Y = 0;  // Hedef merkez Y koordinatı
    double hedef_genislik = 0;  // Hedef genişliği
    double hedef_yukseklik = 0; // Hedef yüksekliği
};

class TelemetryBridge : public QObject {
    Q_OBJECT

public:
    explicit TelemetryBridge(QObject *parent = nullptr);
    void start(const std::string &serverUrl, const std::string &username, const std::string &password, const std::string &ns = "/mavros");
    void startWithToken(const std::string &serverUrl, const std::string &token, const std::string &ns = "/mavros");
    void startWithCookie(const std::string &serverUrl, const std::string &ns = "/mavros");
    void startRosSubscribers(const std::string &ns = "/mavros"); // Sadece ROS subscriber'ları başlat (mavros node'u ile uyumlu)
    void stopTimer();
    TelemetryData currentData() const;
    void setTargetCoordinates(double lat, double lon); // Hedef koordinatlarını ayarla
    void setTeamNumber(int teamNo); // Takım numarasını dışarıdan ayarla
    int teamNumber() const { return team_number; }
    void setCookieJar(QNetworkCookieJar* jar); // Cookie jar'ı dışarıdan set et
    // Kilitlenme durumu
    void setLockActive(bool active) { lock_active = active; }
    bool isLockActive() const { return lock_active; }
    // Otonom durum (panel ile aynı değeri göndermek için)
    void setAutonomousFlag(int v) { autonomous_flag = v ? 1 : 0; }
    // Otonom durum kontrolü için callback fonksiyonu
    void setAutonomousModeCallback(std::function<bool()> callback) { autonomousModeCallback = callback; }
    // Sunucu-saati ofsetini (ms) ayarla; gps_saati bu ofsete göre kalibre edilir
    void setServerOffsetMs(qint64 offsetMs) { server_offset_ms = offsetMs; }
    
    // Hedef tespit değerlerini ayarlama fonksiyonları
    void setTargetDetection(double x, double y, double width, double height);
    void setTargetCenter(double x, double y);
    void setTargetSize(double width, double height);

signals:
    void telemetrySent(QString response); // GUI'ye gösterim için
    void telemetryDataUpdated(QString telemetryData); // Kendi telemetri verilerimiz için
    void telemetryUpdated(double lat, double lon, double alt, double roll, double pitch, double yaw, double speed, double battery); // Telemetri güncellemesi için yeni signal
    void mapPositionUpdated(double latitude, double longitude, double yaw); // Harita güncellemesi için yeni signal
    void loginFailed();
    void loginSuccess(int teamNo);

private:
    ros::NodeHandle nh;
    ros::Subscriber sub_gps, sub_imu, sub_vel, sub_battery, sub_rel_alt, sub_time_ref, sub_compass;
    QTimer *timer;
    QNetworkAccessManager *networkManager;
    std::string telemUrl;
    std::string cookieFile = "/tmp/cookies.txt";
    int team_number = 0;
    // authToken kaldırıldı, cookie kullanılıyor
    TelemetryData telemetry;
    bool lock_active = false; // iha_kilitlenme durumu
    int autonomous_flag = 0;  // iha_otonom (panel ile senkron)
    std::function<bool()> autonomousModeCallback = nullptr; // Otonom durum kontrolü için callback

    bool login(const std::string &url, const std::string &username, const std::string &password);
    void sendTelemetry(const std::string &url);
    void gpsCallback(const sensor_msgs::NavSatFix::ConstPtr &msg);
    void imuCallback(const sensor_msgs::Imu::ConstPtr &msg);
    void velCallback(const geometry_msgs::TwistStamped::ConstPtr &msg);
    void batteryCallback(const sensor_msgs::BatteryState::ConstPtr &msg);
    void compassCallback(const std_msgs::Float64::ConstPtr &msg);
    void relAltCallback(const std_msgs::Float64::ConstPtr &msg);
    void timeRefCallback(const sensor_msgs::TimeReference::ConstPtr &msg);
    double calculateBearing(double lat1, double lon1, double lat2, double lon2);
    void updateTargetBearing();
    
    // Batarya yüzdesi hesaplama fonksiyonu
    double batteryPercentage(double voltage, double v_min = 22.2, double v_max = 25.2);

    // MAVROS time_reference saklama
    bool has_time_ref = false;
    QDateTime last_time_ref_dt;
    qint64 server_offset_ms = 0; // Sunucu saatine göre kalibrasyon ofseti (ms)
};

#endif // TELEMETRYBRIDGE_H
