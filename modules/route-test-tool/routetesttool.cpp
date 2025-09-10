#include "routetesttool.h"
#include <QApplication>
#include <QMessageBox>
#include <QHeaderView>
#include <QFileDialog>
#include <QDateTime>
#include <QDebug>
#include <QTextStream>
#include <QStandardPaths>
#include <QDir>

REGISTER_DYNAMICOBJECT(RouteTestTool);

// RouteTestWorker 实现
RouteTestWorker::RouteTestWorker(QObject *parent)
    : QObject(parent)
    , m_running(false)
    , m_process(nullptr)
{
}

void RouteTestWorker::testSingleRoute(const QString &ip, const QString &name)
{
    if (m_running) {
        emit logMessage("测试正在进行中，请等待完成");
        return;
    }
    
    m_running = true;
    emit testStarted(name.isEmpty() ? ip : name);
    
    RouteTestResult result = performTraceroute(ip, name);
    
    m_running = false;
    emit testCompleted(result);
}

void RouteTestWorker::testMultipleRoutes(const QList<TestNode> &nodes)
{
    if (m_running) {
        emit logMessage("测试正在进行中，请等待完成");
        return;
    }
    
    m_running = true;
    
    for (int i = 0; i < nodes.size() && m_running; ++i) {
        const TestNode &node = nodes[i];
        
        emit progressUpdated(i + 1, nodes.size());
        emit testStarted(node.name);
        emit logMessage(QString("开始测试 %1 (%2)").arg(node.name, node.ip));
        
        RouteTestResult result = performTraceroute(node.ip, node.name);
        emit testCompleted(result);
        
        // 测试间隔
        if (i < nodes.size() - 1 && m_running) {
            QThread::msleep(1000);
        }
    }
    
    m_running = false;
    emit allTestsCompleted();
}

void RouteTestWorker::stopTest()
{
    m_running = false;
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
    }
}

RouteTestResult RouteTestWorker::performTraceroute(const QString &ip, const QString &name)
{
    RouteTestResult result;
    result.targetIP = ip;
    result.targetName = name;
    result.testTime = QDateTime::currentDateTime();
    
    // 创建traceroute进程
    m_process = new QProcess();
    m_process->setProgram("tracert");  // Windows使用tracert命令
    m_process->setArguments({"-h", "30", ip});  // 最大30跳
    
    emit logMessage(QString("执行命令: tracert -h 30 %1").arg(ip));
    
    // 启动进程并等待完成
    m_process->start();
    
    if (!m_process->waitForStarted(5000)) {
        result.success = false;
        result.errorMessage = "无法启动tracert命令";
        emit logMessage("错误: " + result.errorMessage);
        delete m_process;
        m_process = nullptr;
        return result;
    }
    
    if (!m_process->waitForFinished(60000)) {  // 最多等待60秒
        result.success = false;
        result.errorMessage = "tracert命令超时";
        m_process->kill();
        emit logMessage("错误: " + result.errorMessage);
        delete m_process;
        m_process = nullptr;
        return result;
    }
    
    // 读取输出
    QString output = QString::fromLocal8Bit(m_process->readAllStandardOutput());
    QString errorOutput = QString::fromLocal8Bit(m_process->readAllStandardError());
    
    if (m_process->exitCode() == 0) {
        result.success = true;
        result.routeHops = parseTracerouteOutput(output);
        result.totalHops = result.routeHops.size();
        emit logMessage(QString("成功: 共%1跳到达%2").arg(result.totalHops).arg(ip));
    } else {
        result.success = false;
        result.errorMessage = errorOutput.isEmpty() ? "tracert命令执行失败" : errorOutput;
        emit logMessage("错误: " + result.errorMessage);
    }
    
    delete m_process;
    m_process = nullptr;
    
    return result;
}

