#include "imagebase64encoding.h"

#include <QLabel>


REGISTER_DYNAMICOBJECT(ImageBase64Encoding);

ImageBase64Encoding::ImageBase64Encoding() : QWidget(nullptr), DynamicObjectBase()
{
    QVBoxLayout * layout = new QVBoxLayout;

    QLabel * label = new QLabel;

    label->setText("待实现.....");

    layout->addWidget(label);
    layout->setAlignment(Qt::AlignTop);
    this->setLayout(layout);
}

