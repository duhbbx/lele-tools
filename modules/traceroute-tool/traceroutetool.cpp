#include "traceroutetool.h"
#include <QApplication>
#include <QMessageBox>
#include <QHeaderView>
#include <QFileDialog>
#include <QDateTime>
#include <QDebug>
#include <QSplitter>
#include <QTextStream>
#include <QHostInfo>
#include <QNetworkInterface>
#include <QStandardPaths>
#include <QDir>

#ifdef Q_OS_WIN
#include <windows.h>
#include <icmpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#endif

REGISTER_DYNAMICOBJECT(TracerouteTool);

// TracerouteWorker 实现
TracerouteWorker::TracerouteWorker(QObject *parent)
    : QObject(parent)
    , m_running(false)
#ifdef Q_OS_WIN
    , m_socket(INVALID_SOCKET)
#ifdef WITH_PCAP
    , m_pcapHandle(nullptr)
#endif
#ifdef WITH_WINDOWS_ICMP
    , m_icmpHandle(INVALID_HANDLE_VALUE)
#endif
#else
    , m_socket(-1)
    , m_pcapHandle(nullptr)
#endif
{
    initializeNetwork();
}

TracerouteWorker::~TracerouteWorker()
{
    cleanup();
}

bool TracerouteWorker::initializeNetwork()
{
#ifdef Q_OS_WIN
    bool winsockOk = initializeWinsock();
    if (!winsockOk) return false;
    
#ifdef WITH_PCAP
    // 尝试使用Npcap
    if (initializeNpcap()) {
        qDebug() << "Using Npcap for traceroute";
        return true;
    }
#endif

#ifdef WITH_WINDOWS_ICMP
    // 备用方案：使用Windows ICMP API
    if (initializeIcmp()) {
        qDebug() << "Using Windows ICMP API for traceroute";
        return true;
    }
#endif

    qDebug() << "No suitable network API available";
    return false;
#else
    // Linux/macOS实现将在后续添加
    return false;
#endif
}

#ifdef Q_OS_WIN
bool TracerouteWorker::initializeWinsock()
{
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        qDebug() << "WSAStartup failed:" << result;
        return false;
    }
    
    // 创建原始套接字用于发送ICMP包
    m_socket = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (m_socket == INVALID_SOCKET) {
        qDebug() << "Create socket failed:" << WSAGetLastError();
        qDebug() << "注意：需要管理员权限才能创建原始套接字";
        WSACleanup();
        return false;
    }
    
    return true;
}

bool TracerouteWorker::initializeNpcap()
{
    pcap_if_t *alldevs;
    char errbuf[PCAP_ERRBUF_SIZE];
    
    // 查找网络设备
    if (pcap_findalldevs(&alldevs, errbuf) == -1) {
        qDebug() << "Error in pcap_findalldevs:" << errbuf;
        return false;
    }
    
    if (alldevs == nullptr) {
        qDebug() << "No network devices found";
        return false;
    }
    
    // 寻找活动的网络接口（跳过回环接口）
    pcap_if_t *device = nullptr;
    for (pcap_if_t *d = alldevs; d != nullptr; d = d->next) {
        qDebug() << "Found device:" << d->name << d->description;
        
        // 跳过回环接口和无地址的接口
        if (d->addresses != nullptr && 
            !(d->flags & PCAP_IF_LOOPBACK) &&
            (d->flags & PCAP_IF_UP)) {
            device = d;
            qDebug() << "Selected device:" << d->name;
            break;
        }
    }
    
    if (device == nullptr) {
        qDebug() << "No suitable network device found";
        pcap_freealldevs(alldevs);
        return false;
    }
    
    // 打开网络设备
    m_pcapHandle = pcap_open_live(device->name, 65536, 1, 100, errbuf);
    if (m_pcapHandle == nullptr) {
        qDebug() << "Unable to open device:" << errbuf;
        pcap_freealldevs(alldevs);
        return false;
    }
    
    // 设置ICMP过滤器
    struct bpf_program filter;
    if (pcap_compile(m_pcapHandle, &filter, "icmp", 1, PCAP_NETMASK_UNKNOWN) == -1) {
        qDebug() << "Error compiling filter:" << pcap_geterr(m_pcapHandle);
        pcap_close(m_pcapHandle);
        pcap_freealldevs(alldevs);
        return false;
    }
    
    if (pcap_setfilter(m_pcapHandle, &filter) == -1) {
        qDebug() << "Error setting filter:" << pcap_geterr(m_pcapHandle);
        pcap_freecode(&filter);
        pcap_close(m_pcapHandle);
        pcap_freealldevs(alldevs);
        return false;
    }
    
    pcap_freecode(&filter);
    pcap_freealldevs(alldevs);
    qDebug() << "Npcap initialized successfully with ICMP filter";
    return true;
}
#endif

