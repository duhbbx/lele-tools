#include "YamlFormatter.h"


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
#include "../../common/thirdparty/pugixml/pugixml.hpp"
#include <string>
#include <iostream>
#include <sstream>


#include "../../common/dynamicobjectbase.h"

#include "../../common/thirdparty/yaml-cpp/include/yaml-cpp/yaml.h"


REGISTER_DYNAMICOBJECT(YamlFormatter);

YamlFormatter::YamlFormatter(): QWidget(nullptr), DynamicObjectBase() {


    YAML::Node primes = YAML::Load("[2, 3, 5, 7, 11]");
    for (std::size_t i=0;i<primes.size();i++) {
        qDebug() << primes[i].as<int>();
    }
    // or:
    for (YAML::const_iterator it=primes.begin();it!=primes.end();++it) {
        qDebug() << it->as<int>();
    }

    primes.push_back(13);
    assert(primes.size() == 6);



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


    textEdit->setFont(font);

    //当前行颜色设置 this派生自QTextEdit
    connect(textEdit, &QTextEdit::cursorPositionChanged, textEdit, [textEdit](){
        QList<QTextEdit::ExtraSelection> extra_selections;

        QTextEdit::ExtraSelection line;
        line.format.setBackground(QColor(255, 250, 205));
        line.format.setProperty(QTextFormat::FullWidthSelection, true);
        line.cursor = textEdit->textCursor();
        line.cursor.clearSelection();
        extra_selections.append(line);

        textEdit->setExtraSelections(extra_selections);
    });


}

QString formatYaml(const QString &xmlString)
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


void YamlFormatter::onTextChanged() {
    qDebug() << "准备格式化了";

    QString xml = this->textEdit->toPlainText();
    QString formattedXml = formatYaml(xml);

    plainTextEdit->setPlainText(formattedXml);
    this->plainTextEdit->setReadOnly(true);
}
