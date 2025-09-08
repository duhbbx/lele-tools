#include "calculator.h"

REGISTER_DYNAMICOBJECT(Calculator);

Calculator::Calculator() : QWidget(nullptr), DynamicObjectBase()
{
    setupUI();
    applyStyles();
    
    // 设置计算定时器
    calculateTimer = new QTimer(this);
    calculateTimer->setSingleShot(true);
    calculateTimer->setInterval(500); // 500ms延迟
    connect(calculateTimer, &QTimer::timeout, this, &Calculator::calculateExpression);
    
    showMessage("表达式计算器就绪");
}

void Calculator::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(15);

    // 标题
    QLabel *titleLabel = new QLabel("表达式计算器");
    titleLabel->setStyleSheet(
        "QLabel {"
        "    font-size: 22px;"
        "    font-weight: bold;"
        "    color: #2c3e50;"
        "    margin-bottom: 10px;"
        "}"
    );
    mainLayout->addWidget(titleLabel);

    // 表达式输入区域
    QLabel *inputLabel = new QLabel("输入数学表达式:");
    inputLabel->setStyleSheet("font-weight: bold; color: #34495e; font-size: 14px;");
    mainLayout->addWidget(inputLabel);

    expressionInput = new QTextEdit();
    expressionInput->setMaximumHeight(120);
    expressionInput->setPlaceholderText("输入数学表达式，如: 2 + 3 * 4, sqrt(16), sin(30), log(100) 等...\n\n支持的运算符:\n+ - * / ^ ( )\n\n支持的函数:\nsqrt(), sin(), cos(), tan(), log(), ln(), abs()");
    expressionInput->setAcceptRichText(false);
    
    connect(expressionInput, &QTextEdit::textChanged, this, &Calculator::onExpressionChanged);
    mainLayout->addWidget(expressionInput);

    // 按钮区域
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    
    copyButton = new QPushButton("📋 复制结果");
    clearButton = new QPushButton("🗑️ 清空");
    
    copyButton->setMinimumHeight(35);
    clearButton->setMinimumHeight(35);
    copyButton->setEnabled(false);
    
    connect(copyButton, &QPushButton::clicked, this, &Calculator::onCopyResult);
    connect(clearButton, &QPushButton::clicked, this, &Calculator::onClearAll);
    
    buttonLayout->addWidget(copyButton);
    buttonLayout->addWidget(clearButton);
    buttonLayout->addStretch();
    
    mainLayout->addLayout(buttonLayout);

    // 结果显示区域
    QLabel *resultLabel = new QLabel("计算结果:");
    resultLabel->setStyleSheet("font-weight: bold; color: #34495e; font-size: 14px;");
    mainLayout->addWidget(resultLabel);

    resultOutput = new QTextEdit();
    resultOutput->setMaximumHeight(80);
    resultOutput->setReadOnly(true);
    resultOutput->setPlaceholderText("计算结果将显示在这里...");
    mainLayout->addWidget(resultOutput);

    // 状态标签
    statusLabel = new QLabel("就绪");
    statusLabel->setStyleSheet("color: #7f8c8d; font-size: 12px; padding: 5px;");
    mainLayout->addWidget(statusLabel);
}

void Calculator::onExpressionChanged()
{
    // 重启定时器，实现防抖
    calculateTimer->stop();
    calculateTimer->start();
}

void Calculator::calculateExpression()
{
    QString expression = expressionInput->toPlainText().trimmed();
    
    // 防止重复计算相同的表达式
    if (expression == lastCalculatedText) {
        return;
    }
    lastCalculatedText = expression;
    
    if (expression.isEmpty()) {
        resultOutput->clear();
        copyButton->setEnabled(false);
        showMessage("就绪");
        return;
    }
    
    // 验证表达式
    QString errorMsg = validateExpression(expression);
    if (!errorMsg.isEmpty()) {
        resultOutput->setText("❌ " + errorMsg);
        copyButton->setEnabled(false);
        showMessage(errorMsg, true);
        return;
    }
    
    try {
        double result = evaluateExpression(expression);
        
        // 检查结果是否有效
        if (std::isnan(result)) {
            resultOutput->setText("❌ 计算结果为 NaN (非数字)");
            copyButton->setEnabled(false);
            showMessage("计算结果无效", true);
        } else if (std::isinf(result)) {
            resultOutput->setText("❌ 计算结果为无穷大");
            copyButton->setEnabled(false);
            showMessage("计算结果为无穷大", true);
        } else {
            // 格式化结果显示
            QString resultText;
            if (result == static_cast<long long>(result)) {
                // 整数
                resultText = QString::number(static_cast<long long>(result));
            } else {
                // 小数，显示最多10位小数
                resultText = QString::number(result, 'g', 10);
            }
            
            resultOutput->setText("✅ " + resultText);
            copyButton->setEnabled(true);
            showMessage("计算完成");
        }
    } catch (const std::exception &e) {
        resultOutput->setText("❌ 计算错误: " + QString::fromStdString(e.what()));
        copyButton->setEnabled(false);
        showMessage("计算出错", true);
    } catch (...) {
        resultOutput->setText("❌ 未知计算错误");
        copyButton->setEnabled(false);
        showMessage("未知错误", true);
    }
}

