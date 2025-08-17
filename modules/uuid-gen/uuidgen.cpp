#include "uuidgen.h"

REGISTER_DYNAMICOBJECT(UuidGen);

// 静态常量定义
const QStringList UuidGen::VERSION_NAMES = {
    "UUID v4 (随机)", "UUID v1 (时间戳)", "UUID v3 (MD5)", "UUID v5 (SHA1)"
};

const QStringList UuidGen::FORMAT_NAMES = {
    "标准格式", "简单格式", "大括号格式", "圆括号格式", "URN格式", "大写", "小写"
};

const QStringList UuidGen::NAMESPACE_UUIDS = {
    "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
};

UuidGen::UuidGen() : QWidget(nullptr), DynamicObjectBase(), totalGenerated(0)
{
    setupUI();
    onGenerateSingleClicked();
}

UuidGen::~UuidGen()
{
}

void UuidGen::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);
    
    mainTabWidget = new QTabWidget;
    
    setupGeneratorTab();
    setupValidatorTab();
    setupConverterTab();
    setupHistoryTab();
    
    mainTabWidget->addTab(generatorTab, "UUID生成器");
    mainTabWidget->addTab(validatorTab, "UUID验证器");
    mainTabWidget->addTab(converterTab, "格式转换");
    mainTabWidget->addTab(historyTab, "历史记录");
    
    mainLayout->addWidget(mainTabWidget);
    
    setupToolbar();
    mainLayout->addWidget(toolbarGroup);
    
    setStyleSheet(
        "QWidget {"
        "    font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;"
        "    font-size: 12px;"
        "}"
        "QGroupBox {"
        "    font-weight: bold;"
        "    font-size: 14px;"
        "    border: 2px solid #cccccc;"
        "    border-radius: 6px;"
        "    margin-top: 15px;"
        "    padding-top: 10px;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    subcontrol-position: top left;"
        "    left: 10px;"
        "    padding: 0 8px 0 8px;"
        "    background-color: #ffffff;"
        "    color: #495057;"
        "}"
        "QPushButton {"
        "    background-color: #f8f9fa;"
        "    border: 1px solid #dee2e6;"
        "    border-radius: 4px;"
        "    padding: 8px 12px;"
        "    font-weight: normal;"
        "    font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #e9ecef;"
        "    border-color: #adb5bd;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #dee2e6;"
        "}"
        "QLineEdit, QTextEdit, QComboBox, QSpinBox {"
        "    border: 1px solid #ced4da;"
        "    border-radius: 4px;"
        "    padding: 6px;"
        "    background-color: #ffffff;"
        "    font-size: 12px;"
        "}"
        "QLineEdit:focus, QTextEdit:focus, QComboBox:focus, QSpinBox:focus {"
        "    border-color: #80bdff;"
        "    outline: 0;"
        "}"
    );
}

