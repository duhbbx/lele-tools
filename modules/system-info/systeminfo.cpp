#include "systeminfo.h"
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QStorageInfo>
#include <QMessageBox>
#ifndef Q_OS_WIN
#include <netdb.h>
#endif
#include <QFileDialog>
#include <QTextStream>
#include <QDebug>
#include <QDesktopServices>
#include <QUrl>
#include <QRegularExpression>

REGISTER_DYNAMICOBJECT(SystemInfoTool);

// SystemInfoWorker 实现
SystemInfoWorker::SystemInfoWorker(QObject *parent)
    : QObject(parent)
{
}

void SystemInfoWorker::collectSystemInfo()
{
    ::SystemInfo info = getSystemInfo();
    emit systemInfoReady(info);
}

void SystemInfoWorker::collectDiskInfo()
{
    QList<DiskInfo> disks = getDiskInfo();
    emit diskInfoReady(disks);
}

::SystemInfo SystemInfoWorker::getSystemInfo()
{
#ifdef Q_OS_WIN
    return getWindowsSystemInfo();
#else
    return getLinuxSystemInfo();
#endif
}

QList<DiskInfo> SystemInfoWorker::getDiskInfo()
{
#ifdef Q_OS_WIN
    return getWindowsDiskInfo();
#else
    return getLinuxDiskInfo();
#endif
}

#ifdef Q_OS_WIN
::SystemInfo SystemInfoWorker::getWindowsSystemInfo()
{
    ::SystemInfo info;
    
    // 操作系统信息
    info.osName = QSysInfo::prettyProductName();
    info.osVersion = QSysInfo::productVersion();
    info.architecture = QSysInfo::currentCpuArchitecture();
    
    // 计算机名和用户名
    DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
    wchar_t computerName[MAX_COMPUTERNAME_LENGTH + 1];
    if (GetComputerNameW(computerName, &size)) {
        info.computerName = QString::fromWCharArray(computerName);
    }
    
    size = UNLEN + 1;
    wchar_t userName[UNLEN + 1];
    if (GetUserNameW(userName, &size)) {
        info.userName = QString::fromWCharArray(userName);
    }
    
    // CPU信息
    info.cpuName = getWindowsCPUName();
    
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    info.logicalCores = sysInfo.dwNumberOfProcessors;
    
    // 获取物理核心数
    DWORD length = 0;
    GetLogicalProcessorInformation(nullptr, &length);
    if (length > 0) {
        PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = 
            (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(length);
        if (GetLogicalProcessorInformation(buffer, &length)) {
            int physicalCores = 0;
            for (DWORD i = 0; i < length / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION); i++) {
                if (buffer[i].Relationship == RelationProcessorCore) {
                    physicalCores++;
                }
            }
            info.physicalCores = physicalCores;
        }
        free(buffer);
    }
    
    // CPU频率（从注册表获取）
    QProcess process;
    process.start("wmic", QStringList() << "cpu" << "get" << "MaxClockSpeed" << "/value");
    if (process.waitForFinished(3000)) {
        QString output = process.readAllStandardOutput();
        QRegularExpression rx("MaxClockSpeed=(\\d+)");
        QRegularExpressionMatch match = rx.match(output);
        if (match.hasMatch()) {
            info.cpuFrequency = match.captured(1).toDouble();
        }
    }
    
    // 内存信息
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        info.totalMemory = memStatus.ullTotalPhys;
        info.availableMemory = memStatus.ullAvailPhys;
        info.usedMemory = info.totalMemory - info.availableMemory;
        info.memoryUsagePercent = (double)info.usedMemory / info.totalMemory * 100.0;
        
        // 虚拟内存（页面文件）
        info.totalSwap = memStatus.ullTotalPageFile - memStatus.ullTotalPhys;
        info.availableSwap = memStatus.ullAvailPageFile - memStatus.ullAvailPhys;
        info.usedSwap = info.totalSwap - info.availableSwap;
    }
    
    // 网络信息
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &interface : interfaces) {
        if (interface.flags() & QNetworkInterface::IsUp &&
            interface.flags() & QNetworkInterface::IsRunning &&
            !(interface.flags() & QNetworkInterface::IsLoopBack)) {
            
            for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    if (info.primaryIP.isEmpty()) {
                        info.primaryIP = entry.ip().toString();
                    }
                    info.networkInterfaces.append(
                        QString("%1: %2").arg(interface.humanReadableName(), entry.ip().toString())
                    );
                }
            }
        }
    }
    
    info.defaultGateway = getDefaultGateway();
    info.dnsServers = getDNSServers();
    
    return info;
}

