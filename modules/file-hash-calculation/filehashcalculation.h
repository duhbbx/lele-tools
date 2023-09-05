
#ifndef FILEHASHCALCULATION_H
#define FILEHASHCALCULATION_H


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


class FileHashCalculation : public QWidget, public DynamicObjectBase {

    Q_OBJECT
public:
    explicit FileHashCalculation();

private:


public slots:
};

#endif // FILEHASHCALCULATION_H
