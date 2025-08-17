#include "datetimeutil.h"
#include "../../common/datetime-utils/datetimeutils.h"
#include <QDebug>
#include <QDateTime>

REGISTER_DYNAMICOBJECT(DateTimeUtil);

DateTimeUtil::DateTimeUtil() : QWidget(nullptr), DynamicObjectBase()
{
    setupUI();
    
    // 启动定时器更新当前时间
    updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &DateTimeUtil::updateCurrentTime);
    updateTimer->start(1000); // 每秒更新
    
    // 初始更新
    updateCurrentTime();
    populateTimezones();
}

void DateTimeUtil::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(15);

    // 创建选项卡widget
    QTabWidget *tabWidget = new QTabWidget;
    tabWidget->setStyleSheet(
        "QTabWidget::pane {"
        "    border: 2px solid #3498db;"
        "    border-radius: 0px;"
        "    margin-top: 5px;"
        "}"
        "QTabWidget::tab-bar {"
        "    alignment: left;"
        "}"
        "QTabBar::tab {"
        "    background-color: #ecf0f1;"
        "    color: #2c3e50;"
        "    padding: 8px 16px;"
        "    margin-right: 2px;"
        "    border-top-left-radius: 6px;"
        "    border-top-right-radius: 6px;"
        "    font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;"
        "    font-size: 10pt;"
        "}"
        "QTabBar::tab:selected {"
        "    background-color: #3498db;"
        "    color: white;"
        "}"
        "QTabBar::tab:hover {"
        "    background-color: #5dade2;"
        "    color: white;"
        "}"
    );

    // 当前时间页
    QWidget *currentTimeTab = new QWidget;
    setupCurrentTimeSection(currentTimeTab);
    tabWidget->addTab(currentTimeTab, "🕐 当前时间");

    // 转换页
    QWidget *conversionTab = new QWidget;
    setupConversionSection(conversionTab);
    tabWidget->addTab(conversionTab, "🔄 时间转换");

    // 批量转换页
    QWidget *batchTab = new QWidget;
    setupBatchSection(batchTab);
    tabWidget->addTab(batchTab, "📋 批量转换");

    // 时间计算页
    QWidget *calculationTab = new QWidget;
    setupCalculationSection(calculationTab);
    tabWidget->addTab(calculationTab, "🧮 时间计算");

    // 格式示例页
    QWidget *formatTab = new QWidget;
    setupFormatSection(formatTab);
    tabWidget->addTab(formatTab, "📝 格式示例");

    mainLayout->addWidget(tabWidget);
}

void DateTimeUtil::setupCurrentTimeSection(QWidget *parent)
{
    QVBoxLayout *currentLayout = new QVBoxLayout(parent);
    
    currentTimeLabel = new QLabel;
    currentTimestampLabel = new QLabel;
    
    QString labelStyle = 
        "QLabel {"
        "    font-family: 'Consolas', 'Monaco', 'Courier New', monospace;"
        "    font-size: 16px;"
        "    padding: 15px;"
        "    background-color: #f8f9fa;"
        "    border: 2px solid #e9ecef;"
        "    border-radius: 0px;"
        "    color: #495057;"
        "    margin: 5px 0;"
        "}";
    
    currentTimeLabel->setStyleSheet(labelStyle);
    currentTimestampLabel->setStyleSheet(labelStyle);
    currentTimeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    currentTimestampLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    
    currentLayout->addWidget(currentTimeLabel);
    currentLayout->addWidget(currentTimestampLabel);
    
    // 时区选择
    QHBoxLayout *timezoneLayout = new QHBoxLayout;
    QLabel *tzLabel = new QLabel("时区:");
    tzLabel->setStyleSheet("font-weight: bold; color: #2c3e50;");
    timezoneCombo = new QComboBox;
    timezoneCombo->setStyleSheet(
        "QComboBox {"
        "    padding: 5px;"
        "    border: 2px solid #bdc3c7;"
        "    border-radius: 0px;"
        "    background-color: white;"
        "}"
        "QComboBox:focus {"
        "    border-color: #3498db;"
        "}"
    );
    
    QPushButton *copyTsBtn = new QPushButton("复制时间戳");
    QPushButton *copyDtBtn = new QPushButton("复制日期时间");
    
    QString buttonStyle = 
        "QPushButton {"
        "    background-color: #27ae60;"
        "    color: white;"
        "    border: none;"
        "    padding: 8px 16px;"
        "    border-radius: 0px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background-color: #229954;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #1e8449;"
        "}";
    
    copyTsBtn->setStyleSheet(buttonStyle);
    copyDtBtn->setStyleSheet(buttonStyle);
    
    connect(copyTsBtn, &QPushButton::clicked, this, &DateTimeUtil::copyCurrentTimestamp);
    connect(copyDtBtn, &QPushButton::clicked, this, &DateTimeUtil::copyCurrentDateTime);
    connect(timezoneCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DateTimeUtil::onTimezoneChanged);
    
    timezoneLayout->addWidget(tzLabel);
    timezoneLayout->addWidget(timezoneCombo);
    timezoneLayout->addStretch();
    timezoneLayout->addWidget(copyTsBtn);
    timezoneLayout->addWidget(copyDtBtn);
    
    currentLayout->addLayout(timezoneLayout);
    currentLayout->addStretch();
}

