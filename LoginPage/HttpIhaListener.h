#ifndef HTTPIHALISTENER_H
#define HTTPIHALISTENER_H

#include <QObject>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class HttpIhaListener : public QObject
{
    Q_OBJECT
public:
    explicit HttpIhaListener(QObject *parent = nullptr);
    void start();
    void stop();

signals:
    void ihaPositionUpdated(int ihaId, double latitude, double longitude, bool isEnemy);

private slots:
    void fetchData();
    void onReplyFinished();

private:
    QTimer* timer;
    QNetworkAccessManager* manager;
};

#endif // HTTPIHALISTENER_H 