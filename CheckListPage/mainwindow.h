#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QPushButton>
#include "checklist.h"
#include "loginpage.h"

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

private slots:
    void on_loginButton_clicked();
    void on_backToChecklistButton_clicked();

private:
    Ui::MainWindow *ui;
    QStackedWidget *stack;
    Checklist *checklistPage;
    LoginPage *loginPage;
    QPushButton *loginBtn;
    QPushButton *checklistBtn;
};

#endif // MAINWINDOW_H
