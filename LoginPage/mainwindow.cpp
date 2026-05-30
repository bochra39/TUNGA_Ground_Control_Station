#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QApplication>
#include <QIcon>
#include <QSizePolicy>
#include <QMessageBox>
#include <QTransform>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include "MainPageWidget.h"
#include <QStackedWidget>
#include "checklist.h"
#include <QInputDialog>
#include <QTcpSocket>
#include <QProcess>
// config.h kaldırıldı
#include "../SimulasyonPage/SimulasyonPage.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonParseError>
#include <QNetworkCookieJar>
#include <sensor_msgs/TimeReference.h>
#include <mavros_msgs/RCIn.h>
#include <cmath>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("Görev Kontrol Arayüzü");
    menuBar()->hide();
    statusBar()->hide();

    // --- Üst Bar ---
    topBar = new QFrame;
    topBar->setFixedHeight(90);
    topBarLayout = new QHBoxLayout(topBar);
    topBarLayout->setSpacing(1);
    topBarLayout->setContentsMargins(0, 0, 0, 0);
    for (int i = 0; i < 7; i++) {
        QFrame *section = new QFrame;
        section->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        QVBoxLayout *sectionLayout = new QVBoxLayout(section);
        sectionLayout->setAlignment(Qt::AlignCenter);
        if (i >= 0 && i <= 4) {
            QLabel *iconLabel = new QLabel;
            iconLabel->setStyleSheet("border: none;");
            QString iconPath;
            if (i == 0) iconPath = ":/icons/Plane.png";
            else if (i == 1) iconPath = ":/icons/Gps.png";
            else if (i == 2) iconPath = ":/icons/Power.png";
            else if (i == 3) iconPath = ":/icons/Server.png";
            else if (i == 4) iconPath = ":/icons/Telemetry.png";
            iconLabel->setPixmap(QPixmap(iconPath).scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            sectionLayout->addWidget(iconLabel);
            // Durum etiketleri için stil
            QString statusLabelStyle = "font-weight: bold; font-size: 12pt; margin-top: 2px; background-color: #232b3a; color: white; border: none;";
            // Plane
            if (i == 0) {
                QWidget* planeContainer = new QWidget;
                QVBoxLayout* planeLayout = new QVBoxLayout(planeContainer);
                planeLayout->setContentsMargins(0, 0, 0, 0);
                planeLayout->setSpacing(2);
                planeLayout->setAlignment(Qt::AlignCenter);
                iconLabel->setAlignment(Qt::AlignCenter);
                planeLayout->addWidget(iconLabel, 0, Qt::AlignHCenter);
                planeStatusLabel = new QLabel("Plane : <span style='color:#c0392b'>Bağlı Değil</span>");
                planeStatusLabel->setAlignment(Qt::AlignCenter);
                planeStatusLabel->setTextFormat(Qt::RichText);
                planeStatusLabel->setStyleSheet(statusLabelStyle);
                planeStatusLabel->setVisible(true);
                planeLayout->addWidget(planeStatusLabel, 0, Qt::AlignHCenter);
                sectionLayout->addWidget(planeContainer, 0, Qt::AlignCenter);
            }
            // GPS
            if (i == 1) {
                QWidget* gpsContainer = new QWidget;
                QVBoxLayout* gpsLayout = new QVBoxLayout(gpsContainer);
                gpsLayout->setContentsMargins(0, 0, 0, 0);
                gpsLayout->setSpacing(2);
                gpsLayout->setAlignment(Qt::AlignCenter);
                iconLabel->setAlignment(Qt::AlignCenter);
                gpsLayout->addWidget(iconLabel, 0, Qt::AlignHCenter);
                QLabel* gpsTextLabel = new QLabel("GPS");
                gpsTextLabel->setAlignment(Qt::AlignCenter);
                gpsTextLabel->setStyleSheet("color: #232b3a; font-weight: bold; font-size: 12px; margin-top: 2px;");
                gpsLayout->addWidget(gpsTextLabel, 0, Qt::AlignHCenter);
                gpsStatusLabel = new QLabel("GPS : <span style='color:#c0392b'>Bağlı Değil</span>");
                gpsStatusLabel->setAlignment(Qt::AlignCenter);
                gpsStatusLabel->setTextFormat(Qt::RichText);
                gpsStatusLabel->setStyleSheet(statusLabelStyle);
                gpsStatusLabel->setVisible(true);
                gpsLayout->addWidget(gpsStatusLabel, 0, Qt::AlignHCenter);
                sectionLayout->addWidget(gpsContainer, 0, Qt::AlignCenter);
            }
            // Power
            if (i == 2) {
                QWidget* powerContainer = new QWidget;
                QVBoxLayout* powerLayout = new QVBoxLayout(powerContainer);
                powerLayout->setContentsMargins(0, 0, 0, 0);
                powerLayout->setSpacing(2);
                powerLayout->setAlignment(Qt::AlignCenter);
                iconLabel->setAlignment(Qt::AlignCenter);
                powerLayout->addWidget(iconLabel, 0, Qt::AlignHCenter);
                powerStatusLabel = new QLabel("Power : 0%");
                powerStatusLabel->setAlignment(Qt::AlignCenter);
                powerStatusLabel->setTextFormat(Qt::RichText);
                powerStatusLabel->setStyleSheet(statusLabelStyle);
                powerStatusLabel->setVisible(true);
                powerLayout->addWidget(powerStatusLabel, 0, Qt::AlignHCenter);
                sectionLayout->addWidget(powerContainer, 0, Qt::AlignCenter);
            }
            if (i == 3) {
                QWidget* serverContainer = new QWidget;
                QVBoxLayout* serverContainerLayout = new QVBoxLayout(serverContainer);
                serverContainerLayout->setContentsMargins(0, 0, 0, 0);
                serverContainerLayout->setSpacing(2);
                serverContainerLayout->setAlignment(Qt::AlignCenter);
                iconLabel->setAlignment(Qt::AlignCenter);
                serverContainerLayout->addWidget(iconLabel, 0, Qt::AlignHCenter);
                serverStatusLabel = new QLabel("Sunucu : <span style='color:#c0392b'>Bağlantı Yok</span>");
                serverStatusLabel->setAlignment(Qt::AlignCenter);
                serverStatusLabel->setTextFormat(Qt::RichText);
                serverStatusLabel->setStyleSheet("font-weight: bold; font-size: 12pt; margin-top: 2px; background-color: #232b3a; color: white; border: none;");
                serverStatusLabel->setVisible(true);
                serverContainerLayout->addWidget(serverStatusLabel, 0, Qt::AlignHCenter);
                sectionLayout->addWidget(serverContainer, 0, Qt::AlignCenter);
            }
            if (i == 4) {
                QWidget* telemetryContainer = new QWidget;
                QVBoxLayout* telemetryContainerLayout = new QVBoxLayout(telemetryContainer);
                telemetryContainerLayout->setContentsMargins(0, 0, 0, 0);
                telemetryContainerLayout->setSpacing(2);
                telemetryContainerLayout->setAlignment(Qt::AlignCenter);
                
                // Telemetri icon ve RSS label'ı yan yana
                QHBoxLayout* telemetryTopLayout = new QHBoxLayout;
                telemetryTopLayout->setContentsMargins(0, 0, 0, 0);
                telemetryTopLayout->setSpacing(0); // Spacing'i 0 yap (boşluk olmasın)
                
                iconLabel->setAlignment(Qt::AlignCenter);
                telemetryTopLayout->addWidget(iconLabel, 0, Qt::AlignHCenter);
                
                // RSS label'ı sağ üstte - 2 kat büyük boyut (tam sağ üste yapışık)
                rssStatusLabel = new QLabel("RSS: --%");
                rssStatusLabel->setAlignment(Qt::AlignCenter);
                rssStatusLabel->setTextFormat(Qt::PlainText);
                rssStatusLabel->setStyleSheet("font-weight: bold; font-size: 10pt; color: #f1c40f; background-color: #232b3a; padding: 2px 8px; border-radius: 4px;");
                rssStatusLabel->setFixedSize(90, 40); // 2 kat büyük boyut (45x2, 20x2)
                rssStatusLabel->setVisible(true);
                telemetryTopLayout->addWidget(rssStatusLabel, 1, Qt::AlignRight | Qt::AlignTop); // stretch=1 ile sağa yasla
                
                telemetryContainerLayout->addLayout(telemetryTopLayout, 0);
                
                telemetryStatusLabel = new QLabel("Telemetri : <span style='color:#c0392b'>Bağlantı Yok</span>");
                telemetryStatusLabel->setAlignment(Qt::AlignCenter);
                telemetryStatusLabel->setTextFormat(Qt::RichText);
                telemetryStatusLabel->setStyleSheet("font-weight: bold; font-size: 12pt; margin-top: 2px; background-color: #232b3a; color: white; border: none;");
                telemetryStatusLabel->setVisible(true);
                telemetryContainerLayout->addWidget(telemetryStatusLabel, 0, Qt::AlignHCenter);
                sectionLayout->addWidget(telemetryContainer, 0, Qt::AlignCenter);
            }
        } else if (i == 5) {
            section->setStyleSheet("background-color: #dbd7d7; border: none;");
            // Sıfırla/Başlat butonu (tek buton)
            QPushButton *resetButton = new QPushButton("Başlat");
            resetButton->setFixedSize(60, 36);
            QFont btnFont = resetButton->font();
            btnFont.setBold(true);
            resetButton->setFont(btnFont);
            QHBoxLayout* resetLayout = new QHBoxLayout;
            resetLayout->setContentsMargins(0, 0, 0, 0);
            resetLayout->setSpacing(8);
            resetLayout->addStretch();
            resetLayout->addWidget(resetButton);
            competitionTimeLabel = new QLabel("15:00");
            competitionTimeLabel->setAlignment(Qt::AlignCenter);
            competitionTimeLabel->setStyleSheet("background: #232b3a; border-radius: 10px; color: #fff; font-size: 18px; font-weight: bold; margin-left: 8px; padding: 6px 9px;");
            competitionTimeLabel->setCursor(Qt::PointingHandCursor);
            competitionTimeLabel->installEventFilter(this);
            resetLayout->addWidget(competitionTimeLabel);
            resetLayout->addStretch();
            sectionLayout->addLayout(resetLayout);
            // Butonun işlevi: toggle başlat/sıfırla
            connect(resetButton, &QPushButton::clicked, this, [this, resetButton]() {
                if (!isCompetitionRunning) {
                    isCompetitionRunning = true;
                    competitionTimer->start();
                    resetButton->setText("Sıfırla");
                } else {
                    isCompetitionRunning = false;
                    competitionTimer->stop();
                    competitionSecondsLeft = 15 * 60;
                    competitionTimeLabel->setStyleSheet("background: #232b3a; border-radius: 10px; color: #fff; font-size: 18px; font-weight: bold; margin-left: 8px; padding: 6px 9px;");
                    competitionTimeLabel->setText("15:00");
                    resetButton->setText("Başlat");
                }
            });
        } else if (i == 6) {
            lightButton = new QPushButton;
            lightButton->setObjectName("lightButton");
            lightButton->setFixedSize(60, 36); // Daha küçük boyut
            serverTimeLabel = new QLabel("--:--:--.---");
            serverTimeLabel->setAlignment(Qt::AlignCenter);
            serverTimeLabel->setStyleSheet("background: #232b3a; border-radius: 10px; color: #fff; font-size: 18px; font-weight: bold; margin-left: 8px; padding: 6px 9px;");
            dateLabel = new QLabel(this);
            dateLabel->setStyleSheet("font-size: 10px; color: #000; font-weight: bold; background: transparent;");
            dateLabel->setText("");
            dateLabel->setAlignment(Qt::AlignRight | Qt::AlignTop);
            QHBoxLayout* lightTimeLayout = new QHBoxLayout;
            lightTimeLayout->setContentsMargins(0, 0, 0, 0);
            lightTimeLayout->setSpacing(4);
            lightTimeLayout->addWidget(lightButton, 0, Qt::AlignVCenter);
            QVBoxLayout* timeLayout = new QVBoxLayout;
            timeLayout->setContentsMargins(0, 0, 0, 0);
            timeLayout->setSpacing(0);
            timeLayout->addWidget(serverTimeLabel, 0, Qt::AlignRight);
            timeLayout->addWidget(dateLabel, 0, Qt::AlignRight);
            lightTimeLayout->addLayout(timeLayout, 0);
            sectionLayout->addLayout(lightTimeLayout);
        }
        // Section borderlarını profesyonel şekilde ayarla
        if (i == 0 || i == 1 || i == 2)
            section->setStyleSheet("background-color: #dbd7d7; border: none;");
        else
            section->setStyleSheet("background-color: #dbd7d7; border: none;");
        topBarLayout->addWidget(section, 1);
        // Sadece ikonlu bölümlerden (i=0-4) sonra dikey divider ekle
        if (i >= 0 && i < 5) {
            QFrame *divider = new QFrame;
            divider->setFrameShape(QFrame::VLine);
            divider->setFrameShadow(QFrame::Plain);
            divider->setLineWidth(1);
            divider->setStyleSheet("color: #bbbbbb; background: #bbbbbb; margin-top: 12px; margin-bottom: 12px;");
            divider->setFixedWidth(1);
            topBarLayout->addWidget(divider);
        }
        // Timer başlat ve timer label'dan (i==5) sonra da divider ekle
        if (i == 5) {
            QFrame *divider = new QFrame;
            divider->setFrameShape(QFrame::VLine);
            divider->setFrameShadow(QFrame::Plain);
            divider->setLineWidth(1);
            divider->setStyleSheet("color: #bbbbbb; background: #bbbbbb; margin-top: 12px; margin-bottom: 12px;");
            divider->setFixedWidth(1);
            topBarLayout->addWidget(divider);
        }
        // Son bölümden (i==6) sonra da divider ekle (isteğe bağlı, kaldırmak istersen bu kısmı silebilirsin)
        if (i == 6) {
            QFrame *divider = new QFrame;
            divider->setFrameShape(QFrame::VLine);
            divider->setFrameShadow(QFrame::Plain);
            divider->setLineWidth(1);
            divider->setStyleSheet("color: #bbbbbb; background: #bbbbbb; margin-top: 12px; margin-bottom: 12px;");
            divider->setFixedWidth(1);
            topBarLayout->addWidget(divider);
        }
    }

    // --- Sol Menü ---
    leftMenu = new QFrame;
    leftMenu->setFixedWidth(80);
    leftMenu->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    leftMenu->setMinimumHeight(0);
    leftMenu->setStyleSheet("background-color: #353232; border: none;");
    QStringList iconFiles = {"Menu.png","HomePage.png","Login.png", "Checklist.png","Simulasyon.png"};
    leftMenuLayout = new QVBoxLayout(leftMenu);
    leftMenuLayout->setContentsMargins(0, 0, 0, 0);
    leftMenuLayout->setSpacing(0);
    leftMenuLayout->addSpacing(20);
    int activeMenuIndex = 0;
    for (int i = 0; i < iconFiles.size(); ++i) {
        QPushButton *iconBtn = new QPushButton(leftMenu);
        iconBtn->setFixedSize(60, 60);
        iconBtn->setIcon(QIcon(QString(":/icons/%1").arg(iconFiles[i])));
        iconBtn->setIconSize(QSize(36, 36));
        iconBtn->setCursor(Qt::PointingHandCursor);
        iconBtn->setFlat(true);
        if (i == activeMenuIndex) {
            iconBtn->setStyleSheet(
                "QPushButton { background: #444; border-radius: 12px; border: none; margin: 0; }"
                "QPushButton:hover { background: #444; border-radius: 12px; }"
            );
        } else {
            iconBtn->setStyleSheet(
                "QPushButton { background: transparent; border: none; margin: 0; }"
                "QPushButton:hover { background: #444; border-radius: 12px; }"
            );
        }
        leftMenuLayout->addWidget(iconBtn, 0, Qt::AlignHCenter);
        menuButtons.append(iconBtn);
        if (i == 1) leftMenuLayout->addSpacing(18);
        else leftMenuLayout->addSpacing(10);
    }
    // --- Menü butonu dışında kalanları başta gizle ---
    for (int i = 1; i < menuButtons.size(); ++i) {
        menuButtons[i]->setVisible(false);
    }
    // Menü butonuna tıklanınca diğerlerini göster/gizle
    if (!menuButtons.isEmpty()) {
        connect(menuButtons[0], &QPushButton::clicked, this, [this]() {
            bool anyVisible = false;
            for (int i = 1; i < menuButtons.size(); ++i) {
                if (menuButtons[i]->isVisible()) { anyVisible = true; break; }
            }
            for (int i = 1; i < menuButtons.size(); ++i) {
                menuButtons[i]->setVisible(!anyVisible);
            }
        });
    }
    for (int i = 0; i < menuButtons.size(); ++i) {
        connect(menuButtons[i], &QPushButton::clicked, this, [i, this]() {
            for (int j = 0; j < menuButtons.size(); ++j) {
                if (i == j) {
                    menuButtons[j]->setStyleSheet(
                        "QPushButton { background: #444; border-radius: 12px; border: none; margin: 0; }"
                        "QPushButton:hover { background: #444; border-radius: 12px; }"
                    );
                } else {
                    menuButtons[j]->setStyleSheet(
                        "QPushButton { background: transparent; border: none; margin: 0; }"
                        "QPushButton:hover { background: #444; border-radius: 12px; }"
                    );
                }
            }
        });
    }
    leftMenuLayout->addStretch();
    leftMenu->setLayout(leftMenuLayout);

    // --- İçerik Alanı ---
    stackedContent = new QStackedWidget(this);
    stackedContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    // Login sayfası (mevcut contentWidget)
    contentWidget = new QWidget();
    contentLayout = new QHBoxLayout(contentWidget);
    contentLayout->setSpacing(50);
    contentLayout->setContentsMargins(0, 0, 0, 0); // Sağ kenara yapışık olsun

    // --- Kilitlenme ve Kamikaze Paneli (telemetri panelinin solunda) ---
    QFrame* lockPanel = new QFrame;
    lockPanel->setFixedWidth(300);
    lockPanel->setStyleSheet("background-color: #232b3a; border-radius: 16px; border: 2.5px solid #e74c3c; color: #e9f1f7; padding: 18px;");
    QVBoxLayout* lockPanelLayout = new QVBoxLayout(lockPanel);
    lockPanelLayout->setContentsMargins(0, 0, 0, 0);
    lockPanelLayout->setSpacing(12);
    
    // Üst: Kilitlenme Paneli
    QFrame* lockSection = new QFrame;
    lockSection->setStyleSheet("background-color: #1a1f2a; border-radius: 12px; border: 1.5px solid #e74c3c; padding: 12px;");
    QVBoxLayout* lockSectionLayout = new QVBoxLayout(lockSection);
    lockSectionLayout->setContentsMargins(0, 0, 0, 0);
    lockSectionLayout->setSpacing(8);
    
    QLabel* lockPanelTitle = new QLabel("<b><span style='color:#e74c3c;'>Kilitlenme Paneli</span></b>");
    lockPanelTitle->setAlignment(Qt::AlignCenter);
    lockPanelTitle->setStyleSheet("font-size: 16px; margin-bottom: 8px; color: #e74c3c;");
    lockSectionLayout->addWidget(lockPanelTitle);
    
    // Kilitlenme bilgisi göstergesi
    lockInfoLabel = new QLabel("Kilitlenme Bilgisi:\n--");
    lockInfoLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    lockInfoLabel->setTextFormat(Qt::PlainText);
    lockInfoLabel->setStyleSheet("font-size: 16px; margin: 15px 0; color: #e9f1f7; background-color: #232b3a; padding: 20px; border-radius: 12px; min-height: 180px;");
    lockInfoLabel->setWordWrap(true);
    lockSectionLayout->addWidget(lockInfoLabel);
    
    lockPanelLayout->addWidget(lockSection);
    
    // Alt: Kamikaze Paneli
    QFrame* kamikazeSection = new QFrame;
    kamikazeSection->setStyleSheet("background-color: #1a1f2a; border-radius: 12px; border: 1.5px solid #e67e22; padding: 12px;");
    QVBoxLayout* kamikazeSectionLayout = new QVBoxLayout(kamikazeSection);
    kamikazeSectionLayout->setContentsMargins(0, 0, 0, 0);
    kamikazeSectionLayout->setSpacing(8);
    
    QLabel* kamikazePanelTitle = new QLabel("<b><span style='color:#e67e22;'>Kamikaze Paneli</span></b>");
    kamikazePanelTitle->setAlignment(Qt::AlignCenter);
    kamikazePanelTitle->setStyleSheet("font-size: 16px; margin-bottom: 8px; color: #e67e22;");
    kamikazeSectionLayout->addWidget(kamikazePanelTitle);
    
    // Kamikaze bilgisi göstergesi
    kamikazeInfoLabel = new QLabel("Kamikaze Bilgisi:\n--");
    kamikazeInfoLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    kamikazeInfoLabel->setTextFormat(Qt::PlainText);
    kamikazeInfoLabel->setStyleSheet("font-size: 16px; margin: 15px 0; color: #e9f1f7; background-color: #232b3a; padding: 20px; border-radius: 12px; min-height: 180px;");
    kamikazeInfoLabel->setWordWrap(true);
    kamikazeSectionLayout->addWidget(kamikazeInfoLabel);
    
    lockPanelLayout->addWidget(kamikazeSection);
    
    // İrtifa ve hız bilgisi için 2 satırlık panel
    QFrame* altitudeSpeedSection = new QFrame;
    altitudeSpeedSection->setStyleSheet("background-color: #1a1f2a; border-radius: 12px; border: 1.5px solid #f1c40f; padding: 12px;");
    QHBoxLayout* altitudeSpeedLayout = new QHBoxLayout(altitudeSpeedSection);
    altitudeSpeedLayout->setContentsMargins(0, 0, 0, 0);
    altitudeSpeedLayout->setSpacing(20);
    
    // İrtifa label'ı
    QVBoxLayout* altitudeLayout = new QVBoxLayout();
    altitudeLayout->setContentsMargins(0, 0, 0, 0);
    altitudeLayout->setSpacing(4);
    QLabel* altitudeTitle = new QLabel("<b><span style='color:#f1c40f;'>İrtifa</span></b>");
    altitudeTitle->setAlignment(Qt::AlignCenter);
    altitudeTitle->setStyleSheet("font-size: 14px; color: #f1c40f;");
    altitudeLayout->addWidget(altitudeTitle);
    
    altitudeLabel = new QLabel("-- m");
    altitudeLabel->setAlignment(Qt::AlignCenter);
    altitudeLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #e9f1f7; background-color: #232b3a; padding: 8px; border-radius: 8px; min-width: 80px; border: 2px solid #f1c40f;");
    altitudeLayout->addWidget(altitudeLabel);
    
    // Hız label'ı
    QVBoxLayout* speedLayout = new QVBoxLayout();
    speedLayout->setContentsMargins(0, 0, 0, 0);
    speedLayout->setSpacing(4);
    QLabel* speedTitle = new QLabel("<b><span style='color:#f1c40f;'>Hız</span></b>");
    speedTitle->setAlignment(Qt::AlignCenter);
    speedTitle->setStyleSheet("font-size: 14px; color: #f1c40f;");
    speedLayout->addWidget(speedTitle);
    
    speedLabel = new QLabel("-- m/s");
    speedLabel->setAlignment(Qt::AlignCenter);
    speedLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #e9f1f7; background-color: #232b3a; padding: 8px; border-radius: 8px; min-width: 80px; border: 2px solid #f1c40f;");
    speedLayout->addWidget(speedLabel);
    
    altitudeSpeedLayout->addLayout(altitudeLayout);
    altitudeSpeedLayout->addLayout(speedLayout);
    
    lockPanelLayout->addWidget(altitudeSpeedSection);
    lockPanelLayout->addStretch();
    lockPanel->setLayout(lockPanelLayout);
    lockPanel->setMinimumHeight(0);
    lockPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    // --- Telemetri Paneli (kilitlenme panelinin sağında) ---
    QFrame* leftPanel = new QFrame;
    leftPanel->setFixedWidth(450);
    leftPanel->setStyleSheet("background-color: #232b3a; border-radius: 16px; border: 2.5px solid #537fe7; color: #e9f1f7; padding: 18px;");
    QVBoxLayout* leftPanelLayout = new QVBoxLayout(leftPanel);
    leftPanelLayout->setContentsMargins(0, 0, 0, 0);
    leftPanelLayout->setSpacing(0);
    QLabel* panelTitle = new QLabel("<b><span style='color:#537fe7;'>Telemetri Paneli</span></b>");
    panelTitle->setAlignment(Qt::AlignCenter);
    panelTitle->setStyleSheet("font-size: 18px; margin-bottom: 10px; color: #537fe7;");
    leftPanelLayout->addWidget(panelTitle);
    telemetryDisplay = new QTextEdit;
    telemetryDisplay->setReadOnly(true);
    telemetryDisplay->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    telemetryDisplay->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    telemetryDisplay->setFrameStyle(QFrame::NoFrame);
    telemetryDisplay->setStyleSheet("background: transparent; border: none; color: #e9f1f7; font-size: 14px; min-height: 414px; max-height: 414px; padding: 8px; line-height: 18px;");
    leftPanelLayout->addWidget(telemetryDisplay, 1);
    leftPanelLayout->addStretch();
    leftPanel->setLayout(leftPanelLayout);
    leftPanel->setMinimumHeight(0);
    leftPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    // --- Login Bölümü ---
    loginSection = new QWidget();
    loginLayout = new QVBoxLayout(loginSection);
    QString labelStyle = "QLabel { color: #FFFFFF; font-weight: bold; font-size: 11pt; }";
    QString lineEditStyle = "QLineEdit { background-color: #3D3D3D; border: 1px solid #2A2A2A; border-radius: 4px; color: #FFFFFF; padding: 5px; }";
    QString buttonStyle = "QPushButton { "
                         "    background-color: #2c3e50; "
                         "    color: #ecf0f1; "
                         "    font-family: 'Segoe UI', Arial, sans-serif; "
                         "    font-size: 12pt; "
                         "    font-weight: 600; "
                         "    border-radius: 8px; "
                         "    border: 1px solid #3498db; "
                         "    padding: 8px 16px; "
                         "    transition: all 0.3s ease; "
                         "} "
                         "QPushButton:hover { "
                         "    background-color: #3498db; "
                         "    color: #ffffff; "
                         "    border: 1px solid #2980b9; "
                         "    box-shadow: 0 2px 8px rgba(0, 0, 0, 0.2); "
                         "} "
                         "QPushButton:pressed { "
                         "    background-color: #2980b9; "
                         "    color: #ffffff; "
                         "    border: 1px solid #1f618d; "
                         "    box-shadow: inset 0 2px 4px rgba(0, 0, 0, 0.3); "
                         "}";
    serverLabel = new QLabel("Sunucu Yolu :");
    serverLabel->setStyleSheet(labelStyle);
    serverLineEdit = new QLineEdit();
    serverLineEdit->setStyleSheet(lineEditStyle);
    serverLineEdit->setText("10.0.0.10:10001"); // Varsayılan sunucu adresi
    userLabel = new QLabel("Kullanıcı Adı :");
    userLabel->setStyleSheet(labelStyle);
    userLineEdit = new QLineEdit();
    userLineEdit->setStyleSheet(lineEditStyle);
    userLineEdit->setText("tunga"); // Kullanıcı adı
    passLabel = new QLabel("Şifre :");
    passLabel->setStyleSheet(labelStyle);
    passLineEdit = new QLineEdit();
    passLineEdit->setEchoMode(QLineEdit::Password);
    passLineEdit->setStyleSheet(lineEditStyle);
    passLineEdit->setText("XdGbP6Wwza"); // Şifre
    loginButton = new QPushButton("Giriş Yap");
    loginButton->setStyleSheet(buttonStyle);
    loginButton->setMinimumHeight(40);
    statusLabel = new QLabel("");
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setStyleSheet("QLabel { background-color: #5A5A5A; color: white; border-radius: 8px; padding: 8px; font-size: 10pt; }");
    loginLayout->addWidget(serverLabel);
    loginLayout->addWidget(serverLineEdit);
    loginLayout->addSpacing(10);
    loginLayout->addWidget(userLabel);
    loginLayout->addWidget(userLineEdit);
    loginLayout->addSpacing(10);
    loginLayout->addWidget(passLabel);
    loginLayout->addWidget(passLineEdit);
    loginLayout->addSpacing(25);
    loginLayout->addWidget(loginButton);
    loginLayout->addSpacing(15);
    loginLayout->addWidget(statusLabel);
    loginLayout->addStretch();

    // --- Data Bölümü ---
    dataSection = new QWidget();
    dataLayout = new QGridLayout(dataSection);
    fetchLabel = new QLabel("İHA Verilerini Çek :");
    fetchLabel->setStyleSheet(labelStyle);
    fetchButton = new QPushButton("Verileri Al");
    fetchButton->setStyleSheet(buttonStyle);
    sendLabel = new QLabel("Verileri Sunucuya Gönder");
    sendLabel->setStyleSheet(labelStyle);
    sendButton = new QPushButton("Telemetri Bas");
    sendButton->setStyleSheet(buttonStyle);
    stopLabel = new QLabel("Verileri Gönderimini Durdur");
    stopLabel->setStyleSheet(labelStyle);
    stopButton = new QPushButton("Telemetri Durdur");
    stopButton->setStyleSheet(buttonStyle);
    dataLayout->addWidget(fetchLabel, 0, 0, Qt::AlignRight);
    dataLayout->addWidget(fetchButton, 0, 1);
    dataLayout->addItem(new QSpacerItem(10, 25, QSizePolicy::Minimum, QSizePolicy::Fixed), 1, 0, 1, 2);
    dataLayout->addWidget(sendLabel, 2, 0, Qt::AlignRight);
    dataLayout->addWidget(sendButton, 2, 1);
    dataLayout->addItem(new QSpacerItem(10, 25, QSizePolicy::Minimum, QSizePolicy::Fixed), 3, 0, 1, 2);
    dataLayout->addWidget(stopLabel, 4, 0, Qt::AlignRight);
    dataLayout->addWidget(stopButton, 4, 1);
    dataLayout->setRowStretch(5, 1);
    // Telemetri panelini ve loginSection'u yan yana ekle
    contentLayout->addWidget(loginSection);   // Sol: Login formu
    contentLayout->addWidget(dataSection);    // Diğer içerik
    contentLayout->addStretch();           // Stretch panelden önce!
    contentLayout->addWidget(lockPanel);   // Kilitlenme paneli (telemetri panelinin solunda)
    contentLayout->addWidget(leftPanel);   // Telemetri paneli (en sağda)
    contentWidget->setLayout(contentLayout);
    stackedContent->addWidget(contentWidget); // index 0: login

    // MainPageWidget (index 1)
    MainPageWidget* mainPage = new MainPageWidget(this);
    mainPageWidget = mainPage; // Pointer'ı ayarla
    stackedContent->addWidget(mainPage);
    

    
    // MainPageWidget'a sunucu URL'sini set et
    if (mainPageWidget) {
        QString serverAddress = serverLineEdit->text().trimmed();
        if (!serverAddress.isEmpty()) {
            if (!serverAddress.startsWith("http://") && !serverAddress.startsWith("http://")) {
                serverAddress = "http://" + serverAddress;
            }
            mainPageWidget->setServerUrl(serverAddress);
        }
    }

    // Checklist (index 2)
    Checklist* checklistPage = new Checklist(this);
    stackedContent->addWidget(checklistPage);
    // SimulasyonPage (index 3)
    SimulasyonPage* simulasyonPage = new SimulasyonPage(this);
    stackedContent->addWidget(simulasyonPage);

    // --- Tema ve Layout ---
    isLightMode = false;
    QPixmap lightModeOriginal(":/icons/LightMode.png");
    QPixmap darkModeOriginal(":/icons/DarkMode.png");
    QTransform transform;
    transform.rotate(210);
    lightModeIcon = lightModeOriginal.transformed(transform);
    darkModeIcon = darkModeOriginal.transformed(transform);
    allLabels << serverLabel << userLabel << passLabel << statusLabel << fetchLabel << sendLabel << stopLabel;
    auto applyTheme = [this](bool lightMode) {
        if (!lightButton) return;
        if (lightMode) {
            this->setStyleSheet("background-color: #F0FFFF;");
            topBar->setStyleSheet("background-color: #e8e8e8;");
            for (int i = 0; i < topBarLayout->count(); ++i) {
                if (auto s = qobject_cast<QFrame*>(topBarLayout->itemAt(i)->widget()))
                    s->setStyleSheet(i < 6 ? "background-color:#e8e8e8;" : "background-color:#e8e8e8;");
            }
            for (auto l : allLabels) {
                if (l) l->setStyleSheet("color: #111; font-weight: bold; font-size: 11pt;");
            }
            lightButton->setIcon(QIcon(darkModeIcon.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        } else {
            this->setStyleSheet("background-color: #464646;");
            topBar->setStyleSheet("background-color: #dbd7d7;");
            for (int i = 0; i < topBarLayout->count(); ++i) {
                if (auto s = qobject_cast<QFrame*>(topBarLayout->itemAt(i)->widget()))
                    s->setStyleSheet(i < 6 ? "background-color:#dbd7d7;" : "background-color:#dbd7d7;");
            }
            for (auto l : allLabels) {
                if (l) l->setStyleSheet("color: #FFF; font-weight: bold; font-size: 11pt;");
            }
            lightButton->setIcon(QIcon(lightModeIcon.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        }
        lightButton->setIconSize(QSize(40, 40));
    };
    if (lightButton) {
        connect(lightButton, &QPushButton::clicked, this, [this, applyTheme]() mutable {
            isLightMode = !isLightMode;
            applyTheme(isLightMode);
        });
    }
    applyTheme(false);

    // Sunucu saatini periyodik çekme kaldırıldı

    // --- Ana Layout ---
    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(leftMenu, 0);
    mainLayout->addWidget(stackedContent, 1);
    QVBoxLayout *windowLayout = new QVBoxLayout;
    windowLayout->setContentsMargins(0, 0, 0, 0);
    windowLayout->setSpacing(0);
    windowLayout->addWidget(topBar);
    windowLayout->addLayout(mainLayout, 1);
    QWidget *central = new QWidget(this);
    central->setLayout(windowLayout);
    central->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setCentralWidget(central);

    networkManager = new QNetworkAccessManager(this);
    connect(loginButton, &QPushButton::clicked, this, &MainWindow::handleLogin);
    
    // HTTP yanıtlarını dinle
    connect(this, &MainWindow::httpSuccess, this, [this](const QJsonObject &response) {
        handleLoginSuccess(response);
    });
    connect(this, &MainWindow::httpError, this, [this](const QString &error) {
        handleLoginError(error);
    });

    // ROS Telemetri Listener'ı devre dışı bırak - sadece TelemetryBridge kullanılacak
    rosListener = nullptr; // Null pointer olarak ayarla

    // Ortak cookie jar kullan
    QNetworkCookieJar* sharedJar = new QNetworkCookieJar(this);
    networkManager->setCookieJar(sharedJar);
    if (rosListener) {
        rosListener->setCookieJar(sharedJar);
    }

    // --- Buton bağlantıları ---
    connect(fetchButton, &QPushButton::clicked, this, &MainWindow::onFetchButtonClicked);
    connect(sendButton, &QPushButton::clicked, this, &MainWindow::onSendButtonClicked);
    connect(stopButton, &QPushButton::clicked, this, &MainWindow::onStopButtonClicked);
    
    // Sunucu adresi değişikliklerini MainPageWidget'a bildir
    connect(serverLineEdit, &QLineEdit::editingFinished, this, [this]() {
        if (mainPageWidget) {
            QString serverAddress = serverLineEdit->text().trimmed();
            if (!serverAddress.isEmpty()) {
                QString normalized = serverAddress;
                if (!normalized.startsWith("http://") && !normalized.startsWith("https://")) {
                    normalized = "http://" + normalized;
                }
                mainPageWidget->setServerUrl(normalized);
            }
        }
    });

    // --- HomePage butonuna tıklanınca MainPageWidget'e geçiş ---
    if (menuButtons.size() > 1) {
        connect(menuButtons[1], &QPushButton::clicked, this, [this]() {
            stackedContent->setCurrentIndex(1); // MainPageWidget
        });
    }
    // --- LoginPage butonuna tıklanınca Login ekranına geçiş ---
    if (menuButtons.size() > 2) {
        connect(menuButtons[2], &QPushButton::clicked, this, [this]() {
            stackedContent->setCurrentIndex(0); // LoginPage
        });
    }
    // --- Checklist butonuna tıklanınca Checklist ekranına geçiş ---
    if (menuButtons.size() > 3) {
        connect(menuButtons[3], &QPushButton::clicked, this, [this]() {
            stackedContent->setCurrentIndex(2); // Checklist
        });
    }
    // --- Simulasyon butonuna tıklanınca SimulasyonPage ekranına geçiş ---
    if (menuButtons.size() > 4) {
        connect(menuButtons[4], &QPushButton::clicked, this, [this]() {
            stackedContent->setCurrentIndex(3); // SimulasyonPage
        });
    }

    // --- Yarışma Süresi Timer ---
    competitionTimer = new QTimer(this);
    competitionTimer->setInterval(1000);
    connect(competitionTimer, &QTimer::timeout, this, &MainWindow::updateCompetitionTime);

    // --- Telemetri Gönderme Timer ---
    telemetrySendTimer = new QTimer(this);
    telemetrySendTimer->setInterval(500); // 2 Hz
    connect(telemetrySendTimer, &QTimer::timeout, this, &MainWindow::sendTelemetryToServer);
    
    


    // Local saat bir kez çekildi, şimdi arayüzde otomatik ilerlet
    QDateTime localTime = QDateTime::currentDateTime();
    serverTimeLabel->setText(localTime.toString("HH:mm:ss.zzz"));
    dateLabel->setText(localTime.toString("yyyy-MM-dd"));
    QPoint labelPos = serverTimeLabel->mapToParent(QPoint(serverTimeLabel->width() - dateLabel->width() - 8, 4));
    dateLabel->move(labelPos);
    dateLabel->raise();
    dateLabel->show();
    this->currentServerTime = localTime;

    // Local saat timer'ı
    QTimer* localTimeTimer = new QTimer(this);
    localTimeTimer->setTimerType(Qt::PreciseTimer);
    localTimeTimer->setInterval(10);
    connect(localTimeTimer, &QTimer::timeout, this, [this]() {
        QDateTime displayedTime;
        if (serverTimeInitialized) {
            // Sunucudan alınan baz saate göre ilerlet
            qint64 elapsedMs = serverElapsed.elapsed();
            displayedTime = serverBaseTime.addMSecs(elapsedMs);
        } else {
            displayedTime = QDateTime::currentDateTime();
        }
        serverTimeLabel->setText(displayedTime.toString("HH:mm:ss.zzz"));
        dateLabel->setText(displayedTime.toString("yyyy-MM-dd"));
        this->currentServerTime = displayedTime;
    });
    localTimeTimer->start();
    
    // TelemetryBridge signal'ını MainPageWidget'a bağla
    if (mainPageWidget && telemetryBridge) {
        connect(telemetryBridge, &TelemetryBridge::telemetryUpdated, this, [this](double lat, double lon, double alt, double roll, double pitch, double yaw, double speed, double battery) {
            // GPS verisi geldiğinde üst bardaki GPS durumunu güncelle
            if (gpsStatusLabel && lat != 0 && lon != 0) {
                gpsStatusLabel->setText("GPS : <span style='color:#27ae60'>Bağlı</span>");
                lastGpsDataTime = QDateTime::currentDateTime(); // Son GPS verisi zamanını güncelle
            }
            
            // Power status label'ı batarya voltajı ile güncelle
            if (powerStatusLabel) {
                powerStatusLabel->setText(QString("Power : <span style='color:#27ae60'>%1V</span>")
                                            .arg(battery, 0, 'f', 1));
            }
            
            if (mainPageWidget) {
                mainPageWidget->updateTelemetryDisplay(lat, lon, alt, roll, pitch, yaw, speed, battery);
            }
            
            // Telemetri panelini ve TelemetryBridge iha_otonom değerini güncelle
            if (telemetryBridge && mainPageWidget) {
                int otonom = mainPageWidget->isAutonomousMode() ? 1 : 0;
                telemetryBridge->setAutonomousFlag(otonom);
            }
            // Telemetri panelini de güncelle (eğer gönderim modunda değilse ve görüntüleme açıksa)
            if (telemetryDisplay && !isTelemetrySending && isTelemetryViewing) {
                QDateTime currentTime = hasGpsTimeRef ? lastGpsTimeRef : QDateTime::currentDateTime();
                if (serverOffsetMs != 0) {
                    currentTime = currentTime.addMSecs(serverOffsetMs);
                }
                QJsonObject gpsTime;
                gpsTime["saat"] = currentTime.time().hour();
                gpsTime["dakika"] = currentTime.time().minute();
                gpsTime["saniye"] = currentTime.time().second();
                gpsTime["milisaniye"] = currentTime.time().msec();
                
                // Güncel telemetri verilerini JSON formatında göster
                QString jsonString = "{\n";
                // Takım numarasını telemetryBridge'den al, yoksa 0 yaz
                int bridgeTeam = telemetryBridge ? telemetryBridge->teamNumber() : 0;
                jsonString += "    \"takim_numarasi\": " + QString::number(bridgeTeam) + ",\n";
                jsonString += "    \"iha_enlem\": " + QString::number(lat, 'f', 7) + ",\n";
                jsonString += "    \"iha_boylam\": " + QString::number(lon, 'f', 7) + ",\n";
                jsonString += "    \"iha_irtifa\": " + QString::number((int)alt) + ",\n";
                jsonString += "    \"iha_dikilme\": " + QString::number((int)pitch) + ",\n";
                jsonString += "    \"iha_yatis\": " + QString::number((int)roll) + ",\n";
                jsonString += "    \"iha_yonelme\": " + QString::number((int)yaw) + ",\n";
                jsonString += "    \"iha_hiz\": " + QString::number((int)speed) + ",\n";
                // Battery artık direkt percentage olarak geliyor
                double rawBattery = telemetryBridge ? telemetryBridge->currentData().battery : 0;
                qDebug() << "=== MainWindow Battery Debug ===";
                qDebug() << "Battery percentage from bridge:" << rawBattery << "%";
                
                int batteryPerc = (int)std::round(rawBattery);
                qDebug() << "Final battery percentage:" << batteryPerc << "%";
                
                jsonString += "    \"iha_batarya\": " + QString::number(batteryPerc) + ",\n";
                int otonom = (mainPageWidget && mainPageWidget->isAutonomousMode()) ? 1 : 0;
                jsonString += "    \"iha_otonom\": " + QString::number(otonom) + ",\n";
                int kilit = (telemetryBridge && telemetryBridge->isLockActive()) ? 1 : 0;
                jsonString += "    \"iha_kilitlenme\": " + QString::number(kilit) + ",\n";
                if (hasHedefPiksel) {
                    jsonString += "    \"hedef_merkez_X\": " + QString::number(hedefMerkezX) + ",\n";
                    jsonString += "    \"hedef_merkez_Y\": " + QString::number(hedefMerkezY) + ",\n";
                    jsonString += "    \"hedef_genislik\": " + QString::number(hedefGenislik) + ",\n";
                    jsonString += "    \"hedef_yukseklik\": " + QString::number(hedefYukseklik) + ",\n";
                } else {
                    jsonString += "    \"hedef_merkez_X\": ,\n";
                    jsonString += "    \"hedef_merkez_Y\": ,\n";
                    jsonString += "    \"hedef_genislik\": ,\n";
                    jsonString += "    \"hedef_yukseklik\": ,\n";
                }
                jsonString += "    \"gps_saati\": {\n";
                jsonString += "        \"saat\": " + QString::number(gpsTime["saat"].toInt()) + ",\n";
                jsonString += "        \"dakika\": " + QString::number(gpsTime["dakika"].toInt()) + ",\n";
                jsonString += "        \"saniye\": " + QString::number(gpsTime["saniye"].toInt()) + ",\n";
                jsonString += "        \"milisaniye\": " + QString::number(gpsTime["milisaniye"].toInt()) + "\n";
                jsonString += "    }\n";
                jsonString += "}";
                
                telemetryDisplay->setText(jsonString);
            }
        });
        
        // MainPageWidget'taki harita güncellemesi için signal'ları bağla
        connect(telemetryBridge, &TelemetryBridge::telemetryUpdated, 
                mainPageWidget, &MainPageWidget::onTelemetryUpdated);
        // Rakip İHA çekme talebi geldiğinde bir defalık rivals fetch başlat
        connect(mainPageWidget, &MainPageWidget::rivalsFetchRequested, this, &MainWindow::onRivalsFetchRequested);
    }

    // --- TelemetryBridge ve ROS Spin Timer ---
    telemetryBridge = new TelemetryBridge(this);
    // Paylaşılan cookie jar'ı MainPageWidget'a da geçir (HSS/QR/RakipAnaliz için)
    if (mainPageWidget && sharedJar) {
        mainPageWidget->setCookieJar(sharedJar);
    }
    
    // TelemetryBridge'e ortak cookie jar'ı set et
    telemetryBridge->setCookieJar(sharedJar);
    
    // Takım numarası login sonrasında belirlenecek; başlangıçta 0
    telemetryBridge->setTeamNumber(teamNumber);
    
    // Otonom durum kontrolü için callback'i bağla
    if (mainPageWidget) {
        telemetryBridge->setAutonomousModeCallback([this]() {
            return mainPageWidget->isAutonomousMode();
        });
    }
    
    // Otomatik aboneliği kaldır: Kullanıcı isteğiyle başlatılacak
    qDebug() << "TelemetryBridge oluşturuldu; ROS subscriber'ları kullanıcı isteğiyle başlayacak";

    // Telemetri panelini gerçek TelemetryBridge verisiyle güncelle
    connect(telemetryBridge, &TelemetryBridge::telemetryUpdated, this, [this](double lat, double lon, double alt, double roll, double pitch, double yaw, double speed, double battery) {
        // GPS verisi geldiğinde üst bardaki GPS durumunu güncelle
        if (gpsStatusLabel && lat != 0 && lon != 0) {
            gpsStatusLabel->setText("GPS : <span style='color:#27ae60'>Bağlı</span>");
            lastGpsDataTime = QDateTime::currentDateTime(); // Son GPS verisi zamanını güncelle
        }
        
        // Power status label'ı batarya yüzdesi ile güncelle (telemetri panelindeki gibi)
        if (powerStatusLabel) {
            // Battery artık direkt percentage olarak geliyor
            int batteryPerc = static_cast<int>(std::round(battery));
            
            // Renk kodunu batarya seviyesine göre ayarla
            QString colorCode;
            if (batteryPerc > 50) {
                colorCode = "#27ae60"; // Yeşil
            } else if (batteryPerc > 20) {
                colorCode = "#f39c12"; // Turuncu
            } else {
                colorCode = "#e74c3c"; // Kırmızı
            }
            
            powerStatusLabel->setText(QString("Power : <span style='color:%1'>%2%</span>")
                                        .arg(colorCode).arg(batteryPerc));
        }
        
        if (telemetryDisplay && !isTelemetrySending && isTelemetryViewing) {
            QDateTime currentTime = hasGpsTimeRef ? lastGpsTimeRef : QDateTime::currentDateTime();
            if (serverOffsetMs != 0) {
                currentTime = currentTime.addMSecs(serverOffsetMs);
            }
            QJsonObject gpsTime;
            gpsTime["saat"] = currentTime.time().hour();
            gpsTime["dakika"] = currentTime.time().minute();
            gpsTime["saniye"] = currentTime.time().second();
            gpsTime["milisaniye"] = currentTime.time().msec();

            QString jsonString = "{\n";
            int bridgeTeam = telemetryBridge ? telemetryBridge->teamNumber() : 0;
            jsonString += "    \"takim_numarasi\": " + QString::number(bridgeTeam) + ",\n";
            jsonString += "    \"iha_enlem\": " + QString::number(lat, 'f', 7) + ",\n";
            jsonString += "    \"iha_boylam\": " + QString::number(lon, 'f', 7) + ",\n";
            jsonString += "    \"iha_irtifa\": " + QString::number((int)alt) + ",\n";
            jsonString += "    \"iha_dikilme\": " + QString::number((int)pitch) + ",\n";
            jsonString += "    \"iha_yonelme\": " + QString::number((int)yaw) + ",\n";
            jsonString += "    \"iha_yatis\": " + QString::number((int)roll) + ",\n";
            jsonString += "    \"iha_hiz\": " + QString::number((int)speed) + ",\n";
                // Battery artık direkt percentage olarak geliyor
                double rawBattery2 = telemetryBridge ? telemetryBridge->currentData().battery : 0;
                qDebug() << "=== MainWindow Battery Debug 2 ===";
                qDebug() << "Battery percentage from bridge:" << rawBattery2 << "%";
                
                int batteryPerc2 = (int)std::round(rawBattery2);
                qDebug() << "Final battery percentage:" << batteryPerc2 << "%";
                
                jsonString += "    \"iha_batarya\": " + QString::number(batteryPerc2) + ",\n";
            int otonom2 = (mainPageWidget && mainPageWidget->isAutonomousMode()) ? 1 : 0;
            jsonString += "    \"iha_otonom\": " + QString::number(otonom2) + ",\n";
            int kilit2 = (telemetryBridge && telemetryBridge->isLockActive()) ? 1 : 0;
            jsonString += "    \"iha_kilitlenme\": " + QString::number(kilit2) + ",\n";
            if (hasHedefPiksel) {
                jsonString += "    \"hedef_merkez_X\": " + QString::number(hedefMerkezX) + ",\n";
                jsonString += "    \"hedef_merkez_Y\": " + QString::number(hedefMerkezY) + ",\n";
                jsonString += "    \"hedef_genislik\": " + QString::number(hedefGenislik) + ",\n";
                jsonString += "    \"hedef_yukseklik\": " + QString::number(hedefYukseklik) + ",\n";
            } else {
                jsonString += "    \"hedef_merkez_X\": ,\n";
                jsonString += "    \"hedef_merkez_Y\": ,\n";
                jsonString += "    \"hedef_genislik\": ,\n";
                jsonString += "    \"hedef_yukseklik\": ,\n";
            }
            jsonString += "    \"gps_saati\": {\n";
            jsonString += "        \"saat\": " + QString::number(gpsTime["saat"].toInt()) + ",\n";
            jsonString += "        \"dakika\": " + QString::number(gpsTime["dakika"].toInt()) + ",\n";
            jsonString += "        \"saniye\": " + QString::number(gpsTime["saniye"].toInt()) + ",\n";
            jsonString += "        \"milisaniye\": " + QString::number(gpsTime["milisaniye"].toInt()) + "\n";
            jsonString += "    }\n";
            jsonString += "}";

            telemetryDisplay->setText(jsonString);
        }
    });
    
    // ROS kilitlenme ve kamikaze subscriber'larını başlat
    rosNodeHandle = new ros::NodeHandle();
    lockInfoSubscriber = rosNodeHandle->subscribe("/kilitlenme_bilgisi", 10, &MainWindow::onLockInfoReceived, this);
    kamikazeInfoSubscriber = rosNodeHandle->subscribe("/qr_data", 10, &MainWindow::onKamikazeInfoReceived, this);
    // Hedef piksel subscriber
    hedefPikselSubscriber = rosNodeHandle->subscribe("/hedef_piksel", 10, &MainWindow::onHedefPikselReceived, this);
    // MAVROS time reference subscriber
    timeRefSubscriber = rosNodeHandle->subscribe("/mavros/time_reference", 10, &MainWindow::onTimeReference, this);
    // MAVROS RC in subscriber (RSS verisi için)
    rcInSubscriber = rosNodeHandle->subscribe("/mavros/rc/in", 10, &MainWindow::onRcInReceived, this);
    
    // ROS rakip İHA publisher'ını başlat
    rivalRos_.startOnce("rival_pub_gui");
    
    // ROS rakip İHA subscriber'ını başlat
    if (rosNodeHandle && !rivalsSub_) {
        rivalsSub_ = new RivalRosSubscriber(this);
        rivalsSub_->start(*rosNodeHandle, "/rivals/json", "/rivals/poses", "/rivals/ids");

        // JSON hattını direkt mevcut parse fonksiyonuna bağla:
        connect(rivalsSub_, &RivalRosSubscriber::rivalsJsonReceived,
                mainPageWidget, &MainPageWidget::onRivalsFromTelemetryJson,
                Qt::QueuedConnection);

        // Pose/IDs hattı için ayrı bir slot yazalım (aşağıda 4. bölüm)
        connect(rivalsSub_, &RivalRosSubscriber::rivalsPoseListReceived,
                mainPageWidget, &MainPageWidget::onRivalsFromPoseList,
                Qt::QueuedConnection);
                
        qDebug() << "ROS rakip İHA subscriber başlatıldı: /rivals/json, /rivals/poses, /rivals/ids";
    }
    qDebug() << "ROS kilitlenme subscriber başlatıldı: /kilitlenme_bilgisi";
    qDebug() << "ROS kamikaze subscriber başlatıldı: /qr_data";
    qDebug() << "ROS hedef_piksel subscriber başlatıldı: /hedef_piksel";
    qDebug() << "ROS time_reference subscriber başlatıldı: /mavros/time_reference";
    qDebug() << "NodeHandle adresi:" << rosNodeHandle;
    qDebug() << "Kilitlenme subscriber adresi:" << &lockInfoSubscriber;
    qDebug() << "Kamikaze subscriber adresi:" << &kamikazeInfoSubscriber;
    qDebug() << "Hedef piksel subscriber adresi:" << &hedefPikselSubscriber;
    qDebug() << "TimeReference subscriber adresi:" << &timeRefSubscriber;
    qDebug() << "Kilitlenme publisher sayısı:" << lockInfoSubscriber.getNumPublishers();
    qDebug() << "Kamikaze publisher sayısı:" << kamikazeInfoSubscriber.getNumPublishers();
    qDebug() << "Hedef piksel publisher sayısı:" << hedefPikselSubscriber.getNumPublishers();
    qDebug() << "TimeReference publisher sayısı:" << timeRefSubscriber.getNumPublishers();

    // /server_time publisher'ı (üst panelde gösterilen zamanı yayınlar)
    if (rosNodeHandle) {
        serverTimePublisher = rosNodeHandle->advertise<sensor_msgs::TimeReference>("/server_time", 10);
        // Timer başlatımı sunucu saati alındıktan sonra yapılacak
    }
    
    // Subscriber'ların düzgün kurulup kurulmadığını kontrol et
    if (lockInfoSubscriber) {
        qDebug() << "Kilitlenme subscriber başarıyla kuruldu";
    } else {
        qDebug() << "HATA: Kilitlenme subscriber kurulamadı!";
    }
    
    if (kamikazeInfoSubscriber) {
        qDebug() << "Kamikaze subscriber başarıyla kuruldu";
    } else {
        qDebug() << "HATA: Kamikaze subscriber kurulamadı!";
    }
    if (hedefPikselSubscriber) {
        qDebug() << "Hedef piksel subscriber başarıyla kuruldu";
    } else {
        qDebug() << "HATA: Hedef piksel subscriber kurulamadı!";
    }
    
    // ROS spin timer'ı (20 Hz)
    rosSpinTimer = new QTimer(this);
    connect(rosSpinTimer, &QTimer::timeout, this, []() {
        ros::spinOnce();
    });
    rosSpinTimer->start(50); // 20 Hz
    
   
    connect(telemetryBridge, &TelemetryBridge::telemetrySent, this, &MainWindow::onTelemetryBridgeResponse);
    
    connect(telemetryBridge, &TelemetryBridge::loginFailed, this, [this]() {
        statusLabel->setText("TelemetryBridge Giriş Başarısız");
    });
    
    connect(telemetryBridge, &TelemetryBridge::loginSuccess, this, [this](int teamNo) {
        statusLabel->setText(QString("TelemetryBridge Giriş Başarılı - Takım: %1").arg(teamNo));
    });
    
    // TelemetryBridge'den gelen harita pozisyon güncellemelerini MainPageWidget'a bağla
    connect(telemetryBridge, &TelemetryBridge::mapPositionUpdated, 
            mainPageWidget, &MainPageWidget::onMapPositionUpdated);
    
    // TelemetryBridge'den gelen telemetri güncellemelerini MainPageWidget'a bağla
    connect(telemetryBridge, &TelemetryBridge::telemetryUpdated, 
            mainPageWidget, &MainPageWidget::onTelemetryUpdated);
    
    // TelemetryBridge'den gelen telemetri güncellemelerini MainWindow'da da dinle (yeşil bilgi paneli için)
    connect(telemetryBridge, &TelemetryBridge::telemetryUpdated, this, [this](double lat, double lon, double alt, double roll, double pitch, double yaw, double speed, double battery) {
        // Yeşil bilgi panelindeki irtifa ve hız label'larını güncelle
        updateAltitude(alt);
        updateSpeed(speed);
        
        // MainPageWidget'daki irtifa ve hız labellerını da güncelle
        if (mainPageWidget) {
            mainPageWidget->updateAltitude(alt);
            mainPageWidget->updateSpeed(speed);
        }
        
        // GPS verisi zamanını güncelle
        lastGpsDataTime = QDateTime::currentDateTime();
        
        // GPS durumunu güncelle
        if (gpsStatusLabel) {
            gpsStatusLabel->setText("GPS : <span style='color:#27ae60'>Bağlı</span>");
        }
    });

    // MainPageWidget'a TelemetryBridge referansını ayarla
    mainPageWidget->setTelemetryBridge(telemetryBridge);
    
    // Kilitlenme butonu -> TelemetryBridge kilit bayrağı
    connect(mainPageWidget, &MainPageWidget::lockButtonClicked, this, &MainWindow::onLockButtonClicked);
    
    // GPS verisi kontrolü için timer başlat
    gpsCheckTimer = new QTimer(this);
    gpsCheckTimer->setInterval(2000); // 2 saniyede bir kontrol et
    connect(gpsCheckTimer, &QTimer::timeout, this, [this]() {
        // Son GPS verisinden 3 saniye geçtiyse GPS'i "Bağlı Değil" yap
        if (lastGpsDataTime.isValid() && lastGpsDataTime.msecsTo(QDateTime::currentDateTime()) > 3000) {
            if (gpsStatusLabel) {
                gpsStatusLabel->setText("GPS : <span style='color:#c0392b'>Bağlı Değil</span>");
            }
        }
    });
    gpsCheckTimer->start();
}


void MainWindow::onLockButtonClicked() {
    if (!telemetryBridge) return;
    // Butona tıklandığında kilit aktif olsun (1). İleride toggle istenirse geliştirilebilir.
    telemetryBridge->setLockActive(true);
}

void MainWindow::onHedefPikselReceived(const std_msgs::String::ConstPtr& msg) {
    const QString payload = QString::fromStdString(msg->data);
    qDebug() << "Hedef piksel mesajı:" << payload;
    // JSON bekleniyor: {"hedef_merkez_X": n, "hedef_merkez_Y": n, "hedef_genislik": n, "hedef_yukseklik": n}
    QJsonParseError perr;
    QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8(), &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "Hedef piksel JSON parse hata:" << perr.errorString();
        hasHedefPiksel = false;
        return;
    }
    QJsonObject obj = doc.object();
    // Değerleri oku
    int yeniMerkezX = obj.value("hedef_merkez_X").toInt(0);
    int yeniMerkezY = obj.value("hedef_merkez_Y").toInt(0);
    int yeniGenislik = obj.value("hedef_genislik").toInt(0);
    int yeniYukseklik = obj.value("hedef_yukseklik").toInt(0);

    // Yalnızca kilit aktifken telemetriyi güncelle
    if (telemetryBridge && telemetryBridge->isLockActive()) {
        hedefMerkezX = yeniMerkezX;
        hedefMerkezY = yeniMerkezY;
        hedefGenislik = yeniGenislik;
        hedefYukseklik = yeniYukseklik;
        telemetryBridge->setTargetDetection(hedefMerkezX, hedefMerkezY, hedefGenislik, hedefYukseklik);
        hasHedefPiksel = true;
    } else {
        // Kilit kapalıysa telemetri tarafı 0 kalır; veriyi işlemez
        hasHedefPiksel = false;
    }
}

void MainWindow::handleLogin() {
    qDebug() << "=== DEBUG: Login İsteği Gönderiliyor ===";
    QString username = userLineEdit->text();
    QString password = passLineEdit->text();
    QString serverAddress = serverLineEdit->text();
    
    qDebug() << "Kullanıcı adı:" << username;
    qDebug() << "Sunucu adresi:" << serverAddress;
    
    // Sunucu adresini kontrol et
    if (serverAddress.isEmpty()) {
        qDebug() << "HATA: Sunucu adresi boş!";
        statusLabel->setText("Lütfen sunucu adresini girin!");
        return;
    }

    // Sunucu adresini normalize et (http/https ekle)
    QString baseUrl = serverAddress.trimmed();
    if (!baseUrl.startsWith("http://") && !baseUrl.startsWith("https://")) {
        baseUrl = "http://" + baseUrl;
        qDebug() << "HTTP protokolü eklendi:" << baseUrl;
    }

    // Giriş verilerini hazırla
    QJsonObject loginData;
    loginData["kadi"] = username;
    loginData["sifre"] = password;
    
    qDebug() << "Login verileri:" << loginData;

    // Login URL'sini oluştur
    QString url = baseUrl.endsWith('/') ? (baseUrl + "api/giris") : (baseUrl + "/api/giris");
    qDebug() << "Login URL:" << url;
    
    qDebug() << "POST isteği gönderiliyor...";
    sendHttpRequest(url, loginData, "POST");
    
    statusLabel->setText("Giriş yapılıyor...");
}

// Eski handleLoginReply fonksiyonu kaldırıldı - yeni handleHttpResponse + handleLoginSuccess kullanılıyor

void MainWindow::fetchTelemetry() {
    // Cookie otomatik olarak gönderiliyor, token kontrolü gerekmez
    
    // Telemetri verisi varsa güzel formatta göster
    if (!lastTelemetryObject.isEmpty()) {
        QString displayText = "Simulasyon Telemetri Verisi:\n\n";
        
        // Takım numarası
        if (lastTelemetryObject.contains("takim_numarasi")) {
            displayText += QString("Takım Numarası: %1\n").arg(lastTelemetryObject["takim_numarasi"].toInt());
        }
        
        // Konum bilgileri
        if (lastTelemetryObject.contains("iha_enlem") && lastTelemetryObject.contains("iha_boylam")) {
            displayText += QString("Konum: %1, %2\n")
                             .arg(lastTelemetryObject["iha_enlem"].toDouble(), 0, 'f', 6)
                             .arg(lastTelemetryObject["iha_boylam"].toDouble(), 0, 'f', 6);
        }
        
        // Yükseklik (AGL - Above Ground Level)
        if (lastTelemetryObject.contains("iha_irtifa")) {
            displayText += QString("Yükseklik (AGL): %1 m\n").arg(lastTelemetryObject["iha_irtifa"].toInt());
            // İrtifa label'ını güncelle
            updateAltitude(lastTelemetryObject["iha_irtifa"].toDouble());
            // MainPageWidget'daki irtifa label'ını da güncelle
            if (mainPageWidget) {
                mainPageWidget->updateAltitude(lastTelemetryObject["iha_irtifa"].toDouble());
            }
        }
        
        // Dikilme
        if (lastTelemetryObject.contains("iha_dikilme")) {
            displayText += QString("Dikilme: %1°\n").arg(lastTelemetryObject["iha_dikilme"].toInt());
        }
        
        // Yönelme
        if (lastTelemetryObject.contains("iha_yonelme")) {
            displayText += QString("Yönelme: %1°\n").arg(lastTelemetryObject["iha_yonelme"].toInt());
        }
        
        // Yatış
        if (lastTelemetryObject.contains("iha_yatis")) {
            displayText += QString("Yatış: %1°\n").arg(lastTelemetryObject["iha_yatis"].toInt());
        }
        
        // Hız
        if (lastTelemetryObject.contains("iha_hiz")) {
            displayText += QString("Hız: %1 m/s\n").arg(lastTelemetryObject["iha_hiz"].toInt());
            // Hız label'ını güncelle
            updateSpeed(lastTelemetryObject["iha_hiz"].toDouble());
            // MainPageWidget'daki hız label'ını da güncelle
            if (mainPageWidget) {
                mainPageWidget->updateSpeed(lastTelemetryObject["iha_hiz"].toDouble());
            }
        }
        
        // Batarya
        if (lastTelemetryObject.contains("iha_batarya")) {
            displayText += QString("Batarya: %1%\n").arg(lastTelemetryObject["iha_batarya"].toInt());
        }
        
        // Otonom durumu
        if (lastTelemetryObject.contains("iha_otonom")) {
            QString otonomDurum = lastTelemetryObject["iha_otonom"].toInt() ? "Aktif" : "Pasif";
            displayText += QString("Otonom: %1\n").arg(otonomDurum);
        }
        
        // Kilitlenme durumu
        if (lastTelemetryObject.contains("iha_kilitlenme")) {
            QString kilitDurum = lastTelemetryObject["iha_kilitlenme"].toInt() ? "Kilitli" : "Serbest";
            displayText += QString("Kilitlenme: %1\n").arg(kilitDurum);
        }
        
        // Hedef bilgileri
        if (lastTelemetryObject.contains("hedef_merkez_X") && lastTelemetryObject.contains("hedef_merkez_Y")) {
            displayText += QString("Hedef Merkez: (%1, %2)\n")
                             .arg(lastTelemetryObject["hedef_merkez_X"].toInt())
                             .arg(lastTelemetryObject["hedef_merkez_Y"].toInt());
        }
        
        if (lastTelemetryObject.contains("hedef_genislik") && lastTelemetryObject.contains("hedef_yukseklik")) {
            displayText += QString("Hedef Boyut: %1x%2\n")
                             .arg(lastTelemetryObject["hedef_genislik"].toInt())
                             .arg(lastTelemetryObject["hedef_yukseklik"].toInt());
        }
        
        // GPS saati
        if (lastTelemetryObject.contains("gps_saati")) {
            QJsonObject gpsSaat = lastTelemetryObject["gps_saati"].toObject();
            if (gpsSaat.contains("saat") && gpsSaat.contains("dakika") && gpsSaat.contains("saniye")) {
                displayText += QString("GPS Saati: %1:%2:%3\n")
                                 .arg(gpsSaat["saat"].toInt(), 2, 10, QChar('0'))
                                 .arg(gpsSaat["dakika"].toInt(), 2, 10, QChar('0'))
                                 .arg(gpsSaat["saniye"].toInt(), 2, 10, QChar('0'));
            }
        }
        
        telemetryDisplay->setText(displayText);
    } else {
                    telemetryDisplay->setText("Telemetri verisi bekleniyor...\n\n'Verileri Al' butonuna basın.");
    }
}



void MainWindow::fetchServerTime() {
    qDebug() << "=== DEBUG: Sunucu Saati Çekiliyor ===";
    QString serverAddress = serverLineEdit->text();
    qDebug() << "Sunucu adresi:" << serverAddress;
    
    if (serverAddress.isEmpty()) {
        qDebug() << "HATA: Sunucu adresi boş!";
        statusLabel->setText("Lütfen sunucu adresini girin!");
        return;
    }
    
    QString baseUrl = serverAddress.trimmed();
    if (!baseUrl.startsWith("http://") && !baseUrl.startsWith("https://")) {
        baseUrl = "http://" + baseUrl;
        qDebug() << "HTTP protokolü eklendi:" << baseUrl;
    }
    
    QString url = baseUrl.endsWith('/') ? (baseUrl + "api/sunucusaati") : (baseUrl + "/api/sunucusaati");
    qDebug() << "Tam URL:" << url;
    
    QUrl qurl(url);
    QNetworkRequest request(qurl);
    
    // Cookie otomatik olarak gönderiliyor
    qDebug() << "GET isteği gönderiliyor...";
    QNetworkReply* reply = networkManager->get(request);
    
    // Manuel timeout (5 saniye)
    QTimer::singleShot(5000, reply, [reply]() {
        if (reply->isRunning()) {
            qDebug() << "TIMEOUT: Sunucu saati isteği 5 saniye sonra iptal edildi";
            reply->abort();
        }
    });
    
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { 
        qDebug() << "Sunucu saati yanıtı alındı, handleServerTimeReply çağrılıyor...";
        handleServerTimeReply(reply); 
    });
}

void MainWindow::handleServerTimeReply(QNetworkReply* reply) {
    qDebug() << "=== DEBUG: Sunucu Saati Yanıtı ===";
    qDebug() << "URL:" << reply->url();
    qDebug() << "Status:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qDebug() << "Error:" << reply->error();
    qDebug() << "Error String:" << reply->errorString();
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response = reply->readAll();
        QString rawResponse = QString::fromUtf8(response);
        qDebug() << "Sunucu yanıtı (ham):" << rawResponse;
        
        QJsonDocument doc = QJsonDocument::fromJson(response);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            qDebug() << "Sunucu yanıtı (JSON):" << obj;
            
            if (obj.contains("saat") && obj.contains("dakika") && obj.contains("saniye") && obj.contains("milisaniye")) {
                int saat = obj["saat"].toInt();
                int dakika = obj["dakika"].toInt();
                int saniye = obj["saniye"].toInt();
                int milisaniye = obj["milisaniye"].toInt();
                int gun = obj.contains("gun") ? obj["gun"].toInt() : QDate::currentDate().day();
                
                qDebug() << "Sunucu saati parse edildi:";
                qDebug() << "  Saat:" << saat;
                qDebug() << "  Dakika:" << dakika;
                qDebug() << "  Saniye:" << saniye;
                qDebug() << "  Milisaniye:" << milisaniye;
                qDebug() << "  Gün:" << gun;
                
                // Sunucu saatini bir kez baz al
                QTime t(saat, dakika, saniye, milisaniye);
                QDate date = QDate::currentDate();
                if (obj.contains("gun")) {
                    // Sunucudan gelen gün bilgisini kullan
                    date = QDate(date.year(), date.month(), gun);
                    qDebug() << "Sunucudan gelen gün kullanıldı:" << date.toString("yyyy-MM-dd");
                } else {
                    qDebug() << "Yerel gün kullanıldı:" << date.toString("yyyy-MM-dd");
                }
                
                QDateTime serverDt(date, t);
                serverBaseTime = serverDt;
                serverElapsed.restart();
                serverTimeInitialized = true;
                
                qDebug() << "Server base time ayarlandı:" << serverDt.toString("yyyy-MM-dd HH:mm:ss.zzz");
                qDebug() << "Server time initialized:" << serverTimeInitialized;
                
                serverTimeLabel->setText(serverDt.toString("HH:mm:ss.zzz"));
                dateLabel->setText(serverDt.toString("yyyy-MM-dd"));
                // GNSS zamanı ile sunucu zamanı arasındaki ofseti hesapla ve +1000 ms uygula
                updateServerGpsOffset();

                // /server_time yayını: Sunucu saati alındıktan sonra başlat
                if (!serverTimePubTimer) {
                    serverTimePubTimer = new QTimer(this);
                    serverTimePubTimer->setInterval(100); // 10 Hz
                    connect(serverTimePubTimer, &QTimer::timeout, this, [this]() { publishServerTimeOnce(); });
                }
                // İlk publish ve timer start
                publishServerTimeOnce();
                serverTimePubTimer->start();
                
                qDebug() << "UI güncellendi - Saat:" << serverDt.toString("HH:mm:ss.zzz") << "Tarih:" << serverDt.toString("yyyy-MM-dd");
            } else {
                qDebug() << "HATA: Gerekli saat alanları bulunamadı!";
                qDebug() << "Mevcut alanlar:" << obj.keys();
            }
        } else {
            qDebug() << "HATA: JSON parse edilemedi!";
        }
    } else {
        qDebug() << "HATA: Sunucu saati çekilemedi!";
        qDebug() << "Hata kodu:" << reply->error();
        qDebug() << "Hata mesajı:" << reply->errorString();
    }
    reply->deleteLater();
}

