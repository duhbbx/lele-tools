#include "textencryptionanddecryption.h"

#include <QLabel>


REGISTER_DYNAMICOBJECT(TextEncryptionAndDecryption);

TextEncryptionAndDecryption::TextEncryptionAndDecryption() : QWidget(nullptr), DynamicObjectBase()
{
    QVBoxLayout * layout = new QVBoxLayout;

    QLabel * label = new QLabel;

    label->setText(tr("待实现....."));

    layout->addWidget(label);
    layout->setAlignment(Qt::AlignTop);
    this->setLayout(layout);
}

