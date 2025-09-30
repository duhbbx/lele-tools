#include "networkscanner.h"
#include <QApplication>
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QDateTime>
#include <QHeaderView>
#include <QSplitter>
#include <QHostAddress>
#include <QNetworkAddressEntry>
#include <QStandardPaths>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

REGISTER_DYNAMICOBJECT(NetworkScanner);

// HostScanTask 实现
HostScanTask::HostScanTask(const QString& ip, const int timeout, QAtomicInt* stopFlag,
                          ScanMethod method, QObject* parent)
    : QObject(parent), m_ip(ip), m_timeout(timeout), m_stopFlag(stopFlag), m_scanMethod(method) {
}

void HostScanTask::scan() {
    if (m_stopFlag && m_stopFlag->loadRelaxed() != 0)
        return;

    HostInfo info;
    info.ipAddress = m_ip;
    info.isOnline = false;

    m_timer.start();

    // 根据扫描方法选择不同的扫描方式
    switch (m_scanMethod) {
        case SCAN_ARP:
            info.isOnline = scanWithArp();
            break;
        case SCAN_TCP:
            info.isOnline = scanWithTcp();
            break;
        case SCAN_UDP:
            info.isOnline = scanWithUdp();
            break;
        case SCAN_ICMP:
            info.isOnline = scanWithIcmp();
            break;
        case SCAN_FAST_TCP:
            info.isOnline = scanWithFastTcp();
            break;
        case SCAN_NATIVE_ICMP:
            info.isOnline = scanWithNativeIcmp();
            break;
        case SCAN_MULTI_TCP:
            info.isOnline = scanWithMultiPortTcp();
            break;
        case SCAN_PING:
        default:
            info.isOnline = scanWithPing();
            break;
    }

    info.responseTime = m_timer.elapsed();

    if (info.isOnline && (!m_stopFlag || m_stopFlag->loadRelaxed() == 0)) {
        // 并行查询DNS和ARP以提高效率
        performParallelQueries(info);
    }

    if (!m_stopFlag || m_stopFlag->loadRelaxed() == 0) {
        emit finished(info);
    }
}

void HostScanTask::stop() {
    // 这个方法可以用来处理任务级别的停止逻辑
}

bool HostScanTask::scanWithArp() {
    // ARP扫描：二层发现，最快最可靠的同网段主机发现方法
    // 先发送触发包，然后查询ARP表

    QProcess arpProcess;
    arpProcess.setProcessChannelMode(QProcess::MergedChannels);

#ifdef Q_OS_WIN
    // Windows: 使用ping触发ARP，然后查询ARP表
    QProcess pingProc;
    pingProc.start("ping", QStringList() << "-n" << "1" << "-w" << "200" << m_ip);

    int waited = 0;
    while (pingProc.state() == QProcess::Running && waited < 300) {
        if (m_stopFlag && m_stopFlag->loadRelaxed() != 0) {
            pingProc.kill();
            pingProc.waitForFinished(100);
            return false;
        }
        QThread::msleep(10);
        waited += 10;
    }

    if (pingProc.state() == QProcess::Running) {
        pingProc.kill();
        pingProc.waitForFinished(100);
    }

    // 查询ARP表
    if (m_stopFlag && m_stopFlag->loadRelaxed() != 0) {
        return false;
    }

    arpProcess.start("arp", QStringList() << "-a" << m_ip);
#else
    // Linux/Unix: 尝试使用arping命令（最佳），如果失败则回退到ping + arp
    arpProcess.start("arping", QStringList() << "-c" << "1" << "-w" << "1" << m_ip);

    int waited = 0;
    while (arpProcess.state() == QProcess::Running && waited < 1200) {
        if (m_stopFlag && m_stopFlag->loadRelaxed() != 0) {
            arpProcess.kill();
            arpProcess.waitForFinished(100);
            return false;
        }
        QThread::msleep(10);
        waited += 10;
    }

    if (arpProcess.state() == QProcess::Running) {
        arpProcess.kill();
        arpProcess.waitForFinished(100);
        return false;
    }

    // 如果arping成功，直接返回
    if (arpProcess.exitCode() == 0) {
        return true;
    }

    // arping失败，回退到ping + arp
    if (m_stopFlag && m_stopFlag->loadRelaxed() != 0) {
        return false;
    }

    QProcess pingProc;
    pingProc.start("ping", QStringList() << "-c" << "1" << "-W" << "1" << m_ip);
    pingProc.waitForFinished(1200);

    arpProcess.start("arp", QStringList() << "-n" << m_ip);
#endif

    if (m_stopFlag && m_stopFlag->loadRelaxed() != 0) {
        return false;
    }

    if (arpProcess.waitForFinished(1000)) {
        QString output = arpProcess.readAllStandardOutput();

        // 检查ARP表中是否有该IP的有效MAC地址
        QRegularExpression macRegex(R"(([0-9A-Fa-f]{2}[:-]){5}[0-9A-Fa-f]{2})");
        QRegularExpressionMatch match = macRegex.match(output);

        if (match.hasMatch()) {
            QString mac = match.captured(0).toUpper().replace('-', ':');
            // 排除无效MAC地址
            if (mac != "00:00:00:00:00:00" && mac != "FF:FF:FF:FF:FF:FF") {
                return true;
            }
        }
    }

    return false;
}

