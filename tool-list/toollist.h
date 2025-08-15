#ifndef TOOLLIST_H
#define TOOLLIST_H

#include <QWidget>
#include <QLineEdit>
#include <QListWidget>
#include "../main-widget/mainwindow.h"






class ToolList : public QWidget
{
    Q_OBJECT

public:
    explicit ToolList(MainWindow * mainWindow, QWidget *parent = nullptr);
    ~ToolList();

private slots:
    void filterTools(const QString &text);

private:
    MainWindow* mainWindow;
    QLineEdit* searchLineEdit;
    QListWidget* listWidget;
    
    void setupSearchFunctionality();
};

#endif // TOOLLIST_H
