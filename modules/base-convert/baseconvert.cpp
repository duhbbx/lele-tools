#include "baseconvert.h"

#include <QLineEdit>


REGISTER_DYNAMICOBJECT(BaseConvert);

BaseConvert::BaseConvert() : QWidget(nullptr), DynamicObjectBase()
{
    QVBoxLayout * layout = new QVBoxLayout;
    layout->setContentsMargins(0,0,0,0);
    QRadioButton * radioButtonSource_2 = new QRadioButton(this);
    radioButtonSource_2->setText(tr("2进制"));
    radioButtonSource_2->setObjectName("2");
    QRadioButton * radioButtonSource_4 = new QRadioButton(this);
    radioButtonSource_4->setText(tr("4进制"));
    radioButtonSource_4->setObjectName("4");
    QRadioButton * radioButtonSource_8 = new QRadioButton(this);
    radioButtonSource_8->setText(tr("8进制"));
    radioButtonSource_8->setObjectName("8");
    QRadioButton * radioButtonSource_10 = new QRadioButton(this);
    radioButtonSource_10->setText(tr("10进制"));
    radioButtonSource_10->setObjectName("10");
    QRadioButton * radioButtonSource_16 = new QRadioButton(this);
    radioButtonSource_16->setText(tr("16进制"));
    radioButtonSource_16->setObjectName("16");

    QButtonGroup *buttonGroupSource = new QButtonGroup(this);

    buttonGroupSource->addButton(radioButtonSource_2);
    buttonGroupSource->addButton(radioButtonSource_4);
    buttonGroupSource->addButton(radioButtonSource_8);
    buttonGroupSource->addButton(radioButtonSource_10);
    buttonGroupSource->addButton(radioButtonSource_16);

    QRadioButton * radioButtonTarget_2 = new QRadioButton(this);
    radioButtonTarget_2->setText(tr("2进制"));
    radioButtonTarget_2->setObjectName("2");
    QRadioButton * radioButtonTarget_4 = new QRadioButton(this);
    radioButtonTarget_4->setText(tr("4进制"));
    radioButtonTarget_4->setObjectName("4");
    QRadioButton * radioButtonTarget_8 = new QRadioButton(this);
    radioButtonTarget_8->setText(tr("8进制"));
    radioButtonTarget_8->setObjectName("8");
    QRadioButton * radioButtonTarget_10 = new QRadioButton(this);
    radioButtonTarget_10->setText(tr("10进制"));
    radioButtonTarget_10->setObjectName("10");
    QRadioButton * radioButtonTarget_16 = new QRadioButton(this);
    radioButtonTarget_16->setText(tr("16进制"));
    radioButtonTarget_16->setObjectName("16");


    radioButtonTarget_2->setFixedSize(100, 30);
    radioButtonTarget_4->setFixedSize(100, 30);
    radioButtonTarget_8->setFixedSize(100, 30);
    radioButtonTarget_10->setFixedSize(100, 30);
    radioButtonTarget_16->setFixedSize(100, 30);

    radioButtonSource_2->setFixedSize(100, 30);
    radioButtonSource_4->setFixedSize(100, 30);
    radioButtonSource_8->setFixedSize(100, 30);
    radioButtonSource_10->setFixedSize(100, 30);
    radioButtonSource_16->setFixedSize(100, 30);


    QButtonGroup *buttonGroupTarget = new QButtonGroup(this);

    buttonGroupTarget->addButton(radioButtonTarget_2);
    buttonGroupTarget->addButton(radioButtonTarget_4);
    buttonGroupTarget->addButton(radioButtonTarget_8);
    buttonGroupTarget->addButton(radioButtonTarget_10);
    buttonGroupTarget->addButton(radioButtonTarget_16);


    QWidget *containerWidgetSource = new QWidget(this); // 创建一个QWidget容器

    QHBoxLayout * layoutSource = new QHBoxLayout(this);



    layoutSource->addWidget(radioButtonSource_2);
    layoutSource->addWidget(radioButtonSource_4);
    layoutSource->addWidget(radioButtonSource_8);
    layoutSource->addWidget(radioButtonSource_10);
    layoutSource->addWidget(radioButtonSource_16);

    containerWidgetSource->setLayout(layoutSource);

    QWidget *containerWidgetTarget = new QWidget(this); // 创建一个QWidget容器

    QHBoxLayout * layoutTarget = new QHBoxLayout(this);
    layoutTarget->addWidget(radioButtonTarget_2);
    layoutTarget->addWidget(radioButtonTarget_4);
    layoutTarget->addWidget(radioButtonTarget_8);
    layoutTarget->addWidget(radioButtonTarget_10);
    layoutTarget->addWidget(radioButtonTarget_16);

    layoutSource->setAlignment(Qt::AlignLeft);
    layoutTarget->setAlignment(Qt::AlignLeft);

    containerWidgetTarget->setLayout(layoutTarget);

    containerWidgetSource->setFixedHeight(30);

//    containerWidgetTarget->setStyleSheet("border: 1px solid red;");

    containerWidgetTarget->setFixedHeight(30);

//    containerWidgetSource->setStyleSheet("border: 1px solid red;");
    QLineEdit * lineEditSource = new QLineEdit;
    QLineEdit * lineEditTarget = new QLineEdit;

    layout->addWidget(containerWidgetSource);
    layout->addWidget(lineEditSource);
    layout->addWidget(containerWidgetTarget);
    layout->addWidget(lineEditTarget);

    this->setFixedHeight(200);

    this->setLayout(layout);


}