void UuidGen::setupGeneratorTab()
{
    generatorTab = new QWidget;
    QHBoxLayout *layout = new QHBoxLayout(generatorTab);
    layout->setContentsMargins(15, 15, 15, 15);
    layout->setSpacing(15);
    
    // 左侧设置区域
    QVBoxLayout *leftLayout = new QVBoxLayout;
    
    settingsGroup = new QGroupBox("生成设置");
    settingsGroup->setMinimumWidth(280);
    settingsGroup->setMinimumHeight(250);
    QGridLayout *settingsLayout = new QGridLayout(settingsGroup);
    settingsLayout->setContentsMargins(15, 25, 15, 15);
    settingsLayout->setSpacing(12);
    
    QLabel *versionLabel = new QLabel("UUID版本:");
    versionLabel->setStyleSheet("font-weight: bold; font-size: 13px;");
    settingsLayout->addWidget(versionLabel, 0, 0);
    
    versionCombo = new QComboBox;
    versionCombo->addItems(VERSION_NAMES);
    versionCombo->setMinimumHeight(35);
    versionCombo->setStyleSheet("QComboBox { font-size: 13px; padding: 6px; }");
    settingsLayout->addWidget(versionCombo, 0, 1);
    
    QLabel *formatLabel = new QLabel("输出格式:");
    formatLabel->setStyleSheet("font-weight: bold; font-size: 13px;");
    settingsLayout->addWidget(formatLabel, 1, 0);
    
    formatCombo = new QComboBox;
    formatCombo->addItems(FORMAT_NAMES);
    formatCombo->setMinimumHeight(35);
    formatCombo->setStyleSheet("QComboBox { font-size: 13px; padding: 6px; }");
    settingsLayout->addWidget(formatCombo, 1, 1);
    
    QLabel *countLabel = new QLabel("生成数量:");
    countLabel->setStyleSheet("font-weight: bold; font-size: 13px;");
    settingsLayout->addWidget(countLabel, 2, 0);
    
    countSpin = new QSpinBox;
    countSpin->setRange(1, 100);
    countSpin->setValue(1);
    countSpin->setMinimumHeight(35);
    countSpin->setStyleSheet("QSpinBox { font-size: 13px; padding: 6px; }");
    settingsLayout->addWidget(countSpin, 2, 1);
    
    generateButton = new QPushButton("生成UUID");
    generateButton->setMinimumHeight(45);
    generateButton->setStyleSheet("QPushButton { background-color: #007bff; color: white; font-weight: bold; font-size: 14px; padding: 12px; }");
    settingsLayout->addWidget(generateButton, 3, 0, 1, 2);
    
    batchGenerateButton = new QPushButton("批量生成");
    batchGenerateButton->setMinimumHeight(45);
    batchGenerateButton->setStyleSheet("QPushButton { background-color: #28a745; color: white; font-weight: bold; font-size: 14px; padding: 12px; }");
    settingsLayout->addWidget(batchGenerateButton, 4, 0, 1, 2);
    
    leftLayout->addWidget(settingsGroup);
    leftLayout->addStretch();
    
    // 右侧结果区域
    QVBoxLayout *rightLayout = new QVBoxLayout;
    
    resultGroup = new QGroupBox("生成结果");
    resultGroup->setMinimumHeight(350);
    QVBoxLayout *resultLayout = new QVBoxLayout(resultGroup);
    resultLayout->setContentsMargins(15, 25, 15, 15);
    resultLayout->setSpacing(10);
    
    resultEdit = new QTextEdit;
    resultEdit->setMinimumHeight(250);
    resultEdit->setReadOnly(true);
    resultEdit->setStyleSheet(
        "QTextEdit {"
        "    font-family: 'Consolas', monospace;"
        "    font-size: 13px;"
        "    background-color: #f8f9fa;"
        "    border: 1px solid #dee2e6;"
        "    border-radius: 4px;"
        "    padding: 10px;"
        "    line-height: 1.4;"
        "}"
    );
    resultEdit->setPlaceholderText("生成的UUID将显示在这里...");
    resultLayout->addWidget(resultEdit);
    
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    copyButton = new QPushButton("复制当前");
    copyAllButton = new QPushButton("复制全部");
    
    copyButton->setMinimumHeight(35);
    copyAllButton->setMinimumHeight(35);
    copyButton->setStyleSheet("QPushButton { background-color: #17a2b8; color: white; font-weight: bold; padding: 8px 16px; }");
    copyAllButton->setStyleSheet("QPushButton { background-color: #6f42c1; color: white; font-weight: bold; padding: 8px 16px; }");
    
    buttonLayout->addWidget(copyButton);
    buttonLayout->addWidget(copyAllButton);
    buttonLayout->addStretch();
    resultLayout->addLayout(buttonLayout);
    
    statisticsLabel = new QLabel("统计: 已生成 0 个UUID");
    statisticsLabel->setStyleSheet("color: #6c757d; font-style: italic; font-size: 13px; margin-top: 5px;");
    resultLayout->addWidget(statisticsLabel);
    
    rightLayout->addWidget(resultGroup);
    
    layout->addLayout(leftLayout, 1);
    layout->addLayout(rightLayout, 2);
    
    connect(generateButton, &QPushButton::clicked, this, &UuidGen::onGenerateSingleClicked);
    connect(batchGenerateButton, &QPushButton::clicked, this, &UuidGen::onGenerateBatchClicked);
    connect(copyButton, &QPushButton::clicked, this, &UuidGen::onCopyClicked);
    connect(copyAllButton, &QPushButton::clicked, this, &UuidGen::onCopyAllClicked);
}

