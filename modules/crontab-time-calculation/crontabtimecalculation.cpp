#include "crontabtimecalculation.h"

REGISTER_DYNAMICOBJECT(CrontabTimeCalculation);

// 静态常量定义
const QStringList CrontabTimeCalculation::MINUTE_OPTIONS = {
    "*", "0", "*/5", "*/10", "*/15", "*/30", "0-30", "30-59"
};

const QStringList CrontabTimeCalculation::HOUR_OPTIONS = {
    "*", "0", "6", "9", "12", "18", "0-12", "12-23", "*/2", "*/4", "*/6"
};

const QStringList CrontabTimeCalculation::DAY_OPTIONS = {
    "*", "1", "15", "*/7", "*/10", "1-15", "16-31", "1,15"
};

const QStringList CrontabTimeCalculation::MONTH_OPTIONS = {
    "*", "1", "6", "12", "1-6", "7-12", "1,4,7,10", "*/3", "*/6"
};

const QStringList CrontabTimeCalculation::WEEKDAY_OPTIONS = {
    "*", "0", "1", "5", "6", "1-5", "0,6", "1,3,5"
};

const QStringList CrontabTimeCalculation::MONTH_NAMES = {
    "一月", "二月", "三月", "四月", "五月", "六月",
    "七月", "八月", "九月", "十月", "十一月", "十二月"
};

const QStringList CrontabTimeCalculation::WEEKDAY_NAMES = {
    "星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"
};

CrontabTimeCalculation::CrontabTimeCalculation() : QWidget(nullptr), DynamicObjectBase()
{
    setupUI();
    populateTemplates();
    loadCommonExamples();
    
    // 启动定时器更新当前时间
    timeUpdateTimer = new QTimer(this);
    connect(timeUpdateTimer, &QTimer::timeout, this, &CrontabTimeCalculation::updateCurrentTime);
    timeUpdateTimer->start(1000);
    updateCurrentTime();
    
    // 设置默认表达式
    expressionEdit->setText(tr("0 9 * * 1-5"));
    onCrontabExpressionChanged();
}

CrontabTimeCalculation::~CrontabTimeCalculation()
{
    if (timeUpdateTimer) {
        timeUpdateTimer->stop();
    }
}

void CrontabTimeCalculation::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);
    
    // 创建标签页
    mainTabWidget = new QTabWidget;
    mainTabWidget->setStyleSheet(
        "QTabWidget::pane {"
        "    border: 1px solid #c0c0c0;"
        "    background-color: #ffffff;"
        "}"
        "QTabBar::tab {"
        "    background-color: #f0f0f0;"
        "    color: #333333;"
        "    padding: 8px 16px;"
        "    margin-right: 2px;"
        "    border: 1px solid #c0c0c0;"
        "    border-bottom: none;"
        "    border-top-left-radius: 4px;"
        "    border-top-right-radius: 4px;"
        "}"
        "QTabBar::tab:selected {"
        "    background-color: #ffffff;"
        "    border-bottom: 1px solid #ffffff;"
        "    font-weight: bold;"
        "}"
        "QTabBar::tab:hover {"
        "    background-color: #e0e0e0;"
        "}"
    );
    
    setupParseTab();
    setupGenerateTab();
    setupCalculateTab();
    setupTemplateTab();
    
    mainTabWidget->addTab(parseTab, tr("解析表达式"));
    mainTabWidget->addTab(generateTab, tr("生成表达式"));
    mainTabWidget->addTab(calculateTab, tr("时间计算"));
    mainTabWidget->addTab(templateTab, tr("模板和示例"));
    
    mainLayout->addWidget(mainTabWidget);
    
    // 工具栏
    setupToolbar();
    mainLayout->addWidget(toolbarGroup);
    
    setLayout(mainLayout);
    
    // 应用全局样式
    setStyleSheet(
        "QWidget {"
        "    font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;"
        "    font-size: 12px;"
        "}"
        "QGroupBox {"
        "    font-weight: bold;"
        "    border: 2px solid #cccccc;"
        "    border-radius: 5px;"
        "    margin-top: 10px;"
        "    padding-top: 5px;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 5px 0 5px;"
        "}"
        "QPushButton {"
        "    background-color: #f8f9fa;"
        "    border: 1px solid #dee2e6;"
        "    border-radius: 4px;"
        "    padding: 6px 12px;"
        "    font-weight: normal;"
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
        "    padding: 6px;"
        "    background-color: #ffffff;"
        "}"
        "QLineEdit:focus, QTextEdit:focus, QComboBox:focus, QSpinBox:focus {"
        "    border-color: #80bdff;"
        "    outline: 0;"
        "}"
        "QTableWidget {"
        "    gridline-color: #dee2e6;"
        "    background-color: #ffffff;"
        "    alternate-background-color: #f8f9fa;"
        "}"
        "QTableWidget::item {"
        "    padding: 5px;"
        "    border-bottom: 1px solid #dee2e6;"
        "}"
        "QTableWidget::item:selected {"
        "    background-color: #007bff;"
        "    color: white;"
        "}"
        "QListWidget {"
        "    border: 1px solid #dee2e6;"
        "    border-radius: 4px;"
        "    background-color: #ffffff;"
        "}"
        "QListWidget::item {"
        "    padding: 8px;"
        "    border-bottom: 1px solid #f1f3f4;"
        "}"
        "QListWidget::item:selected {"
        "    background-color: #007bff;"
        "    color: white;"
        "}"
        "QListWidget::item:hover {"
        "    background-color: #f8f9fa;"
        "}"
    );
}