#ifdef WITH_WINDOWS_ICMP
bool TracerouteWorker::initializeIcmp()
{
    m_icmpHandle = IcmpCreateFile();
    if (m_icmpHandle == INVALID_HANDLE_VALUE) {
        qDebug() << "IcmpCreateFile failed:" << GetLastError();
        return false;
    }
    
    qDebug() << "Windows ICMP API initialized successfully";
    return true;
}
#endif

void TracerouteWorker::cleanupWindows()
{
    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
    
#ifdef WITH_PCAP
    if (m_pcapHandle) {
        pcap_close(m_pcapHandle);
        m_pcapHandle = nullptr;
    }
#endif

#ifdef WITH_WINDOWS_ICMP
    if (m_icmpHandle != INVALID_HANDLE_VALUE) {
        IcmpCloseHandle(m_icmpHandle);
        m_icmpHandle = INVALID_HANDLE_VALUE;
    }
#endif
    
    WSACleanup();
}

void TracerouteWorker::cleanup()
{
#ifdef Q_OS_WIN
    cleanupWindows();
#else
    // Linux/macOS cleanup将在后续添加
#endif
}

void TracerouteWorker::startTraceroute(const QString &target, int maxHops, int timeout)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_running) {
        emit errorOccurred("Traceroute already running");
        return;
    }
    
    m_running = true;
    
    // 在工作线程中执行traceroute
    QTimer::singleShot(0, this, [this, target, maxHops, timeout]() {
        performTraceroute(target, maxHops, timeout);
    });
}

void TracerouteWorker::stopTraceroute()
{
    QMutexLocker locker(&m_mutex);
    m_running = false;
}

void TracerouteWorker::performTraceroute(const QString &target, int maxHops, int timeoutMs)
{
    qDebug() << "开始traceroute到" << target << "最大跳数:" << maxHops;
    
    for (int ttl = 1; ttl <= maxHops && m_running; ++ttl) {
        emit progressUpdated(ttl, maxHops);
        
        RouteHop hop;
        hop.hopNumber = ttl;
        
        // 发送3个ICMP包测试RTT
        for (int i = 0; i < 3 && m_running; ++i) {
            if (sendICMPPacket(target, ttl, timeoutMs, hop)) {
                switch (i) {
                case 0: hop.rtt1 = hop.rtt1; break;
                case 1: hop.rtt2 = hop.rtt1; hop.rtt1 = -1; break;
                case 2: hop.rtt3 = hop.rtt1; hop.rtt1 = -1; break;
                }
            } else {
                hop.timeout = true;
            }
        }
        
        emit hopDiscovered(hop);
        
        // 如果到达目标，结束traceroute
        if (hop.ipAddress == target || 
            (!hop.hostname.isEmpty() && hop.hostname.contains(target))) {
            break;
        }
        
        // 短暂延迟
        QThread::msleep(100);
    }
    
    {
        QMutexLocker locker(&m_mutex);
        m_running = false;
    }
    
    emit tracerouteCompleted();
}