QString SystemInfoWorker::getWindowsCPUName()
{
    QString cpuName;
    QProcess process;
    process.start("wmic", QStringList() << "cpu" << "get" << "Name" << "/value");
    if (process.waitForFinished(3000)) {
        QString output = process.readAllStandardOutput();
        QRegularExpression rx("Name=(.+)");
        QRegularExpressionMatch match = rx.match(output);
        if (match.hasMatch()) {
            cpuName = match.captured(1).trimmed();
        }
    }
    
    if (cpuName.isEmpty()) {
        cpuName = "Unknown CPU";
    }
    
    return cpuName;
}

QString SystemInfoWorker::getDefaultGateway()
{
    QString gateway;
    
    ULONG bufferSize = 0;
    GetAdaptersInfo(nullptr, &bufferSize);
    
    if (bufferSize > 0) {
        PIP_ADAPTER_INFO adapterInfo = (PIP_ADAPTER_INFO)malloc(bufferSize);
        if (GetAdaptersInfo(adapterInfo, &bufferSize) == NO_ERROR) {
            PIP_ADAPTER_INFO adapter = adapterInfo;
            while (adapter) {
                if (strlen(adapter->GatewayList.IpAddress.String) > 0 &&
                    strcmp(adapter->GatewayList.IpAddress.String, "0.0.0.0") != 0) {
                    gateway = QString::fromLocal8Bit(adapter->GatewayList.IpAddress.String);
                    break;
                }
                adapter = adapter->Next;
            }
        }
        free(adapterInfo);
    }
    
    return gateway;
}

QStringList SystemInfoWorker::getDNSServers()
{
    QStringList dnsServers;
    
    QProcess process;
    process.start("nslookup", QStringList() << "localhost");
    if (process.waitForFinished(3000)) {
        QString output = process.readAllStandardOutput();
        QStringList lines = output.split('\n');
        for (const QString &line : lines) {
            if (line.contains("Server:")) {
                QString dns = line.split(':').value(1).trimmed();
                if (!dns.isEmpty() && dns != "localhost") {
                    dnsServers.append(dns);
                }
            }
        }
    }
    
    return dnsServers;
}

QList<DiskInfo> SystemInfoWorker::getWindowsDiskInfo()
{
    QList<DiskInfo> disks;
    
    QList<QStorageInfo> storageInfos = QStorageInfo::mountedVolumes();
    for (const QStorageInfo &storage : storageInfos) {
        if (storage.isValid() && storage.isReady()) {
            DiskInfo disk;
            disk.device = storage.device();
            disk.mountPoint = storage.rootPath();
            disk.fileSystem = storage.fileSystemType();
            disk.totalSpace = storage.bytesTotal();
            disk.availableSpace = storage.bytesAvailable();
            disk.usedSpace = disk.totalSpace - disk.availableSpace;
            disk.usagePercent = disk.totalSpace > 0 ? 
                               (double)disk.usedSpace / disk.totalSpace * 100.0 : 0;
            disk.isReady = storage.isReady();
            
            disks.append(disk);
        }
    }
    
    return disks;
}
#endif

#if !defined(Q_OS_WIN)

