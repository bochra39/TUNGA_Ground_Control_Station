#include "CameraWidget.h"
#include <QVBoxLayout>
#include <QDebug>
#include <QImage>
#include <QMessageBox>
#include <cstdio>
#include <QTimer>
#include <QElapsedTimer>

// ---- CameraWorker Implementation ----
CameraWorker::CameraWorker(const QString& url, int w, int h, QObject* parent)
    : QThread(parent), rtspUrl(url), width(w), height(h), running(false) {}

CameraWorker::~CameraWorker() { stop(); }

void CameraWorker::stop() {
    running = false;
    wait(1000);
}

void CameraWorker::run() {
    running = true;
    size_t frameSize = static_cast<size_t>(width) * height * 3;
    std::vector<uint8_t> buffer(frameSize);
    
    // Optimized ffmpeg command for low latency UDP streaming
    std::string cmd =
        "ffmpeg -fflags nobuffer -flags low_delay "
        "-probesize 32 -analyzeduration 0 "
        "-i '" + rtspUrl.toStdString() + "' "
        "-vf scale=" + std::to_string(width) + ":" + std::to_string(height) + ",setpts=0 "
        "-f image2pipe -pix_fmt bgr24 -vcodec rawvideo "
        "-vsync 0 -r 30 -"; // Force 30 FPS, disable video sync
    
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        qDebug() << "Failed to start ffmpeg process";
        emit frameReady(QImage());
        return;
    }
    
    qDebug() << "Camera stream started:" << QString::fromStdString(cmd);
    
    // Frame rate control
    const int targetFPS = 30;
    const int frameInterval = 1000 / targetFPS; // milliseconds
    QElapsedTimer frameTimer;
    frameTimer.start();
    
    while (running) {
        size_t bytesRead = fread(buffer.data(), 1, frameSize, pipe);
        
        if (bytesRead < frameSize) {
            if (feof(pipe)) {
                qDebug() << "End of stream reached";
                break;
            }
            if (ferror(pipe)) {
                qDebug() << "Stream error occurred";
                break;
            }
            msleep(5); // Shorter sleep for better responsiveness
            continue;
        }
        
        // Frame rate control
        if (frameTimer.elapsed() < frameInterval) {
            msleep(1);
            continue;
        }
        frameTimer.restart();
        
        // Create OpenCV Mat and QImage without unnecessary copy
        cv::Mat mat(height, width, CV_8UC3, buffer.data());
        QImage img(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_BGR888);
        
        // Emit frame without copy for better performance
        emit frameReady(img);
    }
    
    pclose(pipe);
    qDebug() << "Camera stream stopped";
}

// ---- CameraWidget Implementation ----
CameraWidget::CameraWidget(QWidget* parent) : QWidget(parent) {
    cameraLabel = new QLabel(this);
    cameraLabel->setAlignment(Qt::AlignCenter);
    cameraLabel->setFixedSize(640, 480);
    cameraLabel->setStyleSheet("background: black;");

    // statusLabel = new QLabel("", this);
    // statusLabel->setAlignment(Qt::AlignCenter);
    // statusLabel->setStyleSheet("color: #888; font-size: 18px; background: transparent;");
    // statusLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    // statusLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    // statusLabel->setFixedSize(this->size());
    // statusLabel->move(0, 0);
    // statusLabel->show();

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(cameraLabel);
    setLayout(layout);
    // Widget boyutunu da sabitle (layout marginleri 0 olduğu için 640x480)
    setFixedSize(640, 480);
}

CameraWidget::~CameraWidget() {
    if (worker) {
        worker->stop();
        worker->deleteLater();
        worker = nullptr;
    }
}