bool TracerouteWorker::sendICMPPacket(const QString &target, int ttl, int timeoutMs, RouteHop &hop)
{
#ifdef Q_OS_WIN
    // 检查Npcap是否初始化
    if (m_socket == INVALID_SOCKET || m_pcapHandle == nullptr) {
        qDebug() << "Network not initialized";
        return false;
    }
    
    // 目标地址解析
    struct sockaddr_in targetAddr;
    memset(&targetAddr, 0, sizeof(targetAddr));
    targetAddr.sin_family = AF_INET;
    
    // 尝试直接解析IP
    targetAddr.sin_addr.s_addr = inet_addr(target.toLocal8Bit().data());
    if (targetAddr.sin_addr.s_addr == INADDR_NONE) {
        // 尝试解析主机名
        struct hostent *host = gethostbyname(target.toLocal8Bit().data());
        if (host == nullptr) {
            hop.status = "无法解析主机名";
            return false;
        }
        targetAddr.sin_addr.s_addr = *((unsigned long*)host->h_addr_list[0]);
    }
    
    // 设置套接字TTL
    if (setsockopt(m_socket, IPPROTO_IP, IP_TTL, (char*)&ttl, sizeof(ttl)) == SOCKET_ERROR) {
        qDebug() << "Set TTL failed:" << WSAGetLastError();
    }
    
    // 构造ICMP Echo请求包
    struct icmp_header {
        unsigned char type;      // ICMP类型
        unsigned char code;      // ICMP代码  
        unsigned short checksum; // 校验和
        unsigned short id;       // 标识符
        unsigned short seq;      // 序列号
        char data[32];          // 数据部分
    };
    
    icmp_header icmpPacket;
    memset(&icmpPacket, 0, sizeof(icmpPacket));
    icmpPacket.type = 8;  // ICMP Echo Request
    icmpPacket.code = 0;
    icmpPacket.id = GetCurrentProcessId() & 0xFFFF;
    icmpPacket.seq = ttl;
    strcpy_s(icmpPacket.data, sizeof(icmpPacket.data), "TracerouteTest");
    
    // 计算校验和
    icmpPacket.checksum = 0;
    unsigned short *p = (unsigned short*)&icmpPacket;
    unsigned long sum = 0;
    int len = sizeof(icmpPacket);
    
    while (len > 1) {
        sum += *p++;
        len -= 2;
    }
    if (len == 1) {
        sum += *(unsigned char*)p;
    }
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    icmpPacket.checksum = (unsigned short)(~sum);
    
    QElapsedTimer timer;
    timer.start();
    
    // 发送ICMP包
    qDebug() << "Sending ICMP packet to" << target << "with TTL=" << ttl;
    int bytesSent = sendto(m_socket, (char*)&icmpPacket, sizeof(icmpPacket), 0,
                          (struct sockaddr*)&targetAddr, sizeof(targetAddr));
    
    if (bytesSent == SOCKET_ERROR) {
        DWORD error = WSAGetLastError();
        qDebug() << "Send failed:" << error;
        if (error == WSAEACCES) {
            hop.status = "权限不足";
        } else {
            hop.status = QString("发送失败(%1)").arg(error);
        }
        hop.timeout = true;
        return false;
    }
    
    qDebug() << "ICMP packet sent successfully, waiting for reply...";
    
    // 使用pcap捕获ICMP回复包
    struct pcap_pkthdr *header;
    const u_char *packet;
    bool replyReceived = false;
    
    // 设置超时
    QElapsedTimer timeoutTimer;
    timeoutTimer.start();
    
    while (timeoutTimer.elapsed() < timeoutMs && !replyReceived) {
        int result = pcap_next_ex(m_pcapHandle, &header, &packet);
        
        if (result == 1) {
            // 解析以太网帧
            if (header->caplen >= 14) {  // 以太网头部最小长度
                const u_char *ipHeader = packet + 14;  // 跳过以太网头部
                
                // 检查IP版本
                if ((ipHeader[0] & 0xF0) == 0x40 && header->caplen >= 34) {  // IPv4且长度足够
                    // 检查是否是ICMP包
                    if (ipHeader[9] == 1) {  // ICMP协议号
                        // 计算IP头部长度
                        int ipHeaderLen = (ipHeader[0] & 0x0F) * 4;
                        if (header->caplen >= 14 + ipHeaderLen + 8) {  // 确保有足够的ICMP头部
                            const u_char *icmpHeader = ipHeader + ipHeaderLen;
                            
                            // 提取源IP地址（发送ICMP消息的路由器/主机）
                            struct in_addr sourceAddr;
                            memcpy(&sourceAddr, &ipHeader[12], 4);
                            QString sourceIP = QString::fromLocal8Bit(inet_ntoa(sourceAddr));
                            
                            qDebug() << "Received ICMP packet from" << sourceIP 
                                     << "Type:" << (int)icmpHeader[0] 
                                     << "Code:" << (int)icmpHeader[1];
                            
                            // 检查ICMP类型
                            if (icmpHeader[0] == 11) {  // Time Exceeded
                                hop.ipAddress = sourceIP;
                                hop.rtt1 = timer.nsecsElapsed() / 1000000.0;
                                hop.timeout = false;
                                hop.status = QString("TTL=%1").arg(ttl);
                                replyReceived = true;
                                qDebug() << "Time Exceeded from router:" << sourceIP;
                            }
                            else if (icmpHeader[0] == 0) {  // Echo Reply (到达目标)
                                hop.ipAddress = sourceIP;
                                hop.rtt1 = timer.nsecsElapsed() / 1000000.0;
                                hop.timeout = false;
                                hop.status = "目标到达";
                                replyReceived = true;
                                qDebug() << "Echo Reply from target:" << sourceIP;
                            }
                        }
                    }
                }
            }
        }
        else if (result == 0) {
            // 超时，继续等待
            QThread::msleep(10);
        }
        else {
            // 错误
            break;
        }
    }
    
    if (!replyReceived) {
        hop.timeout = true;
        hop.status = "超时";
        hop.ipAddress = "*";
        qDebug() << "No ICMP reply received within" << timeoutMs << "ms for TTL" << ttl;
    }
    
    // 尝试反向DNS解析
    QHostInfo::lookupHost(hop.ipAddress, [&hop](const QHostInfo &info) {
        if (info.error() == QHostInfo::NoError && !info.hostName().isEmpty()) {
            hop.hostname = info.hostName();
        }
    });
    
    return true;
#else
    // Linux/macOS实现将在后续添加
    Q_UNUSED(target)
    Q_UNUSED(ttl)
    Q_UNUSED(timeoutMs)
    Q_UNUSED(hop)
    return false;
#endif
}

