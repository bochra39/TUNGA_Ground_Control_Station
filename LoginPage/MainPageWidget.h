#ifndef MAINPAGEWIDGET_H
#define MAINPAGEWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QPixmap>
#include <QFrame>
#include <QHBoxLayout>
#include "CameraWidget.h"
#include <QQuickWidget>
#include "ihaModel.h"
#include <QProcess>
#include <QMenu>
#include <QAction>
#include <QGroupBox>
#include <QMessageBox>
#include <QTextEdit>
#include "RosIhaListener.h"
#include "HttpIhaListener.h"
#include "telemetrybridge.h"
#include <QTimer>
#include <QFile>
#include <QRegularExpression>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkCookieJar>
#include <QJsonDocument>
#include <QJsonObject>
// ROS includes for MAVROS bridge
#include <ros/ros.h>
#include <sensor_msgs/NavSatFix.h>
#include <nav_msgs/Odometry.h>
#include <mavros_msgs/State.h>
#include <mavros_msgs/SetMode.h>
#include <tf/transform_datatypes.h>
#include <memory>
#include <QHash>
#include <QPointF>
#include <QDateTime>
#include "RakipAnaliz.h"


// Forward declaration
class RosBridge;
class RakipIhaPoller;

class MainPageWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MainPageWidget(QWidget *parent = nullptr);
    ~MainPageWidget();
    enum LogType { Info, Warn, Error };
    void addLogEntry(const QString &message, LogType type = Info);
    void updateTelemetryDisplay(double lat, double lon, double alt, double roll, double pitch, double yaw, double speed, double battery); // TelemetryBridge'den gelen telemetri güncellemesi
    void updateIhaPosition(double lat, double lon, double alt, double roll, double pitch, double yaw, double speed); // Sunucudan gelen İHA pozisyonu güncellemesi
    void updateServerTime(const QDateTime &serverTime); // Sunucu saatini güncelleme
    void setServerUrl(const QString &serverUrl); // Sunucu URL'sini ayarla
    // Paylaşılan oturum çerezleri için cookie jar set et
    void setCookieJar(QNetworkCookieJar* jar);

    // Geçerli mod bilgisini ve otonomi durumunu dışarı ver
    QString currentMode() const;
    bool isAutonomousMode() const;


public slots:
    void onTelemetryUpdated(double lat, double lon, double alt, double roll, double pitch, double yaw, double speed, double battery);
    void onMapPositionUpdated(double latitude, double longitude, double yaw);
    void onIhaDataUpdated(int ihaId, double latitude, double longitude, double altitude, int battery, bool isEnemy);
    void onIhaPositionUpdated(int ihaId, double latitude, double longitude, bool isEnemy);
    // Yeni: TelemetryBridge POST yanıtından gelen rakip IHA verilerini işle
    void onRivalsFromTelemetryJson(const QString& rawJson);
    // ROS'tan gelen rakip İHA verileri
    void onRivalsFromPoseList(QVector<double> lats, QVector<double> lons, QVector<double> alts,
                              QVector<double> yaws, QVector<int> ids);
    // HSS ve QR koordinatları çekme fonksiyonları
    void refreshQrCoordinates();
    void refreshHssCoordinates();
    // HSS Script fonksiyonları
    void onBtnHssBaslatClicked();
    void onBtnHssDurdurClicked();
    // Kamikaze Script fonksiyonları
    void onBtnKamikazeDurdurClicked();
    // TelemetryBridge referansı için setter
    void setTelemetryBridge(TelemetryBridge* bridge);
    
    // İrtifa ve hız güncelleme fonksiyonları
    void updateAltitude(double altitude);
    void updateSpeed(double speed);
    
    // Yardımcı bearing hesaplama (deg)
    double bearingDeg(double lat1, double lon1, double lat2, double lon2);

signals:
    void rivalsFetchRequested();
    void lockButtonClicked();

private slots:
    // onIhaPositionUpdated fonksiyonu public slots'ta zaten var
    void onQrCoordinatesReceived(QNetworkReply* reply);
    void onHssCoordinatesReceived(QNetworkReply* reply);

