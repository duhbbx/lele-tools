#include "jsonformatter.h"

#include <QDebug>
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QFont>
#include <QFontMetrics>
#include <QScrollBar>
#include <QTextCursor>
#include <QTextBlock>

REGISTER_DYNAMICOBJECT(JsonFormatter);

// JSON语法高亮器实现
JsonHighlighter::JsonHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent) {
    setupFormats();
}

void JsonHighlighter::setupFormats() {
    // 键名格式 - 蓝色
    keyFormat.setForeground(QColor(0, 102, 204));
    keyFormat.setFontWeight(QFont::Bold);

    // 字符串格式 - 绿色
    stringFormat.setForeground(QColor(0, 153, 0));

    // 数字格式 - 紫色
    numberFormat.setForeground(QColor(153, 0, 153));

    // 布尔值格式 - 橙色
    booleanFormat.setForeground(QColor(255, 102, 0));
    booleanFormat.setFontWeight(QFont::Bold);

    // null格式 - 灰色
    nullFormat.setForeground(QColor(128, 128, 128));
    nullFormat.setFontItalic(true);

    // 标点符号格式 - 深灰色
    punctuationFormat.setForeground(QColor(64, 64, 64));

    // 错误格式 - 红色背景
    errorFormat.setBackground(QColor(255, 200, 200));
    errorFormat.setForeground(QColor(204, 0, 0));
}

void JsonHighlighter::highlightBlock(const QString& text) {
    // 高亮键名 (带引号的键后跟冒号)
    const QRegularExpression keyRegex("(\"[^\"]*\")\\s*:");
    QRegularExpressionMatchIterator keyIterator = keyRegex.globalMatch(text);
    while (keyIterator.hasNext()) {
        QRegularExpressionMatch match = keyIterator.next();
        setFormat(match.capturedStart(1), match.capturedLength(1), keyFormat);
    }

    // 高亮字符串值 (不是键的字符串)
    const QRegularExpression stringRegex(":\\s*(\"[^\"]*\")");
    QRegularExpressionMatchIterator stringIterator = stringRegex.globalMatch(text);
    while (stringIterator.hasNext()) {
        QRegularExpressionMatch match = stringIterator.next();
        setFormat(match.capturedStart(1), match.capturedLength(1), stringFormat);
    }

    // 高亮数字
    const QRegularExpression numberRegex(":\\s*(-?\\d+\\.?\\d*([eE][+-]?\\d+)?)");
    QRegularExpressionMatchIterator numberIterator = numberRegex.globalMatch(text);
    while (numberIterator.hasNext()) {
        QRegularExpressionMatch match = numberIterator.next();
        setFormat(match.capturedStart(1), match.capturedLength(1), numberFormat);
    }

    // 高亮布尔值
    const QRegularExpression boolRegex(":\\s*(true|false)");
    QRegularExpressionMatchIterator boolIterator = boolRegex.globalMatch(text);
    while (boolIterator.hasNext()) {
        QRegularExpressionMatch match = boolIterator.next();
        setFormat(match.capturedStart(1), match.capturedLength(1), booleanFormat);
    }

    // 高亮null
    const QRegularExpression nullRegex(":\\s*(null)");
    QRegularExpressionMatchIterator nullIterator = nullRegex.globalMatch(text);
    while (nullIterator.hasNext()) {
        QRegularExpressionMatch match = nullIterator.next();
        setFormat(match.capturedStart(1), match.capturedLength(1), nullFormat);
    }

    // 高亮标点符号
    const QRegularExpression punctRegex("[{}\\[\\],:]");
    QRegularExpressionMatchIterator punctIterator = punctRegex.globalMatch(text);
    while (punctIterator.hasNext()) {
        QRegularExpressionMatch match = punctIterator.next();
        setFormat(match.capturedStart(), match.capturedLength(), punctuationFormat);
    }
}

