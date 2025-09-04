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

// ScanWorker 实现
ScanWorker::ScanWorker(QObject* parent)
    : QObject(parent), m_timeout(1000), m_threadCount(50), m_stopRequested(false), 
      m_currentIndex(0), m_activeProcesses(0) {
}

void ScanWorker::setScanParams(const QString& startIP, const QString& endIP, int timeout, int threadCount) {
    m_startIP = startIP;
    m_endIP = endIP;
    m_timeout = timeout;
    m_threadCount = qMax(1, qMin(threadCount, 100)); // 限制线程数在1-100之间
}

void ScanWorker::stop() {
    m_stopRequested = true;
}

void ScanWorker::startScan() {
    m_stopRequested = false;
    m_currentIndex = 0;
    m_activeProcesses = 0;
    
    // 生成IP范围
    m_ipList = generateIPRange(m_startIP, m_endIP);
    
    if (m_ipList.isEmpty()) {
        emit scanFinished();
        return;
    }
    
    // 开始扫描
    scanRange();
}

void ScanWorker::scanRange() {
    // 启动多个并发ping进程
    while (m_activeProcesses < m_threadCount && m_currentIndex < m_ipList.size() && !m_stopRequested) {
        QString ip = m_ipList[m_currentIndex];
        m_currentIndex++;
        m_activeProcesses++;
        
        pingHost(ip);
        emit scanProgress(m_currentIndex, m_ipList.size());
    }
    
    // 如果没有活动进程且已完成所有IP，结束扫描
    if (m_activeProcesses == 0) {
        emit scanFinished();
    }
}

void ScanWorker::pingHost(const QString& ip) {
    QProcess* process = new QProcess(this);
    m_processMap[process] = ip;
    
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ScanWorker::onPingFinished);
    
    // Windows ping命令
#ifdef Q_OS_WIN
    QString program = "ping";
    QStringList arguments;
    arguments << "-n" << "1" << "-w" << QString::number(m_timeout) << ip;
#else
    QString program = "ping";
    QStringList arguments;
    arguments << "-c" << "1" << "-W" << QString::number(m_timeout / 1000) << ip;
#endif
    
    process->start(program, arguments);
}

void ScanWorker::onPingFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    QProcess* process = qobject_cast<QProcess*>(sender());
    if (!process || !m_processMap.contains(process)) {
        return;
    }
    
    QString ip = m_processMap.take(process);
    m_activeProcesses--;
    
    HostInfo hostInfo;
    hostInfo.ipAddress = ip;
    hostInfo.isOnline = (exitCode == 0 && exitStatus == QProcess::NormalExit);
    
    if (hostInfo.isOnline) {
        // 获取响应时间
        QString output = process->readAllStandardOutput();
        QRegularExpression timeRegex(R"(时间[<=](\d+)ms|time[<=](\d+)ms|time=(\d+)ms)");
        QRegularExpressionMatch match = timeRegex.match(output);
        if (match.hasMatch()) {
            for (int i = 1; i <= 3; ++i) {
                if (!match.captured(i).isEmpty()) {
                    hostInfo.responseTime = match.captured(i).toInt();
                    break;
                }
            }
        }
        
        // 获取主机名和MAC地址
        hostInfo.hostName = getHostName(ip);
        hostInfo.macAddress = getMacAddress(ip);
        hostInfo.vendor = getMacVendor(hostInfo.macAddress);
        
        emit hostFound(hostInfo);
    }
    
    process->deleteLater();
    
    // 继续扫描下一个IP
    if (!m_stopRequested) {
        scanRange();
    }
}

QString ScanWorker::getHostName(const QString& ip) {
    // 使用异步查找，避免阻塞
    QHostInfo hostInfo = QHostInfo::fromName(ip);
    if (hostInfo.error() == QHostInfo::NoError && !hostInfo.hostName().isEmpty()) {
        return hostInfo.hostName();
    }
    return "Unknown";
}

