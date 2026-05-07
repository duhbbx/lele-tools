#include "ping.h"

#include <QAbstractSocket>
#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QFont>
#include <QFrame>
#include <QHeaderView>
#include <QMessageBox>
#include <QPainter>
#include <QPaintEvent>
#include <QRegularExpression>
#include <QTableWidgetItem>
#include <cmath>

REGISTER_DYNAMICOBJECT(Ping);

// ============= RttChart =============

RttChart::RttChart(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(120);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAutoFillBackground(false);
}

void RttChart::addPoint(double rttMs, bool success) {
    m_points.append({rttMs, success});
    while (m_points.size() > m_maxPoints) m_points.removeFirst();
    update();
}

void RttChart::clear() {
    m_points.clear();
    update();
}

void RttChart::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), QColor(0xfa, 0xfb, 0xfc));

    const int padL = 36, padR = 8, padT = 8, padB = 18;
    QRect plot(padL, padT, width() - padL - padR, height() - padT - padB);

    // 网格 + 轴
    p.setPen(QColor(0xe9, 0xec, 0xef));
    p.drawRect(plot);

    if (m_points.isEmpty()) {
        p.setPen(QColor(0xad, 0xb5, 0xbd));
        p.drawText(plot, Qt::AlignCenter, tr("（暂无数据）"));
        return;
    }

    // 计算 Y 范围
    double yMax = 0;
    for (const auto& pt : m_points) if (pt.ok && pt.rtt > yMax) yMax = pt.rtt;
    if (yMax <= 0) yMax = 10;
    yMax *= 1.15;

    // Y 标签
    p.setPen(QColor(0x86, 0x8e, 0x96));
    QFont f = p.font(); f.setPointSize(8); p.setFont(f);
    for (int i = 0; i <= 4; ++i) {
        int y = plot.bottom() - i * plot.height() / 4;
        p.setPen(QColor(0xf1, 0xf3, 0xf5));
        p.drawLine(plot.left(), y, plot.right(), y);
        p.setPen(QColor(0x86, 0x8e, 0x96));
        p.drawText(QRect(0, y - 8, padL - 4, 16),
                   Qt::AlignRight | Qt::AlignVCenter,
                   QString("%1ms").arg(yMax * i / 4.0, 0, 'f', 0));
    }

    // 折线
    int n = m_points.size();
    auto xAt = [&](int i) {
        return plot.left() + (n > 1 ? (i * plot.width() / (n - 1)) : plot.width() / 2);
    };
    auto yAt = [&](double v) {
        return plot.bottom() - int(v / yMax * plot.height());
    };

    QPolygonF line;
    for (int i = 0; i < n; ++i) {
        if (m_points[i].ok) line << QPointF(xAt(i), yAt(m_points[i].rtt));
    }
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(34, 139, 230), 2));
    p.setBrush(Qt::NoBrush);
    p.drawPolyline(line);

    // 失败点：底部红色短竖
    p.setPen(QPen(QColor(220, 50, 50), 2));
    for (int i = 0; i < n; ++i) {
        if (!m_points[i].ok) {
            int x = xAt(i);
            p.drawLine(x, plot.bottom() - 6, x, plot.bottom());
        }
    }

    // 成功点：小圆
    p.setBrush(QColor(34, 139, 230));
    p.setPen(Qt::NoPen);
    for (int i = 0; i < n; ++i) {
        if (m_points[i].ok) {
            p.drawEllipse(QPointF(xAt(i), yAt(m_points[i].rtt)), 2.5, 2.5);
        }
    }
}

// ============= Ping =============