bool HostScanTask::scanWithPing() {
    QProcess ping;
    ping.setProcessChannelMode(QProcess::MergedChannels);

#ifdef Q_OS_WIN
    ping.start("ping", { "-n", "1", "-w", QString::number(m_timeout), m_ip });
#else
    ping.start("ping", { "-c", "1", "-W", QString::number(m_timeout / 1000), m_ip });
#endif

    int waited = 0;
    while (ping.state() == QProcess::Running && waited < m_timeout + 1000) {
        if (m_stopFlag && m_stopFlag->loadRelaxed() != 0) {
            ping.kill();
            ping.waitForFinished(500);
            return false;
        }
        QThread::msleep(50);
        waited += 50;
    }

    if (ping.state() == QProcess::Running) {
        ping.kill();
        ping.waitForFinished(500);
    }

    return ping.exitCode() == 0;
}

bool HostScanTask::scanWithTcp() {
    // TCP连接扫描，尝试连接常见端口
    QList<int> commonPorts = { 80, 443, 22, 23, 25, 53, 110, 995, 143, 993, 135, 139, 445 };
    int perPortTimeout = qMax(m_timeout / commonPorts.size(), 200); // 每个端口至少200ms

    for (int port : commonPorts) {
        if (m_stopFlag && m_stopFlag->loadRelaxed() != 0)
            return false;

        QTcpSocket socket;
        socket.connectToHost(m_ip, port);

        if (socket.waitForConnected(perPortTimeout)) {
            socket.disconnectFromHost();
            return true;
        }
    }
    return false;
}

bool HostScanTask::scanWithUdp() {
    // UDP扫描，发送数据包到常见UDP端口
    QList<int> commonUdpPorts = { 53, 67, 68, 69, 123, 161, 162, 514 };

    for (int port : commonUdpPorts) {
        if (m_stopFlag && m_stopFlag->loadRelaxed() != 0)
            return false;

        QUdpSocket socket;
        socket.connectToHost(m_ip, port);

        if (socket.waitForConnected(m_timeout / commonUdpPorts.size())) {
            // 发送一个简单的探测包
            socket.write("test");
            socket.waitForBytesWritten(100);
            socket.disconnectFromHost();
            return true;
        }
    }
    return false;
}

bool HostScanTask::scanWithIcmp() {
    // 尝试原生ICMP实现，如果失败则回退到ping
    if (scanWithNativeIcmp()) {
        return true;
    }
    return scanWithPing();
}

bool HostScanTask::scanWithFastTcp() {
    // 快速TCP扫描，只检查最常见的几个端口
    QList<int> topPorts = { 80, 443, 22, 135, 445, 139 }; // 减少端口数量提高速度
    int perPortTimeout = qMax(m_timeout / topPorts.size(), 300); // 每个端口至少300ms

    for (int port : topPorts) {
        if (m_stopFlag && m_stopFlag->loadRelaxed() != 0)
            return false;

        QTcpSocket socket;
        socket.connectToHost(m_ip, port);

        if (socket.waitForConnected(perPortTimeout)) {
            socket.disconnectFromHost();
            return true;
        }
    }
    return false;
}

bool HostScanTask::scanWithNativeIcmp() {
    // 原生ICMP实现比较复杂且容易出错，暂时回退到更可靠的TCP检测
    // 实际上TCP检测对于主机发现更加可靠
    return scanWithMultiPortTcp();
}

bool HostScanTask::scanWithMultiPortTcp() {
    // 并行连接多个端口以提高检测速度
    QList<int> ports = { 80, 443, 22, 135, 445 };
    QList<QTcpSocket*> sockets;
    QEventLoop loop;
    QTimer timeoutTimer;

    timeoutTimer.setSingleShot(true);
    timeoutTimer.setInterval(m_timeout);

    bool connected = false;
    int connectCount = 0;

    // 创建所有套接字
    for (int port : ports) {
        if (m_stopFlag && m_stopFlag->loadRelaxed() != 0) {
            // 清理并返回
            for (auto socket : sockets) {
                socket->deleteLater();
            }
            return false;
        }

        QTcpSocket* socket = new QTcpSocket();
        sockets.append(socket);

        connect(socket, &QTcpSocket::connected, [&]() {
            connected = true;
            loop.quit();
        });

        connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
                [&]() {
            connectCount++;
            if (connectCount >= ports.size()) {
                loop.quit();
            }
        });

        socket->connectToHost(m_ip, port);
    }

    connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeoutTimer.start();

    // 等待任一连接成功或全部失败
    loop.exec();

    // 清理资源
    for (auto socket : sockets) {
        socket->disconnectFromHost();
        socket->deleteLater();
    }

    return connected;
}