QStringList RouteTestWorker::parseTracerouteOutput(const QString &output)
{
    QStringList hops;
    QStringList lines = output.split('\n');
    
    for (const QString &line : lines) {
        QString trimmedLine = line.trimmed();
        
        // 跳过空行和标题行
        if (trimmedLine.isEmpty() || 
            trimmedLine.contains("tracert") || 
            trimmedLine.contains("Tracing route") ||
            trimmedLine.contains("over a maximum")) {
            continue;
        }
        
        // 解析跳点行（格式：序号 时间1 时间2 时间3 IP/主机名）
        if (trimmedLine.contains("ms") || trimmedLine.contains("*")) {
            hops.append(trimmedLine);
        }
    }
    
    return hops;
}

// RouteTestTool 实现
RouteTestTool::RouteTestTool(QWidget *parent)
    : QWidget(parent), DynamicObjectBase()
    , m_workerThread(nullptr)
    , m_worker(nullptr)
    , m_testing(false)
    , m_currentTestIndex(0)
    , m_totalTests(0)
{
    setupUI();
    initializeTestNodes();
    loadSettings();
    
    // 创建工作线程
    m_workerThread = new QThread(this);
    m_worker = new RouteTestWorker();
    m_worker->moveToThread(m_workerThread);
    
    // 连接信号
    connect(m_worker, &RouteTestWorker::testStarted,
            this, &RouteTestTool::onTestStarted);
    connect(m_worker, &RouteTestWorker::testCompleted,
            this, &RouteTestTool::onTestCompleted);
    connect(m_worker, &RouteTestWorker::allTestsCompleted,
            this, &RouteTestTool::onAllTestsCompleted);
    connect(m_worker, &RouteTestWorker::progressUpdated,
            this, &RouteTestTool::onProgressUpdated);
    connect(m_worker, &RouteTestWorker::logMessage,
            this, &RouteTestTool::onLogMessage);
    
    // 启动工作线程
    m_workerThread->start();
    
    logMessage("回程路由测试工具初始化完成");
    logMessage("支持电信、联通、移动、教育网四网测试");
}

RouteTestTool::~RouteTestTool()
{
    if (m_testing) {
        m_worker->stopTest();
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

void RouteTestTool::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    m_mainLayout->setSpacing(10);
    
    // 创建主分割器
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    
    // 左侧控制面板
    m_controlPanel = new QWidget();
    m_controlLayout = new QVBoxLayout(m_controlPanel);
    m_controlLayout->setSpacing(10);
    
    setupQuickTestArea();
    setupCustomTestArea();
    setupNodeTestArea();
    setupControlArea();
    
    m_controlLayout->addStretch();
    
    // 右侧结果面板
    m_resultsPanel = new QWidget();
    m_resultsLayout = new QVBoxLayout(m_resultsPanel);
    
    setupResultsArea();
    
    // 设置分割器
    m_mainSplitter->addWidget(m_controlPanel);
    m_mainSplitter->addWidget(m_resultsPanel);
    m_mainSplitter->setStretchFactor(0, 0);
    m_mainSplitter->setStretchFactor(1, 1);
    m_mainSplitter->setSizes({400, 600});
    
    m_mainLayout->addWidget(m_mainSplitter);
    
    // 应用样式
    setStyleSheet(R"(
        QWidget {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            font-size: 10px;
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
        QPushButton#quickTestBtn {
            background-color: #28a745;
            color: white;
            font-weight: bold;
            font-size: 11px;
        }
        QPushButton#quickTestBtn:hover {
            background-color: #218838;
        }
        QPushButton#stopBtn {
            background-color: #dc3545;
            color: white;
            font-weight: bold;
        }
        QPushButton#stopBtn:hover {
            background-color: #c82333;
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
        QLineEdit, QComboBox {
            border: 2px solid #e1e5e9;
            border-radius: 6px;
            padding: 4px 8px;
            font-size: 10px;
            min-height: 20px;
        }
        QLineEdit:focus, QComboBox:focus {
            border-color: #007bff;
        }
        QTableWidget {
            border: 1px solid #dee2e6;
            gridline-color: #dee2e6;
            background-color: white;
            alternate-background-color: #f8f9fa;
        }
        QTextEdit {
            border: 2px solid #e1e5e9;
            border-radius: 6px;
            font-family: 'Consolas', 'Monaco', monospace;
            font-size: 9px;
        }
    )");
}

