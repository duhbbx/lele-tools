#ifndef YAMLFORMATTER_H
#define YAMLFORMATTER_H





#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>

#include <QWidget>

#include <QVBoxLayout>
#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QTextEdit>
#include <QPlainTextEdit>

#include "../../common/dynamicobjectbase.h"

class YamlFormatter : public QWidget, public DynamicObjectBase
{
    Q_OBJECT
public:
    explicit YamlFormatter();

public slots:
    void onTextChanged();

private:
    QTextEdit * textEdit;
    QPlainTextEdit * plainTextEdit;
};

#endif // YAMLFORMATTER_H