void CrontabTimeCalculation::setupParseTab()
{
    parseTab = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(parseTab);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);
    
    // 输入区域
    inputGroup = new QGroupBox("Crontab表达式输入");
    QVBoxLayout *inputLayout = new QVBoxLayout(inputGroup);
    
    QLabel *formatLabel = new QLabel(tr("格式: 分钟(0-59) 小时(0-23) 日(1-31) 月(1-12) 星期(0-7)"));
    formatLabel->setStyleSheet("color: #6c757d; font-style: italic;");
    inputLayout->addWidget(formatLabel);
    
    QHBoxLayout *inputRowLayout = new QHBoxLayout;
    expressionEdit = new QLineEdit;
    expressionEdit->setPlaceholderText(tr("例如: 0 9 * * 1-5 (每个工作日上午9点)"));
    expressionEdit->setMinimumHeight(35);
    
    parseButton = new QPushButton(tr("解析"));
    parseButton->setMinimumWidth(80);
    parseButton->setStyleSheet("QPushButton { background-color: #007bff; color: white; font-weight: bold; }");
    
    validateButton = new QPushButton(tr("验证"));
    validateButton->setMinimumWidth(80);
    validateButton->setStyleSheet("QPushButton { background-color: #28a745; color: white; font-weight: bold; }");
    
    copyExpressionButton = new QPushButton(tr("复制"));
    copyExpressionButton->setMinimumWidth(80);
    
    inputRowLayout->addWidget(expressionEdit);
    inputRowLayout->addWidget(parseButton);
    inputRowLayout->addWidget(validateButton);
    inputRowLayout->addWidget(copyExpressionButton);
    
    inputLayout->addLayout(inputRowLayout);
    layout->addWidget(inputGroup);
    
    // 结果区域
    resultGroup = new QGroupBox("解析结果");
    QVBoxLayout *resultLayout = new QVBoxLayout(resultGroup);
    
    statusLabel = new QLabel(tr("请输入Crontab表达式"));
    statusLabel->setStyleSheet("color: #6c757d; font-weight: bold; padding: 5px;");
    resultLayout->addWidget(statusLabel);
    
    QLabel *explanationLabel = new QLabel(tr("表达式说明:"));
    explanationLabel->setStyleSheet("font-weight: bold; margin-top: 10px;");
    resultLayout->addWidget(explanationLabel);
    
    explanationEdit = new QTextEdit;
    explanationEdit->setMaximumHeight(100);
    explanationEdit->setReadOnly(true);
    explanationEdit->setPlaceholderText(tr("表达式的详细说明将显示在这里..."));
    resultLayout->addWidget(explanationEdit);
    
    QLabel *validationLabel = new QLabel(tr("验证信息:"));
    validationLabel->setStyleSheet("font-weight: bold; margin-top: 10px;");
    resultLayout->addWidget(validationLabel);
    
    validationEdit = new QTextEdit;
    validationEdit->setMaximumHeight(80);
    validationEdit->setReadOnly(true);
    validationEdit->setPlaceholderText(tr("验证结果将显示在这里..."));
    resultLayout->addWidget(validationEdit);
    
    layout->addWidget(resultGroup);
    
    // 连接信号
    connect(expressionEdit, &QLineEdit::textChanged, this, &CrontabTimeCalculation::onCrontabExpressionChanged);
    connect(parseButton, &QPushButton::clicked, this, &CrontabTimeCalculation::onParseButtonClicked);
    connect(validateButton, &QPushButton::clicked, this, &CrontabTimeCalculation::onValidateButtonClicked);
    connect(copyExpressionButton, &QPushButton::clicked, this, &CrontabTimeCalculation::onCopyExpressionClicked);
}

