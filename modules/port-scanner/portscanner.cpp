#include "portscanner.h"
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QTextStream>
#include <QStandardPaths>
#include <QMessageBox>
#include <QSplitter>
#include <QScrollBar>

// 注册动态对�?
REGISTER_DYNAMICOBJECT(PortScanner);

PortScanner::PortScanner(QWidget *parent)
    : QWidget(parent), currentProcess(nullptr), currentLineIndex(0), currentTableIndex(0)
{
    setupUI();
    setupTable();
    setupConnections();
    applyStyles();

    // 初始化分批处理定时器
    parseTimer = new QTimer(this);
    parseTimer->setSingleShot(false);
    parseTimer->setInterval(1); // 1ms 间隔，让UI保持响应
    connect(parseTimer, &QTimer::timeout, this, &PortScanner::processBatchLines);

    // 初始化表格更新定时器
    tableTimer = new QTimer(this);
    tableTimer->setSingleShot(false);
    tableTimer->setInterval(10); // 10ms 间隔
    connect(tableTimer, &QTimer::timeout, this, &PortScanner::updateTableBatch);

    // 启动时自动刷新一�?
    QTimer::singleShot(100, this, &PortScanner::refreshPortList);
}

PortScanner::~PortScanner()
{
    if (currentProcess && currentProcess->state() != QProcess::NotRunning) {
        currentProcess->kill();
        currentProcess->waitForFinished(1000);
    }
}

void PortScanner::setupUI()
{
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // 控制区域
    controlGroup = new QGroupBox("控制面板");
    controlLayout = new QHBoxLayout(controlGroup);

    refreshButton = new QPushButton("🔄 刷新端口列表");
    refreshButton->setFixedHeight(35);
    refreshButton->setMinimumWidth(150);

    cancelButton = new QPushButton("�?取消扫描");
    cancelButton->setFixedHeight(35);
    cancelButton->setMinimumWidth(120);
    cancelButton->setVisible(false);

    exportButton = new QPushButton("📤 导出数据");
    exportButton->setFixedHeight(35);
    exportButton->setMinimumWidth(120);

    countLabel = new QLabel("端口总数: 0");
    countLabel->setStyleSheet("font-weight: bold; color: #2c3e50;");

    progressBar = new QProgressBar();
    progressBar->setVisible(false);
    progressBar->setFixedHeight(25);

    controlLayout->addWidget(refreshButton);
    controlLayout->addWidget(cancelButton);
    controlLayout->addWidget(exportButton);
    controlLayout->addStretch();
    controlLayout->addWidget(countLabel);
    controlLayout->addWidget(progressBar);

    // 搜索区域
    searchGroup = new QGroupBox("搜索过滤");
    searchLayout = new QHBoxLayout(searchGroup);

    // 端口过滤下拉框
    QComboBox* portFilterCombo = new QComboBox();
    portFilterCombo->addItem("不限", "all");
    portFilterCombo->addItem("只查询本地端口", "local");
    portFilterCombo->addItem("只查询远程端口", "remote");
    portFilterCombo->setFixedWidth(140);
    portFilterCombo->setObjectName("portFilterCombo");

    searchTypeCombo = new QComboBox();
    searchTypeCombo->addItem("全部", "all");
    searchTypeCombo->addItem("端口", "port");
    searchTypeCombo->addItem("进程名名", "process");
    searchTypeCombo->addItem("进程名ID", "pid");
    searchTypeCombo->addItem("协议", "protocol");
    searchTypeCombo->addItem("状态态", "state");
    searchTypeCombo->setFixedWidth(100);

    searchEdit = new QLineEdit();
    searchEdit->setPlaceholderText("输入搜索关键字...");
    searchEdit->setFixedHeight(30);

    searchButton = new QPushButton("🔍 搜索");
    searchButton->setFixedHeight(30);
    searchButton->setFixedWidth(80);

    clearButton = new QPushButton("✖ 清除");
    clearButton->setFixedHeight(30);
    clearButton->setFixedWidth(80);

    searchLayout->addWidget(new QLabel("端口过滤:"));
    searchLayout->addWidget(portFilterCombo);
    searchLayout->addWidget(new QLabel("搜索类型:"));
    searchLayout->addWidget(searchTypeCombo);
    searchLayout->addWidget(new QLabel("关键字:"));
    searchLayout->addWidget(searchEdit);
    searchLayout->addWidget(searchButton);
    searchLayout->addWidget(clearButton);
    searchLayout->addStretch();

    // 连接端口过滤信号
    connect(portFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PortScanner::searchPorts);

    // 表格
    portTable = new QTableWidget();

    // 布局组装
    mainLayout->addWidget(controlGroup);
    mainLayout->addWidget(searchGroup);
    mainLayout->addWidget(portTable, 1);
}

