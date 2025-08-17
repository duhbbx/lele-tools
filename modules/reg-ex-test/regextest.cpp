#include "regextest.h"

#include <QDebug>
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QFont>

REGISTER_DYNAMICOBJECT(RegExTest);

// 正则表达式语法高亮器实现
RegexHighlighter::RegexHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
    setupFormats();
}

void RegexHighlighter::setupFormats()
{
    // 字符类格式 - 蓝色
    characterClassFormat.setForeground(QColor(0, 102, 204));
    characterClassFormat.setFontWeight(QFont::Bold);
    
    // 量词格式 - 红色
    quantifierFormat.setForeground(QColor(204, 0, 0));
    quantifierFormat.setFontWeight(QFont::Bold);
    
    // 锚点格式 - 绿色
    anchorFormat.setForeground(QColor(0, 153, 0));
    anchorFormat.setFontWeight(QFont::Bold);
    
    // 分组格式 - 紫色
    groupFormat.setForeground(QColor(153, 0, 153));
    
    // 转义字符格式 - 橙色
    escapeFormat.setForeground(QColor(255, 102, 0));
    
    // 字面量格式 - 黑色
    literalFormat.setForeground(QColor(64, 64, 64));
}

void RegexHighlighter::highlightBlock(const QString& text)
{
    // 高亮字符类 [...]
    QRegularExpression charClassRegex("\\[[^\\]]*\\]");
    QRegularExpressionMatchIterator charClassIterator = charClassRegex.globalMatch(text);
    while (charClassIterator.hasNext()) {
        QRegularExpressionMatch match = charClassIterator.next();
        setFormat(match.capturedStart(), match.capturedLength(), characterClassFormat);
    }
    
    // 高亮量词 *, +, ?, {n,m}
    QRegularExpression quantifierRegex("[*+?]|\\{\\d+,?\\d*\\}");
    QRegularExpressionMatchIterator quantifierIterator = quantifierRegex.globalMatch(text);
    while (quantifierIterator.hasNext()) {
        QRegularExpressionMatch match = quantifierIterator.next();
        setFormat(match.capturedStart(), match.capturedLength(), quantifierFormat);
    }
    
    // 高亮锚点 ^, $, \b, \B
    QRegularExpression anchorRegex("[\\^$]|\\\\[bB]");
    QRegularExpressionMatchIterator anchorIterator = anchorRegex.globalMatch(text);
    while (anchorIterator.hasNext()) {
        QRegularExpressionMatch match = anchorIterator.next();
        setFormat(match.capturedStart(), match.capturedLength(), anchorFormat);
    }
    
    // 高亮分组 (...)
    QRegularExpression groupRegex("[()]");
    QRegularExpressionMatchIterator groupIterator = groupRegex.globalMatch(text);
    while (groupIterator.hasNext()) {
        QRegularExpressionMatch match = groupIterator.next();
        setFormat(match.capturedStart(), match.capturedLength(), groupFormat);
    }
    
    // 高亮转义字符 \d, \w, \s, etc.
    QRegularExpression escapeRegex("\\\\[dwsWDSrntvf]|\\\\.");
    QRegularExpressionMatchIterator escapeIterator = escapeRegex.globalMatch(text);
    while (escapeIterator.hasNext()) {
        QRegularExpressionMatch match = escapeIterator.next();
        setFormat(match.capturedStart(), match.capturedLength(), escapeFormat);
    }
}