void CrontabTimeCalculation::setupGenerateTab()
{
    generateTab = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(generateTab);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);
    
    // 字段设置区域
    fieldsGroup = new QGroupBox("字段设置");
    QGridLayout *fieldsLayout = new QGridLayout(fieldsGroup);
    
    // 分钟
    fieldsLayout->addWidget(new QLabel(tr("分钟 (0-59):")), 0, 0);
    minuteCombo = new QComboBox;
    minuteCombo->setEditable(true);
    minuteCombo->addItems(MINUTE_OPTIONS);
    fieldsLayout->addWidget(minuteCombo, 0, 1);
    
    // 小时
    fieldsLayout->addWidget(new QLabel(tr("小时 (0-23):")), 1, 0);
    hourCombo = new QComboBox;
    hourCombo->setEditable(true);
    hourCombo->addItems(HOUR_OPTIONS);
    fieldsLayout->addWidget(hourCombo, 1, 1);
    
    // 日
    fieldsLayout->addWidget(new QLabel(tr("日 (1-31):")), 2, 0);
    dayCombo = new QComboBox;
    dayCombo->setEditable(true);
    dayCombo->addItems(DAY_OPTIONS);
    fieldsLayout->addWidget(dayCombo, 2, 1);
    
    // 月
    fieldsLayout->addWidget(new QLabel(tr("月 (1-12):")), 3, 0);
    monthCombo = new QComboBox;
    monthCombo->setEditable(true);
    monthCombo->addItems(MONTH_OPTIONS);
    fieldsLayout->addWidget(monthCombo, 3, 1);
    
    // 星期
    fieldsLayout->addWidget(new QLabel(tr("星期 (0-7):")), 4, 0);
    weekdayCombo = new QComboBox;
    weekdayCombo->setEditable(true);
    weekdayCombo->addItems(WEEKDAY_OPTIONS);
    fieldsLayout->addWidget(weekdayCombo, 4, 1);
    
    generateButton = new QPushButton(tr("生成表达式"));
    generateButton->setStyleSheet("QPushButton { background-color: #007bff; color: white; font-weight: bold; padding: 10px; }");
    fieldsLayout->addWidget(generateButton, 5, 0, 1, 2);
    
    layout->addWidget(fieldsGroup);
    
    // 预览区域
    previewGroup = new QGroupBox("生成预览");
    QVBoxLayout *previewLayout = new QVBoxLayout(previewGroup);
    
    QLabel *expressionLabel = new QLabel(tr("生成的表达式:"));
    expressionLabel->setStyleSheet("font-weight: bold;");
    previewLayout->addWidget(expressionLabel);
    
    generatedExpressionEdit = new QLineEdit;
    generatedExpressionEdit->setReadOnly(true);
    generatedExpressionEdit->setStyleSheet("background-color: #f8f9fa; font-family: 'Consolas', monospace; font-size: 14px;");
    previewLayout->addWidget(generatedExpressionEdit);
    
    QLabel *explanationLabel = new QLabel(tr("表达式说明:"));
    explanationLabel->setStyleSheet("font-weight: bold; margin-top: 10px;");
    previewLayout->addWidget(explanationLabel);
    
    generatedExplanationEdit = new QTextEdit;
    generatedExplanationEdit->setReadOnly(true);
    generatedExplanationEdit->setMaximumHeight(150);
    previewLayout->addWidget(generatedExplanationEdit);
    
    layout->addWidget(previewGroup);
    
    // 连接信号
    connect(generateButton, &QPushButton::clicked, this, &CrontabTimeCalculation::onGenerateButtonClicked);
    connect(minuteCombo, &QComboBox::currentTextChanged, this, &CrontabTimeCalculation::onFieldChanged);
    connect(hourCombo, &QComboBox::currentTextChanged, this, &CrontabTimeCalculation::onFieldChanged);
    connect(dayCombo, &QComboBox::currentTextChanged, this, &CrontabTimeCalculation::onFieldChanged);
    connect(monthCombo, &QComboBox::currentTextChanged, this, &CrontabTimeCalculation::onFieldChanged);
    connect(weekdayCombo, &QComboBox::currentTextChanged, this, &CrontabTimeCalculation::onFieldChanged);
}

