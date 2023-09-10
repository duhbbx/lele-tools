
#ifndef RANDOMPASSWORDGEN_H
#define RANDOMPASSWORDGEN_H


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
#include <QPushButton>
#include "../../common/dynamicobjectbase.h"
#include <QCheckBox>
#include <QLineEdit>
class RandomPasswordGen : public QWidget, public DynamicObjectBase {

    Q_OBJECT
public:
    explicit RandomPasswordGen();

private:
    QCheckBox * digit;
    QCheckBox * lowerCase;
    QCheckBox * upperCase;
    QCheckBox * specialChar;
    QLineEdit * customerizedCharInput;
    QLineEdit * numberInput;
    QPlainTextEdit * content;

    QLineEdit * minPasswordLengthInput;
    QLineEdit * maxPasswordLengthInput;

public slots:
    void passwordGenerate();
};

#endif // RANDOMPASSWORDGEN_H