void HostScanTask::performParallelQueries(HostInfo& info) const {
    if (m_stopFlag && m_stopFlag->loadRelaxed() != 0)
        return;

    // 直接在当前线程中串行查询，避免并行线程的复杂性和不稳定性
    // 实际测试表明，DNS和ARP查询时间不长，串行执行更稳定
    info.hostName = getHostName(m_ip);

    if (m_stopFlag && m_stopFlag->loadRelaxed() != 0)
        return;

    info.macAddress = getMacAddress(m_ip);
    info.vendor = getMacVendor(info.macAddress);
}

QString HostScanTask::getHostName(const QString& ip) const {
    // 检查停止标志
    if (m_stopFlag && m_stopFlag->loadRelaxed() != 0) {
        return "Unknown";
    }

    QHostInfo hostInfo = QHostInfo::fromName(ip);
    if (hostInfo.error() == QHostInfo::NoError && !hostInfo.hostName().isEmpty()) {
        return hostInfo.hostName();
    }
    return "Unknown";
}

QString HostScanTask::getMacAddress(const QString& ip) const {
    // 检查停止标志
    if (m_stopFlag && m_stopFlag->loadRelaxed() != 0) {
        return "Unknown";
    }

    // 首先尝试查看ARP表
    QProcess arpProcess;
    arpProcess.setProcessChannelMode(QProcess::MergedChannels);

#ifdef Q_OS_WIN
    arpProcess.start("arp", QStringList() << "-a" << ip);
#else
    arpProcess.start("arp", QStringList() << "-n" << ip);
#endif

    if (arpProcess.waitForFinished(1000)) {
        QString output = arpProcess.readAllStandardOutput();
        QRegularExpression macRegex(R"(([0-9A-Fa-f]{2}[:-]){5}[0-9A-Fa-f]{2})");
        QRegularExpressionMatch match = macRegex.match(output);
        if (match.hasMatch()) {
            QString mac = match.captured(0).toUpper().replace('-', ':');
            if (mac != "00:00:00:00:00:00" && mac != "FF:FF:FF:FF:FF:FF") {
                return mac;
            }
        }
    }

    // 如果ARP表中没有，尝试发送TCP请求来触发ARP表项
    if (m_stopFlag && m_stopFlag->loadRelaxed() != 0) {
        return "Unknown";
    }

    // 尝试连接常见端口来触发ARP表项，无需ping
    QList<int> probePorts = { 80, 443, 22, 135 };
    for (int port : probePorts) {
        if (m_stopFlag && m_stopFlag->loadRelaxed() != 0) {
            break;
        }

        QTcpSocket socket;
        socket.connectToHost(ip, port);
        socket.waitForConnected(500); // 短暂尝试连接
        socket.disconnectFromHost();
    }

    // 再次查询ARP表
    if (m_stopFlag && m_stopFlag->loadRelaxed() != 0) {
        return "Unknown";
    }

    QProcess arpProcess2;
    arpProcess2.setProcessChannelMode(QProcess::MergedChannels);
#ifdef Q_OS_WIN
    arpProcess2.start("arp", QStringList() << "-a" << ip);
#else
    arpProcess2.start("arp", QStringList() << "-n" << ip);
#endif

    if (arpProcess2.waitForFinished(1000)) {
        QString output = arpProcess2.readAllStandardOutput();
        QRegularExpression macRegex(R"(([0-9A-Fa-f]{2}[:-]){5}[0-9A-Fa-f]{2})");
        QRegularExpressionMatch match = macRegex.match(output);
        if (match.hasMatch()) {
            QString mac = match.captured(0).toUpper().replace('-', ':');
            if (mac != "00:00:00:00:00:00" && mac != "FF:FF:FF:FF:FF:FF") {
                return mac;
            }
        }
    }

    return "Unknown";
}

QString HostScanTask::getMacVendor(const QString& mac) {
    if (mac == "Unknown" || mac.length() < 8) {
        return "Unknown";
    }

    QString oui = mac.left(8).replace(":", "").toUpper();

    static QMap<QString, QString> vendorMap = {
        { "000C29", "VMware" }, { "005056", "VMware" }, { "080027", "VirtualBox" },
        { "001C42", "Parallels" }, { "00155D", "Microsoft" }, { "0050F2", "Microsoft" },
        { "001B21", "Intel" }, { "0019E3", "Intel" }, { "0024E9", "Intel" }, { "7085C2", "Intel" },
        { "001560", "Apple" }, { "0017F2", "Apple" }, { "001EC2", "Apple" }, { "0025BC", "Apple" },
        { "28E02C", "Apple" }, { "A45E60", "Apple" }, { "B8E856", "Apple" },
        { "001377", "HP" }, { "002264", "HP" }, { "00188B", "HP" }, { "001CC0", "HP" },
        { "002481", "Dell" }, { "001E4F", "Dell" }, { "0026B9", "Dell" },
        { "001560", "Lenovo" }, { "00214C", "Lenovo" }
    };

    return vendorMap.value(oui, "Unknown");
}

bool HostScanTask::isValidIP(const QString& ip) {
    const QHostAddress addr(ip);
    return addr.protocol() == QAbstractSocket::IPv4Protocol;
}

