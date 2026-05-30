#include "MainPageWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QQuickWidget>
#include <QQmlContext>
#include <QTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QQuickItem>
#include <QPointF>
#include <QDebug>
#include <cmath>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QUrl>
#include <QSizePolicy>
#include <QLineEdit> // Added for QLineEdit
#include <QTabWidget> // Added for QTabWidget
#include <QProcess> // Added for QProcess
#include <QDir> // Added for QDir
#include <QRegularExpression> // Added for QRegularExpression
#include <QMenu> // Added for QMenu
#include <QMessageBox> // Added for QMessageBox
#include <QDateTime>
#include <QStandardPaths>
#include <QThread>
#include "RosBridge.h"
#include "RakipIhaPoller.h"
#include <QVariant>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

// Dosyanın en sonunda:



constexpr double EARTH_RADIUS = 6371000.0;

double haversineDistance(double lat1, double lon1, double lat2, double lon2) {
    double lat1Rad = lat1 * M_PI / 180.0;
    double lat2Rad = lat2 * M_PI / 180.0;
    double dLat = (lat2 - lat1) * M_PI / 180.0;
    double dLon = (lon2 - lon1) * M_PI / 180.0;

    double a = sin(dLat/2) * sin(dLat/2) +
               cos(lat1Rad) * cos(lat2Rad) *
               sin(dLon/2) * sin(dLon/2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    double distance = EARTH_RADIUS * c;
    return distance;
}

double calculateBearing(double lat1, double lon1, double lat2, double lon2) {
    lat1 = lat1 * M_PI / 180.0;
    lat2 = lat2 * M_PI / 180.0;
    double dLon = (lon2 - lon1) * M_PI / 180.0;

    double y = sin(dLon) * cos(lat2);
    double x = cos(lat1) * sin(lat2) -
               sin(lat1) * cos(lat2) * cos(dLon);

    double bearing = atan2(y, x) * 180.0 / M_PI;
    bearing = fmod((bearing + 360.0), 360.0);
    return bearing;
}

QVector<QPointF> coordinateList;
static int currentCoordIndex = 0;

constexpr double G = 9.81; // Yerçekimi ivmesi

// Açıyı -pi ile +pi arasına getir
inline double wrap_pi(double angle) {
    while (angle > M_PI) angle -= 2*M_PI;
    while (angle < -M_PI) angle += 2*M_PI;
    return angle;
}

// İzleme açısı (eta): yol yönü ile İHA yönü arasındaki açı (radyan)
double calculateEta(double chi_p, double chi) {
    return wrap_pi(chi_p - chi);
}

// L1 controller bank angle komutu (phi_cmd, rad)
double l1_bank_angle(double V, double L1_dist, double eta) {
    return atan2(2 * V * V * sin(eta), G * L1_dist);
}

// Yükseklik profili (climb angle, rad)
double climb_angle(double h_target, double h_current, double ground_distance) {
    return atan2(h_target - h_current, ground_distance);
}

// Örnek debug/log kullanımı (her waypoint güncellemesinde çağrılabilir)
void logL1ControlExample(double ihaLat, double ihaLon, double ihaHeadingDeg, double ihaSpeed,
                        double wpLat, double wpLon, double wpAlt, double ihaAlt, double L1_dist) {
    // Yönler radyan cinsinden
    double chi = ihaHeadingDeg * M_PI / 180.0;
    double chi_p = calculateBearing(ihaLat, ihaLon, wpLat, wpLon) * M_PI / 180.0;
    double eta = calculateEta(chi_p, chi);
    double phi_cmd = l1_bank_angle(ihaSpeed, L1_dist, eta); // rad
    double ground_dist = haversineDistance(ihaLat, ihaLon, wpLat, wpLon);
    double gamma_cmd = climb_angle(wpAlt, ihaAlt, ground_dist); // rad

    qDebug() << "L1 Control Debug:";
    qDebug() << "  eta (deg):" << eta * 180.0 / M_PI;
    qDebug() << "  phi_cmd (bank, deg):" << phi_cmd * 180.0 / M_PI;
    qDebug() << "  gamma_cmd (climb, deg):" << gamma_cmd * 180.0 / M_PI;
    qDebug() << "  ground_dist (m):" << ground_dist;
}

MainPageWidget::MainPageWidget(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle("Görev Kontrol Arayüzü");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setSpacing(18);
    mainLayout->setContentsMargins(18, 18, 18, 18);

    // Sol: Harita üstte, uçuş kontrolü ve waypoint kontrolleri altta
    QVBoxLayout* leftLayout = new QVBoxLayout;
    leftLayout->setSpacing(12);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    mapWidget = new QQuickWidget(this);
    mapWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    mapWidget->setSource(QUrl("qrc:/Map.qml"));
    mapWidget->setMinimumSize(500, 300);
    mapWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    leftLayout->addWidget(mapWidget, 2);

    // Sol alt: Uçuş kontrolü ve Waypoint kontrolleri yan yana
    QHBoxLayout* bottomLeftLayout = new QHBoxLayout;
    bottomLeftLayout->setSpacing(12);

    setupFlightPanel();
    bottomLeftLayout->addWidget(groupBoxFlight, 0, Qt::AlignTop);

    // İrtifa ve Hız paneli (LoginPage'den aktarıldı)
    QGroupBox* waypointControlBox = new QGroupBox("İrtifa ve Hız", this);
    QVBoxLayout* waypointLayout = new QVBoxLayout(waypointControlBox);
    QHBoxLayout* coordLayout = new QHBoxLayout;
    
    // İrtifa label'ı
    QVBoxLayout* altitudeLayout = new QVBoxLayout();
    altitudeLayout->setContentsMargins(0, 0, 0, 0);
    altitudeLayout->setSpacing(4);
    QLabel* altitudeTitle = new QLabel("İrtifa:", waypointControlBox);
    altitudeTitle->setAlignment(Qt::AlignCenter);
    altitudeTitle->setStyleSheet("font-size: 14px; color: #f1c40f;");
    altitudeLayout->addWidget(altitudeTitle);
    
    altitudeLabel = new QLabel("-- m", waypointControlBox);
    altitudeLabel->setAlignment(Qt::AlignCenter);
    altitudeLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #e9f1f7; background-color: #232b3a; padding: 8px; border-radius: 8px; min-width: 80px;");
    altitudeLayout->addWidget(altitudeLabel);
    
    // Hız label'ı
    QVBoxLayout* speedLayout = new QVBoxLayout();
    speedLayout->setContentsMargins(0, 0, 0, 0);
    speedLayout->setSpacing(4);
    QLabel* speedTitle = new QLabel("Hız:", waypointControlBox);
    speedTitle->setAlignment(Qt::AlignCenter);
    speedTitle->setStyleSheet("font-size: 14px; color: #f1c40f;");
    speedLayout->addWidget(speedTitle);
    
    speedLabel = new QLabel("-- m/s", waypointControlBox);
    speedLabel->setAlignment(Qt::AlignCenter);
    speedLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #e9f1f7; background-color: #232b3a; padding: 8px; border-radius: 8px; min-width: 80px;");
    speedLayout->addWidget(speedLabel);
    
    coordLayout->addLayout(altitudeLayout);
    coordLayout->addLayout(speedLayout);
    waypointLayout->addLayout(coordLayout);
    QPushButton* addWaypointButton = new QPushButton("HSS Çek", waypointControlBox);
    QPushButton* waypointModeButton = new QPushButton("Qr çek", waypointControlBox);
    QPushButton* clearWaypointsButton = new QPushButton("Waypoint'leri Temizle", waypointControlBox);
    QPushButton* rivalsFetchButton = new QPushButton("Rakip İHA Çek", waypointControlBox);
    QHBoxLayout* buttonRow = new QHBoxLayout;
    buttonRow->addWidget(addWaypointButton);
    buttonRow->addWidget(waypointModeButton);
    buttonRow->addWidget(clearWaypointsButton);
    buttonRow->addWidget(rivalsFetchButton);
    waypointLayout->addLayout(buttonRow);
    // Kamera butonları artık waypointControlBox'un içinde değil, tamamen ayrı bir widget ve layout ile ekleniyor
    // waypointControlBox->setLayout(waypointLayout); // Bu satır artık gerekli değil
    waypointControlBox->setMinimumWidth(400);
    waypointControlBox->setMaximumWidth(600);
    waypointControlBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    waypointControlBox->setStyleSheet(
        "QGroupBox {"
        "  border: 2px solid #FFD600;"
        "  border-radius: 8px;"
        "  margin-top: 8px;"
        "  background: #232b3a;"
        "  color: #fff;"
        "  font-size: 15px;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  left: 10px;"
        "  padding: 0 3px 0 3px;"
        "}"
    );

    // --- Ekstra Panel ---
    QGroupBox* ekstraBox = new QGroupBox("Ekstra Panel", this);
    QVBoxLayout* ekstraLayout = new QVBoxLayout(ekstraBox);
    QPushButton* btnLive = new QPushButton("Kamera Başlat", ekstraBox);
    QPushButton* btnRefresh = new QPushButton("Kamera Yenile", ekstraBox);
    QPushButton* btnHss = new QPushButton("HSS Başlat", ekstraBox);
    QPushButton* btnKilitlenme = new QPushButton("Kilitlenme Başlat", ekstraBox);
    QPushButton* btnKamikaze = new QPushButton("Kamikaze Başlat", ekstraBox);
    QHBoxLayout* buttonRow1 = new QHBoxLayout;
    buttonRow1->addWidget(btnLive);
    buttonRow1->addWidget(btnRefresh);
    QHBoxLayout* buttonRow2 = new QHBoxLayout;
    buttonRow2->addWidget(btnHss);
    buttonRow2->addWidget(btnKilitlenme);
    buttonRow2->addWidget(btnKamikaze);
    ekstraLayout->addLayout(buttonRow1);
    ekstraLayout->addLayout(buttonRow2);
    ekstraBox->setMinimumWidth(400);
    ekstraBox->setMaximumWidth(600);
    ekstraBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ekstraBox->setStyleSheet(
        "QGroupBox {"
        "  border: 2px solid #FFD600;"
        "  border-radius: 8px;"
        "  margin-top: 8px;"
        "  background: #232b3a;"
        "  color: #fff;"
        "  font-size: 15px;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  left: 10px;"
        "  padding: 0 3px 0 3px;"
        "}"
    );

    // --- Waypoint ve Ekstra paneli alt alta ekle ---
    QVBoxLayout* waypointAndExtraLayout = new QVBoxLayout;
    waypointAndExtraLayout->setSpacing(8);
    waypointAndExtraLayout->addWidget(waypointControlBox);
    waypointAndExtraLayout->addWidget(ekstraBox);
    bottomLeftLayout->addLayout(waypointAndExtraLayout, 1);

    leftLayout->addLayout(bottomLeftLayout);
    QWidget* leftWidget = new QWidget(this);
    leftWidget->setLayout(leftLayout);
    leftWidget->setMinimumWidth(500);
    leftWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainLayout->addWidget(leftWidget, 5);

    // Sağ: üstte kamera, altta kamera kontrolleri, log kutusu ve sekmeli alan
    QVBoxLayout* rightLayout = new QVBoxLayout;
    rightLayout->setSpacing(16);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    cameraWidget = new CameraWidget(this);
    cameraWidget->setFixedSize(640, 480);
    cameraWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    rightLayout->addWidget(cameraWidget, 0, Qt::AlignHCenter);

    // Log kutusu (örnekteki gibi)
    // QTextEdit* kamikazeLogBox = new QTextEdit();
    // kamikazeLogBox->setReadOnly(true);
    // kamikazeLogBox->setFixedHeight(48);
    // kamikazeLogBox->setStyleSheet("background: #232b3a; color: #e9f1f7; border-radius: 8px; font-size: 11px; padding: 4px;");
    // rightLayout->addWidget(kamikazeLogBox);

    // Sekmeli alan (örnekteki gibi)
    QTabWidget* tabWidget = new QTabWidget(this);
    tabWidget->setStyleSheet("QTabWidget::pane { border: 2px solid #FFD600; border-radius: 8px; background: #232b3a; } QTabBar::tab { background: #232b3a; color: #e9f1f7; font-weight: bold; padding: 8px 18px; border-top-left-radius: 8px; border-top-right-radius: 8px; } QTabBar::tab:selected { background: #353232; color: #FFD600; }");

    // Kayıtlar sekmesi
    QWidget* kayitlarTab = new QWidget;
    QVBoxLayout* kayitlarLayout = new QVBoxLayout(kayitlarTab);
    QLabel* kayitlarTitle = new QLabel("Kayıtlar");
    kayitlarTitle->setStyleSheet("font-weight: bold; font-size: 16px; color: #e9f1f7;");

    logBox = new QTextEdit;
    logBox->setReadOnly(true);
    logBox->setStyleSheet("background: #232b3a; color: #e9f1f7; font-size: 14px; border: 2px solid #FFD600; border-radius: 8px; font-family: Consolas, monospace; padding: 8px;");

    kayitlarLayout->addWidget(kayitlarTitle);
    kayitlarLayout->addWidget(new QLabel("Takım Adı: 11,   Takım Puanı: 0"));
    kayitlarLayout->addWidget(logBox);

    // Rakip Analizi sekmesi
    QWidget* rakipTab = new QWidget;
    QVBoxLayout* rakipLayout = new QVBoxLayout(rakipTab);
    QLabel* rakipTitle = new QLabel("Rakip Analizi");
    rakipTitle->setStyleSheet("font-weight: bold; font-size: 16px; color: #e9f1f7;");
    rivalAnalysisBox = new QTextEdit;
    rivalAnalysisBox->setReadOnly(true);
    rivalAnalysisBox->setStyleSheet("background: #232b3a; color: #e9f1f7; font-size: 14px; border: 2px solid #FFD600; border-radius: 8px; font-family: Consolas, monospace; padding: 8px;");
    rivalAnalysisBox->setMinimumHeight(220);
    rivalAnalysisBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    rakipLayout->addWidget(rakipTitle);
    rakipLayout->addWidget(rivalAnalysisBox);
    rakipLayout->setStretch(0, 0);
    rakipLayout->setStretch(1, 1);
    qDebug() << "[MainPageWidget] Rakip Analizi sekmesi oluşturuldu. Box minH="
             << rivalAnalysisBox->minimumHeight() << " sizePolicy="
             << rivalAnalysisBox->sizePolicy();

    tabWidget->addTab(kayitlarTab, "Kayıtlar");
    tabWidget->addTab(rakipTab, "Rakip Analizi");
    rightLayout->addWidget(tabWidget, 1);


    QWidget* rightWidget = new QWidget(this);
    rightWidget->setLayout(rightLayout);
    rightWidget->setMinimumWidth(260);
    rightWidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
    mainLayout->addWidget(rightWidget, 3);
    setLayout(mainLayout);

    ihaFileTimer = new QTimer(this);
    connect(ihaFileTimer, &QTimer::timeout, this, &MainPageWidget::updateIhaFromFile);
    ihaFileTimer->start(1000); // Her saniye

    // Initialize ROS Bridges for MAVROS communication
    // tunga1 ile aynı şekilde doğrudan /mavros konularını dinle
    rosBridge = new RosBridge(this, QString());
    connect(rosBridge, &RosBridge::posUpdated, this, [this](double lat, double lon){
        onIhaPositionUpdated(0, lat, lon, true);
    });
    connect(rosBridge, &RosBridge::yawUpdated, this, [this](double yaw){
        onIhaDataUpdated(0, 0, 0, yaw, 0, false);
    });
    connect(rosBridge, &RosBridge::modeUpdated, this, [this](const QString& mode, bool armed){
        if (labelMode) {
            labelMode->setText(mode);
        }
        addLogEntry(QString("Mod: %1 | %2").arg(mode, armed ? "ARMED" : "DISARMED"), Info);
    });

    // Rakip Analiz işçisi: sunucu URL ve cookie paylaşımı MainWindow üzerinden set edilecek
    rakipAnaliz_ = new RakipAnaliz(this);
    connect(rakipAnaliz_, &RakipAnaliz::analysisReady, this, [this](const QString& text){
        if (rivalAnalysisBox) {
            rivalAnalysisBox->setPlainText(text);
            rivalAnalysisBox->moveCursor(QTextCursor::End);
            rivalAnalysisBox->ensureCursorVisible();
            rivalAnalysisBox->viewport()->update();
        }
    });
    // Not: start() çağrısı MainWindow'dan, serverUrl ayarlandıktan ve cookie jar verildikten sonra yapılmalı

    // >>> File timer'ı kapat - RosBridge'ten gelen veriyi üzerine yazmasın
    if (ihaFileTimer) {
        ihaFileTimer->stop();
        qDebug() << "ihaFileTimer stopped - using RosBridge data instead";
    }

    // --- TELEMETRYBRIDGE BAŞLAT ---
    // TelemetryBridge MainWindow'dan gelecek, burada oluşturmuyoruz
    // TelemetryBridge sinyalleri MainWindow'da bağlanacak
    
    qDebug() << "TelemetryBridge MainWindow'dan gelecek, ROS telemetri verileri iha.png'ye aktarılacak";

    // --- RAKİP ROSBRIDGE'LER KALDIRILDI ---
    // Rakipler artık HTTP sunucudan RakipIhaPoller ile besleniyor.
    // Sadece bizim İHA (uav0) simülasyondan geliyor.

    // --- ETKILESIMLER ---
    // ARM/DISARM butonları
    // Mod seçimi ile ilgili menü ve callCommand bağlantılarını kaldırıyorum
    // Sadece dışarıdan (modeReader) gelen veriyle mod güncellenecek
    // HSS Çek
    connect(addWaypointButton, &QPushButton::clicked, this, [=]() {
        addLogEntry("HSS koordinatları çekiliyor...", Info);
        refreshHssCoordinates();
    });
    // Waypoint temizle
    connect(clearWaypointsButton, &QPushButton::clicked, this, [=]() {
        addLogEntry("Tüm waypoint'ler temizlendi.", Info);
        // QML'de waypoints'i temizle
    });
    // Qr çek butonu
    connect(waypointModeButton, &QPushButton::clicked, this, [=]() {
        addLogEntry("Qr çek butonu tıklandı.", Info);
        // C++ fonksiyonunu çağır
        refreshQrCoordinates();
    });
    // Rakip İHA Çek butonu
    connect(rivalsFetchButton, &QPushButton::clicked, this, [this]() {
        addLogEntry("Rakip İHA Çek tıklandı.", Info);
        // Bir sonraki rivals güncellemesinde ilk rakibe merkezle
        centerOnNextRivals_ = true;
        emit rivalsFetchRequested();
    });
    // Kamera butonlarını CameraWidget'a bağla ve logla
    connect(btnLive, &QPushButton::clicked, this, [this]() {
        addLogEntry("Kamera Başlat tıklandı.", Info);
        cameraWidget->onStartClicked();
    });
    connect(btnRefresh, &QPushButton::clicked, this, [this]() {
        addLogEntry("Kamera Yenile tıklandı.", Info);
        cameraWidget->onRefreshClicked();
    });
    
    // HSS button handler - runs HSSAvoid_Start.sh script
    connect(btnHss, &QPushButton::clicked, this, [this, btnHss]() {
        if (!hssRunning_) {
            addLogEntry("HSS başlat butonu tıklandı - HSSAvoid_Start.sh çalıştırılıyor...", Info);
            onBtnHssBaslatClicked();
            hssRunning_ = true;
            updateHssButtonUi();
        } else {
            addLogEntry("HSS durdur tıklandı - hss_geo_avoid kapatılacak...", Warn);
            onBtnHssDurdurClicked();
        }
    });

    this->btnHss = btnHss;
    updateHssButtonUi();
    
    this->btnKilitlenme = btnKilitlenme;
    updateKilitlenmeButtonUi();
    connect(btnKilitlenme, &QPushButton::clicked, this, [this]() {
        if (!kilitlenmeRunning_) {
            addLogEntry("Kilitlenme Başlat tıklandı.", Info);
            emit lockButtonClicked();
            runKilitlenmeScript();
            kilitlenmeRunning_ = true;
            updateKilitlenmeButtonUi();
            // TelemetryBridge'e kilitlenme aktif olduğunu bildir
            if (telemetryBridge_) {
                telemetryBridge_->setLockActive(true);
            }
        } else {
            addLogEntry("Kilitlenme Durdur tıklandı.", Info);
            stopKilitlenmeScript();
            kilitlenmeRunning_ = false;
            updateKilitlenmeButtonUi();
            // TelemetryBridge'e kilitlenme pasif olduğunu bildir
            if (telemetryBridge_) {
                telemetryBridge_->setLockActive(false);
                // Kilit kapandığında hedef değerlerini sıfırla ki telemetri 0 göndersin
                telemetryBridge_->setTargetDetection(0, 0, 0, 0);
            }
        }
    });
    // Kamikaze button handler - starts/stops kamikaze mission
    connect(btnKamikaze, &QPushButton::clicked, this, [this, btnKamikaze]() {
        if (!kamikazeRunning_) {
            addLogEntry("Kamikaze başlat butonu tıklandı - mission_controller_nocam başlatılıyor...", Warn);
            startKamikaze();
            kamikazeRunning_ = true;
            updateKamikazeButtonUi();
        } else {
            addLogEntry("Kamikaze durdur tıklandı - mission_controller_nocam kapatılacak...", Warn);
            onBtnKamikazeDurdurClicked();
        }
    });

    this->btnKamikaze = btnKamikaze;
    updateKamikazeButtonUi();

    // --- Mod dinleyici başlat (sadece mode satırını ayıkla) ---
    // (KALDIRILDI: rostopic echo /mavros/state ve QProcess ile ilgili kodlar)
    startModeListener();
    
    // Kamikaze durumu başlat
    kamikazeRunning_ = false;
    
    // Kilitlenme durumu başlat
    kilitlenmeRunning_ = false;


    // Network manager'ı başlat
    networkManager = new QNetworkAccessManager(this);
    

}

void MainPageWidget::setTelemetryBridge(TelemetryBridge* bridge) {
    telemetryBridge_ = bridge;
    qDebug() << "[MainPageWidget] TelemetryBridge referansı ayarlandı";
}

void MainPageWidget::updateHssButtonUi() {
    if (!btnHss) return;
    if (hssRunning_) {
        btnHss->setText("HSS DURDUR");
        btnHss->setStyleSheet("QPushButton { background: #FFD600; color: #000; font-weight: bold; }");
    } else {
        btnHss->setText("HSS Başlat");
        btnHss->setStyleSheet("");
    }
}

void MainPageWidget::updateKamikazeButtonUi() {
    if (!btnKamikaze) return;
    if (kamikazeRunning_) {
        btnKamikaze->setText("Kamikaze Durdur");
        btnKamikaze->setStyleSheet("QPushButton { background: #FF0000; color: #FFF; font-weight: bold; }");
    } else {
        btnKamikaze->setText("Kamikaze Başlat");
        btnKamikaze->setStyleSheet("");
    }
}

void MainPageWidget::updateKilitlenmeButtonUi() {
    if (!btnKilitlenme) return;
    if (kilitlenmeRunning_) {
        btnKilitlenme->setText("Kilitlenme Durdur");
        btnKilitlenme->setStyleSheet("QPushButton { background: #00FF00; color: #000; font-weight: bold; }");
    } else {
        btnKilitlenme->setText("Kilitlenme Başlat");
        btnKilitlenme->setStyleSheet("");
    }
}

// Paylaşılan cookie jar'ı MainPageWidget içindeki networkManager'a uygula
void MainPageWidget::setCookieJar(QNetworkCookieJar* jar) {
    if (!networkManager) {
        networkManager = new QNetworkAccessManager(this);
    }
    if (jar) {
        networkManager->setCookieJar(jar);
        qDebug() << "[MainPageWidget] Cookie jar set edildi (networkManager).";
        if (rakipAnaliz_) {
            rakipAnaliz_->setCookieJar(jar);
            qDebug() << "[MainPageWidget] Cookie jar set edildi (RakipAnaliz).";
        }
    }
}

void MainPageWidget::updateIhaFromFile() {
    static QPointF prevCoord(41.60500, 32.35000); // Başlangıç noktası
    static double lastYaw = 0; // Son yönelme değeri
    QFile file("/home/oguz/FlashServer/data.json");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "data.json okunamıyor";
        return; // <-- fallback setProperty'leri KALDIR
    }
    QByteArray jsonData = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "Geçersiz JSON:" << error.errorString();
        return; // <-- fallback setProperty'leri KALDIR
    }
    QJsonObject obj = doc.object();

    coordinateList.clear();
    if (obj.contains("koordinatlar") && obj["koordinatlar"].isArray()) {
        QJsonArray arr = obj["koordinatlar"].toArray();
        for (const QJsonValue& val : arr) {
            if (val.isObject()) {
                QJsonObject coordObj = val.toObject();
                double enlem = coordObj["enlem"].toDouble();
                double boylam = coordObj["boylam"].toDouble();
                coordinateList.append(QPointF(boylam, enlem));
            }
        }
    } else if (obj.contains("iha_enlem") && obj.contains("iha_boylam")) {
        double enlem = obj["iha_enlem"].toDouble();
        double boylam = obj["iha_boylam"].toDouble();
        coordinateList.append(QPointF(boylam, enlem));
    } else {
        qWarning() << "JSON formatı beklenen şekilde değil";
        return; // <-- fallback setProperty'leri KALDIR
    }

    if (coordinateList.isEmpty()) {
        return; // <-- fallback setProperty'leri KALDIR
    }

    QQuickItem* root = mapWidget->rootObject();
    if (!root) {
        qWarning() << "QML rootObject bulunamadı";
        return;
    }

    QPointF coord = coordinateList.at(currentCoordIndex % coordinateList.size());
    // Sadece matematiksel bearing ile heading hesapla
    double ihaYaw = lastYaw;
    double lat1 = prevCoord.y();
    double lon1 = prevCoord.x();
    double lat2 = coord.y();
    double lon2 = coord.x();
    if (lat1 != lat2 || lon1 != lon2) {
        ihaYaw = calculateBearing(lat1, lon1, lat2, lon2);
    }
    lastYaw = ihaYaw;
    root->setProperty("ihaLatLocation", coord.y());
    root->setProperty("ihaLonLocation", coord.x());
    root->setProperty("ihaYaw", ihaYaw);
    // Aşağıdaki telemetri tabanlı yönelme kodları baskılandı:
    // if (obj.contains("iha_yonelme")) {
    //     ihaYaw = obj["iha_yonelme"].toDouble();
    //     if (ihaYaw < 0) ihaYaw += 360;
    // }
    // else if (obj.contains("yaw")) {
    //     ihaYaw = obj["yaw"].toDouble();
    //     if (ihaYaw < 0) ihaYaw += 360;
    // }
    // else if (obj.contains("heading")) {
    //     ihaYaw = obj["heading"].toDouble();
    //     if (ihaYaw < 0) ihaYaw += 360;
    // }

    currentCoordIndex++;
    prevCoord = coord;
}

