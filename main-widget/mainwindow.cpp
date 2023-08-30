#include "mainwindow.h"
#include <QLabel>
#include <QListWidget>
#include <QBoxLayout>
#include "../tool-list/toollist.h"
#include <QObject>
#include "../common/dynamicobjectbase.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{

    QWidget* centerWidget = new QWidget(this);

    this->setWindowTitle("乐乐的工具箱");

    QVBoxLayout *layout = new QVBoxLayout();

    QPushButton *button = new QPushButton("返回首页");

    // connect(ui->pushButton, &QPushButton::clicked, this, &MainWindow::on_pushButton_clicked);


    QObject::connect(button, &QPushButton::clicked, this, &MainWindow::on_pushButton_clicked);

    button->setFixedSize(100, 25);

    layout->setContentsMargins(5, 5, 5, 5);
    layout->setSpacing(5);

    layout->addWidget(button);

    layout->setAlignment(Qt::AlignLeft);

    this->resize(1200, 800);

    this->pushButton = button;

    QStackedWidget * stackedWidget = new QStackedWidget;


    stackedWidget->setContentsMargins(0,0,0,0);


    qDebug() << "MainWindow...............";




    this->stackedWidget = stackedWidget;

    layout->addWidget(this->stackedWidget);


    qDebug() << "MainWindow############################################";

    ToolList * toolList = new ToolList(this, nullptr);

    stackedWidget->addWidget(toolList);
    stackedWidget->setCurrentIndex(2);

    centerWidget->setLayout(layout);

    this->setCentralWidget(centerWidget);

    qDebug() << "MainWindow<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<";

}

MainWindow::~MainWindow()
{

}

void MainWindow::itemClickedSlot(QListWidgetItem *item) {
    qDebug() << "Item Clicked: " << item->text();

    QString stringValue = item->data(Qt::UserRole).toString();


    DynamicObjectBase* object = DynamicObjectFactory::Instance()->CreateObject(stringValue.toStdString());

    QWidget * widget = dynamic_cast<QWidget *>(object); // 显式将double转换为int




    qDebug() << "准备................增加widget 到stacked widget中了..............\n";
    this->stackedWidget->addWidget(widget);

    qDebug() << "增加widget 到stacked widget中了..............\n";
    this->stackedWidget->setCurrentIndex(1);
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