Ping::Ping() : QWidget(nullptr), DynamicObjectBase(), isPinging(false), currentSequence(0),
               attemptsMade(0), useIPv6(false) {
    // 初始化统计数据
    statistics = { 0, 0, 0, 0.0, 999999.0, 0.0, 0.0, 0.0, "" };

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
    // 全局样式：让所有控件视觉一致
    setStyleSheet(R"(
        QWidget#pingRoot { background: #ffffff; }
        QLineEdit, QSpinBox {
            border: 1px solid #ced4da;
            border-radius: 4px;
            padding: 5px 8px;
            background: #fff;
            min-height: 22px;
            selection-background-color: #b8d4ff;
        }
        QLineEdit:focus, QSpinBox:focus { border-color: #228be6; }
        QPushButton {
            border: 1px solid #ced4da;
            border-radius: 4px;
            padding: 5px 14px;
            background: #ffffff;
            min-height: 22px;
            color: #343a40;
        }
        QPushButton:hover  { background: #f1f3f5; border-color: #adb5bd; }
        QPushButton:pressed{ background: #e9ecef; }
        QPushButton:disabled { color: #adb5bd; background: #f8f9fa; }

        QPushButton#primaryBtn {
            background: #228be6; border: 1px solid #1c7ed6; color: white; font-weight: bold;
        }
        QPushButton#primaryBtn:hover  { background: #1c7ed6; }
        QPushButton#primaryBtn:pressed{ background: #1971c2; }
        QPushButton#primaryBtn:disabled { background: #adb5bd; border-color: #adb5bd; color: #f1f3f5; }

        QPushButton#dangerBtn {
            background: #fa5252; border: 1px solid #f03e3e; color: white; font-weight: bold;
        }
        QPushButton#dangerBtn:hover  { background: #f03e3e; }
        QPushButton#dangerBtn:disabled { background: #adb5bd; border-color: #adb5bd; color: #f1f3f5; }

        QFrame#card {
            background: #f8f9fa; border: 1px solid #dee2e6; border-radius: 6px;
        }
        QLabel.fieldLabel { color: #495057; }
        QLabel.tileLabel  { color: #868e96; font-size: 9pt; }
        QLabel.tileValue  { color: #212529; font-size: 14pt; font-weight: bold; }
        QTableWidget {
            border: 1px solid #dee2e6;
            border-radius: 6px;
            background: #fff;
            gridline-color: #e9ecef;
        }
        QHeaderView::section {
            background: #f1f3f5; color: #495057; font-weight: bold;
            padding: 6px; border: none; border-right: 1px solid #dee2e6;
            border-bottom: 1px solid #dee2e6;
        }
    )");
    setObjectName("pingRoot");

    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 10, 12, 10);
    mainLayout->setSpacing(10);

    setupInputArea();
    setupControlArea();

    // 操作按钮：主按钮蓝、停止红、其他中性
    buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(6);

    startBtn = new QPushButton(tr("🚀 开始 Ping"));
    startBtn->setObjectName("primaryBtn");
    startBtn->setMinimumWidth(110);

    stopBtn = new QPushButton(tr("⏹ 停止"));
    stopBtn->setObjectName("dangerBtn");
    stopBtn->setMinimumWidth(80);
    stopBtn->setEnabled(false);

    clearBtn = new QPushButton(tr("🗑 清空"));
    clearBtn->setMinimumWidth(80);

    copyBtn = new QPushButton(tr("📋 复制"));
    copyBtn->setMinimumWidth(80);

    buttonLayout->addWidget(startBtn);
    buttonLayout->addWidget(stopBtn);
    buttonLayout->addWidget(clearBtn);
    buttonLayout->addWidget(copyBtn);
    buttonLayout->addStretch();

    // 状态条独立一行，更易读
    statusLabel = new QLabel(tr("就绪"));
    statusLabel->setStyleSheet(
        "color: #2e7d32; padding: 6px 10px; background: #e8f5e8;"
        " border-radius: 4px; font-weight: bold;");

    setupResultsArea();
    setupStatisticsArea();

    // 顶部容器：依次堆叠"主机卡 / 参数卡 / 按钮 / 状态条"
    QWidget* top = new QWidget();
    QVBoxLayout* tl = new QVBoxLayout(top);
    tl->setContentsMargins(0, 0, 0, 0);
    tl->setSpacing(8);
    tl->addWidget(inputGroup);
    tl->addWidget(controlGroup);
    tl->addLayout(buttonLayout);
    tl->addWidget(statusLabel);

    // 主体可拉伸：图表 + 表格在中间，统计固定在底
    mainSplitter = new QSplitter(Qt::Vertical);
    mainSplitter->setHandleWidth(6);
    mainSplitter->addWidget(top);
    mainSplitter->addWidget(resultsWidget);
    mainSplitter->addWidget(statsGroup);
    mainSplitter->setStretchFactor(0, 0);
    mainSplitter->setStretchFactor(1, 1);
    mainSplitter->setStretchFactor(2, 0);
    mainSplitter->setCollapsible(0, false);
    mainSplitter->setCollapsible(2, false);

    mainLayout->addWidget(mainSplitter);
}

void Ping::setupInputArea() {
    auto* card = new QFrame();
    card->setObjectName("card");
    auto* row = new QHBoxLayout(card);
    row->setContentsMargins(12, 10, 12, 10);
    row->setSpacing(8);

    hostLabel = new QLabel(tr("🌐 主机"));
    hostLabel->setStyleSheet("font-weight: bold; color: #495057; background: transparent;");

    hostEdit = new QLineEdit();
    hostEdit->setPlaceholderText(tr("输入域名或 IP 地址，如 baidu.com"));
    hostEdit->setMinimumWidth(220);

    resolveBtn = new QPushButton(tr("🔍 解析"));
    resolveBtn->setMinimumWidth(80);

    ipLabel = new QLabel(tr("→"));
    ipLabel->setStyleSheet("color: #adb5bd; font-size: 14pt; background: transparent;");

    ipValueLabel = new QLabel(tr("未解析"));
    ipValueLabel->setStyleSheet(
        "color: #868e96; padding: 4px 10px; background: #ffffff;"
        " border: 1px solid #dee2e6; border-radius: 4px;");

    row->addWidget(hostLabel);
    row->addWidget(hostEdit, 1);
    row->addWidget(resolveBtn);
    row->addWidget(ipLabel);
    row->addWidget(ipValueLabel);

    inputGroup = card;
    inputLayout = nullptr;
}

void Ping::setupControlArea() {
    auto* card = new QFrame();
    card->setObjectName("card");
    auto* row = new QHBoxLayout(card);
    row->setContentsMargins(12, 10, 12, 10);
    row->setSpacing(10);

    auto labelStyle = QStringLiteral("color:#495057; background: transparent;");

    countLabel = new QLabel(tr("⚙️ 次数"));
    countLabel->setStyleSheet("font-weight: bold; color: #495057; background: transparent;");
    countSpinBox = new QSpinBox();
    countSpinBox->setRange(1, 100);
    countSpinBox->setValue(4);
    countSpinBox->setMinimumWidth(70);

    intervalLabel = new QLabel(tr("间隔"));
    intervalLabel->setStyleSheet(labelStyle);
    intervalSpinBox = new QSpinBox();
    intervalSpinBox->setRange(100, 10000);
    intervalSpinBox->setValue(1000);
    intervalSpinBox->setSuffix(tr(" ms"));
    intervalSpinBox->setMinimumWidth(96);

    timeoutLabel = new QLabel(tr("超时"));
    timeoutLabel->setStyleSheet(labelStyle);
    timeoutSpinBox = new QSpinBox();
    timeoutSpinBox->setRange(1000, 30000);
    timeoutSpinBox->setValue(5000);
    timeoutSpinBox->setSuffix(tr(" ms"));
    timeoutSpinBox->setMinimumWidth(96);

    continuousCheck = new QCheckBox(tr("连续 Ping"));
    continuousCheck->setStyleSheet(labelStyle);

    row->addWidget(countLabel);
    row->addWidget(countSpinBox);
    row->addSpacing(10);
    row->addWidget(intervalLabel);
    row->addWidget(intervalSpinBox);
    row->addSpacing(10);
    row->addWidget(timeoutLabel);
    row->addWidget(timeoutSpinBox);
    row->addSpacing(10);
    row->addWidget(continuousCheck);
    row->addStretch();

    controlGroup = card;
    controlLayout = nullptr;
}

void Ping::setupResultsArea() {
    resultsWidget = new QWidget();
    resultsLayout = new QVBoxLayout(resultsWidget);

    resultsLabel = new QLabel(tr("📊 Ping 结果"));
    resultsLabel->setStyleSheet("font-weight: bold; font-size: 10pt; color: #333;");

    rttChart = new RttChart();

    resultsTable = new QTableWidget();
    resultsTable->setColumnCount(6);
    resultsTable->setHorizontalHeaderLabels(QStringList()
        << tr("序号") << tr("目标主机") << tr("IP地址") << tr("响应时间")
        << tr("TTL") << tr("状态"));
    resultsTable->horizontalHeader()->setStretchLastSection(true);
    resultsTable->setAlternatingRowColors(true);
    resultsTable->verticalHeader()->setVisible(false);
    resultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    resultsTable->setSortingEnabled(false);   // 排序会和"按时间到达顺序追加"冲突

    resultsLayout->addWidget(resultsLabel);
    resultsLayout->addWidget(rttChart);
    resultsLayout->addWidget(resultsTable, 1);
}

void Ping::setupStatisticsArea() {
    auto* card = new QFrame();
    card->setObjectName("card");
    auto* row = new QHBoxLayout(card);
    row->setContentsMargins(12, 10, 12, 10);
    row->setSpacing(8);

    // 创建一个"瓦片"：上面小灰标签，下面大粗值（无边框，纯靠白底与父卡片浅灰底对比）
    auto makeTile = [](const QString& title, QLabel*& valueOut) {
        auto* w = new QFrame();
        w->setStyleSheet(
            "QFrame { background:#ffffff; border: none; border-radius: 6px; }");
        auto* col = new QVBoxLayout(w);
        col->setContentsMargins(10, 6, 10, 6);
        col->setSpacing(2);
        auto* t = new QLabel(title);
        t->setStyleSheet("color: #868e96; font-size: 9pt; background: transparent;");
        valueOut = new QLabel("0");
        valueOut->setStyleSheet("color: #212529; font-size: 14pt; font-weight: bold; background: transparent;");
        col->addWidget(t);
        col->addWidget(valueOut);
        return w;
    };

    sentLabel = nullptr; receivedLabel = nullptr;
    lossLabel = nullptr; minLabel = nullptr; maxLabel = nullptr;
    avgLabel = nullptr; jitterLabel = nullptr;

    row->addWidget(makeTile(tr("已发送"),  sentValueLabel),    1);
    row->addWidget(makeTile(tr("已接收"),  receivedValueLabel),1);
    row->addWidget(makeTile(tr("丢包率"),  lossValueLabel),    1);
    row->addWidget(makeTile(tr("最小"),    minValueLabel),     1);
    row->addWidget(makeTile(tr("平均"),    avgValueLabel),     1);
    row->addWidget(makeTile(tr("最大"),    maxValueLabel),     1);
    row->addWidget(makeTile(tr("抖动"),    jitterValueLabel),  1);

    sentValueLabel->setText("0");
    receivedValueLabel->setText("0");
    lossValueLabel->setText("0%");
    minValueLabel->setText("0ms");
    avgValueLabel->setText("0ms");
    maxValueLabel->setText("0ms");
    jitterValueLabel->setText("0ms");

    statsGroup = card;
    statsLayout = nullptr;
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
    attemptsMade = 0;

    // 检测 IPv6（用解析结果，没解析过则跑一次）
    if (resolvedIP.isEmpty()) onResolveHost();
    QHostAddress addr(resolvedIP);
    useIPv6 = (addr.protocol() == QAbstractSocket::IPv6Protocol);

    startBtn->setEnabled(false);
    stopBtn->setEnabled(true);

    updateStatus(tr("正在 Ping %1 ...").arg(host), false);
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
    statistics = { 0, 0, 0, 0.0, 999999.0, 0.0, 0.0, 0.0, "" };
    rttChart->clear();
    updateStatistics();
    updateStatus(tr("已清空结果"), false);
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
    ipValueLabel->setStyleSheet(
        "color: #868e96; padding: 4px 10px; background: #ffffff;"
        " border: 1px solid #dee2e6; border-radius: 4px;");
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
        ipValueLabel->setStyleSheet(
            "color: #2e7d32; padding: 4px 10px; background: #e8f5e8;"
            " border: 1px solid #b7dfb9; border-radius: 4px; font-weight: bold;");
    } else {
        ipValueLabel->setText(tr("解析失败"));
        ipValueLabel->setStyleSheet(
            "color: #c62828; padding: 4px 10px; background: #ffebee;"
            " border: 1px solid #ffcdd2; border-radius: 4px; font-weight: bold;");
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

    int timeoutMs = qMax(100, timeoutSpinBox->value());

#ifdef Q_OS_WIN
    program = useIPv6 ? "ping" : "ping";
    if (useIPv6) arguments << "-6";
    arguments << "-n" << "1";
    arguments << "-w" << QString::number(timeoutMs);  // ms
    arguments << currentHost;
#else
    // macOS / Linux / BSD
    program = useIPv6 ? "ping6" : "ping";
    arguments << "-c" << "1";
    // 不向 ping 传超时（不同平台 -W 单位不一样：Linux 是秒、macOS 是毫秒）
    // 全部交给 timeoutTimer 兜底，结果一致。
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

    // 检查是否继续ping — 用 attemptsMade 而不是 currentSequence
    attemptsMade++;
    if (isPinging) {
        if (continuousCheck->isChecked() || attemptsMade < countSpinBox->value()) {
            pingTimer->start(intervalSpinBox->value());
        } else {
            onStopPing();
            updateStatus(tr("Ping 完成，共发送 %1 个包").arg(statistics.packetsSent), false);
        }
    }
}

PingResult Ping::parsePingOutput(const QString& output) {
    PingResult result;
    result.success = false;
    result.responseTime = 0;
    result.ttl = 0;

    // 不再按平台/语言区分 — 用三个通用 token 抓：
    //   time=Xms / time<1ms / 时间=Xms / time = X ms
    //   TTL=N / ttl=N / TTL: N
    //   from <ip>  / 来自 <ip>
    // 这些在 Linux/macOS/BSD/Windows 中、英文 ping 输出里都稳定。
    static const QRegularExpression rxTime(
        R"((?:time|时间)\s*[=<]\s*([\d.]+)\s*ms)",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression rxTtl(
        R"(ttl\s*[=:]\s*(\d+))",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression rxIp(
        R"((?:from|来自)\s+\(?([0-9a-fA-F:.]+))",
        QRegularExpression::CaseInsensitiveOption);

    auto mt = rxTime.match(output);
    if (mt.hasMatch()) {
        result.success = true;
        result.responseTime = mt.captured(1).toDouble();
        auto mttl = rxTtl.match(output);
        if (mttl.hasMatch()) result.ttl = mttl.captured(1).toInt();
        auto mip = rxIp.match(output);
        if (mip.hasMatch()) result.ip = mip.captured(1);
        return result;
    }

    // 失败：宽松关键字匹配（中英文都覆盖）
    static const QStringList kFailKeywords = {
        "timeout", "timed out", "unreachable", "no answer",
        "could not find host", "name or service not known",
        "请求超时", "找不到主机", "无法访问目标主机", "传送失败",
        "Destination Host Unreachable",
    };
    for (const QString& kw : kFailKeywords) {
        if (output.contains(kw, Qt::CaseInsensitive)) {
            result.errorMessage = tr("请求超时");
            return result;
        }
    }
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

    // 整行染色：成功淡绿、失败淡红
    QColor rowBg = result.success ? QColor(232, 245, 233) : QColor(255, 235, 238);
    QColor statusFg = result.success ? QColor("#2e7d32") : QColor("#c62828");
    statusItem->setForeground(QBrush(statusFg));

    resultsTable->setItem(row, 0, seqItem);
    resultsTable->setItem(row, 1, hostItem);
    resultsTable->setItem(row, 2, ipItem);
    resultsTable->setItem(row, 3, timeItem);
    resultsTable->setItem(row, 4, ttlItem);
    resultsTable->setItem(row, 5, statusItem);
    for (int c = 0; c < 6; ++c) {
        if (auto* it = resultsTable->item(row, c))
            it->setBackground(QBrush(rowBg));
    }

    // 喂入折线图
    rttChart->addPoint(result.success ? result.responseTime : 0.0, result.success);

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

    // 计算平均响应时间和抖动（标准差）
    if (statistics.packetsReceived > 0) {
        double totalTime = 0;
        for (const PingResult& r : pingResults) if (r.success) totalTime += r.responseTime;
        statistics.avgTime = totalTime / statistics.packetsReceived;

        double sqSum = 0;
        for (const PingResult& r : pingResults) {
            if (r.success) {
                double d = r.responseTime - statistics.avgTime;
                sqSum += d * d;
            }
        }
        statistics.jitter = std::sqrt(sqSum / statistics.packetsReceived);
    } else {
        statistics.avgTime = 0.0;
        statistics.jitter = 0.0;
        statistics.minTime = 0.0;
        statistics.maxTime = 0.0;
    }

    sentValueLabel->setText(QString::number(statistics.packetsSent));
    receivedValueLabel->setText(QString::number(statistics.packetsReceived));
    lossValueLabel->setText(QString("%1%").arg(statistics.lossPercentage, 0, 'f', 1));
    double showMin = (statistics.minTime == 999999.0) ? 0.0 : statistics.minTime;
    minValueLabel->setText(QString("%1ms").arg(showMin, 0, 'f', 1));
    maxValueLabel->setText(QString("%1ms").arg(statistics.maxTime, 0, 'f', 1));
    avgValueLabel->setText(QString("%1ms").arg(statistics.avgTime, 0, 'f', 1));
    jitterValueLabel->setText(QString("%1ms").arg(statistics.jitter, 0, 'f', 1));
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
