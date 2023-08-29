#include "jsonformatter.h"
#include "ui_jsonformatter.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>

#include <QTextEdit>

#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>

class jsonHighlighter : public QSyntaxHighlighter
{
public:
    jsonHighlighter(QTextDocument* parent = nullptr);

protected:
    void highlightBlock(const QString& text) override;

private:
    QTextCharFormat m_keyFormat;
    QTextCharFormat m_stringFormat;
    QTextCharFormat m_numberFormat;
    QTextCharFormat m_booleanFormat;
    QTextCharFormat m_nullFormat;
};


jsonHighlighter::jsonHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
    //自定义json各键值显示高亮颜色
    m_keyFormat.setForeground(Qt::darkBlue);
//    QFont f;
//    f.setFamily("Consolas");
//    f.setBold(true);
//    m_keyFormat.setFont(f);
    m_stringFormat.setForeground(Qt::darkGreen);
    m_numberFormat.setForeground(Qt::darkMagenta);
    m_booleanFormat.setForeground(Qt::darkYellow);
    m_nullFormat.setForeground(Qt::gray);
}

void jsonHighlighter::highlightBlock(const QString& text)
{
    //匹配值
    QRegularExpression regex_1("(\".*?\")|(-?\\d+\\.?\\d*)|(true|false)|(null)");
    int index_1 = 0;
    while (index_1 >= 0) {
        QRegularExpressionMatch match = regex_1.match(text, index_1);
        if (!match.hasMatch()) {
            break;
        }
        int start = match.capturedStart(1);
        int length = match.capturedLength(1);

        if (match.capturedStart(1) != -1) { // String
            setFormat(match.capturedStart(1), match.capturedLength(1), m_stringFormat);
        }
        else if (match.capturedStart(2) != -1) { // Number
            setFormat(match.capturedStart(2), match.capturedLength(2), m_numberFormat);
        }
        else if (match.capturedStart(3) != -1) { // Boolean
            setFormat(match.capturedStart(3), match.capturedLength(3), m_booleanFormat);
        }
        else if (match.capturedStart(4) != -1) { // Null
            setFormat(match.capturedStart(4), match.capturedLength(4), m_nullFormat);
        }
        index_1 = match.capturedEnd();
    }
    //匹配键
    QRegularExpression regex("(\"\\w+\"):\\s*");
    int index = 0;
    while (index >= 0) {
        QRegularExpressionMatch match = regex.match(text, index);
        if (!match.hasMatch()) {
            break;
        }
        int start = match.capturedStart(1);
        int length = match.capturedLength(1);
        setFormat(start, length, m_keyFormat);
        index = match.capturedEnd();
    }
}


JsonFormatter::JsonFormatter(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::JsonFormatter)
{
    ui->setupUi(this);

    // 创建字体对象
    QFont font;
    font.setFamily("Consolas"); // 设置字体家族
    font.setPointSize(12);   // 设置字号
    font.setBold(true);      // 设置粗体

    // 将字体应用于QPlainTextEdit
    ui->plainTextEdit->setFont(font);


    QObject::connect(ui->textEdit, SIGNAL(textChanged()), this, SLOT(onTextChanged()));

}

JsonFormatter::~JsonFormatter()
{
    delete ui;
}



void JsonFormatter::onTextChanged() {
    qDebug() << "准备格式化了";

    QString text = ui->textEdit->toPlainText();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(text.toUtf8());

    // 将QJsonDocument格式化为字符串
    QString formattedJson = QString::fromUtf8(jsonDoc.toJson(QJsonDocument::Indented));

    ui->plainTextEdit->setPlainText(formattedJson);

    jsonHighlighter* highlighter = new jsonHighlighter(ui->plainTextEdit->document());



    ui->plainTextEdit->setReadOnly(true);
}