void UuidGen::setupValidatorTab()
{
    validatorTab = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(validatorTab);
    layout->setContentsMargins(15, 15, 15, 15);
    layout->setSpacing(15);
    
    // UUID输入区域
    inputGroup = new QGroupBox("UUID输入");
    inputGroup->setMinimumHeight(120);
    QVBoxLayout *inputLayout = new QVBoxLayout(inputGroup);
    inputLayout->setContentsMargins(15, 25, 15, 15);
    inputLayout->setSpacing(10);
    
    QLabel *formatHintLabel = new QLabel("支持多种格式: 标准格式、简单格式、大括号格式等");
    formatHintLabel->setStyleSheet("color: #6c757d; font-style: italic; margin-bottom: 5px;");
    inputLayout->addWidget(formatHintLabel);
    
    inputUuidEdit = new QLineEdit;
    inputUuidEdit->setPlaceholderText("例如: 550e8400-e29b-41d4-a716-446655440000");
    inputUuidEdit->setMinimumHeight(40);
    inputUuidEdit->setStyleSheet(
        "QLineEdit {"
        "    font-size: 14px;"
        "    font-family: 'Consolas', monospace;"
        "    padding: 8px 12px;"
        "    border: 2px solid #ced4da;"
        "    border-radius: 6px;"
        "}"
        "QLineEdit:focus {"
        "    border-color: #80bdff;"
        "    box-shadow: 0 0 0 0.2rem rgba(0, 123, 255, 0.25);"
        "}"
    );
    inputLayout->addWidget(inputUuidEdit);
    
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    validateButton = new QPushButton("验证UUID");
    parseButton = new QPushButton("解析详情");
    
    validateButton->setMinimumHeight(35);
    parseButton->setMinimumHeight(35);
    validateButton->setStyleSheet("QPushButton { background-color: #17a2b8; color: white; font-weight: bold; padding: 8px 16px; }");
    parseButton->setStyleSheet("QPushButton { background-color: #ffc107; color: #212529; font-weight: bold; padding: 8px 16px; }");
    
    buttonLayout->addWidget(validateButton);
    buttonLayout->addWidget(parseButton);
    buttonLayout->addStretch();
    inputLayout->addLayout(buttonLayout);
    
    layout->addWidget(inputGroup);
    
    // 验证结果区域
    validationResultGroup = new QGroupBox("验证结果");
    validationResultGroup->setMinimumHeight(250);
    QVBoxLayout *resultLayout = new QVBoxLayout(validationResultGroup);
    resultLayout->setContentsMargins(15, 25, 15, 15);
    resultLayout->setSpacing(10);
    
    validationStatusLabel = new QLabel("请输入UUID进行验证");
    validationStatusLabel->setMinimumHeight(30);
    validationStatusLabel->setStyleSheet(
        "color: #6c757d; "
        "font-weight: bold; "
        "font-size: 14px; "
        "padding: 8px 12px; "
        "background-color: #f8f9fa; "
        "border: 1px solid #dee2e6; "
        "border-radius: 4px;"
    );
    resultLayout->addWidget(validationStatusLabel);
    
    QLabel *detailLabel = new QLabel("详细信息:");
    detailLabel->setStyleSheet("font-weight: bold; margin-top: 10px; font-size: 13px;");
    resultLayout->addWidget(detailLabel);
    
    parseResultEdit = new QTextEdit;
    parseResultEdit->setReadOnly(true);
    parseResultEdit->setMinimumHeight(150);
    parseResultEdit->setStyleSheet(
        "QTextEdit {"
        "    font-family: 'Consolas', monospace;"
        "    font-size: 12px;"
        "    background-color: #f8f9fa;"
        "    border: 1px solid #dee2e6;"
        "    border-radius: 4px;"
        "    padding: 10px;"
        "}"
    );
    parseResultEdit->setPlaceholderText("UUID的详细解析信息将显示在这里...");
    resultLayout->addWidget(parseResultEdit);
    
    layout->addWidget(validationResultGroup);
    
    // 确保布局能够正确伸展
    layout->addStretch();
    
    connect(validateButton, &QPushButton::clicked, this, &UuidGen::onValidateClicked);
    connect(parseButton, &QPushButton::clicked, this, &UuidGen::onParseClicked);
    connect(inputUuidEdit, &QLineEdit::textChanged, this, &UuidGen::onInputUuidChanged);
}

