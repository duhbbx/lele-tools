#ifndef TOOLLIST_H
#define TOOLLIST_H

#include <QWidget>
#include "../main-widget/mainwindow.h"


class ToolList : public QWidget
{
    Q_OBJECT

public:
    explicit ToolList(MainWindow * mainWindow, QWidget *parent = nullptr);
    ~ToolList();

private:
    MainWindow* mainWindow;
};

#endif // TOOLLIST_H