MainPageWidget::~MainPageWidget() {
    if (rakipPoller_) {
        rakipPoller_->stop();
    }
}

QString MainPageWidget::currentMode() const {
    // currentMode_ varsa onu döndür; yoksa labelMode'dan oku
    if (!currentMode_.trimmed().isEmpty()) return currentMode_;
    if (labelMode) return labelMode->text();
    return QString();
}

void MainPageWidget::onIhaDataUpdated(int /*id*/, double /*lat*/, double /*lon*/,
                                      double yawDeg, int /*modeId*/, bool /*armed*/) {
    if (!mapWidget || !mapWidget->rootObject()) return;
    QObject* root = mapWidget->rootObject();
    
    // Trail sistemi için yaw güncellemesi
    root->setProperty("ihaYaw", yawDeg);
    
    // Debug: Yaw güncellemesi
    qDebug() << "Yaw güncellemesi: Yaw=" << yawDeg << "°";
}

bool MainPageWidget::isAutonomousMode() const {
    // İstenilen mantık: Yalnızca MANUAL veya FBWA modlarında 0, diğer tüm dolu modlarda 1
    // Önce currentMode_ kullan; boşsa labelMode üzerindeki canlı metni oku
    QString m = currentMode_.trimmed();
    if (m.isEmpty() && labelMode) {
        m = labelMode->text();
    }
    m = m.trimmed().toUpper();
    if (m.isEmpty()) return false;
    if (m.contains("MANUAL") || m.contains("FBWA")) return false;
    return true;
}