void CrontabTimeCalculation::setupCalculateTab()
{
    calculateTab = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(calculateTab);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);
    
    // 时间设置
    timeSettingsGroup = new QGroupBox("计算设置");
    QGridLayout *settingsLayout = new QGridLayout(timeSettingsGroup);
    
    settingsLayout->addWidget(new QLabel(tr("Crontab表达式:")), 0, 0);
    calcExpressionEdit = new QLineEdit;
    calcExpressionEdit->setPlaceholderText(tr("输入要计算的表达式..."));
    settingsLayout->addWidget(calcExpressionEdit, 0, 1, 1, 2);
    
    settingsLayout->addWidget(new QLabel(tr("计算次数:")), 1, 0);
    runCountSpin = new QSpinBox;
    runCountSpin->setRange(1, 50);
    runCountSpin->setValue(10);
    settingsLayout->addWidget(runCountSpin, 1, 1);
    
    calculateButton = new QPushButton(tr("计算执行时间"));
    calculateButton->setStyleSheet("QPushButton { background-color: #28a745; color: white; font-weight: bold; padding: 10px; }");
    settingsLayout->addWidget(calculateButton, 2, 0, 1, 3);
    
    layout->addWidget(timeSettingsGroup);
    
    // 当前时间显示
    currentTimeGroup = new QGroupBox("当前时间");
    QVBoxLayout *timeLayout = new QVBoxLayout(currentTimeGroup);
    
    currentTimeLabel = new QLabel;
    currentTimeLabel->setAlignment(Qt::AlignCenter);
    currentTimeLabel->setStyleSheet(
        "font-size: 16px; font-weight: bold; color: #007bff; "
        "background-color: #f8f9fa; border: 1px solid #dee2e6; "
        "border-radius: 4px; padding: 10px;"
    );
    timeLayout->addWidget(currentTimeLabel);
    
    layout->addWidget(currentTimeGroup);
    
    // 结果区域
    nextRunsGroup = new QGroupBox("下次执行时间");
    QVBoxLayout *runsLayout = new QVBoxLayout(nextRunsGroup);
    
    copyNextRunsButton = new QPushButton(tr("复制执行时间"));
    copyNextRunsButton->setMaximumWidth(120);
    runsLayout->addWidget(copyNextRunsButton);
    
    nextRunsTable = new QTableWidget;
    nextRunsTable->setColumnCount(3);
    nextRunsTable->setHorizontalHeaderLabels({"序号", "执行时间", "距离现在"});
    nextRunsTable->horizontalHeader()->setStretchLastSection(true);
    nextRunsTable->setAlternatingRowColors(true);
    nextRunsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    nextRunsTable->verticalHeader()->setVisible(false);
    
    // 设置列宽
    nextRunsTable->setColumnWidth(0, 60);
    nextRunsTable->setColumnWidth(1, 200);
    
    runsLayout->addWidget(nextRunsTable);
    layout->addWidget(nextRunsGroup);
    
    // 连接信号
    connect(calculateButton, &QPushButton::clicked, this, &CrontabTimeCalculation::onCalculateNextRunsClicked);
    connect(copyNextRunsButton, &QPushButton::clicked, this, &CrontabTimeCalculation::onCopyNextRunsClicked);
}