void UuidGen::setupConverterTab()
{
    converterTab = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(converterTab);
    layout->setContentsMargins(15, 15, 15, 15);
    layout->setSpacing(15);
    
    // 格式转换区域
    convertGroup = new QGroupBox("格式转换");
    convertGroup->setMinimumHeight(150);
    QGridLayout *convertLayout = new QGridLayout(convertGroup);
    convertLayout->setContentsMargins(15, 25, 15, 15);
    convertLayout->setSpacing(10);
    
    QLabel *inputLabel = new QLabel("输入UUID:");
    inputLabel->setStyleSheet("font-weight: bold; font-size: 13px;");
    convertLayout->addWidget(inputLabel, 0, 0);
    
    convertInputEdit = new QLineEdit;
    convertInputEdit->setPlaceholderText("输入要转换格式的UUID...");
    convertInputEdit->setMinimumHeight(35);
    convertInputEdit->setStyleSheet(
        "QLineEdit {"
        "    font-family: 'Consolas', monospace;"
        "    font-size: 13px;"
        "    padding: 8px 12px;"
        "    border: 2px solid #ced4da;"
        "    border-radius: 6px;"
        "}"
    );
    convertLayout->addWidget(convertInputEdit, 0, 1, 1, 2);
    
    QLabel *formatLabel = new QLabel("转换为:");
    formatLabel->setStyleSheet("font-weight: bold; font-size: 13px;");
    convertLayout->addWidget(formatLabel, 1, 0);
    
    convertToCombo = new QComboBox;
    convertToCombo->addItems(FORMAT_NAMES);
    convertToCombo->setMinimumHeight(35);
    convertToCombo->setStyleSheet("QComboBox { font-size: 13px; padding: 6px; }");
    convertLayout->addWidget(convertToCombo, 1, 1);
    
    convertButton = new QPushButton("转换格式");
    convertButton->setMinimumHeight(40);
    convertButton->setStyleSheet("QPushButton { background-color: #6f42c1; color: white; font-weight: bold; padding: 10px 20px; font-size: 13px; }");
    convertLayout->addWidget(convertButton, 2, 0, 1, 3);
    
    layout->addWidget(convertGroup);
    
    // 转换结果区域
    convertResultGroup = new QGroupBox("转换结果");
    convertResultGroup->setMinimumHeight(150);
    QVBoxLayout *resultLayout = new QVBoxLayout(convertResultGroup);
    resultLayout->setContentsMargins(15, 25, 15, 15);
    resultLayout->setSpacing(10);
    
    convertResultEdit = new QTextEdit;
    convertResultEdit->setReadOnly(true);
    convertResultEdit->setMinimumHeight(80);
    convertResultEdit->setStyleSheet(
        "QTextEdit {"
        "    font-family: 'Consolas', monospace;"
        "    font-size: 14px;"
        "    background-color: #f8f9fa;"
        "    border: 1px solid #dee2e6;"
        "    border-radius: 4px;"
        "    padding: 10px;"
        "}"
    );
    convertResultEdit->setPlaceholderText("转换后的UUID格式将显示在这里...");
    resultLayout->addWidget(convertResultEdit);
    
    QPushButton *copyConvertedButton = new QPushButton("复制转换结果");
    copyConvertedButton->setStyleSheet("QPushButton { background-color: #28a745; color: white; font-weight: bold; }");
    copyConvertedButton->setMaximumWidth(150);
    resultLayout->addWidget(copyConvertedButton);
    
    layout->addWidget(convertResultGroup);
    layout->addStretch();
    
    connect(convertButton, &QPushButton::clicked, this, &UuidGen::onConvertFormatClicked);
    connect(copyConvertedButton, &QPushButton::clicked, [this]() {
        QString text = convertResultEdit->toPlainText().trimmed();
        if (!text.isEmpty()) {
            QClipboard *clipboard = QApplication::clipboard();
            clipboard->setText(text);
            QMessageBox::information(this, "成功", "转换结果已复制到剪贴板");
        } else {
            QMessageBox::information(this, "提示", "没有转换结果可复制");
        }
    });
}

