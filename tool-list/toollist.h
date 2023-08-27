#ifndef TOOLLIST_H
#define TOOLLIST_H

#include <QWidget>

namespace Ui {
class ToolList;
}

class ToolList : public QWidget
{
    Q_OBJECT

public:
    explicit ToolList(QWidget *parent = nullptr);
    ~ToolList();

private:
    Ui::ToolList *ui;
};

#endif // TOOLLIST_H
