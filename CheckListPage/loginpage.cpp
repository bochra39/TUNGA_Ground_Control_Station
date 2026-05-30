#include "loginpage.h"
#include <QPushButton>
#include <QVBoxLayout>

LoginPage::LoginPage(QWidget *parent) : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    QPushButton *checklistButton = new QPushButton("Checklist'e Geç", this);
    layout->addWidget(checklistButton);
    // Checklist'e geçiş için bir sinyal yaymak istersen burada emit edebilirsin
    // Ancak MainWindow'da doğrudan butonun clicked sinyalini kullanacağız
}

LoginPage::~LoginPage() {} 