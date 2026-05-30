#ifndef CAMERAWIDGET_H
#define CAMERAWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QThread>
#include <atomic>
#include <opencv2/opencv.hpp>

class CameraWorker : public QThread {
    Q_OBJECT
public:
    CameraWorker(const QString& url, int width, int height, QObject* parent = nullptr);
    ~CameraWorker();
    void stop();

signals:
    void frameReady(const QImage& img);

protected:
    void run() override;

private:
    QString rtspUrl;
    int width, height;
    std::atomic<bool> running;
};

class CameraWidget : public QWidget {
    Q_OBJECT
public:
    explicit CameraWidget(QWidget* parent = nullptr);
    ~CameraWidget();

public slots:
    void onConnectClicked();
    void onShowClicked();
    void onStartClicked();
    void onStopClicked();
    void onLiveStreamClicked();
    void onLockClicked();
    void onKamikazeClicked();
    void updateFrame(const QImage& img);
    void onRefreshClicked();
    void showStatusMessage(const QString& msg, const QString& color = "#27ae60");

private:
    QLabel* cameraLabel;
    CameraWorker* worker = nullptr;
    bool isConnected = false;
};

#endif // CAMERAWIDGET_H 