void MainWindow::updateCompetitionTime() {
    if (competitionSecondsLeft > 0) {
        competitionSecondsLeft--;
    }
    int min = competitionSecondsLeft / 60;
    int sec = competitionSecondsLeft % 60;
    QString timeStr = QString("%1:%2").arg(min, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0'));
    competitionTimeLabel->setText(timeStr);
    if (competitionSecondsLeft == 0) {
        competitionTimer->stop();
        isCompetitionRunning = false;
        competitionTimeLabel->setStyleSheet("background: #c0392b; border-radius: 10px; color: #fff; font-size: 18px; font-weight: bold; margin-left: 8px; padding: 6px 9px;");
        competitionTimeLabel->setText("00:00");
        QMessageBox::information(this, "Yarışma Süresi", "Yarışma süresi bitti!");
    }
}

// Event filter ile label'a tıklama algılama
bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
    if (obj == competitionTimeLabel && event->type() == QEvent::MouseButtonPress) {
        onCompetitionTimeLabelClicked();
        return true;
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::onCompetitionTimeLabelClicked() {
    bool ok = false;
    int min = QInputDialog::getInt(this, "Yarışma Süresi Ayarla", "Dakika (1-15):", competitionSecondsLeft / 60, 1, 15, 1, &ok);
    if (ok) {
        competitionSecondsLeft = min * 60;
        competitionTimeLabel->setStyleSheet("background: #232b3a; border-radius: 10px; color: #fff; font-size: 18px; font-weight: bold; margin-left: 8px; padding: 6px 9px;");
        updateCompetitionTime();
        if (competitionTimer) competitionTimer->start();
    }
}

void MainWindow::onSendButtonClicked() {
    // Giriş kontrolü - cookie otomatik olarak gönderiliyor
    
    // TelemetryBridge kullanarak telemetri gönder
    if (telemetryBridge) {
        // Sunucu bilgilerini al
        QString serverAddress = serverLineEdit->text();
        if (serverAddress.isEmpty()) {
            statusLabel->setText("Lütfen sunucu adresini girin!");
            return;
        }
        
        // Telemetri gönderim modunu aktif et
        isTelemetrySending = true;
        
        // Cookie ile TelemetryBridge'ı başlat
        telemetryBridge->startWithCookie(serverAddress.toStdString(), "/mavros");

        // Rakip Analizi'ni aktif et: MainPageWidget analiz işçisini başlat
        if (mainPageWidget) {
            qDebug() << "[MainWindow] Telemetri basıldı -> Rakip Analizi start";
            
            // Sunucu adresini normalize et
            QString normalizedServerAddress = serverAddress;
            if (!normalizedServerAddress.startsWith("http://") && !normalizedServerAddress.startsWith("https://")) {
                normalizedServerAddress = "http://" + normalizedServerAddress;
            }
            
            // serverUrl zaten set edildiyse analiz içi timer başlar
            mainPageWidget->setServerUrl(normalizedServerAddress);
            
            // Cookie jar'ını MainPageWidget'a tekrar aktar (kritik!)
            QNetworkCookieJar* currentJar = networkManager->cookieJar();
            if (currentJar) {
                mainPageWidget->setCookieJar(currentJar);
                qDebug() << "[MainWindow] Cookie jar MainPageWidget'a tekrar aktarıldı";
            }
            
            // HSS ve QR koordinatları otomatik çekilmiyor - sadece butonlarla çekilecek
            qDebug() << "[MainWindow] Telemetri basıldı -> HSS ve QR koordinatları manuel butonlarla çekilecek";
        }
        
        // Durum label'ını güncelle
        if (telemetryStatusLabel) {
            telemetryStatusLabel->setVisible(true);
            telemetryStatusLabel->setTextFormat(Qt::RichText);
            telemetryStatusLabel->setText("Telemetri : <span style='color:#27ae60'>Bağlı</span>");
            telemetryStatusLabel->setStyleSheet("font-weight: bold; font-size: 12pt; margin-top: 2px; background-color: #232b3a; color: white; border: none;");
        }
        
        if (telemetryDisplay) {
            telemetryDisplay->setText("=== TELEMETRİ GÖNDERİMİ BAŞLATILDI ===\n\nHer saniye sunucuya veri gönderiliyor...\nSunucu yanıtları burada görünecek.");
        }
    } else {
        if (telemetryDisplay) {
            telemetryDisplay->setText("TelemetryBridge mevcut değil!");
        }
    }
}

void MainWindow::sendTelemetryToServer() {
    // Giriş kontrolü - cookie otomatik olarak gönderiliyor

    // Telemetri verisi kontrolü
    if (lastTelemetryObject.isEmpty()) {
        return;
    }
    
    // Overlap koruması: Eğer önceki istek hala devam ediyorsa, bu isteği atla
    static bool isRequestInProgress = false;
    if (isRequestInProgress) {
        qDebug() << "Önceki telemetri isteği devam ediyor, bu istek atlandı";
        return;
    }
    
    isRequestInProgress = true;

    // Sunucu adresi
    QString serverAddress = serverLineEdit->text();
    if (serverAddress.isEmpty()) {
        statusLabel->setText("Lütfen sunucu adresini girin!");
        return;
    }
    
    // Sunucu adresini normalize et
    QString baseUrl = serverAddress.trimmed();
    if (!baseUrl.startsWith("http://") && !baseUrl.startsWith("http://")) {
        baseUrl = "http://" + baseUrl;
    }
    
    QString url = baseUrl.endsWith('/') ? (baseUrl + "api/telemetri_gonder") : (baseUrl + "/api/telemetri_gonder");
    
    // Gönderilecek telemetriye girişten dönen takım numarasını enjekte et
    // (Sunucudan gelen takım no her zaman önceliklidir)
    lastTelemetryObject["takim_numarasi"] = teamNumber;
    
    // Yeni HTTP fonksiyonunu kullan - cookie otomatik olarak gönderiliyor
    sendHttpRequest(url, lastTelemetryObject, "POST");
    
    // HTTP yanıtlarını dinle
    connect(this, &MainWindow::httpSuccess, this, [this](const QJsonObject &response) {
        isRequestInProgress = false; // İstek tamamlandı
        handleTelemetrySendSuccess(response);
    });
    
    connect(this, &MainWindow::httpError, this, [this](const QString &error) {
        isRequestInProgress = false; // İstek tamamlandı
        handleTelemetrySendError(error);
    });
    
    connect(this, &MainWindow::httpRawResponse, this, [this](const QString &rawResponse) {
        handleTelemetryRawResponse(rawResponse);
    });
}

void MainWindow::onStopButtonClicked() {
    // Telemetri gönderim modunu durdur
    isTelemetrySending = false;
    // Canlı görüntülemeyi de kapat
    isTelemetryViewing = false;
    
    if (telemetryBridge) {
        // Timer'ı durdur
        telemetryBridge->stopTimer();
        
        // Durum label'ını güncelle
        if (telemetryStatusLabel) {
            telemetryStatusLabel->setVisible(true);
            telemetryStatusLabel->setTextFormat(Qt::RichText);
            telemetryStatusLabel->setText("Telemetri : <span style='color:#c0392b'>Bağlı Değil</span>");
            telemetryStatusLabel->setStyleSheet("font-weight: bold; font-size: 12pt; margin-top: 2px; background-color: #232b3a; color: white; border: none;");
        }
        
        if (telemetryDisplay) {
            telemetryDisplay->setText("Telemetri durduruldu.");
        }
    } else {
        if (telemetryDisplay) {
            telemetryDisplay->setText("TelemetryBridge mevcut değil!");
        }
    }
}



void MainWindow::onFetchButtonClicked() {
    // TelemetryBridge üzerinden canlı izlemeyi başlat
    if (!telemetryBridge) {
        if (telemetryDisplay) telemetryDisplay->setText("TelemetryBridge mevcut değil!");
        return;
    }

    // İlk kez ise ROS subscriber'ları başlat
    if (!rosSubscribersStarted) {
        telemetryBridge->startRosSubscribers("/mavros");
        rosSubscribersStarted = true;
    }

    isTelemetryViewing = true;
    
    // GPS durumunu başlangıçta "Bağlı Değil" olarak ayarla
    if (gpsStatusLabel) {
        gpsStatusLabel->setText("GPS : <span style='color:#c0392b'>Bağlı Değil</span>");
    }

    // Durum label'ını güncelle
    if (telemetryStatusLabel) {
        telemetryStatusLabel->setVisible(true);
        telemetryStatusLabel->setTextFormat(Qt::RichText);
        telemetryStatusLabel->setText("Telemetri : <span style='color:#27ae60'>Bağlı</span>");
        telemetryStatusLabel->setStyleSheet("font-weight: bold; font-size: 12pt; margin-top: 2px; background-color: #232b3a; color: white; border: none;");
    }

    if (telemetryDisplay) {
        telemetryDisplay->setText("Telemetri izleme başlatıldı. Güncellemeler burada görünecek.");
    }
}

 



// HTTP istekleri için yardımcı fonksiyonlar
void MainWindow::sendHttpRequest(const QString &url, const QJsonObject &data, const QString &method) {
    qDebug() << "=== DEBUG: sendHttpRequest ===";
    qDebug() << "URL:" << url;
    qDebug() << "Method:" << method;
    qDebug() << "Data:" << data;
    qDebug() << "Cookie kullanılıyor";
    
    // NetworkManager kontrolü
    if (!networkManager) {
        qDebug() << "ERROR: networkManager is null!";
        return;
    }
    qDebug() << "NetworkManager is valid";
    
    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    // Cookie otomatik olarak gönderiliyor
    
    QNetworkReply* reply = nullptr;
    
    if (method.toUpper() == "POST") {
        QJsonDocument doc(data);
        QByteArray jsonData = doc.toJson();
        qDebug() << "Sending POST request with JSON data:" << jsonData;
        reply = networkManager->post(request, jsonData);
    } else if (method.toUpper() == "GET") {
        qDebug() << "Sending GET request";
        reply = networkManager->get(request);
    }
    
    if (reply) {
        qDebug() << "Network reply created successfully";
        
        // Manuel timeout (5 saniye)
        QTimer::singleShot(5000, reply, [reply]() {
            if (reply->isRunning()) {
                reply->abort();
                qDebug() << "Manuel timeout: İstek iptal edildi";
            }
        });
        
        // Tüm sinyalleri bağla
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            qDebug() << "Network reply finished signal received";
            handleHttpResponse(reply);
        });
        
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        connect(reply, &QNetworkReply::errorOccurred, this, [this, reply](QNetworkReply::NetworkError error) {
#else
        connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::error), this, [this, reply](QNetworkReply::NetworkError error) {
#endif
            qDebug() << "Network error occurred:" << error << "-" << reply->errorString();
        });
        
        connect(reply, &QNetworkReply::uploadProgress, this, [this](qint64 bytesSent, qint64 bytesTotal) {
            qDebug() << "Upload progress:" << bytesSent << "/" << bytesTotal;
        });
        
        connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 bytesReceived, qint64 bytesTotal) {
            qDebug() << "Download progress:" << bytesReceived << "/" << bytesTotal;
        });
        
    } else {
        qDebug() << "ERROR: Network reply is null!";
    }
}