// macOS / Linux 系统信息实现
::SystemInfo SystemInfoWorker::getLinuxSystemInfo()
{
    ::SystemInfo info;

    // 操作系统
    info.osName = QSysInfo::prettyProductName();
    info.osVersion = QSysInfo::productVersion();
    info.cpuArchitecture = QSysInfo::currentCpuArchitecture();
    info.architecture = QSysInfo::currentCpuArchitecture();
    info.computerName = QSysInfo::machineHostName();
    info.userName = qEnvironmentVariable("USER");

#ifdef Q_OS_MAC
    // CPU 名称
    char cpuBrand[256] = {0};
    size_t cpuBrandLen = sizeof(cpuBrand);
    if (sysctlbyname("machdep.cpu.brand_string", cpuBrand, &cpuBrandLen, nullptr, 0) == 0)
        info.cpuName = QString::fromUtf8(cpuBrand).trimmed();

    // CPU 核心数
    int physCores = 0, logCores = 0;
    size_t intSize = sizeof(int);
    sysctlbyname("hw.physicalcpu", &physCores, &intSize, nullptr, 0);
    sysctlbyname("hw.logicalcpu", &logCores, &intSize, nullptr, 0);
    info.physicalCores = physCores;
    info.logicalCores = logCores;

    // CPU 频率（Apple Silicon 可能没有，fallback 0）
    uint64_t cpuFreq = 0;
    size_t freqSize = sizeof(cpuFreq);
    if (sysctlbyname("hw.cpufrequency", &cpuFreq, &freqSize, nullptr, 0) == 0)
        info.cpuFrequency = cpuFreq / 1000000.0; // Hz → MHz
    else
        info.cpuFrequency = 0;

    // 内存
    uint64_t totalMem = 0;
    size_t memSize = sizeof(totalMem);
    sysctlbyname("hw.memsize", &totalMem, &memSize, nullptr, 0);
    info.totalMemory = totalMem;

    // 可用内存（通过 mach API）
    vm_size_t pageSize;
    mach_port_t machPort = mach_host_self();
    host_page_size(machPort, &pageSize);

    vm_statistics64_data_t vmStats;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    if (host_statistics64(machPort, HOST_VM_INFO64,
                          reinterpret_cast<host_info64_t>(&vmStats), &count) == KERN_SUCCESS) {
        quint64 freeMem = (quint64)(vmStats.free_count + vmStats.inactive_count) * pageSize;
        info.availableMemory = freeMem;
        info.usedMemory = info.totalMemory - freeMem;
        info.memoryUsagePercent = info.totalMemory > 0
            ? (double)info.usedMemory / info.totalMemory * 100.0 : 0;
    }

    // 交换内存
    struct xsw_usage swapUsage;
    size_t swapSize = sizeof(swapUsage);
    if (sysctlbyname("vm.swapusage", &swapUsage, &swapSize, nullptr, 0) == 0) {
        info.totalSwap = swapUsage.xsu_total;
        info.usedSwap = swapUsage.xsu_used;
        info.availableSwap = swapUsage.xsu_avail;
    }
#else
    // Linux 实现
    info.userName = qEnvironmentVariable("USER");

    // CPU 信息从 /proc/cpuinfo 读取
    QFile cpuFile("/proc/cpuinfo");
    if (cpuFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&cpuFile);
        int coreCount = 0;
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (line.startsWith("model name") && info.cpuName.isEmpty())
                info.cpuName = line.section(':', 1).trimmed();
            if (line.startsWith("processor"))
                coreCount++;
            if (line.startsWith("cpu MHz") && info.cpuFrequency == 0)
                info.cpuFrequency = line.section(':', 1).trimmed().toDouble();
        }
        info.logicalCores = coreCount;
        cpuFile.close();
    }

    // 内存从 /proc/meminfo
    QFile memFile("/proc/meminfo");
    if (memFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&memFile);
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (line.startsWith("MemTotal:"))
                info.totalMemory = line.section(':', 1).trimmed().split(' ').first().toULongLong() * 1024;
            else if (line.startsWith("MemAvailable:"))
                info.availableMemory = line.section(':', 1).trimmed().split(' ').first().toULongLong() * 1024;
            else if (line.startsWith("SwapTotal:"))
                info.totalSwap = line.section(':', 1).trimmed().split(' ').first().toULongLong() * 1024;
            else if (line.startsWith("SwapFree:"))
                info.availableSwap = line.section(':', 1).trimmed().split(' ').first().toULongLong() * 1024;
        }
        info.usedMemory = info.totalMemory - info.availableMemory;
        info.usedSwap = info.totalSwap - info.availableSwap;
        info.memoryUsagePercent = info.totalMemory > 0
            ? (double)info.usedMemory / info.totalMemory * 100.0 : 0;
        memFile.close();
    }
#endif

    // 网络信息（macOS/Linux 通用，使用 ifaddrs）
    struct ifaddrs *ifaddr;
    if (getifaddrs(&ifaddr) == 0) {
        for (struct ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == nullptr) continue;
            if (ifa->ifa_addr->sa_family == AF_INET) {
                char host[NI_MAXHOST];
                getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                            host, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST);
                QString ifName = QString::fromUtf8(ifa->ifa_name);
                QString ip = QString::fromUtf8(host);
                if (ifName != "lo" && ifName != "lo0" && !ip.startsWith("127.")) {
                    info.networkInterfaces.append(QString("%1: %2").arg(ifName, ip));
                    if (info.primaryIP.isEmpty())
                        info.primaryIP = ip;
                }
            }
        }
        freeifaddrs(ifaddr);
    }

    // 屏幕信息
    QList<QScreen*> screens = QGuiApplication::screens();
    for (QScreen* screen : screens) {
        QSize size = screen->size();
        qreal dpr = screen->devicePixelRatio();
        info.monitors.append(QString("%1 (%2x%3 @%4x)")
            .arg(screen->name())
            .arg(size.width()).arg(size.height())
            .arg(dpr, 0, 'f', 1));
    }
    if (!screens.isEmpty()) {
        QSize primary = screens.first()->size();
        info.resolution = QString("%1x%2").arg(primary.width()).arg(primary.height());
    }

    return info;
}

