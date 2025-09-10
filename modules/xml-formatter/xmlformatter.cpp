#include "xmlformatter.h"

#include <QDebug>
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QFont>

REGISTER_DYNAMICOBJECT(XmlFormatter);

// XML语法高亮器实现
XmlHighlighter::XmlHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent) {
    setupFormats();
}

void XmlHighlighter::setupFormats() {
    // XML元素格式 - 蓝色
    xmlElementFormat.setForeground(QColor(0, 102, 204));
    xmlElementFormat.setFontWeight(QFont::Bold);

    // XML属性格式 - 绿色
    xmlAttributeFormat.setForeground(QColor(0, 153, 0));

    // XML值格式 - 红色
    xmlValueFormat.setForeground(QColor(204, 0, 0));

    // XML注释格式 - 灰色
    xmlCommentFormat.setForeground(QColor(128, 128, 128));
    xmlCommentFormat.setFontItalic(true);

    // XML关键字格式 - 紫色
    xmlKeywordFormat.setForeground(QColor(153, 0, 153));
    xmlKeywordFormat.setFontWeight(QFont::Bold);
}

void XmlHighlighter::highlightBlock(const QString& text) {
    // 高亮XML注释
    QRegularExpression commentRegex("<!--[^>]*-->");
    QRegularExpressionMatchIterator commentIterator = commentRegex.globalMatch(text);
    while (commentIterator.hasNext()) {
        QRegularExpressionMatch match = commentIterator.next();
        setFormat(match.capturedStart(), match.capturedLength(), xmlCommentFormat);
    }

    // 高亮XML元素
    QRegularExpression elementRegex("<[!?/]?\\b[A-Za-z][A-Za-z0-9-]*\\b[^>]*>");
    QRegularExpressionMatchIterator elementIterator = elementRegex.globalMatch(text);
    while (elementIterator.hasNext()) {
        QRegularExpressionMatch match = elementIterator.next();
        setFormat(match.capturedStart(), match.capturedLength(), xmlElementFormat);
    }

    // 高亮XML属性
    QRegularExpression attributeRegex("\\b[A-Za-z][A-Za-z0-9-]*(?=\\s*=)");
    QRegularExpressionMatchIterator attributeIterator = attributeRegex.globalMatch(text);
    while (attributeIterator.hasNext()) {
        QRegularExpressionMatch match = attributeIterator.next();
        setFormat(match.capturedStart(), match.capturedLength(), xmlAttributeFormat);
    }

    // 高亮XML属性值
    QRegularExpression valueRegex("\"[^\"]*\"|'[^']*'");
    QRegularExpressionMatchIterator valueIterator = valueRegex.globalMatch(text);
    while (valueIterator.hasNext()) {
        QRegularExpressionMatch match = valueIterator.next();
        setFormat(match.capturedStart(), match.capturedLength(), xmlValueFormat);
    }
}

// XmlFormatter实现
XmlFormatter::XmlFormatter() : QWidget(nullptr), DynamicObjectBase(), isValidXml(false) {
    setupUI();

    // 连接信号槽
    connect(inputTextEdit, &QTextEdit::textChanged, this, &XmlFormatter::onInputTextChanged);
    connect(formatBtn, &QPushButton::clicked, this, &XmlFormatter::onFormatXml);
    connect(minifyBtn, &QPushButton::clicked, this, &XmlFormatter::onMinifyXml);
    connect(validateBtn, &QPushButton::clicked, this, &XmlFormatter::onValidateXml);
    connect(clearBtn, &QPushButton::clicked, this, &XmlFormatter::onClearAll);
    connect(copyBtn, &QPushButton::clicked, this, &XmlFormatter::onCopyFormatted);

    // 设置默认示例
    QString sampleXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<bookstore>
    <book id="1" category="fiction">
        <title lang="en">Great Gatsby</title>
        <author>F. Scott Fitzgerald</author>
        <year>1925</year>
        <price>10.99</price>
    </book>
</bookstore>)";

    inputTextEdit->setPlainText(sampleXml);
    onFormatXml();
}

XmlFormatter::~XmlFormatter() = default;

void XmlFormatter::setupUI() {
    // 主布局
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // 设置工具栏
    setupToolbar();

    // 设置输入输出区域
    setupInputOutput();

    // 添加到主布局
    mainLayout->addWidget(toolbarWidget);
    mainLayout->addWidget(mainSplitter);

    // 设置样式
    setStyleSheet(R"(
        QWidget {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
        }
        QPushButton {
            background-color: #f0f0f0;
            border: 1px solid #cccccc;
            padding: 4px 8px;
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            font-weight: normal;
            font-size: 10pt;
        }
        QPushButton:hover {
            background-color: #e0e0e0;
            border-color: #999999;
        }
        QPushButton:pressed {
            background-color: #d0d0d0;
        }
        QTextEdit, QPlainTextEdit {
            border: 2px solid #dddddd;
            padding: 2px;
            font-family: 'Consolas', 'Monaco', monospace;
            font-size: 10pt;
        }
        QTextEdit:focus, QPlainTextEdit:focus {
            border-color: #4CAF50;
        }
    )");
}

