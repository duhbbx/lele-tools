#include "toollist.h"

#include <QTextEdit>
#include "../main-widget/mainwindow.h"
#include <QListWidget>
#include <QVBoxLayout>
#include <QLineEdit>
ToolList::ToolList(MainWindow* mainWindow, QWidget *parent) : QWidget(parent)
{

    this->mainWindow = mainWindow;


    QVBoxLayout *layout = new QVBoxLayout(this);

    layout->setContentsMargins(0,0,0,0);

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

    QListWidgetItem * item1 = new QListWidgetItem(QIcon(QObject::tr("images/china.png")), QObject::tr("JSON格式化"));
    item1->setData(Qt::UserRole, QVariant("JsonFormatter"));
    listWidget->addItem(item1);


    QListWidgetItem * item2 = new QListWidgetItem(QIcon(QObject::tr("images/hk.png")), QObject::tr("XML格式化"));
    item2->setData(Qt::UserRole, QVariant("XmlFormatter"));
    listWidget->addItem(item2);

    qDebug() << "ToolList############################################";

    QListWidgetItem * item3 = new QListWidgetItem(QIcon(QObject::tr("images/macau.png")), QObject::tr("YAML格式化"));
    item3->setData(Qt::UserRole, QVariant("JsonFormatter"));
    listWidget->addItem(item3);


    layout->addWidget(listWidget);

    QObject::connect(listWidget, &QListWidget::itemClicked, mainWindow, &MainWindow::itemClickedSlot);

    this->setLayout(layout);
}

ToolList::~ToolList()
{

}