void RouteTestTool::setupQuickTestArea()
{
    m_quickTestGroup = new QGroupBox("🚀 四网快速测试", this);
    QVBoxLayout *layout = new QVBoxLayout(m_quickTestGroup);
    
    m_quickTestDesc = new QLabel(
        "一键测试到以下节点的回程路由：\n"
        "• 上海电信(天翼云)\n"
        "• 厦门电信CN2\n" 
        "• 浙江杭州联通\n"
        "• 浙江杭州移动\n"
        "• 北京教育网"
    );
    m_quickTestDesc->setStyleSheet("color: #6c757d; font-weight: normal;");
    layout->addWidget(m_quickTestDesc);
    
    m_quickTestBtn = new QPushButton("开始四网快速测试");
    m_quickTestBtn->setObjectName("quickTestBtn");
    layout->addWidget(m_quickTestBtn);
    
    m_controlLayout->addWidget(m_quickTestGroup);
    
    connect(m_quickTestBtn, &QPushButton::clicked, this, &RouteTestTool::onQuickTestClicked);
}

void RouteTestTool::setupCustomTestArea()
{
    m_customTestGroup = new QGroupBox("🎯 自定义IP测试", this);
    QVBoxLayout *layout = new QVBoxLayout(m_customTestGroup);
    
    // IP输入
    layout->addWidget(new QLabel("目标IP地址:"));
    m_customIPEdit = new QLineEdit();
    m_customIPEdit->setPlaceholderText("输入IP地址，如: 8.8.8.8");
    layout->addWidget(m_customIPEdit);
    
    // 名称输入
    layout->addWidget(new QLabel("节点名称(可选):"));
    m_customNameEdit = new QLineEdit();
    m_customNameEdit->setPlaceholderText("为此IP起个名字，如: Google DNS");
    layout->addWidget(m_customNameEdit);
    
    m_customTestBtn = new QPushButton("测试此IP");
    layout->addWidget(m_customTestBtn);
    
    m_controlLayout->addWidget(m_customTestGroup);
    
    connect(m_customIPEdit, &QLineEdit::textChanged, this, &RouteTestTool::onCustomIPChanged);
    connect(m_customTestBtn, &QPushButton::clicked, this, &RouteTestTool::onCustomTestClicked);
}

void RouteTestTool::setupNodeTestArea()
{
    m_nodeTestGroup = new QGroupBox("🌐 节点选择测试", this);
    QVBoxLayout *layout = new QVBoxLayout(m_nodeTestGroup);
    
    // ISP选择
    layout->addWidget(new QLabel("选择运营商:"));
    m_ispCombo = new QComboBox();
    m_ispCombo->addItems({"电信", "联通", "移动", "教育网"});
    layout->addWidget(m_ispCombo);
    
    // 节点选择
    layout->addWidget(new QLabel("选择节点:"));
    m_nodeCombo = new QComboBox();
    layout->addWidget(m_nodeCombo);
    
    m_nodeTestBtn = new QPushButton("测试选中节点");
    layout->addWidget(m_nodeTestBtn);
    
    m_controlLayout->addWidget(m_nodeTestGroup);
    
    connect(m_ispCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RouteTestTool::onNodeSelectionChanged);
    connect(m_nodeTestBtn, &QPushButton::clicked, this, &RouteTestTool::onNodeTestClicked);
}

