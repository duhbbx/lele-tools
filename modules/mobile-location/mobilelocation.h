
#ifndef MOBILELOCATION_H
#define MOBILELOCATION_H


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


class MobileLocation : public QWidget, public DynamicObjectBase {

    Q_OBJECT
public:
    explicit MobileLocation();

private:


public slots:
};

#endif // MOBILELOCATION_H
