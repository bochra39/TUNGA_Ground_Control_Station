#pragma once
#include <QObject>
#include <QTimer>
#include <atomic>
#include <ros/ros.h>
#include <sensor_msgs/NavSatFix.h>
#include <nav_msgs/Odometry.h>
#include <mavros_msgs/State.h>
#include <mavros_msgs/SetMode.h>
#include <std_msgs/Float64.h>
#include <mavros_msgs/VFR_HUD.h>
#include <QString>

class RosBridge : public QObject {
    Q_OBJECT
public:
    explicit RosBridge(QObject* parent=nullptr, const QString& ns="");
signals:
    void posUpdated(double lat, double lon);
    void yawUpdated(double yawDeg);
    void modeUpdated(const QString& mode, bool armed);
public slots:
    bool setMode(const QString& modeText);
private:
    std::shared_ptr<ros::AsyncSpinner> spinner_;
    std::shared_ptr<ros::NodeHandle> nh_;
    ros::Subscriber sub_gps_, sub_odom_, sub_state_;
    ros::ServiceClient setmode_;
    
    // Ham ve filtreli konum
    std::atomic<double> raw_lat_{NAN}, raw_lon_{NAN};
    double filt_lat_ = NAN, filt_lon_ = NAN;
    QTimer* tick_ = nullptr;
    double alpha_ = 0.20;          // smoothing katsayısı (0.1–0.3 arası iyi)
    double max_step_m_ = 30.0;     // tek adımda izin verilen max mesafe (metre) (sıçrama filtresi)

    // Yaw/heading kaynakları ve filtre
    std::atomic<double> raw_yaw_deg_{NAN};  // 0..360 veya -180..180 gelebilir
    double filt_yaw_deg_ = NAN;
    bool have_compass_ = false;
    bool have_vfr_ = false;
    double yaw_alpha_ = 0.20; // 0.1–0.3 arası iyi
    ros::Subscriber sub_compass_, sub_vfr_;

    QString ns_; // ör: "uav0", "uav1" ...
    
    static double haversine_m(double lat1, double lon1, double lat2, double lon2);
    static double normalize360(double deg);
    static double smoothAngleDeg(double prev_deg, double meas_deg, double alpha);
    
    void gpsCb(const sensor_msgs::NavSatFix::ConstPtr& m);
    void odomCb(const nav_msgs::Odometry::ConstPtr& m);
    void stateCb(const mavros_msgs::State::ConstPtr& s);
    void compassCb(const std_msgs::Float64::ConstPtr& m);
    void vfrCb(const mavros_msgs::VFR_HUD::ConstPtr& m);
};