private:
    QPushButton* lightButton = nullptr;
    bool isLightMode = false;
    void applyTheme(bool darkMode);
    QPixmap lightModeIcon;
    QPixmap darkModeIcon;
    CameraWidget* cameraWidget;
    QQuickWidget *mapWidget;
    // Uçuş kontrol paneli
    QLabel* labelMode = nullptr;
    QGroupBox* groupBoxFlight = nullptr;
    QPushButton* pushButtonArm = nullptr;
    QPushButton* pushButtonDisarm = nullptr;
    QPushButton* modeSelectButton = nullptr;
    QMenu* modeMenu = nullptr;
    QTextEdit* logBox = nullptr;
    QTextEdit* rivalAnalysisBox = nullptr;
    RosIhaListener* rosIhaListener = nullptr;
    HttpIhaListener* httpIhaListener = nullptr;
    // TelemetryBridge* telemetryBridge = nullptr; // MainWindow'dan gelecek
    QList<QObject*> ihaModelList;
    QTimer* ihaFileTimer = nullptr;
    void setupFlightPanel();
    void callCommand(const QString& command);
    void checkActualMode(const QString& command, const QString& targetMode);
    void updateIhaOnMap(int ihaId, double latitude, double longitude, double altitude, int battery, bool isEnemy);
    void updateIhaFromServerData(const QString& jsonData);
    void updateIhaFromFile();
    void startModeListener();
    void startIhaPositionListener();
    void getIhaYawAndUpdatePosition(double lat, double lon);
    void updateIhaPositionOnMap(double lat, double lon, double yaw);
    // Camera control panel members
    QWidget* cameraButtonWidget = nullptr;
    QPushButton* btnLive = nullptr;
    QPushButton* btnRefresh = nullptr;
    QPushButton* btnHss = nullptr;
    QPushButton* btnKilitlenme = nullptr;
    QPushButton* btnKamikaze = nullptr;
    
    // ROS Bridge for MAVROS communication
    RosBridge* rosBridge = nullptr;
    
    // Network manager for QR coordinates
    QNetworkAccessManager* networkManager = nullptr;
    QString serverUrl; // Sunucu URL'si
    
    // Rival IHA position poller
    RakipIhaPoller* rakipPoller_ = nullptr;
    RakipAnaliz* rakipAnaliz_ = nullptr;

    // Rakip yaw fallback için son konum hafızası
    QHash<int, QPointF> lastRivalPos_; // teamNo -> (lon, lat)

    // Rakip IHA bilgilerini saklamak için sözlük (teamNo -> bilgi)
    struct RivalInfo {
        double latitude{0.0};           // iha_enlem
        double longitude{0.0};          // iha_boylam
        double altitudeMeters{0.0};     // iha_irtifa
        double pitchDeg{0.0};           // iha_dikilme
        double rollDeg{0.0};            // iha_yatis
        double yawDeg{0.0};             // iha_yonelme
        double speed{0.0};              // iha_hizi
        int timeOffsetMs{0};            // zaman_farki
        QDateTime lastUpdateUtc{};
    };
    QHash<int, RivalInfo> rivalsByTeam_;

    // Yardımcılar
    void updateRivalOnMap(int teamNo, double lat, double lon, double yaw);

    // Bir sonraki rakip güncellemesinde haritayı ilk rakibe merkezle
    bool centerOnNextRivals_ = false;
    
    // HSS polygon hatası için flag (sadece bir kere göster)
    bool hssPolygonErrorShown_ = false;
    
    // Kamikaze çalışma durumu
    bool kamikazeRunning_ = false;
    
    // Kilitlenme çalışma durumu
    bool kilitlenmeRunning_ = false;
    
    // HSS koordinatlarını dosyaya yazma fonksiyonu
    void writeHssCoordinatesToFile(const QVariantList& hssCoordinates);
    
    // Kamikaze buton UI güncelleme fonksiyonu
    void updateKamikazeButtonUi();
    
    // Kilitlenme buton UI güncelleme fonksiyonu
    void updateKilitlenmeButtonUi();
    
    // QR koordinatlarını dosyaya yazma fonksiyonu
    void writeQrCoordinatesToFile(double qrLat, double qrLon);
    
    // Kamikaze başlatma fonksiyonu
    void startKamikaze();

    // HSS durum ve UI
    bool hssRunning_ = false;
    void updateHssButtonUi();
    
    // Kilitlenme script fonksiyonları
    void runKilitlenmeScript();   // Başlat (buton burayı çağıracak)
    void stopKilitlenmeScript();  // Opsiyonel (şimdilik kullanmasan da dursun)
    QString kilitlenmeScriptPath() const;
    
    // Kilitlenme script process'i
    QProcess* kilitlenmeProc_ = nullptr;
    // TelemetryBridge referansı
    TelemetryBridge* telemetryBridge_ = nullptr;

    // İrtifa ve hız labellerı (LoginPage'den aktarılacak)
    QLabel* altitudeLabel = nullptr;
    QLabel* speedLabel = nullptr;

    // Mod dinleyiciden gelen son mod değeri (boş kalabilir; label üzerinden de okunur)
    QString currentMode_;
    


};

#endif // MAINPAGEWIDGET_H 