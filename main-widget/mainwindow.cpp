#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QLabel>
#include <QListWidget>
#include <QBoxLayout>
#include "../tool-list/toollist.h"
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);




    ui->stackedWidget->addWidget(new ToolList);

    ui->stackedWidget->setCurrentIndex(2);


}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_pushButton_clicked()
{

    qDebug() << "呵呵呵，回到首页吧~";
    ui->stackedWidget->setCurrentIndex(2);
}


void MainWindow::addTool(QWidget *w) {
    qDebug() << "准备................增加widget 到stacked widget中了..............\n";
    ui->stackedWidget->addWidget(w);

    qDebug() << "增加widget 到stacked widget中了..............\n";
    ui->stackedWidget->setCurrentIndex(3);
}
