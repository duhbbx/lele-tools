#include "toollist.h"

#include <QTextEdit>
#include "../main-widget/mainwindow.h"
#include <QListWidget>
#include <QVBoxLayout>
#include <QLineEdit>

#include "module-meta.h"
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
    lineEdit->setFixedSize(300, 30);

    QPushButton * button = new QPushButton("搜索");

    button->setFixedSize(100, 30);

    searchLayout->addWidget(lineEdit);
    searchLayout->addWidget(button);

    wrap->setLayout(searchLayout);

    layout->addWidget(wrap);



    QListWidget *listWidget = new QListWidget;

    for (const ModuleMeta &moduleMeta : moduleMetaArray) {

        QListWidgetItem * item0 = new QListWidgetItem(QIcon(moduleMeta.icon), moduleMeta.title);
        item0->setData(Qt::UserRole, QVariant(moduleMeta.className));
        item0->setData(Qt::DisplayRole, QVariant(moduleMeta.title));
        listWidget->addItem(item0);
    }


    layout->addWidget(listWidget);

    QObject::connect(listWidget, &QListWidget::itemClicked, mainWindow, &MainWindow::itemClickedSlot);

    this->setLayout(layout);
}

ToolList::~ToolList()
{

}