void MainPageWidget::onIhaPositionUpdated(int /*id*/, double lat, double lon, bool /*ok*/) {
    static qint64 last_ms = 0;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - last_ms < 80) return; // 12.5 Hz üstünü yut
    last_ms = now;

    if (!mapWidget || !mapWidget->rootObject()) return;
    QObject* root = mapWidget->rootObject();
    
    // Trail sistemi için koordinat güncellemesi
    root->setProperty("ihaLatLocation", lat);
    root->setProperty("ihaLonLocation", lon);
    
    // Debug: Trail güncellemesi
    qDebug() << "Trail güncellemesi: Lat=" << lat << "Lon=" << lon;
}

// Yardımcı bearing hesaplama (deg)
double MainPageWidget::bearingDeg(double lat1, double lon1, double lat2, double lon2) {
    const double r = M_PI / 180.0;
    double phi1 = lat1 * r, phi2 = lat2 * r;
    double dLon = (lon2 - lon1) * r;
    double y = sin(dLon) * cos(phi2);
    double x = cos(phi1) * sin(phi2) - sin(phi1) * cos(phi2) * cos(dLon);
    double theta = atan2(y, x) * 180.0 / M_PI;
    if (theta < 0) theta += 360.0;
    return fmod(theta, 360.0);
}

void MainPageWidget::updateRivalOnMap(int teamNo, double lat, double lon, double yaw) {
    // Sözlükte temel alanları güncelle
    RivalInfo info = rivalsByTeam_.value(teamNo);
    info.latitude = lat;
    info.longitude = lon;
    info.yawDeg = yaw;
    info.lastUpdateUtc = QDateTime::currentDateTimeUtc();
    rivalsByTeam_.insert(teamNo, info);

    // Dinamik İHA oluşturma devre dışı: QML'e marker oluşturma çağrısı yapılmıyor
}