// RegExTest实现
RegExTest::RegExTest() : QWidget(nullptr), DynamicObjectBase()
{
    setupUI();
    
    // 连接信号槽
    connect(testBtn, &QPushButton::clicked, this, &RegExTest::onTestRegex);
    connect(clearBtn, &QPushButton::clicked, this, &RegExTest::onClearResults);
    connect(copyBtn, &QPushButton::clicked, this, &RegExTest::onCopyMatches);
    connect(loadPresetBtn, &QPushButton::clicked, this, &RegExTest::onLoadPreset);
    connect(replaceBtn, &QPushButton::clicked, [this]() {
        QString text = testTextEdit->toPlainText();
        QString replacement = replaceEdit->text();
        QString result = text.replace(currentRegex, replacement);
        replaceResultEdit->setPlainText(result);
    });
    
    connect(regexEdit, &QLineEdit::textChanged, this, &RegExTest::onRegexChanged);
    connect(testTextEdit, &QTextEdit::textChanged, this, &RegExTest::onTextChanged);
    
    // 设置定时器用于实时测试
    testTimer = new QTimer(this);
    testTimer->setSingleShot(true);
    testTimer->setInterval(500); // 500ms延迟
    connect(testTimer, &QTimer::timeout, this, &RegExTest::performRegexTest);
    
    // 加载预设模式
    loadCommonPresets();
    
    // 设置示例
    regexEdit->setText("\\b\\w+@\\w+\\.\\w+\\b");
    testTextEdit->setPlainText("联系我们：admin@example.com 或 support@test.org\n技术支持：tech@company.net");
    replaceEdit->setText("[邮箱]");
    
    onTestRegex();
}

RegExTest::~RegExTest()
{
}

void RegExTest::setupUI()
{
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
    
    // 设置结果区域
    setupResultsArea();
    
    // 添加到分割器
    mainSplitter->addWidget(inputWidget);
    mainSplitter->addWidget(resultsTabWidget);
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 1);
    
    // 添加到主布局
    mainLayout->addWidget(toolbarWidget);
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
            padding: 6px 12px;
            font-weight: bold;
            font-size: 11pt;
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
        }
        QLineEdit, QTextEdit, QPlainTextEdit {
            border: 2px solid #dddddd;
            border-radius: 0px;
            padding: 6px;
            font-size: 11pt;
        }
        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {
            border-color: #4CAF50;
        }
        QTableWidget {
            border: 2px solid #dddddd;
            border-radius: 0px;
            alternate-background-color: #f9f9f9;
            gridline-color: #eeeeee;
        }
        QTableWidget::item {
            padding: 4px;
        }
        QTableWidget::item:selected {
            background-color: #4CAF50;
            color: white;
        }
        QTabWidget::pane {
            border: 2px solid #dddddd;
            border-radius: 0px;
            background-color: white;
        }
        QTabBar::tab {
            background-color: #f0f0f0;
            border: 1px solid #cccccc;
            padding: 8px 16px;
            margin-right: 2px;
        }
        QTabBar::tab:selected {
            background-color: white;
            border-bottom-color: white;
        }
    )");
}

void RegExTest::setupToolbar()
{
    toolbarWidget = new QWidget();
    toolbarWidget->setFixedHeight(45);
    toolbarLayout = new QHBoxLayout(toolbarWidget);
    toolbarLayout->setContentsMargins(5, 5, 5, 5);
    toolbarLayout->setSpacing(8);
    
    testBtn = new QPushButton("🔍 测试");
    testBtn->setToolTip("测试正则表达式");
    testBtn->setFixedSize(75, 32);
    
    clearBtn = new QPushButton("🗑️ 清空");
    clearBtn->setToolTip("清空结果");
    clearBtn->setFixedSize(75, 32);
    
    copyBtn = new QPushButton("📋 复制");
    copyBtn->setToolTip("复制匹配结果");
    copyBtn->setFixedSize(75, 32);
    copyBtn->setEnabled(false);
    
    statusLabel = new QLabel("就绪");
    statusLabel->setFixedHeight(32);
    statusLabel->setStyleSheet(R"(
        QLabel {
            color: #666; 
            font-weight: bold; 
            padding: 6px 12px; 
            background: #f9f9f9; 
            border-radius: 0px; 
            border: 1px solid #ddd;
        }
    )");
    
    toolbarLayout->addWidget(testBtn);
    toolbarLayout->addWidget(clearBtn);
    toolbarLayout->addWidget(copyBtn);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(statusLabel);
}