// JsonFormatter实现
JsonFormatter::JsonFormatter() : QWidget(nullptr), DynamicObjectBase(), isValidJson(false) {
    setupUI();

    // 不再连接textChanged信号，用户需要手动验证
    connect(formatBtn, &QPushButton::clicked, this, &JsonFormatter::onFormatJson);
    connect(minifyBtn, &QPushButton::clicked, this, &JsonFormatter::onMinifyJson);
    connect(validateBtn, &QPushButton::clicked, this, &JsonFormatter::onValidateJson);
    connect(clearBtn, &QPushButton::clicked, this, &JsonFormatter::onClearAll);
    connect(copyBtn, &QPushButton::clicked, this, &JsonFormatter::onCopyFormatted);
    connect(expandAllBtn, &QPushButton::clicked, this, &JsonFormatter::onExpandAll);
    connect(collapseAllBtn, &QPushButton::clicked, this, &JsonFormatter::onCollapseAll);
    connect(searchLineEdit, &QLineEdit::textChanged, this, &JsonFormatter::onSearchInTree);

    // 设置默认示例
    const QString sampleJson = R"({
  "name": "乐乐的工具箱",
  "version": "1.0.0",
  "features": [
    "JSON格式化",
    "语法高亮",
    "树形结构展示"
  ],
  "settings": {
    "theme": "light",
    "autoFormat": true,
    "showLineNumbers": false
  },
  "stats": {
    "users": 1000,
    "rating": 4.8,
    "isActive": true,
    "lastUpdate": null
  }
})";

    inputTextEdit->setPlainText(sampleJson);
    // 初始化时设置默认状态
    updateStatusBar(tr("请输入JSON数据或点击按钮进行操作"), false);
    onFormatJson();
}

JsonFormatter::~JsonFormatter() = default;

void JsonFormatter::setupUI() {
    // 主布局
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // 设置工具栏
    setupToolbar();

    // 创建主分割器
    mainSplitter = new QSplitter(Qt::Horizontal);
    mainSplitter->setChildrenCollapsible(false);

    // 设置输入区域
    setupInputArea();

    // 设置输出区域
    setupOutputArea();

    // 添加到分割器
    mainSplitter->addWidget(inputWidget);
    mainSplitter->addWidget(outputTabWidget);
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 2);

    // 添加到主布局
    mainLayout->addWidget(toolbarWidget);
    mainLayout->addWidget(mainSplitter);
}

void JsonFormatter::setupToolbar() {
    toolbarWidget = new QWidget();
    toolbarWidget->setFixedHeight(45); // 固定工具栏高度
    toolbarLayout = new QHBoxLayout(toolbarWidget);
    toolbarLayout->setContentsMargins(5, 5, 5, 5);
    toolbarLayout->setSpacing(8);
    // 创建紧凑的按钮
    formatBtn = new QPushButton(tr("🎨 格式化"));
    formatBtn->setToolTip(tr("格式化JSON并添加缩进"));
    minifyBtn = new QPushButton(tr("📦 压缩"));
    minifyBtn->setToolTip(tr("移除空白字符，压缩JSON"));
    validateBtn = new QPushButton(tr("✅ 验证"));
    validateBtn->setToolTip(tr("验证JSON格式是否正确"));
    clearBtn = new QPushButton(tr("🗑️ 清空"));
    clearBtn->setToolTip(tr("清空所有内容"));
    copyBtn = new QPushButton(tr("📋 复制"));
    copyBtn->setToolTip(tr("复制格式化后的JSON到剪贴板"));

    // 状态标签也设置固定高度
    statusLabel = new QLabel(tr("就绪"));
    statusLabel->setStyleSheet(R"(
        QLabel {
            color: #666;
            padding: 6px 12px;
            background: #f9f9f9;
            border-radius: 0px;
            border: 1px solid #ddd;
        }
    )");

    toolbarLayout->addWidget(formatBtn);
    toolbarLayout->addWidget(minifyBtn);
    toolbarLayout->addWidget(validateBtn);
    toolbarLayout->addWidget(clearBtn);
    toolbarLayout->addWidget(copyBtn);
    toolbarLayout->addStretch(); // 推动状态标签到右侧
    toolbarLayout->addWidget(statusLabel);
}

void JsonFormatter::setupInputArea() {
    inputWidget = new QWidget();
    inputLayout = new QVBoxLayout(inputWidget);
    inputLayout->setContentsMargins(0, 0, 0, 0);
    inputLayout->setSpacing(8);

    inputLabel = new QLabel(tr("📝 JSON输入"));
    inputLabel->setStyleSheet("font-weight: bold; font-size: 11pt; color: #333;");

    inputTextEdit = new QTextEdit();
    inputTextEdit->setPlaceholderText(tr("请输入要格式化的JSON数据..."));
    
    // 设置为纯文本模式，禁用富文本格式
    inputTextEdit->setAcceptRichText(false);
    
    // 性能优化设置
    inputTextEdit->setUndoRedoEnabled(true); // 保持撤销重做功能
    inputTextEdit->setLineWrapMode(QTextEdit::NoWrap); // 禁用自动换行，提高性能
    inputLayout->addWidget(inputLabel);
    inputLayout->addWidget(inputTextEdit);
}