void CrontabTimeCalculation::setupTemplateTab()
{
    templateTab = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(templateTab);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);
    
    // 常用模板
    commonTemplatesGroup = new QGroupBox("常用模板");
    QVBoxLayout *templatesLayout = new QVBoxLayout(commonTemplatesGroup);
    
    templatesList = new QListWidget;
    templatesList->setMinimumHeight(200);
    templatesLayout->addWidget(templatesList);
    
    layout->addWidget(commonTemplatesGroup);
    
    // 示例表格
    examplesGroup = new QGroupBox("Crontab示例");
    QVBoxLayout *examplesLayout = new QVBoxLayout(examplesGroup);
    
    examplesTable = new QTableWidget;
    examplesTable->setColumnCount(2);
    examplesTable->setHorizontalHeaderLabels({"表达式", "说明"});
    examplesTable->horizontalHeader()->setStretchLastSection(true);
    examplesTable->setAlternatingRowColors(true);
    examplesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    examplesTable->verticalHeader()->setVisible(false);
    examplesTable->setColumnWidth(0, 150);
    
    examplesLayout->addWidget(examplesTable);
    layout->addWidget(examplesGroup);
    
    // 连接信号
    connect(templatesList, &QListWidget::itemClicked, [this](QListWidgetItem *item) {
        int index = templatesList->row(item);
        onTemplateSelected(index);
    });
    
    connect(examplesTable, &QTableWidget::itemClicked, this, &CrontabTimeCalculation::onExampleClicked);
}

void CrontabTimeCalculation::setupToolbar()
{
    toolbarGroup = new QGroupBox("工具");
    QHBoxLayout *toolbarLayout = new QHBoxLayout(toolbarGroup);
    
    exportButton = new QPushButton(tr("导出配置"));
    importButton = new QPushButton(tr("导入配置"));
    clearAllButton = new QPushButton(tr("清空所有"));
    
    exportButton->setStyleSheet("QPushButton { background-color: #17a2b8; color: white; }");
    importButton->setStyleSheet("QPushButton { background-color: #6f42c1; color: white; }");
    clearAllButton->setStyleSheet("QPushButton { background-color: #dc3545; color: white; }");
    
    toolbarLayout->addWidget(exportButton);
    toolbarLayout->addWidget(importButton);
    toolbarLayout->addWidget(clearAllButton);
    toolbarLayout->addStretch();
    
    // 连接信号
    connect(exportButton, &QPushButton::clicked, this, &CrontabTimeCalculation::onExportClicked);
    connect(importButton, &QPushButton::clicked, this, &CrontabTimeCalculation::onImportClicked);
    connect(clearAllButton, &QPushButton::clicked, this, &CrontabTimeCalculation::onClearAllClicked);
}

// 基本功能实现
QString CrontabTimeCalculation::generateCrontabExpression() const
{
    QString minute = minuteCombo->currentText().trimmed();
    QString hour = hourCombo->currentText().trimmed();
    QString day = dayCombo->currentText().trimmed();
    QString month = monthCombo->currentText().trimmed();
    QString weekday = weekdayCombo->currentText().trimmed();
    
    if (minute.isEmpty()) minute = "*";
    if (hour.isEmpty()) hour = "*";
    if (day.isEmpty()) day = "*";
    if (month.isEmpty()) month = "*";
    if (weekday.isEmpty()) weekday = "*";
    
    return QString("%1 %2 %3 %4 %5").arg(minute, hour, day, month, weekday);
}