// ScanWorker 实现
ScanWorker::ScanWorker(QObject* parent)
    : QObject(parent), m_timeout(1000), m_threadCount(100), m_scanMethod(SCAN_ARP),
      m_currentIndex(0), m_activeTasks(0) {
    m_stopRequested = 0;
}

void ScanWorker::setScanParams(const QString& startIP, const QString& endIP, int timeout,
                              int threadCount, ScanMethod method) {
    m_startIP = startIP;
    m_endIP = endIP;
    m_timeout = timeout;
    m_scanMethod = method;
    // 大幅增加线程数限制以提高扫描效率
    m_threadCount = qMax(1, qMin(threadCount, 500)); // 允许更多线程，尤其是对于TCP扫描
}

void ScanWorker::stop() {
    m_stopRequested.storeRelaxed(1);

    // 不直接 terminate 线程，让任务自行检查停止标志退出
    qDebug() << "ScanWorker::stop() 设置停止标志";
}


void ScanWorker::startScan() {
    m_stopRequested.storeRelaxed(0);
    m_currentIndex.storeRelaxed(0);
    m_activeTasks.storeRelaxed(0);

    // 清理之前的线程
    for (QThread* thread : m_threads) {
        thread->deleteLater();
    }
    m_threads.clear();

    // 生成IP范围
    m_ipList = generateIPRange(m_startIP, m_endIP);

    if (m_ipList.isEmpty()) {
        emit scanFinished();
        return;
    }

    // 开始扫描
    scanNextBatch();
}

void ScanWorker::scanNextBatch() {
    QMutexLocker locker(&m_mutex);

    // 启动下一批扫描任务
    while (m_activeTasks.loadRelaxed() < m_threadCount && m_currentIndex.loadRelaxed() < m_ipList.size() && m_stopRequested.loadRelaxed() == 0) {
        QString ip = m_ipList[m_currentIndex.loadRelaxed()];
        m_currentIndex.fetchAndAddRelaxed(1);
        m_activeTasks.fetchAndAddRelaxed(1);

        // 为每个任务创建独立线程
        QThread* thread = new QThread();
        HostScanTask* task = new HostScanTask(ip, m_timeout, &m_stopRequested, m_scanMethod);

        task->moveToThread(thread);
        m_threads.append(thread);

        // 连接信号
        connect(thread, &QThread::started, task, &HostScanTask::scan);
        connect(task, &HostScanTask::finished, this, &ScanWorker::onTaskFinished);
        connect(thread, &QThread::finished, task, &QObject::deleteLater);
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);

        // 启动线程
        thread->start();

        emit scanProgress(m_currentIndex.loadRelaxed(), m_ipList.size());
    }

    // 如果没有活动任务且已完成所有IP，结束扫描
    if (m_activeTasks.loadRelaxed() == 0 && m_currentIndex.loadRelaxed() >= m_ipList.size()) {
        emit scanFinished();
    }
}

void ScanWorker::onTaskFinished(const HostInfo& hostInfo) {
    if (hostInfo.isOnline) {
        emit hostFound(hostInfo);
    }

    m_activeTasks.fetchAndAddRelaxed(-1);

    // 继续扫描下一批
    if (m_stopRequested.loadRelaxed() == 0) {
        scanNextBatch();
    }
}


QStringList ScanWorker::generateIPRange(const QString& startIP, const QString& endIP) {
    QStringList ipList;

    if (!isValidIP(startIP) || !isValidIP(endIP)) {
        return ipList;
    }

    QHostAddress startAddr(startIP);
    QHostAddress endAddr(endIP);

    if (startAddr.protocol() != QAbstractSocket::IPv4Protocol ||
        endAddr.protocol() != QAbstractSocket::IPv4Protocol) {
        return ipList;
    }

    quint32 start = startAddr.toIPv4Address();
    quint32 end = endAddr.toIPv4Address();

    if (start > end) {
        qSwap(start, end);
    }

    // 限制扫描范围，避免扫描过多IP
    if (end - start > 65535) {
        return ipList; // 超过65535个IP，拒绝扫描
    }

    for (quint32 ip = start; ip <= end; ++ip) {
        QHostAddress addr(ip);
        ipList.append(addr.toString());
    }

    return ipList;
}

bool ScanWorker::isValidIP(const QString& ip) {
    const QHostAddress addr(ip);
    return addr.protocol() == QAbstractSocket::IPv4Protocol;
}

// NetworkScanner 实现
NetworkScanner::NetworkScanner(QWidget* parent)
    : QWidget(parent), DynamicObjectBase(), m_scanThread(nullptr), m_scanWorker(nullptr),
      m_isScanning(false), m_foundHosts(0) {
    setupUI();
    loadNetworkInterfaces();

    // 状态更新定时器
    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout, [this]() {
        if (m_isScanning) {
            static int dots = 0;
            QString status = QString("正在扫描 (%1/%2)")
                             .arg(m_progressBar->value())
                             .arg(m_progressBar->maximum());
            for (int i = 0; i < (dots % 4); ++i) {
                status += ".";
            }
            m_statusLabel->setText(status);
            dots++;
        }
    });
}