void PortScanner::setupTable()
{
    // 设置列
    QStringList headers = {"协议", "本地地址", "本地端口", "远程地址", "远程端口", "状态", "进程名", "进程ID", "进程路径"};
    portTable->setColumnCount(headers.size());
    portTable->setHorizontalHeaderLabels(headers);

    // 表格属性?
    portTable->setAlternatingRowColors(true);
    portTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    portTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    portTable->setSortingEnabled(true);
    portTable->setContextMenuPolicy(Qt::CustomContextMenu);

    // 表头属性?
    QHeaderView* header = portTable->horizontalHeader();
    header->setStretchLastSection(false);
    header->setSectionResizeMode(QHeaderView::Interactive);

    // 设置列宽
    portTable->setColumnWidth(0, 60);   // 协议
    portTable->setColumnWidth(1, 120);  // 本地地址
    portTable->setColumnWidth(2, 80);   // 本地端口
    portTable->setColumnWidth(3, 120);  // 远程地址
    portTable->setColumnWidth(4, 80);   // 远程端口
    portTable->setColumnWidth(5, 100);  // 状态
    portTable->setColumnWidth(6, 150);  // 进程名
    portTable->setColumnWidth(7, 80);   // 进程ID
    portTable->setColumnWidth(8, 300);  // 进程路径

    // 垂直表头
    portTable->verticalHeader()->setVisible(false);
    portTable->verticalHeader()->setDefaultSectionSize(25);
}

void PortScanner::setupConnections()
{
    connect(refreshButton, &QPushButton::clicked, this, &PortScanner::refreshPortList);
    connect(cancelButton, &QPushButton::clicked, this, &PortScanner::cancelScan);
    connect(exportButton, &QPushButton::clicked, this, &PortScanner::exportToFile);
    connect(searchButton, &QPushButton::clicked, this, &PortScanner::searchPorts);
    connect(clearButton, &QPushButton::clicked, this, &PortScanner::clearSearch);
    connect(searchEdit, &QLineEdit::returnPressed, this, &PortScanner::searchPorts);
    connect(portTable, &QTableWidget::cellDoubleClicked, this, &PortScanner::onTableItemDoubleClicked);
    connect(portTable, &QTableWidget::customContextMenuRequested, this, &PortScanner::showContextMenu);
}

void PortScanner::refreshPortList()
{
    // 如果已有进程名正在运行，先取消
    if (currentProcess && currentProcess->state() != QProcess::NotRunning) {
        currentProcess->kill();
        currentProcess->waitForFinished(1000);
    }

    portInfoList.clear();
    filteredPortInfoList.clear();

    progressBar->setVisible(true);
    progressBar->setRange(0, 0); // 不定进度
    refreshButton->setEnabled(false);
    cancelButton->setVisible(true);
    exportButton->setEnabled(false);

    countLabel->setText("正在扫描端口...");

#ifdef Q_OS_WIN
    // Windows: 使用API
    QTimer::singleShot(10, this, &PortScanner::scanPortsWithWindowsAPI);
#else
    // Linux/Mac: 使用 netstat 命令
    currentProcess = new QProcess(this);

    // 连接启动失败信号
    connect(currentProcess, &QProcess::errorOccurred, [this](QProcess::ProcessError error) {
        Q_UNUSED(error)
        QMessageBox::warning(this, "错误", "无法启动netstat命令");
        progressBar->setVisible(false);
        refreshButton->setEnabled(true);
        cancelButton->setVisible(false);
        exportButton->setEnabled(true);
        countLabel->setText("获取失败");
        currentProcess->deleteLater();
        currentProcess = nullptr;
    });

    // 连接完成信号
    connect(currentProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                Q_UNUSED(exitStatus)

                if (exitCode == 0) {
                    QString output = currentProcess->readAllStandardOutput();
                    // 开始分批处�?
                    portInfoList.clear();
                    pendingLines = output.split('\n');
                    currentLineIndex = 0;

                    countLabel->setText("正在解析数据...");
                    parseTimer->start();
                } else {
                    QString errorOutput = currentProcess->readAllStandardError();
                    if (exitCode != -1) { // -1 表示被手动终�?
                        QMessageBox::warning(this, "错误", QString("获取端口信息失败: %1").arg(errorOutput));
                        countLabel->setText("获取失败");
                    } else {
                        countLabel->setText("扫描已取消?);
                    }

                    progressBar->setVisible(false);
                    refreshButton->setEnabled(true);
                    cancelButton->setVisible(false);
                    exportButton->setEnabled(true);
                }

                currentProcess->deleteLater();
                currentProcess = nullptr;
            });

#ifdef Q_OS_WIN
    // Windows: netstat -ano
    currentProcess->start("netstat", QStringList() << "-ano");
#else
    // Linux/Mac: netstat -tulnp
    currentProcess->start("netstat", QStringList() << "-tulnp");
#endif
#endif

    // 异步启动，不阻塞UI
}

