
#include "myintvalidator.h"

MyIntValidator::MyIntValidator(QObject * parent) : QIntValidator(parent) {

}


MyIntValidator::MyIntValidator(int bottom, int top, QObject *parent) : QIntValidator(bottom, top, parent), min(bottom), max(top) {
}


QValidator::State MyIntValidator::validate(QString &input, int &pos) const {
    if (input.isEmpty()) {
        return QValidator::Intermediate;
    }

    bool OK = false;
    int val = input.toInt(&OK);

    if (!OK) {
        qDebug() << "输入的数据不满足要求...........";
        return QValidator::Invalid;
    }

    if(val < this->min) {
        return QValidator::Intermediate;
    } else if (val > this->max) {
        return QValidator::Invalid;
    }
    return QValidator::Acceptable;
}

void MyIntValidator::fixup(QString &input) const {
    bool OK = false;
    int val = input.toInt(&OK);

    qDebug() << "input = " << input << "min = " << min << ", max = " << max << ", val = " << val;

    if (!OK) {
        qDebug() << "输入的数据不满足要求...........";
        input = "";
    }

    if (val < min || val > max) {
        qDebug() << "val = " << val << ", min = " << min;
        input = "";
    }
}

MyIntValidator::~MyIntValidator() {
    qDebug() << "MyIntValidator 销毁了.....";
}