void DateTimeUtil::setupConversionSection(QWidget *parent)
{
    QVBoxLayout *conversionLayout = new QVBoxLayout(parent);
    
    // 时间戳转日期
    QGroupBox *tsToDateGroup = new QGroupBox("时间戳转日期时间");
    tsToDateGroup->setStyleSheet(
        "QGroupBox {"
        "    font-weight: bold;"
        "    border: 2px solid #e74c3c;"
        "    border-radius: 0px;"
        "    margin-top: 10px;"
        "    padding-top: 10px;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 8px 0 8px;"
        "    color: #e74c3c;"
        "}"
    );
    
    QGridLayout *tsLayout = new QGridLayout(tsToDateGroup);
    
    QLabel *tsInputLabel = new QLabel("时间戳:");
    tsInputLabel->setStyleSheet("font-weight: bold; color: #2c3e50;");
    
    timestampInput = new QLineEdit;
    timestampInput->setPlaceholderText("输入时间戳...");
    timestampInput->setStyleSheet(
        "QLineEdit {"
        "    padding: 8px;"
        "    border: 2px solid #bdc3c7;"
        "    border-radius: 0px;"
        "    font-family: 'Consolas', monospace;"
        "    font-size: 14px;"
        "}"
        "QLineEdit:focus {"
        "    border-color: #e74c3c;"
        "}"
    );
    
    timestampUnitCombo = new QComboBox;
    timestampUnitCombo->addItems({"秒", "毫秒", "微秒"});
    
    insertCurrentTsBtn = new QPushButton("插入当前");
    insertCurrentTsBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #f39c12;"
        "    color: white;"
        "    border: none;"
        "    padding: 8px 12px;"
        "    border-radius: 0px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background-color: #e67e22;"
        "}"
    );
    
    timestampResult = new QTextEdit;
    timestampResult->setMaximumHeight(120);
    timestampResult->setStyleSheet(
        "QTextEdit {"
        "    font-family: 'Consolas', monospace;"
        "    font-size: 13px;"
        "    padding: 8px;"
        "    border: 2px solid #bdc3c7;"
        "    border-radius: 0px;"
        "    background-color: #f8f9fa;"
        "}"
    );
    timestampResult->setReadOnly(true);
    
    copyTimestampBtn = new QPushButton("复制结果");
    copyTimestampBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #27ae60;"
        "    color: white;"
        "    border: none;"
        "    padding: 8px 12px;"
        "    border-radius: 0px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background-color: #229954;"
        "}"
    );
    
    tsLayout->addWidget(tsInputLabel, 0, 0);
    tsLayout->addWidget(timestampInput, 0, 1);
    tsLayout->addWidget(timestampUnitCombo, 0, 2);
    tsLayout->addWidget(insertCurrentTsBtn, 0, 3);
    tsLayout->addWidget(new QLabel("转换结果:"), 1, 0);
    tsLayout->addWidget(timestampResult, 1, 1, 1, 2);
    tsLayout->addWidget(copyTimestampBtn, 1, 3);
    
    // 日期转时间戳
    QGroupBox *dateToTsGroup = new QGroupBox("日期时间转时间戳");
    dateToTsGroup->setStyleSheet(
        "QGroupBox {"
        "    font-weight: bold;"
        "    border: 2px solid #3498db;"
        "    border-radius: 0px;"
        "    margin-top: 10px;"
        "    padding-top: 10px;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 8px 0 8px;"
        "    color: #3498db;"
        "}"
    );
    
    QGridLayout *dateLayout = new QGridLayout(dateToTsGroup);
    
    QLabel *dateInputLabel = new QLabel("日期时间:");
    dateInputLabel->setStyleSheet("font-weight: bold; color: #2c3e50;");
    
    dateTimeInput = new QDateTimeEdit;
    dateTimeInput->setDateTime(QDateTime::currentDateTime());
    dateTimeInput->setDisplayFormat("yyyy-MM-dd hh:mm:ss");
    dateTimeInput->setStyleSheet(
        "QDateTimeEdit {"
        "    padding: 8px;"
        "    border: 2px solid #bdc3c7;"
        "    border-radius: 0px;"
        "    font-family: 'Consolas', monospace;"
        "    font-size: 14px;"
        "}"
        "QDateTimeEdit:focus {"
        "    border-color: #3498db;"
        "}"
    );
    
    dateTimeUnitCombo = new QComboBox;
    dateTimeUnitCombo->addItems({"秒", "毫秒", "微秒"});
    
    insertCurrentDtBtn = new QPushButton("插入当前");
    insertCurrentDtBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #f39c12;"
        "    color: white;"
        "    border: none;"
        "    padding: 8px 12px;"
        "    border-radius: 0px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background-color: #e67e22;"
        "}"
    );
    
    dateTimeResult = new QLineEdit;
    dateTimeResult->setReadOnly(true);
    dateTimeResult->setStyleSheet(
        "QLineEdit {"
        "    padding: 8px;"
        "    border: 2px solid #bdc3c7;"
        "    border-radius: 0px;"
        "    font-family: 'Consolas', monospace;"
        "    font-size: 14px;"
        "    background-color: #f8f9fa;"
        "}"
    );
    
    copyDateTimeBtn = new QPushButton("复制结果");
    copyDateTimeBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #27ae60;"
        "    color: white;"
        "    border: none;"
        "    padding: 8px 12px;"
        "    border-radius: 0px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background-color: #229954;"
        "}"
    );
    
    dateLayout->addWidget(dateInputLabel, 0, 0);
    dateLayout->addWidget(dateTimeInput, 0, 1);
    dateLayout->addWidget(dateTimeUnitCombo, 0, 2);
    dateLayout->addWidget(insertCurrentDtBtn, 0, 3);
    dateLayout->addWidget(new QLabel("转换结果:"), 1, 0);
    dateLayout->addWidget(dateTimeResult, 1, 1, 1, 2);
    dateLayout->addWidget(copyDateTimeBtn, 1, 3);
    
    conversionLayout->addWidget(tsToDateGroup);
    conversionLayout->addWidget(dateToTsGroup);
    conversionLayout->addStretch();
    
    // 连接信号
    connect(timestampInput, &QLineEdit::textChanged, this, &DateTimeUtil::onTimestampChanged);
    connect(timestampUnitCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DateTimeUtil::onTimestampChanged);
    connect(dateTimeInput, &QDateTimeEdit::dateTimeChanged, this, &DateTimeUtil::onDateTimeChanged);
    connect(dateTimeUnitCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DateTimeUtil::onDateTimeChanged);
    connect(insertCurrentTsBtn, &QPushButton::clicked, this, &DateTimeUtil::insertCurrentTimestamp);
    connect(insertCurrentDtBtn, &QPushButton::clicked, this, &DateTimeUtil::insertCurrentDateTime);
    connect(copyTimestampBtn, &QPushButton::clicked, this, &DateTimeUtil::copyTimestampResult);
    connect(copyDateTimeBtn, &QPushButton::clicked, this, &DateTimeUtil::copyDateTimeResult);
}