QList<DiskInfo> SystemInfoWorker::getLinuxDiskInfo()
{
    QList<DiskInfo> disks;
    for (const QStorageInfo &storage : QStorageInfo::mountedVolumes()) {
        if (!storage.isValid() || !storage.isReady()) continue;
        // 跳过小分区和系统虚拟分区
        if (storage.bytesTotal() < 100 * 1024 * 1024) continue; // < 100MB
        QString rootPath = storage.rootPath();
        // 跳过 macOS 系统快照和 Linux 虚拟文件系统
        if (rootPath.startsWith("/System") || rootPath.startsWith("/proc")
            || rootPath.startsWith("/sys") || rootPath.startsWith("/dev")
            || rootPath.startsWith("/run")) continue;

        DiskInfo disk;
        disk.device = QString::fromUtf8(storage.device());
        disk.mountPoint = rootPath;
        disk.fileSystem = QString::fromUtf8(storage.fileSystemType());
        disk.totalSpace = storage.bytesTotal();
        disk.availableSpace = storage.bytesAvailable();
        disk.usedSpace = disk.totalSpace - disk.availableSpace;
        disk.usagePercent = disk.totalSpace > 0
            ? (double)disk.usedSpace / disk.totalSpace * 100.0 : 0;
        disk.isReady = true;
        disks.append(disk);
    }
    return disks;
}
#endif

// SystemInfoTool 主类实现
SystemInfoTool::SystemInfoTool(QWidget *parent)
    : QWidget(parent), DynamicObjectBase()
    , m_workerThread(nullptr)
    , m_worker(nullptr)
{
    setupUI();
    loadSettings();
    
    // 创建工作线程
    m_workerThread = new QThread(this);
    m_worker = new SystemInfoWorker();
    m_worker->moveToThread(m_workerThread);
    
    // 连接信号
    connect(m_worker, &SystemInfoWorker::systemInfoReady,
            this, &SystemInfoTool::onSystemInfoReady);
    connect(m_worker, &SystemInfoWorker::diskInfoReady,
            this, &SystemInfoTool::onDiskInfoReady);
    connect(m_worker, &SystemInfoWorker::errorOccurred,
            this, &SystemInfoTool::onErrorOccurred);
    
    // 启动工作线程
    m_workerThread->start();
    
    // 初始加载
    refreshSystemInfo();
}

SystemInfoTool::~SystemInfoTool()
{
    if (m_workerThread) {
        m_workerThread->quit();
        if (!m_workerThread->wait(3000)) {
            m_workerThread->terminate();
            m_workerThread->wait(1000);
        }
    }
    
    saveSettings();
}

void SystemInfoTool::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    m_mainLayout->setSpacing(10);
    
    // 创建标签页
    m_tabWidget = new QTabWidget();
    
    setupSystemInfoArea();
    setupDiskInfoArea();
    setupControlArea();
    
    m_mainLayout->addWidget(m_tabWidget);
    m_mainLayout->addWidget(m_controlGroup);
}

void SystemInfoTool::setupSystemInfoArea()
{
    m_systemTab = new QWidget();
    m_systemScrollArea = new QScrollArea();
    m_systemContent = new QWidget();
    m_systemLayout = new QVBoxLayout(m_systemContent);
    
    setupNetworkInfoArea();
    setupHardwareInfoArea();
    
    m_systemScrollArea->setWidget(m_systemContent);
    m_systemScrollArea->setWidgetResizable(true);
    
    QVBoxLayout *systemTabLayout = new QVBoxLayout(m_systemTab);
    systemTabLayout->addWidget(m_systemScrollArea);
    
    m_tabWidget->addTab(m_systemTab, tr("🖥️ 系统信息"));
}

