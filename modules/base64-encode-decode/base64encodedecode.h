
#ifndef BASE64ENCODEDECODE_H
#define BASE64ENCODEDECODE_H


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
#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>

#include "../../common/dynamicobjectbase.h"


class Base64EncodeDecode : public QWidget, public DynamicObjectBase {

    Q_OBJECT
public:
    explicit Base64EncodeDecode();

private:

    QPlainTextEdit * source;
    QPlainTextEdit * target;

public slots:

    void encode();
    void decode();
};

#endif // BASE64ENCODEDECODE_H