QString ScanWorker::getMacAddress(const QString& ip) {
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    
#ifdef Q_OS_WIN
    // Windows使用arp命令
    process.start("arp", QStringList() << "-a" << ip);
#else
    // Linux/Mac使用arp命令  
    process.start("arp", QStringList() << "-n" << ip);
#endif
    
    if (process.waitForFinished(2000)) { // 减少等待时间
        QString output = process.readAllStandardOutput();
        
        // 匹配MAC地址格式 (支持 : 和 - 分隔符)
        QRegularExpression macRegex(R"(([0-9A-Fa-f]{2}[:-]){5}[0-9A-Fa-f]{2})");
        QRegularExpressionMatch match = macRegex.match(output);
        if (match.hasMatch()) {
            QString mac = match.captured(0).toUpper().replace('-', ':');
            // 验证不是全零或全F的无效MAC
            if (mac != "00:00:00:00:00:00" && mac != "FF:FF:FF:FF:FF:FF") {
                return mac;
            }
        }
    }
    
    return "Unknown";
}

QString ScanWorker::getMacVendor(const QString& mac) {
    if (mac == "Unknown" || mac.length() < 8) {
        return "Unknown";
    }
    
    // 简单的MAC地址厂商识别（实际项目中可以使用在线API或本地数据库）
    QString oui = mac.left(8).replace(":", "").toUpper();
    
    static QMap<QString, QString> vendorMap = {
        {"000C29", "VMware"},
        {"005056", "VMware"},
        {"080027", "VirtualBox"},
        {"001C42", "Parallels"},
        {"00155D", "Microsoft"},
        {"0050F2", "Microsoft"},
        {"001B21", "Intel"},
        {"0019E3", "Intel"},
        {"0024E9", "Intel"},
        {"7085C2", "Intel"},
        {"001560", "Apple"},
        {"0017F2", "Apple"},
        {"001EC2", "Apple"},
        {"0025BC", "Apple"},
        {"28E02C", "Apple"},
        {"A45E60", "Apple"},
        {"B8E856", "Apple"},
        {"001377", "Hewlett Packard"},
        {"002264", "Hewlett Packard"},
        {"00188B", "Hewlett Packard"},
        {"001CC0", "Hewlett Packard"},
        {"002481", "Dell"},
        {"001E4F", "Dell"},
        {"0026B9", "Dell"},
        {"001560", "Lenovo"},
        {"00214C", "Lenovo"}
    };
    
    return vendorMap.value(oui, "Unknown");
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
    QHostAddress addr(ip);
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
            QString status = "正在扫描";
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
    
    // 设置样式
    setStyleSheet(R"(
        QGroupBox {
            font-weight: bold;
            border: 2px solid #cccccc;
            border-radius: 5px;
            margin-top: 1ex;
            padding-top: 10px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px 0 5px;
        }
        QPushButton {
            background-color: #f0f0f0;
            border: 1px solid #cccccc;
            border-radius: 4px;
            padding: 6px 12px;
            font-size: 10pt;
        }
        QPushButton:hover {
            background-color: #e0e0e0;
            border-color: #999999;
        }
        QPushButton:pressed {
            background-color: #d0d0d0;
        }
        QPushButton:disabled {
            background-color: #f5f5f5;
            color: #999999;
        }
        QTableWidget {
            gridline-color: #d0d0d0;
            selection-background-color: #4CAF50;
        }
        QTableWidget::item:selected {
            background-color: #4CAF50;
            color: white;
        }
    )");
}

