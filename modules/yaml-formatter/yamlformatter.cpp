#include "yamlformatter.h"

#include <QDebug>
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QFont>

REGISTER_DYNAMICOBJECT(YamlFormatter);

// YAML语法高亮器实现
YamlHighlighter::YamlHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
    setupFormats();
}

void YamlHighlighter::setupFormats()
{
    // 键名格式 - 蓝色
    keyFormat.setForeground(QColor(0, 102, 204));
    keyFormat.setFontWeight(QFont::Bold);
    
    // 值格式 - 绿色
    valueFormat.setForeground(QColor(0, 153, 0));
    
    // 注释格式 - 灰色
    commentFormat.setForeground(QColor(128, 128, 128));
    commentFormat.setFontItalic(true);
    
    // 字符串格式 - 红色
    stringFormat.setForeground(QColor(204, 0, 0));
    
    // 数字格式 - 紫色
    numberFormat.setForeground(QColor(153, 0, 153));
    
    // 布尔值格式 - 橙色
    booleanFormat.setForeground(QColor(255, 102, 0));
    booleanFormat.setFontWeight(QFont::Bold);
    
    // null格式 - 灰色
    nullFormat.setForeground(QColor(128, 128, 128));
    nullFormat.setFontItalic(true);
    
    // 文档分隔符格式 - 深蓝色
    documentFormat.setForeground(QColor(0, 51, 102));
    documentFormat.setFontWeight(QFont::Bold);
    
    // 列表格式 - 深绿色
    listFormat.setForeground(QColor(0, 102, 51));
    listFormat.setFontWeight(QFont::Bold);
}

void YamlHighlighter::highlightBlock(const QString& text)
{
    // 高亮注释
    QRegularExpression commentRegex("#.*$");
    QRegularExpressionMatchIterator commentIterator = commentRegex.globalMatch(text);
    while (commentIterator.hasNext()) {
        QRegularExpressionMatch match = commentIterator.next();
        setFormat(match.capturedStart(), match.capturedLength(), commentFormat);
    }
    
    // 高亮文档分隔符
    QRegularExpression docRegex("^(---|\\.\\.\\.)\\s*$");
    QRegularExpressionMatchIterator docIterator = docRegex.globalMatch(text);
    while (docIterator.hasNext()) {
        QRegularExpressionMatch match = docIterator.next();
        setFormat(match.capturedStart(), match.capturedLength(), documentFormat);
    }
    
    // 高亮键名
    QRegularExpression keyRegex("^\\s*([\\w\\-_]+)\\s*:");
    QRegularExpressionMatchIterator keyIterator = keyRegex.globalMatch(text);
    while (keyIterator.hasNext()) {
        QRegularExpressionMatch match = keyIterator.next();
        setFormat(match.capturedStart(1), match.capturedLength(1), keyFormat);
    }
    
    // 高亮列表项
    QRegularExpression listRegex("^\\s*-\\s+");
    QRegularExpressionMatchIterator listIterator = listRegex.globalMatch(text);
    while (listIterator.hasNext()) {
        QRegularExpressionMatch match = listIterator.next();
        setFormat(match.capturedStart(), match.capturedLength(), listFormat);
    }
    
    // 高亮字符串值 (带引号)
    QRegularExpression stringRegex("([\"'])[^\\1]*\\1");
    QRegularExpressionMatchIterator stringIterator = stringRegex.globalMatch(text);
    while (stringIterator.hasNext()) {
        QRegularExpressionMatch match = stringIterator.next();
        setFormat(match.capturedStart(), match.capturedLength(), stringFormat);
    }
    
    // 高亮数字
    QRegularExpression numberRegex(":\\s*(-?\\d+\\.?\\d*)");
    QRegularExpressionMatchIterator numberIterator = numberRegex.globalMatch(text);
    while (numberIterator.hasNext()) {
        QRegularExpressionMatch match = numberIterator.next();
        setFormat(match.capturedStart(1), match.capturedLength(1), numberFormat);
    }
    
    // 高亮布尔值
    QRegularExpression boolRegex(":\\s*(true|false|yes|no|on|off)\\b");
    QRegularExpressionMatchIterator boolIterator = boolRegex.globalMatch(text);
    while (boolIterator.hasNext()) {
        QRegularExpressionMatch match = boolIterator.next();
        setFormat(match.capturedStart(1), match.capturedLength(1), booleanFormat);
    }
    
    // 高亮null值
    QRegularExpression nullRegex(":\\s*(null|~)\\b");
    QRegularExpressionMatchIterator nullIterator = nullRegex.globalMatch(text);
    while (nullIterator.hasNext()) {
        QRegularExpressionMatch match = nullIterator.next();
        setFormat(match.capturedStart(1), match.capturedLength(1), nullFormat);
    }
}