void PortScanner::cancelScan()
{
    if (currentProcess && currentProcess->state() != QProcess::NotRunning) {
        currentProcess->kill();
    }

    // 停止分批处理
    parseTimer->stop();
    tableTimer->stop();

    progressBar->setVisible(false);
    refreshButton->setEnabled(true);
    cancelButton->setVisible(false);
    exportButton->setEnabled(true);
    countLabel->setText("扫描已取消");
}

void PortScanner::processBatchLines()
{
    const int batchSize = 50; // 每批处理50行?
    int processed = 0;

    while (currentLineIndex < pendingLines.size() && processed < batchSize) {
        const QString& line = pendingLines[currentLineIndex];
        QString trimmedLine = line.trimmed();

        if (!trimmedLine.isEmpty() &&
            !trimmedLine.startsWith("活动连接") &&
            !trimmedLine.startsWith("协议") &&
            !trimmedLine.startsWith("Proto") &&
            !trimmedLine.startsWith("Active") &&
            (trimmedLine.startsWith("tcp") || trimmedLine.startsWith("udp") ||
             trimmedLine.startsWith("TCP") || trimmedLine.startsWith("UDP"))) {

#ifdef Q_OS_WIN
            parseNetstatLineWindows(trimmedLine);
#else
            parseNetstatLineLinux(trimmedLine);
#endif
        }

        currentLineIndex++;
        processed++;

        // 更新进度
        if (currentLineIndex % 100 == 0) {
            countLabel->setText(QString("正在解析数据... (%1/%2)").arg(currentLineIndex).arg(pendingLines.size()));
            QApplication::processEvents(); // 让UI保持响应
        }
    }

    // 检查是否完�?
    if (currentLineIndex >= pendingLines.size()) {
        parseTimer->stop();

        // 分批更新表格
        populateTableAsync();
        filteredPortInfoList = portInfoList;
    }
}

void PortScanner::parseNetstatOutput(const QString& output)
{
    QStringList lines = output.split('\n');

    for (const QString& line : lines) {
        QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty() || trimmedLine.startsWith("活动连接") ||
            trimmedLine.startsWith("协议") || trimmedLine.startsWith("Proto") ||
            trimmedLine.startsWith("Active") || trimmedLine.startsWith("tcp") == false &&
            trimmedLine.startsWith("udp") == false && trimmedLine.startsWith("TCP") == false &&
            trimmedLine.startsWith("UDP") == false) {
            continue;
        }

#ifdef Q_OS_WIN
        parseNetstatLineWindows(trimmedLine);
#else
        parseNetstatLineLinux(trimmedLine);
#endif
    }
}

void PortScanner::parseNetstatLineWindows(const QString& line)
{
    // Windows netstat -ano 输出格式:
    // TCP    127.0.0.1:135          0.0.0.0:0              LISTENING       1036

    QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (parts.size() < 4) return;

    PortInfo info;
    info.protocol = parts[0];

    // 解析本地地址和端�?
    QString localAddr = parts[1];
    int lastColon = localAddr.lastIndexOf(':');
    if (lastColon > 0) {
        info.localAddress = localAddr.left(lastColon);
        info.localPort = localAddr.mid(lastColon + 1);
    }

    // 解析远程地址和端�?
    if (parts.size() > 2) {
        QString remoteAddr = parts[2];
        int lastColon = remoteAddr.lastIndexOf(':');
        if (lastColon > 0) {
            info.remoteAddress = remoteAddr.left(lastColon);
            info.remotePort = remoteAddr.mid(lastColon + 1);
        }
    }

    // 状态?(TCP才有)
    if (parts.size() > 3 && info.protocol.toUpper() == "TCP") {
        info.state = parts[3];
        if (parts.size() > 4) {
            info.processId = parts[4];
        }
    } else if (info.protocol.toUpper() == "UDP") {
        info.state = ""; // UDP无状态态?
        if (parts.size() > 3) {
            info.processId = parts[3];
        }
    }

    // 获取进程名?
    info.processName = getProcessName(info.processId);

    portInfoList.append(info);
}

