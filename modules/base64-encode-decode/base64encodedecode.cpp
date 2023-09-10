#include "base64encodedecode.h"

#include <QLabel>
#include <QPushButton>

#include <QPlainTextEdit>
#include <QByteArray>

REGISTER_DYNAMICOBJECT(Base64EncodeDecode);

Base64EncodeDecode::Base64EncodeDecode() : QWidget(nullptr), DynamicObjectBase()
{
    QVBoxLayout * layout = new QVBoxLayout;
    layout->setContentsMargins(0,0,0,0);
    layout->setAlignment(Qt::AlignTop);

    QHBoxLayout * buttons = new QHBoxLayout;

    QPushButton * encodeButton = new QPushButton("编码");
    encodeButton->setFixedSize(60, 30);

    QPushButton * decodeButton = new QPushButton("解码");
    decodeButton->setFixedSize(60, 30);

    buttons->addWidget(encodeButton);
    buttons->addWidget(decodeButton);

    buttons->setAlignment(Qt::AlignLeft);

    QObject::connect(encodeButton, &QPushButton::clicked, this, &Base64EncodeDecode::encode);
    QObject::connect(decodeButton, &QPushButton::clicked, this, &Base64EncodeDecode::decode);

    layout->addLayout(buttons);

    QPlainTextEdit * source = new QPlainTextEdit;

    QPlainTextEdit * target = new QPlainTextEdit;
    this->source = source;
    this->target = target;

    layout->addWidget(source);
    layout->addWidget(target);





    this->setLayout(layout);
}


void Base64EncodeDecode::encode() {
    QString originalText = this->source->toPlainText();
    QByteArray originalData = originalText.toUtf8();

    // 进行Base64编码
    QByteArray encodedData = originalData.toBase64();

    QString encodedString = QString(encodedData);

    this->target->setPlainText(encodedString);

    qDebug() << "Original Text: " << originalText;
    qDebug() << "Base64 Encoded String: " << encodedString;
}

void Base64EncodeDecode::decode() {

    QString encodedString = this->source->toPlainText();

    // 将Base64编码的字符串转换为QByteArray
    QByteArray encodedData = encodedString.toUtf8();

    // 进行Base64解码
    QByteArray decodedData = QByteArray::fromBase64(encodedData);

    QString originalText = QString::fromUtf8(decodedData);

    this->target->setPlainText(originalText);

    qDebug() << "Base64 Encoded String: " << encodedString;
    qDebug() << "Original Text: " << originalText;
}