// TelemetryBridge POST yanıtından rakipleri işle
void MainPageWidget::onRivalsFromTelemetryJson(const QString& rawJson) {
    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(rawJson.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qDebug() << "onRivalsFromTelemetryJson: JSON parse error:" << err.errorString();
        return;
    }
    const QJsonObject root = doc.object();
    // Birden fazla backend formatını destekle: konumBilgileri[] veya telemetry[]
    QJsonArray arr;
    if (root.value(QStringLiteral("konumBilgileri")).isArray()) {
        arr = root.value(QStringLiteral("konumBilgileri")).toArray();
    } else if (root.value(QStringLiteral("telemetry")).isArray()) {
        arr = root.value(QStringLiteral("telemetry")).toArray();
    } else {
        qDebug() << "onRivalsFromTelemetryJson: Beklenen alan yok (konumBilgileri/telemetry)";
        return;
    }
    QList<int> activeTeams;
    QList<int> newTeams; // Yeni gelen takımları takip etmek için
    int processedCount = 0;
    QPointF firstRivalCoord;
    // Takım 10 için özel yakınlaştırma isteği
    bool team10Found = false;
    QPointF team10Coord;
    for (const QJsonValue& v : arr) {
        if (!v.isObject()) continue;
        const QJsonObject o = v.toObject();
        const int teamNo = o.value(QStringLiteral("takim_numarasi")).toInt();
        if (!activeTeams.contains(teamNo)) {
            activeTeams.append(teamNo);
            newTeams.append(teamNo);

        }
        const double lat = o.value(QStringLiteral("iha_enlem")).toDouble();
        const double lon = o.value(QStringLiteral("iha_boylam")).toDouble();
        if (!std::isfinite(lat) || !std::isfinite(lon)) continue;
        double yaw = o.value(QStringLiteral("iha_yonelme")).toDouble(std::numeric_limits<double>::quiet_NaN());
        if (!std::isfinite(yaw)) {
            // fallback bearing
            if (lastRivalPos_.contains(teamNo)) {
                const QPointF p = lastRivalPos_.value(teamNo);
                yaw = bearingDeg(p.y(), p.x(), lat, lon);
            } else {
                yaw = 0.0;
            }
        }
        lastRivalPos_[teamNo] = QPointF(lon, lat);

        // Tüm alanları C++ sözlüğüne işle
        RivalInfo info = rivalsByTeam_.value(teamNo);
        info.latitude = lat;
        info.longitude = lon;
        info.yawDeg = yaw;
        info.altitudeMeters = o.value(QStringLiteral("iha_irtifa")).toDouble(info.altitudeMeters);
        info.pitchDeg = o.value(QStringLiteral("iha_dikilme")).toDouble(info.pitchDeg);
        info.rollDeg = o.value(QStringLiteral("iha_yatis")).toDouble(info.rollDeg);
        info.speed = o.value(QStringLiteral("iha_hizi")).toDouble(info.speed);
        info.timeOffsetMs = o.value(QStringLiteral("zaman_farki")).toInt(info.timeOffsetMs);
        info.lastUpdateUtc = QDateTime::currentDateTimeUtc();
        rivalsByTeam_.insert(teamNo, info);



        // QML tarafındaki dinamik İHA marker'ını güncelle/oluştur
        if (mapWidget && mapWidget->rootObject()) {
            QObject* root = mapWidget->rootObject();
            QMetaObject::invokeMethod(
                root,
                "updateDynamicIha",
                Q_ARG(QVariant, QVariant::fromValue(teamNo)),
                Q_ARG(QVariant, QVariant::fromValue(lat)),
                Q_ARG(QVariant, QVariant::fromValue(lon)),
                Q_ARG(QVariant, QVariant::fromValue(yaw)),
                Q_ARG(QVariant, QVariant::fromValue(info.altitudeMeters)),
                Q_ARG(QVariant, QVariant::fromValue(info.pitchDeg)),
                Q_ARG(QVariant, QVariant::fromValue(info.rollDeg)),
                Q_ARG(QVariant, QVariant::fromValue(info.speed)),
                Q_ARG(QVariant, QVariant::fromValue(info.timeOffsetMs))
            );

            // Yarı-dinamik (mevcut T1..T14) marker'ları da güncelle
            // Statik marker'lara index'e göre aktar (1..14). Takım numaraları 1..14 dışında olabilir.
            int slotIndex = processedCount + 1; // 1 tabanlı indeks
            QMetaObject::invokeMethod(
                root,
                "updateSemiRival",
                Q_ARG(QVariant, QVariant::fromValue(slotIndex)),
                Q_ARG(QVariant, QVariant::fromValue(lat)),
                Q_ARG(QVariant, QVariant::fromValue(lon)),
                Q_ARG(QVariant, QVariant::fromValue(yaw))
            );

            // İlk rakibin koordinatını kaydet
            if (processedCount == 0) {
                firstRivalCoord = QPointF(lon, lat);
            }

            // Takım 10 için koordinatı sakla (zoom için)
            if (teamNo == 10) {
                team10Found = true;
                team10Coord = QPointF(lon, lat);
            }
        }
        processedCount++;
    }

    // Not: Marker havuzu senkronizasyonu bu aşamada gerekmiyor; QML kendi sözlüğünü yönetiyor
    // Manuel rakip çekme sonrası: takım 10 varsa ona merkezle ve yakınlaştır; yoksa önceki davranışı kullan
    if (processedCount > 0 && mapWidget && mapWidget->rootObject()) {
        QObject* root = mapWidget->rootObject();
        QObject* mapObj = root->findChild<QObject*>("mapView");
        if (!mapObj) {
            qWarning() << "mapView bulunamadı; centerOnCoordinate çağrısı atlandı";
        } else if (centerOnNextRivals_) {
            if (team10Found) {
                QMetaObject::invokeMethod(
                    mapObj,
                    "centerOnCoordinate",
                    Q_ARG(QVariant, QVariant::fromValue(team10Coord.y())),
                    Q_ARG(QVariant, QVariant::fromValue(team10Coord.x())),
                    Q_ARG(QVariant, QVariant::fromValue(16))
                );
                addLogEntry("Takım 10'a merkezlendi ve zoom yapıldı.", Info);
            } else {
                QMetaObject::invokeMethod(
                    mapObj,
                    "centerOnCoordinate",
                    Q_ARG(QVariant, QVariant::fromValue(firstRivalCoord.y())),
                    Q_ARG(QVariant, QVariant::fromValue(firstRivalCoord.x())),
                    Q_ARG(QVariant, QVariant::fromValue(16))
                );
                addLogEntry("İlk rakibe merkezlendi ve zoom yapıldı.", Info);
            }
            centerOnNextRivals_ = false; // Tek seferlik zoom
        }
    }
    // RakipAnaliz'e ham JSON'u geçir (özet üretmesi için)
    if (rakipAnaliz_) {
        qDebug() << "[MainPageWidget] RakipAnaliz.consumeRivalsJson çağrılıyor.";
        rakipAnaliz_->consumeRivalsJson(rawJson);
    } else {
        qWarning() << "[MainPageWidget] rakipAnaliz_ null - analiz güncellenemedi";
    }
    
    // Başarılı çekim sonrası mesaj
    if (processedCount > 0) {
        qDebug() << processedCount << "Rakip İHA çekildi";
    }
}

void MainPageWidget::addLogEntry(const QString &message, LogType type) {
    if (logBox) {
        QString timeStr = QDateTime::currentDateTime().toString("HH:mm:ss");
        QString tag, color;
        
        if (type == Error) {
            tag = "<b>[ERROR]</b>";
            color = "#ff3333"; // Kırmızı
        } else if (type == Warn) {
            tag = "<b>[WARN]</b>";
            color = "#ffaa00"; // Turuncu/Sarı
        } else {
            tag = "<b>[INFO]</b>";
            // Sadece başarılı mesajlar yeşil olsun
            if (message.contains("başarıyla gerçekleşti") || message.contains("başarıyla geçildi")) {
                color = "#00cc00"; // Koyu yeşil
            } else {
                color = "#ffffff"; // Beyaz
            }
        }
        
        QString html = QString("<span style='color:%1; font-family:Consolas,monospace;'>%2 [%3] %4</span>")
            .arg(color, tag, timeStr, message.toHtmlEscaped());
        logBox->append(html);
    }
}

void MainPageWidget::setupFlightPanel() {
    groupBoxFlight = new QGroupBox("Uçuş Kontrolü", this);
    QVBoxLayout* flightLayout = new QVBoxLayout(groupBoxFlight);
    flightLayout->setSpacing(10);
    flightLayout->setContentsMargins(12, 12, 12, 12);

    // ARM/DISARM butonları
    QHBoxLayout* armLayout = new QHBoxLayout;
    armLayout->setSpacing(8);
    pushButtonArm = new QPushButton("ARM", groupBoxFlight);
    pushButtonDisarm = new QPushButton("DISARM", groupBoxFlight);
    QSize smallBtnSize(70, 32);
    QFont smallFont;
    smallFont.setPointSize(10);
    smallFont.setBold(true);
    pushButtonArm->setFixedSize(smallBtnSize);
    pushButtonDisarm->setFixedSize(smallBtnSize);
    pushButtonArm->setFont(smallFont);
    pushButtonDisarm->setFont(smallFont);
    armLayout->addWidget(pushButtonArm);
    armLayout->addWidget(pushButtonDisarm);
    flightLayout->addLayout(armLayout);

    // Mod seç butonu ve menü
    modeSelectButton = new QPushButton("Mod Seç", groupBoxFlight);
    modeSelectButton->setFixedHeight(32);
    modeSelectButton->setFont(smallFont);
    modeMenu = new QMenu(modeSelectButton);
    modeMenu->setFont(smallFont);
    modeMenu->addAction("AUTO");
    modeMenu->addAction("TAKEOFF");
    modeMenu->addAction("MANUAL");
    modeMenu->addAction("LAND");
    modeMenu->addAction("RTL");
    modeMenu->setStyleSheet(
        "QMenu { background: #fff; font-size: 10pt; } "
        "QMenu::item { padding: 6px 18px; } "
        "QMenu::item:selected { background: #537fe7; color: #fff; } "
        "QMenu::icon { width: 0; } "
    );
    modeSelectButton->setMenu(modeMenu);
    flightLayout->addWidget(modeSelectButton);

    // Şimdiki Mod kutusu
    QGroupBox* modeBox = new QGroupBox("Şimdiki Mod", groupBoxFlight);
    modeBox->setFixedHeight(70);

    // smallFont kalınsa başlığı normale çek
    QFont modeTitleFont = smallFont;
    modeTitleFont.setBold(false);
    modeBox->setFont(modeTitleFont);

    // Sadece başlık için stil (ayrı çağrı!)
    modeBox->setStyleSheet(
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  left: 10px;"
        "  padding: 0 3px 0 3px;"
        "  font-weight: normal;"
        "}"
    );

    QVBoxLayout* modeLayout = new QVBoxLayout(modeBox);
    modeLayout->setContentsMargins(7, 7, 7, 7);
    modeLayout->setSpacing(2);

    labelMode = new QLabel("Mod Bekleniyor...", modeBox);
    labelMode->setAlignment(Qt::AlignCenter);
    // Eğer içerideki yazı da normal olsun istiyorsan bold'u kaldır:
    labelMode->setStyleSheet("font-size: 13px; padding: 4px;");
    labelMode->setFont(smallFont);
    labelMode->setWordWrap(true);

    modeLayout->addWidget(labelMode);
    modeBox->setLayout(modeLayout);
    flightLayout->addWidget(modeBox);

    // Uçuş Kontrolü genel kutu (ayrı styleSheet!)
    groupBoxFlight->setFixedWidth(220);
    groupBoxFlight->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
    groupBoxFlight->setStyleSheet(
        "QGroupBox {"
        "  border: 2px solid #FFD600;"
        "  border-radius: 8px;"
        "  margin-top: 8px;"
        "  background: #232b3a;"
        "  color: #fff;"
        "  font-size: 15px;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  left: 10px;"
        "  padding: 0 3px 0 3px;"
        "}"
    );

  // ARM/DISARM bağlantıları
  connect(pushButtonArm, &QPushButton::clicked, this, [this]() {
    addLogEntry("ARM komutu gönderildi.", Info);
    callCommand("arm");
});
connect(pushButtonDisarm, &QPushButton::clicked, this, [this]() {
    addLogEntry("DISARM komutu gönderildi.", Info);
    callCommand("disarm");
});
// Mod seçimi
connect(modeMenu, &QMenu::triggered, this, [this](QAction* action) {
    QString mod = action->text().toLower();
    if (mod == "land") {
        QMessageBox::StandardButton reply = QMessageBox::question(this,
            "Güvenlik Uyarısı",
            "Uçuş LAND moduna geçirilecek. Devam etmek istiyor musunuz?",
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No) {
            addLogEntry("LAND modu iptal edildi.", Warn);
            return;
        }
        addLogEntry("Mod değiştirildi: LAND", Warn);
    } else if (mod.contains("kapat") || mod.contains("sıfırla") || mod.contains("reset")) {
        addLogEntry("Mod değiştirildi: " + mod.toUpper(), Warn);
    } else {
        addLogEntry("Mod değiştirildi: " + mod.toUpper(), Info);
    }
    callCommand(mod);
});
}

