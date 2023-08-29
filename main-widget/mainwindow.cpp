#include "mainwindow.h"
#include <QLabel>
#include <QListWidget>
#include <QBoxLayout>
#include "../tool-list/toollist.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{

    QWidget* centerWidget = new QWidget(this);



    QVBoxLayout *layout = new QVBoxLayout(this);

    QPushButton *button = new QPushButton("点击我");

    button->setFixedSize(100, 25);

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(button);


    this->resize(1200, 800);

    this->pushButton = button;

    QStackedWidget * stackedWidget = new QStackedWidget;


    stackedWidget->setContentsMargins(0,0,0,0);


    this->stackedWidget = stackedWidget;

    layout->addWidget(this->stackedWidget);

    ToolList * toolList = new ToolList;
    toolList->setMainWindow(this);
    stackedWidget->addWidget(toolList);

    stackedWidget->setCurrentIndex(2);

    centerWidget->setLayout(layout);

    this->setCentralWidget(centerWidget);
}

MainWindow::~MainWindow()
{

}


void MainWindow::on_pushButton_clicked()
{

    qDebug() << "呵呵呵，回到首页吧~";
    this->stackedWidget->setCurrentIndex(2);
}


void MainWindow::addTool(QWidget *w) {
    qDebug() << "准备................增加widget 到stacked widget中了..............\n";
    this->stackedWidget->addWidget(w);

    qDebug() << "增加widget 到stacked widget中了..............\n";
    this->stackedWidget->setCurrentIndex(3);
}