// YamlFormatter实现
YamlFormatter::YamlFormatter() : QWidget(nullptr), DynamicObjectBase(), isValidYaml(false)
{
    setupUI();
    
    // 连接信号槽
    connect(inputTextEdit, &QTextEdit::textChanged, this, &YamlFormatter::onInputTextChanged);
    connect(formatBtn, &QPushButton::clicked, this, &YamlFormatter::onFormatYaml);
    connect(minifyBtn, &QPushButton::clicked, this, &YamlFormatter::onMinifyYaml);
    connect(validateBtn, &QPushButton::clicked, this, &YamlFormatter::onValidateYaml);
    connect(clearBtn, &QPushButton::clicked, this, &YamlFormatter::onClearAll);
    connect(copyBtn, &QPushButton::clicked, this, &YamlFormatter::onCopyFormatted);
    connect(indentSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &YamlFormatter::onIndentChanged);
    
    // 设置默认示例
    QString sampleYaml = R"(---
# 乐乐工具箱配置示例
app:
  name: "乐乐的工具箱"
  version: "1.0.0"
  debug: true
  
database:
  host: "localhost"
  port: 5432
  name: "lele_tools"
  
features:
  - "JSON格式化"
  - "XML格式化" 
  - "YAML格式化"
  - "正则测试"
  - "密码生成"
  
settings:
  theme: "light"
  language: "zh-CN"
  auto_save: true
  max_history: 100
  timeout: null
...)";
    
    inputTextEdit->setPlainText(sampleYaml);
    onFormatYaml();
}

YamlFormatter::~YamlFormatter()
{
}

void YamlFormatter::setupUI()
{
    // 主布局
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);
    
    // 设置工具栏
    setupToolbar();
    
    // 设置选项区域
    setupOptions();
    
    // 设置输入输出区域
    setupInputOutput();
    
    // 添加到主布局
    mainLayout->addWidget(toolbarWidget);
    mainLayout->addWidget(optionsGroup);
    mainLayout->addWidget(mainSplitter);
    
    // 设置样式
    setStyleSheet(R"(
        QWidget {
            font-family: 'Segoe UI', Arial, sans-serif;
        }
        QPushButton {
            background-color: #f0f0f0;
            border: 1px solid #cccccc;
            border-radius: 0px;
            padding: 4px 8px;
            font-size: 10pt;
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
        }
        QPushButton:hover {
            background-color: #e0e0e0;
            border-color: #999999;
        }
        QPushButton:pressed {
            background-color: #d0d0d0;
        }
        QGroupBox {
            font-weight: bold;
            border: 2px solid #cccccc;
            border-radius: 0px;
            margin-top: 1ex;
            padding-top: 10px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px 0 5px;
            font-size: 11px;
        }
        QTextEdit, QPlainTextEdit {
            border: 2px solid #dddddd;
            border-radius: 0px;
            padding: 8px;
            font-family: 'Consolas', 'Monaco', monospace;
            font-size: 11pt;
        }
        QTextEdit:focus, QPlainTextEdit:focus {
            border-color: #4CAF50;
        }
        QSpinBox {
            border: 2px solid #dddddd;
            border-radius: 0px;
            padding: 2px 4px;
            font-size: 10pt;
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
        }
        QSpinBox:focus {
            border-color: #4CAF50;
        }
    )");
}

