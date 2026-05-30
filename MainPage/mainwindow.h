#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QPixmap>
#include <QFrame>
#include <QHBoxLayout>
#include <QList>
#include "CameraWidget.h"
#include <QQuickWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    // QCheckBox* centerCheckBox; // Orta alan için checkbox
    // QLabel* centerLabel;       // Orta alan için label
    QPushButton* lightButton = nullptr;
    bool isLightMode = false;
    void applyTheme(bool lightMode);
    QPixmap lightModeIcon;
    QPixmap darkModeIcon;
    QFrame* topBar = nullptr;
    QHBoxLayout* topBarLayout = nullptr;
    QList<QLabel*> allLabels;
    CameraWidget* cameraWidget;
    QQuickWidget* mapWidget = nullptr;
    QList<QPushButton*> menuButtons;
    int activeMenuIndex = 0;
};
#endif // MAINWINDOW_H
