#include "jsonformatter.h"
#include "ui_jsonformatter.h"

JsonFormatter::JsonFormatter(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::JsonFormatter)
{
    ui->setupUi(this);
}

JsonFormatter::~JsonFormatter()
{
    delete ui;
}
