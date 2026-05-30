TUNGA Ground Control Station 
TUNGA Ground Control Station is a Qt-based ground control interface developed for the TUNGA UAV system. The interface was designed to support the operational needs of our UAV team during the Savaşan İHA competition. It provides a unified control environment for managing UAV flight modes, mission execution, lock-on operations, simulation modules, map-based monitoring, camera visualization, telemetry communication, and interaction with the competition server.

Through this interface, the team can control key UAV functions, trigger mission-related algorithms such as lock-on and autonomous task routines, receive data from the official competition server, and monitor or transmit the UAV’s own telemetry data. In this way, the system serves as a custom ground control station that brings together the essential tools required to operate, monitor, and manage the team’s UAV during competition scenarios.

Project Structure
TUNGA_Ground_Control_Station/
├── LoginPage/
├── MainPage/
├── CheckListPage/
├── SimulasyonPage/
├── osm_cache/
├── package.xml
├── telemetry_uploader.cpp
└── README.md

Technologies Used
C++
Qt 6.9.1
Qt Widgets / Qt Quick
ROS Noetic
roscpp
OpenCV
QML
Linux / Ubuntu
Requirements

This project was tested with the following environment:
Ubuntu 20.04
ROS Noetic
Qt 6.9.1
GCC / qmake

Before running the project, make sure ROS Noetic and Qt 6.9.1 are installed.

Build and Run
Use the following commands to build the project without modifying the source directory:

source /opt/ros/noetic/setup.bash
export LD_LIBRARY_PATH=/home/bushra/Qt/6.9.1/gcc_64/lib:$LD_LIBRARY_PATH
export QT_QPA_PLATFORM_PLUGIN_PATH=/home/bushra/Qt/6.9.1/gcc_64/plugins/platforms

mkdir -p /tmp/tunga_gcs_build
cd /tmp/tunga_gcs_build

/home/bushra/Qt/6.9.1/gcc_64/bin/qmake /home/bushra/TumTunga/TUNGA_Ground_Control_Station/LoginPage/LoginPage.pro
make -j$(nproc)
./LoginPage
Notes
The project should be built with Qt 6.9.1.
Qt 5 may cause compatibility issues.
Build files, object files, executable files, logs, and cache folders should not be uploaded to GitHub.
The main project entry point is located under:
LoginPage/LoginPage.pro
Project Status

This repository contains the final interface version of the TUNGA Ground Control Station project for backup, documentation, and future development.
