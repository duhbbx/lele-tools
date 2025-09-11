#include "xmlformatter.h"

#include <QDebug>
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QFont>

REGISTER_DYNAMICOBJECT(XmlFormatter);

// XML语法高亮器实现
XmlHighlighter::XmlHighlighter(QTextDocument* parent) : QSyntaxHighlighter(parent) {
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
    const QRegularExpression commentRegex("<!--[^>]*-->");
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
    const QString sampleXml = R"(<?xml version="1.0" encoding="UTF-8"?>
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
}

void XmlFormatter::setupToolbar() {
    toolbarWidget = new QWidget();
    toolbarWidget->setFixedHeight(45);
    toolbarLayout = new QHBoxLayout(toolbarWidget);
    toolbarLayout->setContentsMargins(5, 5, 5, 5);
    toolbarLayout->setSpacing(8);

    formatBtn = new QPushButton(tr("🎨 格式化"));
    minifyBtn = new QPushButton(tr("📦 压缩"));
    validateBtn = new QPushButton(tr("✅ 验证"));
    clearBtn = new QPushButton(tr("🗑️ 清空"));
    copyBtn = new QPushButton(tr("📋 复制"));
    statusLabel = new QLabel(tr("就绪"));
    statusLabel->setStyleSheet("color: #666; font-weight: bold; font-size: 11pt; padding: 6px 12px; background: #f9f9f9; border: 1px solid #ddd;");

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

    inputLabel = new QLabel(tr("📝 XML输入"));
    inputLabel->setStyleSheet("font-weight: bold; font-size: 11pt; color: #333;");

    inputTextEdit = new QTextEdit();
    inputTextEdit->setPlaceholderText(tr("请输入要格式化的XML数据..."));

    inputLayout->addWidget(inputLabel);
    inputLayout->addWidget(inputTextEdit);

    // 输出区域
    outputWidget = new QWidget();
    outputLayout = new QVBoxLayout(outputWidget);

    outputLabel = new QLabel(tr("📄 格式化结果"));
    outputLabel->setStyleSheet("font-weight: bold; font-size: 11pt; color: #333;");

    outputTextEdit = new QPlainTextEdit();
    outputTextEdit->setReadOnly(true);

    highlighter = new XmlHighlighter(outputTextEdit->document());

    outputLayout->addWidget(outputLabel);
    outputLayout->addWidget(outputTextEdit);

    mainSplitter->addWidget(inputWidget);
    mainSplitter->addWidget(outputWidget);

    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 2);
}

void XmlFormatter::onInputTextChanged() {
    const QString text = inputTextEdit->toPlainText().trimmed();
    if (text.isEmpty()) {
        updateStatus(tr("输入为空"), false);
        return;
    }

    QString errorMessage;
    isValidXml = validateXmlString(text, errorMessage);
    updateStatus(isValidXml ? tr("XML格式正确") : QString(tr("XML格式错误: %1")).arg(errorMessage), !isValidXml);
}

void XmlFormatter::onFormatXml() const {
    const QString text = inputTextEdit->toPlainText().trimmed();
    if (text.isEmpty())
        return;

    if (const QString formatted = formatXmlString(text); !formatted.isEmpty()) {
        outputTextEdit->setPlainText(formatted);
        updateStatus(tr("XML格式化完成"), false);
    }
}

void XmlFormatter::onMinifyXml() const {
    const QString text = inputTextEdit->toPlainText().trimmed();
    if (text.isEmpty())
        return;

    const QString minified = minifyXmlString(text);
    outputTextEdit->setPlainText(minified);
    updateStatus(tr("XML压缩完成"), false);
}

void XmlFormatter::onValidateXml() {
    const QString text = inputTextEdit->toPlainText().trimmed();
    if (text.isEmpty())
        return;

    QString errorMessage;
    const bool isValid = validateXmlString(text, errorMessage);
    QMessageBox::information(this, tr("验证结果"), isValid ? tr("✅ XML格式正确！") : QString(tr("❌ XML格式错误:\n%1")).arg(errorMessage));
}

void XmlFormatter::onClearAll() {
    inputTextEdit->clear();
    outputTextEdit->clear();
    updateStatus(tr("已清空所有内容"), false);
}

void XmlFormatter::onCopyFormatted() {
    if (const QString text = outputTextEdit->toPlainText(); !text.isEmpty()) {
        QApplication::clipboard()->setText(text);
        updateStatus(tr("已复制到剪贴板"), false);
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

void XmlFormatter::updateStatus(const QString& message, const bool isError) const {
    statusLabel->setText(message);
    QString color = isError ? "#d32f2f" : "#2e7d32";
    QString bg = isError ? "#ffebee" : "#e8f5e8";
    statusLabel->setStyleSheet(QString("color: %1; font-size: 1pt; padding: 4px 8px; background: %2;").arg(color, bg));
}

#include "xmlformatter.moc"