void RegExTest::setupInputArea()
{
    inputWidget = new QWidget();
    inputLayout = new QVBoxLayout(inputWidget);
    inputLayout->setContentsMargins(0, 0, 0, 0);
    inputLayout->setSpacing(10);
    
    // 正则表达式输入组
    regexGroup = new QGroupBox("🔤 正则表达式");
    QVBoxLayout* regexLayout = new QVBoxLayout(regexGroup);
    
    regexEdit = new QLineEdit();
    regexEdit->setPlaceholderText("输入正则表达式...");
    regexEdit->setFont(QFont("Consolas", 11));
    
    // 标志选项
    flagsLayout = new QHBoxLayout();
    caseSensitiveCheck = new QCheckBox("区分大小写");
    multilineCheck = new QCheckBox("多行模式");
    dotAllCheck = new QCheckBox("点匹配所有");
    extendedCheck = new QCheckBox("扩展模式");
    
    flagsLayout->addWidget(caseSensitiveCheck);
    flagsLayout->addWidget(multilineCheck);
    flagsLayout->addWidget(dotAllCheck);
    flagsLayout->addWidget(extendedCheck);
    flagsLayout->addStretch();
    
    regexLayout->addWidget(regexEdit);
    regexLayout->addLayout(flagsLayout);
    
    // 测试文本输入组
    textGroup = new QGroupBox("📝 测试文本");
    QVBoxLayout* textLayout = new QVBoxLayout(textGroup);
    
    testTextEdit = new QTextEdit();
    testTextEdit->setPlaceholderText("输入要测试的文本...");
    testTextEdit->setFont(QFont("Consolas", 11));
    
    textLayout->addWidget(testTextEdit);
    
    // 预设模式组
    setupPresetsArea();
    
    inputLayout->addWidget(regexGroup);
    inputLayout->addWidget(textGroup);
    inputLayout->addWidget(presetsGroup);
}

void RegExTest::setupResultsArea()
{
    resultsTabWidget = new QTabWidget();
    
    // 匹配结果标签页
    matchesTab = new QWidget();
    matchesLayout = new QVBoxLayout(matchesTab);
    
    matchesTable = new QTableWidget();
    matchesTable->setColumnCount(3);
    matchesTable->setHorizontalHeaderLabels(QStringList() << "匹配项" << "位置" << "长度");
    matchesTable->horizontalHeader()->setStretchLastSection(true);
    matchesTable->setAlternatingRowColors(true);
    matchesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    
    matchesLayout->addWidget(matchesTable);
    
    // 替换标签页
    replaceTab = new QWidget();
    replaceLayout = new QVBoxLayout(replaceTab);
    
    QHBoxLayout* replaceInputLayout = new QHBoxLayout();
    QLabel* replaceLabel = new QLabel("替换为:");
    replaceEdit = new QLineEdit();
    replaceEdit->setPlaceholderText("输入替换文本...");
    replaceBtn = new QPushButton("替换");
    replaceBtn->setFixedSize(60, 28);
    
    replaceInputLayout->addWidget(replaceLabel);
    replaceInputLayout->addWidget(replaceEdit);
    replaceInputLayout->addWidget(replaceBtn);
    
    replaceResultEdit = new QPlainTextEdit();
    replaceResultEdit->setReadOnly(true);
    replaceResultEdit->setPlaceholderText("替换结果将显示在这里...");
    replaceResultEdit->setFont(QFont("Consolas", 11));
    
    replaceLayout->addLayout(replaceInputLayout);
    replaceLayout->addWidget(replaceResultEdit);
    
    // 添加标签页
    resultsTabWidget->addTab(matchesTab, "🎯 匹配结果");
    resultsTabWidget->addTab(replaceTab, "🔄 替换结果");
}

void RegExTest::setupPresetsArea()
{
    presetsGroup = new QGroupBox("📚 常用模式");
    QHBoxLayout* presetsLayout = new QHBoxLayout(presetsGroup);
    
    presetsCombo = new QComboBox();
    presetsCombo->setMinimumWidth(200);
    
    loadPresetBtn = new QPushButton("加载");
    loadPresetBtn->setFixedSize(50, 28);
    
    presetsLayout->addWidget(presetsCombo);
    presetsLayout->addWidget(loadPresetBtn);
    presetsLayout->addStretch();
}

void RegExTest::onTestRegex()
{
    performRegexTest();
}

void RegExTest::onRegexChanged()
{
    testTimer->start(); // 延迟测试
}

void RegExTest::onTextChanged()
{
    testTimer->start(); // 延迟测试
}

void RegExTest::onClearResults()
{
    matchesTable->setRowCount(0);
    replaceResultEdit->clear();
    copyBtn->setEnabled(false);
    updateStatus("已清空结果", false);
}

