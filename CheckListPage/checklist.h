#ifndef CHECKLIST_H
#define CHECKLIST_H

#include <QWidget>

class Checklist : public QWidget
{
    Q_OBJECT
public:
    explicit Checklist(QWidget *parent = nullptr);
    ~Checklist();

signals:
    void goToLoginPage();
};

#endif // CHECKLIST_H 