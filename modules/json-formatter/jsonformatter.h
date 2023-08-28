#ifndef JSONFORMATTER_H
#define JSONFORMATTER_H

#include <QWidget>

namespace Ui {
class JsonFormatter;
}

class JsonFormatter : public QWidget
{
    Q_OBJECT

public:
    explicit JsonFormatter(QWidget *parent = nullptr);
    ~JsonFormatter();

private:
    Ui::JsonFormatter *ui;
};

#endif // JSONFORMATTER_H
