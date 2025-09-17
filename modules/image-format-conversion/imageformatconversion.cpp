#include "imageformatconversion.h"

#include <QLabel>


REGISTER_DYNAMICOBJECT(ImageFormatConversion);

ImageFormatConversion::ImageFormatConversion() : QWidget(nullptr), DynamicObjectBase()
{
    QVBoxLayout * layout = new QVBoxLayout;
    layout->setContentsMargins(0,0,0,0);
    QLabel * label = new QLabel;

    label->setText(tr("待实现....."));

    layout->addWidget(label);
    layout->setAlignment(Qt::AlignTop);
    this->setLayout(layout);
}

