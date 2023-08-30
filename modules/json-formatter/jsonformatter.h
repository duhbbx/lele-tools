#ifndef JSONFORMATTER_H
#define JSONFORMATTER_H

#include <QWidget>

#include <QVBoxLayout>
#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QTextEdit>
#include <QPlainTextEdit>

#include "../../common/dynamicobjectbase.h"


class JsonFormatter : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit JsonFormatter();
    ~JsonFormatter();

public slots:
    void onTextChanged();

private:
    QTextEdit * textEdit;
    QPlainTextEdit * plainTextEdit;

};

#endif // JSONFORMATTER_H
