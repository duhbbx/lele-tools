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

    // 初始化进程和定时器
    pingProcess = nullptr;
    pingTimer = new QTimer(this);
    timeoutTimer = new QTimer(this);

    connect(pingTimer, &QTimer::timeout, this, &Ping::onPingFinished);
    connect(timeoutTimer, &QTimer::timeout, this, &Ping::onPingTimeout);

    setupUI();

    // 连接信号槽
    connect(startBtn, &QPushButton::clicked, this, &Ping::onStartPing);
    connect(stopBtn, &QPushButton::clicked, this, &Ping::onStopPing);
    connect(clearBtn, &QPushButton::clicked, this, &Ping::onClearResults);
    connect(copyBtn, &QPushButton::clicked, this, &Ping::onCopyResults);
    connect(resolveBtn, &QPushButton::clicked, this, &Ping::onResolveHost);
    connect(hostEdit, &QLineEdit::textChanged, this, &Ping::onHostChanged);

    // 设置默认值
    hostEdit->setText(tr("www.baidu.com"));
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

    startBtn = new QPushButton(tr("🚀 开始Ping"));
    startBtn->setFixedSize(110, 32);

    stopBtn = new QPushButton(tr("⏹️ 停止"));
    stopBtn->setFixedSize(80, 32);
    stopBtn->setEnabled(false);

    clearBtn = new QPushButton(tr("🗑️ 清空"));
    clearBtn->setFixedSize(80, 32);

    copyBtn = new QPushButton(tr("📋 复制"));
    copyBtn->setFixedSize(80, 32);

    statusLabel = new QLabel(tr("就绪"));
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

    // 设置布局顶对齐
    topLayout->setAlignment(Qt::AlignTop);

    // 可选：去掉多余间距
    topLayout->setSpacing(5);       // 控件间间距
    topLayout->setContentsMargins(0,0,0,0); // 外边距


    topLayout->addWidget(inputGroup);
    topLayout->addWidget(controlGroup);
    topLayout->addLayout(buttonLayout);




    mainSplitter->addWidget(topWidget);
    mainSplitter->addWidget(resultsWidget);
    mainSplitter->addWidget(statsGroup);

    mainSplitter->setStretchFactor(0, 0); // topWidget 不拉伸
    mainSplitter->setStretchFactor(1, 1); // resultsWidget 可拉伸
    mainSplitter->setStretchFactor(2, 0); // statsGroup 不拉伸


    // 添加到主布局
    mainLayout->addWidget(mainSplitter);
}

void Ping::setupInputArea() {
    inputGroup = new QGroupBox("🌐 目标主机");
    inputLayout = new QGridLayout(inputGroup);

    hostLabel = new QLabel(tr("主机地址:"));
    hostEdit = new QLineEdit();
    hostEdit->setPlaceholderText(tr("输入域名或IP地址"));

    resolveBtn = new QPushButton(tr("🔍 解析"));
    resolveBtn->setFixedSize(85, 32);

    ipLabel = new QLabel(tr("解析IP:"));
    ipValueLabel = new QLabel(tr("未解析"));

    inputLayout->addWidget(hostLabel, 0, 0);
    inputLayout->addWidget(hostEdit, 0, 1);
    inputLayout->addWidget(resolveBtn, 0, 2);
    inputLayout->addWidget(ipLabel, 1, 0);
    inputLayout->addWidget(ipValueLabel, 1, 1, 1, 2);

    inputGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

}

void Ping::setupControlArea() {
    controlGroup = new QGroupBox("⚙️ Ping参数");
    controlLayout = new QHBoxLayout(controlGroup);

    countLabel = new QLabel(tr("次数:"));
    countSpinBox = new QSpinBox();
    countSpinBox->setRange(1, 100);
    countSpinBox->setValue(4);

    intervalLabel = new QLabel(tr("间隔(ms):"));
    intervalSpinBox = new QSpinBox();
    intervalSpinBox->setRange(100, 10000);
    intervalSpinBox->setValue(1000);

    timeoutLabel = new QLabel(tr("超时(ms):"));
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

    controlGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

}

