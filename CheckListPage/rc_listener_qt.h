#ifndef RC_LISTENER_QT_H
#define RC_LISTENER_QT_H

#include <QObject>
#include <ros/ros.h>
#include <mavros_msgs/RCIn.h>
#include <QLabel>

class RCListener : public QObject {
    Q_OBJECT
public:
    explicit RCListener(QObject *parent = nullptr);
    ~RCListener();

    void startListening();
    void stopListening();
    void setLabels(QLabel* labels[8]);

private Q_SLOTS:
    void rcCallback(const mavros_msgs::RCIn::ConstPtr& msg);

private:
    ros::NodeHandle* nh;
    ros::Subscriber rc_sub;
    QLabel* rcLabels[8];
    bool isListening = false;
};

#endif // RC_LISTENER_QT_H 