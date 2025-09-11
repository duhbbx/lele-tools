#include "ping.h"

#include <QDebug>
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QFont>
#include <QRegularExpression>

REGISTER_DYNAMICOBJECT(Ping);

Ping::Ping() : QWidget(nullptr), DynamicObjectBase(), isPinging(false), currentSequence(0) {
    // 初始化统计数据
    statistics = { 0, 0, 0, 0.0, 999999.0, 0.0, 0.0, "" };

    setupUI();

    // 连接信号槽
    connect(startBtn, &QPushButton::clicked, this, &Ping::onStartPing);
    connect(stopBtn, &QPushButton::clicked, this, &Ping::onStopPing);
    connect(clearBtn, &QPushButton::clicked, this, &Ping::onClearResults);
    connect(copyBtn, &QPushButton::clicked, this, &Ping::onCopyResults);
    connect(resolveBtn, &QPushButton::clicked, this, &Ping::onResolveHost);
    connect(hostEdit, &QLineEdit::textChanged, this, &Ping::onHostChanged);

    // 设置默认值
    hostEdit->setText("www.baidu.com");
    countSpinBox->setValue(4);
    intervalSpinBox->setValue(1000);
    timeoutSpinBox->setValue(5000);

    onResolveHost();
}

Ping::~Ping() {
    if (pingProcess && pingProcess->state() != QProcess::NotRunning) {
        pingProcess->kill();
        pingProcess->waitForFinished(3000);
    }
}

