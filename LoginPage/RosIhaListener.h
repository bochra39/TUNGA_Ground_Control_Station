#ifndef ROSIHALISTENER_H
#define ROSIHALISTENER_H

#include <QObject>
#include <QTimer>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

class RosIhaListener : public QObject
{
    Q_OBJECT

public:
    explicit RosIhaListener(QObject *parent = nullptr);
    ~RosIhaListener();

    void startListening();
    void stopListening();

signals:
    void ihaDataUpdated(int ihaId, double latitude, double longitude, double altitude, int battery, bool isEnemy);

private slots:
    void readRosData();
    void processRosOutput();

private:
    QProcess *rosProcess;
    QTimer *updateTimer;
    bool isListening;
    
    void parseIhaData(const QString &data);
};

#endif // ROSIHALISTENER_H 