void PortScanner::parseNetstatLineLinux(const QString& line)
{
    // Linux netstat -tulnp 输出格式:
    // tcp        0      0 0.0.0.0:22              0.0.0.0:*               LISTEN      1234/sshd

    QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (parts.size() < 4) return;

    PortInfo info;
    info.protocol = parts[0].toUpper();

    // 解析本地地址和端�?
    QString localAddr = parts[3];
    int lastColon = localAddr.lastIndexOf(':');
    if (lastColon > 0) {
        info.localAddress = localAddr.left(lastColon);
        info.localPort = localAddr.mid(lastColon + 1);
    }

    // 解析远程地址和端�?
    if (parts.size() > 4) {
        QString remoteAddr = parts[4];
        int lastColon = remoteAddr.lastIndexOf(':');
        if (lastColon > 0) {
            info.remoteAddress = remoteAddr.left(lastColon);
            info.remotePort = remoteAddr.mid(lastColon + 1);
        }
    }

    // 状态?
    if (parts.size() > 5) {
        info.state = parts[5];
    }

    // 进程名信息 (最后一列PID/进程名?
    if (parts.size() > 6) {
        QString processInfo = parts[6];
        if (processInfo != "-") {
            QStringList processParts = processInfo.split('/');
            if (processParts.size() >= 2) {
                info.processId = processParts[0];
                info.processName = processParts[1];
            }
        }
    }

    portInfoList.append(info);
}

QString PortScanner::getProcessName(const QString& pid)
{
    if (pid.isEmpty() || pid == "0") return "System";

#ifdef Q_OS_WIN
    // Windows: 使用tasklist命令获取进程名?
    QProcess process;
    process.start("tasklist", QStringList() << "/FI" << QString("PID eq %1").arg(pid) << "/FO" << "CSV" << "/NH");

    if (process.waitForFinished(3000)) {
        QString output = process.readAllStandardOutput();
        QStringList lines = output.split('\n');
        if (!lines.isEmpty()) {
            QString line = lines.first().trimmed();
            if (!line.isEmpty()) {
                // CSV格式: "进程名?,"PID","会话名?,"会话名#","内存使用"
                QStringList parts = line.split(',');
                if (!parts.isEmpty()) {
                    QString processName = parts[0];
                    processName.remove('"'); // 移除引号
                    return processName;
                }
            }
        }
    }
#endif

    return QString("PID:%1").arg(pid);
}

void PortScanner::populateTable(const QList<PortInfo>& portList)
{
    portTable->setRowCount(portList.size());
    portTable->setSortingEnabled(false);

    for (int i = 0; i < portList.size(); ++i) {
        const PortInfo& info = portList[i];

        portTable->setItem(i, 0, new QTableWidgetItem(info.protocol));
        portTable->setItem(i, 1, new QTableWidgetItem(info.localAddress));
        portTable->setItem(i, 2, new QTableWidgetItem(info.localPort));
        portTable->setItem(i, 3, new QTableWidgetItem(info.remoteAddress));
        portTable->setItem(i, 4, new QTableWidgetItem(info.remotePort));
        portTable->setItem(i, 5, new QTableWidgetItem(info.state));
        portTable->setItem(i, 6, new QTableWidgetItem(info.processName));
        portTable->setItem(i, 7, new QTableWidgetItem(info.processId));
        portTable->setItem(i, 8, new QTableWidgetItem(info.processPath));

        // 根据协议设置行颜色
        QColor rowColor;
        if (info.protocol.toUpper() == "TCP") {
            rowColor = QColor(232, 245, 255); // 淡蓝色
        } else {
            rowColor = QColor(245, 255, 232); // 淡绿色
        }

        for (int j = 0; j < 9; ++j) {
            if (portTable->item(i, j)) {
                portTable->item(i, j)->setBackground(rowColor);
            }
        }
    }

    portTable->setSortingEnabled(true);
    resizeTableColumns();
}