void Ping::setupUI() {
    // 主布局
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // 设置输入区域
    setupInputArea();

    // 设置控制区域
    setupControlArea();

    // 设置按钮区域
    buttonLayout = new QHBoxLayout();
    buttonLayout->setSizeConstraint(QLayout::SetFixedSize);

    startBtn = new QPushButton("🚀 开始Ping");
    startBtn->setFixedSize(110, 32);

    stopBtn = new QPushButton("⏹️ 停止");
    stopBtn->setFixedSize(80, 32);
    stopBtn->setEnabled(false);

    clearBtn = new QPushButton("🗑️ 清空");
    clearBtn->setFixedSize(80, 32);

    copyBtn = new QPushButton("📋 复制");
    copyBtn->setFixedSize(80, 32);

    statusLabel = new QLabel("就绪");
    statusLabel->setFixedHeight(32);
    statusLabel->setStyleSheet("color: #666; font-weight: bold; padding: 4px 8px; background: #f9f9f9; border-radius: 0px; border: 1px solid #ddd;");

    buttonLayout->addWidget(startBtn);
    buttonLayout->addWidget(stopBtn);
    buttonLayout->addWidget(clearBtn);
    buttonLayout->addWidget(copyBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(statusLabel);

    // 创建分割器
    mainSplitter = new QSplitter(Qt::Vertical);

    // 设置结果区域
    setupResultsArea();

    // 设置统计区域
    setupStatisticsArea();

    // 添加到分割器
    QWidget* topWidget = new QWidget();
    QVBoxLayout* topLayout = new QVBoxLayout(topWidget);
    topLayout->addWidget(inputGroup);
    topLayout->addWidget(controlGroup);
    topLayout->addLayout(buttonLayout);

    mainSplitter->addWidget(topWidget);
    mainSplitter->addWidget(resultsWidget);
    mainSplitter->addWidget(statsGroup);

    // 添加到主布局
    mainLayout->addWidget(mainSplitter);
}

void Ping::setupInputArea() {
    inputGroup = new QGroupBox("🌐 目标主机");
    inputLayout = new QGridLayout(inputGroup);

    hostLabel = new QLabel("主机地址:");
    hostEdit = new QLineEdit();
    hostEdit->setPlaceholderText("输入域名或IP地址");

    resolveBtn = new QPushButton("🔍 解析");
    resolveBtn->setFixedSize(85, 32);

    ipLabel = new QLabel("解析IP:");
    ipValueLabel = new QLabel("未解析");

    inputLayout->addWidget(hostLabel, 0, 0);
    inputLayout->addWidget(hostEdit, 0, 1);
    inputLayout->addWidget(resolveBtn, 0, 2);
    inputLayout->addWidget(ipLabel, 1, 0);
    inputLayout->addWidget(ipValueLabel, 1, 1, 1, 2);
}

void Ping::setupControlArea() {
    controlGroup = new QGroupBox("⚙️ Ping参数");
    controlLayout = new QHBoxLayout(controlGroup);

    countLabel = new QLabel("次数:");
    countSpinBox = new QSpinBox();
    countSpinBox->setRange(1, 100);
    countSpinBox->setValue(4);

    intervalLabel = new QLabel("间隔(ms):");
    intervalSpinBox = new QSpinBox();
    intervalSpinBox->setRange(100, 10000);
    intervalSpinBox->setValue(1000);

    timeoutLabel = new QLabel("超时(ms):");
    timeoutSpinBox = new QSpinBox();
    timeoutSpinBox->setRange(1000, 30000);
    timeoutSpinBox->setValue(5000);

    continuousCheck = new QCheckBox("连续Ping");

    controlLayout->addWidget(countLabel);
    controlLayout->addWidget(countSpinBox);
    controlLayout->addWidget(intervalLabel);
    controlLayout->addWidget(intervalSpinBox);
    controlLayout->addWidget(timeoutLabel);
    controlLayout->addWidget(timeoutSpinBox);
    controlLayout->addWidget(continuousCheck);
    controlLayout->addStretch();
}

void Ping::setupResultsArea() {
    resultsWidget = new QWidget();
    resultsLayout = new QVBoxLayout(resultsWidget);

    resultsLabel = new QLabel("📊 Ping结果");
    resultsLabel->setStyleSheet("font-weight: bold; font-size: 11pt; color: #333;");

    resultsTable = new QTableWidget();
    resultsTable->setColumnCount(6);
    resultsTable->setHorizontalHeaderLabels(QStringList() << "序号" << "目标主机" << "IP地址" << "响应时间" << "TTL" << "状态");
    resultsTable->horizontalHeader()->setStretchLastSection(true);
    resultsTable->setAlternatingRowColors(true);
    resultsTable->verticalHeader()->setVisible(false);

    resultsLayout->addWidget(resultsLabel);
    resultsLayout->addWidget(resultsTable);
}

void Ping::setupStatisticsArea() {
    statsGroup = new QGroupBox("📈 统计信息");
    statsGroup->setFixedHeight(120);
    statsLayout = new QGridLayout(statsGroup);

    sentLabel = new QLabel("已发送:");
    sentValueLabel = new QLabel("0");
    receivedLabel = new QLabel("已接收:");
    receivedValueLabel = new QLabel("0");
    lossLabel = new QLabel("丢包率:");
    lossValueLabel = new QLabel("0%");
    minLabel = new QLabel("最小时间:");
    minValueLabel = new QLabel("0ms");
    maxLabel = new QLabel("最大时间:");
    maxValueLabel = new QLabel("0ms");
    avgLabel = new QLabel("平均时间:");
    avgValueLabel = new QLabel("0ms");

    statsLayout->addWidget(sentLabel, 0, 0);
    statsLayout->addWidget(sentValueLabel, 0, 1);
    statsLayout->addWidget(receivedLabel, 0, 2);
    statsLayout->addWidget(receivedValueLabel, 0, 3);
    statsLayout->addWidget(lossLabel, 0, 4);
    statsLayout->addWidget(lossValueLabel, 0, 5);

    statsLayout->addWidget(minLabel, 1, 0);
    statsLayout->addWidget(minValueLabel, 1, 1);
    statsLayout->addWidget(maxLabel, 1, 2);
    statsLayout->addWidget(maxValueLabel, 1, 3);
    statsLayout->addWidget(avgLabel, 1, 4);
    statsLayout->addWidget(avgValueLabel, 1, 5);
}

void Ping::onStartPing() {
    QString host = hostEdit->text().trimmed();
    if (host.isEmpty()) {
        QMessageBox::warning(this, "错误", "请输入要Ping的主机地址");
        return;
    }

    currentHost = host;
    isPinging = true;
    currentSequence = 0;

    startBtn->setEnabled(false);
    stopBtn->setEnabled(true);

    updateStatus("正在Ping " + host + "...", false);
    startPingProcess();
}

void Ping::onStopPing() {
    stopPingProcess();
    startBtn->setEnabled(true);
    stopBtn->setEnabled(false);
    isPinging = false;
    updateStatus("Ping已停止", false);
}

void Ping::onClearResults() {
    resultsTable->setRowCount(0);
    pingResults.clear();
    statistics = { 0, 0, 0, 0.0, 999999.0, 0.0, 0.0, "" };
    updateStatistics();
    updateStatus("已清空结果", false);
}

void Ping::onCopyResults() {
    QString result = "Ping统计信息:\n";
    result += QString("目标主机: %1\n").arg(currentHost);
    result += QString("已发送: %1, 已接收: %2, 丢包率: %3%\n")
              .arg(statistics.packetsSent)
              .arg(statistics.packetsReceived)
              .arg(statistics.lossPercentage, 0, 'f', 1);

    QApplication::clipboard()->setText(result);
    updateStatus("结果已复制到剪贴板", false);
}

void Ping::onHostChanged() {
    ipValueLabel->setText("未解析");
    resolvedIP.clear();
}

void Ping::onResolveHost() {
    QString host = hostEdit->text().trimmed();
    if (host.isEmpty())
        return;

    QHostInfo info = QHostInfo::fromName(host);

    if (info.error() == QHostInfo::NoError && !info.addresses().isEmpty()) {
        resolvedIP = info.addresses().first().toString();
        ipValueLabel->setText(resolvedIP);
        ipValueLabel->setStyleSheet("color: #4CAF50; font-weight: bold;");
    } else {
        ipValueLabel->setText("解析失败");
        ipValueLabel->setStyleSheet("color: #f44336; font-weight: bold;");
        resolvedIP.clear();
    }
}

void Ping::startPingProcess() {
    // 简化实现：模拟ping结果
    updateStatus("Ping功能已简化实现", false);
}

void Ping::stopPingProcess() {
    // 简化实现
}

void Ping::processPingOutput() {
    // 简化实现
}

void Ping::processPingError() {
    // 简化实现
}

void Ping::pingProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitCode)
    Q_UNUSED(exitStatus)
}

