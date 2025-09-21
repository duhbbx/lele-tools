#include "randompasswordgen.h"
#include "../../common/global-styles.h"

#include <QDebug>
#include <QFont>
#include <QFontMetrics>
#include <QHeaderView>
#include <QListWidgetItem>
#include <QDateTime>
#include <algorithm>
#include <set>

REGISTER_DYNAMICOBJECT(RandomPasswordGen);

RandomPasswordGen::RandomPasswordGen() : QWidget(nullptr), DynamicObjectBase()
{
    setupUI();
    
    // 连接信号槽
    connect(generateBtn, &QPushButton::clicked, this, &RandomPasswordGen::onGenerateClicked);
    connect(copyBtn, &QPushButton::clicked, this, &RandomPasswordGen::onCopyClicked);
    connect(clearBtn, &QPushButton::clicked, this, &RandomPasswordGen::onClearClicked);
    
    connect(lengthSlider, &QSlider::valueChanged, lengthSpinBox, &QSpinBox::setValue);
    connect(lengthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), lengthSlider, &QSlider::setValue);
    connect(lengthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &RandomPasswordGen::onLengthChanged);
    
    connect(countSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &RandomPasswordGen::onPasswordCountChanged);
    
    // 字符集变化信号
    connect(includeDigits, &QCheckBox::toggled, this, &RandomPasswordGen::onCharsetChanged);
    connect(includeLowercase, &QCheckBox::toggled, this, &RandomPasswordGen::onCharsetChanged);
    connect(includeUppercase, &QCheckBox::toggled, this, &RandomPasswordGen::onCharsetChanged);
    connect(includeSpecialChars, &QCheckBox::toggled, this, &RandomPasswordGen::onCharsetChanged);
    connect(includeCustomChars, &QCheckBox::toggled, this, &RandomPasswordGen::onCharsetChanged);
    connect(customCharsEdit, &QLineEdit::textChanged, this, &RandomPasswordGen::onCharsetChanged);
    
    connect(passwordList, &QListWidget::itemClicked, this, &RandomPasswordGen::onPasswordItemClicked);
    
    // 设置默认值
    includeDigits->setChecked(true);
    includeLowercase->setChecked(true);
    includeUppercase->setChecked(true);
    lengthSpinBox->setValue(12);
    countSpinBox->setValue(5);
    
    onCharsetChanged();
    updatePasswordStrength();
}

RandomPasswordGen::~RandomPasswordGen()
{
}

void RandomPasswordGen::setupUI()
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
    
    // 设置左侧设置面板
    setupCharsetOptions();
    setupPasswordSettings();
    
    // 设置右侧密码显示
    setupPasswordDisplay();
    
    // 添加到分割器
    mainSplitter->addWidget(settingsScrollArea);
    mainSplitter->addWidget(displayWidget);
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 2);
    
    // 添加到主布局
    mainLayout->addWidget(toolbarWidget);
    mainLayout->addWidget(mainSplitter, 1); // 添加stretch factor，让splitter占据剩余空间
}

void RandomPasswordGen::setupToolbar()
{
    toolbarWidget = new QWidget();
    toolbarWidget->setFixedHeight(45);
    toolbarLayout = new QHBoxLayout(toolbarWidget);
    toolbarLayout->setContentsMargins(5, 5, 5, 5);
    toolbarLayout->setSpacing(8);
    
    generateBtn = new QPushButton(tr("🎲 生成密码"));
    generateBtn->setToolTip(tr("生成随机密码"));
    generateBtn->setFixedSize(110, 32);
    
    copyBtn = new QPushButton(tr("📋 复制"));
    copyBtn->setToolTip(tr("复制选中的密码到剪贴板"));
    copyBtn->setFixedSize(75, 32);
    copyBtn->setEnabled(false);
    
    clearBtn = new QPushButton(tr("🗑️ 清空"));
    clearBtn->setToolTip(tr("清空所有生成的密码"));
    clearBtn->setFixedSize(75, 32);
    
    statusLabel = new QLabel(tr("就绪"));
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
    
    toolbarLayout->addWidget(generateBtn);
    toolbarLayout->addWidget(copyBtn);
    toolbarLayout->addWidget(clearBtn);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(statusLabel);
}

