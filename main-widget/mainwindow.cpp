#include "mainwindow.h"
#include <QLabel>
#include <QListWidget>
#include <QBoxLayout>
#include "../tool-list/toollist.h"
#include <QObject>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{

    QWidget* centerWidget = new QWidget(this);

    this->setWindowTitle("乐乐的工具箱");

    QVBoxLayout *layout = new QVBoxLayout();

    QPushButton *button = new QPushButton("返回首页");

    QObject::connect(button, SIGNAL(clicked()), this, SLOT(on_pushButton_clicked()));

    button->setFixedSize(100, 25);


    layout->setContentsMargins(5, 5, 5, 5);
    layout->setSpacing(5);

    layout->addWidget(button);

    layout->setAlignment(Qt::AlignLeft);


    this->resize(1200, 800);

    this->pushButton = button;

    QStackedWidget * stackedWidget = new QStackedWidget;


    stackedWidget->setContentsMargins(0,0,0,0);






    this->stackedWidget = stackedWidget;

    layout->addWidget(this->stackedWidget);

    ToolList * toolList = new ToolList;
//    toolList->setStyleSheet("border: 1px solid red;");
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

    // 假设你有一个名为 stackedWidget 的 QStackedWidget
    int currentIndex = stackedWidget->currentIndex();  // 获取当前显示的页面的索引

    if (currentIndex > 0) {
        QWidget *currentWidget = stackedWidget->widget(currentIndex);  // 获取当前显示的页面
        stackedWidget->removeWidget(currentWidget);  // 从 stackedWidget 中移除当前页面
        delete currentWidget;  // 手动销毁该页面
    }

    this->setWindowTitle(tr("乐乐的工具箱"));
}


void MainWindow::addTool(QWidget *w) {
    qDebug() << "准备................增加widget 到stacked widget中了..............\n";
    this->stackedWidget->addWidget(w);

    qDebug() << "增加widget 到stacked widget中了..............\n";
    this->stackedWidget->setCurrentIndex(1);
}