void UuidGen::setupHistoryTab()
{
    historyTab = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(historyTab);
    layout->setContentsMargins(15, 15, 15, 15);
    layout->setSpacing(15);
    
    // UUID生成历史区域
    historyGroup = new QGroupBox("UUID生成历史");
    QVBoxLayout *historyLayout = new QVBoxLayout(historyGroup);
    historyLayout->setContentsMargins(15, 25, 15, 15);
    historyLayout->setSpacing(10);
    
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    deleteItemButton = new QPushButton("删除选中");
    clearHistoryButton = new QPushButton("清空历史");
    
    deleteItemButton->setMinimumHeight(35);
    clearHistoryButton->setMinimumHeight(35);
    deleteItemButton->setStyleSheet("QPushButton { background-color: #ffc107; color: #212529; font-weight: bold; padding: 8px 16px; }");
    clearHistoryButton->setStyleSheet("QPushButton { background-color: #dc3545; color: white; font-weight: bold; padding: 8px 16px; }");
    
    buttonLayout->addWidget(deleteItemButton);
    buttonLayout->addWidget(clearHistoryButton);
    buttonLayout->addStretch();
    historyLayout->addLayout(buttonLayout);
    
    historyTable = new QTableWidget;
    historyTable->setColumnCount(3);
    historyTable->setHorizontalHeaderLabels({"UUID", "版本", "生成时间"});
    historyTable->setAlternatingRowColors(true);
    historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    historyTable->verticalHeader()->setVisible(false);
    historyTable->setMinimumHeight(300);
    
    // 设置表格样式
    historyTable->setStyleSheet(
        "QTableWidget {"
        "    gridline-color: #dee2e6;"
        "    background-color: #ffffff;"
        "    alternate-background-color: #f8f9fa;"
        "    border: 1px solid #dee2e6;"
        "    border-radius: 4px;"
        "}"
        "QTableWidget::item {"
        "    padding: 8px;"
        "    border-bottom: 1px solid #dee2e6;"
        "    font-family: 'Consolas', monospace;"
        "}"
        "QTableWidget::item:selected {"
        "    background-color: #007bff;"
        "    color: white;"
        "}"
        "QHeaderView::section {"
        "    background-color: #f8f9fa;"
        "    padding: 8px;"
        "    border: 1px solid #dee2e6;"
        "    font-weight: bold;"
        "}"
    );
    
    // 设置列宽
    historyTable->setColumnWidth(0, 300);
    historyTable->setColumnWidth(1, 80);
    historyTable->setColumnWidth(2, 120);
    historyTable->horizontalHeader()->setStretchLastSection(true);
    
    historyLayout->addWidget(historyTable);
    
    historyStatsLabel = new QLabel("历史记录: 0 条");
    historyStatsLabel->setStyleSheet("color: #6c757d; font-style: italic; font-size: 13px; margin-top: 5px;");
    historyLayout->addWidget(historyStatsLabel);
    
    layout->addWidget(historyGroup);
    
    connect(clearHistoryButton, &QPushButton::clicked, this, &UuidGen::onClearHistoryClicked);
    connect(deleteItemButton, &QPushButton::clicked, this, &UuidGen::onDeleteHistoryItemClicked);
    connect(historyTable, &QTableWidget::itemClicked, this, &UuidGen::onHistoryItemClicked);
}