void RouteTestTool::setupControlArea()
{
    m_controlGroup = new QGroupBox("🔧 控制操作", this);
    QVBoxLayout *layout = new QVBoxLayout(m_controlGroup);
    
    // 进度条
    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    layout->addWidget(m_progressBar);
    
    // 状态标签
    m_statusLabel = new QLabel("就绪");
    m_statusLabel->setStyleSheet("font-weight: bold; color: #007bff;");
    layout->addWidget(m_statusLabel);
    
    // 控制按钮
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_stopBtn = new QPushButton("停止测试");
    m_stopBtn->setObjectName("stopBtn");
    m_stopBtn->setEnabled(false);
    m_clearBtn = new QPushButton("清空结果");
    m_exportBtn = new QPushButton("导出结果");
    
    btnLayout->addWidget(m_stopBtn);
    btnLayout->addWidget(m_clearBtn);
    btnLayout->addWidget(m_exportBtn);
    
    layout->addLayout(btnLayout);
    m_controlLayout->addWidget(m_controlGroup);
    
    connect(m_stopBtn, &QPushButton::clicked, this, &RouteTestTool::onStopTestClicked);
    connect(m_clearBtn, &QPushButton::clicked, this, &RouteTestTool::onClearResultsClicked);
    connect(m_exportBtn, &QPushButton::clicked, this, &RouteTestTool::onExportResultsClicked);
}

void RouteTestTool::setupResultsArea()
{
    // 创建标签页
    m_resultTabs = new QTabWidget();
    
    // 结果表格标签页
    m_resultsTable = new QTableWidget();
    m_resultsTable->setColumnCount(5);
    QStringList headers;
    headers << "测试时间" << "目标名称" << "目标IP" << "跳数" << "状态";
    m_resultsTable->setHorizontalHeaderLabels(headers);
    
    m_resultsTable->setAlternatingRowColors(true);
    m_resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultsTable->horizontalHeader()->setStretchLastSection(true);
    m_resultsTable->verticalHeader()->setVisible(false);
    
    m_resultTabs->addTab(m_resultsTable, "📊 测试结果");
    
    // 当前测试详情标签页
    m_currentTestText = new QTextEdit();
    m_currentTestText->setReadOnly(true);
    m_resultTabs->addTab(m_currentTestText, "🔍 当前测试详情");
    
    // 日志标签页
    m_logText = new QTextEdit();
    m_logText->setReadOnly(true);
    m_logText->setMaximumHeight(150);
    m_resultTabs->addTab(m_logText, "📝 操作日志");
    
    m_resultsLayout->addWidget(m_resultTabs);
}

void RouteTestTool::initializeTestNodes()
{
    // 电信节点
    QList<TestNode> telecomNodes;
    telecomNodes.append(TestNode("上海电信(天翼云)", "101.227.255.45", "电信", "上海"));
    telecomNodes.append(TestNode("厦门电信CN2", "117.28.254.129", "电信", "厦门"));
    telecomNodes.append(TestNode("湖北襄阳电信", "58.51.94.106", "电信", "湖北"));
    telecomNodes.append(TestNode("江西南昌电信", "182.98.238.226", "电信", "江西"));
    telecomNodes.append(TestNode("广东深圳电信", "119.147.52.35", "电信", "广东"));
    telecomNodes.append(TestNode("广州电信(天翼云)", "14.215.116.1", "电信", "广州"));
    m_testNodes["电信"] = telecomNodes;
    
    // 联通节点
    QList<TestNode> unicomNodes;
    unicomNodes.append(TestNode("西藏拉萨联通", "221.13.70.244", "联通", "西藏"));
    unicomNodes.append(TestNode("重庆联通", "113.207.32.65", "联通", "重庆"));
    unicomNodes.append(TestNode("河南郑州联通", "61.168.23.74", "联通", "河南"));
    unicomNodes.append(TestNode("安徽合肥联通", "112.122.10.26", "联通", "安徽"));
    unicomNodes.append(TestNode("江苏南京联通", "58.240.53.78", "联通", "江苏"));
    unicomNodes.append(TestNode("浙江杭州联通", "101.71.241.238", "联通", "浙江"));
    m_testNodes["联通"] = unicomNodes;
    
    // 移动节点
    QList<TestNode> mobileNodes;
    mobileNodes.append(TestNode("上海移动", "221.130.188.251", "移动", "上海"));
    mobileNodes.append(TestNode("四川成都移动", "183.221.247.9", "移动", "四川"));
    mobileNodes.append(TestNode("安徽合肥移动", "120.209.140.60", "移动", "安徽"));
    mobileNodes.append(TestNode("浙江杭州移动", "112.17.0.106", "移动", "浙江"));
    m_testNodes["移动"] = mobileNodes;
    
    // 教育网节点
    QList<TestNode> eduNodes;
    eduNodes.append(TestNode("北京教育网", "202.205.6.30", "教育网", "北京"));
    m_testNodes["教育网"] = eduNodes;
}

