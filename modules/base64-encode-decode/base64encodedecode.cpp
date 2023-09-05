#include "base64encodedecode.h"

#include <QLabel>


REGISTER_DYNAMICOBJECT(Base64EncodeDecode);

Base64EncodeDecode::Base64EncodeDecode() : QWidget(nullptr), DynamicObjectBase()
{
    QVBoxLayout * layout = new QVBoxLayout;

    QLabel * label = new QLabel;

    label->setText("待实现.....");

    layout->addWidget(label);
    layout->setAlignment(Qt::AlignTop);
    this->setLayout(layout);
}

