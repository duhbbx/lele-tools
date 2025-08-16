#include "randompasswordgen.h"

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
    mainLayout->addWidget(mainSplitter);
    
    // 设置样式
    setStyleSheet(R"(
        QWidget {
            font-family: 'Segoe UI', Arial, sans-serif;
        }
        QPushButton {
            background-color: #f0f0f0;
            border: 1px solid #cccccc;
            border-radius: 4px;
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
            border-radius: 8px;
            margin-top: 1ex;
            padding-top: 10px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px 0 5px;
        }
        QCheckBox {
            spacing: 8px;
        }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
        }
        QLineEdit, QSpinBox {
            border: 2px solid #dddddd;
            border-radius: 6px;
            padding: 4px 8px;
            font-size: 11pt;
        }
        QLineEdit:focus, QSpinBox:focus {
            border-color: #4CAF50;
        }
        QSlider::groove:horizontal {
            border: 1px solid #999999;
            height: 8px;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #B1B1B1, stop:1 #c4c4c4);
            margin: 2px 0;
            border-radius: 4px;
        }
        QSlider::handle:horizontal {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #b4b4b4, stop:1 #8f8f8f);
            border: 1px solid #5c5c5c;
            width: 18px;
            margin: -2px 0;
            border-radius: 9px;
        }
        QSlider::handle:horizontal:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #d4d4d4, stop:1 #afafaf);
        }
        QListWidget {
            border: 2px solid #dddddd;
            border-radius: 8px;
            alternate-background-color: #f9f9f9;
            font-family: 'Consolas', 'Monaco', monospace;
            font-size: 11pt;
        }
        QListWidget::item {
            padding: 8px;
            border-bottom: 1px solid #eeeeee;
        }
        QListWidget::item:selected {
            background-color: #4CAF50;
            color: white;
        }
        QPlainTextEdit {
            border: 2px solid #dddddd;
            border-radius: 8px;
            padding: 8px;
            font-family: 'Consolas', 'Monaco', monospace;
            font-size: 11pt;
        }
        QProgressBar {
            border: 2px solid #cccccc;
            border-radius: 6px;
            text-align: center;
            font-weight: bold;
        }
        QProgressBar::chunk {
            border-radius: 4px;
        }
    )");
}

