#include "ros/ros.h"
#include "mavros_msgs/RCIn.h"

void rcCallback(const mavros_msgs::RCIn::ConstPtr& msg)
{
    ROS_INFO("RC Channels:");
    for (size_t i = 0; i < msg->channels.size(); i++) {
        ROS_INFO("Channel %ld: %d", i + 1, msg->channels[i]);
    }
}

int main(int argc, char **argv)
{
    ros::init(argc, argv, "rc_listener");
    ros::NodeHandle nh;

    ros::Subscriber rc_sub = nh.subscribe("/mavros/rc/in", 10, rcCallback);

    ros::spin();

    return 0;
}