void SystemInfoTool::setupNetworkInfoArea()
{
    m_networkGroup = new QGroupBox("🌐 网络信息", m_systemContent);
    m_networkLayout = new QGridLayout(m_networkGroup);
    m_networkLayout->setSpacing(8);
    
    // 添加网络信息输入框（只读，可选中复制）
    int row = 0;
    
    m_networkLayout->addWidget(new QLabel(tr("主机名:")), row, 0);
    m_hostnameEdit = new QLineEdit("获取中...");
    m_hostnameEdit->setReadOnly(true);
    m_hostnameEdit->setContextMenuPolicy(Qt::DefaultContextMenu);
    m_hostnameEdit->setStyleSheet("QLineEdit[readOnly=\"true\"] { background-color: #f8f9fa; color: #007bff; font-weight: bold; }");
    m_networkLayout->addWidget(m_hostnameEdit, row++, 1);
    
    m_networkLayout->addWidget(new QLabel(tr("主IP地址:")), row, 0);
    m_primaryIPEdit = new QLineEdit("获取中...");
    m_primaryIPEdit->setReadOnly(true);
    m_primaryIPEdit->setContextMenuPolicy(Qt::DefaultContextMenu);
    m_primaryIPEdit->setStyleSheet("QLineEdit[readOnly=\"true\"] { background-color: #f8f9fa; color: #007bff; font-weight: bold; }");
    m_networkLayout->addWidget(m_primaryIPEdit, row++, 1);
    
    m_networkLayout->addWidget(new QLabel(tr("默认网关:")), row, 0);
    m_gatewayEdit = new QLineEdit("获取中...");
    m_gatewayEdit->setReadOnly(true);
    m_gatewayEdit->setContextMenuPolicy(Qt::DefaultContextMenu);
    m_gatewayEdit->setStyleSheet("QLineEdit[readOnly=\"true\"] { background-color: #f8f9fa; color: #007bff; font-weight: bold; }");
    m_networkLayout->addWidget(m_gatewayEdit, row++, 1);
    
    m_networkLayout->addWidget(new QLabel(tr("DNS服务器:")), row, 0);
    m_dnsEdit = new QTextEdit();
    m_dnsEdit->setReadOnly(true);
    m_dnsEdit->setMaximumHeight(60);
    m_dnsEdit->setContextMenuPolicy(Qt::DefaultContextMenu);
    m_dnsEdit->setStyleSheet("QTextEdit[readOnly=\"true\"] { background-color: #f8f9fa; color: #007bff; font-weight: bold; }");
    m_networkLayout->addWidget(m_dnsEdit, row++, 1);
    
    m_systemLayout->addWidget(m_networkGroup);
}

