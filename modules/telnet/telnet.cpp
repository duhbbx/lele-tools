#include "telnet.h"
#include <QDebug>
#include <QApplication>
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QScrollBar>
#include <QShortcut>
#include <QTextCursor>

REGISTER_DYNAMICOBJECT(Telnet);

Telnet::Telnet() : QWidget(nullptr), DynamicObjectBase(), 
    isConnected(false), bytesReceived(0), bytesSent(0), historyIndex(-1)
{
    setupUI();
    
    tcpSocket = new QTcpSocket(this);
    connect(tcpSocket, &QTcpSocket::connected, this, &Telnet::onSocketConnected);
    connect(tcpSocket, &QTcpSocket::disconnected, this, &Telnet::onSocketDisconnected);
    connect(tcpSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred), this, &Telnet::onSocketError);
    connect(tcpSocket, &QTcpSocket::readyRead, this, &Telnet::onSocketReadyRead);
    
    connectionTimer = new QTimer(this);
    connectionTimer->setSingleShot(true);
    connect(connectionTimer, &QTimer::timeout, this, &Telnet::onConnectionTimeout);
    
    connect(connectBtn, &QPushButton::clicked, this, &Telnet::onConnect);
    connect(disconnectBtn, &QPushButton::clicked, this, &Telnet::onDisconnect);
    connect(sendBtn, &QPushButton::clicked, this, &Telnet::onSendCommand);
    connect(clearBtn, &QPushButton::clicked, this, &Telnet::onClearOutput);
    connect(saveLogBtn, &QPushButton::clicked, this, &Telnet::onSaveLog);
    
    hostEdit->setText(tr("127.0.0.1"));
    portSpinBox->setValue(23);
    timeoutSpinBox->setValue(10);
    autoScrollCheck->setChecked(true);
    timestampCheck->setChecked(true);
    
    updateConnectionButtons(false);
    updateStatus(tr("就绪"), false);
}

Telnet::~Telnet()
{
    if (isConnected) {
        disconnectFromHost();
    }
}

void Telnet::setupUI()
{
    mainLayout = new QVBoxLayout(this);
    mainSplitter = new QSplitter(Qt::Vertical);
    
    setupConnectionArea();
    setupCommandArea();
    setupOutputArea();
    setupStatusArea();
    
    QWidget* topWidget = new QWidget();
    QHBoxLayout* topLayout = new QHBoxLayout(topWidget);
    topLayout->addWidget(connectionGroup);
    topLayout->addWidget(commandGroup);
    
    // 设置上部分的大小策略为固定高度
    topWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    
    mainSplitter->addWidget(topWidget);
    mainSplitter->addWidget(outputGroup);
    
    // 设置分割器的拉伸因子：上部分固定高度，下部分可伸缩
    mainSplitter->setStretchFactor(0, 0);  // 上部分不拉伸
    mainSplitter->setStretchFactor(1, 1);  // 下部分可拉伸
    
    mainLayout->addWidget(mainSplitter);
    mainLayout->addLayout(statusLayout);
    
    setStyleSheet(R"(
        QPushButton {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            background-color: #ffffff;
            border: 2px solid #e1e1e1;
            border-radius: 8px;
            padding: 8px 16px;
            font-weight: 600;
            font-size: 12pt;
            min-width: 90px;
            color: #333333;
        }
        QPushButton:hover {
            background-color: #f5f5f5;
            border-color: #d1d1d1;
        }
        QPushButton:pressed {
            background-color: #e8e8e8;
            border-color: #bfbfbf;
        }
        QGroupBox {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            font-weight: 600;
            font-size: 13pt;
            border: 2px solid #e1e1e1;
            border-radius: 12px;
            margin-top: 12px;
            padding-top: 15px;
            background-color: #fafafa;
            color: #333333;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 15px;
            padding: 0 8px 0 8px;
            color: #2c3e50;
        }
        QLineEdit {
            border: 2px solid #e1e1e1;
            border-radius: 8px;
            padding: 8px 12px;
            font-size: 12pt;
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            background-color: #ffffff;
            color: #333333;
        }
        QLineEdit:focus {
            border-color: #3498db;
            background-color: #fcfcfc;
        }
        QSpinBox {
            border: 2px solid #e1e1e1;
            border-radius: 8px;
            padding: 8px 12px;
            font-size: 12pt;
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            background-color: #ffffff;
            color: #333333;
            min-width: 80px;
        }
        QSpinBox:focus {
            border-color: #3498db;
            background-color: #fcfcfc;
        }
        QSpinBox::up-button {
            subcontrol-origin: border;
            subcontrol-position: top right;
            width: 20px;
            border-left: 1px solid #e1e1e1;
            border-bottom: 1px solid #e1e1e1;
            background-color: #f8f8f8;
            border-top-right-radius: 6px;
        }
        QSpinBox::down-button {
            subcontrol-origin: border;
            subcontrol-position: bottom right;
            width: 20px;
            border-left: 1px solid #e1e1e1;
            border-top: 1px solid #e1e1e1;
            background-color: #f8f8f8;
            border-bottom-right-radius: 6px;
        }
        QSpinBox::up-button:hover, QSpinBox::down-button:hover {
            background-color: #e8e8e8;
        }
        QSpinBox::up-arrow {
            image: none;
            border-left: 4px solid transparent;
            border-right: 4px solid transparent;
            border-bottom: 6px solid #666666;
            width: 0;
            height: 0;
        }
        QSpinBox::down-arrow {
            image: none;
            border-left: 4px solid transparent;
            border-right: 4px solid transparent;
            border-top: 6px solid #666666;
            width: 0;
            height: 0;
        }
        QTextEdit {
            border: 2px solid #e1e1e1;
            border-radius: 8px;
            padding: 12px;
            font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
            font-size: 11pt;
            background-color: #1e1e1e;
            color: #ffffff;
            selection-background-color: #3498db;
        }
        QTextEdit:focus {
            border-color: #3498db;
        }
        QCheckBox {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            font-size: 11pt;
            color: #333333;
            spacing: 8px;
        }
        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border: 2px solid #e1e1e1;
            border-radius: 4px;
            background-color: #ffffff;
        }
        QCheckBox::indicator:checked {
            background-color: #3498db;
            border-color: #3498db;
        }
        QCheckBox::indicator:checked {
            image: none;
            background-color: #3498db;
            border-color: #3498db;
        }
        QLabel {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            font-size: 11pt;
            color: #333333;
        }
    )");
}

