QT += core gui network widgets qml quick quickwidgets
CONFIG += c++17 link_pkgconfig

TARGET = LoginPage
TEMPLATE = app

# --- ROS (pkg-config üzerinden) ---
PKGCONFIG += roscpp tf
# link_pkgconfig zaten CFLAGS/LIBS ekler; yine de INCLUDEPATH/LIBS'i takviye edelim:
INCLUDEPATH += $$system(pkg-config --cflags-only-I roscpp) \
               $$system(pkg-config --cflags-only-I tf)
LIBS += $$system(pkg-config --libs roscpp) \
        $$system(pkg-config --libs tf)

# --- OpenCV ---
INCLUDEPATH += /usr/include/opencv4
LIBS += -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_videoio -lopencv_imgcodecs

SOURCES += \
    RosBridge.cpp \
    main.cpp \
    mainwindow.cpp \
    MainPageWidget.cpp \
    CameraWidget.cpp \
    checklist.cpp \
    RosIhaListener.cpp \
    HttpIhaListener.cpp \
    RosTelemetryListener.cpp \
    telemetrybridge.cpp \
    RakipIhaPoller.cpp \
    RakipAnaliz.cpp \
    RivalRosBridge.cpp \
    RivalRosSubscriber.cpp \
    ../SimulasyonPage/SimulasyonPage.cpp \


HEADERS += \
    RosBridge.h \
    mainwindow.h \
    MainPageWidget.h \
    CameraWidget.h \
    ihaModel.h \
    checklist.h \
    RosIhaListener.h \
    HttpIhaListener.h \
    RosTelemetryListener.h \
    telemetrybridge.h \
    RakipIhaPoller.h \
    RakipAnaliz.h \
    RivalRosBridge.h \
    RivalRosSubscriber.h \
    ../SimulasyonPage/SimulasyonPage.h \

FORMS += mainwindow.ui

RESOURCES += \
    icons.qrc \
    resources.qrc

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
