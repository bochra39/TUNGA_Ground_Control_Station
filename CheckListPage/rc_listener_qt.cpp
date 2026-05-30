#include "rc_listener_qt.h"

RCListener::RCListener(QObject *parent) : QObject(parent) {
    int argc = 0;
    char **argv = nullptr;
    ros::init(argc, argv, "rc_listener_gui");
    nh = new ros::NodeHandle();
}

RCListener::~RCListener() {
    delete nh;
}

void RCListener::startListening() {
    if (!isListening) {
        rc_sub = nh->subscribe("/mavros/rc/in", 10, &RCListener::rcCallback, this);
        isListening = true;
    }
}

void RCListener::stopListening() {
    if (isListening) {
        rc_sub.shutdown();
        isListening = false;
    }
}

void RCListener::setLabels(QLabel* labels[8]) {
    for(int i = 0; i < 8; i++) {
        rcLabels[i] = labels[i];
    }
}

void RCListener::rcCallback(const mavros_msgs::RCIn::ConstPtr& msg) {
    // MavProxy ile RC_CHANNELS verilerini çek
    for(size_t i = 0; i < msg->channels.size() && i < 8; i++) {
        rcLabels[i]->setText(QString("Kanal %1: %2").arg(i + 1).arg(msg->channels[i]));
    }
} 