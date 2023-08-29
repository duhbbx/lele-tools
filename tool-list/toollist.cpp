#include "toollist.h"

#include <QTextEdit>
#include "../main-widget/mainwindow.h"
#include "../modules/json-formatter/jsonformatter.h"
#include <QListWidget>

#include <QLineEdit>
ToolList::ToolList(QWidget *parent) : QWidget(parent)
{


    QVBoxLayout *layout = new QVBoxLayout(this);

    QWidget * wrap = new QWidget;

    QHBoxLayout *searchLayout = new QHBoxLayout();

    searchLayout->setAlignment(Qt::AlignLeft);

    searchLayout->setContentsMargins(0, 0, 0, 0);


    QLineEdit * lineEdit = new QLineEdit;
    lineEdit->setFixedSize(300, 25);

    QPushButton * button = new QPushButton("搜索");

    button->setFixedSize(100, 25);

    searchLayout->addWidget(lineEdit);
    searchLayout->addWidget(button);

    wrap->setLayout(searchLayout);

    layout->addWidget(wrap);



    QListWidget *listWidget = new QListWidget;
    listWidget->addItem(new QListWidgetItem(QIcon(QObject::tr("images/china.png")), QObject::tr("China")));
    listWidget->addItem(new QListWidgetItem(QIcon(QObject::tr("images/hk.png")), QObject::tr("Hong Kong")));
    listWidget->addItem(new QListWidgetItem(QIcon(QObject::tr("images/macau.png")), QObject::tr("Macau")));


    layout->addWidget(listWidget);

    connect(listWidget, &QListWidget::pressed, this, [=](QModelIndex pos) {
        qDebug() << "m_ListWidget pos.row:" << pos.row();

        switch (pos.row()) {
        case 0:
            // json 格式化
            this->mainWindow->addTool(new JsonFormatter);
            this->mainWindow->setWindowTitle("JSON格式化");
            break;
        }
    });

    this->setLayout(layout);
}

void ToolList::setMainWindow(MainWindow* mainWindow) {
    this->mainWindow = mainWindow;
}

ToolList::~ToolList()
{

}