void Telnet::setupConnectionArea()
{
    connectionGroup = new QGroupBox(tr("🌐 连接设置"));
    connectionGroup->setFixedWidth(300);
    connectionLayout = new QGridLayout(connectionGroup);
    
    hostLabel = new QLabel(tr("主机地址:"));
    hostEdit = new QLineEdit();
    hostEdit->setPlaceholderText(tr("输入IP地址或域名"));
    
    portLabel = new QLabel(tr("端口:"));
    portSpinBox = new QSpinBox();
    portSpinBox->setRange(1, 65535);
    
    timeoutLabel = new QLabel(tr("超时(秒):"));
    timeoutSpinBox = new QSpinBox();
    timeoutSpinBox->setRange(1, 60);
    
    autoReconnectCheck = new QCheckBox(tr("自动重连"));
    
    connectionButtonLayout = new QHBoxLayout();
    connectBtn = new QPushButton(tr("🔗 连接"));
    connectBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #27ae60;
            color: white;
            border: none;
            border-radius: 8px;
            padding: 10px 20px;
            font-weight: 600;
            font-size: 12pt;
        }
        QPushButton:hover {
            background-color: #2ecc71;
        }
        QPushButton:pressed {
            background-color: #219a52;
        }
        QPushButton:disabled {
            background-color: #95a5a6;
            color: #ecf0f1;
        }
    )");

    disconnectBtn = new QPushButton(tr("❌ 断开"));
    disconnectBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #e74c3c;
            color: white;
            border: none;
            border-radius: 8px;
            padding: 10px 20px;
            font-weight: 600;
            font-size: 12pt;
        }
        QPushButton:hover {
            background-color: #ec7063;
        }
        QPushButton:pressed {
            background-color: #c0392b;
        }
        QPushButton:disabled {
            background-color: #95a5a6;
            color: #ecf0f1;
        }
    )");
    
    connectionButtonLayout->addWidget(connectBtn);
    connectionButtonLayout->addWidget(disconnectBtn);
    
    connectionLayout->addWidget(hostLabel, 0, 0);
    connectionLayout->addWidget(hostEdit, 0, 1);
    connectionLayout->addWidget(portLabel, 1, 0);
    connectionLayout->addWidget(portSpinBox, 1, 1);
    connectionLayout->addWidget(timeoutLabel, 2, 0);
    connectionLayout->addWidget(timeoutSpinBox, 2, 1);
    connectionLayout->addWidget(autoReconnectCheck, 3, 0, 1, 2);
    connectionLayout->addLayout(connectionButtonLayout, 4, 0, 1, 2);
}

