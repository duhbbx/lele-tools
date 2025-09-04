#ifndef TOOLLIST_H
#define TOOLLIST_H

#include <QLineEdit>
#include "../main-widget/mainwindow.h"

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
};

#endif // TOOLLIST_H
