#include "regexcontentgen.h"

REGISTER_DYNAMICOBJECT(RegExContentGen);

// 正则表达式语法高亮器实现
RegexSyntaxHighlighter::RegexSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    HighlightingRule rule;

    // 字符类 [...]
    QTextCharFormat charClassFormat;
    charClassFormat.setForeground(QColor("#2E7D32")); // 绿色
    charClassFormat.setFontWeight(QFont::Bold);
    rule.pattern = QRegularExpression("\\[[^\\]]*\\]");
    rule.format = charClassFormat;
    highlightingRules.append(rule);

    // 量词 +, *, ?, {n,m}
    QTextCharFormat quantifierFormat;
    quantifierFormat.setForeground(QColor("#1976D2")); // 蓝色
    quantifierFormat.setFontWeight(QFont::Bold);
    rule.pattern = QRegularExpression("[+*?]|\\{\\d+(,\\d*)?\\}");
    rule.format = quantifierFormat;
    highlightingRules.append(rule);

    // 分组 (...)
    QTextCharFormat groupFormat;
    groupFormat.setForeground(QColor("#7B1FA2")); // 紫色
    rule.pattern = QRegularExpression("\\([^)]*\\)");
    rule.format = groupFormat;
    highlightingRules.append(rule);

    // 转义字符 \d, \w, \s 等
    QTextCharFormat escapeFormat;
    escapeFormat.setForeground(QColor("#F57C00")); // 橙色
    escapeFormat.setFontWeight(QFont::Bold);
    rule.pattern = QRegularExpression("\\\\[dwsWDSntrf]|\\\\.");
    rule.format = escapeFormat;
    highlightingRules.append(rule);

    // 锚点 ^, $
    QTextCharFormat anchorFormat;
    anchorFormat.setForeground(QColor("#D32F2F")); // 红色
    anchorFormat.setFontWeight(QFont::Bold);
    rule.pattern = QRegularExpression("[\\^$]");
    rule.format = anchorFormat;
    highlightingRules.append(rule);

    // 选择 |
    QTextCharFormat alternationFormat;
    alternationFormat.setForeground(QColor("#388E3C")); // 深绿色
    alternationFormat.setFontWeight(QFont::Bold);
    rule.pattern = QRegularExpression("\\|");
    rule.format = alternationFormat;
    highlightingRules.append(rule);
}

void RegexSyntaxHighlighter::highlightBlock(const QString &text)
{
    foreach (const HighlightingRule &rule, highlightingRules) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
}

// 主类实现
RegExContentGen::RegExContentGen() : QWidget(nullptr), DynamicObjectBase()
{
    generateTimer = new QTimer(this);
    generateTimer->setSingleShot(true);
    
    setupUI();
    initializePresets();
    
    // 连接信号槽
    connect(generateBtn, &QPushButton::clicked, this, &RegExContentGen::onGenerateContent);
    connect(clearBtn, &QPushButton::clicked, this, &RegExContentGen::onClearAll);
    connect(copyBtn, &QPushButton::clicked, this, &RegExContentGen::onCopyResults);
    connect(exportBtn, &QPushButton::clicked, this, &RegExContentGen::onExportResults);
    connect(importBtn, &QPushButton::clicked, this, &RegExContentGen::onImportRegex);
    connect(presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RegExContentGen::onPresetSelected);
    connect(regexInput, &QLineEdit::textChanged, this, &RegExContentGen::onRegexChanged);
    connect(countSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &RegExContentGen::onGenerateCountChanged);
    
    // 设置初始状态
    onRegexChanged();
}

RegExContentGen::~RegExContentGen()
{
}

void RegExContentGen::setupUI()
{
    mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    
    // 创建分割器
    mainSplitter = new QSplitter(Qt::Horizontal);
    
    setupInputArea();
    setupOptionsArea();
    setupResultsArea();
    setupPresetArea();
    setupStatusArea();
    
    // 左侧面板（输入和选项）
    QWidget *leftPanel = new QWidget();
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->addWidget(inputGroup);
    leftLayout->addWidget(optionsGroup);
    leftLayout->addWidget(presetGroup);
    leftLayout->addStretch();
    
    // 右侧面板（结果）
    QWidget *rightPanel = new QWidget();
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->addWidget(resultsGroup);
    
    mainSplitter->addWidget(leftPanel);
    mainSplitter->addWidget(rightPanel);
    mainSplitter->setStretchFactor(0, 0);
    mainSplitter->setStretchFactor(1, 1);
    mainSplitter->setSizes({400, 600});
    
    mainLayout->addWidget(mainSplitter);
    mainLayout->addLayout(statusLayout);
}