PingResult Ping::parsePingOutput(const QString& output) {
    Q_UNUSED(output)
    PingResult result;
    return result;
}

void Ping::addPingResult(const PingResult& result) {
    Q_UNUSED(result)
}

void Ping::updateStatistics() {
    sentValueLabel->setText(QString::number(statistics.packetsSent));
    receivedValueLabel->setText(QString::number(statistics.packetsReceived));
    lossValueLabel->setText(QString("%1%").arg(statistics.lossPercentage, 0, 'f', 1));
    minValueLabel->setText(QString("%1ms").arg(statistics.minTime, 0, 'f', 1));
    maxValueLabel->setText(QString("%1ms").arg(statistics.maxTime, 0, 'f', 1));
    avgValueLabel->setText(QString("%1ms").arg(statistics.avgTime, 0, 'f', 1));
}

void Ping::updateStatus(const QString& message, bool isError) {
    statusLabel->setText(message);
    QString color = isError ? "#d32f2f" : "#2e7d32";
    QString bg = isError ? "#ffebee" : "#e8f5e8";
    statusLabel->setStyleSheet(QString("color: %1; font-weight: bold; padding: 4px 8px; background: %2; border-radius: 0px;").arg(color, bg));
}

void Ping::onPingFinished() {
}

void Ping::onPingTimeout() {
}
