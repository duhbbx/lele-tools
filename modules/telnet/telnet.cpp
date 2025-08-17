#include "telnet.h"
#include <QDebug>
#include <QApplication>
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QScrollBar>

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
    connect(commandEdit, &QLineEdit::returnPressed, this, &Telnet::onSendCommand);
    
    hostEdit->setText("127.0.0.1");
    portSpinBox->setValue(23);
    timeoutSpinBox->setValue(10);
    autoScrollCheck->setChecked(true);
    timestampCheck->setChecked(true);
    
    updateConnectionButtons(false);
    updateStatus("就绪", false);
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
    
    mainSplitter->addWidget(topWidget);
    mainSplitter->addWidget(outputGroup);
    
    mainLayout->addWidget(mainSplitter);
    mainLayout->addLayout(statusLayout);
    
    setStyleSheet(R"(
        QPushButton {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            background-color: #f0f0f0;
            border: 1px solid #cccccc;
            border-radius: 0px;
            padding: 6px 12px;
            font-weight: bold;
            font-size: 11pt;
            min-width: 80px;
        }
        QPushButton:hover { background-color: #e0e0e0; }
        QGroupBox {
            font-weight: bold;
            border: 2px solid #cccccc;
            border-radius: 0px;
            margin-top: 1ex;
            padding-top: 10px;
        }
        QLineEdit, QSpinBox, QComboBox {
            border: 2px solid #dddddd;
            border-radius: 0px;
            padding: 4px 8px;
            font-size: 11pt;
        }
        QTextEdit {
            border: 2px solid #dddddd;
            border-radius: 0px;
            padding: 8px;
            font-family: 'Consolas', monospace;
            font-size: 10pt;
            background-color: #1e1e1e;
            color: #ffffff;
        }
    )");
}

void Telnet::setupConnectionArea()
{
    connectionGroup = new QGroupBox("🌐 连接设置");
    connectionGroup->setFixedWidth(300);
    connectionLayout = new QGridLayout(connectionGroup);
    
    hostLabel = new QLabel("主机地址:");
    hostEdit = new QLineEdit();
    hostEdit->setPlaceholderText("输入IP地址或域名");
    
    portLabel = new QLabel("端口:");
    portSpinBox = new QSpinBox();
    portSpinBox->setRange(1, 65535);
    
    timeoutLabel = new QLabel("超时(秒):");
    timeoutSpinBox = new QSpinBox();
    timeoutSpinBox->setRange(1, 60);
    
    autoReconnectCheck = new QCheckBox("自动重连");
    
    connectionButtonLayout = new QHBoxLayout();
    connectBtn = new QPushButton("🔗 连接");
    connectBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-family: 'Microsoft YaHei'; }");
    
    disconnectBtn = new QPushButton("❌ 断开");
    disconnectBtn->setStyleSheet("QPushButton { background-color: #f44336; color: white; font-family: 'Microsoft YaHei'; }");
    
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
    commandGroup = new QGroupBox("⌨️ 命令输入");
    commandLayout = new QVBoxLayout(commandGroup);
    
    commonCommandsCombo = new QComboBox();
    commonCommandsCombo->addItems(QStringList() 
        << "选择常用命令..." << "help" << "ls -la" << "pwd" << "whoami" << "exit");
    
    commandInputLayout = new QHBoxLayout();
    commandEdit = new QLineEdit();
    commandEdit->setPlaceholderText("输入Telnet命令...");
    sendBtn = new QPushButton("📤 发送");
    sendBtn->setStyleSheet("font-family: 'Microsoft YaHei';");
    
    commandInputLayout->addWidget(commandEdit);
    commandInputLayout->addWidget(sendBtn);
    
    commandButtonLayout = new QHBoxLayout();
    clearBtn = new QPushButton("🗑️ 清空");
    clearBtn->setStyleSheet("font-family: 'Microsoft YaHei';");
    saveLogBtn = new QPushButton("💾 保存日志");
    saveLogBtn->setStyleSheet("font-family: 'Microsoft YaHei';");
    
    commandButtonLayout->addWidget(clearBtn);
    commandButtonLayout->addWidget(saveLogBtn);
    commandButtonLayout->addStretch();
    
    commandLayout->addWidget(commonCommandsCombo);
    commandLayout->addLayout(commandInputLayout);
    commandLayout->addLayout(commandButtonLayout);
}