void RandomPasswordGen::setupCharsetOptions()
{
    settingsWidget = new QWidget();
    settingsLayout = new QVBoxLayout(settingsWidget);
    settingsLayout->setContentsMargins(0, 0, 0, 0);
    settingsLayout->setSpacing(15);
    
    // 字符集选项组
    charsetGroup = new QGroupBox(tr("🔤 字符集选项"));
    QVBoxLayout* charsetLayout = new QVBoxLayout(charsetGroup);
    charsetLayout->setSpacing(8);
    
    includeDigits = new QCheckBox(tr("数字 (0-9)"));
    includeLowercase = new QCheckBox(tr("小写字母 (a-z)"));
    includeUppercase = new QCheckBox(tr("大写字母 (A-Z)"));
    includeSpecialChars = new QCheckBox(tr("特殊字符 (!@#$%^&*...)"));
    includeCustomChars = new QCheckBox(tr("自定义字符:"));
    
    customCharsEdit = new QLineEdit();
    customCharsEdit->setPlaceholderText(tr("输入自定义字符..."));
    customCharsEdit->setEnabled(false);
    
    connect(includeCustomChars, &QCheckBox::toggled, customCharsEdit, &QLineEdit::setEnabled);
    
    charsetLayout->addWidget(includeDigits);
    charsetLayout->addWidget(includeLowercase);
    charsetLayout->addWidget(includeUppercase);
    charsetLayout->addWidget(includeSpecialChars);
    charsetLayout->addWidget(includeCustomChars);
    charsetLayout->addWidget(customCharsEdit);
    
    settingsLayout->addWidget(charsetGroup);
}

void RandomPasswordGen::setupPasswordSettings()
{
    // 密码设置组
    passwordGroup = new QGroupBox(tr("⚙️ 密码设置"));
    QVBoxLayout* passwordLayout = new QVBoxLayout(passwordGroup);
    passwordLayout->setSpacing(12);
    
    // 密码长度
    QHBoxLayout* lengthLayout = new QHBoxLayout();
    lengthLabel = new QLabel(tr("密码长度:"));
    lengthSlider = new QSlider(Qt::Horizontal);
    lengthSlider->setRange(4, 128);
    lengthSlider->setValue(12);
    lengthSpinBox = new QSpinBox();
    lengthSpinBox->setRange(4, 128);
    lengthSpinBox->setValue(12);
    lengthSpinBox->setFixedWidth(60);
    
    lengthLayout->addWidget(lengthLabel);
    lengthLayout->addWidget(lengthSlider);
    lengthLayout->addWidget(lengthSpinBox);
    
    // 生成数量
    QHBoxLayout* countLayout = new QHBoxLayout();
    countLabel = new QLabel(tr("生成数量:"));
    countSpinBox = new QSpinBox();
    countSpinBox->setRange(1, 100);
    countSpinBox->setValue(5);
    countSpinBox->setFixedWidth(60);
    
    countLayout->addWidget(countLabel);
    countLayout->addStretch();
    countLayout->addWidget(countSpinBox);
    
    passwordLayout->addLayout(lengthLayout);
    passwordLayout->addLayout(countLayout);
    
    // 高级选项组
    advancedGroup = new QGroupBox(tr("🔧 高级选项"));
    QVBoxLayout* advancedLayout = new QVBoxLayout(advancedGroup);
    
    avoidAmbiguous = new QCheckBox(tr("避免易混淆字符 (0OIl1)"));
    requireAllTypes = new QCheckBox(tr("必须包含所有选中类型"));
    noRepeatingChars = new QCheckBox(tr("不重复字符"));
    
    advancedLayout->addWidget(avoidAmbiguous);
    advancedLayout->addWidget(requireAllTypes);
    advancedLayout->addWidget(noRepeatingChars);
    
    // 密码强度显示组
    strengthGroup = new QGroupBox(tr("💪 密码强度"));
    QVBoxLayout* strengthLayout = new QVBoxLayout(strengthGroup);
    
    strengthBar = new QProgressBar();
    strengthBar->setRange(0, 100);
    strengthBar->setValue(0);
    strengthBar->setFixedHeight(20);
    
    strengthLabel = new QLabel(tr("强度: 未知"));
    strengthLabel->setAlignment(Qt::AlignCenter);
    strengthLabel->setStyleSheet("font-weight: bold;");
    
    strengthDescription = new QLabel(tr("请选择字符集并设置密码长度"));
    strengthDescription->setAlignment(Qt::AlignCenter);
    strengthDescription->setStyleSheet("color: #666; font-size: 10pt;");
    strengthDescription->setWordWrap(true);
    
    strengthLayout->addWidget(strengthBar);
    strengthLayout->addWidget(strengthLabel);
    strengthLayout->addWidget(strengthDescription);
    
    passwordLayout->addWidget(advancedGroup);
    passwordLayout->addWidget(strengthGroup);
    
    settingsLayout->addWidget(passwordGroup);
    settingsLayout->addStretch();
    
    // 创建滚动区域
    settingsScrollArea = new QScrollArea();
    settingsScrollArea->setWidget(settingsWidget);
    settingsScrollArea->setWidgetResizable(true);
    settingsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    settingsScrollArea->setMinimumWidth(350);
}