void SystemInfoTool::setupHardwareInfoArea()
{
    m_hardwareGroup = new QGroupBox("💻 硬件信息", m_systemContent);
    m_hardwareLayout = new QGridLayout(m_hardwareGroup);
    m_hardwareLayout->setSpacing(8);
    
    int row = 0;
    
    // 操作系统
    m_hardwareLayout->addWidget(new QLabel(tr("操作系统:")), row, 0);
    m_osEdit = new QLineEdit("获取中...");
    m_osEdit->setReadOnly(true);
    m_osEdit->setContextMenuPolicy(Qt::DefaultContextMenu);
    m_osEdit->setStyleSheet("QLineEdit[readOnly=\"true\"] { background-color: #f8f9fa; color: #007bff; font-weight: bold; }");
    m_hardwareLayout->addWidget(m_osEdit, row++, 1, 1, 2);
    
    // CPU信息
    m_hardwareLayout->addWidget(new QLabel(tr("处理器:")), row, 0);
    m_cpuEdit = new QTextEdit();
    m_cpuEdit->setReadOnly(true);
    m_cpuEdit->setMaximumHeight(60);
    m_cpuEdit->setContextMenuPolicy(Qt::DefaultContextMenu);
    m_cpuEdit->setStyleSheet("QTextEdit[readOnly=\"true\"] { background-color: #f8f9fa; color: #007bff; font-weight: bold; }");
    m_hardwareLayout->addWidget(m_cpuEdit, row++, 1, 1, 2);
    
    m_hardwareLayout->addWidget(new QLabel(tr("CPU核心:")), row, 0);
    m_coresEdit = new QLineEdit("获取中...");
    m_coresEdit->setReadOnly(true);
    m_coresEdit->setContextMenuPolicy(Qt::DefaultContextMenu);
    m_coresEdit->setStyleSheet("QLineEdit[readOnly=\"true\"] { background-color: #f8f9fa; color: #007bff; font-weight: bold; }");
    m_hardwareLayout->addWidget(m_coresEdit, row++, 1);
    
    m_hardwareLayout->addWidget(new QLabel(tr("CPU频率:")), row, 0);
    m_frequencyEdit = new QLineEdit("获取中...");
    m_frequencyEdit->setReadOnly(true);
    m_frequencyEdit->setContextMenuPolicy(Qt::DefaultContextMenu);
    m_frequencyEdit->setStyleSheet("QLineEdit[readOnly=\"true\"] { background-color: #f8f9fa; color: #007bff; font-weight: bold; }");
    m_hardwareLayout->addWidget(m_frequencyEdit, row++, 1);
    
    // 内存信息
    m_hardwareLayout->addWidget(new QLabel(tr("系统内存:")), row, 0);
    m_memoryEdit = new QLineEdit("获取中...");
    m_memoryEdit->setReadOnly(true);
    m_memoryEdit->setContextMenuPolicy(Qt::DefaultContextMenu);
    m_memoryEdit->setStyleSheet("QLineEdit[readOnly=\"true\"] { background-color: #f8f9fa; color: #007bff; font-weight: bold; }");
    m_hardwareLayout->addWidget(m_memoryEdit, row, 1);
    
    m_memoryProgress = new QProgressBar();
    m_memoryProgress->setRange(0, 100);
    m_hardwareLayout->addWidget(m_memoryProgress, row++, 2);
    
    // 交换内存
    m_hardwareLayout->addWidget(new QLabel(tr("虚拟内存:")), row, 0);
    m_swapEdit = new QLineEdit("获取中...");
    m_swapEdit->setReadOnly(true);
    m_swapEdit->setContextMenuPolicy(Qt::DefaultContextMenu);
    m_swapEdit->setStyleSheet("QLineEdit[readOnly=\"true\"] { background-color: #f8f9fa; color: #007bff; font-weight: bold; }");
    m_hardwareLayout->addWidget(m_swapEdit, row, 1);
    
    m_swapProgress = new QProgressBar();
    m_swapProgress->setRange(0, 100);
    m_hardwareLayout->addWidget(m_swapProgress, row++, 2);
    
    m_systemLayout->addWidget(m_hardwareGroup);
}

void SystemInfoTool::setupDiskInfoArea()
{
    m_diskTab = new QWidget();
    m_diskLayout = new QVBoxLayout(m_diskTab);
    
    // 磁盘信息表格
    m_diskTable = new QTableWidget();
    m_diskTable->setColumnCount(7);
    QStringList headers;
    headers << "设备" << "挂载点" << "文件系统" << "总容量" << "已用" << "可用" << "使用率";
    m_diskTable->setHorizontalHeaderLabels(headers);
    
    m_diskTable->setAlternatingRowColors(true);
    m_diskTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_diskTable->horizontalHeader()->setStretchLastSection(false);
    m_diskTable->verticalHeader()->setVisible(false);
    
    // 设置列宽
    m_diskTable->setColumnWidth(0, 80);   // 设备
    m_diskTable->setColumnWidth(1, 100);  // 挂载点
    m_diskTable->setColumnWidth(2, 80);   // 文件系统
    m_diskTable->setColumnWidth(3, 100);  // 总容量
    m_diskTable->setColumnWidth(4, 100);  // 已用
    m_diskTable->setColumnWidth(5, 100);  // 可用
    m_diskTable->setColumnWidth(6, 120);  // 使用率
    
    m_diskLayout->addWidget(m_diskTable);
    
    m_tabWidget->addTab(m_diskTab, tr("💾 磁盘信息"));
}

void SystemInfoTool::setupControlArea()
{
    m_controlGroup = new QGroupBox("🔧 控制操作");
    QHBoxLayout *layout = new QHBoxLayout(m_controlGroup);
    
    m_refreshBtn = new QPushButton(tr("刷新信息"));
    m_refreshBtn->setObjectName("refreshBtn");
    m_exportBtn = new QPushButton(tr("导出报告"));
    
    m_lastUpdateLabel = new QLabel(tr("最后更新: 从未"));
    m_lastUpdateLabel->setStyleSheet("color: #6c757d; font-style: italic;");
    
    layout->addWidget(m_refreshBtn);
    layout->addWidget(m_exportBtn);
    layout->addStretch();
    layout->addWidget(m_lastUpdateLabel);
    
    connect(m_refreshBtn, &QPushButton::clicked, this, &SystemInfoTool::onRefreshClicked);
    connect(m_exportBtn, &QPushButton::clicked, this, &SystemInfoTool::onExportClicked);
}