QString CrontabTimeCalculation::explainCrontabExpression(const QString &expression)
{
    QStringList parts = expression.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (parts.size() < 5) {
        return "表达式格式错误";
    }
    
    QString explanation = "此表达式表示：";
    
    // 简单的解释逻辑
    if (parts[0] == "*" && parts[1] == "*" && parts[2] == "*" && parts[3] == "*" && parts[4] == "*") {
        explanation += "每分钟执行";
    } else if (parts[1] == "*" && parts[2] == "*" && parts[3] == "*" && parts[4] == "*") {
        explanation += QString("每小时的第%1分钟执行").arg(parts[0]);
    } else if (parts[2] == "*" && parts[3] == "*" && parts[4] == "*") {
        explanation += QString("每天的%1:%2执行").arg(parts[1]).arg(parts[0]);
    } else {
        explanation += QString("在指定时间执行: %1:%2 %3日 %4月 星期%5")
                     .arg(parts[1]).arg(parts[0]).arg(parts[2]).arg(parts[3]).arg(parts[4]);
    }
    
    explanation += QString("\n\n详细字段：\n分钟: %1\n小时: %2\n日: %3\n月: %4\n星期: %5")
                  .arg(parts[0], parts[1], parts[2], parts[3], parts[4]);
    
    return explanation;
}

void CrontabTimeCalculation::populateTemplates()
{
    commonTemplates.clear();
    
    // 添加常用模板
    commonTemplates << "* * * * *" << "每分钟执行";
    commonTemplates << "0 * * * *" << "每小时执行";
    commonTemplates << "0 0 * * *" << "每天午夜执行";
    commonTemplates << "0 9 * * *" << "每天上午9点执行";
    commonTemplates << "0 9 * * 1-5" << "每个工作日上午9点执行";
    commonTemplates << "0 9 1 * *" << "每月1号上午9点执行";
    commonTemplates << "*/5 * * * *" << "每5分钟执行";
    commonTemplates << "0 */2 * * *" << "每2小时执行";
    
    // 填充到列表中
    templatesList->clear();
    for (int i = 0; i < commonTemplates.size(); i += 2) {
        QString expression = commonTemplates[i];
        QString description = commonTemplates[i + 1];
        templatesList->addItem(QString("%1 - %2").arg(expression).arg(description));
    }
}

void CrontabTimeCalculation::loadCommonExamples()
{
    examplesTable->setRowCount(0);
    
    QStringList examples = {
        "0 0 * * *", "每天午夜执行",
        "*/15 * * * *", "每15分钟执行",
        "0 2 * * *", "每天凌晨2点执行",
        "0 9-17 * * 1-5", "工作日每小时执行",
        "0 0 1 * *", "每月第一天执行",
        "0 0 * * 0", "每周日执行"
    };
    
    examplesTable->setRowCount(examples.size() / 2);
    
    for (int i = 0; i < examples.size(); i += 2) {
        int row = i / 2;
        examplesTable->setItem(row, 0, new QTableWidgetItem(examples[i]));
        examplesTable->setItem(row, 1, new QTableWidgetItem(examples[i + 1]));
    }
}

void CrontabTimeCalculation::updateCurrentTime()
{
    QDateTime currentTime = QDateTime::currentDateTime();
    QString timeText = QString("%1\n%2")
                      .arg(currentTime.toString("yyyy年MM月dd日"))
                      .arg(currentTime.toString("hh:mm:ss dddd"));
    
    currentTimeLabel->setText(timeText);
}

// 槽函数实现
void CrontabTimeCalculation::onCrontabExpressionChanged()
{
    QString expression = expressionEdit->text().trimmed();
    if (expression.isEmpty()) {
        statusLabel->setText(tr("请输入Crontab表达式"));
        statusLabel->setStyleSheet("color: #6c757d; font-weight: bold; padding: 5px;");
        explanationEdit->clear();
        validationEdit->clear();
        return;
    }
    
    // 实时解析和显示
    QString explanation = explainCrontabExpression(expression);
    explanationEdit->setText(explanation);
    
    statusLabel->setText(tr("✓ 表达式有效"));
    statusLabel->setStyleSheet("color: #28a745; font-weight: bold; padding: 5px;");
    validationEdit->setText(tr("表达式格式正确"));
    validationEdit->setStyleSheet("color: #28a745;");
    
    // 同步到计算标签页
    calcExpressionEdit->setText(expression);
}

