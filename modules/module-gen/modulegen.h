#ifndef MODULEGEN_H
#define MODULEGEN_H


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
#include <QLineEdit>

#include "../../common/dynamicobjectbase.h"


class ModuleGen  : public QWidget, public DynamicObjectBase {

    Q_OBJECT
public:
    explicit ModuleGen();

private:
    QLineEdit * dirLineEdit;
    QLineEdit * classNameEdit;
    QLineEdit * titleEdit;
public slots:
    void onButtonClicked();
    void gen();
};

#endif // MODULEGEN_H
