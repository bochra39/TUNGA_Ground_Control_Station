#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QList>
#include <QLineEdit>
#include <QPixmap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTextEdit>
#include <QTimer>
#include <QStackedWidget>
#include <QProcess>
#include <QTcpSocket>
#include <QJsonObject>
#include <QDateTime>
#include <QJsonArray>
#include <QElapsedTimer>
#include "RosTelemetryListener.h"
#include "telemetrybridge.h"
#include "MainPageWidget.h"
#include "RivalRosBridge.h"
#include "RivalRosSubscriber.h"
// ROS includes for kilitlenme paneli
#include <ros/ros.h>
#include <std_msgs/String.h>
#include <sensor_msgs/TimeReference.h>
#include <mavros_msgs/RCIn.h>

#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    Ui::MainWindow *ui;
    // Ana pencere ve arayüz yönetimi için widgetlar
    QFrame *topBar;
    QHBoxLayout *topBarLayout;
    QList<QCheckBox*> allCheckBoxes;
    QList<QLabel*> allHeaderLabels;
    QPushButton* lightButton;
    QFrame *leftMenu;
    QVBoxLayout *leftMenuLayout;
    QList<QPushButton*> menuButtons;
    QWidget *contentWidget;
    QHBoxLayout *contentLayout;
    QWidget *loginSection;
    QVBoxLayout *loginLayout;
    QLabel *serverLabel;
    QLineEdit *serverLineEdit;
    QLabel *userLabel;
    QLineEdit *userLineEdit;
    QLabel *passLabel;
    QLineEdit *passLineEdit;
    QPushButton *loginButton;
    QLabel *statusLabel;
    QWidget *dataSection;
    QGridLayout *dataLayout;
    QLabel *fetchLabel;
    QPushButton *fetchButton;
    QLabel *sendLabel;
    QPushButton *sendButton;
    QLabel *stopLabel;
    QPushButton *stopButton;
    QList<QLabel*> allLabels;
    bool isLightMode;
    QPixmap lightModeIcon;
    QPixmap darkModeIcon;
    QNetworkAccessManager* networkManager;
    // Token kaldırıldı, cookie kullanılıyor
    QTextEdit* telemetryDisplay;
    QLabel* serverTimeLabel;
    QLabel* dateLabel;
    QLabel *serverStatusLabel;
    QLabel *telemetryStatusLabel; // Telemetri durumu için label
    QLabel* competitionTimeLabel;
    QTimer* competitionTimer;
    int competitionSecondsLeft = 15 * 60; // 15 dakika
    QStackedWidget* stackedContent = nullptr;
    QTimer* telemetrySendTimer;
    QTimer* gpsCheckTimer; // GPS verisi kontrolü için timer
    bool isTelemetrySending = false;
    bool isTelemetryViewing = false;
    QDateTime lastGpsDataTime; // Son GPS verisi zamanı
    bool isCompetitionRunning = false;

    QJsonObject lastTelemetryObject;
    TelemetryBridge* telemetryBridge = nullptr;
    MainPageWidget* mainPageWidget = nullptr;
    QDateTime currentServerTime;
    int teamNumber = 0;
    // Sunucu saatine göre ilerleme için baz alım
    bool serverTimeInitialized = false;
    QDateTime serverBaseTime;
    QElapsedTimer serverElapsed;
    
    // ROS kilitlenme, kamikaze ve QR message subscriber'ları
    ros::Subscriber lockInfoSubscriber;
    ros::Subscriber kamikazeInfoSubscriber;
    ros::Subscriber qrMessageSubscriber;
    ros::Subscriber hedefPikselSubscriber;
    ros::Subscriber timeRefSubscriber;
    ros::Subscriber rcInSubscriber; // RSS verisi için (/mavros/rc/in)
    ros::NodeHandle* rosNodeHandle;
    ros::Publisher serverTimePublisher; // sensor_msgs::TimeReference yayınlayacak
    
    // Kilitlenme paneli widget'ları
    QLabel* lockInfoLabel;
    QLabel* kamikazeInfoLabel;
    QLabel* sessionInfoLabel;
    // İrtifa ve hız bilgisi için label'lar
    QLabel* altitudeLabel;
    QLabel* speedLabel;
    QTimer* rosSpinTimer = nullptr;
    int telemetryIndex = 0;
    // --- Eklendi: Üst bar durum label'ları ---
    QLabel* planeStatusLabel = nullptr;
    QLabel* gpsStatusLabel = nullptr;
    QLabel* powerStatusLabel = nullptr;
    QLabel* rssStatusLabel = nullptr; // RSS sinyal gücü için
    bool rosSubscribersStarted = false;
    
    RosTelemetryListener* rosListener = nullptr;
    // Rakip İHA çekme isteği için bayraklar
    bool pendingRivalsFetch = false;
    bool autoStopTelemetryAfterRivals = false;
    
    // ROS rakip İHA publisher'ı
    RivalRosBridge rivalRos_;
    // ROS rakip İHA subscriber'ı
    RivalRosSubscriber* rivalsSub_ = nullptr;
    
    // Hedef piksel verisi (panel için)
    bool hasHedefPiksel = false;
    int hedefMerkezX = 0;
    int hedefMerkezY = 0;
    int hedefGenislik = 0;
    int hedefYukseklik = 0;

    // MAVROS GPS zaman referansı
    bool hasGpsTimeRef = false;
    QDateTime lastGpsTimeRef;
    // Sunucu saatine göre gps_saati kalibrasyon ofseti (ms)
    qint64 serverOffsetMs = 0;

    // GPS saatini sunucu saatine göre kalibre etmek için yardımcı
    void updateServerGpsOffset();

    // Üst panelde gösterilen sunucu saatini ROS'a publish etmek için
    QTimer* serverTimePubTimer = nullptr;
    void publishServerTimeOnce();