void Ping::setupResultsArea() {
    resultsWidget = new QWidget();
    resultsLayout = new QVBoxLayout(resultsWidget);

    resultsLabel = new QLabel(tr("📊 Ping结果"));
    resultsLabel->setStyleSheet("font-weight: bold; font-size: 10pt; color: #333;");

    resultsTable = new QTableWidget();
    resultsTable->setColumnCount(6);
    resultsTable->setHorizontalHeaderLabels(QStringList() << "序号" << "目标主机" << "IP地址" << "响应时间" << "TTL" << "状态");
    resultsTable->horizontalHeader()->setStretchLastSection(true);
    resultsTable->setAlternatingRowColors(true);
    resultsTable->verticalHeader()->setVisible(false);

    resultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    resultsLayout->addWidget(resultsLabel);
    resultsLayout->addWidget(resultsTable);
}

void Ping::setupStatisticsArea() {
    statsGroup = new QGroupBox("📈 统计信息");
    statsLayout = new QGridLayout(statsGroup);

    sentLabel = new QLabel(tr("已发送:"));
    sentValueLabel = new QLabel(tr("0"));
    receivedLabel = new QLabel(tr("已接收:"));
    receivedValueLabel = new QLabel(tr("0"));
    lossLabel = new QLabel(tr("丢包率:"));
    lossValueLabel = new QLabel(tr("0%"));
    minLabel = new QLabel(tr("最小时间:"));
    minValueLabel = new QLabel(tr("0ms"));
    maxLabel = new QLabel(tr("最大时间:"));
    maxValueLabel = new QLabel(tr("0ms"));
    avgLabel = new QLabel(tr("平均时间:"));
    avgValueLabel = new QLabel(tr("0ms"));

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
        QMessageBox::warning(this, tr("错误"), tr("请输入要Ping的主机地址"));
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
    ipValueLabel->setText(tr("未解析"));
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
        ipValueLabel->setText(tr("解析失败"));
        ipValueLabel->setStyleSheet("color: #f44336; font-weight: bold;");
        resolvedIP.clear();
    }
}

void Ping::startPingProcess() {
    if (pingProcess) {
        delete pingProcess;
    }

    pingProcess = new QProcess(this);
    connect(pingProcess, &QProcess::readyReadStandardOutput, this, &Ping::processPingOutput);
    connect(pingProcess, &QProcess::readyReadStandardError, this, &Ping::processPingError);
    connect(pingProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Ping::pingProcessFinished);

    // 构建ping命令
    QString program;
    QStringList arguments;

#ifdef Q_OS_WIN
    program = "ping";
    arguments << "-n" << "1";  // 发送一个包
    arguments << "-w" << QString::number(timeoutSpinBox->value());  // 超时时间(ms)

    // 检查是否需要指定包大小(可选)
    // arguments << "-l" << "32";  // 数据包大小

    arguments << currentHost;
#else
    // Linux/Unix/Mac
    program = "ping";
    arguments << "-c" << "1";  // 发送一个包
    arguments << "-W" << QString::number(timeoutSpinBox->value() / 1000);  // 超时时间(秒)
    arguments << currentHost;
#endif

    // 启动ping进程
    pingProcess->start(program, arguments);

    if (!pingProcess->waitForStarted()) {
        updateStatus("无法启动ping命令", true);
        onStopPing();
        return;
    }

    // 启动超时定时器
    timeoutTimer->start(timeoutSpinBox->value() + 1000);
}

void Ping::stopPingProcess() {
    if (pingProcess && pingProcess->state() != QProcess::NotRunning) {
        pingProcess->kill();
        pingProcess->waitForFinished(1000);
    }

    pingTimer->stop();
    timeoutTimer->stop();
}