void UuidGen::setupToolbar()
{
    toolbarGroup = new QGroupBox("工具");
    QHBoxLayout *layout = new QHBoxLayout(toolbarGroup);
    
    exportButton = new QPushButton("导出UUID");
    importButton = new QPushButton("导入设置");
    
    exportButton->setStyleSheet("QPushButton { background-color: #17a2b8; color: white; }");
    importButton->setStyleSheet("QPushButton { background-color: #6f42c1; color: white; }");
    
    layout->addWidget(exportButton);
    layout->addWidget(importButton);
    layout->addStretch();
    
    connect(exportButton, &QPushButton::clicked, this, &UuidGen::onExportClicked);
    connect(importButton, &QPushButton::clicked, this, &UuidGen::onImportClicked);
}

QString UuidGen::generateUuid(UuidVersion version, const QString &customData)
{
    Q_UNUSED(customData);
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString UuidGen::generateUuidV1()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString UuidGen::generateUuidV3(const QString &namespace_uuid, const QString &name)
{
    Q_UNUSED(namespace_uuid);
    Q_UNUSED(name);
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString UuidGen::generateUuidV4()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString UuidGen::generateUuidV5(const QString &namespace_uuid, const QString &name)
{
    Q_UNUSED(namespace_uuid);
    Q_UNUSED(name);
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString UuidGen::formatUuid(const QString &uuid, UuidFormat format)
{
    QString clean = uuid;
    clean.remove(QRegularExpression("[^0-9a-fA-F]"));
    
    if (clean.length() != 32) return uuid;
    
    QString formatted = QString("%1-%2-%3-%4-%5")
                       .arg(clean.mid(0, 8))
                       .arg(clean.mid(8, 4))
                       .arg(clean.mid(12, 4))
                       .arg(clean.mid(16, 4))
                       .arg(clean.mid(20, 12));
    
    switch (format) {
        case FORMAT_SIMPLE: return clean;
        case FORMAT_BRACES: return "{" + formatted + "}";
        case FORMAT_PARENTHESES: return "(" + formatted + ")";
        case FORMAT_URN: return "urn:uuid:" + formatted;
        case FORMAT_UPPERCASE: return formatted.toUpper();
        case FORMAT_LOWERCASE: return formatted.toLower();
        default: return formatted;
    }
}

bool UuidGen::isValidUuid(const QString &uuid)
{
    QRegularExpression regex("^[\\{\\(]?[0-9a-fA-F]{8}-?[0-9a-fA-F]{4}-?[0-9a-fA-F]{4}-?[0-9a-fA-F]{4}-?[0-9a-fA-F]{12}[\\}\\)]?$");
    return regex.match(uuid.trimmed()).hasMatch();
}

UuidInfo UuidGen::parseUuid(const QString &uuid)
{
    UuidInfo info;
    info.uuid = uuid;
    info.version = UUID_V4;
    info.description = isValidUuid(uuid) ? "有效的UUID" : "无效的UUID";
    info.generated = QDateTime::currentDateTime();
    return info;
}

QString UuidGen::getVersionDescription(UuidVersion version)
{
    switch (version) {
        case UUID_V1: return "版本1: 基于时间戳";
        case UUID_V3: return "版本3: 基于MD5哈希";
        case UUID_V4: return "版本4: 随机生成";
        case UUID_V5: return "版本5: 基于SHA1哈希";
        default: return "未知版本";
    }
}

void UuidGen::addToHistory(const UuidInfo &info)
{
    uuidHistory.prepend(info);
    if (uuidHistory.size() > 50) {
        uuidHistory.removeLast();
    }
    updateHistoryTable();
}

void UuidGen::updateHistoryTable()
{
    historyTable->setRowCount(uuidHistory.size());
    
    for (int i = 0; i < uuidHistory.size(); ++i) {
        const UuidInfo &info = uuidHistory[i];
        historyTable->setItem(i, 0, new QTableWidgetItem(info.uuid));
        historyTable->setItem(i, 1, new QTableWidgetItem(QString("v%1").arg(info.version)));
        historyTable->setItem(i, 2, new QTableWidgetItem(info.generated.toString("hh:mm:ss")));
    }
    
    historyStatsLabel->setText(QString("历史记录: %1 条").arg(uuidHistory.size()));
}

void UuidGen::updateStatistics()
{
    statisticsLabel->setText(QString("统计: 已生成 %1 个UUID").arg(totalGenerated));
}

void UuidGen::onGenerateSingleClicked()
{
    QString uuid = generateUuidV4();
    UuidFormat format = static_cast<UuidFormat>(formatCombo->currentIndex());
    QString formatted = formatUuid(uuid, format);
    
    resultEdit->clear();
    resultEdit->append(formatted);
    
    UuidInfo info(formatted, UUID_V4, "随机生成");
    addToHistory(info);
    
    totalGenerated++;
    updateStatistics();
}

void UuidGen::onGenerateBatchClicked()
{
    int count = countSpin->value();
    UuidFormat format = static_cast<UuidFormat>(formatCombo->currentIndex());
    
    resultEdit->clear();
    
    for (int i = 0; i < count; ++i) {
        QString uuid = generateUuidV4();
        QString formatted = formatUuid(uuid, format);
        resultEdit->append(formatted);
        
        UuidInfo info(formatted, UUID_V4, "批量生成");
        addToHistory(info);
        totalGenerated++;
    }
    
    updateStatistics();
}

void UuidGen::onValidateClicked()
{
    QString uuid = inputUuidEdit->text().trimmed();
    if (uuid.isEmpty()) {
        QMessageBox::information(this, "提示", "请输入UUID");
        return;
    }
    
    if (isValidUuid(uuid)) {
        validationStatusLabel->setText("✓ UUID格式有效");
        validationStatusLabel->setStyleSheet("color: #28a745;");
        parseResultEdit->setText("这是一个有效的UUID格式");
    } else {
        validationStatusLabel->setText("✗ UUID格式无效");
        validationStatusLabel->setStyleSheet("color: #dc3545;");
        parseResultEdit->setText("输入的字符串不是有效的UUID格式");
    }
}

void UuidGen::onParseClicked()
{
    QString uuid = inputUuidEdit->text().trimmed();
    if (uuid.isEmpty()) {
        QMessageBox::information(this, "提示", "请输入UUID");
        return;
    }
    
    UuidInfo info = parseUuid(uuid);
    QString result = QString("UUID: %1\n版本: %2\n描述: %3\n解析时间: %4")
                    .arg(info.uuid)
                    .arg(info.version)
                    .arg(info.description)
                    .arg(QDateTime::currentDateTime().toString("hh:mm:ss"));
    
    parseResultEdit->setText(result);
}

void UuidGen::onInputUuidChanged()
{
    QString uuid = inputUuidEdit->text().trimmed();
    if (uuid.isEmpty()) {
        validationStatusLabel->setText("请输入UUID进行验证");
        validationStatusLabel->setStyleSheet("color: #6c757d;");
        return;
    }
    
    if (isValidUuid(uuid)) {
        validationStatusLabel->setText("✓ UUID格式有效");
        validationStatusLabel->setStyleSheet("color: #28a745;");
    } else {
        validationStatusLabel->setText("✗ UUID格式无效");
        validationStatusLabel->setStyleSheet("color: #dc3545;");
    }
}

void UuidGen::onCopyClicked()
{
    QString text = resultEdit->toPlainText().split('\n').first().trimmed();
    if (!text.isEmpty()) {
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setText(text);
        QMessageBox::information(this, "成功", "UUID已复制到剪贴板");
    }
}

void UuidGen::onCopyAllClicked()
{
    QString text = resultEdit->toPlainText().trimmed();
    if (!text.isEmpty()) {
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setText(text);
        QMessageBox::information(this, "成功", "所有UUID已复制到剪贴板");
    }
}

void UuidGen::onClearHistoryClicked()
{
    int ret = QMessageBox::question(this, "确认", "确定要清空历史记录吗？");
    if (ret == QMessageBox::Yes) {
        uuidHistory.clear();
        updateHistoryTable();
    }
}

void UuidGen::onExportClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, "导出UUID",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/uuid_list.txt",
        "文本文件 (*.txt)");
    
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly)) {
            QTextStream out(&file);
            out.setEncoding(QStringConverter::Utf8);
            for (const UuidInfo &info : uuidHistory) {
                out << info.uuid << "\n";
            }
            file.close();
            QMessageBox::information(this, "成功", "UUID已导出");
        }
    }
}

