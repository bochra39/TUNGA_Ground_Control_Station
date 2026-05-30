cmake_minimum_required(VERSION 3.16)
project(GorevKontrolArayuzu LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ---- ROS'u bulmayı kolaylaştır (gerekirse)
list(APPEND CMAKE_PREFIX_PATH "/opt/ros/noetic")

# ---- ROS / catkin
find_package(catkin REQUIRED COMPONENTS
    roscpp
    sensor_msgs
    nav_msgs
    mavros_msgs
    tf
)

catkin_package()

# ---- Qt6
find_package(Qt6 REQUIRED COMPONENTS Widgets Network QuickWidgets Qml Quick)

# ---- nlohmann/json
find_package(nlohmann_json REQUIRED)

# ---- include ve tanımlar
include_directories(
    ${catkin_INCLUDE_DIRS}
)

add_definitions(${catkin_DEFINITIONS})

# ---- kaynaklar
set(SOURCES
    main.cpp
    mainwindow.cpp
    telemetrybridge.cpp
    RosTelemetryListener.cpp
    HttpIhaListener.cpp
    RosIhaListener.cpp
    MainPageWidget.cpp
    CameraWidget.cpp
    checklist.cpp
    navbar.cpp
    resources.qrc
    # --- HSS Manager removed
    
)

add_executable(${PROJECT_NAME} ${SOURCES})

# Catkin mesaj/srv hedefleriyle sıralı derleme (önerilir)
add_dependencies(${PROJECT_NAME} ${catkin_EXPORTED_TARGETS})

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core           # NEW: açıkça ekle (Widgets zaten çeker ama netlik için)
    Qt6::Gui            # (Widgets bunu da çekiyor; açık belirtmek temiz)
    Qt6::Widgets
    Qt6::Network
    Qt6::QuickWidgets
    Qt6::Qml
    Qt6::Quick
    ${catkin_LIBRARIES}
    nlohmann_json::nlohmann_json
)