void DateTimeUtil::setupBatchSection(QWidget *parent)
{
    QVBoxLayout *batchLayout = new QVBoxLayout(parent);
    
    QGroupBox *batchGroup = new QGroupBox("批量时间转换");
    batchGroup->setStyleSheet(
        "QGroupBox {"
        "    font-weight: bold;"
        "    border: 2px solid #9b59b6;"
        "    border-radius: 0px;"
        "    margin-top: 10px;"
        "    padding-top: 10px;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 8px 0 8px;"
        "    color: #9b59b6;"
        "}"
    );
    
    QVBoxLayout *batchGroupLayout = new QVBoxLayout(batchGroup);
    
    // 模式选择
    QHBoxLayout *modeLayout = new QHBoxLayout;
    QLabel *modeLabel = new QLabel("转换模式:");
    modeLabel->setStyleSheet("font-weight: bold; color: #2c3e50;");
    
    batchModeCombo = new QComboBox;
    batchModeCombo->addItems({"时间戳转日期", "日期转时间戳"});
    
    QPushButton *processBatchBtn = new QPushButton("批量转换");
    processBatchBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #9b59b6;"
        "    color: white;"
        "    border: none;"
        "    padding: 8px 16px;"
        "    border-radius: 0px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background-color: #8e44ad;"
        "}"
    );
    
    modeLayout->addWidget(modeLabel);
    modeLayout->addWidget(batchModeCombo);
    modeLayout->addStretch();
    modeLayout->addWidget(processBatchBtn);
    
    // 输入输出区域
    QHBoxLayout *ioLayout = new QHBoxLayout;
    
    QVBoxLayout *inputLayout = new QVBoxLayout;
    QLabel *inputLabel = new QLabel("输入 (每行一个):");
    inputLabel->setStyleSheet("font-weight: bold; color: #2c3e50;");
    
    batchInput = new QTextEdit;
    batchInput->setPlaceholderText("输入要转换的时间戳或日期时间...\n例如:\n1640995200\n1640995260\n1640995320");
    batchInput->setStyleSheet(
        "QTextEdit {"
        "    font-family: 'Consolas', monospace;"
        "    font-size: 13px;"
        "    padding: 8px;"
        "    border: 2px solid #bdc3c7;"
        "    border-radius: 0px;"
        "}"
    );
    
    inputLayout->addWidget(inputLabel);
    inputLayout->addWidget(batchInput);
    
    QVBoxLayout *outputLayout = new QVBoxLayout;
    QLabel *outputLabel = new QLabel("输出结果:");
    outputLabel->setStyleSheet("font-weight: bold; color: #2c3e50;");
    
    batchOutput = new QTextEdit;
    batchOutput->setReadOnly(true);
    batchOutput->setStyleSheet(
        "QTextEdit {"
        "    font-family: 'Consolas', monospace;"
        "    font-size: 13px;"
        "    padding: 8px;"
        "    border: 2px solid #bdc3c7;"
        "    border-radius: 0px;"
        "    background-color: #f8f9fa;"
        "}"
    );
    
    outputLayout->addWidget(outputLabel);
    outputLayout->addWidget(batchOutput);
    
    ioLayout->addLayout(inputLayout);
    ioLayout->addLayout(outputLayout);
    
    batchGroupLayout->addLayout(modeLayout);
    batchGroupLayout->addLayout(ioLayout);
    
    batchLayout->addWidget(batchGroup);
    
    connect(processBatchBtn, &QPushButton::clicked, this, &DateTimeUtil::processBatchConversion);
}