void JsonFormatter::setupOutputArea() {
    outputTabWidget = new QTabWidget();

    // 格式化输出标签页
    formattedTab = new QWidget();
    formattedLayout = new QVBoxLayout(formattedTab);
    formattedLayout->setContentsMargins(8, 8, 8, 8);

    outputTextEdit = new QPlainTextEdit();
    outputTextEdit->setReadOnly(true);
    outputTextEdit->setPlaceholderText(tr("格式化后的JSON将显示在这里..."));

    highlighter = new JsonHighlighter(outputTextEdit->document());
    formattedLayout->addWidget(outputTextEdit);

    setupTreeView();

    outputTabWidget->addTab(formattedTab, tr("📄 格式化视图"));
    outputTabWidget->addTab(treeTab, tr("🌳 树形结构"));
}

void JsonFormatter::setupTreeView() {
    treeTab = new QWidget();
    treeLayout = new QVBoxLayout(treeTab);
    treeLayout->setContentsMargins(8, 8, 8, 8);
    treeLayout->setSpacing(6);

    // 创建紧凑的树形视图工具栏
    auto* treeToolbarWidget = new QWidget();
    treeToolbarWidget->setFixedHeight(35); // 固定高度
    treeToolbarLayout = new QHBoxLayout(treeToolbarWidget);
    treeToolbarLayout->setContentsMargins(0, 0, 0, 0);
    treeToolbarLayout->setSpacing(8);

    searchLineEdit = new QLineEdit();
    searchLineEdit->setPlaceholderText(tr("🔍 搜索节点..."));
    searchLineEdit->setFixedSize(180, 28);

    expandAllBtn = new QPushButton(tr("展开全部"));
    collapseAllBtn = new QPushButton(tr("折叠全部"));


    treeToolbarLayout->addWidget(searchLineEdit);
    treeToolbarLayout->addStretch();
    treeToolbarLayout->addWidget(expandAllBtn);
    treeToolbarLayout->addWidget(collapseAllBtn);

    jsonTreeWidget = new QTreeWidget();
    jsonTreeWidget->setHeaderLabels(QStringList() << tr("键/索引") << tr("值") << tr("类型"));
    jsonTreeWidget->setAlternatingRowColors(true);
    jsonTreeWidget->setRootIsDecorated(true);

    jsonTreeWidget->header()->resizeSection(0, 200);
    jsonTreeWidget->header()->resizeSection(1, 300);
    jsonTreeWidget->header()->resizeSection(2, 80);

    treeLayout->addWidget(treeToolbarWidget);
    treeLayout->addWidget(jsonTreeWidget);
}

// 主要功能方法实现
void JsonFormatter::performValidation() {
    const QString text = inputTextEdit->toPlainText().trimmed();
    if (text.isEmpty()) {
        updateStatusBar(tr("输入为空"), false);
        isValidJson = false;
        return;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &error);

    if (error.error != QJsonParseError::NoError) {
        updateStatusBar(QString(tr("JSON格式错误: %1")).arg(error.errorString()), true);
        isValidJson = false;
    } else {
        updateStatusBar(tr("JSON格式正确"), false);
        isValidJson = true;
        currentJsonDoc = doc;
    }
}

void JsonFormatter::onFormatJson() {
    // 先进行验证
    performValidation();
    
    if (!isValidJson) {
        QMessageBox::warning(this, tr("格式化失败"), tr("请先输入有效的JSON数据"));
        return;
    }

    const QString formatted = QString::fromUtf8(currentJsonDoc.toJson(QJsonDocument::Indented));
    outputTextEdit->setPlainText(formatted);
    updateJsonTree(currentJsonDoc);
    updateStatusBar(tr("JSON格式化完成"), false);
}

void JsonFormatter::onMinifyJson() {
    // 先进行验证
    performValidation();
    
    if (!isValidJson) {
        QMessageBox::warning(this, tr("压缩失败"), tr("请先输入有效的JSON数据"));
        return;
    }

    const QString minified = QString::fromUtf8(currentJsonDoc.toJson(QJsonDocument::Compact));
    outputTextEdit->setPlainText(minified);
    updateStatusBar(tr("JSON压缩完成"), false);
}

void JsonFormatter::onValidateJson() {
    const QString text = inputTextEdit->toPlainText().trimmed();
    if (text.isEmpty()) {
        QMessageBox::information(this, tr("验证结果"), tr("输入为空，请输入JSON数据"));
        return;
    }

    // 使用统一的验证方法
    performValidation();

    if (isValidJson) {
        QMessageBox::information(this, tr("验证结果"), tr("✅ JSON格式正确！"));
    } else {
        // 再次解析以获取详细错误信息用于显示
        QJsonParseError error;
        QJsonDocument::fromJson(text.toUtf8(), &error);
        const QString errorMsg = QString(tr("❌ JSON格式错误:\n\n错误位置: 第%1个字符\n错误信息: %2"))
                           .arg(error.offset)
                           .arg(error.errorString());
        QMessageBox::warning(this, tr("验证结果"), errorMsg);
    }
}

