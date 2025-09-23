#ifndef TOOLLIST_H
#define TOOLLIST_H

#include <QLineEdit>
#include <QListWidget>
#include "../main-widget/mainwindow.h"
#include "../common/toolusagetracker.h"

class ToolList final : public QWidget {
    Q_OBJECT

public:
    explicit ToolList(MainWindow* mainWindow, QWidget* parent = nullptr);
    ~ToolList() override;

private slots:
    void filterTools(const QString& text) const;

private:
    MainWindow* mainWindow;
    QLineEdit* searchLineEdit;
    QListWidget* listWidget;

    void setupSearchFunctionality();
    void sortToolsByUsage() const; // 按使用频率排序工具
};

#endif // TOOLLIST_H
