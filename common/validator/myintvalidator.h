
#ifndef MYINTVALIDATOR_H
#define MYINTVALIDATOR_H


#include <QValidator>
#include <QIntValidator>

class MyIntValidator : public QIntValidator
{
public:

    explicit MyIntValidator(QObject * parent = nullptr);
    MyIntValidator(int bottom, int top, QObject *parent = nullptr);
    ~MyIntValidator();

    virtual QValidator::State validate(QString &, int &) const override;
};

#endif // MYINTVALIDATOR_H