void MainPageWidget::callCommand(const QString &command) {
QProcess *process = new QProcess(this);
QString fullCommand;
QString targetMode;

if (command == "arm")
    fullCommand = "rosservice call /mavros/cmd/arming \"value: true\"";
else if (command == "disarm")
    fullCommand = "rosservice call /mavros/cmd/arming \"value: false\"";
else if (command == "takeoff") {
    fullCommand = "rosservice call /mavros/set_mode \"custom_mode: 'GUIDED'\"";
    targetMode = "GUIDED";
}
else if (command == "manual") {
    fullCommand = "rosservice call /mavros/set_mode \"custom_mode: 'MANUAL'\"";
    targetMode = "MANUAL";
}
else if (command == "land") {
    fullCommand = "rosservice call /mavros/set_mode \"custom_mode: 'LAND'\"";
    targetMode = "LAND";
}
else if (command == "auto") {
    fullCommand = "rosservice call /mavros/set_mode \"custom_mode: 'AUTO'\"";
    targetMode = "AUTO";
}
else if (command == "rtl") {
    fullCommand = "rosservice call /mavros/set_mode \"custom_mode: 'RTL'\"";
    targetMode = "RTL";
}
else {
    qDebug() << "Unknown command:" << command;
    return;
}

QString wrapped = "bash -c 'source /opt/ros/noetic/setup.bash && source ~/catkin_ws/devel/setup.bash && " + fullCommand + "'";
process->start("bash", QStringList() << "-c" << wrapped);

// ARM/DISARM için direkt sonuç kontrolü
if (command == "arm" || command == "disarm") {
    connect(process, &QProcess::readyReadStandardOutput, [this, process, command]() {
        QString output = process->readAllStandardOutput();
        qDebug() << "Command output:" << output;
        
        if (output.contains("success: True") || output.contains("success: 1")) {
            if (command == "arm") {
                addLogEntry("ARM komutu başarıyla gerçekleşti.", Info);
            } else if (command == "disarm") {
                addLogEntry("DISARM komutu başarıyla gerçekleşti.", Info);
            }
        } else if (output.contains("success: False") || output.contains("success: 0")) {
            if (command == "arm") {
                addLogEntry("ARM komutu başarısız oldu!", LogType::Error);
            } else if (command == "disarm") {
                addLogEntry("DISARM komutu başarısız oldu!", LogType::Error);
            }
        } else {
            if (command == "arm") {
                addLogEntry("ARM komutu gönderildi, sonuç belirsiz.", Warn);
            } else if (command == "disarm") {
                addLogEntry("DISARM komutu gönderildi, sonuç belirsiz.", Warn);
            }
        }
    });
} else {
    // Mod değişikliği için gerçek mod kontrolü
    connect(process, &QProcess::readyReadStandardOutput, [this, process, command, targetMode]() {
        QString output = process->readAllStandardOutput();
        qDebug() << "Command output:" << output;
        
        // 2 saniye sonra gerçek modu kontrol et
        QTimer::singleShot(2000, this, [this, command, targetMode]() {
            checkActualMode(command, targetMode);
        });
    });
}

connect(process, &QProcess::readyReadStandardError, [this, process, command]() {
    QString error = process->readAllStandardError();
    qDebug() << "Command error:" << error;
    
    if (command == "arm") {
        addLogEntry("ARM komutu çalıştırılamadı " + error, LogType::Error);
    } else if (command == "disarm") {
        addLogEntry("DISARM komutu çalıştırılamadı " + error, LogType::Error);
    } else {
        addLogEntry(QString("%1 modu çalıştırılamadı %2").arg(command.toUpper(), error), LogType::Error);
    }
});

connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), [this, process, command](int exitCode, QProcess::ExitStatus exitStatus) {
    // Exit code mesajını gösterme, sadece process'i temizle
    process->deleteLater();
});
}

// Gerçek modu kontrol et ve istenen mod ile karşılaştır
void MainPageWidget::checkActualMode(const QString& command, const QString& targetMode) {
(void)command; // kullanılmayan parametre uyarısı için
QProcess* modeChecker = new QProcess(this);
QString rosCmd = "bash -c 'source /opt/ros/noetic/setup.bash && rostopic echo /mavros/state -n 1'";
modeChecker->start("bash", QStringList() << "-c" << rosCmd);

connect(modeChecker, &QProcess::readyReadStandardOutput, [this, modeChecker, command, targetMode]() {
    QString output = QString::fromUtf8(modeChecker->readAllStandardOutput());
    qDebug() << "Mode check output:" << output;
    
    // JSON formatından mode bilgisini çıkar
    if (output.contains("mode:")) {
        QRegularExpression modeRegex("mode:\\s*\"([^\"]+)\"");
        QRegularExpressionMatch match = modeRegex.match(output);
        if (match.hasMatch()) {
            QString actualMode = match.captured(1);
            qDebug() << "Target mode:" << targetMode << "Actual mode:" << actualMode;
            
            if (actualMode == targetMode) {
                addLogEntry(QString("%1 moduna başarıyla geçildi.").arg(targetMode), Info);
            } else {
                addLogEntry(QString("%1 moduna geçiş başarısız! (Hedef: %2, Gerçek: %3)").arg(targetMode, targetMode, actualMode), LogType::Error);
            }
        } else {
            addLogEntry(QString("%1 moduna geçiş komutu gönderildi, mod okunamadı.").arg(targetMode), Warn);
        }
    } else {
        addLogEntry(QString("%1 moduna geçiş komutu gönderildi, mod bilgisi alınamadı.").arg(targetMode), Warn);
    }
    modeChecker->deleteLater();
});

connect(modeChecker, &QProcess::readyReadStandardError, [this, modeChecker, command, targetMode]() {
    QString error = modeChecker->readAllStandardError();
    qDebug() << "Mode check error:" << error;
    addLogEntry(QString("%1 modu kontrolü hatası: %2").arg(targetMode, error), LogType::Error);
    modeChecker->deleteLater();
});
}


// Mod dinleyici başlat (ROS /mavros/state üzerinden modu okuyup labelMode'a yaz)
void MainPageWidget::startModeListener() {
    QProcess* modeReader = new QProcess(this);
    QString command = "bash -c 'source /opt/ros/noetic/setup.bash && source ~/catkin_ws/devel/setup.bash && rostopic echo /mavros/state'";
    modeReader->start("bash", QStringList() << "-c" << command);

    static QTimer* modeTimeoutTimer = nullptr;
    if (!modeTimeoutTimer) {
        modeTimeoutTimer = new QTimer(this);
        modeTimeoutTimer->setSingleShot(true);
        QObject::connect(modeTimeoutTimer, &QTimer::timeout, this, [this]() {
            if (labelMode)
                labelMode->setText("Mod Bekleniyor...");
        });
    }
    // Başlangıçta bekleme moduna al ve zamanlayıcıyı başlat
    if (labelMode) labelMode->setText("Mod Bekleniyor...");
    modeTimeoutTimer->start(2000);

    connect(modeReader, &QProcess::readyReadStandardOutput, this, [=]() {
        const QString output = QString::fromUtf8(modeReader->readAllStandardOutput());
        QRegularExpression modeRegex("mode:\\s*\"([^\"]+)\"");
        QRegularExpressionMatchIterator it = modeRegex.globalMatch(output);
        bool anyFound = false;
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            if (m.hasMatch()) {
                const QString mode = m.captured(1);
                if (labelMode) labelMode->setText(mode);
                anyFound = true;
            }
        }
        // Mod akışından güncelleme geliyorsa bekleme zamanlayıcısını yenile
        modeTimeoutTimer->start(2000);
        if (!anyFound && labelMode && labelMode->text().isEmpty()) {
            labelMode->setText("Mod Bekleniyor...");
        }
    });

    connect(modeReader, &QProcess::readyReadStandardError, this, [=]() {
        QString error = QString::fromUtf8(modeReader->readAllStandardError());
        qDebug() << "MODE STDERR:" << error;
        if (labelMode) labelMode->setText("Mod Bekleniyor...");
    });

    connect(modeReader, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [=](int, QProcess::ExitStatus){
        if (labelMode) labelMode->setText("Mod Bekleniyor...");
        // küçük bir gecikme ile tekrar dene
        QTimer::singleShot(1500, this, [this]() { startModeListener(); });
    });
}

// --- TELEMETRYBRIDGE SLOT FONKSİYONLARI ---
// TelemetryBridge'den gelen telemetri verilerini işle
void MainPageWidget::onTelemetryUpdated(double lat, double lon, double alt, double roll, double pitch, double yaw, double speed, double battery) {
    // Harita güncellemesi burada yapılmıyor; sadece log/panel için kullanılabilir
    addLogEntry(QString("İHA Konumu: Lat=%1, Lon=%2, AGL=%3m, Yaw=%4°, Batarya=%5V").arg(lat, 0, 'f', 6).arg(lon, 0, 'f', 6).arg(alt, 0, 'f', 1).arg(yaw, 0, 'f', 1).arg(battery, 0, 'f', 1), Info);
    
    // İrtifa ve hız labellerını güncelle
    updateAltitude(alt);
    updateSpeed(speed);
}

// TelemetryBridge'den gelen harita pozisyon güncellemesini işle
void MainPageWidget::onMapPositionUpdated(double latitude, double longitude, double yaw) {
    Q_UNUSED(latitude);
    Q_UNUSED(longitude);
    Q_UNUSED(yaw);
    // Harita güncellemesi burada yapılmıyor; sadece panel/label amaçlı kullanılabilir
}

// TelemetryBridge'den gelen telemetri verilerini GUI'de göstermek için gerekli güncellemeleri yap
void MainPageWidget::updateTelemetryDisplay(double lat, double lon, double alt, double roll, double pitch, double yaw, double speed, double battery) {
    Q_UNUSED(lat);
    Q_UNUSED(lon);
    Q_UNUSED(roll);
    Q_UNUSED(pitch);
    Q_UNUSED(yaw);
    Q_UNUSED(battery);
    
    // İrtifa ve hız labellerını güncelle
    updateAltitude(alt);
    updateSpeed(speed);
    
    // Harita güncellemesi burada yapılmıyor; sadece panel/label amaçlı kullanılabilir
}

// Sunucu saatini güncelleme
void MainPageWidget::updateServerTime(const QDateTime &serverTime) {
    qDebug() << "=== updateServerTime çağrıldı ===";
    qDebug() << "Sunucu saati:" << serverTime.toString("HH:mm:ss.zzz");
    qDebug() << "Sunucu tarihi:" << serverTime.toString("yyyy-MM-dd");
    
    // QML haritasındaki sunucu saatini güncelle
    if (mapWidget && mapWidget->rootObject()) {
        QObject* mapObject = mapWidget->rootObject();
        
        qDebug() << "Sunucu saati QML haritasına aktarıldı";
    } else {
        qDebug() << "QML harita objesi bulunamadı!";
    }
}

// --- QR KOORDİNATLARI FONKSİYONLARI ---
void MainPageWidget::refreshQrCoordinates() {
    qDebug() << "QR koordinatları yenileniyor...";
    // Sabit QR koordinatları kullan
    const double qrLat = 40.20323;
    const double qrLon = 25.88129;
    qDebug() << "QR koordinatları sabit olarak ayarlanıyor:" << qrLat << qrLon;

    // QML'e QR koordinatlarını gönder
    if (mapWidget && mapWidget->rootObject()) {
        QObject* root = mapWidget->rootObject();
        root->setProperty("qrLat", qrLat);
        root->setProperty("qrLon", qrLon);
        addLogEntry(QString("QR koordinatları güncellendi: Lat=%1, Lon=%2").arg(qrLat, 0, 'f', 10).arg(qrLon, 0, 'f', 10), Info);
    }

    // QR koordinatlarını dosyaya yaz
    writeQrCoordinatesToFile(qrLat, qrLon);
}

