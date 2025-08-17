#ifndef MAINWINDOWTABWIDGET_H
#define MAINWINDOWTABWIDGET_H

#include <QTabWidget>
#include "customtabbar.h"

class MainWindowTabWidget : public QTabWidget
{
    Q_OBJECT

public:
    explicit MainWindowTabWidget(QWidget *parent = nullptr);

private:
    CustomTabBar *m_customTabBar;
};

#endif // MAINWINDOWTABWIDGET_H