void SystemInfoTool::updateSystemInfoDisplay(const ::SystemInfo &info)
{
    m_currentSystemInfo = info;
    
    // 更新网络信息
    m_hostnameEdit->setText(info.computerName);
    m_primaryIPEdit->setText(info.primaryIP);
    m_gatewayEdit->setText(info.defaultGateway);
    m_dnsEdit->setPlainText(info.dnsServers.join("\n"));
    
    // 更新硬件信息
    m_osEdit->setText(QString("%1 %2 (%3)")
                      .arg(info.osName).arg(info.osVersion).arg(info.architecture));
    m_cpuEdit->setPlainText(info.cpuName);
    m_coresEdit->setText(QString("%1 物理核心, %2 逻辑核心")
                         .arg(info.physicalCores).arg(info.logicalCores));
    m_frequencyEdit->setText(QString("%1 MHz").arg(info.cpuFrequency, 0, 'f', 0));
    
    // 更新内存信息
    m_memoryEdit->setText(QString("%1 / %2")
                          .arg(formatBytes(info.usedMemory)).arg(formatBytes(info.totalMemory)));
    m_memoryProgress->setValue((int)info.memoryUsagePercent);
    m_memoryProgress->setFormat(QString("%1%").arg(info.memoryUsagePercent, 0, 'f', 1));
    
    // 更新交换内存
    if (info.totalSwap > 0) {
        double swapPercent = (double)info.usedSwap / info.totalSwap * 100.0;
        m_swapEdit->setText(QString("%1 / %2")
                            .arg(formatBytes(info.usedSwap)).arg(formatBytes(info.totalSwap)));
        m_swapProgress->setValue((int)swapPercent);
        m_swapProgress->setFormat(QString("%1%").arg(swapPercent, 0, 'f', 1));
    } else {
        m_swapEdit->setText(tr("未配置"));
        m_swapProgress->setValue(0);
        m_swapProgress->setFormat("N/A");
    }
    
    // 更新最后更新时间
    m_lastUpdateLabel->setText("最后更新: " + QDateTime::currentDateTime().toString("hh:mm:ss"));
}

void SystemInfoTool::updateDiskInfoDisplay(const QList<DiskInfo> &disks)
{
    m_currentDiskInfo = disks;
    
    m_diskTable->setRowCount(0);
    
    for (const DiskInfo &disk : disks) {
        int row = m_diskTable->rowCount();
        m_diskTable->insertRow(row);
        
        m_diskTable->setItem(row, 0, new QTableWidgetItem(disk.device));
        m_diskTable->setItem(row, 1, new QTableWidgetItem(disk.mountPoint));
        m_diskTable->setItem(row, 2, new QTableWidgetItem(disk.fileSystem));
        m_diskTable->setItem(row, 3, new QTableWidgetItem(formatBytes(disk.totalSpace)));
        m_diskTable->setItem(row, 4, new QTableWidgetItem(formatBytes(disk.usedSpace)));
        m_diskTable->setItem(row, 5, new QTableWidgetItem(formatBytes(disk.availableSpace)));
        
        // 使用率单元格带进度条效果
        QTableWidgetItem *usageItem = new QTableWidgetItem(formatPercentage(disk.usagePercent));
        
        // 根据使用率设置颜色
        if (disk.usagePercent > 90) {
            usageItem->setBackground(QColor("#f8d7da"));
            usageItem->setForeground(QColor("#721c24"));
        } else if (disk.usagePercent > 75) {
            usageItem->setBackground(QColor("#fff3cd"));
            usageItem->setForeground(QColor("#856404"));
        } else {
            usageItem->setBackground(QColor("#d4edda"));
            usageItem->setForeground(QColor("#155724"));
        }
        
        m_diskTable->setItem(row, 6, usageItem);
    }
}