void DateTimeUtil::setupCalculationSection(QWidget *parent)
{
    QVBoxLayout *calcLayout = new QVBoxLayout(parent);
    
    QGroupBox *calcGroup = new QGroupBox("时间差计算");
    calcGroup->setStyleSheet(
        "QGroupBox {"
        "    font-weight: bold;"
        "    border: 2px solid #f39c12;"
        "    border-radius: 0px;"
        "    margin-top: 10px;"
        "    padding-top: 10px;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 8px 0 8px;"
        "    color: #f39c12;"
        "}"
    );
    
    QGridLayout *calcGridLayout = new QGridLayout(calcGroup);
    
    QLabel *startLabel = new QLabel("开始时间:");
    startLabel->setStyleSheet("font-weight: bold; color: #2c3e50;");
    
    startDateTime = new QDateTimeEdit;
    startDateTime->setDateTime(QDateTime::currentDateTime());
    startDateTime->setDisplayFormat("yyyy-MM-dd hh:mm:ss");
    
    QLabel *endLabel = new QLabel("结束时间:");
    endLabel->setStyleSheet("font-weight: bold; color: #2c3e50;");
    
    endDateTime = new QDateTimeEdit;
    endDateTime->setDateTime(QDateTime::currentDateTime().addSecs(3600));
    endDateTime->setDisplayFormat("yyyy-MM-dd hh:mm:ss");
    
    QPushButton *calcBtn = new QPushButton("计算时间差");
    calcBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #f39c12;"
        "    color: white;"
        "    border: none;"
        "    padding: 8px 16px;"
        "    border-radius: 0px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background-color: #e67e22;"
        "}"
    );
    
    QLabel *resultLabel = new QLabel("计算结果:");
    resultLabel->setStyleSheet("font-weight: bold; color: #2c3e50;");
    
    timeDiffResult = new QTextEdit;
    timeDiffResult->setMaximumHeight(150);
    timeDiffResult->setReadOnly(true);
    timeDiffResult->setStyleSheet(
        "QTextEdit {"
        "    font-family: 'Consolas', monospace;"
        "    font-size: 13px;"
        "    padding: 8px;"
        "    border: 2px solid #bdc3c7;"
        "    border-radius: 0px;"
        "    background-color: #f8f9fa;"
        "}"
    );
    
    calcGridLayout->addWidget(startLabel, 0, 0);
    calcGridLayout->addWidget(startDateTime, 0, 1);
    calcGridLayout->addWidget(endLabel, 1, 0);
    calcGridLayout->addWidget(endDateTime, 1, 1);
    calcGridLayout->addWidget(calcBtn, 2, 0, 1, 2);
    calcGridLayout->addWidget(resultLabel, 3, 0);
    calcGridLayout->addWidget(timeDiffResult, 3, 1);
    
    calcLayout->addWidget(calcGroup);
    calcLayout->addStretch();
    
    connect(calcBtn, &QPushButton::clicked, this, &DateTimeUtil::calculateTimeDifference);
}