void RandomPasswordGen::setupPasswordDisplay()
{
    displayWidget = new QWidget();
    displayLayout = new QVBoxLayout(displayWidget);
    displayLayout->setContentsMargins(0, 0, 0, 0);
    displayLayout->setSpacing(10);
    
    displayLabel = new QLabel(tr("🔐 生成的密码"));
    displayLabel->setStyleSheet("font-weight: bold; font-size: 12pt; color: #333;");
    
    passwordList = new QListWidget();
    passwordList->setAlternatingRowColors(true);
    passwordList->setSelectionMode(QAbstractItemView::SingleSelection);
    
    // 预览区域
    previewGroup = new QGroupBox(tr("👁️ 密码预览"));
    QVBoxLayout* previewLayout = new QVBoxLayout(previewGroup);
    
    previewEdit = new QPlainTextEdit();
    previewEdit->setReadOnly(true);
    previewEdit->setMaximumHeight(100);
    previewEdit->setPlaceholderText(tr("点击密码列表中的项目查看详细信息..."));
    
    previewLayout->addWidget(previewEdit);
    
    displayLayout->addWidget(displayLabel);
    displayLayout->addWidget(passwordList, 2); // 密码列表占主要空间
    displayLayout->addWidget(previewGroup, 0); // 预览区域固定大小
}

void RandomPasswordGen::onGenerateClicked()
{
    QString charset = getCharacterSet();
    if (charset.isEmpty()) {
        updateStatus(tr("请至少选择一种字符类型"), true);
        return;
    }
    
    int length = lengthSpinBox->value();
    int count = countSpinBox->value();
    
    generatedPasswords.clear();
    passwordList->clear();
    previewEdit->clear();
    
    for (int i = 0; i < count; ++i) {
        QString password = generatePassword(length);
        generatedPasswords.append(password);
        
        QListWidgetItem* item = new QListWidgetItem(password);
        item->setToolTip(tr("长度: %1").arg(length));
        passwordList->addItem(item);
    }
    
    copyBtn->setEnabled(!generatedPasswords.isEmpty());
    updateStatus(tr("成功生成 %1 个密码").arg(count), false);
}

void RandomPasswordGen::onCopyClicked()
{
    QListWidgetItem* currentItem = passwordList->currentItem();
    if (currentItem) {
        QString password = currentItem->text();
        QApplication::clipboard()->setText(password);
        updateStatus(tr("密码已复制到剪贴板"), false);
    } else if (!generatedPasswords.isEmpty()) {
        QApplication::clipboard()->setText(generatedPasswords.first());
        updateStatus(tr("第一个密码已复制到剪贴板"), false);
    }
}

void RandomPasswordGen::onClearClicked()
{
    generatedPasswords.clear();
    passwordList->clear();
    previewEdit->clear();
    copyBtn->setEnabled(false);
    updateStatus(tr("已清空所有密码"), false);
}

void RandomPasswordGen::onLengthChanged()
{
    updatePasswordStrength();
}

void RandomPasswordGen::onCharsetChanged()
{
    currentCharset = getCharacterSet();
    updatePasswordStrength();
}

void RandomPasswordGen::onPasswordCountChanged()
{
    int count = countSpinBox->value();
    updateStatus(tr("将生成 %1 个密码").arg(count), false);
}

void RandomPasswordGen::onPasswordItemClicked()
{
    QListWidgetItem* currentItem = passwordList->currentItem();
    if (currentItem) {
        QString password = currentItem->text();
        int strength = calculatePasswordStrength(password);
        
        QString info = tr("密码: %1\n长度: %2 字符\n强度: %3")
                      .arg(password)
                      .arg(password.length())
                      .arg(getStrengthText(strength));
        
        previewEdit->setPlainText(info);
        copyBtn->setEnabled(true);
    }
}

QString RandomPasswordGen::generatePassword(int length)
{
    QString charset = getCharacterSet();
    if (charset.isEmpty()) return QString();
    
    QString password;
    QRandomGenerator* generator = QRandomGenerator::global();
    
    for (int i = 0; i < length; ++i) {
        int index = generator->bounded(charset.length());
        password.append(charset.at(index));
    }
    
    return password;
}