void MainWindow::handleHttpResponse(QNetworkReply* reply) {
    qDebug() << "=== DEBUG: HTTP Response ===";
    qDebug() << "URL:" << reply->url();
    qDebug() << "Status:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    
    // Sadece gerçek hata durumunda error logları göster
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Error:" << reply->error();
        qDebug() << "Error String:" << reply->errorString();
    }
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response = reply->readAll();
        QString rawResponse = QString::fromUtf8(response);
        
        // Ham yanıtı debug'da göster
        qDebug() << "Sunucu yanıtı:" << rawResponse;
        // Kilitlenme POST'u için özel debug
        if (reply->url().toString().contains("/api/kilitlenme_bilgisi")) {
            qDebug() << "[DEBUG] Kilitlenme POST yanıtı:" << rawResponse;
        }
        
        // Ham yanıtı da emit et
        emit httpRawResponse(rawResponse);
        
        // Önce basit sayısal yanıtları kontrol et (örn: 15)
        bool isNumber = false;
        int numericResponse = rawResponse.trimmed().toInt(&isNumber);
        if (isNumber && numericResponse > 0) {
            qDebug() << "Sayısal yanıt (takım numarası) alındı:" << numericResponse;
            // tungaY ile aynı yaklaşım: sayıyı takım numarası olarak kabul et
            this->teamNumber = numericResponse;
            if (telemetryBridge) {
                telemetryBridge->setTeamNumber(teamNumber);
            }
            QJsonObject successObj;
            successObj["team_number"] = numericResponse;
            successObj["success"] = true;
            emit httpSuccess(successObj);
            reply->deleteLater();
            return;
        }
        
        // JSON yanıtı işle
        QJsonDocument doc = QJsonDocument::fromJson(response);
        if (doc.isNull()) {
            qDebug() << "JSON parse hatası! Ham yanıt:" << rawResponse;
            // JSON parse hatası olsa bile başarılı olabilir
            if (rawResponse.trimmed().toLower().contains("success") || 
                rawResponse.trimmed().toLower().contains("başar") ||
                rawResponse.trimmed().toLower().contains("ok")) {
                QJsonObject successObj;
                successObj["message"] = "Giriş başarılı: " + rawResponse;
                emit httpSuccess(successObj);
            } else {
                emit httpError("JSON parse hatası: " + rawResponse);
            }
            reply->deleteLater();
            return;
        }
        
        QJsonObject obj = doc.object();
        
        // Yanıt tipine göre işle
        if (obj.contains("success") && obj["success"].toBool()) {
            qDebug() << "Success response detected";
            emit httpSuccess(obj);
        } else if (obj.contains("status") && obj["status"].toString() == "success") {
            qDebug() << "Status success detected";
            emit httpSuccess(obj);
        } else if (obj.contains("ok") && obj["ok"].toBool()) {
            qDebug() << "OK response detected";
            emit httpSuccess(obj);
        } else if (obj.contains("success")) {
            qDebug() << "Success response detected";
            emit httpSuccess(obj);
        } else if (obj.contains("message") && obj.value("message").toString().contains("başar", Qt::CaseInsensitive)) {
            qDebug() << "Success message detected";
            emit httpSuccess(obj);
        } else if (obj.contains("message") && obj.value("message").toString().contains("alındı", Qt::CaseInsensitive)) {
            qDebug() << "Received message detected";
            emit httpSuccess(obj);
        } else {
            QString errorMsg = "Sunucu hatası: " + (obj.contains("message") ? obj["message"].toString() : "Bilinmeyen hata");
            qDebug() << "Server error:" << errorMsg;
            emit httpError(errorMsg);
        }
    } else {
        QString errorMsg = "Ağ hatası: " + reply->errorString();
        qDebug() << "HTTP ERROR:" << errorMsg;
        // Kilitlenme POST'u için özel debug (hata)
        if (reply->url().toString().contains("/api/kilitlenme_bilgisi")) {
            qDebug() << "[DEBUG] Kilitlenme POST hatası:" << reply->error() << reply->errorString();
        }
        
        // Retry yapılabilir hatalar için retry mekanizması
        if (reply->error() == QNetworkReply::ConnectionRefusedError ||
            reply->error() == QNetworkReply::RemoteHostClosedError ||
            reply->error() == QNetworkReply::HostNotFoundError ||
            reply->error() == QNetworkReply::TimeoutError) {
            
            // URL'den hangi endpoint olduğunu belirle
            QString url = reply->url().toString();
            if (url.contains("/api/telemetri_gonder")) {
                // Telemetri gönderimi hatası - retry yap
                qDebug() << "Telemetri gönderimi hatası, retry yapılıyor...";
                // Burada retry yapılabilir ama şimdilik sadece log
            }
        }
        
        emit httpError(errorMsg);
    }
    
    reply->deleteLater();
}