void DateTimeUtil::setupFormatSection(QWidget *parent)
{
    QVBoxLayout *formatLayout = new QVBoxLayout(parent);
    
    QGroupBox *formatGroup = new QGroupBox("常用时间格式示例");
    formatGroup->setStyleSheet(
        "QGroupBox {"
        "    font-weight: bold;"
        "    border: 2px solid #16a085;"
        "    border-radius: 0px;"
        "    margin-top: 10px;"
        "    padding-top: 10px;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 8px 0 8px;"
        "    color: #16a085;"
        "}"
    );
    
    QVBoxLayout *formatGroupLayout = new QVBoxLayout(formatGroup);
    
    formatExamples = new QTextEdit;
    formatExamples->setReadOnly(true);
    formatExamples->setStyleSheet(
        "QTextEdit {"
        "    font-family: 'Consolas', monospace;"
        "    font-size: 13px;"
        "    padding: 12px;"
        "    border: 2px solid #bdc3c7;"
        "    border-radius: 0px;"
        "    background-color: #f8f9fa;"
        "    line-height: 1.5;"
        "}"
    );
    
    // 设置格式示例内容
    QDateTime now = QDateTime::currentDateTime();
    QStringList examples = getCommonFormats(now);
    formatExamples->setText(examples.join("\n"));
    
    formatGroupLayout->addWidget(formatExamples);
    formatLayout->addWidget(formatGroup);
}

void DateTimeUtil::populateTimezones()
{
    QList<QByteArray> timezoneIds = QTimeZone::availableTimeZoneIds();
    
    // 添加一些常用时区
    QStringList commonTimezones = {
        "Local",
        "UTC",
        "Asia/Shanghai",
        "Asia/Tokyo", 
        "Europe/London",
        "America/New_York",
        "America/Los_Angeles"
    };
    
    timezoneCombo->addItem("本地时区", "Local");
    
    for (const QString &tzId : commonTimezones) {
        if (tzId != "Local") {
            QTimeZone tz(tzId.toUtf8());
            if (tz.isValid()) {
                QString displayName = QString("%1 (%2)").arg(tzId).arg(tz.displayName(QTimeZone::StandardTime));
                timezoneCombo->addItem(displayName, tzId);
            }
        }
    }
    
    timezoneCombo->setCurrentIndex(0); // 默认选择本地时区
}