void Calculator::onCopyResult()
{
    QString result = resultOutput->toPlainText();
    if (result.startsWith("✅ ")) {
        QString actualResult = result.mid(2); // 移除前缀
        QApplication::clipboard()->setText(actualResult);
        showMessage("结果已复制到剪贴板");
    }
}

void Calculator::onClearAll()
{
    expressionInput->clear();
    resultOutput->clear();
    copyButton->setEnabled(false);
    lastCalculatedText.clear();
    showMessage("已清空");
}

double Calculator::evaluateExpression(const QString &expression)
{
    // 将表达式转换为标记队列
    std::queue<QString> tokens = tokenizeExpression(expression);
    
    // 转换为后缀表达式
    std::queue<QString> postfix = infixToPostfix(tokens);
    
    // 计算后缀表达式
    return evaluatePostfix(postfix);
}

std::queue<QString> Calculator::tokenizeExpression(const QString &expression)
{
    std::queue<QString> tokens;
    QString currentToken;
    
    for (int i = 0; i < expression.length(); i++) {
        QChar c = expression[i];
        
        if (c.isSpace()) {
            if (!currentToken.isEmpty()) {
                tokens.push(currentToken);
                currentToken.clear();
            }
        } else if (c.isDigit() || c == '.') {
            currentToken += c;
        } else if (c.isLetter()) {
            currentToken += c;
        } else if (QString("+-*/^()").contains(c)) {
            if (!currentToken.isEmpty()) {
                tokens.push(currentToken);
                currentToken.clear();
            }
            tokens.push(QString(c));
        }
    }
    
    if (!currentToken.isEmpty()) {
        tokens.push(currentToken);
    }
    
    return tokens;
}

std::queue<QString> Calculator::infixToPostfix(const std::queue<QString> &tokens)
{
    std::queue<QString> postfix;
    std::stack<QString> operators;
    std::queue<QString> tokensCopy = tokens;
    
    while (!tokensCopy.empty()) {
        QString token = tokensCopy.front();
        tokensCopy.pop();
        
        if (isNumber(token)) {
            postfix.push(token);
        } else if (token.startsWith("sqrt") || token.startsWith("sin") || 
                   token.startsWith("cos") || token.startsWith("tan") ||
                   token.startsWith("log") || token.startsWith("ln") ||
                   token.startsWith("abs")) {
            operators.push(token);
        } else if (token == "(") {
            operators.push(token);
        } else if (token == ")") {
            while (!operators.empty() && operators.top() != "(") {
                postfix.push(operators.top());
                operators.pop();
            }
            if (!operators.empty()) {
                operators.pop(); // 移除 "("
            }
            // 如果栈顶是函数，也要弹出
            if (!operators.empty() && !isOperator(operators.top()) && operators.top() != "(") {
                postfix.push(operators.top());
                operators.pop();
            }
        } else if (isOperator(token)) {
            while (!operators.empty() && operators.top() != "(" &&
                   (getPrecedence(operators.top()) > getPrecedence(token) ||
                    (getPrecedence(operators.top()) == getPrecedence(token) && isLeftAssociative(token)))) {
                postfix.push(operators.top());
                operators.pop();
            }
            operators.push(token);
        }
    }
    
    while (!operators.empty()) {
        postfix.push(operators.top());
        operators.pop();
    }
    
    return postfix;
}