void MainWindow::retryRequest(const QString &url, const QJsonObject &data, const QString &method, int retryCount) {
    const int maxRetries = 3;
    const int baseDelay = 1000; // 1 saniye
    
    if (retryCount >= maxRetries) {
        qDebug() << "Maksimum retry sayısına ulaşıldı, istek iptal edildi";
        emit httpError("Maksimum retry sayısına ulaşıldı");
        return;
    }
    
    // Exponential backoff: 1s, 2s, 4s
    int delay = baseDelay * (1 << retryCount);
    qDebug() << QString("İstek %1 saniye sonra tekrar deneniyor (retry %2/%3)").arg(delay/1000.0).arg(retryCount + 1).arg(maxRetries);
    
    QTimer::singleShot(delay, this, [this, url, data, method, retryCount]() {
        sendHttpRequest(url, data, method);
    });
}

void MainWindow::handleLoginSuccess(const QJsonObject &response) {
    // Token kaldırıldı, cookie kullanılıyor
    
    // Takım numarasını yanıt içinden al (birden fazla ihtimali destekle)
    int parsedTeam = 0;
    if (response.contains("team_number")) {
        parsedTeam = response.value("team_number").toInt();
    } else if (response.contains("takim_numarasi")) {
        parsedTeam = response.value("takim_numarasi").toInt();
    } else if (response.contains("response_code")) {
        // Bazı durumlarda sayısal kod takım no olabilir
        parsedTeam = response.value("response_code").toInt();
    }
    if (parsedTeam > 0) {
        teamNumber = parsedTeam;
        if (telemetryBridge) {
            telemetryBridge->setTeamNumber(teamNumber);
        }
        statusLabel->setText(QString("Giriş Başarılı - Takım: %1").arg(teamNumber));
    } else {
        statusLabel->setText("Giriş Başarılı");
    }
    
    // ROS listener'ı başlat
    if (rosListener) {
        try {
            QString serverAddress = serverLineEdit->text();
            if (serverAddress.isEmpty()) {
                statusLabel->setText("Lütfen sunucu adresini girin!");
                return;
            }
            // Sunucu adresini normalize et
            QString baseUrl = serverAddress.trimmed();
            if (!baseUrl.startsWith("http://") && !baseUrl.startsWith("https://")) {
                baseUrl = "http://" + baseUrl;
            }
            rosListener->setServerUrl(baseUrl);
            // Cookie otomatik olarak gönderiliyor
            rosListener->startListening();
        } catch (const std::exception& e) {
            // ROS listener başlatma hatası
        } catch (...) {
            // Bilinmeyen ROS listener hatası
        }
    }
    
    // Otomatik telemetri gönderimi kaldırıldı; yalnızca 'Telemetri Bas' ile başlatılacak
    
    // Durum label'larını güncelle
    updateStatusLabels(true);
    
    // Power label'ını hemen 0% olarak ayarla (batarya verisi gelene kadar)
    if (powerStatusLabel) {
        powerStatusLabel->setText("Power : <span style='color:#c0392b'>0%</span>");
    }
    
    // Sunucu bağlantısı durumunu özel olarak güncelle
    if (serverStatusLabel) {
        serverStatusLabel->setText("Sunucu : <span style='color:#27ae60'>Bağlı</span>");
    }
    
    // Giriş başarılı: Cookie set edilmiş durumda. Sunucu saatini bir kez çek ve baz al.
    qDebug() << "=== DEBUG: Login Başarılı - Sunucu Saati Çekiliyor ===";
    qDebug() << "Cookie set edildi, serverTimeInitialized:" << serverTimeInitialized;
    
    if (!serverTimeInitialized) {
        qDebug() << "Sunucu saati henüz alınmamış, fetchServerTime() çağrılıyor...";
        fetchServerTime();
    } else {
        qDebug() << "Sunucu saati zaten alınmış, tekrar çekilmiyor.";
    }
}