void NetworkScanner::setupScanSettings() {
    m_settingsGroup = new QGroupBox("🔍 扫描设置");
    m_mainLayout->addWidget(m_settingsGroup);
    
    QGridLayout* settingsLayout = new QGridLayout(m_settingsGroup);
    settingsLayout->setSpacing(8);
    
    // 网络接口选择
    settingsLayout->addWidget(new QLabel("网络接口:"), 0, 0);
    m_interfaceCombo = new QComboBox();
    m_interfaceCombo->setMinimumWidth(200);
    connect(m_interfaceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &NetworkScanner::onInterfaceChanged);
    settingsLayout->addWidget(m_interfaceCombo, 0, 1);
    
    m_refreshBtn = new QPushButton("🔄 刷新");
    m_refreshBtn->setMaximumWidth(80);
    connect(m_refreshBtn, &QPushButton::clicked, this, &NetworkScanner::onRefreshInterfaces);
    settingsLayout->addWidget(m_refreshBtn, 0, 2);
    
    // 自定义IP范围
    m_customRangeCheck = new QCheckBox("自定义IP范围");
    connect(m_customRangeCheck, &QCheckBox::toggled, this, &NetworkScanner::onCustomRangeToggled);
    settingsLayout->addWidget(m_customRangeCheck, 1, 0);
    
    // IP范围显示/编辑
    m_ipRangeLabel = new QLabel("IP范围: ");
    settingsLayout->addWidget(m_ipRangeLabel, 2, 0);
    
    QHBoxLayout* ipLayout = new QHBoxLayout();
    m_startIPEdit = new QLineEdit();
    m_startIPEdit->setPlaceholderText("开始IP");
    m_startIPEdit->setEnabled(false);
    ipLayout->addWidget(m_startIPEdit);
    
    ipLayout->addWidget(new QLabel(" - "));
    
    m_endIPEdit = new QLineEdit();
    m_endIPEdit->setPlaceholderText("结束IP");
    m_endIPEdit->setEnabled(false);
    ipLayout->addWidget(m_endIPEdit);
    
    settingsLayout->addLayout(ipLayout, 2, 1, 1, 2);
    
    // 超时设置
    settingsLayout->addWidget(new QLabel("超时(ms):"), 3, 0);
    m_timeoutSpin = new QSpinBox();
    m_timeoutSpin->setRange(100, 10000);
    m_timeoutSpin->setValue(1000);
    m_timeoutSpin->setSuffix(" ms");
    settingsLayout->addWidget(m_timeoutSpin, 3, 1);
    
    // 并发线程数
    settingsLayout->addWidget(new QLabel("并发数:"), 3, 2);
    m_threadSpin = new QSpinBox();
    m_threadSpin->setRange(1, 100);
    m_threadSpin->setValue(50);
    settingsLayout->addWidget(m_threadSpin, 3, 3);
    
    // 控制按钮
    m_buttonLayout = new QHBoxLayout();
    m_scanBtn = new QPushButton("🚀 开始扫描");
    m_scanBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; }");
    connect(m_scanBtn, &QPushButton::clicked, this, &NetworkScanner::onStartScan);
    m_buttonLayout->addWidget(m_scanBtn);
    
    m_stopBtn = new QPushButton("⏹️ 停止扫描");
    m_stopBtn->setEnabled(false);
    m_stopBtn->setStyleSheet("QPushButton { background-color: #f44336; color: white; font-weight: bold; }");
    connect(m_stopBtn, &QPushButton::clicked, this, &NetworkScanner::onStopScan);
    m_buttonLayout->addWidget(m_stopBtn);
    
    m_clearBtn = new QPushButton("🗑️ 清空结果");
    connect(m_clearBtn, &QPushButton::clicked, this, &NetworkScanner::onClearResults);
    m_buttonLayout->addWidget(m_clearBtn);
    
    m_exportBtn = new QPushButton("📤 导出结果");
    connect(m_exportBtn, &QPushButton::clicked, this, &NetworkScanner::onExportResults);
    m_buttonLayout->addWidget(m_exportBtn);
    
    m_buttonLayout->addStretch();
    settingsLayout->addLayout(m_buttonLayout, 4, 0, 1, 4);
}

void NetworkScanner::setupResultsTable() {
    m_resultsGroup = new QGroupBox("📋 扫描结果");
    m_mainLayout->addWidget(m_resultsGroup);
    
    QVBoxLayout* resultsLayout = new QVBoxLayout(m_resultsGroup);
    
    m_resultsTable = new QTableWidget();
    m_resultsTable->setColumnCount(6);
    QStringList headers = {"IP地址", "主机名", "MAC地址", "厂商", "响应时间(ms)", "状态"};
    m_resultsTable->setHorizontalHeaderLabels(headers);
    
    // 设置表格属性
    m_resultsTable->setAlternatingRowColors(true);
    m_resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultsTable->setSortingEnabled(true);
    m_resultsTable->horizontalHeader()->setStretchLastSection(false);
    
    // 设置列宽
    m_resultsTable->setColumnWidth(0, 120);  // IP地址
    m_resultsTable->setColumnWidth(1, 150);  // 主机名
    m_resultsTable->setColumnWidth(2, 130);  // MAC地址
    m_resultsTable->setColumnWidth(3, 100);  // 厂商
    m_resultsTable->setColumnWidth(4, 100);  // 响应时间
    m_resultsTable->setColumnWidth(5, 80);   // 状态
    
    resultsLayout->addWidget(m_resultsTable);
}

