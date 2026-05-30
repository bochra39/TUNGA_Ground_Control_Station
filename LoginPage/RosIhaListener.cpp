#include "RosIhaListener.h"
#include <QDir>
#include <QRegularExpression>

RosIhaListener::RosIhaListener(QObject *parent)
    : QObject(parent)
    , rosProcess(nullptr)
    , updateTimer(nullptr)
    , isListening(false)
{
    rosProcess = new QProcess(this);
    updateTimer = new QTimer(this);
    
    connect(rosProcess, &QProcess::readyReadStandardOutput, this, &RosIhaListener::processRosOutput);
    connect(rosProcess, &QProcess::readyReadStandardError, this, [this]() {
        QString error = rosProcess->readAllStandardError();
        qDebug() << "ROS Error:" << error;
    });
    
    connect(updateTimer, &QTimer::timeout, this, &RosIhaListener::readRosData);
}

RosIhaListener::~RosIhaListener()
{
    stopListening();
}

void RosIhaListener::startListening()
{
    if (isListening) return;
    
    isListening = true;
    
    // Önce topic'in var olup olmadığını kontrol et
    QString checkCommand = "bash -c 'source /opt/ros/noetic/setup.bash && rostopic list | grep -q /iha_positions'";
    QProcess checkProcess;
    checkProcess.start("bash", QStringList() << "-c" << checkCommand);
    checkProcess.waitForFinished(2000);
    
    if (checkProcess.exitCode() != 0) {
        qDebug() << "Topic /iha_positions bulunamadı. Mevcut topic'ler:";
        QProcess listProcess;
        listProcess.start("bash", QStringList() << "-c" << "source /opt/ros/noetic/setup.bash && rostopic list");
        listProcess.waitForFinished(2000);
        QString topics = QString::fromUtf8(listProcess.readAllStandardOutput());
        qDebug() << topics;
        
        // Topic yoksa alternatif topic'leri dene
        QStringList alternativeTopics = {"/mavros/global_position/global", "/mavros/state"};
        for (const QString& topic : alternativeTopics) {
            QString altCheckCommand = QString("bash -c 'source /opt/ros/noetic/setup.bash && rostopic list | grep -q %1'").arg(topic);
            QProcess altCheckProcess;
            altCheckProcess.start("bash", QStringList() << "-c" << altCheckCommand);
            altCheckProcess.waitForFinished(2000);
            
            if (altCheckProcess.exitCode() == 0) {
                qDebug() << "Alternatif topic kullanılıyor:" << topic;
                QString command = QString("bash -c 'source /opt/ros/noetic/setup.bash && rostopic echo %1 -n 1'").arg(topic);
                rosProcess->setWorkingDirectory(QDir::homePath());
                rosProcess->start("bash", QStringList() << "-c" << command);
                break;
            }
        }
    } else {
        // Normal topic varsa onu kullan
        QString command = "bash -c 'source /opt/ros/noetic/setup.bash && rostopic echo /iha_positions -n 1'";
        rosProcess->setWorkingDirectory(QDir::homePath());
        rosProcess->start("bash", QStringList() << "-c" << command);
    }
    
    // Her 3 saniyede bir güncelle (1 saniye yerine)
    updateTimer->start(3000);
    
    qDebug() << "ROS IHA Listener başlatıldı";
}

void RosIhaListener::stopListening()
{
    if (!isListening) return;
    
    isListening = false;
    updateTimer->stop();
    
    if (rosProcess->state() == QProcess::Running) {
        rosProcess->terminate();
        rosProcess->waitForFinished(3000);
        if (rosProcess->state() == QProcess::Running) {
            rosProcess->kill();
        }
    }
    
    qDebug() << "ROS IHA Listener durduruldu";
}

void RosIhaListener::readRosData()
{
    static int restartCounter = 0;
    
    if (!isListening) return;
    
    // Process çalışmıyorsa ve restart sayacı 5'ten azsa yeniden başlat
    if (rosProcess->state() != QProcess::Running && restartCounter < 5) {
        restartCounter++;
        qDebug() << "ROS process yeniden başlatılıyor (deneme:" << restartCounter << ")";
        
        // Yeni veri almak için process'i yeniden başlat
        rosProcess->terminate();
        rosProcess->waitForFinished(1000);
        
        // Topic'in hala var olup olmadığını kontrol et (sadece ilk birkaç denemede)
        if (restartCounter <= 2) {
            QString checkCommand = "bash -c 'source /opt/ros/noetic/setup.bash && rostopic list | grep -q /iha_positions'";
            QProcess checkProcess;
            checkProcess.start("bash", QStringList() << "-c" << checkCommand);
            checkProcess.waitForFinished(2000);
            
            if (checkProcess.exitCode() != 0) {
                // Alternatif topic'leri dene
                QStringList alternativeTopics = {"/mavros/global_position/global", "/mavros/state"};
                for (const QString& topic : alternativeTopics) {
                    QString altCheckCommand = QString("bash -c 'source /opt/ros/noetic/setup.bash && rostopic list | grep -q %1'").arg(topic);
                    QProcess altCheckProcess;
                    altCheckProcess.start("bash", QStringList() << "-c" << altCheckCommand);
                    altCheckProcess.waitForFinished(2000);
                    
                    if (altCheckProcess.exitCode() == 0) {
                        QString command = QString("bash -c 'source /opt/ros/noetic/setup.bash && rostopic echo %1 -n 1'").arg(topic);
                        rosProcess->start("bash", QStringList() << "-c" << command);
                        return;
                    }
                }
                // Hiçbir topic bulunamazsa, dinlemeyi durdur
                qDebug() << "Hiçbir ROS topic'i bulunamadı. Dinleme durduruluyor.";
                stopListening();
                return;
            } else {
                // Normal topic varsa onu kullan
                QString command = "bash -c 'source /opt/ros/noetic/setup.bash && rostopic echo /iha_positions -n 1'";
                rosProcess->start("bash", QStringList() << "-c" << command);
            }
        } else {
            // Sonraki denemelerde sadece mevcut topic'i kullan
            QString command = "bash -c 'source /opt/ros/noetic/setup.bash && rostopic echo /mavros/global_position/global -n 1'";
            rosProcess->start("bash", QStringList() << "-c" << command);
        }
    } else if (restartCounter >= 5) {
        // Çok fazla deneme yapıldıysa timer'ı durdur
        qDebug() << "Çok fazla restart denemesi. Timer durduruluyor.";
        updateTimer->stop();
        restartCounter = 0;
    }
}