void RandomPasswordGen::setupToolbar()
{
    toolbarWidget = new QWidget();
    toolbarWidget->setFixedHeight(45);
    toolbarLayout = new QHBoxLayout(toolbarWidget);
    toolbarLayout->setContentsMargins(5, 5, 5, 5);
    toolbarLayout->setSpacing(8);
    
    generateBtn = new QPushButton("🎲 生成密码");
    generateBtn->setToolTip("生成随机密码");
    generateBtn->setFixedSize(110, 32);
    
    copyBtn = new QPushButton("📋 复制");
    copyBtn->setToolTip("复制选中的密码到剪贴板");
    copyBtn->setFixedSize(75, 32);
    copyBtn->setEnabled(false);
    
    clearBtn = new QPushButton("🗑️ 清空");
    clearBtn->setToolTip("清空所有生成的密码");
    clearBtn->setFixedSize(75, 32);
    
    statusLabel = new QLabel("就绪");
    statusLabel->setFixedHeight(32);
    statusLabel->setStyleSheet(R"(
        QLabel {
            color: #666; 
            font-weight: bold; 
            padding: 6px 12px; 
            background: #f9f9f9; 
            border-radius: 6px; 
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
    charsetGroup = new QGroupBox("🔤 字符集选项");
    QVBoxLayout* charsetLayout = new QVBoxLayout(charsetGroup);
    charsetLayout->setSpacing(8);
    
    includeDigits = new QCheckBox("数字 (0-9)");
    includeLowercase = new QCheckBox("小写字母 (a-z)");
    includeUppercase = new QCheckBox("大写字母 (A-Z)");
    includeSpecialChars = new QCheckBox("特殊字符 (!@#$%^&*...)");
    includeCustomChars = new QCheckBox("自定义字符:");
    
    customCharsEdit = new QLineEdit();
    customCharsEdit->setPlaceholderText("输入自定义字符...");
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
    passwordGroup = new QGroupBox("⚙️ 密码设置");
    QVBoxLayout* passwordLayout = new QVBoxLayout(passwordGroup);
    passwordLayout->setSpacing(12);
    
    // 密码长度
    QHBoxLayout* lengthLayout = new QHBoxLayout();
    lengthLabel = new QLabel("密码长度:");
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
    countLabel = new QLabel("生成数量:");
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
    advancedGroup = new QGroupBox("🔧 高级选项");
    QVBoxLayout* advancedLayout = new QVBoxLayout(advancedGroup);
    
    avoidAmbiguous = new QCheckBox("避免易混淆字符 (0OIl1)");
    requireAllTypes = new QCheckBox("必须包含所有选中类型");
    noRepeatingChars = new QCheckBox("不重复字符");
    
    advancedLayout->addWidget(avoidAmbiguous);
    advancedLayout->addWidget(requireAllTypes);
    advancedLayout->addWidget(noRepeatingChars);
    
    // 密码强度显示组
    strengthGroup = new QGroupBox("💪 密码强度");
    QVBoxLayout* strengthLayout = new QVBoxLayout(strengthGroup);
    
    strengthBar = new QProgressBar();
    strengthBar->setRange(0, 100);
    strengthBar->setValue(0);
    strengthBar->setFixedHeight(20);
    
    strengthLabel = new QLabel("强度: 未知");
    strengthLabel->setAlignment(Qt::AlignCenter);
    strengthLabel->setStyleSheet("font-weight: bold;");
    
    strengthDescription = new QLabel("请选择字符集并设置密码长度");
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
    
    displayLabel = new QLabel("🔐 生成的密码");
    displayLabel->setStyleSheet("font-weight: bold; font-size: 12pt; color: #333;");
    
    passwordList = new QListWidget();
    passwordList->setAlternatingRowColors(true);
    passwordList->setSelectionMode(QAbstractItemView::SingleSelection);
    
    // 预览区域
    previewGroup = new QGroupBox("👁️ 密码预览");
    QVBoxLayout* previewLayout = new QVBoxLayout(previewGroup);
    
    previewEdit = new QPlainTextEdit();
    previewEdit->setReadOnly(true);
    previewEdit->setMaximumHeight(100);
    previewEdit->setPlaceholderText("点击密码列表中的项目查看详细信息...");
    
    previewLayout->addWidget(previewEdit);
    
    displayLayout->addWidget(displayLabel);
    displayLayout->addWidget(passwordList, 2);
    displayLayout->addWidget(previewGroup, 0);
}

void RandomPasswordGen::onGenerateClicked()
{
    QString charset = getCharacterSet();
    if (charset.isEmpty()) {
        updateStatus("请至少选择一种字符类型", true);
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
        item->setToolTip(QString("长度: %1").arg(length));
        passwordList->addItem(item);
    }
    
    copyBtn->setEnabled(!generatedPasswords.isEmpty());
    updateStatus(QString("成功生成 %1 个密码").arg(count), false);
}

void RandomPasswordGen::onCopyClicked()
{
    QListWidgetItem* currentItem = passwordList->currentItem();
    if (currentItem) {
        QString password = currentItem->text();
        QApplication::clipboard()->setText(password);
        updateStatus("密码已复制到剪贴板", false);
    } else if (!generatedPasswords.isEmpty()) {
        QApplication::clipboard()->setText(generatedPasswords.first());
        updateStatus("第一个密码已复制到剪贴板", false);
    }
}

void RandomPasswordGen::onClearClicked()
{
    generatedPasswords.clear();
    passwordList->clear();
    previewEdit->clear();
    copyBtn->setEnabled(false);
    updateStatus("已清空所有密码", false);
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
    updateStatus(QString("将生成 %1 个密码").arg(count), false);
}

void RandomPasswordGen::onPasswordItemClicked()
{
    QListWidgetItem* currentItem = passwordList->currentItem();
    if (currentItem) {
        QString password = currentItem->text();
        int strength = calculatePasswordStrength(password);
        
        QString info = QString("密码: %1\n长度: %2 字符\n强度: %3")
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
    if (strength < 30) return "弱";
    else if (strength < 60) return "中等";
    else if (strength < 80) return "强";
    else return "非常强";
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
        strengthLabel->setText("强度: 无");
        strengthDescription->setText("请选择至少一种字符类型");
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
    strengthLabel->setText(QString("强度: %1 (%2/100)").arg(getStrengthText(strength)).arg(strength));
    
    QColor color = getStrengthColor(strength);
    strengthBar->setStyleSheet(QString("QProgressBar::chunk { background-color: %1; }").arg(color.name()));
    
    QString description;
    if (strength < 30) {
        description = "密码强度较弱，建议增加长度或字符类型";
    } else if (strength < 60) {
        description = "密码强度中等，可以考虑添加特殊字符";
    } else if (strength < 80) {
        description = "密码强度良好，适合大多数用途";
    } else {
        description = "密码强度非常好，安全性很高";
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
                border-radius: 6px; 
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
                border-radius: 6px; 
                border: 1px solid #c8e6c9;
            }
        )");
    }
}
