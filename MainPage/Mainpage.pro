TARGET = MainPage
QT       += core gui quickwidgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

TEMPLATE = app

# ROS bağımlılıkları
INCLUDEPATH += /opt/ros/noetic/include
LIBS += -L/opt/ros/noetic/lib -lroscpp -lrosconsole -lrostime -lroscpp_serialization -lxmlrpcpp -lcpp_common -lrosconsole_log4cxx -lrosconsole_backend_interface

# OpenCV kütüphaneleri
INCLUDEPATH += /usr/include/opencv4
LIBS += -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_videoio -lopencv_imgcodecs

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    CameraWidget.cpp

HEADERS += \
    mainwindow.h \
    CameraWidget.h \
    ihaModel.h

RESOURCES += \
    resources.qrc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