void NetworkScanner::setupStatusArea() {
    m_statusGroup = new QGroupBox("📊 扫描状态");
    m_mainLayout->addWidget(m_statusGroup);
    
    QVBoxLayout* statusLayout = new QVBoxLayout(m_statusGroup);
    
    // 进度条和状态标签
    QHBoxLayout* progressLayout = new QHBoxLayout();
    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    progressLayout->addWidget(m_progressBar);
    
    m_statusLabel = new QLabel("就绪");
    m_statusLabel->setStyleSheet("color: #666; font-weight: bold;");
    progressLayout->addWidget(m_statusLabel);
    
    m_foundLabel = new QLabel("发现主机: 0");
    m_foundLabel->setStyleSheet("color: #4CAF50; font-weight: bold;");
    progressLayout->addWidget(m_foundLabel);
    
    statusLayout->addLayout(progressLayout);
    
    // 日志文本
    m_logText = new QTextEdit();
    m_logText->setMaximumHeight(100);
    m_logText->setPlaceholderText("扫描日志将显示在这里...");
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
        QMessageBox::warning(this, "输入错误", "请输入有效的IP地址范围");
        return false;
    }
    
    QHostAddress startAddr(startIP);
    QHostAddress endAddr(endIP);
    
    if (startAddr.protocol() != QAbstractSocket::IPv4Protocol ||
        endAddr.protocol() != QAbstractSocket::IPv4Protocol) {
        QMessageBox::warning(this, "输入错误", "请输入有效的IPv4地址");
        return false;
    }
    
    quint32 start = startAddr.toIPv4Address();
    quint32 end = endAddr.toIPv4Address();
    
    if (start > end) {
        QMessageBox::warning(this, "输入错误", "开始IP不能大于结束IP");
        return false;
    }
    
    if (end - start > 65535) {
        QMessageBox::warning(this, "范围过大", "IP范围不能超过65535个地址");
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
    m_foundLabel->setText("发现主机: 0");
    m_logText->clear();
    
    // 创建扫描线程
    m_scanThread = new QThread(this);
    m_scanWorker = new ScanWorker();
    m_scanWorker->moveToThread(m_scanThread);
    
    // 设置扫描参数
    m_scanWorker->setScanParams(
        m_startIPEdit->text().trimmed(),
        m_endIPEdit->text().trimmed(),
        m_timeoutSpin->value(),
        m_threadSpin->value()
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
    if (m_scanWorker) {
        m_scanWorker->stop();
    }
    
    if (m_scanThread && m_scanThread->isRunning()) {
        m_scanThread->quit();
        m_scanThread->wait(3000);
    }
    
    onScanFinished();
    
    m_logText->append(QString("[%1] 用户停止扫描")
                     .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
}

void NetworkScanner::onClearResults() {
    m_resultsTable->setRowCount(0);
    m_foundHosts = 0;
    m_foundLabel->setText("发现主机: 0");
    m_logText->clear();
    m_progressBar->setValue(0);
    m_progressBar->setVisible(false);
    m_statusLabel->setText("就绪");
}

void NetworkScanner::onExportResults() {
    if (m_resultsTable->rowCount() == 0) {
        QMessageBox::information(this, "导出结果", "没有可导出的数据");
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
    m_foundHosts++;
    m_foundLabel->setText(QString("发现主机: %1").arg(m_foundHosts));
    
    // 添加到表格
    int row = m_resultsTable->rowCount();
    m_resultsTable->insertRow(row);
    
    m_resultsTable->setItem(row, 0, new QTableWidgetItem(hostInfo.ipAddress));
    m_resultsTable->setItem(row, 1, new QTableWidgetItem(hostInfo.hostName));
    m_resultsTable->setItem(row, 2, new QTableWidgetItem(hostInfo.macAddress));
    m_resultsTable->setItem(row, 3, new QTableWidgetItem(hostInfo.vendor));
    
    QString responseTime = hostInfo.responseTime >= 0 ? 
                          QString::number(hostInfo.responseTime) : "N/A";
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
}

void NetworkScanner::onScanProgress(int current, int total) {
    m_progressBar->setMaximum(total);
    m_progressBar->setValue(current);
}

void NetworkScanner::onScanFinished() {
    updateScanButton(false);
    m_progressBar->setVisible(false);
    m_statusTimer->stop();
    m_statusLabel->setText(QString("扫描完成 - 发现 %1 台主机").arg(m_foundHosts));
    
    if (m_scanThread && m_scanThread->isRunning()) {
        m_scanThread->quit();
        m_scanThread->wait();
    }
    
    m_logText->append(QString("[%1] 扫描完成，共发现 %2 台在线主机")
                     .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                     .arg(m_foundHosts));
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

#include "networkscanner.moc"