// TracerouteTool 实现
TracerouteTool::TracerouteTool(QWidget *parent)
    : QWidget(parent), DynamicObjectBase()
    , m_workerThread(nullptr)
    , m_worker(nullptr)
    , m_running(false)
    , m_currentHop(0)
    , m_maxHops(30)
{
    setupUI();
    loadSettings();
    
    // 创建工作线程
    m_workerThread = new QThread(this);
    m_worker = new TracerouteWorker();
    m_worker->moveToThread(m_workerThread);
    
    // 连接信号
    connect(m_worker, &TracerouteWorker::hopDiscovered,
            this, &TracerouteTool::onHopDiscovered);
    connect(m_worker, &TracerouteWorker::tracerouteCompleted,
            this, &TracerouteTool::onTracerouteCompleted);
    connect(m_worker, &TracerouteWorker::errorOccurred,
            this, &TracerouteTool::onErrorOccurred);
    connect(m_worker, &TracerouteWorker::progressUpdated,
            this, &TracerouteTool::onProgressUpdated);
    
    // 启动工作线程
    m_workerThread->start();
    
    logMessage("Traceroute工具初始化完成");
#ifdef Q_OS_WIN
    logMessage("Windows平台 - 使用Npcap库");
    logMessage("注意：需要管理员权限才能使用原始套接字");
    logMessage("确保已安装Npcap SDK");
#else
    logMessage("Linux/macOS平台 - 使用libpcap库");
#endif
}

TracerouteTool::~TracerouteTool()
{
    if (m_running) {
        m_worker->stopTraceroute();
    }
    
    if (m_workerThread) {
        m_workerThread->quit();
        if (!m_workerThread->wait(3000)) {
            m_workerThread->terminate();
            m_workerThread->wait(1000);
        }
    }
    
    saveSettings();
}

void TracerouteTool::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    m_mainLayout->setSpacing(10);
    
    setupControlArea();
    setupResultsArea();
    setupStatusArea();
}