QList<TestNode> RouteTestTool::getQuickTestNodes() const
{
    QList<TestNode> quickNodes;
    quickNodes.append(TestNode("上海电信(天翼云)", "101.227.255.45", "电信", "上海"));
    quickNodes.append(TestNode("厦门电信CN2", "117.28.254.129", "电信", "厦门"));
    quickNodes.append(TestNode("浙江杭州联通", "101.71.241.238", "联通", "浙江"));
    quickNodes.append(TestNode("浙江杭州移动", "112.17.0.106", "移动", "浙江"));
    quickNodes.append(TestNode("北京教育网", "202.205.6.30", "教育网", "北京"));
    return quickNodes;
}

void RouteTestTool::updateUI()
{
    bool hasCustomIP = !m_customIPEdit->text().trimmed().isEmpty();
    
    m_quickTestBtn->setEnabled(!m_testing);
    m_customTestBtn->setEnabled(!m_testing && hasCustomIP);
    m_nodeTestBtn->setEnabled(!m_testing);
    m_stopBtn->setEnabled(m_testing);
    m_clearBtn->setEnabled(!m_testing && !m_testResults.isEmpty());
    m_exportBtn->setEnabled(!m_testing && !m_testResults.isEmpty());
    
    m_progressBar->setVisible(m_testing && m_totalTests > 1);
    
    if (m_testing) {
        m_statusLabel->setText("测试进行中...");
        m_statusLabel->setStyleSheet("font-weight: bold; color: #28a745;");
    } else {
        m_statusLabel->setText("就绪");
        m_statusLabel->setStyleSheet("font-weight: bold; color: #007bff;");
    }
}

void RouteTestTool::addResultToTable(const RouteTestResult &result)
{
    int row = m_resultsTable->rowCount();
    m_resultsTable->insertRow(row);
    
    m_resultsTable->setItem(row, 0, new QTableWidgetItem(result.testTime.toString("MM-dd hh:mm:ss")));
    m_resultsTable->setItem(row, 1, new QTableWidgetItem(result.targetName));
    m_resultsTable->setItem(row, 2, new QTableWidgetItem(result.targetIP));
    m_resultsTable->setItem(row, 3, new QTableWidgetItem(QString::number(result.totalHops)));
    
    QTableWidgetItem *statusItem = new QTableWidgetItem(result.success ? "成功" : "失败");
    if (result.success) {
        statusItem->setBackground(QColor("#d4edda"));
        statusItem->setForeground(QColor("#155724"));
    } else {
        statusItem->setBackground(QColor("#f8d7da"));
        statusItem->setForeground(QColor("#721c24"));
    }
    m_resultsTable->setItem(row, 4, statusItem);
    
    m_resultsTable->scrollToBottom();
    
    // 显示详细路由信息
    QString detailText = QString("=== %1 (%2) ===\n").arg(result.targetName).arg(result.targetIP);
    detailText += QString("测试时间: %1\n").arg(result.testTime.toString());
    detailText += QString("状态: %1\n").arg(result.success ? "成功" : "失败");
    detailText += QString("总跳数: %1\n\n").arg(result.totalHops);
    
    if (result.success) {
        detailText += "路由详情:\n";
        for (int i = 0; i < result.routeHops.size(); ++i) {
            detailText += QString("%1\n").arg(result.routeHops[i]);
        }
    } else {
        detailText += QString("错误信息: %1\n").arg(result.errorMessage);
    }
    
    detailText += "\n" + QString("=").repeated(50) + "\n\n";
    m_currentTestText->append(detailText);
}

