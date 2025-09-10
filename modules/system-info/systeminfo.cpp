#include "systeminfo.h"
#include <QApplication>
#include <QMessageBox>
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
    
    // 应用样式
    setStyleSheet(R"(
        QWidget {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            font-size: 10px;
        }
        QGroupBox {
            font-weight: bold;
            border: 2px solid #dee2e6;
            border-radius: 8px;
            margin-top: 1ex;
            padding-top: 8px;
            font-size: 11px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 8px;
            padding: 0 4px 0 4px;
            color: #495057;
        }
        QLabel {
            font-size: 10px;
            color: #495057;
        }
        QLabel[class="value"] {
            font-weight: bold;
            color: #007bff;
        }
        QLabel[class="header"] {
            font-weight: bold;
            font-size: 11px;
            color: #343a40;
        }
        QPushButton {
            background-color: #f8f9fa;
            border: 1px solid #dee2e6;
            border-radius: 6px;
            padding: 8px 16px;
            font-size: 10px;
            min-height: 20px;
        }
        QPushButton:hover {
            background-color: #e9ecef;
            border-color: #adb5bd;
        }
        QPushButton#refreshBtn {
            background-color: #007bff;
            color: white;
            font-weight: bold;
        }
        QPushButton#refreshBtn:hover {
            background-color: #0056b3;
        }
        QTableWidget {
            border: 1px solid #dee2e6;
            gridline-color: #dee2e6;
            background-color: white;
            alternate-background-color: #f8f9fa;
        }
        QTableWidget::item {
            padding: 4px;
        }
        QTableWidget::item:selected {
            background-color: #007bff;
            color: white;
        }
        QLineEdit[readOnly="true"] {
            background-color: #f8f9fa;
            color: #007bff;
            font-weight: bold;
            border: 1px solid #e1e5e9;
            border-radius: 4px;
            padding: 4px 8px;
        }
        QLineEdit[readOnly="true"]:focus {
            border-color: #007bff;
            background-color: #ffffff;
        }
        QTextEdit[readOnly="true"] {
            background-color: #f8f9fa;
            color: #007bff;
            font-weight: bold;
            border: 1px solid #e1e5e9;
            border-radius: 4px;
        }
        QTextEdit[readOnly="true"]:focus {
            border-color: #007bff;
            background-color: #ffffff;
        }
        QProgressBar {
            border: 1px solid #dee2e6;
            border-radius: 4px;
            text-align: center;
            font-weight: bold;
        }
        QProgressBar::chunk {
            background-color: #007bff;
            border-radius: 3px;
        }
        QTabWidget::pane {
            border: 1px solid #dee2e6;
            border-radius: 4px;
        }
        QTabBar::tab {
            background-color: #f8f9fa;
            border: 1px solid #dee2e6;
            border-bottom: none;
            border-radius: 4px 4px 0 0;
            padding: 8px 16px;
            margin-right: 2px;
        }
        QTabBar::tab:selected {
            background-color: white;
            border-bottom: 1px solid white;
        }
    )");
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
    
    m_tabWidget->addTab(m_systemTab, "🖥️ 系统信息");
}

