
#include "myintvalidator.h"

MyIntValidator::MyIntValidator(QObject * parent) : QIntValidator(parent) {

}


MyIntValidator::MyIntValidator(int bottom, int top, QObject *parent) : QIntValidator(bottom, top, parent), min(bottom), max(top) {
}


QValidator::State MyIntValidator::validate(QString &input, int &pos) {
    if (input.isEmpty()) {
        return QValidator::Intermediate;
    }

    bool OK = false;
    int val = input.toInt(&OK);

    if (!OK) {
        return QValidator::Invalid;
    }

    if(val < this->min || val > this->max) {
        return QValidator::Invalid;
    }
    return QValidator::Acceptable;
}

void MyIntValidator::fixup(QString &input) {
    input = "";
}