void TracerouteTool::setupControlArea()
{
    m_controlGroup = new QGroupBox("Traceroute配置", this);
    QGridLayout *layout = new QGridLayout(m_controlGroup);
    layout->setSpacing(8);
    
    // 目标地址输入
    layout->addWidget(new QLabel(tr("目标地址:")), 0, 0);
    m_targetEdit = new QLineEdit();
    m_targetEdit->setPlaceholderText(tr("输入IP地址或域名 (例: google.com, 8.8.8.8)"));
    layout->addWidget(m_targetEdit, 0, 1, 1, 2);
    
    // 最大跳数
    layout->addWidget(new QLabel(tr("最大跳数:")), 1, 0);
    m_maxHopsSpin = new QSpinBox();
    m_maxHopsSpin->setRange(1, 64);
    m_maxHopsSpin->setValue(30);
    layout->addWidget(m_maxHopsSpin, 1, 1);
    
    // 超时时间
    layout->addWidget(new QLabel(tr("超时(ms):")), 1, 2);
    m_timeoutSpin = new QSpinBox();
    m_timeoutSpin->setRange(1000, 10000);
    m_timeoutSpin->setValue(3000);
    m_timeoutSpin->setSingleStep(500);
    layout->addWidget(m_timeoutSpin, 1, 3);
    
    // 解析主机名选项
    m_resolveNamesCheck = new QCheckBox("解析主机名");
    m_resolveNamesCheck->setChecked(true);
    layout->addWidget(m_resolveNamesCheck, 2, 0, 1, 2);
    
    // 控制按钮
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_startBtn = new QPushButton(tr("开始追踪"));
    m_startBtn->setObjectName("startBtn");
    m_stopBtn = new QPushButton(tr("停止"));
    m_stopBtn->setObjectName("stopBtn");
    m_stopBtn->setEnabled(false);
    m_clearBtn = new QPushButton(tr("清空结果"));
    m_exportBtn = new QPushButton(tr("导出结果"));
    
    btnLayout->addWidget(m_startBtn);
    btnLayout->addWidget(m_stopBtn);
    btnLayout->addWidget(m_clearBtn);
    btnLayout->addWidget(m_exportBtn);
    btnLayout->addStretch();
    
    layout->addLayout(btnLayout, 3, 0, 1, 4);
    
    m_mainLayout->addWidget(m_controlGroup);
    
    // 连接信号
    connect(m_targetEdit, &QLineEdit::textChanged, this, &TracerouteTool::onTargetChanged);
    connect(m_startBtn, &QPushButton::clicked, this, &TracerouteTool::onStartTraceroute);
    connect(m_stopBtn, &QPushButton::clicked, this, &TracerouteTool::onStopTraceroute);
    connect(m_clearBtn, &QPushButton::clicked, this, &TracerouteTool::onClearResults);
    connect(m_exportBtn, &QPushButton::clicked, this, &TracerouteTool::onExportResults);
}

void TracerouteTool::setupResultsArea()
{
    m_resultsGroup = new QGroupBox("路由追踪结果", this);
    QVBoxLayout *layout = new QVBoxLayout(m_resultsGroup);
    
    // 进度条
    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    layout->addWidget(m_progressBar);
    
    // 结果表格
    m_resultsTable = new QTableWidget();
    m_resultsTable->setColumnCount(7);
    QStringList headers;
    headers << "跳数" << "IP地址" << "主机名" << "RTT1(ms)" << "RTT2(ms)" << "RTT3(ms)" << "状态";
    m_resultsTable->setHorizontalHeaderLabels(headers);
    
    // 设置表格属性
    m_resultsTable->setAlternatingRowColors(true);
    m_resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultsTable->horizontalHeader()->setStretchLastSection(true);
    m_resultsTable->verticalHeader()->setVisible(false);
    
    // 设置列宽
    m_resultsTable->setColumnWidth(0, 50);   // 跳数
    m_resultsTable->setColumnWidth(1, 120);  // IP地址
    m_resultsTable->setColumnWidth(2, 200);  // 主机名
    m_resultsTable->setColumnWidth(3, 80);   // RTT1
    m_resultsTable->setColumnWidth(4, 80);   // RTT2
    m_resultsTable->setColumnWidth(5, 80);   // RTT3
    
    layout->addWidget(m_resultsTable);
    
    m_mainLayout->addWidget(m_resultsGroup);
}

