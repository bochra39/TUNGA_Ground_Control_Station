#include "checklist.h"
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QCheckBox>
#include <QLabel>
#include <QMessageBox>
#include <QList>

Checklist::Checklist(QWidget *parent) : QWidget(parent)
{
    QList<QCheckBox*> allCheckBoxes;
    QList<QLabel*> allHeaderLabels;
    QLabel* counterLabel = nullptr;
    QPushButton* lightButton = nullptr;

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    QWidget *scrollContentWidget = new QWidget;
    QHBoxLayout *contentLayout = new QHBoxLayout(scrollContentWidget);
    contentLayout->setSpacing(30);
    contentLayout->setContentsMargins(20, 10, 20, 10);
    QVBoxLayout *leftColumnLayout = new QVBoxLayout();
    QVBoxLayout *rightColumnLayout = new QVBoxLayout();

    // Checkbox listelerini oluştur
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
    mainLayout->addWidget(scrollArea);

    // Sayaç
    counterLabel = new QLabel(QString("0 / %1").arg(allCheckBoxes.size()));
    mainLayout->addWidget(counterLabel);
    QPushButton *resetButton = new QPushButton("Sıfırla", this);
    mainLayout->addWidget(resetButton);
    QObject::connect(resetButton, &QPushButton::clicked, [=]() {
        for (QCheckBox *cb : allCheckBoxes) { cb->setChecked(false); }
        if (counterLabel) { counterLabel->setText(QString("0 / %1").arg(allCheckBoxes.size())); }
    });

    // Checkbox sayaç güncelleme
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
}

Checklist::~Checklist() {} 