QString SystemInfoTool::formatBytes(quint64 bytes) const
{
    const quint64 KB = 1024;
    const quint64 MB = KB * 1024;
    const quint64 GB = MB * 1024;
    const quint64 TB = GB * 1024;
    
    if (bytes >= TB) {
        return QString::number(bytes / (double)TB, 'f', 2) + " TB";
    } else if (bytes >= GB) {
        return QString::number(bytes / (double)GB, 'f', 2) + " GB";
    } else if (bytes >= MB) {
        return QString::number(bytes / (double)MB, 'f', 1) + " MB";
    } else if (bytes >= KB) {
        return QString::number(bytes / (double)KB, 'f', 0) + " KB";
    } else {
        return QString::number(bytes) + " B";
    }
}

QString SystemInfoTool::formatPercentage(double percent) const
{
    return QString::number(percent, 'f', 1) + "%";
}

void SystemInfoTool::refreshSystemInfo()
{
    QMetaObject::invokeMethod(m_worker, "collectSystemInfo", Qt::QueuedConnection);
    QMetaObject::invokeMethod(m_worker, "collectDiskInfo", Qt::QueuedConnection);
}

void SystemInfoTool::saveSettings()
{
    // 暂无需要保存的设置
}

void SystemInfoTool::loadSettings()
{
    // 暂无需要加载的设置
}

// 槽函数实现
void SystemInfoTool::onRefreshClicked()
{
    refreshSystemInfo();
}

void SystemInfoTool::onExportClicked()
{
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "导出系统信息报告",
        QString("system_info_%1.txt")
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")),
        "文本文件 (*.txt)"
    );
    
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            
            out << "系统基本信息报告\n";
            out << "生成时间: " << QDateTime::currentDateTime().toString() << "\n";
            out << QString("=").repeated(50) << "\n\n";
            
            // 系统信息
            out << "操作系统信息:\n";
            out << "  名称: " << m_currentSystemInfo.osName << "\n";
            out << "  版本: " << m_currentSystemInfo.osVersion << "\n";
            out << "  架构: " << m_currentSystemInfo.architecture << "\n";
            out << "  计算机名: " << m_currentSystemInfo.computerName << "\n";
            out << "  用户名: " << m_currentSystemInfo.userName << "\n\n";
            
            // CPU信息
            out << "处理器信息:\n";
            out << "  名称: " << m_currentSystemInfo.cpuName << "\n";
            out << "  物理核心: " << m_currentSystemInfo.physicalCores << "\n";
            out << "  逻辑核心: " << m_currentSystemInfo.logicalCores << "\n";
            out << "  频率: " << m_currentSystemInfo.cpuFrequency << " MHz\n\n";
            
            // 内存信息
            out << "内存信息:\n";
            out << "  总内存: " << formatBytes(m_currentSystemInfo.totalMemory) << "\n";
            out << "  已用内存: " << formatBytes(m_currentSystemInfo.usedMemory) << "\n";
            out << "  可用内存: " << formatBytes(m_currentSystemInfo.availableMemory) << "\n";
            out << "  使用率: " << formatPercentage(m_currentSystemInfo.memoryUsagePercent) << "\n\n";
            
            // 网络信息
            out << "网络信息:\n";
            out << "  主IP地址: " << m_currentSystemInfo.primaryIP << "\n";
            out << "  默认网关: " << m_currentSystemInfo.defaultGateway << "\n";
            out << "  DNS服务器: " << m_currentSystemInfo.dnsServers.join(", ") << "\n\n";
            
            // 磁盘信息
            out << "磁盘信息:\n";
            for (const DiskInfo &disk : m_currentDiskInfo) {
                out << QString("  %1 (%2):\n").arg(disk.device, disk.mountPoint);
                out << QString("    文件系统: %1\n").arg(disk.fileSystem);
                out << QString("    总容量: %1\n").arg(formatBytes(disk.totalSpace));
                out << QString("    已用: %1\n").arg(formatBytes(disk.usedSpace));
                out << QString("    可用: %1\n").arg(formatBytes(disk.availableSpace));
                out << QString("    使用率: %1\n").arg(formatPercentage(disk.usagePercent));
                out << "\n";
            }
            
            QMessageBox::information(this, tr("成功"), tr("系统信息报告已导出"));
        } else {
            QMessageBox::warning(this, tr("错误"), tr("无法写入文件"));
        }
    }
}

void SystemInfoTool::onSystemInfoReady(const ::SystemInfo &info)
{
    updateSystemInfoDisplay(info);
}

void SystemInfoTool::onDiskInfoReady(const QList<DiskInfo> &disks)
{
    updateDiskInfoDisplay(disks);
}

void SystemInfoTool::onErrorOccurred(const QString &error)
{
    QMessageBox::warning(this, "错误", error);
}