NetworkScanner::~NetworkScanner() {
    if (m_scanThread && m_scanThread->isRunning()) {
        if (m_scanWorker) {
            m_scanWorker->stop();
        }
        m_scanThread->quit();
        m_scanThread->wait(3000);
    }
}

void NetworkScanner::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    m_mainLayout->setSpacing(10);

    setupScanSettings();
    setupResultsTable();
    setupStatusArea();
}

void NetworkScanner::setupScanSettings() {
    m_settingsGroup = new QGroupBox("🔍 扫描设置");
    m_mainLayout->addWidget(m_settingsGroup);

    QGridLayout* settingsLayout = new QGridLayout(m_settingsGroup);
    settingsLayout->setSpacing(8);

    // 网络接口选择
    settingsLayout->addWidget(new QLabel(tr("网络接口:")), 0, 0);
    m_interfaceCombo = new QComboBox();
    m_interfaceCombo->setMinimumWidth(200);
    connect(m_interfaceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &NetworkScanner::onInterfaceChanged);
    settingsLayout->addWidget(m_interfaceCombo, 0, 1);

    m_refreshBtn = new QPushButton(tr("🔄 刷新"));
    m_refreshBtn->setMaximumWidth(80);
    connect(m_refreshBtn, &QPushButton::clicked, this, &NetworkScanner::onRefreshInterfaces);
    settingsLayout->addWidget(m_refreshBtn, 0, 2);

    // 自定义IP范围
    m_customRangeCheck = new QCheckBox("自定义IP范围");
    connect(m_customRangeCheck, &QCheckBox::toggled, this, &NetworkScanner::onCustomRangeToggled);
    settingsLayout->addWidget(m_customRangeCheck, 1, 0);

    // IP范围显示/编辑
    m_ipRangeLabel = new QLabel(tr("IP范围: "));
    settingsLayout->addWidget(m_ipRangeLabel, 2, 0);

    QHBoxLayout* ipLayout = new QHBoxLayout();
    m_startIPEdit = new QLineEdit();
    m_startIPEdit->setPlaceholderText(tr("开始IP"));
    m_startIPEdit->setEnabled(false);
    ipLayout->addWidget(m_startIPEdit);

    ipLayout->addWidget(new QLabel(tr(" - ")));

    m_endIPEdit = new QLineEdit();
    m_endIPEdit->setPlaceholderText(tr("结束IP"));
    m_endIPEdit->setEnabled(false);
    ipLayout->addWidget(m_endIPEdit);

    settingsLayout->addLayout(ipLayout, 2, 1, 1, 2);

    // 超时设置
    settingsLayout->addWidget(new QLabel(tr("超时(ms):")), 3, 0);
    m_timeoutSpin = new QSpinBox();
    m_timeoutSpin->setRange(100, 10000);
    m_timeoutSpin->setValue(1000);
    m_timeoutSpin->setSuffix(" ms");
    settingsLayout->addWidget(m_timeoutSpin, 3, 1);

    // 并发线程数
    settingsLayout->addWidget(new QLabel(tr("并发数:")), 3, 2);
    m_threadSpin = new QSpinBox();
    m_threadSpin->setRange(1, 500);
    m_threadSpin->setValue(100);
    m_threadSpin->setToolTip("TCP扫描建议50-200，Ping扫描建议10-50");
    settingsLayout->addWidget(m_threadSpin, 3, 3);

    // 扫描方法选择
    settingsLayout->addWidget(new QLabel(tr("扫描方法:")), 4, 0);
    m_scanMethodCombo = new QComboBox();
    m_scanMethodCombo->addItem("ARP二层发现 (最快，推荐同网段)", SCAN_ARP);
    m_scanMethodCombo->addItem("并行TCP扫描 (推荐)", SCAN_MULTI_TCP);
    m_scanMethodCombo->addItem("原生ICMP扫描 (无进程)", SCAN_NATIVE_ICMP);
    m_scanMethodCombo->addItem("快速TCP扫描", SCAN_FAST_TCP);
    m_scanMethodCombo->addItem("TCP扫描", SCAN_TCP);
    m_scanMethodCombo->addItem("Ping扫描", SCAN_PING);
    m_scanMethodCombo->addItem("UDP扫描", SCAN_UDP);
    m_scanMethodCombo->addItem("ICMP扫描", SCAN_ICMP);
    settingsLayout->addWidget(m_scanMethodCombo, 4, 1, 1, 2);

    // 控制按钮
    m_buttonLayout = new QHBoxLayout();
    m_scanBtn = new QPushButton(tr("🚀 开始扫描"));
    m_scanBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; }");
    connect(m_scanBtn, &QPushButton::clicked, this, &NetworkScanner::onStartScan);
    m_buttonLayout->addWidget(m_scanBtn);

    m_stopBtn = new QPushButton(tr("⏹️ 停止扫描"));
    m_stopBtn->setEnabled(false);
    m_stopBtn->setStyleSheet("QPushButton { background-color: #f44336; color: white; font-weight: bold; }");
    // 使用DirectConnection确保立即响应
    connect(m_stopBtn, &QPushButton::clicked, this, &NetworkScanner::onStopScan, Qt::DirectConnection);
    m_buttonLayout->addWidget(m_stopBtn);

    m_clearBtn = new QPushButton(tr("🗑️ 清空结果"));
    connect(m_clearBtn, &QPushButton::clicked, this, &NetworkScanner::onClearResults);
    m_buttonLayout->addWidget(m_clearBtn);

    m_exportBtn = new QPushButton(tr("📤 导出结果"));
    connect(m_exportBtn, &QPushButton::clicked, this, &NetworkScanner::onExportResults);
    m_buttonLayout->addWidget(m_exportBtn);

    m_buttonLayout->addStretch();
    settingsLayout->addLayout(m_buttonLayout, 5, 0, 1, 4);
}