void TracerouteTool::setupStatusArea()
{
    m_logGroup = new QGroupBox("状态日志", this);
    QVBoxLayout *layout = new QVBoxLayout(m_logGroup);
    
    // 状态标签
    m_statusLabel = new QLabel(tr("就绪"));
    m_statusLabel->setStyleSheet("font-weight: bold; color: #007bff;");
    layout->addWidget(m_statusLabel);
    
    // 日志文本
    m_logText = new QTextEdit();
    m_logText->setMaximumHeight(120);
    m_logText->setReadOnly(true);
    layout->addWidget(m_logText);
    
    m_mainLayout->addWidget(m_logGroup);
}

void TracerouteTool::updateUI()
{
    m_startBtn->setEnabled(!m_running && !m_targetEdit->text().trimmed().isEmpty());
    m_stopBtn->setEnabled(m_running);
    m_clearBtn->setEnabled(!m_running && m_resultsTable->rowCount() > 0);
    m_exportBtn->setEnabled(!m_running && m_resultsTable->rowCount() > 0);
    m_progressBar->setVisible(m_running);
    
    if (m_running) {
        m_statusLabel->setText(QString("正在追踪到 %1...").arg(m_currentTarget));
        m_statusLabel->setStyleSheet("font-weight: bold; color: #28a745;");
    } else {
        m_statusLabel->setText(tr("就绪"));
        m_statusLabel->setStyleSheet("font-weight: bold; color: #007bff;");
    }
}

void TracerouteTool::addHopToTable(const RouteHop &hop)
{
    int row = m_resultsTable->rowCount();
    m_resultsTable->insertRow(row);
    
    m_resultsTable->setItem(row, 0, new QTableWidgetItem(QString::number(hop.hopNumber)));
    m_resultsTable->setItem(row, 1, new QTableWidgetItem(hop.ipAddress));
    m_resultsTable->setItem(row, 2, new QTableWidgetItem(hop.hostname));
    m_resultsTable->setItem(row, 3, new QTableWidgetItem(formatRTT(hop.rtt1)));
    m_resultsTable->setItem(row, 4, new QTableWidgetItem(formatRTT(hop.rtt2)));
    m_resultsTable->setItem(row, 5, new QTableWidgetItem(formatRTT(hop.rtt3)));
    m_resultsTable->setItem(row, 6, new QTableWidgetItem(hop.status));
    
    // 设置超时行的样式
    if (hop.timeout) {
        for (int col = 0; col < 7; ++col) {
            QTableWidgetItem *item = m_resultsTable->item(row, col);
            if (item) {
                item->setBackground(QColor("#fff3cd"));
                item->setForeground(QColor("#856404"));
            }
        }
    }
    
    // 滚动到最新行
    m_resultsTable->scrollToBottom();
}

void TracerouteTool::logMessage(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString logEntry = QString("[%1] %2").arg(timestamp, message);
    m_logText->append(logEntry);
    
    // 限制日志行数
    if (m_logText->document()->lineCount() > 100) {
        QTextCursor cursor = m_logText->textCursor();
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, 10);
        cursor.removeSelectedText();
    }
}

QString TracerouteTool::formatRTT(double rtt) const
{
    if (rtt < 0) {
        return "*";
    }
    return QString::number(rtt, 'f', 1);
}

void TracerouteTool::saveSettings()
{
    QSettings settings;
    settings.setValue("TracerouteTool/target", m_targetEdit->text());
    settings.setValue("TracerouteTool/maxHops", m_maxHopsSpin->value());
    settings.setValue("TracerouteTool/timeout", m_timeoutSpin->value());
    settings.setValue("TracerouteTool/resolveNames", m_resolveNamesCheck->isChecked());
}

void TracerouteTool::loadSettings()
{
    QSettings settings;
    if (m_targetEdit) {
        m_targetEdit->setText(settings.value("TracerouteTool/target", "google.com").toString());
    }
    if (m_maxHopsSpin) {
        m_maxHopsSpin->setValue(settings.value("TracerouteTool/maxHops", 30).toInt());
    }
    if (m_timeoutSpin) {
        m_timeoutSpin->setValue(settings.value("TracerouteTool/timeout", 3000).toInt());
    }
    if (m_resolveNamesCheck) {
        m_resolveNamesCheck->setChecked(settings.value("TracerouteTool/resolveNames", true).toBool());
    }
}