void PortScanner::searchPorts()
{
    QString searchText = searchEdit->text().trimmed();
    QString searchType = searchTypeCombo->currentData().toString();

    // 获取端口过滤类型
    QComboBox* portFilterCombo = findChild<QComboBox*>("portFilterCombo");
    QString portFilter = portFilterCombo ? portFilterCombo->currentData().toString() : "all";

    filteredPortInfoList.clear();

    // 用于本地端口去重的集合
    QSet<QString> localPortSet;

    for (const PortInfo& info : portInfoList) {
        // 首先应用端口过滤
        bool portMatch = true;
        if (portFilter == "local") {
            // 只查询本地端口：排除远程端口�?0, *, 或空的连�?
            portMatch = !info.localPort.isEmpty() &&
                       (info.remotePort == "0" || info.remotePort == "*" || info.remotePort.isEmpty() ||
                        info.remoteAddress == "0.0.0.0" || info.remoteAddress == "*" || info.remoteAddress.isEmpty());

            // 去重：如果匹配，检查是否已存在
            if (portMatch) {
                QString localKey = QString("%1:%2:%3").arg(info.protocol).arg(info.localAddress).arg(info.localPort);
                if (localPortSet.contains(localKey)) {
                    continue; // 跳过重复的本地端口
                }
                localPortSet.insert(localKey);
            }
        } else if (portFilter == "remote") {
            // 只查询远程端口：只显示有实际远程连接�?
            portMatch = !info.remotePort.isEmpty() &&
                       info.remotePort != "0" &&
                       info.remotePort != "*" &&
                       !info.remoteAddress.isEmpty() &&
                       info.remoteAddress != "0.0.0.0" &&
                       info.remoteAddress != "*";
        }

        if (!portMatch) {
            continue;
        }

        // 然后应用搜索过滤
        bool searchMatch = true;
        if (!searchText.isEmpty()) {
            if (searchType == "all") {
                searchMatch = info.protocol.contains(searchText, Qt::CaseInsensitive) ||
                       info.localAddress.contains(searchText, Qt::CaseInsensitive) ||
                       info.localPort.contains(searchText, Qt::CaseInsensitive) ||
                       info.remoteAddress.contains(searchText, Qt::CaseInsensitive) ||
                       info.remotePort.contains(searchText, Qt::CaseInsensitive) ||
                       info.state.contains(searchText, Qt::CaseInsensitive) ||
                       info.processName.contains(searchText, Qt::CaseInsensitive) ||
                       info.processId.contains(searchText, Qt::CaseInsensitive);
            } else if (searchType == "port") {
                searchMatch = info.localPort.contains(searchText, Qt::CaseInsensitive) ||
                       info.remotePort.contains(searchText, Qt::CaseInsensitive);
            } else if (searchType == "process") {
                searchMatch = info.processName.contains(searchText, Qt::CaseInsensitive);
            } else if (searchType == "pid") {
                searchMatch = info.processId.contains(searchText, Qt::CaseInsensitive);
            } else if (searchType == "protocol") {
                searchMatch = info.protocol.contains(searchText, Qt::CaseInsensitive);
            } else if (searchType == "state") {
                searchMatch = info.state.contains(searchText, Qt::CaseInsensitive);
            }
        }

        if (searchMatch) {
            filteredPortInfoList.append(info);
        }
    }

    // 按本地端口从小到大排序
    std::sort(filteredPortInfoList.begin(), filteredPortInfoList.end(),
              [](const PortInfo& a, const PortInfo& b) {
                  return a.localPort.toInt() < b.localPort.toInt();
              });

    populateTable(filteredPortInfoList);
    countLabel->setText(QString("显示: %1 / 总数: %2").arg(filteredPortInfoList.size()).arg(portInfoList.size()));
}

void PortScanner::clearSearch()
{
    searchEdit->clear();
    filteredPortInfoList = portInfoList;
    populateTable(filteredPortInfoList);
    countLabel->setText(QString("端口总数: %1").arg(portInfoList.size()));
}

void PortScanner::onTableItemDoubleClicked(int row, int column)
{
    Q_UNUSED(column)

    if (row < 0 || row >= portTable->rowCount()) return;

    // 复制整行信息到剪贴板
    QStringList rowData;
    for (int i = 0; i < portTable->columnCount(); ++i) {
        QTableWidgetItem* item = portTable->item(row, i);
        rowData << (item ? item->text() : "");
    }

    QString text = rowData.join(" | ");
    QApplication::clipboard()->setText(text);

    showStatusMessage("已复制行数据到剪贴板", 2000);
}

void PortScanner::copySelectedToClipboard()
{
    QList<QTableWidgetItem*> selected = portTable->selectedItems();
    if (selected.isEmpty()) return;

    QStringList data;
    for (QTableWidgetItem* item : selected) {
        data << item->text();
    }

    QApplication::clipboard()->setText(data.join('\n'));
    showStatusMessage("已复制选中内容到剪贴板", 2000);
}

void PortScanner::exportToFile()
{
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "导出端口信息",
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + "/port_info.csv",
        "CSV 文件 (*.csv);;文本文件 (*.txt)");

    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "错误", "无法创建文件");
        return;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);

    // 写入表头
    QStringList headers = {"协议", "本地地址", "本地端口", "远程地址", "远程端口", "状态", "进程名", "进程名ID"};
    stream << headers.join(',') << '\n';

    // 写入数据
    const QList<PortInfo>& dataList = filteredPortInfoList.isEmpty() ? portInfoList : filteredPortInfoList;
    for (const PortInfo& info : dataList) {
        QStringList row;
        row << info.protocol << info.localAddress << info.localPort
            << info.remoteAddress << info.remotePort << info.state
            << info.processName << info.processId;
        stream << row.join(',') << '\n';
    }

    file.close();
    showStatusMessage(QString("已导�?%1 条记录到 %2").arg(dataList.size()).arg(fileName), 3000);
}