void RouteTestTool::logMessage(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString logEntry = QString("[%1] %2").arg(timestamp, message);
    m_logText->append(logEntry);
    
    // 限制日志行数
    if (m_logText->document()->lineCount() > 200) {
        QTextCursor cursor = m_logText->textCursor();
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, 20);
        cursor.removeSelectedText();
    }
}

void RouteTestTool::saveSettings()
{
    QSettings settings;
    settings.setValue("RouteTestTool/customIP", m_customIPEdit->text());
    settings.setValue("RouteTestTool/customName", m_customNameEdit->text());
    settings.setValue("RouteTestTool/selectedISP", m_ispCombo->currentText());
}

void RouteTestTool::loadSettings()
{
    QSettings settings;
    if (m_customIPEdit) {
        m_customIPEdit->setText(settings.value("RouteTestTool/customIP", "8.8.8.8").toString());
    }
    if (m_customNameEdit) {
        m_customNameEdit->setText(settings.value("RouteTestTool/customName", "Google DNS").toString());
    }
    if (m_ispCombo) {
        QString savedISP = settings.value("RouteTestTool/selectedISP", "电信").toString();
        int index = m_ispCombo->findText(savedISP);
        if (index >= 0) {
            m_ispCombo->setCurrentIndex(index);
        }
    }
    
    // 触发节点选择更新
    onNodeSelectionChanged();
}

// 槽函数实现
void RouteTestTool::onQuickTestClicked()
{
    QList<TestNode> quickNodes = getQuickTestNodes();
    
    m_testing = true;
    m_totalTests = quickNodes.size();
    m_currentTestIndex = 0;
    
    m_progressBar->setRange(0, m_totalTests);
    m_progressBar->setValue(0);
    
    updateUI();
    logMessage("开始四网快速测试");
    
    QMetaObject::invokeMethod(m_worker, "testMultipleRoutes", Qt::QueuedConnection,
                              Q_ARG(QList<TestNode>, quickNodes));
}

void RouteTestTool::onCustomTestClicked()
{
    QString ip = m_customIPEdit->text().trimmed();
    QString name = m_customNameEdit->text().trimmed();
    
    if (ip.isEmpty()) {
        QMessageBox::warning(this, "错误", "请输入目标IP地址");
        return;
    }
    
    if (name.isEmpty()) {
        name = ip;
    }
    
    m_testing = true;
    m_totalTests = 1;
    updateUI();
    
    logMessage(QString("开始测试自定义IP: %1").arg(ip));
    
    QMetaObject::invokeMethod(m_worker, "testSingleRoute", Qt::QueuedConnection,
                              Q_ARG(QString, ip), Q_ARG(QString, name));
}

void RouteTestTool::onNodeTestClicked()
{
    QList<TestNode> selectedNodes = getSelectedNodes();
    if (selectedNodes.isEmpty()) {
        QMessageBox::warning(this, "错误", "请选择要测试的节点");
        return;
    }
    
    m_testing = true;
    m_totalTests = selectedNodes.size();
    m_currentTestIndex = 0;
    
    if (m_totalTests > 1) {
        m_progressBar->setRange(0, m_totalTests);
        m_progressBar->setValue(0);
    }
    
    updateUI();
    logMessage(QString("开始测试%1节点").arg(m_ispCombo->currentText()));
    
    QMetaObject::invokeMethod(m_worker, "testMultipleRoutes", Qt::QueuedConnection,
                              Q_ARG(QList<TestNode>, selectedNodes));
}