void RegExTest::onCopyMatches()
{
    QString result;
    for (int i = 0; i < matchesTable->rowCount(); ++i) {
        QTableWidgetItem* item = matchesTable->item(i, 0);
        if (item) {
            result += item->text() + "\n";
        }
    }
    
    if (!result.isEmpty()) {
        QApplication::clipboard()->setText(result);
        updateStatus("匹配结果已复制到剪贴板", false);
    }
}

void RegExTest::onLoadPreset()
{
    QString preset = presetsCombo->currentData().toString();
    if (!preset.isEmpty()) {
        regexEdit->setText(preset);
        onTestRegex();
    }
}

void RegExTest::performRegexTest()
{
    QString pattern = regexEdit->text();
    QString text = testTextEdit->toPlainText();
    
    if (pattern.isEmpty()) {
        updateStatus("请输入正则表达式", true);
        return;
    }
    
    // 设置正则表达式选项
    QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
    if (!caseSensitiveCheck->isChecked()) {
        options |= QRegularExpression::CaseInsensitiveOption;
    }
    if (multilineCheck->isChecked()) {
        options |= QRegularExpression::MultilineOption;
    }
    if (dotAllCheck->isChecked()) {
        options |= QRegularExpression::DotMatchesEverythingOption;
    }
    if (extendedCheck->isChecked()) {
        options |= QRegularExpression::ExtendedPatternSyntaxOption;
    }
    
    currentRegex.setPattern(pattern);
    currentRegex.setPatternOptions(options);
    
    if (!currentRegex.isValid()) {
        updateStatus(QString("正则表达式错误: %1").arg(currentRegex.errorString()), true);
        return;
    }
    
    // 执行匹配
    QRegularExpressionMatchIterator iterator = currentRegex.globalMatch(text);
    matchesTable->setRowCount(0);
    
    int matchCount = 0;
    while (iterator.hasNext()) {
        QRegularExpressionMatch match = iterator.next();
        
        matchesTable->insertRow(matchCount);
        matchesTable->setItem(matchCount, 0, new QTableWidgetItem(match.captured(0)));
        matchesTable->setItem(matchCount, 1, new QTableWidgetItem(QString::number(match.capturedStart())));
        matchesTable->setItem(matchCount, 2, new QTableWidgetItem(QString::number(match.capturedLength())));
        
        matchCount++;
    }
    
    copyBtn->setEnabled(matchCount > 0);
    updateStatus(QString("找到 %1 个匹配项").arg(matchCount), false);
}

void RegExTest::loadCommonPresets()
{
    presetsCombo->addItem("邮箱地址", "\\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Z|a-z]{2,}\\b");
    presetsCombo->addItem("手机号码", "1[3-9]\\d{9}");
    presetsCombo->addItem("身份证号", "\\d{17}[\\dXx]");
    presetsCombo->addItem("IP地址", "\\b(?:[0-9]{1,3}\\.){3}[0-9]{1,3}\\b");
    presetsCombo->addItem("网址URL", "https?://[\\w\\-]+(\\.[\\w\\-]+)+([\\w\\-\\.,@?^=%&:/~\\+#]*[\\w\\-\\@?^=%&/~\\+#])?");
    presetsCombo->addItem("中文字符", "[\\u4e00-\\u9fa5]+");
    presetsCombo->addItem("数字", "\\d+");
    presetsCombo->addItem("英文单词", "\\b[A-Za-z]+\\b");
}

void RegExTest::updateStatus(const QString& message, bool isError)
{
    statusLabel->setText(message);
    if (isError) {
        statusLabel->setStyleSheet(R"(
            QLabel {
                color: #d32f2f; 
                font-weight: bold; 
                padding: 6px 12px; 
                background: #ffebee; 
                border-radius: 0px; 
                border: 1px solid #f8bbd9;
            }
        )");
    } else {
        statusLabel->setStyleSheet(R"(
            QLabel {
                color: #2e7d32; 
                font-weight: bold; 
                padding: 6px 12px; 
                background: #e8f5e8; 
                border-radius: 0px; 
                border: 1px solid #c8e6c9;
            }
        )");
    }
}

#include "regextest.moc"