void PortScanner::showStatusMessage(const QString& message, int timeout)
{
    // 这里可以添加状态态栏消息或者临时提示?
    // 暂时使用简单的工具提示
    countLabel->setToolTip(message);
    QTimer::singleShot(timeout, [this]() {
        countLabel->setToolTip("");
    });
}

void PortScanner::resizeTableColumns()
{
    // 自动调整列宽
    portTable->resizeColumnsToContents();

    // 确保最小宽�?
    for (int i = 0; i < portTable->columnCount(); ++i) {
        int currentWidth = portTable->columnWidth(i);
        int minWidth = 60;
        if (i == 1 || i == 3) minWidth = 100; // 地址�?
        if (i == 6) minWidth = 120; // 进程名名列

        if (currentWidth < minWidth) {
            portTable->setColumnWidth(i, minWidth);
        }
    }
}

void PortScanner::applyStyles()
{

}

void PortScanner::populateTableAsync()
{
    // 准备异步表格更新
    currentTableIndex = 0;
    portTable->setRowCount(portInfoList.size());
    countLabel->setText("正在更新表格...");

    // 开始分批更新表�?
    tableTimer->start();
}

void PortScanner::updateTableBatch()
{
    const int batchSize = 100; // 每批更新100�?
    int processed = 0;

    while (currentTableIndex < portInfoList.size() && processed < batchSize) {
        const PortInfo& info = portInfoList[currentTableIndex];

        // 创建表格项
        portTable->setItem(currentTableIndex, 0, new QTableWidgetItem(info.protocol));
        portTable->setItem(currentTableIndex, 1, new QTableWidgetItem(info.localAddress));
        portTable->setItem(currentTableIndex, 2, new QTableWidgetItem(info.localPort));
        portTable->setItem(currentTableIndex, 3, new QTableWidgetItem(info.remoteAddress));
        portTable->setItem(currentTableIndex, 4, new QTableWidgetItem(info.remotePort));
        portTable->setItem(currentTableIndex, 5, new QTableWidgetItem(info.state));
        portTable->setItem(currentTableIndex, 6, new QTableWidgetItem(info.processName));
        portTable->setItem(currentTableIndex, 7, new QTableWidgetItem(info.processId));
        portTable->setItem(currentTableIndex, 8, new QTableWidgetItem(info.processPath));

        currentTableIndex++;
        processed++;

        // 每处�?00行更新一次进�?
        if (currentTableIndex % 200 == 0) {
            countLabel->setText(QString("正在更新表格... (%1/%2)").arg(currentTableIndex).arg(portInfoList.size()));
        }
    }

    // 检查是否完�?
    if (currentTableIndex >= portInfoList.size()) {
        tableTimer->stop();
        countLabel->setText(QString("端口总数: %1").arg(portInfoList.size()));

        // 调整列宽
        resizeTableColumns();

        // 恢复UI状态态?
        progressBar->setVisible(false);
        refreshButton->setEnabled(true);
        cancelButton->setVisible(false);
        exportButton->setEnabled(true);
    }
}

#ifdef Q_OS_WIN
void PortScanner::scanPortsWithWindowsAPI()
{
    portInfoList.clear();

    countLabel->setText("正在扫描TCP连接...");
    QApplication::processEvents();

    // 扫描TCP连接 (IPv4)
    scanTcpConnections();

    countLabel->setText("正在扫描UDP连接...");
    QApplication::processEvents();

    // 扫描UDP连接 (IPv4)
    scanUdpConnections();

    countLabel->setText("正在整理数据...");
    QApplication::processEvents();

    // 开始分批更新表�?
    populateTableAsync();
    filteredPortInfoList = portInfoList;
}

void PortScanner::scanTcpConnections()
{
    ULONG size = 0;
    DWORD result = GetTcpTable2(nullptr, &size, TRUE);

    if (result == ERROR_INSUFFICIENT_BUFFER) {
        PMIB_TCPTABLE2 tcpTable = (PMIB_TCPTABLE2)malloc(size);
        if (tcpTable) {
            result = GetTcpTable2(tcpTable, &size, TRUE);
            if (result == NO_ERROR) {
                for (DWORD i = 0; i < tcpTable->dwNumEntries; i++) {
                    MIB_TCPROW2& row = tcpTable->table[i];

                    PortInfo info;
                    info.protocol = "TCP";
                    info.localAddress = formatAddress(row.dwLocalAddr, ntohs((WORD)row.dwLocalPort)).split(":")[0];
                    info.localPort = QString::number(ntohs((WORD)row.dwLocalPort));
                    info.remoteAddress = formatAddress(row.dwRemoteAddr, ntohs((WORD)row.dwRemotePort)).split(":")[0];
                    info.remotePort = QString::number(ntohs((WORD)row.dwRemotePort));
                    info.state = getStateString(row.dwState);
                    info.processId = QString::number(row.dwOwningPid);
                    info.processName = getProcessNameByPid(row.dwOwningPid);
                    info.processPath = getProcessPathByPid(row.dwOwningPid);

                    portInfoList.append(info);
                }
            }
            free(tcpTable);
        }
    }
}

