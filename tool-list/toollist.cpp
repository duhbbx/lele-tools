#include "toollist.h"
#include "ui_toollist.h"

#include "../main-widget/mainwindow.h"
#include "../modules/json-formatter/jsonformatter.h"

ToolList::ToolList(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ToolList)
{
    ui->setupUi(this);

    QListWidget *listWidget = ui->listWidget;
    listWidget->addItem(new QListWidgetItem(QIcon(QObject::tr("images/china.png")), QObject::tr("China")));
    listWidget->addItem(new QListWidgetItem(QIcon(QObject::tr("images/hk.png")), QObject::tr("Hong Kong")));
    listWidget->addItem(new QListWidgetItem(QIcon(QObject::tr("images/macau.png")), QObject::tr("Macau")));

    connect(ui->listWidget, &QListWidget::pressed, [=](QModelIndex pos) {
        qDebug() << "m_ListWidget pos.row:" << pos.row();

        switch (pos.row()) {
        case 0:
            // json 格式化
            MainWindow* mainWindow = (MainWindow *)parent;
            mainWindow->addTool(new JsonFormatter);
            break;
        }
    });
}

ToolList::~ToolList()
{
    delete ui;
}
