#include "randompasswordgen.h"
#include "qpushbutton.h"

#include <QLabel>
#include <QCheckBox>
#include <QLineEdit>
#include <QIntValidator>

#include "../../common/validator/myintvalidator.h"
REGISTER_DYNAMICOBJECT(RandomPasswordGen);

RandomPasswordGen::RandomPasswordGen() : QWidget(nullptr), DynamicObjectBase()
{
    QVBoxLayout * layout = new QVBoxLayout;
    layout->setContentsMargins(0,0,0,0);


    QCheckBox *digit = new QCheckBox("0-9");
    QCheckBox *lowerCase = new QCheckBox("a-z");
    QCheckBox *upperCase = new QCheckBox("A-Z");
    QCheckBox *specialChar = new QCheckBox("`~!@#$%^&*()_+{}|:?><[];'./'");


    digit->setFixedSize(100, 30);
    lowerCase->setFixedSize(100, 30);
    upperCase->setFixedSize(100, 30);
    specialChar->setFixedSize(100, 30);
    QHBoxLayout * checkBoxes = new QHBoxLayout;

    checkBoxes->addWidget(digit);
    checkBoxes->addWidget(lowerCase);
    checkBoxes->addWidget(upperCase);
    checkBoxes->addWidget(specialChar);
    checkBoxes->setAlignment(Qt::AlignLeft);
    layout->addLayout(checkBoxes);


    QHBoxLayout * customerizedLayout = new QHBoxLayout;
    QLabel * customerizedCharLabel = new QLabel("用户自定义字符");
    QLineEdit * customerizedCharInput = new QLineEdit;
    customerizedCharLabel->setFixedSize(100, 30);
    customerizedCharInput->setFixedSize(300, 30);
    customerizedLayout->addWidget(customerizedCharLabel);
    customerizedLayout->addWidget(customerizedCharInput);
    customerizedLayout->setAlignment(Qt::AlignLeft);
    layout->addLayout(customerizedLayout);



    layout->setAlignment(Qt::AlignTop);

    QHBoxLayout * numberInputLayout = new QHBoxLayout;
    QLabel * numberInputLabel = new QLabel("生成的密码数量");
    QLineEdit * numberInput = new QLineEdit;
    numberInputLabel->setFixedSize(100, 30);
    numberInput->setFixedSize(300, 30);
    numberInputLayout->addWidget(numberInputLabel);
    numberInputLayout->addWidget(numberInput);
    numberInputLayout->setAlignment(Qt::AlignLeft);
    layout->addLayout(numberInputLayout);


    QHBoxLayout * passwordLengthLayout = new QHBoxLayout;

    QLabel * passwordLengthLabel = new QLabel("密码长度范围");
    QLineEdit * minPasswordLengthInput = new QLineEdit;
    QLineEdit * maxPasswordLengthInput = new QLineEdit;

    passwordLengthLabel->setFixedSize(100, 30);

    minPasswordLengthInput->setFixedSize(50, 30);
    maxPasswordLengthInput->setFixedSize(50, 30);

    passwordLengthLayout->addWidget(passwordLengthLabel);
    passwordLengthLayout->addWidget(minPasswordLengthInput);

    // 创建一个整数验证器，限制输入值在0到100之间
    QIntValidator *validatorForMin = new MyIntValidator(6, 100, this);
    minPasswordLengthInput->setValidator(validatorForMin);

    // 创建一个整数验证器，限制输入值在0到100之间
    QIntValidator *validatorForMax = new MyIntValidator(6, 100, this);
    maxPasswordLengthInput->setValidator(validatorForMax);

    QLabel * to = new QLabel("-");
    to->setFixedSize(6, 30);

    passwordLengthLayout->addWidget(to);
    passwordLengthLayout->addWidget(maxPasswordLengthInput);
    passwordLengthLayout->setAlignment(Qt::AlignLeft);
    layout->addLayout(passwordLengthLayout);

    this->digit = digit;
    this->lowerCase = lowerCase;
    this->upperCase = upperCase;
    this->specialChar = specialChar;
    this->customerizedCharInput = customerizedCharInput;
    this->numberInput = numberInput;
    this->minPasswordLengthInput = minPasswordLengthInput;
    this->maxPasswordLengthInput = maxPasswordLengthInput;

    QHBoxLayout * buttonWrap = new QHBoxLayout;

    QPushButton * button = new QPushButton("生成");

    buttonWrap->addWidget(button);

    QObject::connect(button, &QPushButton::clicked, this, &RandomPasswordGen::passwordGenerate);

    QPlainTextEdit * content = new QPlainTextEdit;

    layout->addWidget(content);

    this->content = content;
    this->setLayout(layout);
}

void RandomPasswordGen::passwordGenerate() {



}

