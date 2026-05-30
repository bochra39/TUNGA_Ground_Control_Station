#pragma once
#include <ros/ros.h>
#include <std_msgs/String.h>
#include <geometry_msgs/PoseArray.h>
#include <geometry_msgs/Pose.h>
#include <std_msgs/Int32MultiArray.h>

#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

class RivalRosBridge {
public:
  RivalRosBridge() = default;

  // node adını dışarıdan verelim (örn: "rival_pub_gui")
  void startOnce(const std::string& node_name) {
    if (started_) return;
    int argc = 0; char** argv = nullptr;
    if (!ros::isInitialized()) {
      ros::init(argc, argv, node_name);
    }
    nh_ = std::make_shared<ros::NodeHandle>();
    spinner_ = std::make_shared<ros::AsyncSpinner>(1);
    spinner_->start();

    pub_json_ = nh_->advertise<std_msgs::String>("/rivals/json", 10, false);
    pub_poses_ = nh_->advertise<geometry_msgs::PoseArray>("/rivals/poses", 10, false);
    pub_ids_   = nh_->advertise<std_msgs::Int32MultiArray>("/rivals/ids", 10, false);

    started_ = true;
  }

  // Sunucudan gelen ham JSON'u aynen publish et
  void publishRawJson(const QString& raw) {
    if (!started_) return;
    std_msgs::String msg;
    msg.data = raw.toStdString();
    pub_json_.publish(msg);
  }

  // (Opsiyonel) JSON'dan PoseArray + id listesi üret ve publish et
  void publishParsed(const QString& raw) {
    if (!started_) return;

    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8(), &err);
    if (doc.isNull() || !doc.isObject()) return;

    const QJsonObject root = doc.object();
    QJsonArray arr;
    if (root.value(QStringLiteral("konumBilgileri")).isArray())
      arr = root.value(QStringLiteral("konumBilgileri")).toArray();
    else if (root.value(QStringLiteral("telemetry")).isArray())
      arr = root.value(QStringLiteral("telemetry")).toArray();
    else
      return;

    geometry_msgs::PoseArray poses;
    poses.header.stamp = ros::Time::now();
    poses.header.frame_id = "map"; // istiyorsan "world" yap

    std_msgs::Int32MultiArray ids;

    for (const QJsonValue& v : arr) {
      const QJsonObject o = v.toObject();
      const int teamNo = o.value("takim_numarasi").toInt();
      const double lat  = o.value("iha_enlem").toDouble();
      const double lon  = o.value("iha_boylam").toDouble();
      const double altm = o.value("iha_irtifa").toDouble();
      const double yawd = o.value("iha_yonelme").toDouble();

      // NOT: Burada lat/lon'u direkt metreye çevirmiyoruz (projeksiyon gerektirir).
      // Şimdilik "demo" amaçlı: x=lon, y=lat, z=alt (kilitlenme tarafında gerçek dönüşümü yaparsın).
      geometry_msgs::Pose p;
      p.position.x = lon;
      p.position.y = lat;
      p.position.z = altm;

      // yaw(deg) → quaternion (roll=pitch=0)
      double yaw = yawd * M_PI / 180.0;
      double cr = 1.0, sr = 0.0; // roll=0
      double cp = 1.0, sp = 0.0; // pitch=0
      double cy = cos(yaw * 0.5), sy = sin(yaw * 0.5);
      p.orientation.w = cr*cp*cy + sr*sp*sy;
      p.orientation.x = sr*cp*cy - cr*sp*sy;
      p.orientation.y = cr*sp*cy + sr*cp*sy;
      p.orientation.z = cr*cp*sy - sr*sp*cy;

      poses.poses.push_back(p);
      ids.data.push_back(teamNo);
    }

    pub_poses_.publish(poses);
    pub_ids_.publish(ids);
  }

private:
  bool started_ = false;
  std::shared_ptr<ros::NodeHandle> nh_;
  std::shared_ptr<ros::AsyncSpinner> spinner_;
  ros::Publisher pub_json_;
  ros::Publisher pub_poses_;
  ros::Publisher pub_ids_;
}; 