void PortScanner::scanUdpConnections()
{
    ULONG size = 0;
    DWORD result = GetExtendedUdpTable(nullptr, &size, TRUE, AF_INET, UDP_TABLE_OWNER_PID, 0);

    if (result == ERROR_INSUFFICIENT_BUFFER) {
        PMIB_UDPTABLE_OWNER_PID udpTable = (PMIB_UDPTABLE_OWNER_PID)malloc(size);
        if (udpTable) {
            result = GetExtendedUdpTable(udpTable, &size, TRUE, AF_INET, UDP_TABLE_OWNER_PID, 0);
            if (result == NO_ERROR) {
                for (DWORD i = 0; i < udpTable->dwNumEntries; i++) {
                    MIB_UDPROW_OWNER_PID& row = udpTable->table[i];

                    PortInfo info;
                    info.protocol = "UDP";
                    info.localAddress = formatAddress(row.dwLocalAddr, ntohs((WORD)row.dwLocalPort)).split(":")[0];
                    info.localPort = QString::number(ntohs((WORD)row.dwLocalPort));
                    info.remoteAddress = "*";
                    info.remotePort = "*";
                    info.state = "LISTENING";
                    info.processId = QString::number(row.dwOwningPid);
                    info.processName = getProcessNameByPid(row.dwOwningPid);
                    info.processPath = getProcessPathByPid(row.dwOwningPid);

                    portInfoList.append(info);
                }
            }
            free(udpTable);
        }
    }
}

QString PortScanner::getProcessNameByPid(DWORD pid)
{
    if (pid == 0) return "System";

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess) {
        TCHAR processName[MAX_PATH];
        DWORD size = MAX_PATH;

        if (QueryFullProcessImageName(hProcess, 0, processName, &size)) {
            CloseHandle(hProcess);
            QString fullPath = QString::fromWCharArray(processName);
            return QFileInfo(fullPath).baseName();
        }
        CloseHandle(hProcess);
    }

    return QString("PID:%1").arg(pid);
}

QString PortScanner::getProcessPathByPid(DWORD pid)
{
    if (pid == 0) return "System";

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess) {
        TCHAR processPath[MAX_PATH];
        DWORD size = MAX_PATH;

        if (QueryFullProcessImageName(hProcess, 0, processPath, &size)) {
            CloseHandle(hProcess);
            return QString::fromWCharArray(processPath);
        }
        CloseHandle(hProcess);
    }

    return QString("");
}

void PortScanner::killProcessByPid(DWORD pid)
{
    if (pid == 0 || pid == 4) {
        QMessageBox::warning(this, "警告", "无法终止系统进程！");
        return;
    }

    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProcess) {
        if (TerminateProcess(hProcess, 0)) {
            CloseHandle(hProcess);
            QMessageBox::information(this, "成功", QString("进程 PID:%1 已被终止").arg(pid));
            // 自动重新扫描
            QTimer::singleShot(500, this, &PortScanner::refreshPortList);
        } else {
            CloseHandle(hProcess);
            QMessageBox::critical(this, "错误", QString("无法终止进程 PID:%1\n可能需要管理员权限").arg(pid));
        }
    } else {
        QMessageBox::critical(this, "错误", QString("无法打开进程 PID:%1\n可能需要管理员权限").arg(pid));
    }
}

QString PortScanner::getStateString(DWORD state)
{
    switch (state) {
        case MIB_TCP_STATE_CLOSED: return "CLOSED";
        case MIB_TCP_STATE_LISTEN: return "LISTENING";
        case MIB_TCP_STATE_SYN_SENT: return "SYN_SENT";
        case MIB_TCP_STATE_SYN_RCVD: return "SYN_RECV";
        case MIB_TCP_STATE_ESTAB: return "ESTABLISHED";
        case MIB_TCP_STATE_FIN_WAIT1: return "FIN_WAIT1";
        case MIB_TCP_STATE_FIN_WAIT2: return "FIN_WAIT2";
        case MIB_TCP_STATE_CLOSE_WAIT: return "CLOSE_WAIT";
        case MIB_TCP_STATE_CLOSING: return "CLOSING";
        case MIB_TCP_STATE_LAST_ACK: return "LAST_ACK";
        case MIB_TCP_STATE_TIME_WAIT: return "TIME_WAIT";
        case MIB_TCP_STATE_DELETE_TCB: return "DELETE_TCB";
        default: return "UNKNOWN";
    }
}