void RouteTestTool::onStopTestClicked()
{
    m_testing = false;
    QMetaObject::invokeMethod(m_worker, "stopTest", Qt::QueuedConnection);
    logMessage("用户停止了测试");
    updateUI();
}

void RouteTestTool::onClearResultsClicked()
{
    m_resultsTable->setRowCount(0);
    m_currentTestText->clear();
    m_logText->clear();
    m_testResults.clear();
    logMessage("结果已清空");
}

void RouteTestTool::onExportResultsClicked()
{
    if (m_testResults.isEmpty()) {
        QMessageBox::information(this, "提示", "没有测试结果可导出");
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "导出路由测试结果",
        QString("route_test_results_%1.txt")
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")),
        "文本文件 (*.txt)"
    );
    
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "回程路由测试结果报告\n";
            out << "生成时间: " << QDateTime::currentDateTime().toString() << "\n";
            out << "总计测试: " << m_testResults.size() << " 个节点\n\n";
            
            for (const RouteTestResult &result : m_testResults) {
                out << QString("=== %1 (%2) ===\n").arg(result.targetName, result.targetIP);
                out << QString("测试时间: %1\n").arg(result.testTime.toString());
                out << QString("状态: %1\n").arg(result.success ? "成功" : "失败");
                out << QString("跳数: %1\n\n").arg(result.totalHops);
                
                if (result.success) {
                    for (const QString &hop : result.routeHops) {
                        out << hop << "\n";
                    }
                } else {
                    out << "错误: " << result.errorMessage << "\n";
                }
                out << "\n";
            }
            
            logMessage("结果已导出到: " + fileName);
            QMessageBox::information(this, "成功", "测试结果已导出");
        }
    }
}

void RouteTestTool::onCustomIPChanged()
{
    updateUI();
}

void RouteTestTool::onNodeSelectionChanged()
{
    QString isp = m_ispCombo->currentText();
    m_nodeCombo->clear();
    
    if (m_testNodes.contains(isp)) {
        const QList<TestNode> &nodes = m_testNodes[isp];
        for (const TestNode &node : nodes) {
            m_nodeCombo->addItem(QString("%1 (%2)").arg(node.name, node.location), 
                                QVariant::fromValue(node));
        }
    }
}

QList<TestNode> RouteTestTool::getSelectedNodes() const
{
    QList<TestNode> selectedNodes;
    QVariant nodeData = m_nodeCombo->currentData();
    if (nodeData.canConvert<TestNode>()) {
        selectedNodes.append(nodeData.value<TestNode>());
    }
    return selectedNodes;
}

// 工作线程信号处理
void RouteTestTool::onTestStarted(const QString &target)
{
    logMessage(QString("开始测试: %1").arg(target));
    m_statusLabel->setText(QString("正在测试: %1").arg(target));
}

void RouteTestTool::onTestCompleted(const RouteTestResult &result)
{
    m_testResults.append(result);
    addResultToTable(result);
    
    if (result.success) {
        logMessage(QString("测试完成: %1 (%2跳)").arg(result.targetName).arg(result.totalHops));
    } else {
        logMessage(QString("测试失败: %1 - %2").arg(result.targetName, result.errorMessage));
    }
}

void RouteTestTool::onAllTestsCompleted()
{
    m_testing = false;
    updateUI();
    
    logMessage("所有测试已完成");
    QMessageBox::information(this, "完成", 
                           QString("回程路由测试已完成\n共测试 %1 个节点")
                           .arg(m_testResults.size()));
}

void RouteTestTool::onProgressUpdated(int current, int total)
{
    m_currentTestIndex = current;
    m_progressBar->setValue(current);
    m_statusLabel->setText(QString("测试进度: %1/%2").arg(current).arg(total));
}

void RouteTestTool::onLogMessage(const QString &message)
{
    logMessage(message);
}