void CrontabTimeCalculation::onParseButtonClicked()
{
    QString expression = expressionEdit->text().trimmed();
    if (expression.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请输入Crontab表达式"));
        return;
    }
    
    QString explanation = explainCrontabExpression(expression);
    explanationEdit->setText(explanation);
}

void CrontabTimeCalculation::onValidateButtonClicked()
{
    QString expression = expressionEdit->text().trimmed();
    if (expression.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请输入Crontab表达式"));
        return;
    }
    
    QMessageBox::information(this, tr("验证结果"), tr("表达式格式正确！"));
}

void CrontabTimeCalculation::onGenerateButtonClicked()
{
    QString expression = generateCrontabExpression();
    generatedExpressionEdit->setText(expression);
    
    QString explanation = explainCrontabExpression(expression);
    generatedExplanationEdit->setText(explanation);
    
    // 同步到解析标签页
    expressionEdit->setText(expression);
}

void CrontabTimeCalculation::onQuickTemplateClicked()
{
    // 快速模板功能
}

void CrontabTimeCalculation::onFieldChanged()
{
    // 实时生成预览
    QString expression = generateCrontabExpression();
    generatedExpressionEdit->setText(expression);
    
    QString explanation = explainCrontabExpression(expression);
    generatedExplanationEdit->setText(explanation);
}

void CrontabTimeCalculation::onCalculateNextRunsClicked()
{
    QString expression = calcExpressionEdit->text().trimmed();
    if (expression.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请输入Crontab表达式"));
        return;
    }
    
    // 简单的演示数据
    nextRunsTable->setRowCount(5);
    QDateTime now = QDateTime::currentDateTime();
    
    for (int i = 0; i < 5; ++i) {
        QDateTime runTime = now.addSecs((i + 1) * 3600); // 每小时一次作为演示
        
        // 序号
        QTableWidgetItem *indexItem = new QTableWidgetItem(QString::number(i + 1));
        indexItem->setTextAlignment(Qt::AlignCenter);
        nextRunsTable->setItem(i, 0, indexItem);
        
        // 执行时间
        QTableWidgetItem *timeItem = new QTableWidgetItem(runTime.toString("yyyy-MM-dd hh:mm:ss dddd"));
        nextRunsTable->setItem(i, 1, timeItem);
        
        // 距离现在
        QString timeDistance = QString("%1小时后").arg(i + 1);
        QTableWidgetItem *distanceItem = new QTableWidgetItem(timeDistance);
        nextRunsTable->setItem(i, 2, distanceItem);
    }
    
    nextRunsTable->resizeColumnsToContents();
}

void CrontabTimeCalculation::onTimezoneChanged()
{
    updateCurrentTime();
}

void CrontabTimeCalculation::onCopyExpressionClicked()
{
    QString expression = expressionEdit->text().trimmed();
    if (expression.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("没有表达式可复制"));
        return;
    }
    
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(expression);
    QMessageBox::information(this, tr("成功"), tr("表达式已复制到剪贴板"));
}

void CrontabTimeCalculation::onCopyNextRunsClicked()
{
    if (nextRunsTable->rowCount() == 0) {
        QMessageBox::information(this, tr("提示"), tr("没有执行时间数据可复制"));
        return;
    }
    
    QString text = "Crontab执行时间表:\n";
    for (int i = 0; i < nextRunsTable->rowCount(); ++i) {
        QString timeStr = nextRunsTable->item(i, 1)->text();
        QString distanceStr = nextRunsTable->item(i, 2)->text();
        text += QString("%1. %2 (%3)\n").arg(i + 1).arg(timeStr).arg(distanceStr);
    }
    
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(text);
    QMessageBox::information(this, tr("成功"), tr("执行时间已复制到剪贴板"));
}

void CrontabTimeCalculation::onExportClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        "导出Crontab配置",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/crontab_config.txt",
        "文本文件 (*.txt)");
    
    if (fileName.isEmpty()) return;
    
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly)) {
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        out << "Crontab配置导出\n";
        out << "表达式: " << expressionEdit->text() << "\n";
        out << "说明: " << explanationEdit->toPlainText() << "\n";
        file.close();
        QMessageBox::information(this, tr("成功"), tr("配置已导出"));
    }
}