void YamlFormatter::setupToolbar()
{
    toolbarWidget = new QWidget();
    toolbarWidget->setFixedHeight(45);
    toolbarLayout = new QHBoxLayout(toolbarWidget);
    toolbarLayout->setContentsMargins(5, 5, 5, 5);
    toolbarLayout->setSpacing(8);
    
    formatBtn = new QPushButton("🎨 格式化");
    formatBtn->setFixedSize(90, 32);
    
    minifyBtn = new QPushButton("📦 压缩");
    minifyBtn->setFixedSize(75, 32);
    
    validateBtn = new QPushButton("✅ 验证");
    validateBtn->setFixedSize(75, 32);
    
    clearBtn = new QPushButton("🗑️ 清空");
    clearBtn->setFixedSize(75, 32);
    
    copyBtn = new QPushButton("📋 复制");
    copyBtn->setFixedSize(75, 32);
    
    statusLabel = new QLabel("就绪");
    statusLabel->setFixedHeight(32);
    statusLabel->setStyleSheet("color: #666; padding: 6px 12px; background: #f9f9f9; border-radius: 0px; border: 1px solid #ddd; font-family: 'Microsoft YaHei', '微软雅黑', sans-serif; font-size: 10pt;");
    
    toolbarLayout->addWidget(formatBtn);
    toolbarLayout->addWidget(minifyBtn);
    toolbarLayout->addWidget(validateBtn);
    toolbarLayout->addWidget(clearBtn);
    toolbarLayout->addWidget(copyBtn);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(statusLabel);
}

void YamlFormatter::setupOptions()
{
    optionsGroup = new QGroupBox("⚙️ 格式选项");
    optionsGroup->setFixedHeight(60);
    optionsLayout = new QHBoxLayout(optionsGroup);
    optionsLayout->setContentsMargins(10, 10, 10, 10);
    optionsLayout->setSpacing(15);
    
    indentLabel = new QLabel("缩进空格:");
    indentSpinBox = new QSpinBox();
    indentSpinBox->setRange(2, 8);
    indentSpinBox->setValue(2);
    indentSpinBox->setFixedWidth(60);
    
    sortKeysCheck = new QCheckBox("排序键名");
    flowStyleCheck = new QCheckBox("流式风格");
    
    optionsLayout->addWidget(indentLabel);
    optionsLayout->addWidget(indentSpinBox);
    optionsLayout->addWidget(sortKeysCheck);
    optionsLayout->addWidget(flowStyleCheck);
    optionsLayout->addStretch();
}

void YamlFormatter::setupInputOutput()
{
    mainSplitter = new QSplitter(Qt::Horizontal);
    
    // 输入区域
    inputWidget = new QWidget();
    inputLayout = new QVBoxLayout(inputWidget);
    
    inputLabel = new QLabel("📝 YAML输入");
    inputLabel->setStyleSheet("font-weight: bold; font-size: 12pt; color: #333;");
    
    inputTextEdit = new QTextEdit();
    inputTextEdit->setPlaceholderText("请输入要格式化的YAML数据...");
    
    inputLayout->addWidget(inputLabel);
    inputLayout->addWidget(inputTextEdit);
    
    // 输出区域
    outputWidget = new QWidget();
    outputLayout = new QVBoxLayout(outputWidget);
    
    outputLabel = new QLabel("📄 格式化结果");
    outputLabel->setStyleSheet("font-weight: bold; font-size: 12pt; color: #333;");
    
    outputTextEdit = new QPlainTextEdit();
    outputTextEdit->setReadOnly(true);
    outputTextEdit->setFont(QFont("Consolas", 11));
    
    highlighter = new YamlHighlighter(outputTextEdit->document());
    
    outputLayout->addWidget(outputLabel);
    outputLayout->addWidget(outputTextEdit);
    
    mainSplitter->addWidget(inputWidget);
    mainSplitter->addWidget(outputWidget);
}

void YamlFormatter::onInputTextChanged()
{
    QString text = inputTextEdit->toPlainText().trimmed();
    if (text.isEmpty()) {
        updateStatus("输入为空", false);
        return;
    }
    
    QString errorMessage;
    isValidYaml = validateYamlString(text, errorMessage);
    updateStatus(isValidYaml ? "YAML格式看起来正确" : QString("YAML格式可能有问题: %1").arg(errorMessage), !isValidYaml);
}

void YamlFormatter::onFormatYaml()
{
    QString text = inputTextEdit->toPlainText().trimmed();
    if (text.isEmpty()) return;
    
    int indent = indentSpinBox->value();
    QString formatted = formatYamlString(text, indent);
    outputTextEdit->setPlainText(formatted);
    updateStatus("YAML格式化完成", false);
}

