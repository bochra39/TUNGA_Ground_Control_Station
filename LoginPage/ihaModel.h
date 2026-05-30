// IhaDataObject.h
#ifndef IHADATAOBJECT_H
#define IHADATAOBJECT_H

#include <QObject>
#include <QList>

class IhaDataObject : public QObject {
    Q_OBJECT
    Q_PROPERTY(int takim_numarasi READ takim_numarasi WRITE setTakimNumarasi NOTIFY dataChanged)
    Q_PROPERTY(double iha_enlem READ iha_enlem WRITE setIhaEnlem NOTIFY dataChanged)
    Q_PROPERTY(double iha_boylam READ iha_boylam WRITE setIhaBoylam NOTIFY dataChanged)
    Q_PROPERTY(double iha_hiz READ iha_hiz WRITE setIhaHiz NOTIFY dataChanged)
    Q_PROPERTY(int iha_batarya READ iha_batarya WRITE setIhaBatarya NOTIFY dataChanged)
    Q_PROPERTY(bool isEnemy READ isEnemy WRITE setIsEnemy NOTIFY dataChanged)

public:
    IhaDataObject(QObject *parent = nullptr) : QObject(parent) {}
    IhaDataObject(int takimNo, double enlem, double boylam, double hiz, int batarya, QObject* parent = nullptr)
        : QObject(parent), m_takim_numarasi(takimNo), m_iha_enlem(enlem), m_iha_boylam(boylam), m_iha_hiz(hiz), m_iha_batarya(batarya), m_isEnemy(false) {}
    IhaDataObject(int takimNo, double enlem, double boylam, double hiz, int batarya, bool enemy, QObject* parent = nullptr)
        : QObject(parent), m_takim_numarasi(takimNo), m_iha_enlem(enlem), m_iha_boylam(boylam), m_iha_hiz(hiz), m_iha_batarya(batarya), m_isEnemy(enemy) {}

    int takim_numarasi() const { return m_takim_numarasi; }
    void setTakimNumarasi(int val) { m_takim_numarasi = val; emit dataChanged(); }

    double iha_enlem() const { return m_iha_enlem; }
    void setIhaEnlem(double val) { m_iha_enlem = val; emit dataChanged(); }

    double iha_boylam() const { return m_iha_boylam; }
    void setIhaBoylam(double val) { m_iha_boylam = val; emit dataChanged(); }

    double iha_hiz() const { return m_iha_hiz; }
    void setIhaHiz(double val) { m_iha_hiz = val; emit dataChanged(); }

    int iha_batarya() const { return m_iha_batarya; }
    void setIhaBatarya(int val) { m_iha_batarya = val; emit dataChanged(); }

    bool isEnemy() const { return m_isEnemy; }
    void setIsEnemy(bool val) { m_isEnemy = val; emit dataChanged(); }

signals:
    void dataChanged();

private:
    int m_takim_numarasi;
    double m_iha_enlem;
    double m_iha_boylam;
    double m_iha_hiz;
    int m_iha_batarya;
    bool m_isEnemy;
};

#endif // IHADATAOBJECT_H