void Telnet::setupOutputArea()
{
    outputGroup = new QGroupBox("📺 输出显示");
    outputLayout = new QVBoxLayout(outputGroup);
    
    outputTextEdit = new QTextEdit();
    outputTextEdit->setReadOnly(true);
    
    QHBoxLayout* optionsLayout = new QHBoxLayout();
    autoScrollCheck = new QCheckBox("自动滚动");
    timestampCheck = new QCheckBox("显示时间戳");
    wordWrapCheck = new QCheckBox("自动换行");
    
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
    
    statusLabel = new QLabel("就绪");
    statusLabel->setStyleSheet("color: #666; font-weight: bold; padding: 4px 8px; background: #f9f9f9; border-radius: 0px;");
    
    connectionStatusLabel = new QLabel("未连接");
    connectionStatusLabel->setStyleSheet("color: #f44336; font-weight: bold;");
    
    bytesLabel = new QLabel("发送: 0B | 接收: 0B");
    bytesLabel->setStyleSheet("color: #666; font-size: 10pt;");
    
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
        QMessageBox::warning(this, "错误", "请输入主机地址");
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
    QString command = commandEdit->text();
    if (command.isEmpty() || !isConnected) return;
    
    sendCommand(command);
    addToHistory(command);
    commandEdit->clear();
}

void Telnet::onClearOutput()
{
    outputTextEdit->clear();
    bytesReceived = 0;
    bytesSent = 0;
    updateStatus("输出已清空", false);
    bytesLabel->setText("发送: 0B | 接收: 0B");
}

void Telnet::onSaveLog()
{
    QString fileName = QString("telnet_log_%1.txt").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    QString filePath = QFileDialog::getSaveFileName(this, "保存Telnet日志", fileName, "文本文件 (*.txt)");
    
    if (!filePath.isEmpty()) {
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << outputTextEdit->toPlainText();
            updateStatus("日志已保存", false);
        }
    }
}

void Telnet::onSocketConnected()
{
    isConnected = true;
    connectionTimer->stop();
    updateConnectionButtons(true);
    connectionStatusLabel->setText("已连接");
    connectionStatusLabel->setStyleSheet("color: #4CAF50; font-weight: bold;");
    
    QString message = QString("成功连接到 %1:%2").arg(currentHost).arg(currentPort);
    appendOutput(message, "#4CAF50");
    updateStatus(message, false);
}

void Telnet::onSocketDisconnected()
{
    isConnected = false;
    updateConnectionButtons(false);
    connectionStatusLabel->setText("未连接");
    connectionStatusLabel->setStyleSheet("color: #f44336; font-weight: bold;");
    
    QString message = QString("连接已断开");
    appendOutput(message, "#f44336");
    updateStatus(message, false);
}

void Telnet::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    isConnected = false;
    connectionTimer->stop();
    updateConnectionButtons(false);
    
    QString message = QString("连接错误: %1").arg(tcpSocket->errorString());
    appendOutput(message, "#f44336");
    updateStatus(message, true);
}

void Telnet::onSocketReadyRead()
{
    QByteArray data = tcpSocket->readAll();
    bytesReceived += data.size();
    
    QString text = QString::fromUtf8(data);
    appendOutput(text, "#ffffff");
    
    bytesLabel->setText(QString("发送: %1B | 接收: %2B").arg(bytesSent).arg(bytesReceived));
}

void Telnet::onConnectionTimeout()
{
    if (tcpSocket->state() == QAbstractSocket::ConnectingState) {
        tcpSocket->abort();
        updateStatus("连接超时", true);
    }
}

void Telnet::connectToHost()
{
    updateStatus(QString("正在连接到 %1:%2...").arg(currentHost).arg(currentPort), false);
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
    bytesLabel->setText(QString("发送: %1B | 接收: %2B").arg(bytesSent).arg(bytesReceived));
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
    statusLabel->setStyleSheet(isError ? "color: red; font-weight: bold;" : "color: green; font-weight: bold;");
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