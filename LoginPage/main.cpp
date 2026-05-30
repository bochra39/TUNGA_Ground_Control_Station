#include "mainwindow.h"
#include <QApplication>
#include <ros/ros.h>

int main(int argc, char *argv[]) {
    // ROS node'u başlat
    ros::init(argc, argv, "gorev_kontrol_arayuzu");

    QApplication app(argc, argv);
    MainWindow w;
    w.setMinimumSize(800, 600);
    w.showMaximized();
    return app.exec();
}