void MainPageWidget::onQrCoordinatesReceived(QNetworkReply* reply) {
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response = reply->readAll();
        qDebug() << "QR koordinatları alındı:" << QString::fromUtf8(response);
        
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(response, &error);
        
        if (error.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject obj = doc.object();
            
            if (obj.contains("qrEnlem") && obj.contains("qrBoylam")) {
                double qrLat = obj["qrEnlem"].toDouble();
                double qrLon = obj["qrBoylam"].toDouble();
                
                qDebug() << "QR koordinatları güncellendi:" << qrLat << qrLon;
                
                // QML'e QR koordinatlarını gönder
                QObject* root = mapWidget->rootObject();
                if (root) {
                    root->setProperty("qrLat", qrLat);
                    root->setProperty("qrLon", qrLon);
                    addLogEntry(QString("QR koordinatları güncellendi: Lat=%1, Lon=%2").arg(qrLat, 0, 'f', 10).arg(qrLon, 0, 'f', 10), Info);
                }
                
                // QR koordinatlarını dosyaya yaz
                writeQrCoordinatesToFile(qrLat, qrLon);

            } else {
                qDebug() << "QR koordinatları JSON'da beklenen alanlar bulunamadı";
                addLogEntry("QR koordinatları alınamadı: Geçersiz JSON formatı", Warn);
            }
        } else {
            qDebug() << "QR koordinatları JSON parse hatası:" << error.errorString();
            addLogEntry("QR koordinatları alınamadı: JSON parse hatası", Error);
        }
    } else {
        qDebug() << "QR koordinatları network hatası:" << reply->errorString();
        addLogEntry("QR koordinatları alınamadı: " + reply->errorString(), Error);
    }
    
    reply->deleteLater();
}

void MainPageWidget::setServerUrl(const QString &url) {
    serverUrl = url;
    qDebug() << "MainPageWidget sunucu URL'si ayarlandı:" << serverUrl;
    if (rakipAnaliz_) {
        rakipAnaliz_->setServerUrl(serverUrl);
        qDebug() << "[MainPageWidget] RakipAnaliz serverUrl aktarıldı ve start() çağrılacak.";
        rakipAnaliz_->start();
    }
}

// --- HSS KOORDİNATLARI FONKSİYONLARI ---
void MainPageWidget::refreshHssCoordinates() {
    qDebug() << "HSS koordinatları yenileniyor...";
    
    if (!networkManager) {
        qDebug() << "Network manager bulunamadı!";
        return;
    }
    
    // Sunucu URL'sini kullan
    if (serverUrl.isEmpty()) {
        qDebug() << "Sunucu URL'si ayarlanmamış!";
        addLogEntry("HSS koordinatları alınamadı: Sunucu URL'si ayarlanmamış", Error);
        return;
    }
    
    QString url = serverUrl + "/api/hss_koordinatlari";
    
    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    qDebug() << "HSS koordinatları isteniyor:" << url;
    
    QNetworkReply *reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onHssCoordinatesReceived(reply);
    });
}

void MainPageWidget::onHssCoordinatesReceived(QNetworkReply* reply) {
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response = reply->readAll();
        qDebug() << "HSS koordinatları alındı:" << QString::fromUtf8(response);
        
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(response, &error);
        
        if (error.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject obj = doc.object();
            
            if (obj.contains("hss_koordinat_bilgileri") && obj["hss_koordinat_bilgileri"].isArray()) {
                QJsonArray hssArray = obj["hss_koordinat_bilgileri"].toArray();
                
                qDebug() << "HSS koordinatları sayısı:" << hssArray.size();
                
                // QML'e HSS koordinatlarını gönder
                QObject* root = mapWidget->rootObject();
                if (root) {
                    // HSS koordinatlarını QML'e aktar
                    QVariantList hssCoordinates;
                    
                    for (const QJsonValue& value : hssArray) {
                        if (value.isObject()) {
                            QJsonObject hssObj = value.toObject();
                            
                            if (hssObj.contains("hssEnlem") && hssObj.contains("hssBoylam") && hssObj.contains("hssYaricap")) {
                                double lat = hssObj["hssEnlem"].toDouble();
                                double lon = hssObj["hssBoylam"].toDouble();
                                double radius = hssObj["hssYaricap"].toDouble();
                                int id = hssObj["id"].toInt();
                                
                                QVariantMap hssCoord;
                                hssCoord["id"] = id;
                                hssCoord["lat"] = lat;
                                hssCoord["lon"] = lon;
                                hssCoord["radius"] = radius;
                                
                                hssCoordinates.append(hssCoord);
                                
                                qDebug() << "HSS koordinatı eklendi - ID:" << id << "Lat:" << lat << "Lon:" << lon << "Yarıçap:" << radius;
                            }
                        }
                    }
                    
                    // QML'e HSS koordinatlarını gönder
                    root->setProperty("hssCoordinates", hssCoordinates);
                    qDebug() << "HSS koordinatları QML'e gönderildi, sayı:" << hssCoordinates.size();
                    addLogEntry(QString("HSS koordinatları güncellendi: %1 adet HSS alanı").arg(hssCoordinates.size()), Info);
                    
                    // Statik HSS alanlarını güncelle (ilk 3 koordinat)
                    QMetaObject::invokeMethod(root, "updateStaticHssAreas", Qt::QueuedConnection, 
                                            Q_ARG(QVariant, QVariant::fromValue(hssCoordinates)));
                    
                    // ListModel'i de güncelle (alternatif yöntem)
                    QMetaObject::invokeMethod(root, "updateHssListModel", Qt::QueuedConnection, 
                                            Q_ARG(QVariant, QVariant::fromValue(hssCoordinates)));
                    
                    // HSS koordinatlarını fnc.txt dosyasına yaz
                    writeHssCoordinatesToFile(hssCoordinates);
                }
            } else {
                qDebug() << "HSS koordinatları JSON'da beklenen alanlar bulunamadı";
                addLogEntry("HSS koordinatları alınamadı: Geçersiz JSON formatı", Warn);
            }
        } else {
            qDebug() << "HSS koordinatları JSON parse hatası:" << error.errorString();
            addLogEntry("HSS koordinatları alınamadı: JSON parse hatası", Error);
        }
    } else {
        qDebug() << "HSS koordinatları network hatası:" << reply->errorString();
        addLogEntry("HSS koordinatları alınamadı: " + reply->errorString(), Error);
    }
    
    reply->deleteLater();
}

// HSS Script Functions
void MainPageWidget::onBtnHssBaslatClicked() {
    auto proc = new QProcess(this);
    connect(proc, &QProcess::readyReadStandardOutput, this, [proc](){
        qDebug() << "[HSS]" << QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
    });
    connect(proc, &QProcess::readyReadStandardError, this, [proc](){
        qDebug() << "[HSS-ERR]" << QString::fromUtf8(proc->readAllStandardError()).trimmed();
    });
    connect(proc, qOverload<int,QProcess::ExitStatus>(&QProcess::finished),
            this, [proc](int, QProcess::ExitStatus){ proc->deleteLater(); });

    const QString script = QDir::homePath() + "/test_ws/src/test_pkg/src/HSSAvoid_Start.sh";
    const QString cmd = QString("source ~/test_ws/devel/setup.bash && \"%1\" --ns /mavros --wpl /home/oguz/test_ws/src/test_pkg/src/fnc.txt --keepin /home/oguz/test_ws/src/test_pkg/src/kisit.polygon --keepin_enable 1").arg(script);
    proc->start("bash", {"-lc", cmd});
    if (!proc->waitForStarted(1500)) {
        QMessageBox::critical(this, "HSS", "HSSAvoid_Start.sh başlatılamadı!");
        proc->deleteLater();
    }
}

void MainPageWidget::onBtnHssDurdurClicked() {
    // rosnode list | grep -i hss && kill if /hss_geo_avoid exists
    auto proc = new QProcess(this);
    connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc]() {
        const QString out = QString::fromUtf8(proc->readAllStandardOutput());
        if (out.contains("/hss_geo_avoid")) {
            addLogEntry("/hss_geo_avoid bulundu, kill komutu gönderiliyor...", Warn);
            // hss_geo_avoid PID bul ve öldür (rosnode kill daha doğru, ama gerekirse killall)
            QProcess* killer = new QProcess(this);
            connect(killer, qOverload<int,QProcess::ExitStatus>(&QProcess::finished), this, [this, killer](int, QProcess::ExitStatus){
                addLogEntry("HSS düğümü kapatıldı.", Info);
                killer->deleteLater();
                hssRunning_ = false;
                updateHssButtonUi();
                
                // HSS durdurulduktan sonra Auto moda geç
                addLogEntry("Auto moda geçiliyor...", Info);
                if (rosBridge) {
                    if (rosBridge->setMode("AUTO")) {
                        addLogEntry("Auto moda başarıyla geçildi.", Info);
                    } else {
                        addLogEntry("Auto moda geçilemedi!", LogType::Error);
                    }
                } else {
                    addLogEntry("RosBridge bulunamadı, Auto moda geçilemedi!", LogType::Error);
                }
            });
            const QString killCmd = "source /opt/ros/noetic/setup.bash && rosnode kill /hss_geo_avoid";
            killer->start("bash", {"-lc", killCmd});
        } else {
            addLogEntry("/hss_geo_avoid bulunamadı.", Warn);
            hssRunning_ = false;
            updateHssButtonUi();
            
            // HSS düğümü bulunamadığında da Auto moda geç
            addLogEntry("Auto moda geçiliyor...", Info);
            if (rosBridge) {
                if (rosBridge->setMode("AUTO")) {
                    addLogEntry("Auto moda başarıyla geçildi.", Info);
                } else {
                    addLogEntry("Auto moda geçilemedi!", LogType::Error);
                }
            } else {
                addLogEntry("RosBridge bulunamadı, Auto moda geçilemedi!", LogType::Error);
            }
        }
    });
    connect(proc, &QProcess::readyReadStandardError, this, [this, proc]() {
        const QString err = QString::fromUtf8(proc->readAllStandardError());
        if (!err.trimmed().isEmpty()) addLogEntry(err.trimmed(), LogType::Error);
    });
    connect(proc, qOverload<int,QProcess::ExitStatus>(&QProcess::finished), this, [proc](int, QProcess::ExitStatus){ proc->deleteLater(); });
    const QString listCmd = "source /opt/ros/noetic/setup.bash && rosnode list | grep -i hss";
    proc->start("bash", {"-lc", listCmd});
}

