#ifndef JSONFORMATTER_H
#define JSONFORMATTER_H

#include <QWidget>

#include <QVBoxLayout>
#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QTextCharFormat>


namespace Ui {
class JsonFormatter;
}


class JsonFormatter : public QWidget
{
    Q_OBJECT

public:
    explicit JsonFormatter(QWidget *parent = nullptr);
    ~JsonFormatter();

public slots:
    void onTextChanged();

private:
    Ui::JsonFormatter *ui;
};

#endif // JSONFORMATTER_H