void Telnet::setupCommandArea()
{
    commandGroup = new QGroupBox(tr("⌨️ 命令输入"));
    commandLayout = new QVBoxLayout(commandGroup);

    // 创建命令输入标签
    QLabel* commandLabel = new QLabel(tr("输入命令 (支持多行):"));
    commandLabel->setStyleSheet("font-weight: 600; color: #2c3e50; margin-bottom: 4px;");

    // 多行命令输入框
    commandEdit = new QTextEdit();
    commandEdit->setPlaceholderText(tr("输入Telnet命令...\n支持多行输入，Ctrl+Enter发送"));
    commandEdit->setMaximumHeight(120); // 限制高度
    commandEdit->setStyleSheet(R"(
        QTextEdit {
            background-color: #2c3e50;
            color: #ecf0f1;
            border: 2px solid #34495e;
            border-radius: 8px;
            padding: 10px;
            font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
            font-size: 12pt;
            line-height: 1.4;
        }
        QTextEdit:focus {
            border-color: #3498db;
            background-color: #34495e;
        }
    )");

    // 按钮布局
    commandInputLayout = new QHBoxLayout();

    // 发送按钮（发送全部）
    sendBtn = new QPushButton(tr("📤 发送全部"));
    sendBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #27ae60;
            color: white;
            border: none;
            border-radius: 8px;
            padding: 10px 20px;
            font-weight: 600;
            font-size: 12pt;
        }
        QPushButton:hover {
            background-color: #2ecc71;
        }
        QPushButton:pressed {
            background-color: #219a52;
        }
    )");

    // 发送选中按钮
    QPushButton* sendSelectedBtn = new QPushButton(tr("📝 发送选中"));
    sendSelectedBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #3498db;
            color: white;
            border: none;
            border-radius: 8px;
            padding: 10px 20px;
            font-weight: 600;
            font-size: 12pt;
        }
        QPushButton:hover {
            background-color: #5dade2;
        }
        QPushButton:pressed {
            background-color: #2980b9;
        }
    )");

    commandInputLayout->addWidget(sendBtn);
    commandInputLayout->addWidget(sendSelectedBtn);
    commandInputLayout->addStretch();

    commandButtonLayout = new QHBoxLayout();
    clearBtn = new QPushButton(tr("🗑️ 清空"));
    clearBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #e74c3c;
            color: white;
            border: none;
            border-radius: 8px;
            padding: 8px 16px;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: #ec7063;
        }
        QPushButton:pressed {
            background-color: #c0392b;
        }
    )");

    saveLogBtn = new QPushButton(tr("💾 保存日志"));
    saveLogBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #9b59b6;
            color: white;
            border: none;
            border-radius: 8px;
            padding: 8px 16px;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: #af7ac5;
        }
        QPushButton:pressed {
            background-color: #8e44ad;
        }
    )");

    commandButtonLayout->addWidget(clearBtn);
    commandButtonLayout->addWidget(saveLogBtn);
    commandButtonLayout->addStretch();

    // 添加到布局
    commandLayout->addWidget(commandLabel);
    commandLayout->addWidget(commandEdit);
    commandLayout->addLayout(commandInputLayout);
    commandLayout->addLayout(commandButtonLayout);

    // 连接信号
    connect(sendSelectedBtn, &QPushButton::clicked, this, &Telnet::onSendSelectedCommand);

    // 添加快捷键支持
    QShortcut* sendShortcut = new QShortcut(QKeySequence("Ctrl+Return"), commandEdit);
    connect(sendShortcut, &QShortcut::activated, this, &Telnet::onSendCommand);
}

