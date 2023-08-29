#ifndef TOOLLIST_H
#define TOOLLIST_H

#include <QWidget>
#include "../main-widget/mainwindow.h"


class ToolList : public QWidget
{
    Q_OBJECT

public:
    explicit ToolList(QWidget *parent = nullptr);
    ~ToolList();
    void setMainWindow(MainWindow* mainWindow);

private:
    MainWindow* mainWindow;
};

#endif // TOOLLIST_H