void UuidGen::onConvertFormatClicked()
{
    QString uuid = convertInputEdit->text().trimmed();
    if (uuid.isEmpty()) {
        QMessageBox::information(this, "提示", "请输入UUID");
        return;
    }
    
    if (!isValidUuid(uuid)) {
        QMessageBox::warning(this, "错误", "UUID格式无效");
        return;
    }
    
    UuidFormat format = static_cast<UuidFormat>(convertToCombo->currentIndex());
    QString converted = formatUuid(uuid, format);
    convertResultEdit->setText(converted);
}

void UuidGen::onHistoryItemClicked(QTableWidgetItem *item)
{
    if (!item) return;
    
    int row = item->row();
    if (row >= 0 && row < uuidHistory.size()) {
        inputUuidEdit->setText(uuidHistory[row].uuid);
        mainTabWidget->setCurrentIndex(1);
    }
}

void UuidGen::onDeleteHistoryItemClicked()
{
    int row = historyTable->currentRow();
    if (row >= 0 && row < uuidHistory.size()) {
        uuidHistory.removeAt(row);
        updateHistoryTable();
    }
}

// 简化的空实现
void UuidGen::onVersionChanged() {}
void UuidGen::onFormatChanged() {}
void UuidGen::onCustomParametersChanged() {}
void UuidGen::onImportClicked() { QMessageBox::information(this, "提示", "功能开发中..."); }
void UuidGen::onGenerateFromNameClicked() { QMessageBox::information(this, "提示", "功能开发中..."); }
void UuidGen::onCompareUuidsClicked() { QMessageBox::information(this, "提示", "功能开发中..."); }
void UuidGen::saveSettings() {}
void UuidGen::loadSettings() {}
UuidVersion UuidGen::detectVersion(const QString &uuid) { Q_UNUSED(uuid); return UUID_V4; }
QString UuidGen::convertCase(const QString &uuid, bool uppercase) { return uppercase ? uuid.toUpper() : uuid.toLower(); }
QString UuidGen::generateMD5Hash(const QString &data) { return QCryptographicHash::hash(data.toUtf8(), QCryptographicHash::Md5).toHex(); }
QString UuidGen::generateSHA1Hash(const QString &data) { return QCryptographicHash::hash(data.toUtf8(), QCryptographicHash::Sha1).toHex(); }
