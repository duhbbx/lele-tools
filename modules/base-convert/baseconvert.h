#ifndef BASECONVERT_H
#define BASECONVERT_H


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
#include <QRadioButton>
#include <QButtonGroup>

#include "../../common/dynamicobjectbase.h"


class BaseConvert  : public QWidget, public DynamicObjectBase {

    Q_OBJECT
public:
    explicit BaseConvert();

private:

    QRadioButton * radioButtonSource_2;
    QRadioButton * radioButtonSource_4;
    QRadioButton * radioButtonSource_8;
    QRadioButton * radioButtonSource_10;
    QRadioButton * radioButtonSource_16;

    QRadioButton * radioButtonTarget_2;
    QRadioButton * radioButtonTarget_4;
    QRadioButton * radioButtonTarget_8;
    QRadioButton * radioButtonTarget_10;
    QRadioButton * radioButtonTarget_16;

    QButtonGroup *buttonGroupSource;

    QButtonGroup *buttonGroupTarget;
};

#endif // BASECONVERT_H