void RosIhaListener::processRosOutput()
{
    QString output = QString::fromUtf8(rosProcess->readAllStandardOutput());
    
    if (!output.trimmed().isEmpty()) {
        parseIhaData(output);
    }
}

void RosIhaListener::parseIhaData(const QString &data)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8(), &error);

    if (error.error == QJsonParseError::NoError && doc.isObject()) {
        QJsonObject obj = doc.object();
        
        // /iha_positions topic formatı
        if (obj.contains("telemetry") && obj["telemetry"].isArray()) {
            QJsonArray telemetryArray = obj["telemetry"].toArray();
            for (int i = 0; i < telemetryArray.size(); ++i) {
                QJsonObject telem = telemetryArray[i].toObject();
                int ihaId = i; // 0: bizim, 1-2: enemy
                double latitude = telem["iha_enlem"].toDouble();
                double longitude = telem["iha_boylam"].toDouble();
                double altitude = telem["iha_irtifa"].toDouble();
                int battery = telem["iha_batarya"].toInt();
                bool isEnemy = (i != 0);
                emit ihaDataUpdated(ihaId, latitude, longitude, altitude, battery, isEnemy);
            }
            return;
        }
        
        // /mavros/global_position/global topic formatı
        if (obj.contains("latitude") && obj.contains("longitude")) {
            int ihaId = 0; // Bizim IHA
            double latitude = obj["latitude"].toDouble();
            double longitude = obj["longitude"].toDouble();
            double altitude = obj.contains("altitude") ? obj["altitude"].toDouble() : 0.0;
            int battery = 100; // Varsayılan değer
            bool isEnemy = false;
            emit ihaDataUpdated(ihaId, latitude, longitude, altitude, battery, isEnemy);
            qDebug() << "Mavros global position güncellendi:" << "lat:" << latitude << "lon:" << longitude;
            return;
        }
        
        // /mavros/state topic formatı
        if (obj.contains("mode") && obj.contains("armed")) {
            // State bilgisi, pozisyon bilgisi değil
            qDebug() << "Mavros state güncellendi:" << "mode:" << obj["mode"].toString() << "armed:" << obj["armed"].toBool();
            return;
        }
    }
    
    // Eski kod: Tekli JSON veya string parse
    QStringList lines = data.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        QString trimmedLine = line.trimmed();
        if (trimmedLine.startsWith("{") && trimmedLine.endsWith("}")) {
            QJsonParseError error;
            QJsonDocument doc = QJsonDocument::fromJson(trimmedLine.toUtf8(), &error);
            if (error.error == QJsonParseError::NoError && doc.isObject()) {
                QJsonObject obj = doc.object();
                int ihaId = obj["iha_id"].toInt();
                double latitude = obj["latitude"].toDouble();
                double longitude = obj["longitude"].toDouble();
                double altitude = obj["altitude"].toDouble();
                int battery = obj["battery"].toInt();
                bool isEnemy = obj["is_enemy"].toBool();
                emit ihaDataUpdated(ihaId, latitude, longitude, altitude, battery, isEnemy);
                qDebug() << "IHA güncellendi:" << ihaId << "lat:" << latitude << "lon:" << longitude;
            }
        }
        else if (trimmedLine.contains("iha_enlem") && trimmedLine.contains("iha_boylam")) {
            QJsonParseError error;
            QJsonDocument doc = QJsonDocument::fromJson(trimmedLine.toUtf8(), &error);
            if (error.error == QJsonParseError::NoError && doc.isObject()) {
                QJsonObject obj = doc.object();
                int ihaId = obj.contains("takim_numarasi") ? obj["takim_numarasi"].toInt() : 0;
                double latitude = obj["iha_enlem"].toDouble();
                double longitude = obj["iha_boylam"].toDouble();
                // Dost IHA
                emit ihaDataUpdated(ihaId, latitude, longitude, 0.0, 0, false);
                qDebug() << "IHA güncellendi (enlem-boylam parse):" << ihaId << "lat:" << latitude << "lon:" << longitude;
                // 1. Enemy IHA (örnek sabit koordinat)
                emit ihaDataUpdated(1001, latitude + 0.01, longitude + 0.01, 0.0, 0, true);
                // 2. Enemy IHA (örnek sabit koordinat)
                emit ihaDataUpdated(1002, latitude - 0.01, longitude - 0.01, 0.0, 0, true);
            }
        }
    }
} 