void SystemInfoTool::setupNetworkInfoArea()
{
    m_networkGroup = new QGroupBox("🌐 网络信息", m_systemContent);
    m_networkLayout = new QGridLayout(m_networkGroup);
    m_networkLayout->setSpacing(8);
    
    // 添加网络信息输入框（只读，可选中复制）
    int row = 0;
    
    m_networkLayout->addWidget(new QLabel("主机名:"), row, 0);
    m_hostnameEdit = new QLineEdit("获取中...");
    m_hostnameEdit->setReadOnly(true);
    m_hostnameEdit->setContextMenuPolicy(Qt::DefaultContextMenu);
    m_hostnameEdit->setStyleSheet("QLineEdit[readOnly=\"true\"] { background-color: #f8f9fa; color: #007bff; font-weight: bold; }");
    m_networkLayout->addWidget(m_hostnameEdit, row++, 1);
    
    m_networkLayout->addWidget(new QLabel("主IP地址:"), row, 0);
    m_primaryIPEdit = new QLineEdit("获取中...");
    m_primaryIPEdit->setReadOnly(true);
    m_primaryIPEdit->setContextMenuPolicy(Qt::DefaultContextMenu);
    m_primaryIPEdit->setStyleSheet("QLineEdit[readOnly=\"true\"] { background-color: #f8f9fa; color: #007bff; font-weight: bold; }");
    m_networkLayout->addWidget(m_primaryIPEdit, row++, 1);
    
    m_networkLayout->addWidget(new QLabel("默认网关:"), row, 0);
    m_gatewayEdit = new QLineEdit("获取中...");
    m_gatewayEdit->setReadOnly(true);
    m_gatewayEdit->setContextMenuPolicy(Qt::DefaultContextMenu);
    m_gatewayEdit->setStyleSheet("QLineEdit[readOnly=\"true\"] { background-color: #f8f9fa; color: #007bff; font-weight: bold; }");
    m_networkLayout->addWidget(m_gatewayEdit, row++, 1);
    
    m_networkLayout->addWidget(new QLabel("DNS服务器:"), row, 0);
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
    m_hardwareLayout->addWidget(new QLabel("操作系统:"), row, 0);
    m_osEdit = new QLineEdit("获取中...");
    m_osEdit->setReadOnly(true);
    m_osEdit->setContextMenuPolicy(Qt::DefaultContextMenu);
    m_osEdit->setStyleSheet("QLineEdit[readOnly=\"true\"] { background-color: #f8f9fa; color: #007bff; font-weight: bold; }");
    m_hardwareLayout->addWidget(m_osEdit, row++, 1, 1, 2);
    
    // CPU信息
    m_hardwareLayout->addWidget(new QLabel("处理器:"), row, 0);
    m_cpuEdit = new QTextEdit();
    m_cpuEdit->setReadOnly(true);
    m_cpuEdit->setMaximumHeight(60);
    m_cpuEdit->setContextMenuPolicy(Qt::DefaultContextMenu);
    m_cpuEdit->setStyleSheet("QTextEdit[readOnly=\"true\"] { background-color: #f8f9fa; color: #007bff; font-weight: bold; }");
    m_hardwareLayout->addWidget(m_cpuEdit, row++, 1, 1, 2);
    
    m_hardwareLayout->addWidget(new QLabel("CPU核心:"), row, 0);
    m_coresEdit = new QLineEdit("获取中...");
    m_coresEdit->setReadOnly(true);
    m_coresEdit->setContextMenuPolicy(Qt::DefaultContextMenu);
    m_coresEdit->setStyleSheet("QLineEdit[readOnly=\"true\"] { background-color: #f8f9fa; color: #007bff; font-weight: bold; }");
    m_hardwareLayout->addWidget(m_coresEdit, row++, 1);
    
    m_hardwareLayout->addWidget(new QLabel("CPU频率:"), row, 0);
    m_frequencyEdit = new QLineEdit("获取中...");
    m_frequencyEdit->setReadOnly(true);
    m_frequencyEdit->setContextMenuPolicy(Qt::DefaultContextMenu);
    m_frequencyEdit->setStyleSheet("QLineEdit[readOnly=\"true\"] { background-color: #f8f9fa; color: #007bff; font-weight: bold; }");
    m_hardwareLayout->addWidget(m_frequencyEdit, row++, 1);
    
    // 内存信息
    m_hardwareLayout->addWidget(new QLabel("系统内存:"), row, 0);
    m_memoryEdit = new QLineEdit("获取中...");
    m_memoryEdit->setReadOnly(true);
    m_memoryEdit->setContextMenuPolicy(Qt::DefaultContextMenu);
    m_memoryEdit->setStyleSheet("QLineEdit[readOnly=\"true\"] { background-color: #f8f9fa; color: #007bff; font-weight: bold; }");
    m_hardwareLayout->addWidget(m_memoryEdit, row, 1);
    
    m_memoryProgress = new QProgressBar();
    m_memoryProgress->setRange(0, 100);
    m_hardwareLayout->addWidget(m_memoryProgress, row++, 2);
    
    // 交换内存
    m_hardwareLayout->addWidget(new QLabel("虚拟内存:"), row, 0);
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
    
    m_tabWidget->addTab(m_diskTab, "💾 磁盘信息");
}

void SystemInfoTool::setupControlArea()
{
    m_controlGroup = new QGroupBox("🔧 控制操作");
    QHBoxLayout *layout = new QHBoxLayout(m_controlGroup);
    
    m_refreshBtn = new QPushButton("刷新信息");
    m_refreshBtn->setObjectName("refreshBtn");
    m_exportBtn = new QPushButton("导出报告");
    
    m_lastUpdateLabel = new QLabel("最后更新: 从未");
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
        m_swapEdit->setText("未配置");
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
            
            QMessageBox::information(this, "成功", "系统信息报告已导出");
        } else {
            QMessageBox::warning(this, "错误", "无法写入文件");
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
