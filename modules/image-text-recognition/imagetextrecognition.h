
#ifndef IMAGETEXTRECOGNITION_H
#define IMAGETEXTRECOGNITION_H


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


class ImageTextRecognition : public QWidget, public DynamicObjectBase {

    Q_OBJECT
public:
    explicit ImageTextRecognition();

private:


public slots:
};

#endif // IMAGETEXTRECOGNITION_H