void Ping::processPingOutput() {
    if (!pingProcess) return;

    QString output = QString::fromLocal8Bit(pingProcess->readAllStandardOutput());

    if (output.isEmpty()) return;

    // 解析ping输出
    PingResult result = parsePingOutput(output);
    result.host = currentHost;
    result.ip = resolvedIP.isEmpty() ? currentHost : resolvedIP;
    result.sequenceNumber = ++currentSequence;

    // 添加结果
    addPingResult(result);

    // 更新统计
    statistics.packetsSent++;
    if (result.success) {
        statistics.packetsReceived++;
        if (result.responseTime < statistics.minTime) {
            statistics.minTime = result.responseTime;
        }
        if (result.responseTime > statistics.maxTime) {
            statistics.maxTime = result.responseTime;
        }
    } else {
        statistics.packetsLost++;
    }

    updateStatistics();
}

void Ping::processPingError() {
    if (!pingProcess) return;

    QString error = QString::fromLocal8Bit(pingProcess->readAllStandardError());
    updateStatus("Ping错误: " + error, true);
}

void Ping::pingProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    timeoutTimer->stop();

    // 如果进程异常退出
    if (exitStatus == QProcess::CrashExit) {
        PingResult result;
        result.host = currentHost;
        result.ip = resolvedIP;
        result.sequenceNumber = ++currentSequence;
        result.success = false;
        result.errorMessage = "进程崩溃";
        addPingResult(result);

        statistics.packetsSent++;
        statistics.packetsLost++;
        updateStatistics();
    }
    // 如果有输出但是exitCode != 0，表示ping失败
    else if (exitCode != 0) {
        // 输出已经在processPingOutput中处理了
        // 如果没有输出，说明是超时或其他错误
        if (currentSequence == statistics.packetsSent) {
            // 已经处理过了
        } else {
            PingResult result;
            result.host = currentHost;
            result.ip = resolvedIP;
            result.sequenceNumber = ++currentSequence;
            result.success = false;
            result.errorMessage = "请求超时";
            addPingResult(result);

            statistics.packetsSent++;
            statistics.packetsLost++;
            updateStatistics();
        }
    }

    // 检查是否继续ping
    if (isPinging) {
        if (continuousCheck->isChecked() || currentSequence < countSpinBox->value()) {
            // 延迟后继续下一次ping
            pingTimer->start(intervalSpinBox->value());
        } else {
            // 完成所有ping
            onStopPing();
            updateStatus(QString("Ping完成，共发送%1个包").arg(statistics.packetsSent), false);
        }
    }
}

PingResult Ping::parsePingOutput(const QString& output) {
    PingResult result;
    result.success = false;
    result.responseTime = 0;
    result.ttl = 0;

#ifdef Q_OS_WIN
    // Windows ping输出格式:
    // 来自 14.215.177.38 的回复: 字节=32 时间=8ms TTL=54
    // 请求超时。

    QRegularExpression successRegex(R"(来自\s+([^\s]+)\s+的回复.*时间[=<](\d+)ms.*TTL=(\d+))");
    QRegularExpression timeoutRegex(R"(请求超时|Request timed out|Destination host unreachable|找不到主机)");

    QRegularExpressionMatch match = successRegex.match(output);
    if (match.hasMatch()) {
        result.success = true;
        result.ip = match.captured(1);
        result.responseTime = match.captured(2).toDouble();
        result.ttl = match.captured(3).toInt();
    } else if (timeoutRegex.match(output).hasMatch()) {
        result.success = false;
        result.errorMessage = "请求超时";
    }
#else
    // Linux/Mac ping输出格式:
    // 64 bytes from 14.215.177.38: icmp_seq=1 ttl=54 time=8.123 ms

    QRegularExpression successRegex(R"((\d+)\s+bytes from\s+([^:]+).*ttl=(\d+).*time=([\d.]+)\s*ms)");
    QRegularExpression timeoutRegex(R"(timeout|no answer|unreachable)");

    QRegularExpressionMatch match = successRegex.match(output);
    if (match.hasMatch()) {
        result.success = true;
        result.ip = match.captured(2);
        result.ttl = match.captured(3).toInt();
        result.responseTime = match.captured(4).toDouble();
    } else if (timeoutRegex.match(output).hasMatch()) {
        result.success = false;
        result.errorMessage = "请求超时";
    }
#endif

    return result;
}