void MainWindow::handleLoginError(const QString &error) {
    statusLabel->setText("Giriş Hatası: " + error);
    
    // Durum label'larını güncelle
    updateStatusLabels(false);
    
    // Power label'ını 0% olarak ayarla
    if (powerStatusLabel) {
        powerStatusLabel->setText("Power : <span style='color:#c0392b'>0%</span>");
    }
    
    // Sunucu bağlantısı durumunu özel olarak güncelle
    if (serverStatusLabel) {
        serverStatusLabel->setText("Sunucu : <span style='color:#c0392b'>Bağlı Değil</span>");
    }
}

void MainWindow::updateStatusLabels(bool isConnected) {
    QString color = isConnected ? "#27ae60" : "#c0392b";
    QString status = isConnected ? "Bağlı" : "Bağlı Değil";
    
    if (powerStatusLabel) {
        // TelemetryBridge'den gerçek batarya yüzdesini al
        double batteryPercentage = 0.0;
        if (telemetryBridge) {
            batteryPercentage = telemetryBridge->currentData().battery;
        }
        
        if (batteryPercentage > 0) {
            // Battery artık direkt percentage olarak geliyor
            int batteryPerc = static_cast<int>(std::round(batteryPercentage));
            
            // Renk kodunu batarya seviyesine göre ayarla
            QString colorCode;
            if (batteryPerc > 50) {
                colorCode = "#27ae60"; // Yeşil
            } else if (batteryPerc > 20) {
                colorCode = "#f39c12"; // Turuncu
            } else {
                colorCode = "#e74c3c"; // Kırmızı
            }
            
            powerStatusLabel->setText(QString("Power : <span style='color:%1'>%2%</span>")
                                        .arg(colorCode).arg(batteryPerc));
        } else {
            // Batarya verisi yoksa 0% göster
            powerStatusLabel->setText(QString("Power : <span style='color:#c0392b'>0%</span>"));
        }
    }
    
    if (planeStatusLabel) {
        planeStatusLabel->setText(QString("Plane : <span style='color:%1'>%2</span>")
                                    .arg(color)
                                    .arg(status));
    }
    
    // GPS durumunu giriş yapıldığında otomatik olarak değiştirme
    // GPS durumu sadece gerçek GPS verisi geldiğinde güncellenecek
    // if (gpsStatusLabel) {
    //     gpsStatusLabel->setText(QString("GPS : <span style='color:%1'>%2</span>")
    //                               .arg(color)
    //                               .arg(status));
    // }
    
    if (serverStatusLabel) {
        serverStatusLabel->setText(QString("Sunucu : <span style='color:%1'>%2</span>")
                                     .arg(color)
                                     .arg(status));
    }
    
    if (telemetryStatusLabel) {
        QString telemetryColor = isTelemetrySending ? "#27ae60" : "#c0392b";
        QString telemetryStatus = isTelemetrySending ? "Bağlı" : "Bağlı Değil";
        telemetryStatusLabel->setText(QString("Telemetri : <span style='color:%1'>%2</span>")
                                        .arg(telemetryColor)
                                        .arg(telemetryStatus));
    }
}

