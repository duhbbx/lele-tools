
#ifndef DATETIMECALCULATOR_H
#define DATETIMECALCULATOR_H


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


class DateTimeCalculator : public QWidget, public DynamicObjectBase {

    Q_OBJECT
public:
    explicit DateTimeCalculator();

private:


public slots:
};

#endif // DATETIMECALCULATOR_H
