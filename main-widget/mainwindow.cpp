#include "mainwindow.h"
#include <QLabel>
#include <QListWidget>
#include <QBoxLayout>
#include "../tool-list/toollist.h"
#include <QObject>
#include "../common/dynamicobjectbase.h"

#include <QApplication>
#include <QScreen>
#include <QMessageBox>


MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{

    QWidget* centerWidget = new QWidget(this);

    this->setWindowTitle("乐乐的工具箱");

    QVBoxLayout *layout = new QVBoxLayout();

    QPushButton *button = new QPushButton("返回首页");

    // connect(ui->pushButton, &QPushButton::clicked, this, &MainWindow::on_pushButton_clicked);


    QObject::connect(button, &QPushButton::clicked, this, &MainWindow::on_pushButton_clicked);

    button->setFixedSize(100, 30);

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

    QString stringValue = item->data(Qt::UserRole).toString();

    QString title = item->data(Qt::DisplayRole).toString();

    DynamicObjectBase* object = DynamicObjectFactory::Instance()->CreateObject(stringValue.toStdString());

    if (!object) {
        // 创建一个错误提示对话框
        QMessageBox errorBox;
        errorBox.setIcon(QMessageBox::Critical); // 设置图标为错误图标
        errorBox.setWindowTitle("错误提示");
        errorBox.setText("对应的模块还未实现");
        errorBox.setStandardButtons(QMessageBox::Ok);

        // 弹出错误提示对话框
        errorBox.exec();

        return;
    }

    // 显式将double转换为int
    QWidget * widget = dynamic_cast<QWidget *>(object);
    this->stackedWidget->addWidget(widget);
    this->stackedWidget->setCurrentIndex(1);
    this->setWindowTitle(title);
}


void MainWindow::on_pushButton_clicked() {

    // 获取当前显示的页面的索引
    int currentIndex = stackedWidget->currentIndex();

    if (currentIndex > 0) {
        // 获取当前显示的页面
        QWidget *currentWidget = stackedWidget->widget(currentIndex);
        stackedWidget->removeWidget(currentWidget);  // 从 stackedWidget 中移除当前页面
        delete currentWidget;  // 手动销毁该页面
    }

    this->setWindowTitle(tr("乐乐的工具箱"));
}
