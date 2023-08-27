#include "toollist.h"
#include "ui_toollist.h"

ToolList::ToolList(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ToolList)
{
    ui->setupUi(this);

    QListWidget *listWidget = ui->listWidget;
    listWidget->addItem(new QListWidgetItem(QIcon(QObject::tr("images/china.png")),
                                            QObject::tr("China")));
    listWidget->addItem(new QListWidgetItem(QIcon(QObject::tr("images/hk.png")),
                                            QObject::tr("Hong Kong")));
    listWidget->addItem(new QListWidgetItem(QIcon(QObject::tr("images/macau.png")),
                                            QObject::tr("Macau")));
}

ToolList::~ToolList()
{
    delete ui;
}
