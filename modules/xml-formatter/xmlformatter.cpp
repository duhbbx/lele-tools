#include "xmlformatter.h"


#include <QWidget>

#include <QVBoxLayout>
#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QXmlStreamReader>
#include <QString>
#include <QTextStream>
#include "lib/pugixml.hpp"
#include <string>
#include <iostream>
#include <sstream>


#include "../../common/dynamicobjectbase.h"

REGISTER_DYNAMICOBJECT(XmlFormatter);

XmlFormatter::XmlFormatter(): QWidget(nullptr), DynamicObjectBase() {


    QHBoxLayout * layout = new QHBoxLayout;
    QTextEdit * textEdit = new QTextEdit;
    QPlainTextEdit * plainTextEdit = new QPlainTextEdit;


    layout->addWidget(textEdit);

    layout->addWidget(plainTextEdit);

    this->textEdit = textEdit;
    this->plainTextEdit = plainTextEdit;


    this->setLayout(layout);

    // 创建字体对象
    QFont font;
    font.setFamily("Consolas"); // 设置字体家族
    font.setPointSize(10);   // 设置字号
    font.setBold(true);      // 设置粗体

    // 将字体应用于QPlainTextEdit
    this->plainTextEdit->setFont(font);


    QObject::connect(this->textEdit, SIGNAL(textChanged()), this, SLOT(onTextChanged()));

}

QString formatXml(const QString &xmlString)
{

    pugi::xml_document doc;
    doc.load_string(xmlString.toStdString().c_str());

    // 创建一个 std::ostringstream 对象
    std::ostringstream oss;

    doc.save(oss, "  ");

    // 从 oss 中获取一个 std::string
    std::string result = oss.str();

    QString qstr = QString::fromStdString(result);

    return qstr;
}


void XmlFormatter::onTextChanged() {
    qDebug() << "准备格式化了";

    QString xml = this->textEdit->toPlainText();
    QString formattedXml = formatXml(xml);

    plainTextEdit->setPlainText(formattedXml);
    this->plainTextEdit->setReadOnly(true);
}