void XmlFormatter::setupToolbar() {
    toolbarWidget = new QWidget();
    toolbarWidget->setFixedHeight(45);
    toolbarLayout = new QHBoxLayout(toolbarWidget);
    toolbarLayout->setContentsMargins(5, 5, 5, 5);
    toolbarLayout->setSpacing(8);

    formatBtn = new QPushButton("🎨 格式化");
    formatBtn->setFixedSize(85, 32);

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
    statusLabel->setStyleSheet("color: #666; font-weight: bold; font-size: 10pt; padding: 6px 12px; background: #f9f9f9; border: 1px solid #ddd;");

    toolbarLayout->addWidget(formatBtn);
    toolbarLayout->addWidget(minifyBtn);
    toolbarLayout->addWidget(validateBtn);
    toolbarLayout->addWidget(clearBtn);
    toolbarLayout->addWidget(copyBtn);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(statusLabel);
}

void XmlFormatter::setupInputOutput() {
    mainSplitter = new QSplitter(Qt::Horizontal);

    // 输入区域
    inputWidget = new QWidget();
    inputLayout = new QVBoxLayout(inputWidget);

    inputLabel = new QLabel("📝 XML输入");
    inputLabel->setStyleSheet("font-weight: bold; font-size: 10pt; color: #333;");

    inputTextEdit = new QTextEdit();
    inputTextEdit->setPlaceholderText("请输入要格式化的XML数据...");

    inputLayout->addWidget(inputLabel);
    inputLayout->addWidget(inputTextEdit);

    // 输出区域
    outputWidget = new QWidget();
    outputLayout = new QVBoxLayout(outputWidget);

    outputLabel = new QLabel("📄 格式化结果");
    outputLabel->setStyleSheet("font-weight: bold; font-size: 10pt; color: #333;");

    outputTextEdit = new QPlainTextEdit();
    outputTextEdit->setReadOnly(true);
    outputTextEdit->setFont(QFont("Consolas", 11));

    highlighter = new XmlHighlighter(outputTextEdit->document());

    outputLayout->addWidget(outputLabel);
    outputLayout->addWidget(outputTextEdit);

    mainSplitter->addWidget(inputWidget);
    mainSplitter->addWidget(outputWidget);
}

void XmlFormatter::onInputTextChanged() {
    QString text = inputTextEdit->toPlainText().trimmed();
    if (text.isEmpty()) {
        updateStatus("输入为空", false);
        return;
    }

    QString errorMessage;
    isValidXml = validateXmlString(text, errorMessage);
    updateStatus(isValidXml ? "XML格式正确" : QString("XML格式错误: %1").arg(errorMessage), !isValidXml);
}

void XmlFormatter::onFormatXml() {
    const QString text = inputTextEdit->toPlainText().trimmed();
    if (text.isEmpty())
        return;

    QString formatted = formatXmlString(text);
    if (!formatted.isEmpty()) {
        outputTextEdit->setPlainText(formatted);
        updateStatus("XML格式化完成", false);
    }
}

void XmlFormatter::onMinifyXml() {
    const QString text = inputTextEdit->toPlainText().trimmed();
    if (text.isEmpty())
        return;

    QString minified = minifyXmlString(text);
    outputTextEdit->setPlainText(minified);
    updateStatus("XML压缩完成", false);
}

void XmlFormatter::onValidateXml() {
    QString text = inputTextEdit->toPlainText().trimmed();
    if (text.isEmpty())
        return;

    QString errorMessage;
    bool isValid = validateXmlString(text, errorMessage);
    QMessageBox::information(this, "验证结果", isValid ? "✅ XML格式正确！" : QString("❌ XML格式错误:\n%1").arg(errorMessage));
}

void XmlFormatter::onClearAll() {
    inputTextEdit->clear();
    outputTextEdit->clear();
    updateStatus("已清空所有内容", false);
}

void XmlFormatter::onCopyFormatted() {
    QString text = outputTextEdit->toPlainText();
    if (!text.isEmpty()) {
        QApplication::clipboard()->setText(text);
        updateStatus("已复制到剪贴板", false);
    }
}

QString XmlFormatter::formatXmlString(const QString& xml) {
    QString result;
    QXmlStreamReader reader(xml);
    QXmlStreamWriter writer(&result);
    writer.setAutoFormatting(true);
    writer.setAutoFormattingIndent(2);

    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isWhitespace()) {
            writer.writeCurrentToken(reader);
        }
    }

    return reader.hasError() ? QString() : result;
}

QString XmlFormatter::minifyXmlString(const QString& xml) {
    QString result = xml;
    result.replace(QRegularExpression(">\\s+<"), "><");
    result.replace(QRegularExpression("\\s+"), " ");
    return result.trimmed();
}

bool XmlFormatter::validateXmlString(const QString& xml, QString& errorMessage) {
    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.hasError()) {
            errorMessage = reader.errorString();
            return false;
        }
    }
    return true;
}

void XmlFormatter::updateStatus(const QString& message, bool isError) {
    statusLabel->setText(message);
    QString color = isError ? "#d32f2f" : "#2e7d32";
    QString bg = isError ? "#ffebee" : "#e8f5e8";
    statusLabel->setStyleSheet(QString("color: %1; font-size: 1pt; padding: 4px 8px; background: %2;").arg(color, bg));
}

#include "xmlformatter.moc"