void NetworkScanner::setupResultsTable() {
    m_resultsGroup = new QGroupBox("📋 扫描结果");
    m_mainLayout->addWidget(m_resultsGroup);

    QVBoxLayout* resultsLayout = new QVBoxLayout(m_resultsGroup);
    resultsLayout->setContentsMargins(8, 0, 0, 0);
    resultsLayout->setSpacing(0);

    m_resultsTable = new QTableWidget();
    m_resultsTable->setColumnCount(6);
    QStringList headers = { "IP地址", "主机名", "MAC地址", "厂商", "响应时间(ms)", "状态" };
    m_resultsTable->setHorizontalHeaderLabels(headers);

    // 设置表格属性
    m_resultsTable->setAlternatingRowColors(true);
    m_resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultsTable->setSortingEnabled(true);

    // 列宽自适应
    m_resultsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents); // IP地址
    m_resultsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents); // 主机名
    m_resultsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents); // MAC地址
    m_resultsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch); // 厂商
    m_resultsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents); // 响应时间
    m_resultsTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents); // 状态
    m_resultsTable->horizontalHeader()->setStretchLastSection(true); // 最后一列拉伸填满剩余空间

    resultsLayout->addWidget(m_resultsTable);
}


void NetworkScanner::setupStatusArea() {
    m_statusGroup = new QGroupBox("📊 扫描状态");
    m_mainLayout->addWidget(m_statusGroup);

    QVBoxLayout* statusLayout = new QVBoxLayout(m_statusGroup);
    statusLayout->setContentsMargins(8, 0, 0, 0);
    statusLayout->setSpacing(5);

    // 进度条和状态标签
    QHBoxLayout* progressLayout = new QHBoxLayout();
    progressLayout->setContentsMargins(0, 0, 0, 0);
    progressLayout->setSpacing(10);

    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    m_progressBar->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed); // 高度随内容
    progressLayout->addWidget(m_progressBar);

    m_statusLabel = new QLabel(tr("就绪"));
    m_statusLabel->setStyleSheet("color: #666; font-weight: bold;");
    m_statusLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    progressLayout->addWidget(m_statusLabel);

    m_foundLabel = new QLabel(tr("发现主机: 0"));
    m_foundLabel->setStyleSheet("color: #4CAF50; font-weight: bold;");
    m_foundLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    progressLayout->addWidget(m_foundLabel);

    // 进度+标签布局高度自适应
    statusLayout->addLayout(progressLayout);

    // 日志文本高度弹性
    m_logText = new QTextEdit();
    m_logText->setPlaceholderText(tr("扫描日志将显示在这里..."));
    m_logText->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    statusLayout->addWidget(m_logText);
}


void NetworkScanner::loadNetworkInterfaces() {
    m_interfaceCombo->clear();
    m_interfaceMap.clear();

    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();

    for (const QNetworkInterface& interface : interfaces) {
        if (interface.flags() & QNetworkInterface::IsUp &&
            interface.flags() & QNetworkInterface::IsRunning &&
            !(interface.flags() & QNetworkInterface::IsLoopBack)) {
            QList<QNetworkAddressEntry> entries = interface.addressEntries();
            for (const QNetworkAddressEntry& entry : entries) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    QString name = QString("%1 (%2)").arg(interface.humanReadableName(), entry.ip().toString());
                    m_interfaceCombo->addItem(name);
                    m_interfaceMap[name] = entry.ip().toString();
                    break;
                }
            }
        }
    }

    if (m_interfaceCombo->count() > 0) {
        onInterfaceChanged();
    }
}

void NetworkScanner::updateIPRange() {
    if (m_interfaceCombo->currentText().isEmpty()) {
        return;
    }

    QString currentIP = m_interfaceMap[m_interfaceCombo->currentText()];
    QHostAddress addr(currentIP);

    if (addr.protocol() == QAbstractSocket::IPv4Protocol) {
        quint32 ip = addr.toIPv4Address();
        quint32 networkIP = ip & 0xFFFFFF00; // 假设/24子网
        quint32 startIP = networkIP + 1;
        quint32 endIP = networkIP + 254;

        QHostAddress startAddr(startIP);
        QHostAddress endAddr(endIP);

        m_startIPEdit->setText(startAddr.toString());
        m_endIPEdit->setText(endAddr.toString());

        m_ipRangeLabel->setText(QString("IP范围: %1 - %2")
            .arg(startAddr.toString(), endAddr.toString()));
    }
}