QString PortScanner::formatAddress(DWORD addr, WORD port)
{
    struct in_addr inAddr;
    inAddr.s_addr = addr;

    QString ipStr = QString("%1.%2.%3.%4")
        .arg((addr) & 0xFF)
        .arg((addr >> 8) & 0xFF)
        .arg((addr >> 16) & 0xFF)
        .arg((addr >> 24) & 0xFF);

    return QString("%1:%2").arg(ipStr).arg(port);
}

void PortScanner::showContextMenu(const QPoint& pos)
{
    QTableWidgetItem* item = portTable->itemAt(pos);
    if (!item) return;

    int row = item->row();
    QTableWidgetItem* pidItem = portTable->item(row, 7); // 进程ID列
    QTableWidgetItem* pathItem = portTable->item(row, 8); // 进程路径列

    if (!pidItem) return;

    QMenu contextMenu(this);

    // 查看进程路径
    QAction* showPathAction = contextMenu.addAction("📁 查看进程路径");
    showPathAction->setEnabled(!pathItem || !pathItem->text().isEmpty());

    // 杀死进程
    QAction* killProcessAction = contextMenu.addAction("❌ 终止进程");

    contextMenu.addSeparator();

    // 复制信息
    QAction* copyPidAction = contextMenu.addAction("📋 复制进程ID");
    QAction* copyPathAction = contextMenu.addAction("📋 复制进程路径");
    copyPathAction->setEnabled(!pathItem || !pathItem->text().isEmpty());

    QAction* selectedAction = contextMenu.exec(portTable->viewport()->mapToGlobal(pos));

    if (selectedAction == showPathAction) {
        onShowProcessPathRequested();
    } else if (selectedAction == killProcessAction) {
        onKillProcessRequested();
    } else if (selectedAction == copyPidAction) {
        QApplication::clipboard()->setText(pidItem->text());
        QMessageBox::information(this, "成功", "进程ID已复制到剪贴板");
    } else if (selectedAction == copyPathAction && pathItem) {
        QApplication::clipboard()->setText(pathItem->text());
        QMessageBox::information(this, "成功", "进程路径已复制到剪贴板");
    }
}

void PortScanner::onShowProcessPathRequested()
{
    int currentRow = portTable->currentRow();
    if (currentRow < 0) return;

    QTableWidgetItem* pathItem = portTable->item(currentRow, 8);
    QTableWidgetItem* nameItem = portTable->item(currentRow, 6);

    if (!pathItem || pathItem->text().isEmpty()) {
        QMessageBox::information(this, "提示", "无法获取该进程的路径信息");
        return;
    }

    QString processPath = pathItem->text();
    QString processName = nameItem ? nameItem->text() : "未知";

    QString message = QString("进程名: %1\n\n进程路径:\n%2").arg(processName).arg(processPath);
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("进程路径");
    msgBox.setText(message);
    msgBox.setIcon(QMessageBox::Information);

    // 添加打开文件位置按钮
    QPushButton* openLocationBtn = msgBox.addButton("打开文件位置", QMessageBox::ActionRole);
    msgBox.addButton(QMessageBox::Ok);

    msgBox.exec();

    if (msgBox.clickedButton() == openLocationBtn) {
        QFileInfo fileInfo(processPath);
        if (fileInfo.exists()) {
            QString command = QString("explorer /select,\"%1\"").arg(QDir::toNativeSeparators(processPath));
            QProcess::startDetached(command);
        }
    }
}

void PortScanner::onKillProcessRequested()
{
    int currentRow = portTable->currentRow();
    if (currentRow < 0) return;

    QTableWidgetItem* pidItem = portTable->item(currentRow, 7);
    QTableWidgetItem* nameItem = portTable->item(currentRow, 6);

    if (!pidItem) return;

    bool ok;
    DWORD pid = pidItem->text().toULong(&ok);
    if (!ok) return;

    QString processName = nameItem ? nameItem->text() : "未知";

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "确认终止进程",
        QString("确定要终止进程吗？\n\n进程名: %1\nPID: %2\n\n注意：终止系统进程可能导致系统不稳定！")
            .arg(processName)
            .arg(pid),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        killProcessByPid(pid);
    }
}
#endif
