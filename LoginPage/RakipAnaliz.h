#ifndef RAKIPANALIZ_H
#define RAKIPANALIZ_H

#include <QObject>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QHash>
#include <QPointF>
#include <deque>

// Basit analiz işçisi: periyodik olarak analiz metni üretir ve sinyal yollar
class RakipAnaliz : public QObject {
    Q_OBJECT
public:
    explicit RakipAnaliz(QObject* parent = nullptr);
    ~RakipAnaliz();

    void setServerUrl(const QString& url);
    void setCookieJar(QNetworkCookieJar* jar);

public slots:
    void start();
    void stop();
    void consumeRivalsJson(const QString& rawJson); // MainPageWidget'ten gelen ham JSON

signals:
    void analysisReady(const QString& text);

private slots:
    void onTick();

private:
    QTimer timer_;
    QString serverUrl_;
    QNetworkAccessManager* net_{nullptr};

    // Analiz durumu
    struct TeamState {
        int teamId{ -1 };
        std::deque<double> yawHist;         // iha_yonelme
        std::deque<double> yawDiffHist;     // ardışık farklar
        std::deque<double> altHist;         // irtifa
        std::deque<double> altDiffHist;     // ardışık farklar
        std::deque<double> speedHist;       // hız
        std::deque<double> speedDiffHist;   // ardışık farklar
        std::deque<QPointF> routeHist;      // lon,lat son N nokta
        double lastYaw{0.0};
        double lastAlt{0.0};
        double lastSpeed{std::numeric_limits<double>::quiet_NaN()};
        double prevSpeed{std::numeric_limits<double>::quiet_NaN()};
        int speedSampleCount{0};
        double score{0.0};
    };

    QHash<int, TeamState> teams_;           // teamId -> state
    QHash<int, QPointF> lastPos_;           // teamId -> (lon,lat)

    // Yardımcılar
    static double clamp(double v, double lo, double hi);
    static double deg2rad(double d);
    static double rad2deg(double r);
    static double haversineMeters(double lat1, double lon1, double lat2, double lon2);
    static double bearingDeg(double lat1, double lon1, double lat2, double lon2);
    static double angleDiff(double a, double b); // 0..180 kısa fark
    template<typename T>
    static void pushBounded(std::deque<T>& dq, const T& v, size_t maxn = 10) {
        dq.push_back(v); if (dq.size() > maxn) dq.pop_front();
    }
};

#endif // RAKIPANALIZ_H