void MainWindow::handleTelemetrySendSuccess(const QJsonObject &response) {
    qDebug() << "=== handleTelemetrySendSuccess çağrıldı ===";
    qDebug() << "Response boş mu:" << response.isEmpty();
    qDebug() << "telemetryDisplay null mu:" << (telemetryDisplay == nullptr);
    
    // Başarılı gönderim durumunda sunucu durumunu güncelle
    if (serverStatusLabel) {
        serverStatusLabel->setVisible(true);
        serverStatusLabel->setTextFormat(Qt::RichText);
        serverStatusLabel->setText("Sunucu : <span style='color:#27ae60'>Bağlı</span>");
        serverStatusLabel->setStyleSheet("font-weight: bold; font-size: 12pt; margin-top: 2px;");
    }
    
    // Ham yanıt zaten handleTelemetryRawResponse'da gösterildiği için burada sadece durum güncellemesi yapıyoruz
    qDebug() << "Telemetri gönderimi başarılı - sunucu durumu güncellendi";
}

void MainWindow::handleTelemetryRawResponse(const QString &rawResponse) {
    qDebug() << "=== handleTelemetryRawResponse çağrıldı ===";
    qDebug() << "Raw response:" << rawResponse;
    
    // Ham yanıtı telemetri panelinde göster
    if (telemetryDisplay) {
        // JSON'u düzenli formatta göstermeye çalış
        QJsonDocument doc = QJsonDocument::fromJson(rawResponse.toUtf8());
        QString displayText;
        
        if (!doc.isNull()) {
            // JSON düzenli formatta göster
            displayText = doc.toJson(QJsonDocument::Indented);
        } else {
            // Ham string olarak göster
            displayText = rawResponse;
        }
        
        telemetryDisplay->setText(displayText);
        qDebug() << "Telemetri paneli güncellendi - yanıt gösterildi";
        
        // Telemetri verilerinden irtifa ve hız bilgilerini çek ve label'ları güncelle
        if (!doc.isNull()) {
            QJsonObject telemetryData = doc.object();
            if (telemetryData.contains("iha_irtifa")) {
                updateAltitude(telemetryData["iha_irtifa"].toDouble());
                // MainPageWidget'daki irtifa label'ını da güncelle
                if (mainPageWidget) {
                    mainPageWidget->updateAltitude(telemetryData["iha_irtifa"].toDouble());
                }
            }
            if (telemetryData.contains("iha_hiz")) {
                updateSpeed(telemetryData["iha_hiz"].toDouble());
                // MainPageWidget'daki hız label'ını da güncelle
                if (mainPageWidget) {
                    mainPageWidget->updateSpeed(telemetryData["iha_hiz"].toDouble());
                }
            }
        }
    } else {
        qDebug() << "Telemetri paneli null - yanıt gösterilemedi";
    }
}