void Telnet::setupOutputArea()
{
    outputGroup = new QGroupBox(tr("📺 输出显示"));
    outputLayout = new QVBoxLayout(outputGroup);
    
    outputTextEdit = new QTextEdit();
    outputTextEdit->setReadOnly(true);
    
    QHBoxLayout* optionsLayout = new QHBoxLayout();
    autoScrollCheck = new QCheckBox(tr("自动滚动"));
    timestampCheck = new QCheckBox(tr("显示时间戳"));
    wordWrapCheck = new QCheckBox(tr("自动换行"));
    
    optionsLayout->addWidget(autoScrollCheck);
    optionsLayout->addWidget(timestampCheck);
    optionsLayout->addWidget(wordWrapCheck);
    optionsLayout->addStretch();
    
    outputLayout->addWidget(outputTextEdit);
    outputLayout->addLayout(optionsLayout);
}

void Telnet::setupStatusArea()
{
    statusLayout = new QHBoxLayout();

    statusLabel = new QLabel(tr("就绪"));
    statusLabel->setStyleSheet(R"(
        color: #2c3e50;
        font-weight: 600;
        padding: 6px 12px;
        background: #ecf0f1;
        border-radius: 6px;
        border: 1px solid #bdc3c7;
    )");

    connectionStatusLabel = new QLabel(tr("未连接"));
    connectionStatusLabel->setStyleSheet(R"(
        color: #e74c3c;
        font-weight: 600;
        padding: 6px 12px;
        background: #fadbd8;
        border-radius: 6px;
        border: 1px solid #f1948a;
    )");

    bytesLabel = new QLabel(tr("发送: 0B | 接收: 0B"));
    bytesLabel->setStyleSheet(R"(
        color: #7f8c8d;
        font-size: 11pt;
        font-weight: 500;
        padding: 6px 12px;
        background: #f8f9fa;
        border-radius: 6px;
        border: 1px solid #e9ecef;
    )");

    statusLayout->addWidget(statusLabel);
    statusLayout->addWidget(connectionStatusLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(bytesLabel);
}

void Telnet::onConnect()
{
    currentHost = hostEdit->text().trimmed();
    currentPort = portSpinBox->value();
    
    if (currentHost.isEmpty()) {
        QMessageBox::warning(this, tr("错误"), tr("请输入主机地址"));
        return;
    }
    
    connectToHost();
}

void Telnet::onDisconnect()
{
    disconnectFromHost();
}

void Telnet::onSendCommand()
{
    QString command = commandEdit->toPlainText().trimmed();
    if (command.isEmpty() || !isConnected) return;

    // 按行分割命令并发送
    QStringList lines = command.split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        if (!line.trimmed().isEmpty()) {
            sendCommand(line.trimmed());
            addToHistory(line.trimmed());
        }
    }

    commandEdit->clear();
}

void Telnet::onSendSelectedCommand()
{
    if (!isConnected) return;

    QTextCursor cursor = commandEdit->textCursor();
    QString selectedText = cursor.selectedText().trimmed();

    if (selectedText.isEmpty()) {
        // 如果没有选中文本，发送当前行
        cursor.select(QTextCursor::LineUnderCursor);
        selectedText = cursor.selectedText().trimmed();
    }

    if (!selectedText.isEmpty()) {
        // 按行分割并发送
        QStringList lines = selectedText.split('\n', Qt::SkipEmptyParts);
        for (const QString& line : lines) {
            if (!line.trimmed().isEmpty()) {
                sendCommand(line.trimmed());
                addToHistory(line.trimmed());
            }
        }
    }
}

void Telnet::onClearOutput()
{
    outputTextEdit->clear();
    bytesReceived = 0;
    bytesSent = 0;
    updateStatus(tr("输出已清空"), false);
    bytesLabel->setText(tr("发送: 0B | 接收: 0B"));
}

void Telnet::onSaveLog()
{
    QString fileName = QString("telnet_log_%1.txt").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    QString filePath = QFileDialog::getSaveFileName(this, tr("保存Telnet日志"), fileName, tr("文本文件 (*.txt)"));
    
    if (!filePath.isEmpty()) {
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << outputTextEdit->toPlainText();
            updateStatus(tr("日志已保存"), false);
        }
    }
}

void Telnet::onSocketConnected()
{
    isConnected = true;
    connectionTimer->stop();
    updateConnectionButtons(true);
    connectionStatusLabel->setText(tr("已连接"));
    connectionStatusLabel->setStyleSheet(R"(
        color: #27ae60;
        font-weight: 600;
        padding: 6px 12px;
        background: #d5f4e6;
        border-radius: 6px;
        border: 1px solid #82e0aa;
    )");
    
    QString message = tr("成功连接到 %1:%2").arg(currentHost).arg(currentPort);
    appendOutput(message, "#4CAF50");
    updateStatus(message, false);
}