// HSS koordinatlarını fnc.txt dosyasına yazma fonksiyonu (QGC WPL formatında)
void MainPageWidget::writeHssCoordinatesToFile(const QVariantList& hssCoordinates) {
    const QString filePath = QDir::homePath() + "/test_ws/src/test_pkg/src/fnc.txt";
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "fnc.txt dosyası yazılamıyor:" << filePath;
        addLogEntry("HSS koordinatları fnc.txt dosyasına yazılamadı: Dosya açılamadı", Error);
        return;
    }
    
    QTextStream out(&file);
    
    // QGC WPL header'ı yaz
    out << "QGC WPL 110\n";
    
    // HSS koordinatlarını QGC WPL formatında yaz
    for (const QVariant& coordVar : hssCoordinates) {
        QVariantMap coord = coordVar.toMap();
        
        int id = coord["id"].toInt();
        double lat = coord["lat"].toDouble();
        double lon = coord["lon"].toDouble();
        double radius = coord["radius"].toDouble();
        
        // QGC WPL formatında yaz:
        // ID, Frame, Command, Param1, Param2, Param3, Param4, Lat, Lon, Alt, Autocontinue
        // 0, 1, 0, 16, 0, 0, 0, 0, Lat, Lon, 30.000000, 1
        out << QString("%1\t1\t0\t16\t0\t0\t0\t0\t%2\t%3\t30.000000\t1\n")
               .arg(id)
               .arg(lat, 0, 'f', 7)  // 7 ondalık basamak
               .arg(lon, 0, 'f', 7); // 7 ondalık basamak
    }
    
    file.close();
    
    qDebug() << "HSS koordinatları fnc.txt dosyasına QGC WPL formatında yazıldı:" << filePath;
    addLogEntry(QString("HSS koordinatları fnc.txt dosyasına QGC WPL formatında yazıldı: %1 adet koordinat").arg(hssCoordinates.size()), Info);
}

// QR koordinatlarını qrKoordinat.txt dosyasına yazma fonksiyonu
void MainPageWidget::writeQrCoordinatesToFile(double qrLat, double qrLon) {
    const QString filePath = "/home/oguz/test_ws/src/test_pkg/src/qrKoordinat.txt";
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "qrKoordinat.txt dosyası yazılamıyor:" << filePath;
        addLogEntry("QR koordinatları qrKoordinat.txt dosyasına yazılamadı: Dosya açılamadı", Error);
        return;
    }
    
    QTextStream out(&file);
    
    // QR koordinatlarını tek satırda yaz (enlem,boylam formatında)
    out << QString("%1,%2\n").arg(qrLat, 0, 'f', 15).arg(qrLon, 0, 'f', 15);
    
    file.close();
    
    qDebug() << "QR koordinatları qrKoordinat.txt dosyasına yazıldı:" << filePath;
    addLogEntry(QString("QR koordinatları qrKoordinat.txt dosyasına yazıldı: Lat=%1, Lon=%2").arg(qrLat, 0, 'f', 10).arg(qrLon, 0, 'f', 10), Info);
}

// Kamikaze başlatma fonksiyonu
void MainPageWidget::startKamikaze() {
    qDebug() << "=== [DEBUG] startKamikaze() fonksiyonu başladı ===";
    qDebug() << "[DEBUG] Zaman:" << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    qDebug() << "[DEBUG] Thread ID:" << QThread::currentThreadId();
    qDebug() << "[DEBUG] MainPageWidget pointer:" << this;
    
    addLogEntry("Kamikaze başlatılıyor...", Warn);
    
    // Kamikaze başlatıldı olarak işaretle
    kamikazeRunning_ = true;
    updateKamikazeButtonUi();

    const QString script = "/home/oguz/test_ws/src/test_pkg/src/kamikaze_start.sh";
    qDebug() << "[DEBUG] Script yolu:" << script;
    qDebug() << "[DEBUG] Script yolu (absolute):" << QFileInfo(script).absoluteFilePath();
    qDebug() << "[DEBUG] Script yolu (canonical):" << QFileInfo(script).canonicalFilePath();
    
    // Dosya detayları
    QFileInfo scriptInfo(script);
    qDebug() << "[DEBUG] Script dosya bilgileri:";
    qDebug() << "  - Mevcut:" << scriptInfo.exists();
    qDebug() << "  - Boyut:" << scriptInfo.size() << "bytes";
    qDebug() << "  - Oluşturulma:" << scriptInfo.birthTime().toString("yyyy-MM-dd hh:mm:ss");
    qDebug() << "  - Son değişiklik:" << scriptInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss");
    qDebug() << "  - Okunabilir:" << scriptInfo.isReadable();
    qDebug() << "  - Yazılabilir:" << scriptInfo.isWritable();
    qDebug() << "  - Çalıştırılabilir:" << scriptInfo.isExecutable();
    qDebug() << "  - Dosya türü:" << scriptInfo.suffix();
    qDebug() << "  - Dosya adı:" << scriptInfo.fileName();
    qDebug() << "  - Dizin:" << scriptInfo.absolutePath();
    
    if (!QFile::exists(script)) {
        qDebug() << "[DEBUG] Script dosyası bulunamadı!";
        qDebug() << "[DEBUG] Dizin içeriği:";
        QDir scriptDir = QFileInfo(script).absoluteDir();
        if (scriptDir.exists()) {
            QStringList files = scriptDir.entryList(QDir::Files);
            for (const QString& file : files) {
                qDebug() << "  - " << file;
            }
        } else {
            qDebug() << "  - Dizin mevcut değil!";
        }
        addLogEntry("kamikaze_start.sh bulunamadı: " + script, LogType::Error);
        return;
    }
    
    qDebug() << "[DEBUG] Script dosyası bulundu, boyut:" << QFileInfo(script).size() << "bytes";

    // Mevcut izinleri kontrol et
    QFile::Permissions currentPerms = QFile::permissions(script);
    qDebug() << "[DEBUG] Mevcut izinler:";
    qDebug() << "  - Owner Read:" << (currentPerms & QFileDevice::ReadOwner ? "Evet" : "Hayır");
    qDebug() << "  - Owner Write:" << (currentPerms & QFileDevice::WriteOwner ? "Evet" : "Hayır");
    qDebug() << "  - Owner Execute:" << (currentPerms & QFileDevice::ExeOwner ? "Evet" : "Hayır");
    qDebug() << "  - Group Read:" << (currentPerms & QFileDevice::ReadGroup ? "Evet" : "Hayır");
    qDebug() << "  - Group Write:" << (currentPerms & QFileDevice::WriteGroup ? "Evet" : "Hayır");
    qDebug() << "  - Group Execute:" << (currentPerms & QFileDevice::ExeGroup ? "Evet" : "Hayır");
    qDebug() << "  - Other Read:" << (currentPerms & QFileDevice::ReadOther ? "Evet" : "Hayır");
    qDebug() << "  - Other Write:" << (currentPerms & QFileDevice::WriteOther ? "Evet" : "Hayır");
    qDebug() << "  - Other Execute:" << (currentPerms & QFileDevice::ExeOther ? "Evet" : "Hayır");

    // Çalıştırılabilir izni ver (emin olmak için)
    QFile::setPermissions(script, currentPerms
        | QFileDevice::ExeOwner | QFileDevice::ExeGroup | QFileDevice::ExeUser);
    
    QFile::Permissions newPerms = QFile::permissions(script);
    qDebug() << "[DEBUG] Yeni izinler:";
    qDebug() << "  - Owner Execute:" << (newPerms & QFileDevice::ExeOwner ? "Evet" : "Hayır");
    qDebug() << "  - Group Execute:" << (newPerms & QFileDevice::ExeGroup ? "Evet" : "Hayır");
    qDebug() << "  - Other Execute:" << (newPerms & QFileDevice::ExeOther ? "Evet" : "Hayır");
    
    qDebug() << "[DEBUG] Script çalıştırma izni verildi";

    // QProcess ile çalıştır
    qDebug() << "[DEBUG] QProcess oluşturuluyor...";
    QProcess* proc = new QProcess(this);
    qDebug() << "[DEBUG] QProcess oluşturuldu, pointer:" << proc;
    qDebug() << "[DEBUG] QProcess parent:" << proc->parent();
    
    proc->setProcessChannelMode(QProcess::MergedChannels);
    qDebug() << "[DEBUG] Process channel mode: MergedChannels";
    
    qDebug() << "[DEBUG] QProcess oluşturuldu, ID:" << proc->processId();
    qDebug() << "[DEBUG] QProcess state:" << proc->state();
    qDebug() << "[DEBUG] QProcess working directory:" << proc->workingDirectory();

    // Signal bağlantıları
    qDebug() << "[DEBUG] Signal bağlantıları kuruluyor...";
    
    connect(proc, &QProcess::readyReadStandardOutput, [this, proc]() {
        qDebug() << "[DEBUG] readyReadStandardOutput sinyali alındı";
        const QString out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        qDebug() << "[DEBUG] Ham çıktı uzunluğu:" << out.length() << "karakter";
        
        if (!out.isEmpty()) {
            // ANSI escape kodlarını temizle
            QString cleanOut = out;
            int beforeLength = cleanOut.length();
            cleanOut.remove(QRegularExpression("\\u001B\\[[0-9;]*[a-zA-Z]"));
            cleanOut.remove(QRegularExpression("\\u001B\\[0m"));
            int afterLength = cleanOut.length();
            
            qDebug() << "[DEBUG] ANSI kodları temizlendi:" << beforeLength << "->" << afterLength << "karakter";
            qDebug() << "[DEBUG] Temizlenmiş çıktı:" << cleanOut;
            addLogEntry(cleanOut, Info);
        } else {
            qDebug() << "[DEBUG] Boş çıktı, loglanmadı";
        }
    });
    
    connect(proc, &QProcess::readyReadStandardError, [this, proc]() {
        qDebug() << "[DEBUG] readyReadStandardError sinyali alındı";
        const QString err = QString::fromUtf8(proc->readAllStandardError()).trimmed();
        qDebug() << "[DEBUG] Ham hata çıktısı uzunluğu:" << err.length() << "karakter";
        
        if (!err.isEmpty()) {
            // ANSI escape kodlarını temizle
            QString cleanErr = err;
            int beforeLength = cleanErr.length();
            cleanErr.remove(QRegularExpression("\\u001B\\[[0-9;]*[a-zA-Z]"));
            cleanErr.remove(QRegularExpression("\\u001B\\[0m"));
            int afterLength = cleanErr.length();
            
            qDebug() << "[DEBUG] ANSI kodları temizlendi:" << beforeLength << "->" << afterLength << "karakter";
            qDebug() << "[DEBUG] Temizlenmiş hata çıktısı:" << cleanErr;
            addLogEntry(cleanErr, LogType::Error);
        } else {
            qDebug() << "[DEBUG] Boş hata çıktısı, loglanmadı";
        }
    });
    
    connect(proc, &QProcess::started, [this, proc]() {
        qDebug() << "[DEBUG] Process started sinyali alındı";
        qDebug() << "[DEBUG] Process ID:" << proc->processId();
        qDebug() << "[DEBUG] Process state:" << proc->state();
    });
    
    connect(proc, &QProcess::errorOccurred, [this, proc](QProcess::ProcessError error) {
        qDebug() << "[DEBUG] Process error sinyali alındı:" << error;
        qDebug() << "[DEBUG] Error string:" << proc->errorString();
        qDebug() << "[DEBUG] Process state:" << proc->state();
    });
    
    connect(proc, qOverload<int,QProcess::ExitStatus>(&QProcess::finished),
            this, [this, proc](int code, QProcess::ExitStatus st) {
        qDebug() << "[DEBUG] Process finished sinyali alındı";
        qDebug() << "[DEBUG] Exit code:" << code;
        qDebug() << "[DEBUG] Exit status:" << (st == QProcess::NormalExit ? "Normal" : "Crashed");
        qDebug() << "[DEBUG] Process state:" << proc->state();
        qDebug() << "[DEBUG] Process ID:" << proc->processId();
        
        addLogEntry(QString("kamikaze_start.sh bitti (code=%1, status=%2)")
                        .arg(code)
                        .arg(st == QProcess::NormalExit ? "Normal" : "Crashed"),
                    st == QProcess::NormalExit ? Info : LogType::Error);
        
        qDebug() << "[DEBUG] Process temizleniyor...";
        proc->deleteLater();
        qDebug() << "[DEBUG] Process temizlendi";
    });

    // ROS ortamını yükle ve script'i çalıştır
    qDebug() << "[DEBUG] Komut hazırlanıyor...";
    const QString cmd =
        "source /opt/ros/noetic/setup.bash && "
        "source ~/test_ws/devel/setup.bash && "
        "\"" + script + "\"";
    
    qDebug() << "[DEBUG] Çalıştırılacak komut:" << cmd;
    qDebug() << "[DEBUG] Komut uzunluğu:" << cmd.length() << "karakter";
    qDebug() << "[DEBUG] Script başlatılıyor...";
    qDebug() << "[DEBUG] Bash path:" << QStandardPaths::findExecutable("bash");
    qDebug() << "[DEBUG] Current working directory:" << QDir::currentPath();
    qDebug() << "[DEBUG] Home directory:" << QDir::homePath();
    qDebug() << "[DEBUG] Temp directory:" << QDir::tempPath();

    qDebug() << "[DEBUG] Process start çağrılıyor...";
    proc->start("bash", QStringList() << "-lc" << cmd);
    qDebug() << "[DEBUG] Process start çağrıldı";
    
    qDebug() << "[DEBUG] Process başlatma bekleniyor...";
    if (proc->waitForStarted(5000)) {  // 5 saniye bekle
        qDebug() << "[DEBUG] Script başarıyla başlatıldı";
        qDebug() << "[DEBUG] Process ID:" << proc->processId();
        qDebug() << "[DEBUG] Process state:" << proc->state();
        qDebug() << "[DEBUG] Process program:" << proc->program();
        qDebug() << "[DEBUG] Process arguments:" << proc->arguments();
    } else {
        qDebug() << "[DEBUG] Script başlatılamadı!";
        qDebug() << "[DEBUG] Process error:" << proc->error();
        qDebug() << "[DEBUG] Process error string:" << proc->errorString();
        qDebug() << "[DEBUG] Process state:" << proc->state();
    }
    
    addLogEntry("Kamikaze komutu gönderildi.", Info);
    qDebug() << "[DEBUG] Log entry eklendi";
    qDebug() << "=== [DEBUG] startKamikaze() fonksiyonu tamamlandı ===";
}