void Ping::addPingResult(const PingResult& result) {
    pingResults.append(result);

    // 添加到表格
    int row = resultsTable->rowCount();
    resultsTable->insertRow(row);

    QTableWidgetItem* seqItem = new QTableWidgetItem(QString::number(result.sequenceNumber));
    QTableWidgetItem* hostItem = new QTableWidgetItem(result.host);
    QTableWidgetItem* ipItem = new QTableWidgetItem(result.ip);
    QTableWidgetItem* timeItem = new QTableWidgetItem(
        result.success ? QString("%1 ms").arg(result.responseTime, 0, 'f', 1) : "-"
    );
    QTableWidgetItem* ttlItem = new QTableWidgetItem(
        result.success ? QString::number(result.ttl) : "-"
    );
    QTableWidgetItem* statusItem = new QTableWidgetItem(
        result.success ? "✓ 成功" : "✗ " + result.errorMessage
    );

    // 设置居中对齐
    seqItem->setTextAlignment(Qt::AlignCenter);
    timeItem->setTextAlignment(Qt::AlignCenter);
    ttlItem->setTextAlignment(Qt::AlignCenter);
    statusItem->setTextAlignment(Qt::AlignCenter);

    // 设置颜色
    if (result.success) {
        statusItem->setForeground(QBrush(QColor("#4CAF50")));
    } else {
        statusItem->setForeground(QBrush(QColor("#f44336")));
    }

    resultsTable->setItem(row, 0, seqItem);
    resultsTable->setItem(row, 1, hostItem);
    resultsTable->setItem(row, 2, ipItem);
    resultsTable->setItem(row, 3, timeItem);
    resultsTable->setItem(row, 4, ttlItem);
    resultsTable->setItem(row, 5, statusItem);

    // 滚动到最新行
    resultsTable->scrollToBottom();

    // 更新状态
    QString status = result.success
        ? QString("Ping成功: %1ms").arg(result.responseTime, 0, 'f', 1)
        : QString("Ping失败: %1").arg(result.errorMessage);
    updateStatus(status, !result.success);
}

void Ping::updateStatistics() {
    // 计算丢包率
    if (statistics.packetsSent > 0) {
        statistics.lossPercentage = (double)statistics.packetsLost * 100.0 / statistics.packetsSent;
    } else {
        statistics.lossPercentage = 0.0;
    }

    // 计算平均响应时间
    if (statistics.packetsReceived > 0) {
        double totalTime = 0;
        for (const PingResult& result : pingResults) {
            if (result.success) {
                totalTime += result.responseTime;
            }
        }
        statistics.avgTime = totalTime / statistics.packetsReceived;
    } else {
        statistics.avgTime = 0.0;
        statistics.minTime = 0.0;
        statistics.maxTime = 0.0;
    }

    sentValueLabel->setText(QString::number(statistics.packetsSent));
    receivedValueLabel->setText(QString::number(statistics.packetsReceived));
    lossValueLabel->setText(QString("%1%").arg(statistics.lossPercentage, 0, 'f', 1));
    minValueLabel->setText(QString("%1ms").arg(statistics.minTime == 999999.0 ? 0.0 : statistics.minTime, 0, 'f', 1));
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
    // 定时器触发，开始下一次ping
    if (isPinging) {
        startPingProcess();
    }
}

void Ping::onPingTimeout() {
    // 超时处理
    if (pingProcess && pingProcess->state() != QProcess::NotRunning) {
        pingProcess->kill();

        PingResult result;
        result.host = currentHost;
        result.ip = resolvedIP;
        result.sequenceNumber = ++currentSequence;
        result.success = false;
        result.errorMessage = "请求超时";
        addPingResult(result);

        statistics.packetsSent++;
        statistics.packetsLost++;
        updateStatistics();

        // 继续下一次ping
        if (isPinging) {
            if (continuousCheck->isChecked() || currentSequence < countSpinBox->value()) {
                pingTimer->start(intervalSpinBox->value());
            } else {
                onStopPing();
                updateStatus(QString("Ping完成，共发送%1个包").arg(statistics.packetsSent), false);
            }
        }
    }
}
