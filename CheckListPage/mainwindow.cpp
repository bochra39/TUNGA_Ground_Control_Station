#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QCheckBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QFrame>
#include <QPixmap>
#include <QPainter>
#include <QTransform>
#include <QMessageBox>
#include <QProcess>
#include <QStringList>
#include <QList>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("Görev Kontrol Arayüzü");
    setStyleSheet("background-color: #e5e5e5;");

    // Ana pencereyi ayarla
    this->resize(1500, 850);

    // Widget listeleri ve işaretçiler
    QList<QCheckBox*> allCheckBoxes;
    QList<QLabel*> allHeaderLabels;
    QLabel* counterLabel = nullptr;
    QPushButton* lightButton = nullptr;

    // Üst bar container
    QFrame *topBar = new QFrame;
    topBar->setFixedHeight(90);
    QHBoxLayout *topBarLayout = new QHBoxLayout(topBar);
    topBarLayout->setSpacing(1);
    topBarLayout->setContentsMargins(0, 0, 0, 0);
    int sectionWidth = 1500 / 7;
    for(int i = 0; i < 7; i++) {
        QFrame *section = new QFrame;
        section->setFixedWidth(sectionWidth);
        if(i == 0) {
            QVBoxLayout *sectionLayout = new QVBoxLayout(section);
            sectionLayout->setAlignment(Qt::AlignCenter);
            QLabel *iconLabel = new QLabel;
            iconLabel->setStyleSheet("border: none;");
            iconLabel->setPixmap(QPixmap(":/icons/Plane.png").scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            sectionLayout->addWidget(iconLabel);
        } else if(i == 1) {
            QVBoxLayout *sectionLayout = new QVBoxLayout(section);
            sectionLayout->setAlignment(Qt::AlignCenter);
            QLabel *iconLabel2 = new QLabel;
            iconLabel2->setStyleSheet("border: none;");
            iconLabel2->setPixmap(QPixmap(":/icons/Gps.png").scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            sectionLayout->addWidget(iconLabel2);
        } else if(i == 2) {
            QVBoxLayout *sectionLayout = new QVBoxLayout(section);
            sectionLayout->setAlignment(Qt::AlignCenter);
            QLabel *iconLabel3 = new QLabel;
            iconLabel3->setStyleSheet("border: none;");
            iconLabel3->setPixmap(QPixmap(":/icons/Power.png").scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            sectionLayout->addWidget(iconLabel3);
        } else if(i == 3) {
            QVBoxLayout *sectionLayout = new QVBoxLayout(section);
            sectionLayout->setAlignment(Qt::AlignCenter);
            QLabel *iconLabel4 = new QLabel;
            iconLabel4->setStyleSheet("border: none;");
            iconLabel4->setPixmap(QPixmap(":/icons/Server.png").scaled(36, 36, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            sectionLayout->addWidget(iconLabel4);
        } else if(i == 4) {
            QVBoxLayout *sectionLayout = new QVBoxLayout(section);
            sectionLayout->setAlignment(Qt::AlignCenter);
            QLabel *iconLabel5 = new QLabel;
            iconLabel5->setStyleSheet("border: none;");
            iconLabel5->setPixmap(QPixmap(":/icons/Telemetry.png").scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            sectionLayout->addWidget(iconLabel5);
        } else if(i == 5) {
            section->setStyleSheet("background-color: #000000; border: none;");
            QHBoxLayout *hLayout = new QHBoxLayout(section);
            counterLabel = new QLabel("0 / 0");
            counterLabel->setStyleSheet("color: #111; font-weight: bold; font-size: 13pt; background: transparent; border: none;");
            QPushButton *resetButton = new QPushButton("Sıfırla", section);
            resetButton->setFixedSize(110, 40);
            resetButton->setStyleSheet(
                "QPushButton { color: #111; font-weight: bold; font-size: 11pt; background: #e0e0e0; border-radius: 8px; border: 1px solid #888; }"
                "QPushButton:hover { background: #C0C0C0; color: #111; border: 1.5px solid #888; }"
            );
            hLayout->addWidget(counterLabel);
            hLayout->addStretch();
            hLayout->addWidget(resetButton);
            QObject::connect(resetButton, &QPushButton::clicked, [=]() {
                for (QCheckBox *cb : allCheckBoxes) { cb->setChecked(false); }
                if (counterLabel) { counterLabel->setText(QString("0 / %1").arg(allCheckBoxes.size())); }
            });
        } else if(i == 6) {
            QVBoxLayout *sectionLayout = new QVBoxLayout(section);
            sectionLayout->setAlignment(Qt::AlignCenter);
            lightButton = new QPushButton(section);
            lightButton->setObjectName("lightButton");
            lightButton->setFixedSize(80, 50);
            lightButton->setStyleSheet(
                "QPushButton { background: #e0e0e0; color: #111; font-weight: bold; font-size: 11pt; border-radius: 8px; border: 1px solid #888; }"
                "QPushButton:hover { background: #444; color: #fff; }"
            );
            sectionLayout->addWidget(lightButton);
        }
        if(i < 5) section->setStyleSheet("background-color: #dbd7d7; border: none;");
        if(i == 5) section->setStyleSheet("background-color: #dbd7d7; border: none;");
        topBarLayout->addWidget(section);
    }
    // Sol Menü
    QFrame *leftMenu = new QFrame;
    leftMenu->setFixedWidth(80);
    leftMenu->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    leftMenu->setStyleSheet("background-color: #353232;");
    QStringList iconFiles = {"Menu.png","HomePage.png","Login.png", "Checklist.png","Simulasyon.png"};
    QList<QPushButton*> menuButtons;
    QVBoxLayout *leftMenuLayout = new QVBoxLayout(leftMenu);
    leftMenuLayout->setContentsMargins(0, 0, 0, 0);
    leftMenuLayout->setSpacing(0);
    leftMenuLayout->addSpacing(20);
    for (int i = 0; i < iconFiles.size(); ++i) {
        QPushButton *iconBtn = new QPushButton(leftMenu);
        iconBtn->setFixedSize(60, 60);
        iconBtn->setIcon(QIcon(QString(":/icons/%1").arg(iconFiles[i])));
        iconBtn->setIconSize(QSize(36, 36));
        iconBtn->setCursor(Qt::PointingHandCursor);
        iconBtn->setFlat(true);
        if (iconFiles[i] == "Checklist.png") {
            iconBtn->setStyleSheet(
                "QPushButton { background: #444; border-radius: 12px; border: 2px solid #888; }"
                "QPushButton:hover { background: #444; border-radius: 12px; border: 2px solid #888; }"
            );
            QPixmap pixmap(":/icons/Checklist.png");
            QPixmap whiteIcon = pixmap;
            QPainter painter(&whiteIcon);
            painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
            painter.fillRect(whiteIcon.rect(), Qt::white);
            painter.end();
            iconBtn->setIcon(QIcon(whiteIcon));
        } else {
            iconBtn->setStyleSheet(
                "QPushButton { background: transparent; border: none; margin: 0; }"
                "QPushButton:hover { background: #444; border-radius: 12px; }"
            );
        }
        leftMenuLayout->addWidget(iconBtn, 0, Qt::AlignHCenter);
        menuButtons.append(iconBtn);
        if (i == 0) leftMenuLayout->addSpacing(18);
        else leftMenuLayout->addSpacing(10);
    }
    if (menuButtons.size() > 2) {
        QObject::connect(menuButtons[2], &QPushButton::clicked, [=]() {
            QString exePath = QCoreApplication::applicationDirPath() + "/LoginPage/V2";
            QProcess::startDetached(exePath);
        });
    }
    leftMenuLayout->addStretch();
    leftMenu->setLayout(leftMenuLayout);
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    QWidget *scrollContentWidget = new QWidget();
    QHBoxLayout *contentLayout = new QHBoxLayout(scrollContentWidget);
    contentLayout->setSpacing(30);
    contentLayout->setContentsMargins(20, 10, 20, 10);
    QVBoxLayout *leftColumnLayout = new QVBoxLayout();
    QVBoxLayout *rightColumnLayout = new QVBoxLayout();
    auto createCheckboxSection = [&](QVBoxLayout* layout, const QString& title, const QStringList& items) {
        QLabel* header = new QLabel(title);
        layout->addWidget(header);
        allHeaderLabels.append(header);
        for (const QString& text : items) {
            QCheckBox* checkbox = new QCheckBox(text);
            layout->addWidget(checkbox);
            allCheckBoxes.append(checkbox);
        }
    };
    createCheckboxSection(leftColumnLayout, "Yapısal Kontroller", { "1. Gövde sağlamlığı kontrolü", "2. Kanat ve kuyruk sağlamlığı kontrolü", "3. ESC'lerin Sağlamlığı ve Bağlantı Kontrolü", "4. Motorların Motor Yuvasına Bağlantısının Kontrolü", "5. Servo Motorların Sağlamlık ve Bağlantı Kontrolü", "6. Kanatçıkların Manuel Modda Açı ve Yön Kontrolü", "7. Stabilize Modda Kanatçık Kontrolü", "8. Motorların Dönüş Yönü ve RPM Kontrolü", "9. Montaj Elemanlarının (vida) Kontrolü", "10. Ağırlık Merkezi Kontrolü", "11. Pervane Montajı, Pervane Yönü ve Sıklığı Kontrolü", "12. Antenlerin Hiza Kontrolü", "12. Aviyonik Kutunun Sağlamlık ve Bağlantı Kontrolü", "11. Kameranın Sağlamlık ve Bağlantı Kontrolü" });
    createCheckboxSection(leftColumnLayout, "Harita ve Konumlandırma Sistemleri", { "13. Rüzgar Yönünün Belirlenmesi, Uçuş Rotasının Düzenlenmesi", "14. Pitot Tüpü Veri Kontrolü", "15. GPS veri Kontrolü" });
    leftColumnLayout->addStretch();
    createCheckboxSection(rightColumnLayout, "Elektronik Sistem", { "16. Ana Elektrik Hattının Kontrolü", "17. Bataryanın Gerilim Kontrolü", "18. Bataryanın İHA içerisine Montajlanması ve Elektrik Hattına Bağlanması", "19. Sistemin Aktifleştirilip Bütün Elektronik Elemanlara Güç Gitmesinin Kontrolü", "20. Uçuş Kontrol Kartı Sağlamlık ve Bağlantı Kontrolü", "21. GPS ve Pitot Tüpünün Sağlamlık ve Bağlantı Kontrolü", "22. Kumanda Bataryası Doluluğu Kontrolü" });
    createCheckboxSection(rightColumnLayout, "Haberleşme Sistemleri", { "23. İHA ve YKİ Arası Bağlantı Sağlanması", "24. Kumandanın Aktifleştirilip İHA ile Bağlantısının Kontrolü", "25. Kumanda Modları ve Kumanda Bataryası Doluluğu Kontrolü", "26. Antenlerin Hiza Kontrolü", "27. Telemetri Veri Kontrolü", "28. Wi-Fi Modülü Bağlantı Sağlanması ve Veri Kontrolü", "29. Görev Bilgisayarı Veri Kontrolü", "30. Yayıncı Sunucusu Veri Kontrolü" });
    createCheckboxSection(rightColumnLayout, "Yazılım ve Uçuş Kontrol Sistemi", { "31. Uçuş Kontrol Kartı Veri Kontrolü", "32. Uçuş Kontrol Kartı Kalibrasyonu", "33. Uçuş Kontrol Kartı Parametre Kontrolü", "34. Auto-Arm Denemesi Kontrolü", "35. Manuel Modda Arm Verme ve İtki Kontrolü", "36. Kamera veri kontrolü" });
    rightColumnLayout->addStretch();
    contentLayout->addLayout(leftColumnLayout);
    contentLayout->addLayout(rightColumnLayout);
    scrollArea->setWidget(scrollContentWidget);
    if (counterLabel) { counterLabel->setText(QString("0 / %1").arg(allCheckBoxes.size())); }
    for (QCheckBox *checkbox : allCheckBoxes) {
        QObject::connect(checkbox, &QCheckBox::clicked, [=]() {
            int checkedCount = 0;
            for (QCheckBox *cb : allCheckBoxes) { if (cb->isChecked()) checkedCount++; }
            if (counterLabel) { counterLabel->setText(QString("%1 / %2").arg(checkedCount).arg(allCheckBoxes.size())); }
            if (checkedCount > 0 && checkedCount == allCheckBoxes.size()) {
                QMessageBox msgBox;
                msgBox.setWindowTitle("Tebrikler!");
                msgBox.setIcon(QMessageBox::Information);
                msgBox.setText("<div style='text-align:center;'><span style='font-size:20pt; font-weight:bold;'>Tunga Ekibi</span><br><span style='font-size:13pt; font-weight:normal;'>Artık Uçuşa Hazırsınız</span></div>");
                msgBox.setStandardButtons(QMessageBox::Ok);
                msgBox.setStyleSheet("QLabel{min-width:260px; min-height:60px;} QMessageBox{background:#f8f8f8; font-size:12pt;}");
                msgBox.exec();
            }
        });
    }
    QPixmap lightModeOriginal(":/icons/LightMode.png");
    QPixmap darkModeOriginal(":/icons/DarkMode.png");
    QTransform transform;
    transform.rotate(210);
    QPixmap lightModeIcon = lightModeOriginal.transformed(transform);
    QPixmap darkModeIcon = darkModeOriginal.transformed(transform);
    auto applyTheme = [=](bool lightMode) {
        if (!lightButton) return;
        if (lightMode) {
            this->setStyleSheet("background-color: #F0FFFF; color: #111;");
            topBar->setStyleSheet("background-color: #e8e8e8;");
            for (int i = 0; i < topBarLayout->count(); ++i) {
                if (auto s = qobject_cast<QFrame*>(topBarLayout->itemAt(i)->widget()))
                    s->setStyleSheet(i < 6 ? "background-color:#e8e8e8;" : "background-color:#e8e8e8;");
            }
            for (auto l : allHeaderLabels) if (l) l->setStyleSheet("color: #111; font-weight: bold; font-size: 11pt;");
            for (auto cb : allCheckBoxes) if (cb) cb->setStyleSheet("color: #111; font-size: 10.5pt;");
            lightButton->setIcon(QIcon(darkModeIcon.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        } else {
            this->setStyleSheet("background-color: #464646; color: #FFF;");
            topBar->setStyleSheet("background-color: #dbd7d7;");
            for (int i = 0; i < topBarLayout->count(); ++i) {
                if (auto s = qobject_cast<QFrame*>(topBarLayout->itemAt(i)->widget()))
                    s->setStyleSheet(i < 6 ? "background-color:#dbd7d7;" : "background-color:#dbd7d7;");
            }
            for (auto l : allHeaderLabels) if (l) l->setStyleSheet("color: #FFF; font-weight: bold; font-size: 11pt;");
            for (auto cb : allCheckBoxes) if (cb) cb->setStyleSheet("color: #FFF; font-size: 10.5pt;");
            lightButton->setIcon(QIcon(lightModeIcon.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
            scrollArea->setStyleSheet("QScrollArea { background: #f8f8f8; border-top: 2px solid #dbd7d7; border-right: 2px solid #dbd7d7; border-bottom: 2px solid #dbd7d7; border-left: none; }");
        }
        lightButton->setIconSize(QSize(40, 40));
    };
    bool isLightMode = false;
    if (lightButton) {
        QObject::connect(lightButton, &QPushButton::clicked, [=]() mutable {
            isLightMode = !isLightMode;
            applyTheme(isLightMode);
        });
    }
    applyTheme(false);
    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(leftMenu);
    mainLayout->addWidget(scrollArea, 2);
    QVBoxLayout *windowLayout = new QVBoxLayout(this->centralWidget());
    windowLayout->setContentsMargins(0, 0, 0, 0);
    windowLayout->setSpacing(0);
    windowLayout->addWidget(topBar);
    windowLayout->addLayout(mainLayout);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_loginButton_clicked()
{
    ui->stackedWidget->setCurrentIndex(1); // LoginPage'e geç
}

void MainWindow::on_backToChecklistButton_clicked()
{
    ui->stackedWidget->setCurrentIndex(0); // Checklist'e geri dön
}