// Kamikaze durdurma fonksiyonu
void MainPageWidget::onBtnKamikazeDurdurClicked() {
    addLogEntry("Kamikaze durdurma işlemi başlatılıyor...", Warn);
    
    // Kamikaze node'larını durdur
    QProcess* stopProc = new QProcess(this);
    
    connect(stopProc, &QProcess::readyReadStandardOutput, this, [this, stopProc]() {
        const QString out = QString::fromUtf8(stopProc->readAllStandardOutput()).trimmed();
        if (!out.isEmpty()) {
            addLogEntry(QString("[kamikaze-stop] %1").arg(out), Info);
        }
    });
    
    connect(stopProc, &QProcess::readyReadStandardError, this, [this, stopProc]() {
        const QString err = QString::fromUtf8(stopProc->readAllStandardError()).trimmed();
        if (!err.isEmpty()) {
            addLogEntry(QString("[kamikaze-stop:ERR] %1").arg(err), LogType::Error);
        }
    });
    
    connect(stopProc, qOverload<int,QProcess::ExitStatus>(&QProcess::finished), this, [this, stopProc](int code, QProcess::ExitStatus status) {
        addLogEntry(QString("Kamikaze durdurma işlemi bitti (code=%1, status=%2)").arg(code).arg(status), Info);
        stopProc->deleteLater();
        
        // Kamikaze durumunu güncelle
        kamikazeRunning_ = false;
        updateKamikazeButtonUi();
        
        // Kamikaze durdurulduktan sonra Auto moda geç
        addLogEntry("Auto moda geçiliyor...", Info);
        if (rosBridge) {
            if (rosBridge->setMode("AUTO")) {
                addLogEntry("Auto moda başarıyla geçildi.", Info);
            } else {
                addLogEntry("Auto moda geçilemedi!", LogType::Error);
            }
        } else {
            addLogEntry("RosBridge bulunamadı, Auto moda geçilemedi!", LogType::Error);
        }
    });
    
    // Kamikaze node'larını kill et
    const QString stopCmd = "source ~/test_ws/devel/setup.bash && rosnode kill /kamikaze_dive_final /kamikaze_start_stop 2>/dev/null || true";
    stopProc->start("bash", {"-lc", stopCmd});
    
    if (!stopProc->waitForStarted(3000)) {
        addLogEntry("Kamikaze durdurma komutu başlatılamadı!", LogType::Error);
        stopProc->deleteLater();
        kamikazeRunning_ = false;
        updateKamikazeButtonUi();
    }
}

// ===== KİLİTLENME SCRIPT FONKSİYONLARI =====

QString MainPageWidget::kilitlenmeScriptPath() const {
    return QDir::homePath() + "/test_ws/src/kilitlenme_son/scripts/kilitlenme_all_in_one.sh";
}

void MainPageWidget::runKilitlenmeScript() {
    const QString script = kilitlenmeScriptPath();

    if (!QFile::exists(script)) {
        addLogEntry(QString("Script bulunamadı: %1").arg(script), Error);
        return;
    }

    // Varsa önceki süreci kapat (aynı anda bir kere çalışsın)
    stopKilitlenmeScript();

    kilitlenmeProc_ = new QProcess(this);

    // Script kendi içinde setup/roscore/node'ları açıyor
    const QString cmd = QString("'%1'").arg(script);

    kilitlenmeProc_->setProgram("/bin/bash");
    kilitlenmeProc_->setArguments({"-lc", cmd});

    // Log akışı
    connect(kilitlenmeProc_, &QProcess::readyReadStandardOutput, this, [this]() {
        const QString out = QString::fromUtf8(kilitlenmeProc_->readAllStandardOutput()).trimmed();
        if (!out.isEmpty()) addLogEntry(QString("[kilitlenme] %1").arg(out), Info);
    });
    connect(kilitlenmeProc_, &QProcess::readyReadStandardError, this, [this]() {
        const QString err = QString::fromUtf8(kilitlenmeProc_->readAllStandardError()).trimmed();
        if (!err.isEmpty()) addLogEntry(QString("[kilitlenme:ERR] %1").arg(err));
    });
    connect(kilitlenmeProc_, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus){
        addLogEntry(QString("kilitlenme_all_in_one.sh bitti (exit=%1)").arg(code));
    });

    kilitlenmeProc_->start();

    if (!kilitlenmeProc_->waitForStarted(3000)) {
        addLogEntry("kilitlenme_all_in_one.sh başlatılamadı!", Error);
        stopKilitlenmeScript();
        return;
    }

    addLogEntry("Kilitlenme başlatıldı ✅", Info);
    
    // Kilitlenme script'i başlatıldıktan sonra kilit durumunu sıfırla
    if (telemetryBridge_) {
        telemetryBridge_->setLockActive(false);
        addLogEntry("Kilit durumu sıfırlandı (0)", Info);
    }
}

void MainPageWidget::stopKilitlenmeScript() {
    if (!kilitlenmeProc_) return;
    kilitlenmeProc_->terminate();
    kilitlenmeProc_->waitForFinished(1000);
    kilitlenmeProc_->kill();
    kilitlenmeProc_->deleteLater();
    kilitlenmeProc_ = nullptr;
}

// ===== ROS'TAN GELEN RAKİP İHA VERİLERİ =====

void MainPageWidget::onRivalsFromPoseList(QVector<double> lats, QVector<double> lons, QVector<double> alts,
                                          QVector<double> yaws, QVector<int> ids)
{
    const int n = std::min({ lats.size(), lons.size(), alts.size(), yaws.size(), ids.size() });
    if (n <= 0) return;

    bool team10Found = false;
    QPointF team10Coord;
    QPointF firstRivalCoord;
    int processedCount = 0;

    for (int i=0; i<n; ++i) {
        const int teamNo = ids[i];
        const double lat  = lats[i];
        const double lon  = lons[i];
        const double altm = alts[i];
        const double yawd = yaws[i];

        RivalInfo info = rivalsByTeam_.value(teamNo);
        info.latitude       = lat;
        info.longitude      = lon;
        info.altitudeMeters = altm;
        info.yawDeg         = yawd;
        rivalsByTeam_.insert(teamNo, info);

        if (mapWidget && mapWidget->rootObject()) {
            QObject* root = mapWidget->rootObject();
            QMetaObject::invokeMethod(
                root, "updateDynamicIha",
                Q_ARG(QVariant, QVariant::fromValue(teamNo)),
                Q_ARG(QVariant, QVariant::fromValue(lat)),
                Q_ARG(QVariant, QVariant::fromValue(lon)),
                Q_ARG(QVariant, QVariant::fromValue(yawd)),
                Q_ARG(QVariant, QVariant::fromValue(altm)),
                Q_ARG(QVariant, QVariant::fromValue(info.pitchDeg)),
                Q_ARG(QVariant, QVariant::fromValue(info.rollDeg)),
                Q_ARG(QVariant, QVariant::fromValue(info.speed)),
                Q_ARG(QVariant, QVariant::fromValue(info.timeOffsetMs))
                );
        }

        if (processedCount == 0) firstRivalCoord = QPointF(lon, lat);
        if (teamNo == 10) { team10Found = true; team10Coord = QPointF(lon, lat); }
        ++processedCount;
    } // ✅ for kapanışı

    // burada zoom/merkezleme işlemleri vs.
    if (processedCount > 0 && mapWidget && mapWidget->rootObject() && centerOnNextRivals_) {
        QObject* root = mapWidget->rootObject();
        QObject* mapObj = root->findChild<QObject*>("mapView");
        const QPointF target = team10Found ? team10Coord : firstRivalCoord;

        QMetaObject::invokeMethod(
            mapObj, "centerOnCoordinate",
            Q_ARG(QVariant, QVariant::fromValue(target.y())), // lat
            Q_ARG(QVariant, QVariant::fromValue(target.x())), // lon
            Q_ARG(QVariant, QVariant::fromValue(16))
            );
        addLogEntry(team10Found ? "Takım 10'a merkezlendi." : "İlk rakibe merkezlendi.", Info);
        centerOnNextRivals_ = false;
    }
} // ✅ fonksiyon kapanışı

// İrtifa güncelleme fonksiyonu
void MainPageWidget::updateAltitude(double altitude) {
    if (altitudeLabel) {
        altitudeLabel->setText(QString("%1 m").arg(altitude, 0, 'f', 1));
    }
}

// Hız güncelleme fonksiyonu
void MainPageWidget::updateSpeed(double speed) {
    if (speedLabel) {
        speedLabel->setText(QString("%1 m/s").arg(speed, 0, 'f', 1));
    }
}