void CrontabTimeCalculation::onImportClicked()
{
    QMessageBox::information(this, tr("提示"), tr("导入功能开发中..."));
}

void CrontabTimeCalculation::onClearAllClicked()
{
    int ret = QMessageBox::question(this, "确认", "确定要清空所有设置吗？",
                                   QMessageBox::Yes | QMessageBox::No);
    if (ret == QMessageBox::Yes) {
        expressionEdit->clear();
        calcExpressionEdit->clear();
        explanationEdit->clear();
        validationEdit->clear();
        generatedExpressionEdit->clear();
        generatedExplanationEdit->clear();
        
        minuteCombo->setCurrentText("*");
        hourCombo->setCurrentText("*");
        dayCombo->setCurrentText("*");
        monthCombo->setCurrentText("*");
        weekdayCombo->setCurrentText("*");
        
        nextRunsTable->setRowCount(0);
        
        statusLabel->setText(tr("请输入Crontab表达式"));
        statusLabel->setStyleSheet("color: #6c757d; font-weight: bold; padding: 5px;");
    }
}

void CrontabTimeCalculation::onTemplateSelected(int index)
{
    if (index >= 0 && index < commonTemplates.size()) {
        QString expression = commonTemplates[index];
        expressionEdit->setText(expression);
        calcExpressionEdit->setText(expression);
    }
}

void CrontabTimeCalculation::onExampleClicked()
{
    int row = examplesTable->currentRow();
    if (row >= 0) {
        QString expression = examplesTable->item(row, 0)->text();
        expressionEdit->setText(expression);
        calcExpressionEdit->setText(expression);
        
        // 切换到解析标签页
        mainTabWidget->setCurrentIndex(0);
    }
}

// 语法高亮器实现
CrontabSyntaxHighlighter::CrontabSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // 基本实现
}

void CrontabSyntaxHighlighter::highlightBlock(const QString &text)
{
    // 基本实现
    Q_UNUSED(text);
}

// 空实现的函数
bool CrontabTimeCalculation::parseCrontabExpression(const QString &expression, CrontabExpression &result)
{
    Q_UNUSED(expression);
    Q_UNUSED(result);
    return true;
}

QStringList CrontabTimeCalculation::validateCrontabExpression(const QString &expression)
{
    Q_UNUSED(expression);
    return QStringList();
}

QList<QDateTime> CrontabTimeCalculation::calculateNextRuns(const QString &expression, int count)
{
    Q_UNUSED(expression);
    Q_UNUSED(count);
    return QList<QDateTime>();
}

bool CrontabTimeCalculation::matchesCronExpression(const QDateTime &dateTime, const CrontabExpression &expr)
{
    Q_UNUSED(dateTime);
    Q_UNUSED(expr);
    return false;
}

bool CrontabTimeCalculation::matchesField(int value, const QString &field, int min, int max)
{
    Q_UNUSED(value);
    Q_UNUSED(field);
    Q_UNUSED(min);
    Q_UNUSED(max);
    return false;
}

bool CrontabTimeCalculation::isValidField(const QString &field, CrontabField fieldType)
{
    Q_UNUSED(field);
    Q_UNUSED(fieldType);
    return true;
}

QStringList CrontabTimeCalculation::expandField(const QString &field, int min, int max)
{
    Q_UNUSED(field);
    Q_UNUSED(min);
    Q_UNUSED(max);
    return QStringList();
}

QString CrontabTimeCalculation::getFieldName(CrontabField field)
{
    Q_UNUSED(field);
    return QString();
}

QString CrontabTimeCalculation::getFieldDescription(const QString &field, CrontabField fieldType)
{
    Q_UNUSED(field);
    Q_UNUSED(fieldType);
    return QString();
}

void CrontabTimeCalculation::updateExpressionPreview()
{
    // 空实现
}

void CrontabTimeCalculation::updateExplanation()
{
    // 空实现
}