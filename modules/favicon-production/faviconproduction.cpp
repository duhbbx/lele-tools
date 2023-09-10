#include "faviconproduction.h"

#include <QLabel>


REGISTER_DYNAMICOBJECT(FaviconProduction);

FaviconProduction::FaviconProduction() : QWidget(nullptr), DynamicObjectBase()
{
    QVBoxLayout * layout = new QVBoxLayout;
    layout->setContentsMargins(0,0,0,0);
    QLabel * label = new QLabel;

    label->setText("待实现.....");

    layout->addWidget(label);
    layout->setAlignment(Qt::AlignTop);
    this->setLayout(layout);
}