void Telnet::onSocketDisconnected()
{
    isConnected = false;
    updateConnectionButtons(false);
    connectionStatusLabel->setText(tr("未连接"));
    connectionStatusLabel->setStyleSheet(R"(
        color: #e74c3c;
        font-weight: 600;
        padding: 6px 12px;
        background: #fadbd8;
        border-radius: 6px;
        border: 1px solid #f1948a;
    )");
    
    QString message = tr("连接已断开");
    appendOutput(message, "#f44336");
    updateStatus(message, false);
}

void Telnet::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    isConnected = false;
    connectionTimer->stop();
    updateConnectionButtons(false);
    
    QString message = tr("连接错误: %1").arg(tcpSocket->errorString());
    appendOutput(message, "#f44336");
    updateStatus(message, true);
}

void Telnet::onSocketReadyRead()
{
    QByteArray data = tcpSocket->readAll();
    bytesReceived += data.size();
    
    QString text = QString::fromUtf8(data);
    appendOutput(text, "#ffffff");
    
    bytesLabel->setText(tr("发送: %1B | 接收: %2B").arg(bytesSent).arg(bytesReceived));
}

void Telnet::onConnectionTimeout()
{
    if (tcpSocket->state() == QAbstractSocket::ConnectingState) {
        tcpSocket->abort();
        updateStatus(tr("连接超时"), true);
    }
}

void Telnet::connectToHost()
{
    updateStatus(tr("正在连接到 %1:%2...").arg(currentHost).arg(currentPort), false);
    tcpSocket->connectToHost(currentHost, currentPort);
    connectionTimer->start(timeoutSpinBox->value() * 1000);
    updateConnectionButtons(false);
}

void Telnet::disconnectFromHost()
{
    tcpSocket->disconnectFromHost();
}

void Telnet::sendCommand(const QString& command)
{
    if (!isConnected) return;
    
    QByteArray data = (command + "\r\n").toUtf8();
    qint64 written = tcpSocket->write(data);
    bytesSent += written;
    
    appendOutput(QString("> %1").arg(command), "#00ff00");
    bytesLabel->setText(tr("发送: %1B | 接收: %2B").arg(bytesSent).arg(bytesReceived));
}

void Telnet::appendOutput(const QString& text, const QString& color)
{
    QString timestamp = timestampCheck->isChecked() ? formatTimestamp() + " " : "";
    QString formattedText = QString("<span style='color: %1;'>%2%3</span>").arg(color, timestamp, text.toHtmlEscaped());
    
    outputTextEdit->append(formattedText);
    
    if (autoScrollCheck->isChecked()) {
        outputTextEdit->moveCursor(QTextCursor::End);
    }
}

void Telnet::updateStatus(const QString& message, bool isError)
{
    statusLabel->setText(message);
    if (isError) {
        statusLabel->setStyleSheet(R"(
            color: #e74c3c;
            font-weight: 600;
            padding: 6px 12px;
            background: #fadbd8;
            border-radius: 6px;
            border: 1px solid #f1948a;
        )");
    } else {
        statusLabel->setStyleSheet(R"(
            color: #27ae60;
            font-weight: 600;
            padding: 6px 12px;
            background: #d5f4e6;
            border-radius: 6px;
            border: 1px solid #82e0aa;
        )");
    }
}

void Telnet::addToHistory(const QString& command)
{
    if (!command.isEmpty() && (commandHistory.isEmpty() || commandHistory.last() != command)) {
        commandHistory.append(command);
        if (commandHistory.size() > 50) commandHistory.removeFirst();
    }
    historyIndex = -1;
}

void Telnet::updateConnectionButtons(bool connected)
{
    connectBtn->setEnabled(!connected);
    disconnectBtn->setEnabled(connected);
    sendBtn->setEnabled(connected);
    commandEdit->setEnabled(connected);
}

QString Telnet::formatTimestamp()
{
    return QDateTime::currentDateTime().toString("[hh:mm:ss]");
}

// 空实现避免编译错误
void Telnet::onHostChanged() {}
void Telnet::onPortChanged() {}
void Telnet::onCommandHistoryUp() {}
void Telnet::onCommandHistoryDown() {}