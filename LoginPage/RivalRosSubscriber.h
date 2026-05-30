#pragma once
#include <QObject>
#include <ros/ros.h>
#include <std_msgs/String.h>
#include <geometry_msgs/PoseArray.h>
#include <std_msgs/Int32MultiArray.h>

class RivalRosSubscriber : public QObject {
    Q_OBJECT
public:
    explicit RivalRosSubscriber(QObject* parent=nullptr);

    void start(ros::NodeHandle& nh,
               const std::string& json_topic  = "/rivals/json",
               const std::string& poses_topic = "/rivals/poses",
               const std::string& ids_topic   = "/rivals/ids");

signals:
    void rivalsJsonReceived(const QString& rawJson);
    void rivalsPoseListReceived(const QVector<double>& lats,
                                const QVector<double>& lons,
                                const QVector<double>& alts,
                                const QVector<double>& yaws,
                                const QVector<int>&    ids);

private:
    ros::Subscriber sub_json_, sub_poses_, sub_ids_;
    geometry_msgs::PoseArray lastPoses_;
    std::vector<int> lastIds_;

    void onJson(const std_msgs::String::ConstPtr& msg);
    void onPoses(const geometry_msgs::PoseArray::ConstPtr& msg);
    void onIds(const std_msgs::Int32MultiArray::ConstPtr& msg);
    void maybeEmitPoses();
}; 