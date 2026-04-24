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
    setStyleSheet(R"(
        QPushButton { padding:4px 10px; border:none; border-radius:4px; font-size:9pt; color:#495057; background:transparent; }
        QPushButton:hover { background-color:#e9ecef; }
        QPushButton:disabled { color:#adb5bd; }
        QLabel { font-size:9pt; color:#495057; }
        QCheckBox { font-size:9pt; color:#495057; }
        QLineEdit { border:1px solid #dee2e6; border-radius:4px; padding:4px 8px; font-size:9pt; }
        QSpinBox { border:1px solid #dee2e6; border-radius:4px; padding:4px 8px; font-size:9pt; }
        QSlider::groove:horizontal { height:4px; background:#dee2e6; border-radius:2px; }
        QSlider::handle:horizontal { width:14px; height:14px; margin:-5px 0; background:#228be6; border-radius:7px; }
        QSlider::sub-page:horizontal { background:#228be6; border-radius:2px; }
        QProgressBar { border:1px solid #dee2e6; border-radius:4px; text-align:center; font-size:8pt; height:8px; }
        QProgressBar::chunk { border-radius:3px; }
        QListWidget { border:1px solid #dee2e6; border-radius:4px; font-size:9pt; background:#fff; }
        QListWidget::item { padding:6px 8px; border-bottom:1px solid #f1f3f5; }
        QListWidget::item:selected { background-color:#e7f5ff; color:#212529; }
    )");

    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    setupToolbar();

    // Content area: horizontal layout (left settings | right display)
    QHBoxLayout* contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(16);

    setupCharsetOptions();
    setupPasswordSettings();
    setupPasswordDisplay();

    contentLayout->addWidget(settingsWidget, 1);
    contentLayout->addWidget(displayWidget, 2);

    mainLayout->addWidget(toolbarWidget);
    mainLayout->addLayout(contentLayout, 1);
}

void RandomPasswordGen::setupToolbar()
{
    toolbarWidget = new QWidget();
    toolbarLayout = new QHBoxLayout(toolbarWidget);
    toolbarLayout->setContentsMargins(0, 0, 0, 0);
    toolbarLayout->setSpacing(8);

    generateBtn = new QPushButton(tr("生成"));
    generateBtn->setToolTip(tr("生成随机密码"));
    generateBtn->setStyleSheet(R"(
        QPushButton { background-color:#228be6; color:#fff; font-weight:bold; padding:4px 14px; border:none; border-radius:4px; font-size:9pt; }
        QPushButton:hover { background-color:#1c7ed6; }
    )");

    copyBtn = new QPushButton(tr("复制"));
    copyBtn->setToolTip(tr("复制选中的密码到剪贴板"));
    copyBtn->setEnabled(false);

    clearBtn = new QPushButton(tr("清空"));
    clearBtn->setToolTip(tr("清空所有生成的密码"));

    statusLabel = new QLabel(tr("就绪"));

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
    settingsLayout->setSpacing(6);

    // 字符集 section
    QLabel* charsetTitle = new QLabel(tr("字符集:"));
    charsetTitle->setStyleSheet("font-weight:bold;");
    settingsLayout->addWidget(charsetTitle);

    includeDigits = new QCheckBox(tr("数字 (0-9)"));
    includeLowercase = new QCheckBox(tr("小写字母 (a-z)"));
    includeUppercase = new QCheckBox(tr("大写字母 (A-Z)"));
    includeSpecialChars = new QCheckBox(tr("特殊字符 (!@#$%^&*...)"));
    includeCustomChars = new QCheckBox(tr("自定义字符:"));

    customCharsEdit = new QLineEdit();
    customCharsEdit->setPlaceholderText(tr("输入自定义字符..."));
    customCharsEdit->setEnabled(false);

    connect(includeCustomChars, &QCheckBox::toggled, customCharsEdit, &QLineEdit::setEnabled);

    settingsLayout->addWidget(includeDigits);
    settingsLayout->addWidget(includeLowercase);
    settingsLayout->addWidget(includeUppercase);
    settingsLayout->addWidget(includeSpecialChars);

    QHBoxLayout* customRow = new QHBoxLayout();
    customRow->setSpacing(6);
    customRow->addWidget(includeCustomChars);
    customRow->addWidget(customCharsEdit, 1);
    settingsLayout->addLayout(customRow);
}

void RandomPasswordGen::setupPasswordSettings()
{
    settingsLayout->addSpacing(8);

    // 密码长度
    lengthLabel = new QLabel(tr("密码长度:"));
    lengthLabel->setStyleSheet("font-weight:bold;");
    settingsLayout->addWidget(lengthLabel);

    QHBoxLayout* lengthLayout = new QHBoxLayout();
    lengthSlider = new QSlider(Qt::Horizontal);
    lengthSlider->setRange(4, 128);
    lengthSlider->setValue(12);
    lengthSpinBox = new QSpinBox();
    lengthSpinBox->setRange(4, 128);
    lengthSpinBox->setValue(12);
    lengthSpinBox->setFixedWidth(60);
    lengthLayout->addWidget(lengthSlider, 1);
    lengthLayout->addWidget(lengthSpinBox);
    settingsLayout->addLayout(lengthLayout);

    // 生成数量
    QHBoxLayout* countLayout = new QHBoxLayout();
    countLabel = new QLabel(tr("生成数量:"));
    countLabel->setStyleSheet("font-weight:bold;");
    countSpinBox = new QSpinBox();
    countSpinBox->setRange(1, 100);
    countSpinBox->setValue(5);
    countSpinBox->setFixedWidth(60);
    countLayout->addWidget(countLabel);
    countLayout->addStretch();
    countLayout->addWidget(countSpinBox);
    settingsLayout->addLayout(countLayout);

    settingsLayout->addSpacing(8);

    // 高级 section
    QLabel* advancedTitle = new QLabel(tr("高级:"));
    advancedTitle->setStyleSheet("font-weight:bold;");
    settingsLayout->addWidget(advancedTitle);

    avoidAmbiguous = new QCheckBox(tr("避免易混淆字符 (0OIl1)"));
    requireAllTypes = new QCheckBox(tr("必须包含所有选中类型"));
    noRepeatingChars = new QCheckBox(tr("不重复字符"));

    settingsLayout->addWidget(avoidAmbiguous);
    settingsLayout->addWidget(requireAllTypes);
    settingsLayout->addWidget(noRepeatingChars);

    settingsLayout->addStretch();
}

void RandomPasswordGen::setupPasswordDisplay()
{
    displayWidget = new QWidget();
    displayLayout = new QVBoxLayout(displayWidget);
    displayLayout->setContentsMargins(0, 0, 0, 0);
    displayLayout->setSpacing(8);

    passwordList = new QListWidget();
    passwordList->setSelectionMode(QAbstractItemView::SingleSelection);
    QFont monoFont("Menlo, Consolas, monospace");
    monoFont.setStyleHint(QFont::Monospace);
    passwordList->setFont(monoFont);

    // Selected password preview
    previewLabel = new QLabel();
    previewLabel->setFont(monoFont);
    previewLabel->setStyleSheet("font-size:12pt; color:#212529; padding:6px 0;");
    previewLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    previewLabel->setWordWrap(true);

    // previewEdit kept for compatibility with logic methods
    previewEdit = new QPlainTextEdit();
    previewEdit->setReadOnly(true);
    previewEdit->setMaximumHeight(60);
    previewEdit->setFont(monoFont);
    previewEdit->setPlaceholderText(tr("点击密码列表中的项目查看详细信息..."));

    // Strength bar (thin, no text)
    strengthBar = new QProgressBar();
    strengthBar->setRange(0, 100);
    strengthBar->setValue(0);
    strengthBar->setTextVisible(false);
    strengthBar->setFixedHeight(8);

    strengthLabel = new QLabel(tr("强度: 未知"));
    strengthLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    strengthDescription = new QLabel(tr("请选择字符集并设置密码长度"));
    strengthDescription->setWordWrap(true);

    displayLayout->addWidget(passwordList, 1);
    displayLayout->addWidget(previewEdit);
    displayLayout->addWidget(strengthBar);
    displayLayout->addWidget(strengthLabel);
    displayLayout->addWidget(strengthDescription);
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
        statusLabel->setStyleSheet("QLabel { color:#d32f2f; }");
    } else {
        statusLabel->setStyleSheet("QLabel { color:#495057; }");
    }
}
