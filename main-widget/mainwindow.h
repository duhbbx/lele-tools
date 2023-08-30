#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QPushButton>
#include <QListWidgetItem>


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void on_pushButton_clicked();
    void itemClickedSlot(QListWidgetItem *item);

private:
    QStackedWidget * stackedWidget;
    QPushButton * pushButton;

public:
    void addTool(QWidget* w);
};
#endif // MAINWINDOW_H