// 槽函数实现
void TracerouteTool::onStartTraceroute()
{
    QString target = m_targetEdit->text().trimmed();
    if (target.isEmpty()) {
        QMessageBox::warning(this, tr("错误"), tr("请输入目标地址"));
        return;
    }
    
    m_running = true;
    m_currentTarget = target;
    m_maxHops = m_maxHopsSpin->value();
    m_currentHop = 0;
    
    // 清空结果
    m_resultsTable->setRowCount(0);
    
    // 设置进度条
    m_progressBar->setRange(1, m_maxHops);
    m_progressBar->setValue(0);
    
    updateUI();
    logMessage(QString("开始追踪路由到 %1，最大跳数: %2").arg(target).arg(m_maxHops));
    
    // 启动traceroute
    QMetaObject::invokeMethod(m_worker, "startTraceroute", Qt::QueuedConnection,
                              Q_ARG(QString, target),
                              Q_ARG(int, m_maxHops),
                              Q_ARG(int, m_timeoutSpin->value()));
}

void TracerouteTool::onStopTraceroute()
{
    m_running = false;
    QMetaObject::invokeMethod(m_worker, "stopTraceroute", Qt::QueuedConnection);
    logMessage("用户停止了路由追踪");
    updateUI();
}

void TracerouteTool::onClearResults()
{
    m_resultsTable->setRowCount(0);
    m_logText->clear();
    logMessage("结果已清空");
}

void TracerouteTool::onExportResults()
{
    if (m_resultsTable->rowCount() == 0) {
        QMessageBox::information(this, tr("提示"), tr("没有结果可导出"));
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "导出Traceroute结果",
        QString("traceroute_%1_%2.txt")
            .arg(m_currentTarget)
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")),
        "文本文件 (*.txt);;CSV文件 (*.csv)"
    );
    
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            
            // 写入标题
            out << "Traceroute to " << m_currentTarget << "\n";
            out << "Generated at: " << QDateTime::currentDateTime().toString() << "\n";
            out << "Max hops: " << m_maxHops << "\n\n";
            
            // 写入表头
            out << "Hop\tIP Address\tHostname\tRTT1(ms)\tRTT2(ms)\tRTT3(ms)\tStatus\n";
            
            // 写入数据
            for (int row = 0; row < m_resultsTable->rowCount(); ++row) {
                for (int col = 0; col < m_resultsTable->columnCount(); ++col) {
                    QTableWidgetItem *item = m_resultsTable->item(row, col);
                    if (item) {
                        out << item->text();
                    }
                    if (col < m_resultsTable->columnCount() - 1) {
                        out << "\t";
                    }
                }
                out << "\n";
            }
            
            logMessage("结果已导出到: " + fileName);
            QMessageBox::information(this, "成功", "结果已导出到:\n" + fileName);
        } else {
            QMessageBox::warning(this, tr("错误"), tr("无法写入文件"));
        }
    }
}

void TracerouteTool::onTargetChanged()
{
    updateUI();
}

void TracerouteTool::onHopDiscovered(const RouteHop &hop)
{
    addHopToTable(hop);
    logMessage(QString("跳 %1: %2 (%3ms)")
               .arg(hop.hopNumber)
               .arg(hop.ipAddress.isEmpty() ? "*" : hop.ipAddress)
               .arg(formatRTT(hop.rtt1)));
}

void TracerouteTool::onTracerouteCompleted()
{
    m_running = false;
    updateUI();
    logMessage("路由追踪完成");
    
    QMessageBox::information(this, "完成", 
                           QString("路由追踪到 %1 已完成\n共发现 %2 个跳点")
                           .arg(m_currentTarget)
                           .arg(m_resultsTable->rowCount()));
}

void TracerouteTool::onErrorOccurred(const QString &error)
{
    m_running = false;
    updateUI();
    logMessage("错误: " + error);
    QMessageBox::critical(this, "错误", error);
}

void TracerouteTool::onProgressUpdated(int currentHop, int maxHops)
{
    m_currentHop = currentHop;
    m_progressBar->setValue(currentHop);
    m_statusLabel->setText(QString("正在追踪跳点 %1/%2...").arg(currentHop).arg(maxHops));
}