double Calculator::evaluatePostfix(const std::queue<QString> &postfix)
{
    std::stack<double> values;
    std::queue<QString> postfixCopy = postfix;
    
    while (!postfixCopy.empty()) {
        QString token = postfixCopy.front();
        postfixCopy.pop();
        
        if (isNumber(token)) {
            values.push(token.toDouble());
        } else if (isOperator(token)) {
            if (values.size() < 2) {
                throw std::runtime_error("表达式格式错误");
            }
            double b = values.top(); values.pop();
            double a = values.top(); values.pop();
            values.push(performOperation(token, a, b));
        } else if (token.startsWith("sqrt")) {
            if (values.empty()) throw std::runtime_error("sqrt函数参数不足");
            double a = values.top(); values.pop();
            values.push(std::sqrt(a));
        } else if (token.startsWith("sin")) {
            if (values.empty()) throw std::runtime_error("sin函数参数不足");
            double a = values.top(); values.pop();
            values.push(std::sin(a * M_PI / 180.0)); // 转换为弧度
        } else if (token.startsWith("cos")) {
            if (values.empty()) throw std::runtime_error("cos函数参数不足");
            double a = values.top(); values.pop();
            values.push(std::cos(a * M_PI / 180.0));
        } else if (token.startsWith("tan")) {
            if (values.empty()) throw std::runtime_error("tan函数参数不足");
            double a = values.top(); values.pop();
            values.push(std::tan(a * M_PI / 180.0));
        } else if (token.startsWith("log")) {
            if (values.empty()) throw std::runtime_error("log函数参数不足");
            double a = values.top(); values.pop();
            values.push(std::log10(a));
        } else if (token.startsWith("ln")) {
            if (values.empty()) throw std::runtime_error("ln函数参数不足");
            double a = values.top(); values.pop();
            values.push(std::log(a));
        } else if (token.startsWith("abs")) {
            if (values.empty()) throw std::runtime_error("abs函数参数不足");
            double a = values.top(); values.pop();
            values.push(std::abs(a));
        }
    }
    
    if (values.size() != 1) {
        throw std::runtime_error("表达式格式错误");
    }
    
    return values.top();
}

bool Calculator::isOperator(const QString &token)
{
    return token == "+" || token == "-" || token == "*" || token == "/" || token == "^";
}

bool Calculator::isNumber(const QString &token)
{
    bool ok;
    token.toDouble(&ok);
    return ok;
}

int Calculator::getPrecedence(const QString &op)
{
    if (op == "+" || op == "-") return 1;
    if (op == "*" || op == "/") return 2;
    if (op == "^") return 3;
    return 0;
}

bool Calculator::isLeftAssociative(const QString &op)
{
    return op != "^"; // 只有幂运算是右结合的
}

double Calculator::performOperation(const QString &op, double a, double b)
{
    if (op == "+") return a + b;
    if (op == "-") return a - b;
    if (op == "*") return a * b;
    if (op == "/") {
        if (b == 0) throw std::runtime_error("除零错误");
        return a / b;
    }
    if (op == "^") return std::pow(a, b);
    
    throw std::runtime_error("未知运算符: " + op.toStdString());
}

QString Calculator::validateExpression(const QString &expression)
{
    if (expression.length() > 1000) {
        return "表达式过长";
    }
    
    // 检查括号匹配
    int bracketCount = 0;
    for (QChar c : expression) {
        if (c == '(') bracketCount++;
        else if (c == ')') bracketCount--;
        if (bracketCount < 0) return "括号不匹配";
    }
    if (bracketCount != 0) return "括号不匹配";
    
    // 检查是否包含无效字符
    QRegularExpression validChars("^[0-9+\\-*/^().\\s a-zA-Z]*$");
    if (!validChars.match(expression).hasMatch()) {
        return "包含无效字符";
    }
    
    return "";
}

void Calculator::applyStyles()
{
    setStyleSheet(
        "QTextEdit {"
        "    border: 2px solid #bdc3c7;"
        "    border-radius: 0px;"
        "    padding: 8px;"
        "    font-family: 'Consolas', 'Monaco', 'Courier New', monospace;"
        "    font-size: 14px;"
        "    background-color: white;"
        "}"
        "QTextEdit:focus {"
        "    border-color: #3498db;"
        "}"
        "QTextEdit[readOnly='true'] {"
        "    background-color: #f8f9fa;"
        "    color: #2c3e50;"
        "}"
        
        "QPushButton {"
        "    background-color: #3498db;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 0px;"
        "    padding: 8px 16px;"
        "    font-size: 13px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background-color: #2980b9;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #21618c;"
        "}"
        "QPushButton:disabled {"
        "    background-color: #bdc3c7;"
        "    color: #7f8c8d;"
        "}"
    );
}

void Calculator::showMessage(const QString &message, bool isError)
{
    if (isError) {
        statusLabel->setStyleSheet("color: #e74c3c; font-size: 12px; padding: 5px;");
    } else {
        statusLabel->setStyleSheet("color: #27ae60; font-size: 12px; padding: 5px;");
    }
    statusLabel->setText(message);
}

#include "calculator.moc"