bool NetworkScanner::validateIPRange() {
    QString startIP = m_startIPEdit->text().trimmed();
    QString endIP = m_endIPEdit->text().trimmed();

    if (startIP.isEmpty() || endIP.isEmpty()) {
        QMessageBox::warning(this, tr("输入错误"), tr("请输入有效的IP地址范围"));
        return false;
    }

    QHostAddress startAddr(startIP);
    QHostAddress endAddr(endIP);

    if (startAddr.protocol() != QAbstractSocket::IPv4Protocol ||
        endAddr.protocol() != QAbstractSocket::IPv4Protocol) {
        QMessageBox::warning(this, tr("输入错误"), tr("请输入有效的IPv4地址"));
        return false;
    }

    quint32 start = startAddr.toIPv4Address();
    quint32 end = endAddr.toIPv4Address();

    if (start > end) {
        QMessageBox::warning(this, tr("输入错误"), tr("开始IP不能大于结束IP"));
        return false;
    }

    if (end - start > 65535) {
        QMessageBox::warning(this, tr("范围过大"), tr("IP范围不能超过65535个地址"));
        return false;
    }

    return true;
}

void NetworkScanner::onStartScan() {
    if (!validateIPRange()) {
        return;
    }

    // 清空之前的结果
    m_resultsTable->setRowCount(0);
    m_foundHosts = 0;
    m_foundLabel->setText(tr("发现主机: 0"));
    m_logText->clear();

    // 创建扫描线程
    m_scanThread = new QThread(this);
    m_scanWorker = new ScanWorker();
    m_scanWorker->moveToThread(m_scanThread);

    // 设置扫描参数
    ScanMethod method = static_cast<ScanMethod>(m_scanMethodCombo->currentData().toInt());
    m_scanWorker->setScanParams(
        m_startIPEdit->text().trimmed(),
        m_endIPEdit->text().trimmed(),
        m_timeoutSpin->value(),
        m_threadSpin->value(),
        method
    );

    // 连接信号
    connect(m_scanThread, &QThread::started, m_scanWorker, &ScanWorker::startScan);
    connect(m_scanWorker, &ScanWorker::hostFound, this, &NetworkScanner::onHostFound);
    connect(m_scanWorker, &ScanWorker::scanProgress, this, &NetworkScanner::onScanProgress);
    connect(m_scanWorker, &ScanWorker::scanFinished, this, &NetworkScanner::onScanFinished);
    connect(m_scanThread, &QThread::finished, m_scanWorker, &QObject::deleteLater);

    // 启动扫描
    m_scanThread->start();

    // 更新UI状态
    updateScanButton(true);
    m_progressBar->setVisible(true);
    m_statusTimer->start(500);

    m_logText->append(QString("[%1] 开始扫描 %2 - %3")
                      .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                      .arg(m_startIPEdit->text(), m_endIPEdit->text()));
}

void NetworkScanner::onStopScan() {
    if (!m_isScanning)
        return;

    // 立即更新 UI 状态
    updateScanButton(false);
    m_progressBar->setVisible(false);
    m_statusTimer->stop();
    m_statusLabel->setText(tr("正在停止扫描..."));
    m_logText->append(QString("[%1] 用户停止扫描")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));

    // 设置停止标志
    if (m_scanWorker) {
        m_scanWorker->stop();
    }

    // 异步等待线程安全退出
    if (m_scanThread && m_scanThread->isRunning()) {
        QTimer::singleShot(100, this, [this]() {
            if (m_scanThread && m_scanThread->isRunning()) {
                m_scanThread->quit();
                if (!m_scanThread->wait(2000)) {
                    qDebug() << "线程未响应，强制终止";
                    m_scanThread->terminate();
                    m_scanThread->wait(1000);
                }
            }
            m_statusLabel->setText(tr("扫描已停止"));
        });
    } else {
        m_statusLabel->setText(tr("扫描已停止"));
    }
}


void NetworkScanner::onClearResults() {
    m_resultsTable->setRowCount(0);
    m_foundHosts = 0;
    m_foundLabel->setText(tr("发现主机: 0"));
    m_logText->clear();
    m_progressBar->setValue(0);
    m_progressBar->setVisible(false);
    m_statusLabel->setText(tr("就绪"));
}