void MainWindow::onTelemetryBridgeResponse(QString response) {
    // 1) Panelde göster
    if (telemetryDisplay) {
        QString displayText;
        QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
        if (!doc.isNull()) displayText = doc.toJson(QJsonDocument::Indented);
        else displayText = response;
        telemetryDisplay->setText(displayText);
        
        // Telemetri verilerinden irtifa ve hız bilgilerini çek ve label'ları güncelle
        if (!doc.isNull()) {
            QJsonObject telemetryData = doc.object();
            if (telemetryData.contains("iha_irtifa")) {
                updateAltitude(telemetryData["iha_irtifa"].toDouble());
                // MainPageWidget'daki irtifa label'ını da güncelle
                if (mainPageWidget) {
                    mainPageWidget->updateAltitude(telemetryData["iha_irtifa"].toDouble());
                }
            }
            if (telemetryData.contains("iha_hiz")) {
                updateSpeed(telemetryData["iha_hiz"].toDouble());
                // MainPageWidget'daki hız label'ını da güncelle
                if (mainPageWidget) {
                    mainPageWidget->updateSpeed(telemetryData["iha_hiz"].toDouble());
                }
            }
        }
    }
    // 2) Rakipleri işle (her zaman). Merkezleme/durdurma sadece istek sonrası yapılır
    if (mainPageWidget) {
        mainPageWidget->onRivalsFromTelemetryJson(response);
    }
    
    // 3) ROS topiğine rakip İHA verilerini publish et
    rivalRos_.publishRawJson(response);   // Ham JSON → /rivals/json
    rivalRos_.publishParsed(response);    // PoseArray+IDs → /rivals/poses, /rivals/ids
    // 3) Sadece butonla istendiyse bir defalık merkezle/durdur
    if (pendingRivalsFetch && mainPageWidget) {
        qDebug() << "[DEBUG] Rakip aktarma başlatılıyor -> MainPageWidget::onRivalsFromTelemetryJson çağrılacak";
        pendingRivalsFetch = false;
        // İsteğe bağlı: otomatik başlatıldıysa durdur
        if (autoStopTelemetryAfterRivals && telemetryBridge) {
            telemetryBridge->stopTimer();
            isTelemetrySending = false;
            isTelemetryViewing = false;
            updateStatusLabels(true);
            autoStopTelemetryAfterRivals = false;
        }
    }
}

