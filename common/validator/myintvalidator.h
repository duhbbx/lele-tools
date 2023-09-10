
#ifndef MYINTVALIDATOR_H
#define MYINTVALIDATOR_H


#include <QValidator>
#include <QIntValidator>

class MyIntValidator : public QIntValidator {
public:

    explicit MyIntValidator(QObject * parent = nullptr);
    MyIntValidator(int bottom, int top, QObject *parent = nullptr);
    ~MyIntValidator();

    QValidator::State validate(QString &input, int &pos) const;

    void fixup(QString &input) const;

private:
    int min;
    int max;
};

#endif // MYINTVALIDATOR_H