void CameraWidget::onConnectClicked() {
    // statusLabel->setFixedSize(this->size());
    // statusLabel->move(0, 0);
    if (!isConnected) {
        if (worker) return;
        QString url = "udp://192.168.50.2:5000";
        int width = 640, height = 480;
        worker = new CameraWorker(url, width, height, this);
        connect(worker, &CameraWorker::frameReady, this, &CameraWidget::updateFrame);
        worker->setPriority(QThread::HighPriority); // Set high priority for smooth streaming
        worker->start();
        // statusLabel->setText("Bağlanıyor...");
        // statusLabel->setStyleSheet("color: #FFD600; font-size: 18px; background: transparent;");
        // statusLabel->show();
        cameraLabel->clear();
        isConnected = true;
    } else {
        if (worker) {
            worker->stop();
            worker->wait();
            worker->deleteLater();
            worker = nullptr;
        }
        // statusLabel->setText("Bağlantı Kesildi");
        // statusLabel->setStyleSheet("color: #e74c3c; font-size: 18px; background: transparent;");
        // statusLabel->show();
        cameraLabel->clear();
        isConnected = false;
    }
}

void CameraWidget::onRefreshClicked() {
    // statusLabel->setFixedSize(this->size());
    // statusLabel->move(0, 0);

    if (worker) {
        connect(worker, &QThread::finished, this, [this]() {
            worker->deleteLater();
            worker = nullptr;
            isConnected = false;
            // statusLabel->setText("Yenileniyor...");
            // statusLabel->setStyleSheet("color: #3498db; font-size: 18px; background: transparent;");
            // statusLabel->show();
            cameraLabel->clear();
            onConnectClicked(); // Yeniden yayına bağlan
        });
        worker->stop();
        // wait() YOK! Arayüz donmaz.
        return;
    }

    // Worker yoksa doğrudan başlat
    isConnected = false;
    // statusLabel->setText("Yenileniyor...");
    // statusLabel->setStyleSheet("color: #3498db; font-size: 18px; background: transparent;");
    // statusLabel->show();
    cameraLabel->clear();
    onConnectClicked();
}

void CameraWidget::updateFrame(const QImage& img) {
    if (img.isNull()) {
        cameraLabel->clear();
        isConnected = false;
        return;
    }
    
    // Optimized image display - no scaling needed since widget is fixed size
    cameraLabel->setPixmap(QPixmap::fromImage(img));
    
    // Status label logic (commented out for now)
    // if (statusLabel->isVisible() && (statusLabel->text() == "Bağlanıyor..." || statusLabel->text() == "Yenileniyor...")) {
    //     statusLabel->setText("Bağlandı");
    //     statusLabel->setStyleSheet("color: #27ae60; font-size: 18px; background: transparent;");
    //     statusLabel->show();
    //     QTimer::singleShot(1000, this, [this]() { statusLabel->hide(); });
    // } else if (statusLabel->text() == "Bağlantı Kesildi") {
    //     // Eğer hata mesajı varsa, dokunma
    // } else {
    //     statusLabel->hide();
    // }
}

void CameraWidget::showStatusMessage(const QString& /*msg*/, const QString& /*color*/) {
    // statusLabel->setText(msg);
    // statusLabel->setStyleSheet(QString("color: %1; font-size: 18px; background: transparent;").arg(color));
    // statusLabel->show();
}

void CameraWidget::onShowClicked() {
    showStatusMessage("Kamera gösteriliyor", "#FFD600");
}
void CameraWidget::onStartClicked() {
    showStatusMessage("Kamera başlatılıyor", "#27ae60");
    onConnectClicked(); // Gerçek bağlantıyı başlat
}
void CameraWidget::onStopClicked() {
    showStatusMessage("Kamera durduruluyor", "#e74c3c");
}
void CameraWidget::onLiveStreamClicked() {
    showStatusMessage("Canlı yayın başlatılıyor", "#3498db");
}
void CameraWidget::onLockClicked() {
    showStatusMessage("Kilitlenme başlatılıyor", "#FFD600");
}
void CameraWidget::onKamikazeClicked() {
    showStatusMessage("Kamikaze başlatılıyor", "#e67e22");
}