void DateTimeUtil::updateCurrentTime()
{
    QDateTime currentTime;
    QString selectedTz = timezoneCombo->currentData().toString();
    
    if (selectedTz == "Local" || selectedTz.isEmpty()) {
        currentTime = QDateTime::currentDateTime();
    } else {
        QTimeZone tz(selectedTz.toUtf8());
        currentTime = QDateTime::currentDateTime().toTimeZone(tz);
    }
    
    qint64 timestamp = currentTime.toSecsSinceEpoch();
    qint64 timestampMs = currentTime.toMSecsSinceEpoch();
    
    QString timeStr = QString("📅 当前日期时间: %1").arg(currentTime.toString("yyyy-MM-dd hh:mm:ss dddd"));
    QString timestampStr = QString("⏰ 当前时间戳: %1 (秒) | %2 (毫秒)").arg(timestamp).arg(timestampMs);
    
    currentTimeLabel->setText(timeStr);
    currentTimestampLabel->setText(timestampStr);
}

void DateTimeUtil::onTimestampChanged()
{
    updateTimestampConversion();
}

void DateTimeUtil::onDateTimeChanged()
{
    updateDateTimeConversion();
}

void DateTimeUtil::updateTimestampConversion()
{
    QString timestampStr = timestampInput->text().trimmed();
    if (timestampStr.isEmpty()) {
        timestampResult->clear();
        return;
    }
    
    bool ok;
    qint64 timestamp = timestampStr.toLongLong(&ok);
    if (!ok) {
        timestampResult->setText("❌ 无效的时间戳格式");
        return;
    }
    
    // 根据单位转换
    QString unit = timestampUnitCombo->currentText();
    QDateTime dateTime;
    
    if (unit == "秒") {
        dateTime = QDateTime::fromSecsSinceEpoch(timestamp);
    } else if (unit == "毫秒") {
        dateTime = QDateTime::fromMSecsSinceEpoch(timestamp);
    } else if (unit == "微秒") {
        dateTime = QDateTime::fromMSecsSinceEpoch(timestamp / 1000);
    }
    
    if (!dateTime.isValid()) {
        timestampResult->setText("❌ 时间戳超出有效范围");
        return;
    }
    
    QStringList formats = getCommonFormats(dateTime);
    timestampResult->setText(formats.join("\n"));
}

void DateTimeUtil::updateDateTimeConversion()
{
    QDateTime dateTime = dateTimeInput->dateTime();
    QString unit = dateTimeUnitCombo->currentText();
    
    qint64 result;
    if (unit == "秒") {
        result = dateTime.toSecsSinceEpoch();
    } else if (unit == "毫秒") {
        result = dateTime.toMSecsSinceEpoch();
    } else if (unit == "微秒") {
        result = dateTime.toMSecsSinceEpoch() * 1000;
    }
    
    dateTimeResult->setText(QString::number(result));
}

void DateTimeUtil::processBatchConversion()
{
    QString input = batchInput->toPlainText().trimmed();
    if (input.isEmpty()) {
        batchOutput->setText("❌ 请输入要转换的内容");
        return;
    }
    
    QStringList lines = input.split('\n', Qt::SkipEmptyParts);
    QStringList results;
    
    QString mode = batchModeCombo->currentText();
    
    for (const QString &line : lines) {
        QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty()) continue;
        
        if (mode == "时间戳转日期") {
            bool ok;
            qint64 timestamp = trimmedLine.toLongLong(&ok);
            if (ok) {
                QDateTime dateTime = QDateTime::fromSecsSinceEpoch(timestamp);
                results.append(QString("%1 → %2").arg(trimmedLine).arg(dateTime.toString("yyyy-MM-dd hh:mm:ss")));
            } else {
                results.append(QString("%1 → ❌ 无效时间戳").arg(trimmedLine));
            }
        } else {
            QDateTime dateTime = QDateTime::fromString(trimmedLine, "yyyy-MM-dd hh:mm:ss");
            if (dateTime.isValid()) {
                results.append(QString("%1 → %2").arg(trimmedLine).arg(dateTime.toSecsSinceEpoch()));
            } else {
                results.append(QString("%1 → ❌ 无效日期格式").arg(trimmedLine));
            }
        }
    }
    
    batchOutput->setText(results.join("\n"));
}