private slots:
    void handleLogin();
    void fetchTelemetry();
    void fetchServerTime();
    void handleServerTimeReply(QNetworkReply* reply);
    void updateCompetitionTime();
    void onCompetitionTimeLabelClicked();
    void sendTelemetryToServer();
    void onSendButtonClicked();
    void onStopButtonClicked();
    void onFetchButtonClicked();
    void onRivalsFetchRequested();
    void onLockButtonClicked();
    
    
    // HTTP istekleri için yeni slot'lar
    void sendHttpRequest(const QString &url, const QJsonObject &data, const QString &method);
    void handleHttpResponse(QNetworkReply* reply);
    
    // Giriş işlemleri için yeni slot'lar
    void handleLoginSuccess(const QJsonObject &response);
    void handleLoginError(const QString &error);
    void updateStatusLabels(bool isConnected);
    
    // Telemetri işlemleri için yeni slot'lar
    void handleTelemetrySendSuccess(const QJsonObject &response);
    void handleTelemetrySendError(const QString &error);
    void handleTelemetryRawResponse(const QString &rawResponse);
    void onTelemetryBridgeResponse(QString response);
    
    // ROS kilitlenme, kamikaze ve QR message callback fonksiyonları
    void onLockInfoReceived(const std_msgs::String::ConstPtr& msg);
    void onKamikazeInfoReceived(const std_msgs::String::ConstPtr& msg);
    void onQrMessageReceived(const std_msgs::String::ConstPtr& msg);
    void onHedefPikselReceived(const std_msgs::String::ConstPtr& msg);
    void onTimeReference(const sensor_msgs::TimeReference::ConstPtr& msg);
    void onRcInReceived(const mavros_msgs::RCIn::ConstPtr& msg); // RSS callback
    
    // Session bilgisi güncelleme fonksiyonu
    void updateSessionInfo(const QString& sessionValue);
    
    // İrtifa ve hız güncelleme fonksiyonları
    void updateAltitude(double altitude);
    void updateSpeed(double speed);
    
    // RSS güncelleme fonksiyonu
    void updateRssStatus(int rssi, int remrssi);
    
    // Retry mekanizması
    void retryRequest(const QString &url, const QJsonObject &data, const QString &method, int retryCount = 0);

signals:
    void httpSuccess(const QJsonObject &response);
    void httpError(const QString &errorMessage);
    void httpRawResponse(const QString &rawResponse);
};
#endif // MAINWINDOW_H
