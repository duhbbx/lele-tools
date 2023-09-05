
#ifndef DATETIMEUTIL_H
#define DATETIMEUTIL_H


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
#include <QTimer>
#include <QButtonGroup>

#include <QLabel>
#include "../../common/dynamicobjectbase.h"

#include <QComboBox>

class DateTimeUtil : public QWidget, public DynamicObjectBase {

    Q_OBJECT
public:
    explicit DateTimeUtil();

private:

    int seconds;
    QLabel * timeLabel;
    QTimer * timer;
    QComboBox * timestampToDateOption;
    QComboBox * dateToTimestampOption;
    QLineEdit * dateDisplay;
    QLineEdit * timestampDisplay;
    QLineEdit * dateInput;


public slots:
    void updateTime();
    void timestampTextChangedSlot(const QString&);
    void dateTextChangedSlot(const QString&);
};

#endif // DATETIMEUTIL_H