void DateTimeUtil::calculateTimeDifference()
{
    QDateTime start = startDateTime->dateTime();
    QDateTime end = endDateTime->dateTime();
    
    if (start >= end) {
        timeDiffResult->setText("❌ 结束时间必须大于开始时间");
        return;
    }
    
    qint64 diffSecs = start.secsTo(end);
    qint64 diffMs = start.msecsTo(end);
    
    // 计算各种单位
    qint64 days = diffSecs / 86400;
    qint64 hours = (diffSecs % 86400) / 3600;
    qint64 minutes = (diffSecs % 3600) / 60;
    qint64 seconds = diffSecs % 60;
    
    QStringList results;
    results << QString("📊 时间差统计:");
    results << QString("总秒数: %1 秒").arg(diffSecs);
    results << QString("总毫秒数: %1 毫秒").arg(diffMs);
    results << QString("详细时间: %1天 %2小时 %3分钟 %4秒").arg(days).arg(hours).arg(minutes).arg(seconds);
    results << QString("总小时数: %1 小时").arg(QString::number(diffSecs / 3600.0, 'f', 2));
    results << QString("总天数: %1 天").arg(QString::number(diffSecs / 86400.0, 'f', 2));
    
    timeDiffResult->setText(results.join("\n"));
}

QStringList DateTimeUtil::getCommonFormats(const QDateTime &dateTime)
{
    QStringList formats;
    formats << QString("🗓️  标准格式: %1").arg(dateTime.toString("yyyy-MM-dd hh:mm:ss"));
    formats << QString("📅 ISO 8601: %1").arg(dateTime.toString(Qt::ISODate));
    formats << QString("🇺🇸 美式格式: %1").arg(dateTime.toString("MM/dd/yyyy hh:mm:ss AP"));
    formats << QString("🇪🇺 欧式格式: %1").arg(dateTime.toString("dd.MM.yyyy hh:mm:ss"));
    formats << QString("📝 中文格式: %1").arg(dateTime.toString("yyyy年MM月dd日 hh时mm分ss秒"));
    formats << QString("⏰ 时间戳(秒): %1").arg(dateTime.toSecsSinceEpoch());
    formats << QString("⚡ 时间戳(毫秒): %1").arg(dateTime.toMSecsSinceEpoch());
    return formats;
}

// 复制相关槽函数
void DateTimeUtil::copyCurrentTimestamp()
{
    QDateTime current = QDateTime::currentDateTime();
    QString timestamp = QString::number(current.toSecsSinceEpoch());
    QApplication::clipboard()->setText(timestamp);
    showMessage("已复制当前时间戳");
}

void DateTimeUtil::copyCurrentDateTime()
{
    QDateTime current = QDateTime::currentDateTime();
    QString dateTimeStr = current.toString("yyyy-MM-dd hh:mm:ss");
    QApplication::clipboard()->setText(dateTimeStr);
    showMessage("已复制当前日期时间");
}

void DateTimeUtil::copyTimestampResult()
{
    QString result = timestampResult->toPlainText();
    if (!result.isEmpty()) {
        QApplication::clipboard()->setText(result);
        showMessage("已复制转换结果");
    }
}

void DateTimeUtil::copyDateTimeResult()
{
    QString result = dateTimeResult->text();
    if (!result.isEmpty()) {
        QApplication::clipboard()->setText(result);
        showMessage("已复制转换结果");
    }
}

void DateTimeUtil::insertCurrentTimestamp()
{
    QDateTime current = QDateTime::currentDateTime();
    QString timestamp = QString::number(current.toSecsSinceEpoch());
    timestampInput->setText(timestamp);
}

void DateTimeUtil::insertCurrentDateTime()
{
    dateTimeInput->setDateTime(QDateTime::currentDateTime());
}

void DateTimeUtil::onTimezoneChanged()
{
    updateCurrentTime();
}

void DateTimeUtil::showMessage(const QString &message)
{
    // 可以在这里添加状态栏或其他提示方式
    qDebug() << "Message:" << message;
}