void YamlFormatter::onMinifyYaml()
{
    QString text = inputTextEdit->toPlainText().trimmed();
    if (text.isEmpty()) return;
    
    QString minified = minifyYamlString(text);
    outputTextEdit->setPlainText(minified);
    updateStatus("YAML压缩完成", false);
}

void YamlFormatter::onValidateYaml()
{
    QString text = inputTextEdit->toPlainText().trimmed();
    if (text.isEmpty()) return;
    
    QString errorMessage;
    bool isValid = validateYamlString(text, errorMessage);
    QMessageBox::information(this, "验证结果", isValid ? "✅ YAML格式正确！" : QString("⚠️ YAML可能有格式问题:\n%1").arg(errorMessage));
}

void YamlFormatter::onClearAll()
{
    inputTextEdit->clear();
    outputTextEdit->clear();
    updateStatus("已清空所有内容", false);
}

void YamlFormatter::onCopyFormatted()
{
    QString text = outputTextEdit->toPlainText();
    if (!text.isEmpty()) {
        QApplication::clipboard()->setText(text);
        updateStatus("已复制到剪贴板", false);
    }
}

void YamlFormatter::onIndentChanged()
{
    if (!inputTextEdit->toPlainText().trimmed().isEmpty() && !outputTextEdit->toPlainText().isEmpty()) {
        onFormatYaml();
    }
}

QString YamlFormatter::formatYamlString(const QString& yaml, int indent)
{
    // 简单的YAML格式化实现
    QStringList lines = yaml.split('\n');
    QStringList result;
    QString indentStr = QString(" ").repeated(indent);
    int currentIndent = 0;
    
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith('#')) {
            result.append(line);
            continue;
        }
        
        if (trimmed.startsWith("---") || trimmed.startsWith("...")) {
            result.append(trimmed);
            currentIndent = 0;
            continue;
        }
        
        if (trimmed.startsWith('-')) {
            QString formatted = QString(indentStr).repeated(currentIndent) + trimmed;
            result.append(formatted);
        } else if (trimmed.contains(':')) {
            QStringList parts = trimmed.split(':', Qt::KeepEmptyParts);
            if (parts.size() >= 2) {
                QString key = parts[0].trimmed();
                QString value = parts.mid(1).join(':').trimmed();
                
                QString formatted = QString(indentStr).repeated(currentIndent) + key + ":";
                if (!value.isEmpty()) {
                    formatted += " " + value;
                } else {
                    currentIndent++;
                }
                result.append(formatted);
            }
        } else {
            result.append(QString(indentStr).repeated(currentIndent) + trimmed);
        }
    }
    
    return result.join('\n');
}

QString YamlFormatter::minifyYamlString(const QString& yaml)
{
    QStringList lines = yaml.split('\n');
    QStringList result;
    
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        if (!trimmed.isEmpty() && !trimmed.startsWith('#')) {
            result.append(trimmed);
        }
    }
    
    return result.join('\n');
}

bool YamlFormatter::validateYamlString(const QString& yaml, QString& errorMessage)
{
    QStringList lines = yaml.split('\n');
    int lineNumber = 0;
    
    for (const QString& line : lines) {
        lineNumber++;
        QString trimmed = line.trimmed();
        
        if (trimmed.isEmpty() || trimmed.startsWith('#') || 
            trimmed.startsWith("---") || trimmed.startsWith("...")) {
            continue;
        }
        
        if (trimmed.contains(':')) {
            QStringList parts = trimmed.split(':', Qt::KeepEmptyParts);
            if (parts.size() < 2) {
                errorMessage = QString("第%1行: 键值对格式不正确").arg(lineNumber);
                return false;
            }
        }
        
        if (line.startsWith('\t')) {
            errorMessage = QString("第%1行: 建议使用空格而不是制表符缩进").arg(lineNumber);
            return false;
        }
    }
    
    return true;
}

void YamlFormatter::updateStatus(const QString& message, bool isError)
{
    statusLabel->setText(message);
    QString color = isError ? "#d32f2f" : "#2e7d32";
    QString bg = isError ? "#ffebee" : "#e8f5e8";
    statusLabel->setStyleSheet(QString("color: %1; padding: 6px 12px; background: %2; border-radius: 0px;").arg(color, bg));
}

#include "yamlformatter.moc"