void MainWindow::onRivalsFetchRequested() {
    // Sunucu adresi
    QString serverAddress = serverLineEdit ? serverLineEdit->text().trimmed() : QString();
    qDebug() << "[DEBUG] onRivalsFetchRequested: server=\"" << serverAddress << "\"";
    if (serverAddress.isEmpty()) {
        if (telemetryDisplay) {
            telemetryDisplay->append("Rakip İHA çek: Sunucu adresi boş");
        }
        return;
    }
    pendingRivalsFetch = true;
    qDebug() << "[DEBUG] pendingRivalsFetch=true olarak ayarlandı";
    // Eğer gönderim/izleme aktif değilse, bir defaya mahsus TelemetryBridge başlat ve sonra durdur
    if (!isTelemetrySending && !isTelemetryViewing && telemetryBridge) {
        if (telemetryDisplay) {
            telemetryDisplay->append("Rakip İHA çek: Tek seferlik telemetri gönderimi başlatılıyor...");
        }
        qDebug() << "[DEBUG] telemetryBridge->startWithCookie() çağrılıyor";
        telemetryBridge->startWithCookie(serverAddress.toStdString(), "/mavros");
        autoStopTelemetryAfterRivals = true;
    } else {
        if (telemetryDisplay) {
            telemetryDisplay->append("Rakip İHA çek: Mevcut oturumda bir sonraki yanıt işlenecek");
        }
        qDebug() << "[DEBUG] Mevcut oturum açık; bir sonraki yanıt işlenecek";
    }
}

void MainWindow::handleTelemetrySendError(const QString &error) {
    // Hata durumunda sunucu durumunu güncelle
    if (serverStatusLabel) {
        serverStatusLabel->setVisible(true);
        serverStatusLabel->setTextFormat(Qt::RichText);
        serverStatusLabel->setText("Sunucu : <span style='color:#c0392b'>Bağlantı Hatası</span>");
        serverStatusLabel->setStyleSheet("font-weight: bold; font-size: 12pt; margin-top: 2px;");
    }
    
    // Hata mesajını telemetri panelinde göster
    if (telemetryDisplay) {
        QString displayText = telemetryDisplay->toPlainText();
        displayText += QString("\n\nTelemetri Gönderme Hatası:\n%1").arg(error);
        telemetryDisplay->setText(displayText);
    }
}

void MainWindow::onLockInfoReceived(const std_msgs::String::ConstPtr& msg) {
    QString lockInfo = QString::fromStdString(msg->data);
    qDebug() << "Kilitlenme bilgisi alındı:" << lockInfo;

    // UI güncelleme ve HTTP POST işlemlerini GUI thread'ine sıraya al (thread-safety)
    QTimer::singleShot(0, this, [this, lockInfo]() {
        // Kilitlenme panelinde göster
        if (lockInfoLabel) {
            lockInfoLabel->setText("Kilitlenme Bilgisi:\n" + lockInfo);
        }

        // Alır almaz sunucuya 1 defa POST gönder
        if (serverLineEdit && networkManager) {
            QString serverAddress = serverLineEdit->text().trimmed();
            if (!serverAddress.isEmpty()) {
                // Sunucu adresini normalize et (http/https ekle)
                QString baseUrl = serverAddress;
                if (!baseUrl.startsWith("http://") && !baseUrl.startsWith("https://")) {
                    baseUrl = "http://" + baseUrl;
                }
                QString url = baseUrl.endsWith('/') ? (baseUrl + "api/kilitlenme_bilgisi") : (baseUrl + "/api/kilitlenme_bilgisi");

                // Gönderilecek yük: lockInfo JSON ise obje olarak, değilse alan içinde gönder
                QJsonObject payload;
                QJsonParseError parseError;
                QJsonDocument doc = QJsonDocument::fromJson(lockInfo.toUtf8(), &parseError);
                if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
                    payload = doc.object();
                } else {
                    payload.insert("kilitlenme_bilgisi", lockInfo);
                }

                // Mevcut yardımcıyı kullanarak POST yap
                sendHttpRequest(url, payload, "POST");
            } else {
                qDebug() << "Kilitlenme POST atlanıyor: sunucu adresi boş";
            }
        }
    });
}

void MainWindow::onKamikazeInfoReceived(const std_msgs::String::ConstPtr& msg) {
    QString qrMetni = QString::fromStdString(msg->data);
    qDebug() << "QR message alındı:" << qrMetni;

    // UI güncelleme ve HTTP POST işlemlerini GUI thread'ine sıraya al (thread-safety)
    QTimer::singleShot(0, this, [this, qrMetni]() {
        // Kamikaze panelinde göster
        if (kamikazeInfoLabel) {
            kamikazeInfoLabel->setText("QR Message:\n" + qrMetni);
        }

        // ServerTime'dan kamikaze başlangıç zamanını al
        QDateTime currentServerTime = QDateTime::currentDateTime();
        QJsonObject kamikazeBaslangicZamani;
        kamikazeBaslangicZamani["saat"] = currentServerTime.time().hour();
        kamikazeBaslangicZamani["dakika"] = currentServerTime.time().minute();
        kamikazeBaslangicZamani["saniye"] = currentServerTime.time().second();
        kamikazeBaslangicZamani["milisaniye"] = currentServerTime.time().msec();

        // Kamikaze bitiş zamanı da aynı (şimdilik)
        QJsonObject kamikazeBitisZamani = kamikazeBaslangicZamani;

        // Tam kamikaze formatını oluştur
        QJsonObject kamikazePayload;
        kamikazePayload["kamikazeBaslangicZamani"] = kamikazeBaslangicZamani;
        kamikazePayload["kamikazeBitisZamani"] = kamikazeBitisZamani;
        kamikazePayload["qrMetni"] = qrMetni;

        // Alır almaz sunucuya 1 defa POST gönder
        if (serverLineEdit && networkManager) {
            QString serverAddress = serverLineEdit->text().trimmed();
            if (!serverAddress.isEmpty()) {
                // Sunucu adresini normalize et (http/https ekle)
                QString baseUrl = serverAddress;
                if (!baseUrl.startsWith("http://") && !baseUrl.startsWith("https://")) {
                    baseUrl = "http://" + baseUrl;
                }
                QString url = baseUrl.endsWith('/') ? (baseUrl + "api/kamikaze_bilgisi") : (baseUrl + "/api/kamikaze_bilgisi");

                // Mevcut yardımcıyı kullanarak POST yap
                sendHttpRequest(url, kamikazePayload, "POST");
            } else {
                qDebug() << "Kamikaze POST atlanıyor: sunucu adresi boş";
            }
        }
    });
}

void MainWindow::onQrMessageReceived(const std_msgs::String::ConstPtr& msg) {
    QString qrMessage = QString::fromStdString(msg->data);
    qDebug() << "QR message alındı:" << qrMessage;

    // UI güncelleme ve HTTP POST işlemlerini GUI thread'ine sıraya al (thread-safety)
    QTimer::singleShot(0, this, [this, qrMessage]() {
        // QR Message'ı kamikaze panelinde göster
        if (kamikazeInfoLabel) {
            QString currentText = kamikazeInfoLabel->text();
            QString newText = "Kamikaze Bilgisi:\n" + qrMessage;
            if (currentText != "Kamikaze Bilgisi:\n--") {
                newText = currentText + "\n\nQR Message:\n" + qrMessage;
            }
            kamikazeInfoLabel->setText(newText);
        }

        // Alır almaz sunucuya 1 defa POST gönder
        if (serverLineEdit && networkManager) {
            QString serverAddress = serverLineEdit->text().trimmed();
            if (!serverAddress.isEmpty()) {
                // Sunucu adresini normalize et (http/https ekle)
                QString baseUrl = serverAddress;
                if (!baseUrl.startsWith("http://") && !baseUrl.startsWith("https://")) {
                    baseUrl = "http://" + baseUrl;
                }
                QString url = baseUrl.endsWith('/') ? (baseUrl + "api/qr_data") : (baseUrl + "/api/qr_data");

                // Gönderilecek yük: qrMessage JSON ise obje olarak, değilse alan içinde gönder
                QJsonObject payload;
                QJsonParseError parseError;
                QJsonDocument doc = QJsonDocument::fromJson(qrMessage.toUtf8(), &parseError);
                if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
                    payload = doc.object();
                } else {
                    payload.insert("qr_data", qrMessage);
                }

                // Mevcut yardımcıyı kullanarak POST yap
                sendHttpRequest(url, payload, "POST");
            } else {
                qDebug() << "QR Message POST atlanıyor: sunucu adresi boş";
            }
        }
    });
}

void MainWindow::updateSessionInfo(const QString& sessionValue) {
    if (sessionInfoLabel && !sessionValue.isEmpty()) {
        QString formattedSession = sessionValue;
        // Sadece ilk 8 karakteri göster
        if (formattedSession.length() > 8) {
            formattedSession = formattedSession.left(8) + "...";
        }
        sessionInfoLabel->setText(QString("<b>Session:</b> <span style='color:#000; font-family: monospace; font-weight: bold;'>%1</span>").arg(formattedSession));
    }
}

// ROS /mavros/time_reference callback: GNSS zamanını sakla
void MainWindow::onTimeReference(const sensor_msgs::TimeReference::ConstPtr& msg) {
    const ros::Time& t = msg->time_ref;
    if (t.isZero()) {
        hasGpsTimeRef = false;
        return;
    }
    qint64 msec = static_cast<qint64>(t.sec) * 1000 + static_cast<qint64>(t.nsec) / 1000000;
    lastGpsTimeRef = QDateTime::fromMSecsSinceEpoch(msec, Qt::UTC);
    hasGpsTimeRef = true;
    // Sunucu zamanı mevcutsa ofseti güncelle
    if (serverTimeInitialized) {
        updateServerGpsOffset();
    }
}

MainWindow::~MainWindow()
{
    // ROS subscriber'ları temizle
    if (lockInfoSubscriber) {
        lockInfoSubscriber.shutdown();
    }
    if (kamikazeInfoSubscriber) {
        kamikazeInfoSubscriber.shutdown();
    }
    if (rcInSubscriber) {
        rcInSubscriber.shutdown();
    }
    if (rosNodeHandle) {
        delete rosNodeHandle;
    }
    
    delete ui;
}

// GNSS zamanı ile sunucu zamanı arasındaki farkı hesapla ve +1000ms ekle
void MainWindow::updateServerGpsOffset() {
    // Referans zamanları belirle
    QDateTime gpsRef = hasGpsTimeRef ? lastGpsTimeRef : QDateTime::currentDateTime();
    QDateTime srvRef;
    if (serverTimeInitialized) {
        qint64 elapsedMs = serverElapsed.elapsed();
        srvRef = serverBaseTime.addMSecs(elapsedMs);
    } else {
        srvRef = QDateTime::currentDateTime();
    }

    // Ofset: sunucu - gps + 1000 ms
    qint64 diff = gpsRef.msecsTo(srvRef);
    serverOffsetMs = diff + 1000; // örneğe göre +1s

    // TelemetryBridge'e de uygula
    if (telemetryBridge) {
        telemetryBridge->setServerOffsetMs(serverOffsetMs);
    }

    qDebug() << "[TimeCalib] serverOffsetMs=" << serverOffsetMs
             << "(server=" << srvRef.toString("HH:mm:ss.zzz")
             << ", gps=" << gpsRef.toString("HH:mm:ss.zzz") << ")";
}

// Üst panelde gösterilen serverTimeLabel anındaki zamanı ROS /server_time topiğine publish et
void MainWindow::publishServerTimeOnce() {
    if (!rosNodeHandle || !serverTimePublisher || !serverTimeInitialized) return;
    // Üst panelde gösterilen zamanı hesapla
    QDateTime displayedTime;
    if (serverTimeInitialized) {
        qint64 elapsedMs = serverElapsed.elapsed();
        displayedTime = serverBaseTime.addMSecs(elapsedMs);
    } else {
        displayedTime = QDateTime::currentDateTime();
    }
    sensor_msgs::TimeReference msg;
    qint64 msec = displayedTime.toMSecsSinceEpoch();
    msg.time_ref.sec = static_cast<uint32_t>(msec / 1000);
    msg.time_ref.nsec = static_cast<uint32_t>((msec % 1000) * 1000000);
    msg.source = "server";
    serverTimePublisher.publish(msg);
}

// İrtifa güncelleme fonksiyonu
void MainWindow::updateAltitude(double altitude) {
    if (altitudeLabel) {
        altitudeLabel->setText(QString("%1 m").arg(altitude, 0, 'f', 1));
    }
}

// Hız güncelleme fonksiyonu
void MainWindow::updateSpeed(double speed) {
    if (speedLabel) {
        speedLabel->setText(QString("%1 m/s").arg(speed, 0, 'f', 1));
    }
}

// RSS callback fonksiyonu
void MainWindow::onRcInReceived(const mavros_msgs::RCIn::ConstPtr& msg) {
    // RSS değerlerini güncelle (rssi field'ından)
    if (msg->rssi > 0) {
        updateRssStatus(msg->rssi, 0);
    }
}

// RSS güncelleme fonksiyonu
void MainWindow::updateRssStatus(int rssi, int remrssi) {
    if (!rssStatusLabel) return;
    
    // RSSI (0–255) değerini %0–100 aralığına çevir
    int r = qBound(0, rssi, 255);
    int rssPercentage = static_cast<int>(std::round(r * 100.0 / 255.0));
    
    // Renk kodlaması
    QString color;
    if (rssPercentage >= 80) {
        color = "#27ae60"; // Yeşil - İyi sinyal
    } else if (rssPercentage >= 50) {
        color = "#f39c12"; // Sarı - Orta sinyal
    } else {
        color = "#e74c3c"; // Kırmızı - Kötü sinyal
    }
    
    // RSS label'ını güncelle
    rssStatusLabel->setText(QString("RSS: %1%").arg(rssPercentage));
    
    // Renk güncelle
    QString styleSheet = QString("font-weight: bold; font-size: 10pt; color: %1; background-color: #232b3a; padding: 2px 8px; border-radius: 4px;").arg(color);
    rssStatusLabel->setStyleSheet(styleSheet);
}