void RegExContentGen::setupInputArea()
{
    inputGroup = new QGroupBox(tr("🎯 正则表达式输入"));
    inputLayout = new QVBoxLayout(inputGroup);
    
    regexLabel = new QLabel(tr("输入正则表达式:"));
    regexLabel->setStyleSheet("font-weight: bold; color: #495057;");
    
    regexInput = new QLineEdit();
    regexInput->setPlaceholderText(tr("例如: [a-zA-Z]{3,8}@[a-z]{2,6}\\.[a-z]{2,3}"));
    regexInput->setMinimumHeight(40);
    
    statusLabel = new QLabel(tr("请输入正则表达式"));
    statusLabel->setStyleSheet("color: #6c757d; font-size: 11pt; padding: 4px;");
    
    inputLayout->addWidget(regexLabel);
    inputLayout->addWidget(regexInput);
    inputLayout->addWidget(statusLabel);
}

void RegExContentGen::setupOptionsArea()
{
    optionsGroup = new QGroupBox(tr("⚙️ 生成选项"));
    optionsLayout = new QGridLayout(optionsGroup);
    
    // 生成数量
    optionsLayout->addWidget(new QLabel(tr("生成数量:")), 0, 0);
    countSpinBox = new QSpinBox();
    countSpinBox->setRange(1, 1000);
    countSpinBox->setValue(10);
    countSpinBox->setSuffix(tr(" 个"));
    optionsLayout->addWidget(countSpinBox, 0, 1);
    
    // 最大长度限制
    optionsLayout->addWidget(new QLabel(tr("最大长度:")), 1, 0);
    maxLengthSpinBox = new QSpinBox();
    maxLengthSpinBox->setRange(1, 1000);
    maxLengthSpinBox->setValue(50);
    maxLengthSpinBox->setSuffix(tr(" 字符"));
    optionsLayout->addWidget(maxLengthSpinBox, 1, 1);
    
    // 选项复选框
    uniqueCheckBox = new QCheckBox(tr("生成唯一结果"));
    uniqueCheckBox->setChecked(true);
    optionsLayout->addWidget(uniqueCheckBox, 2, 0, 1, 2);
    
    caseSensitiveCheckBox = new QCheckBox(tr("区分大小写"));
    caseSensitiveCheckBox->setChecked(true);
    optionsLayout->addWidget(caseSensitiveCheckBox, 3, 0, 1, 2);
    
    // 操作按钮
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    generateBtn = new QPushButton(tr("🎲 生成内容"));
    generateBtn->setStyleSheet("QPushButton { background-color: #28a745; color: white; font-weight: bold; } QPushButton:hover { background-color: #218838; }");
    
    clearBtn = new QPushButton(tr("🧹 清空"));
    clearBtn->setStyleSheet("QPushButton { background-color: #dc3545; color: white; } QPushButton:hover { background-color: #c82333; }");
    
    buttonLayout->addWidget(generateBtn);
    buttonLayout->addWidget(clearBtn);
    optionsLayout->addLayout(buttonLayout, 4, 0, 1, 2);
}

void RegExContentGen::setupResultsArea()
{
    resultsGroup = new QGroupBox(tr("📋 生成结果"));
    resultsLayout = new QVBoxLayout(resultsGroup);
    
    resultsText = new QTextEdit();
    resultsText->setPlaceholderText(tr("生成的内容将显示在这里..."));
    resultsText->setMinimumHeight(300);
    resultsText->setFont(QFont("Consolas", 10));
    
    // 结果操作按钮
    resultsButtonLayout = new QHBoxLayout();
    
    copyBtn = new QPushButton(tr("📋 复制结果"));
    copyBtn->setStyleSheet("QPushButton { background-color: #007bff; color: white; } QPushButton:hover { background-color: #0056b3; }");
    copyBtn->setEnabled(false);
    
    exportBtn = new QPushButton(tr("💾 导出结果"));
    exportBtn->setStyleSheet("QPushButton { background-color: #17a2b8; color: white; } QPushButton:hover { background-color: #138496; }");
    exportBtn->setEnabled(false);
    
    resultsButtonLayout->addWidget(copyBtn);
    resultsButtonLayout->addWidget(exportBtn);
    resultsButtonLayout->addStretch();
    
    resultsLayout->addWidget(resultsText);
    resultsLayout->addLayout(resultsButtonLayout);
}