void JsonFormatter::onClearAll() {
    inputTextEdit->clear();
    outputTextEdit->clear();
    jsonTreeWidget->clear();
    updateStatusBar(tr("已清空所有内容"), false);
}

void JsonFormatter::onCopyFormatted() {
    const QString text = outputTextEdit->toPlainText();
    if (text.isEmpty()) {
        QMessageBox::information(this, tr("复制失败"), tr("没有可复制的内容，请先格式化JSON"));
        return;
    }

    QApplication::clipboard()->setText(text);
    updateStatusBar(tr("已复制到剪贴板"), false);
}

void JsonFormatter::onExpandAll() const {
    jsonTreeWidget->expandAll();
}

void JsonFormatter::onCollapseAll() const {
    jsonTreeWidget->collapseAll();
}

void JsonFormatter::onSearchInTree() const {
    QString searchText = searchLineEdit->text().toLower();

    QTreeWidgetItemIterator it(jsonTreeWidget);
    while (*it) {
        QTreeWidgetItem* item = *it;
        bool matches = false;

        for (int col = 0; col < item->columnCount(); ++col) {
            if (item->text(col).toLower().contains(searchText)) {
                matches = true;
                break;
            }
        }

        item->setHidden(!matches && !searchText.isEmpty());
        ++it;
    }
}

void JsonFormatter::updateJsonTree(const QJsonDocument& doc) {
    jsonTreeWidget->clear();

    auto* rootItem = new QTreeWidgetItem(jsonTreeWidget);

    if (doc.isObject()) {
        rootItem->setText(0, "root");
        rootItem->setText(1, "{...}");
        rootItem->setText(2, "Object");

        QJsonObject obj = doc.object();
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            addJsonValueToTree(rootItem, it.key(), it.value());
        }
    } else if (doc.isArray()) {
        rootItem->setText(0, "root");
        rootItem->setText(1, "[...]");
        rootItem->setText(2, "Array");

        QJsonArray arr = doc.array();
        for (int i = 0; i < arr.size(); ++i) {
            addJsonValueToTree(rootItem, QString("[%1]").arg(i), arr[i]);
        }
    }

    jsonTreeWidget->expandAll();
}

void JsonFormatter::addJsonValueToTree(QTreeWidgetItem* parent, const QString& key, const QJsonValue& value) {
    auto* item = new QTreeWidgetItem(parent);
    item->setText(0, key);
    item->setText(2, getJsonValueTypeString(value));

    switch (value.type()) {
    case QJsonValue::Object: {
        item->setText(1, "{...}");
        QJsonObject obj = value.toObject();
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            addJsonValueToTree(item, it.key(), it.value());
        }
        break;
    }
    case QJsonValue::Array: {
        QJsonArray arr = value.toArray();
        item->setText(1, QString("[%1 items]").arg(arr.size()));
        for (int i = 0; i < arr.size(); ++i) {
            addJsonValueToTree(item, QString("[%1]").arg(i), arr[i]);
        }
        break;
    }
    case QJsonValue::String:
        item->setText(1, QString("\"%1\"").arg(value.toString()));
        break;
    case QJsonValue::Double:
        item->setText(1, QString::number(value.toDouble()));
        break;
    case QJsonValue::Bool:
        item->setText(1, value.toBool() ? "true" : "false");
        break;
    case QJsonValue::Null:
        item->setText(1, "null");
        break;
    default:
        item->setText(1, "undefined");
        break;
    }
}

void JsonFormatter::updateStatusBar(const QString& message, bool isError) {
    statusLabel->setText(message);
    if (isError) {
        statusLabel->setStyleSheet(
            "color: #d32f2f; padding: 8px; background: #ffebee; border-radius: 0px; border: 1px solid #f8bbd9;");
    } else {
        statusLabel->setStyleSheet(
            "color: #2e7d32; padding: 8px; background: #e8f5e8; border-radius: 0px; border: 1px solid #c8e6c9;");
    }
}

QString JsonFormatter::getJsonValueTypeString(const QJsonValue& value) {
    switch (value.type()) {
    case QJsonValue::Object: return "Object";
    case QJsonValue::Array: return "Array";
    case QJsonValue::String: return "String";
    case QJsonValue::Double: return "Number";
    case QJsonValue::Bool: return "Boolean";
    case QJsonValue::Null: return "Null";
    default: return "Unknown";
    }
}

QIcon JsonFormatter::getJsonValueIcon(const QJsonValue& value) {
    return QIcon();
}