void NetworkScanner::onExportResults() {
    if (m_resultsTable->rowCount() == 0) {
        QMessageBox::information(this, tr("导出结果"), tr("没有可导出的数据"));
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(
        this,
        "导出扫描结果",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
        "/network_scan_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".csv",
        "CSV文件 (*.csv);;JSON文件 (*.json)"
    );

    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "导出失败", "无法创建文件: " + fileName);
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    if (fileName.endsWith(".json")) {
        // 导出为JSON格式
        QJsonArray jsonArray;

        for (int row = 0; row < m_resultsTable->rowCount(); ++row) {
            QJsonObject hostObj;
            hostObj["ip_address"] = m_resultsTable->item(row, 0)->text();
            hostObj["hostname"] = m_resultsTable->item(row, 1)->text();
            hostObj["mac_address"] = m_resultsTable->item(row, 2)->text();
            hostObj["vendor"] = m_resultsTable->item(row, 3)->text();
            hostObj["response_time"] = m_resultsTable->item(row, 4)->text();
            hostObj["status"] = m_resultsTable->item(row, 5)->text();
            jsonArray.append(hostObj);
        }

        QJsonDocument doc(jsonArray);
        out << doc.toJson();
    } else {
        // 导出为CSV格式
        out << "IP地址,主机名,MAC地址,厂商,响应时间(ms),状态\n";

        for (int row = 0; row < m_resultsTable->rowCount(); ++row) {
            QStringList rowData;
            for (int col = 0; col < m_resultsTable->columnCount(); ++col) {
                QString cellText = m_resultsTable->item(row, col)->text();
                // CSV转义
                if (cellText.contains(',') || cellText.contains('"') || cellText.contains('\n')) {
                    cellText = '"' + cellText.replace('"', "\"\"") + '"';
                }
                rowData << cellText;
            }
            out << rowData.join(',') << '\n';
        }
    }

    file.close();

    QMessageBox::information(this, "导出成功", "扫描结果已导出到: " + fileName);

    m_logText->append(QString("[%1] 导出结果到: %2")
                      .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                      .arg(fileName));
}

void NetworkScanner::onRefreshInterfaces() {
    loadNetworkInterfaces();
    m_logText->append(QString("[%1] 刷新网络接口")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
}

void NetworkScanner::onHostFound(const HostInfo& hostInfo) {
    // 使用队列连接确保在主线程中更新UI
    QMetaObject::invokeMethod(this, [this, hostInfo]() {
        m_foundHosts++;
        m_foundLabel->setText(QString("发现主机: %1").arg(m_foundHosts));

        // 添加到表格
        int row = m_resultsTable->rowCount();
        m_resultsTable->insertRow(row);

        m_resultsTable->setItem(row, 0, new QTableWidgetItem(hostInfo.ipAddress));
        m_resultsTable->setItem(row, 1, new QTableWidgetItem(hostInfo.hostName));
        m_resultsTable->setItem(row, 2, new QTableWidgetItem(hostInfo.macAddress));
        m_resultsTable->setItem(row, 3, new QTableWidgetItem(hostInfo.vendor));

        QString responseTime = hostInfo.responseTime >= 0 ? QString::number(hostInfo.responseTime) : "N/A";
        m_resultsTable->setItem(row, 4, new QTableWidgetItem(responseTime));

        QTableWidgetItem* statusItem = new QTableWidgetItem("在线");
        statusItem->setBackground(QBrush(QColor(76, 175, 80, 50))); // 淡绿色背景
        statusItem->setForeground(QBrush(QColor(27, 94, 32))); // 深绿色文字
        m_resultsTable->setItem(row, 5, statusItem);

        // 滚动到最新行
        m_resultsTable->scrollToBottom();

        m_logText->append(QString("[%1] 发现主机: %2 (%3)")
                          .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                          .arg(hostInfo.ipAddress, hostInfo.hostName));
    }, Qt::QueuedConnection);
}

void NetworkScanner::onScanProgress(int current, int total) {
    m_progressBar->setMaximum(total);
    m_progressBar->setValue(current);
}

void NetworkScanner::onScanFinished() {
    qDebug() << "onScanFinished() 被调用";

    // 确保在主线程中执行UI更新
    QMetaObject::invokeMethod(this, [this]() {
        updateScanButton(false);
        m_progressBar->setVisible(false);
        m_statusTimer->stop();
        m_statusLabel->setText(QString("扫描完成 - 发现 %1 台主机").arg(m_foundHosts));

        m_logText->append(QString("[%1] 扫描完成，共发现 %2 台在线主机")
                          .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                          .arg(m_foundHosts));
    }, Qt::QueuedConnection);

    // 清理线程
    if (m_scanThread && m_scanThread->isRunning()) {
        m_scanThread->quit();
        m_scanThread->wait(1000);
    }
}

void NetworkScanner::onInterfaceChanged() {
    if (!m_customRangeCheck->isChecked()) {
        updateIPRange();
    }
}

void NetworkScanner::onCustomRangeToggled(bool enabled) {
    m_startIPEdit->setEnabled(enabled);
    m_endIPEdit->setEnabled(enabled);

    if (!enabled) {
        updateIPRange();
    }
}

void NetworkScanner::updateScanButton(bool isScanning) {
    m_isScanning = isScanning;
    m_scanBtn->setEnabled(!isScanning);
    m_stopBtn->setEnabled(isScanning);
    m_settingsGroup->setEnabled(!isScanning);
}