void RegExContentGen::setupPresetArea()
{
    presetGroup = new QGroupBox(tr("📚 预设模式"));
    presetLayout = new QVBoxLayout(presetGroup);
    
    presetCombo = new QComboBox();
    presetCombo->addItem(tr("选择预设模式..."));
    
    presetList = new QListWidget();
    presetList->setMaximumHeight(120);
    
    importBtn = new QPushButton(tr("📁 导入正则"));
    importBtn->setStyleSheet("QPushButton { background-color: #6f42c1; color: white; } QPushButton:hover { background-color: #5a32a3; }");
    
    presetLayout->addWidget(new QLabel(tr("快速选择:")));
    presetLayout->addWidget(presetCombo);
    presetLayout->addWidget(new QLabel(tr("常用模式:")));
    presetLayout->addWidget(presetList);
    presetLayout->addWidget(importBtn);
}

void RegExContentGen::setupStatusArea()
{
    statusLayout = new QHBoxLayout();
    
    generatedCountLabel = new QLabel(tr("已生成: 0 个"));
    generatedCountLabel->setStyleSheet("color: #28a745; font-weight: bold;");
    
    progressBar = new QProgressBar();
    progressBar->setVisible(false);
    progressBar->setMaximumHeight(20);
    progressBar->setMaximumWidth(200);
    
    statusLayout->addWidget(generatedCountLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(progressBar);
}

void RegExContentGen::initializePresets()
{
    // 添加常用的正则表达式预设
    addPreset(tr("邮箱地址"), "[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}", tr("生成邮箱地址格式"));
    addPreset(tr("手机号码"), "1[3-9]\\d{9}", tr("生成中国手机号码"));
    addPreset(tr("身份证号"), "[1-9]\\d{5}(18|19|20)\\d{2}(0[1-9]|1[0-2])(0[1-9]|[12]\\d|3[01])\\d{3}[0-9Xx]", tr("生成身份证号码"));
    addPreset(tr("IPv4地址"), "((25[0-5]|2[0-4]\\d|[01]?\\d\\d?)\\.){3}(25[0-5]|2[0-4]\\d|[01]?\\d\\d?)", tr("生成IPv4地址"));
    addPreset(tr("MAC地址"), "([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})", tr("生成MAC地址"));
    addPreset(tr("URL链接"), "https?://[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}(/[a-zA-Z0-9._~:/?#\\[\\]@!$&'()*+,;=-]*)?", tr("生成URL链接"));
    addPreset(tr("用户名"), "[a-zA-Z][a-zA-Z0-9_]{3,15}", tr("生成用户名"));
    addPreset(tr("密码"), "(?=.*[a-z])(?=.*[A-Z])(?=.*\\d)[a-zA-Z\\d@$!%*?&]{8,}", tr("生成强密码"));
    addPreset(tr("日期格式"), "\\d{4}-(0[1-9]|1[0-2])-(0[1-9]|[12]\\d|3[01])", tr("生成YYYY-MM-DD格式日期"));
    addPreset(tr("时间格式"), "(0[0-9]|1[0-9]|2[0-3]):[0-5][0-9]:[0-5][0-9]", tr("生成HH:MM:SS格式时间"));
    addPreset(tr("颜色代码"), "#[0-9A-Fa-f]{6}", tr("生成十六进制颜色代码"));
    addPreset(tr("中文姓名"), "[\\u4e00-\\u9fa5]{2,4}", tr("生成中文姓名"));
    
    // 填充下拉框和列表
    for (const RegexPreset &preset : presets) {
        presetCombo->addItem(preset.name);
        
        QListWidgetItem *item = new QListWidgetItem(preset.name);
        item->setToolTip(preset.description + "\n正则: " + preset.regex);
        presetList->addItem(item);
    }
    
    // 连接列表点击事件
    connect(presetList, &QListWidget::itemClicked, [this](QListWidgetItem *item) {
        int index = presetList->row(item);
        if (index >= 0 && index < presets.size()) {
            regexInput->setText(presets[index].regex);
            statusLabel->setText(tr("已加载预设: %1").arg(presets[index].description));
        }
    });
}

void RegExContentGen::addPreset(const QString &name, const QString &regex, const QString &description)
{
    RegexPreset preset;
    preset.name = name;
    preset.regex = regex;
    preset.description = description;
    presets.append(preset);
}

void RegExContentGen::onGenerateContent()
{
    QString regex = regexInput->text().trimmed();
    if (regex.isEmpty()) {
        statusLabel->setText(tr("请输入正则表达式"));
        statusLabel->setStyleSheet("color: #dc3545; font-weight: bold;");
        return;
    }
    
    int count = countSpinBox->value();
    progressBar->setVisible(true);
    progressBar->setRange(0, count);
    progressBar->setValue(0);
    
    generateBtn->setEnabled(false);
    statusLabel->setText(tr("正在生成内容..."));
    statusLabel->setStyleSheet("color: #007bff; font-weight: bold;");
    
    try {
        QStringList results = generateMultipleFromRegex(regex, count);
        
        if (results.isEmpty()) {
            statusLabel->setText(tr("无法从该正则表达式生成内容"));
            statusLabel->setStyleSheet("color: #dc3545; font-weight: bold;");
        } else {
            generatedResults = results;
            resultsText->clear();
            
            for (int i = 0; i < results.size(); ++i) {
                resultsText->append(QString("%1. %2").arg(i + 1, 3, 10, QChar('0')).arg(results[i]));
            }
            
            generatedCountLabel->setText(tr("已生成: %1 个").arg(results.size()));
            statusLabel->setText(tr("成功生成 %1 个结果").arg(results.size()));
            statusLabel->setStyleSheet("color: #28a745; font-weight: bold;");
            
            copyBtn->setEnabled(true);
            exportBtn->setEnabled(true);
        }
    } catch (const std::exception &e) {
        statusLabel->setText(tr("生成失败: %1").arg(e.what()));
        statusLabel->setStyleSheet("color: #dc3545; font-weight: bold;");
    }
    
    progressBar->setVisible(false);
    generateBtn->setEnabled(true);
}

QStringList RegExContentGen::generateMultipleFromRegex(const QString &regex, int count)
{
    QStringList results;
    QSet<QString> uniqueResults;
    bool needUnique = uniqueCheckBox->isChecked();
    int maxLength = maxLengthSpinBox->value();
    
    int attempts = 0;
    int maxAttempts = count * 10; // 最多尝试10倍的次数
    
    while (results.size() < count && attempts < maxAttempts) {
        QString generated = generateFromRegex(regex);
        
        if (!generated.isEmpty() && generated.length() <= maxLength) {
            if (needUnique) {
                if (!uniqueResults.contains(generated)) {
                    uniqueResults.insert(generated);
                    results.append(generated);
                }
            } else {
                results.append(generated);
            }
        }
        
        attempts++;
        progressBar->setValue(results.size());
        QApplication::processEvents();
    }
    
    return results;
}

QString RegExContentGen::generateFromRegex(const QString &regex)
{
    QString result;
    QString workingRegex = regex;
    
    // 移除锚点
    workingRegex.remove("^").remove("$");
    
    // 处理选择 | (在其他处理之前)
    if (workingRegex.contains("|")) {
        QStringList alternatives = workingRegex.split("|");
        if (!alternatives.isEmpty()) {
            int index = QRandomGenerator::global()->bounded(alternatives.size());
            workingRegex = alternatives[index].trimmed();
        }
    }
    
    // 递归处理整个表达式
    result = processRegexPattern(workingRegex);
    
    return result;
}

QString RegExContentGen::processRegexPattern(const QString &pattern)
{
    QString result;
    QString workingPattern = pattern;
    int i = 0;
    
    while (i < workingPattern.length()) {
        QChar currentChar = workingPattern[i];
        
        if (currentChar == '\\' && i + 1 < workingPattern.length()) {
            // 处理转义字符
            QChar nextChar = workingPattern[i + 1];
            if (nextChar == 'd') {
                result += QString::number(QRandomGenerator::global()->bounded(10));
            } else if (nextChar == 'w') {
                result += generateRandomString("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_", 1, 1);
            } else if (nextChar == 's') {
                result += " ";
            } else if (nextChar == 'D') {
                result += generateRandomString("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()_+-=[]{}|;:,.<>?", 1, 1);
            } else if (nextChar == 'W') {
                result += generateRandomString("!@#$%^&*()_+-=[]{}|;:,.<>? ", 1, 1);
            } else if (nextChar == 'S') {
                result += generateRandomString("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", 1, 1);
            } else {
                // 其他转义字符，直接输出字符本身
                result += nextChar;
            }
            i += 2;
        } else if (currentChar == '[') {
            // 处理字符类
            int closePos = workingPattern.indexOf(']', i + 1);
            if (closePos != -1) {
                QString charClass = workingPattern.mid(i + 1, closePos - i - 1);
                
                // 检查后面是否有量词
                QString quantifier;
                int nextPos = closePos + 1;
                if (nextPos < workingPattern.length()) {
                    QChar nextChar = workingPattern[nextPos];
                    if (nextChar == '+' || nextChar == '*' || nextChar == '?') {
                        quantifier = nextChar;
                        i = nextPos + 1;
                    } else if (nextChar == '{') {
                        int quantEnd = workingPattern.indexOf('}', nextPos);
                        if (quantEnd != -1) {
                            quantifier = workingPattern.mid(nextPos, quantEnd - nextPos + 1);
                            i = quantEnd + 1;
                        } else {
                            i = closePos + 1;
                        }
                    } else {
                        i = closePos + 1;
                    }
                } else {
                    i = closePos + 1;
                }
                
                QString baseChar = processCharacterClass(charClass);
                if (!quantifier.isEmpty()) {
                    result += processQuantifier(baseChar, quantifier);
                } else {
                    result += baseChar;
                }
            } else {
                result += currentChar;
                i++;
            }
        } else if (currentChar == '(' && !workingPattern.mid(i).startsWith("(?")) {
            // 处理分组 (但不处理非捕获组等特殊语法)
            int level = 1;
            int groupStart = i + 1;
            int j = i + 1;
            
            while (j < workingPattern.length() && level > 0) {
                if (workingPattern[j] == '(') level++;
                else if (workingPattern[j] == ')') level--;
                j++;
            }
            
            if (level == 0) {
                QString groupContent = workingPattern.mid(groupStart, j - groupStart - 1);
                
                // 检查后面是否有量词
                QString quantifier;
                if (j < workingPattern.length()) {
                    QChar nextChar = workingPattern[j];
                    if (nextChar == '+' || nextChar == '*' || nextChar == '?') {
                        quantifier = nextChar;
                        i = j + 1;
                    } else if (nextChar == '{') {
                        int quantEnd = workingPattern.indexOf('}', j);
                        if (quantEnd != -1) {
                            quantifier = workingPattern.mid(j, quantEnd - j + 1);
                            i = quantEnd + 1;
                        } else {
                            i = j;
                        }
                    } else {
                        i = j;
                    }
                } else {
                    i = j;
                }
                
                QString groupResult = processRegexPattern(groupContent);
                if (!quantifier.isEmpty()) {
                    result += processQuantifier(groupResult, quantifier);
                } else {
                    result += groupResult;
                }
            } else {
                result += currentChar;
                i++;
            }
        } else if (i + 1 < workingPattern.length() && 
                   (workingPattern[i + 1] == '+' || workingPattern[i + 1] == '*' || 
                    workingPattern[i + 1] == '?' || workingPattern[i + 1] == '{')) {
            // 处理单个字符后跟量词
            QString baseChar = QString(currentChar);
            QString quantifier;
            
            if (workingPattern[i + 1] == '{') {
                int quantEnd = workingPattern.indexOf('}', i + 1);
                if (quantEnd != -1) {
                    quantifier = workingPattern.mid(i + 1, quantEnd - i);
                    i = quantEnd + 1;
                } else {
                    result += currentChar;
                    i++;
                }
            } else {
                quantifier = workingPattern[i + 1];
                i += 2;
            }
            
            result += processQuantifier(baseChar, quantifier);
        } else {
            // 普通字符
            result += currentChar;
            i++;
        }
    }
    
    return result;
}

QString RegExContentGen::processCharacterClass(const QString &charClass)
{
    QString charset;
    
    // 处理范围 a-z, A-Z, 0-9
    QRegularExpression rangeRegex("([a-zA-Z0-9])\\-([a-zA-Z0-9])");
    QRegularExpressionMatchIterator rangeIter = rangeRegex.globalMatch(charClass);
    
    QString processedClass = charClass;
    while (rangeIter.hasNext()) {
        QRegularExpressionMatch match = rangeIter.next();
        QChar start = match.captured(1)[0];
        QChar end = match.captured(2)[0];
        
        QString range;
        for (int i = start.unicode(); i <= end.unicode(); ++i) {
            range += QChar(i);
        }
        
        processedClass.replace(match.captured(0), range);
    }
    
    charset = processedClass;
    
    if (charset.isEmpty()) {
        return "";
    }
    
    int index = QRandomGenerator::global()->bounded(charset.length());
    return QString(charset[index]);
}

QString RegExContentGen::processQuantifier(const QString &base, const QString &quantifier)
{
    int min = 1, max = 1;
    
    if (quantifier == "+") {
        min = 1; max = 5;
    } else if (quantifier == "*") {
        min = 0; max = 5;
    } else if (quantifier == "?") {
        min = 0; max = 1;
    } else if (quantifier.startsWith("{")) {
        QRegularExpression quantRegex("\\{(\\d+)(,(\\d*))?\\}");
        QRegularExpressionMatch match = quantRegex.match(quantifier);
        if (match.hasMatch()) {
            min = match.captured(1).toInt();
            if (match.captured(2).isEmpty()) {
                max = min;
            } else if (match.captured(3).isEmpty()) {
                max = min + 5; // 如果没有指定上限，使用min+5
            } else {
                max = match.captured(3).toInt();
            }
        }
    }
    
    int count = QRandomGenerator::global()->bounded(max - min + 1) + min;
    return base.repeated(count);
}

QString RegExContentGen::generateRandomString(const QString &charset, int minLen, int maxLen)
{
    int length = QRandomGenerator::global()->bounded(maxLen - minLen + 1) + minLen;
    QString result;
    
    for (int i = 0; i < length; ++i) {
        int index = QRandomGenerator::global()->bounded(charset.length());
        result += charset[index];
    }
    
    return result;
}

void RegExContentGen::onClearAll()
{
    resultsText->clear();
    generatedResults.clear();
    generatedCountLabel->setText(tr("已生成: 0 个"));
    copyBtn->setEnabled(false);
    exportBtn->setEnabled(false);
    statusLabel->setText(tr("已清空结果"));
    statusLabel->setStyleSheet("color: #6c757d; font-size: 11pt;");
}

void RegExContentGen::onCopyResults()
{
    if (generatedResults.isEmpty()) {
        return;
    }
    
    QString text = generatedResults.join("\n");
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(text);
    
    statusLabel->setText(tr("结果已复制到剪贴板"));
    statusLabel->setStyleSheet("color: #28a745; font-weight: bold;");
}

void RegExContentGen::onPresetSelected()
{
    int index = presetCombo->currentIndex() - 1; // 减1因为第一项是提示文本
    if (index >= 0 && index < presets.size()) {
        regexInput->setText(presets[index].regex);
        statusLabel->setText(tr("已加载预设: %1").arg(presets[index].description));
        statusLabel->setStyleSheet("color: #007bff; font-weight: bold;");
        presetCombo->setCurrentIndex(0); // 重置为提示文本
    }
}

void RegExContentGen::onRegexChanged()
{
    QString regex = regexInput->text().trimmed();
    if (regex.isEmpty()) {
        statusLabel->setText(tr("请输入正则表达式"));
        statusLabel->setStyleSheet("color: #6c757d; font-size: 11pt;");
        generateBtn->setEnabled(false);
    } else {
        // 简单验证正则表达式
        QRegularExpression testRegex(regex);
        if (testRegex.isValid()) {
            statusLabel->setText(tr("正则表达式有效，可以生成内容"));
            statusLabel->setStyleSheet("color: #28a745; font-weight: bold;");
            generateBtn->setEnabled(true);
        } else {
            statusLabel->setText(tr("正则表达式语法错误: %1").arg(testRegex.errorString()));
            statusLabel->setStyleSheet("color: #dc3545; font-weight: bold;");
            generateBtn->setEnabled(false);
        }
    }
}

void RegExContentGen::onGenerateCountChanged()
{
    int count = countSpinBox->value();
    if (count > 100) {
        statusLabel->setText(tr("生成大量数据可能需要较长时间"));
        statusLabel->setStyleSheet("color: #ffc107; font-weight: bold;");
    }
}

void RegExContentGen::onExportResults()
{
    if (generatedResults.isEmpty()) {
        return;
    }
    
    QString dateTimeStr = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString fileName = QString("regex_generated_%1.txt").arg(dateTimeStr);
    
    // 这里可以添加文件保存对话框
    // 暂时显示提示信息
    QMessageBox::information(this, tr("导出结果"), 
                           tr("将导出 %1 个结果到文件: %2")
                           .arg(generatedResults.size())
                           .arg(fileName));
}

void RegExContentGen::onImportRegex()
{
    // 这里可以添加从文件导入正则表达式的功能
    // 暂时显示提示信息
    QMessageBox::information(this, tr("导入正则"), tr("导入正则表达式功能待实现"));
}

#include "regexcontentgen.moc"