QString RandomPasswordGen::getCharacterSet()
{
    QString charset;
    
    if (includeDigits->isChecked()) {
        charset += "0123456789";
    }
    if (includeLowercase->isChecked()) {
        charset += "abcdefghijklmnopqrstuvwxyz";
    }
    if (includeUppercase->isChecked()) {
        charset += "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    }
    if (includeSpecialChars->isChecked()) {
        charset += "!@#$%^&*()_+-=[]{}|;:,.<>?";
    }
    if (includeCustomChars->isChecked() && !customCharsEdit->text().isEmpty()) {
        charset += customCharsEdit->text();
    }
    
    // 移除易混淆字符
    if (avoidAmbiguous->isChecked()) {
        charset.remove('0');
        charset.remove('O');
        charset.remove('I');
        charset.remove('l');
        charset.remove('1');
    }
    
    // 去重
    QString uniqueCharset;
    for (const QChar& c : charset) {
        if (!uniqueCharset.contains(c)) {
            uniqueCharset.append(c);
        }
    }
    
    return uniqueCharset;
}

int RandomPasswordGen::calculatePasswordStrength(const QString& password)
{
    if (password.isEmpty()) return 0;
    
    int score = 0;
    int length = password.length();
    
    // 长度评分
    if (length >= 8) score += 10;
    if (length >= 12) score += 10;
    if (length >= 16) score += 5;
    
    // 字符类型评分
    bool hasDigit = false, hasLower = false, hasUpper = false, hasSpecial = false;
    for (const QChar& c : password) {
        if (c.isDigit()) hasDigit = true;
        else if (c.isLower()) hasLower = true;
        else if (c.isUpper()) hasUpper = true;
        else hasSpecial = true;
    }
    
    int typeCount = (hasDigit ? 1 : 0) + (hasLower ? 1 : 0) + (hasUpper ? 1 : 0) + (hasSpecial ? 1 : 0);
    score += typeCount * 10;
    
    // 复杂度评分
    if (length > 8) score += (length - 8) * 2;
    if (typeCount >= 3) score += 10;
    if (hasSpecial) score += 5;
    
    return qMin(score, 100);
}

QString RandomPasswordGen::getStrengthText(int strength)
{
    if (strength < 30) return tr("弱");
    else if (strength < 60) return tr("中等");
    else if (strength < 80) return tr("强");
    else return tr("非常强");
}

QColor RandomPasswordGen::getStrengthColor(int strength)
{
    if (strength < 30) return QColor("#f44336");
    else if (strength < 60) return QColor("#ff9800");
    else if (strength < 80) return QColor("#4caf50");
    else return QColor("#2196f3");
}

void RandomPasswordGen::updatePasswordStrength()
{
    QString charset = getCharacterSet();
    int length = lengthSpinBox->value();
    
    if (charset.isEmpty()) {
        strengthBar->setValue(0);
        strengthLabel->setText(tr("强度: 无"));
        strengthDescription->setText(tr("请选择至少一种字符类型"));
        strengthBar->setStyleSheet("QProgressBar::chunk { background-color: #cccccc; }");
        return;
    }
    
    // 计算理论强度
    int strength = calculatePasswordStrength(QString(length, 'A')); // 模拟计算
    if (includeDigits->isChecked()) strength += 5;
    if (includeLowercase->isChecked()) strength += 5;
    if (includeUppercase->isChecked()) strength += 5;
    if (includeSpecialChars->isChecked()) strength += 10;
    
    strength = qMin(strength, 100);
    
    strengthBar->setValue(strength);
    strengthLabel->setText(tr("强度: %1 (%2/100)").arg(getStrengthText(strength)).arg(strength));
    
    QColor color = getStrengthColor(strength);
    strengthBar->setStyleSheet(QString("QProgressBar::chunk { background-color: %1; }").arg(color.name()));
    
    QString description;
    if (strength < 30) {
        description = tr("密码强度较弱，建议增加长度或字符类型");
    } else if (strength < 60) {
        description = tr("密码强度中等，可以考虑添加特殊字符");
    } else if (strength < 80) {
        description = tr("密码强度良好，适合大多数用途");
    } else {
        description = tr("密码强度非常好，安全性很高");
    }
    
    strengthDescription->setText(description);
}

void RandomPasswordGen::updateStatus(const QString& message, bool isError)
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
