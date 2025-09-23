#ifndef CALCULATOR_H
#define CALCULATOR_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QRegularExpression>
#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <stack>
#include <queue>
#include <cmath>

#include "../../common/dynamicobjectbase.h"

class Calculator : public QWidget, public DynamicObjectBase {
    Q_OBJECT

public:
    explicit Calculator();

private slots:
    void onExpressionChanged();
    void onCopyResult();
    void onClearAll();
    void calculateExpression();

private:
    // UI 组件
    QTextEdit *expressionInput;
    QTextEdit *resultOutput;
    QPushButton *copyButton;
    QPushButton *clearButton;
    QLabel *statusLabel;
    
    // 计算相关
    QTimer *calculateTimer;
    QString lastCalculatedText;
    
    void setupUI();
    void applyStyles();
    void showMessage(const QString &message, bool isError = false);
    
    // 表达式解析和计算
    double evaluateExpression(const QString &expression);
    std::queue<QString> tokenizeExpression(const QString &expression);
    std::queue<QString> infixToPostfix(const std::queue<QString> &tokens);
    double evaluatePostfix(const std::queue<QString> &postfix);
    bool isOperator(const QString &token);
    bool isNumber(const QString &token);
    int getPrecedence(const QString &op);
    bool isLeftAssociative(const QString &op);
    double performOperation(const QString &op, double a, double b);
    QString validateExpression(const QString &expression);
};

#endif // CALCULATOR_H










