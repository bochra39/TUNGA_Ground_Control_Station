#include "RivalRosSubscriber.h"
#include <QString>
#include <QVector>

RivalRosSubscriber::RivalRosSubscriber(QObject* parent) : QObject(parent) {}

void RivalRosSubscriber::start(ros::NodeHandle& nh,
                               const std::string& json_topic,
                               const std::string& poses_topic,
                               const std::string& ids_topic)
{
    sub_json_  = nh.subscribe(json_topic,  10, &RivalRosSubscriber::onJson,  this);
    sub_poses_ = nh.subscribe(poses_topic, 10, &RivalRosSubscriber::onPoses, this);
    sub_ids_   = nh.subscribe(ids_topic,   10, &RivalRosSubscriber::onIds,   this);
}

void RivalRosSubscriber::onJson(const std_msgs::String::ConstPtr& msg) {
    emit rivalsJsonReceived(QString::fromStdString(msg->data));
}

void RivalRosSubscriber::onPoses(const geometry_msgs::PoseArray::ConstPtr& msg) {
    lastPoses_ = *msg;
    maybeEmitPoses();
}

void RivalRosSubscriber::onIds(const std_msgs::Int32MultiArray::ConstPtr& msg) {
    lastIds_.assign(msg->data.begin(), msg->data.end());
    maybeEmitPoses();
}

void RivalRosSubscriber::maybeEmitPoses() {
    if (lastIds_.empty() || lastPoses_.poses.empty()) return;
    const size_t n = std::min(lastIds_.size(), lastPoses_.poses.size());

    QVector<double> lats, lons, alts, yaws;
    QVector<int> ids;
    lats.reserve(n); lons.reserve(n); alts.reserve(n); yaws.reserve(n); ids.reserve(n);

    for (size_t i=0; i<n; ++i) {
        const auto& p = lastPoses_.poses[i];
        lons.push_back(p.position.x); // yayınlayan: x=lon
        lats.push_back(p.position.y); // y=lat
        alts.push_back(p.position.z); // z=alt (m)
        yaws.push_back(0.0);          // gerekirse quat→yaw eklenir
        ids.push_back(lastIds_[i]);
    }
    emit rivalsPoseListReceived(lats, lons, alts, yaws, ids);
} 