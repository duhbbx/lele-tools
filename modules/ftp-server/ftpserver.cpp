#include "ftpserver.h"

REGISTER_DYNAMICOBJECT(FTPServer);

#include <QApplication>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDesktopServices>
#include <QMimeDatabase>
#include <QTextStream>

// ========== FTPServer Implementation ==========

FTPServer::FTPServer(QWidget* parent)
    : QWidget(parent), DynamicObjectBase()
    , m_mainSplitter(nullptr)
    , m_leftTabs(nullptr)
    , m_rightTabs(nullptr)
    , m_configWidget(nullptr)
    , m_userManager(nullptr)
    , m_statusWidget(nullptr)
    , m_connectionMonitor(nullptr)
    , m_logTextEdit(nullptr)
    , m_menuBar(nullptr)
    , m_statusBar(nullptr)
    , m_serverCore(nullptr)
    , m_isServerRunning(false)
    , m_activeConnections(0)
    , m_totalConnections(0)
{
    setWindowTitle(tr("FTP服务器"));
    setMinimumSize(1200, 800);

    // 初始化配置
    m_config.rootDirectory = QDir::homePath() + "/FTPServer";

    setupUI();
    setupMenuBar();
    loadSettings();

    // 创建服务器核心
    m_serverCore = new FTPServerCore(this);
    m_userManager->setServerCore(m_serverCore);
    m_connectionMonitor->setServerCore(m_serverCore);

    // 连接信号
    connect(m_serverCore, &FTPServerCore::serverStarted,
            this, &FTPServer::onServerStarted);
    connect(m_serverCore, &FTPServerCore::serverStopped,
            this, &FTPServer::onServerStopped);
    connect(m_serverCore, &FTPServerCore::serverError,
            this, &FTPServer::onServerError);
    connect(m_serverCore, &FTPServerCore::newConnection,
            this, &FTPServer::onNewConnection);
    connect(m_serverCore, &FTPServerCore::connectionClosed,
            this, &FTPServer::onConnectionClosed);
    connect(m_serverCore, &FTPServerCore::logMessage,
            this, [this](const QString& message) {
                QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
                m_logTextEdit->append(QString("[%1] %2").arg(timestamp, message));
            });

    updateServerStatus();
}

FTPServer::~FTPServer()
{
    if (m_serverCore && m_serverCore->isRunning()) {
        m_serverCore->stopServer();
    }
    saveSettings();
}

void FTPServer::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 创建菜单栏 - 固定高度，不拉伸
    m_menuBar = new QMenuBar(this);
    m_menuBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    mainLayout->addWidget(m_menuBar, 0);

    // 创建主分割器 - 占据所有剩余空间
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainLayout->addWidget(m_mainSplitter, 1); // 拉伸因子为1，占据所有剩余高度

    // 左侧：配置和用户管理
    m_leftTabs = new QTabWidget(this);
    m_leftTabs->setMinimumWidth(400);
    m_leftTabs->setMaximumWidth(500);

    // 服务器配置标签页
    setupConfigWidget();
    m_leftTabs->addTab(m_configWidget, tr("服务器配置"));

    // 用户管理标签页
    m_userManager = new FTPUserManager(this);
    m_leftTabs->addTab(m_userManager, tr("用户管理"));

    m_mainSplitter->addWidget(m_leftTabs);

    // 右侧：状态和监控
    m_rightTabs = new QTabWidget(this);

    // 服务器状态标签页
    setupStatusWidget();
    m_rightTabs->addTab(m_statusWidget, tr("服务器状态"));

    // 连接监控标签页
    m_connectionMonitor = new FTPConnectionMonitor(this);
    m_rightTabs->addTab(m_connectionMonitor, tr("连接监控"));

    // 日志标签页
    m_logTextEdit = new QTextEdit(this);
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setFont(QFont("Consolas", 10));
    m_rightTabs->addTab(m_logTextEdit, tr("服务器日志"));

    m_mainSplitter->addWidget(m_rightTabs);

    // 设置分割器比例
    m_mainSplitter->setSizes({400, 800});

    // 创建状态栏 - 固定高度，不拉伸
    m_statusBar = new QStatusBar(this);
    m_statusBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    mainLayout->addWidget(m_statusBar, 0);
}

void FTPServer::setupConfigWidget()
{
    m_configWidget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(m_configWidget);

    // 基本配置组
    QGroupBox* basicGroup = new QGroupBox(tr("基本配置"));
    QFormLayout* basicLayout = new QFormLayout(basicGroup);

    m_portSpinBox = new QSpinBox();
    m_portSpinBox->setRange(1, 65535);
    m_portSpinBox->setValue(21);
    basicLayout->addRow(tr("端口:"), m_portSpinBox);

    QHBoxLayout* rootDirLayout = new QHBoxLayout();
    m_rootDirEdit = new QLineEdit();
    m_browseDirButton = new QPushButton(tr("浏览..."));
    rootDirLayout->addWidget(m_rootDirEdit);
    rootDirLayout->addWidget(m_browseDirButton);
    basicLayout->addRow(tr("根目录:"), rootDirLayout);

    m_serverNameEdit = new QLineEdit();
    m_serverNameEdit->setText("LeLe FTP Server");
    basicLayout->addRow(tr("服务器名称:"), m_serverNameEdit);

    m_maxConnectionsSpinBox = new QSpinBox();
    m_maxConnectionsSpinBox->setRange(1, 1000);
    m_maxConnectionsSpinBox->setValue(10);
    basicLayout->addRow(tr("最大连接数:"), m_maxConnectionsSpinBox);

    layout->addWidget(basicGroup);

    // 高级配置组
    QGroupBox* advancedGroup = new QGroupBox(tr("高级配置"));
    QVBoxLayout* advancedLayout = new QVBoxLayout(advancedGroup);

    m_allowAnonymousCheckBox = new QCheckBox(tr("允许匿名访问"));
    advancedLayout->addWidget(m_allowAnonymousCheckBox);

    m_usePassiveModeCheckBox = new QCheckBox(tr("使用被动模式"));
    m_usePassiveModeCheckBox->setChecked(true);
    advancedLayout->addWidget(m_usePassiveModeCheckBox);

    QHBoxLayout* passivePortLayout = new QHBoxLayout();
    m_passiveMinPortSpinBox = new QSpinBox();
    m_passiveMinPortSpinBox->setRange(1024, 65535);
    m_passiveMinPortSpinBox->setValue(50000);
    m_passiveMaxPortSpinBox = new QSpinBox();
    m_passiveMaxPortSpinBox->setRange(1024, 65535);
    m_passiveMaxPortSpinBox->setValue(50100);
    passivePortLayout->addWidget(new QLabel(tr("被动端口范围:")));
    passivePortLayout->addWidget(m_passiveMinPortSpinBox);
    passivePortLayout->addWidget(new QLabel(tr("-")));
    passivePortLayout->addWidget(m_passiveMaxPortSpinBox);
    passivePortLayout->addStretch();
    advancedLayout->addLayout(passivePortLayout);

    layout->addWidget(advancedGroup);

    // 服务器控制组
    QGroupBox* controlGroup = new QGroupBox(tr("服务器控制"));
    QVBoxLayout* controlLayout = new QVBoxLayout(controlGroup);

    m_startStopButton = new QPushButton(tr("启动服务器"));
    m_startStopButton->setMinimumHeight(40);
    controlLayout->addWidget(m_startStopButton);

    layout->addWidget(controlGroup);

    layout->addStretch();

    // 连接信号
    connect(m_browseDirButton, &QPushButton::clicked, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, tr("选择FTP根目录"),
                                                       m_rootDirEdit->text());
        if (!dir.isEmpty()) {
            m_rootDirEdit->setText(dir);
            onConfigurationChanged();
        }
    });

    connect(m_startStopButton, &QPushButton::clicked, [this]() {
        if (m_isServerRunning) {
            onStopServer();
        } else {
            onStartServer();
        }
    });

    // 配置改变信号
    connect(m_portSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &FTPServer::onConfigurationChanged);
    connect(m_rootDirEdit, &QLineEdit::textChanged,
            this, &FTPServer::onConfigurationChanged);
    connect(m_serverNameEdit, &QLineEdit::textChanged,
            this, &FTPServer::onConfigurationChanged);
    connect(m_maxConnectionsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &FTPServer::onConfigurationChanged);
    connect(m_allowAnonymousCheckBox, &QCheckBox::toggled,
            this, &FTPServer::onConfigurationChanged);
    connect(m_usePassiveModeCheckBox, &QCheckBox::toggled,
            this, &FTPServer::onConfigurationChanged);
}

void FTPServer::setupStatusWidget()
{
    m_statusWidget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(m_statusWidget);

    // 服务器状态组
    QGroupBox* statusGroup = new QGroupBox(tr("服务器状态"));
    QFormLayout* statusLayout = new QFormLayout(statusGroup);

    m_serverStatusLabel = new QLabel(tr("已停止"));
    m_serverStatusLabel->setStyleSheet("color: red; font-weight: bold;");
    statusLayout->addRow(tr("状态:"), m_serverStatusLabel);

    m_serverAddressLabel = new QLabel(tr("未启动"));
    statusLayout->addRow(tr("地址:"), m_serverAddressLabel);

    m_activeConnectionsLabel = new QLabel("0");
    statusLayout->addRow(tr("活动连接:"), m_activeConnectionsLabel);

    m_totalConnectionsLabel = new QLabel("0");
    statusLayout->addRow(tr("总连接数:"), m_totalConnectionsLabel);

    layout->addWidget(statusGroup);

    // 网络接口信息
    QGroupBox* networkGroup = new QGroupBox(tr("网络接口"));
    QVBoxLayout* networkLayout = new QVBoxLayout(networkGroup);

    QTextEdit* networkInfo = new QTextEdit();
    networkInfo->setReadOnly(true);
    networkInfo->setMaximumHeight(150);

    // 获取网络接口信息
    QString networkText;
    QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
    for (const QHostAddress& address : addresses) {
        if (address != QHostAddress::LocalHost &&
            address.protocol() == QAbstractSocket::IPv4Protocol) {
            networkText += QString("IPv4: %1\n").arg(address.toString());
        }
    }
    networkInfo->setText(networkText);
    networkLayout->addWidget(networkInfo);

    layout->addWidget(networkGroup);

    layout->addStretch();
}

void FTPServer::setupMenuBar()
{
    // 服务器菜单
    QMenu* serverMenu = m_menuBar->addMenu(tr("服务器(&S)"));

    QAction* startAction = serverMenu->addAction(tr("启动服务器(&S)"));
    startAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
    connect(startAction, &QAction::triggered, this, &FTPServer::onStartServer);

    QAction* stopAction = serverMenu->addAction(tr("停止服务器(&T)"));
    stopAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(stopAction, &QAction::triggered, this, &FTPServer::onStopServer);

    serverMenu->addSeparator();

    QAction* exitAction = serverMenu->addAction(tr("退出(&X)"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    // 用户菜单
    QMenu* userMenu = m_menuBar->addMenu(tr("用户(&U)"));
    QAction* addUserAction = userMenu->addAction(tr("添加用户(&A)"));
    connect(addUserAction, &QAction::triggered, m_userManager, &FTPUserManager::onAddUser);

    // 帮助菜单
    QMenu* helpMenu = m_menuBar->addMenu(tr("帮助(&H)"));
    QAction* aboutAction = helpMenu->addAction(tr("关于"));
    connect(aboutAction, &QAction::triggered, [this]() {
        QMessageBox::about(this, tr("关于FTP服务器"),
                          tr("FTP服务器工具\\n\\n"
                             "提供完整的FTP服务器功能\\n"
                             "支持多用户、权限管理和实时监控"));
    });
}

void FTPServer::loadSettings()
{
    QSettings settings;
    settings.beginGroup("FTPServer");

    // 恢复窗口几何信息
    restoreGeometry(settings.value("geometry").toByteArray());

    // 恢复分割器状态
    m_mainSplitter->restoreState(settings.value("splitterState").toByteArray());

    // 恢复配置
    m_config.port = settings.value("port", 21).toInt();
    m_config.rootDirectory = settings.value("rootDirectory", QDir::homePath() + "/FTPServer").toString();
    m_config.serverName = settings.value("serverName", "LeLe FTP Server").toString();
    m_config.maxConnections = settings.value("maxConnections", 10).toInt();
    m_config.allowAnonymous = settings.value("allowAnonymous", false).toBool();
    m_config.usePassiveMode = settings.value("usePassiveMode", true).toBool();
    m_config.passivePortMin = settings.value("passivePortMin", 50000).toInt();
    m_config.passivePortMax = settings.value("passivePortMax", 50100).toInt();

    // 更新UI
    m_portSpinBox->setValue(m_config.port);
    m_rootDirEdit->setText(m_config.rootDirectory);
    m_serverNameEdit->setText(m_config.serverName);
    m_maxConnectionsSpinBox->setValue(m_config.maxConnections);
    m_allowAnonymousCheckBox->setChecked(m_config.allowAnonymous);
    m_usePassiveModeCheckBox->setChecked(m_config.usePassiveMode);
    m_passiveMinPortSpinBox->setValue(m_config.passivePortMin);
    m_passiveMaxPortSpinBox->setValue(m_config.passivePortMax);

    settings.endGroup();
}

void FTPServer::saveSettings()
{
    QSettings settings;
    settings.beginGroup("FTPServer");

    // 保存窗口几何信息
    settings.setValue("geometry", saveGeometry());

    // 保存分割器状态
    settings.setValue("splitterState", m_mainSplitter->saveState());

    // 保存配置
    settings.setValue("port", m_config.port);
    settings.setValue("rootDirectory", m_config.rootDirectory);
    settings.setValue("serverName", m_config.serverName);
    settings.setValue("maxConnections", m_config.maxConnections);
    settings.setValue("allowAnonymous", m_config.allowAnonymous);
    settings.setValue("usePassiveMode", m_config.usePassiveMode);
    settings.setValue("passivePortMin", m_config.passivePortMin);
    settings.setValue("passivePortMax", m_config.passivePortMax);

    settings.endGroup();
}

void FTPServer::closeEvent(QCloseEvent* event)
{
    if (m_isServerRunning) {
        int ret = QMessageBox::question(this, tr("确认退出"),
                                      tr("服务器正在运行，确定要退出吗？"),
                                      QMessageBox::Yes | QMessageBox::No);
        if (ret != QMessageBox::Yes) {
            event->ignore();
            return;
        }
        m_serverCore->stopServer();
    }
    saveSettings();
    event->accept();
}

void FTPServer::onStartServer()
{
    // 更新配置
    onConfigurationChanged();

    // 检查根目录
    QDir rootDir(m_config.rootDirectory);
    if (!rootDir.exists()) {
        int ret = QMessageBox::question(this, tr("目录不存在"),
                                      tr("根目录不存在，是否创建？\\n%1").arg(m_config.rootDirectory),
                                      QMessageBox::Yes | QMessageBox::No);
        if (ret == QMessageBox::Yes) {
            if (!rootDir.mkpath(m_config.rootDirectory)) {
                QMessageBox::critical(this, tr("错误"), tr("无法创建根目录"));
                return;
            }
        } else {
            return;
        }
    }

    // 启动服务器
    if (m_serverCore->startServer(m_config)) {
        m_logTextEdit->append(tr("[系统] 正在启动FTP服务器..."));
    } else {
        QMessageBox::critical(this, tr("启动失败"), tr("无法启动FTP服务器"));
    }
}

void FTPServer::onStopServer()
{
    m_serverCore->stopServer();
    m_logTextEdit->append(tr("[系统] 正在停止FTP服务器..."));
}

void FTPServer::onServerStarted()
{
    m_isServerRunning = true;
    updateServerStatus();
    m_startStopButton->setText(tr("停止服务器"));
    m_startStopButton->setStyleSheet("background-color: #ff6b6b; color: white;");
    m_statusBar->showMessage(tr("FTP服务器已启动 - 端口: %1").arg(m_config.port));
    m_logTextEdit->append(tr("[系统] FTP服务器启动成功"));
}

void FTPServer::onServerStopped()
{
    m_isServerRunning = false;
    m_activeConnections = 0;
    updateServerStatus();
    m_startStopButton->setText(tr("启动服务器"));
    m_startStopButton->setStyleSheet("");
    m_statusBar->showMessage(tr("FTP服务器已停止"));
    m_logTextEdit->append(tr("[系统] FTP服务器已停止"));
}

void FTPServer::onServerError(const QString& error)
{
    QMessageBox::critical(this, tr("服务器错误"), error);
    m_logTextEdit->append(tr("[错误] %1").arg(error));
}

void FTPServer::onNewConnection(const QString& clientIP)
{
    m_activeConnections++;
    m_totalConnections++;
    updateServerStatus();
    m_connectionMonitor->refreshConnections();
    m_logTextEdit->append(tr("[连接] 新客户端连接: %1").arg(clientIP));
}

void FTPServer::onConnectionClosed(const QString& clientIP)
{
    m_activeConnections--;
    updateServerStatus();
    m_connectionMonitor->refreshConnections();
    m_logTextEdit->append(tr("[连接] 客户端断开: %1").arg(clientIP));
}

void FTPServer::onConfigurationChanged()
{
    m_config.port = m_portSpinBox->value();
    m_config.rootDirectory = m_rootDirEdit->text();
    m_config.serverName = m_serverNameEdit->text();
    m_config.maxConnections = m_maxConnectionsSpinBox->value();
    m_config.allowAnonymous = m_allowAnonymousCheckBox->isChecked();
    m_config.usePassiveMode = m_usePassiveModeCheckBox->isChecked();
    m_config.passivePortMin = m_passiveMinPortSpinBox->value();
    m_config.passivePortMax = m_passiveMaxPortSpinBox->value();
}

void FTPServer::updateServerStatus()
{
    if (m_isServerRunning) {
        m_serverStatusLabel->setText(tr("运行中"));
        m_serverStatusLabel->setStyleSheet("color: green; font-weight: bold;");

        // 获取本机IP地址
        QString serverAddress = "localhost";
        QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
        for (const QHostAddress& address : addresses) {
            if (address != QHostAddress::LocalHost &&
                address.protocol() == QAbstractSocket::IPv4Protocol) {
                serverAddress = address.toString();
                break;
            }
        }
        m_serverAddressLabel->setText(QString("%1:%2").arg(serverAddress).arg(m_config.port));
    } else {
        m_serverStatusLabel->setText(tr("已停止"));
        m_serverStatusLabel->setStyleSheet("color: red; font-weight: bold;");
        m_serverAddressLabel->setText(tr("未启动"));
    }

    m_activeConnectionsLabel->setText(QString::number(m_activeConnections));
    m_totalConnectionsLabel->setText(QString::number(m_totalConnections));
}

// ========== FTPServerCore Implementation ==========

FTPServerCore::FTPServerCore(QObject* parent)
    : QObject(parent)
    , m_tcpServer(nullptr)
    , m_isRunning(false)
{
    // 添加默认用户
    FTPUserInfo defaultUser;
    defaultUser.username = "admin";
    defaultUser.password = "admin";
    defaultUser.homeDirectory = "/";
    defaultUser.canRead = true;
    defaultUser.canWrite = true;
    defaultUser.canDelete = true;
    defaultUser.canCreateDir = true;
    defaultUser.description = "默认管理员用户";
    addUser(defaultUser);
}

FTPServerCore::~FTPServerCore()
{
    stopServer();
}

bool FTPServerCore::startServer(const FTPServerConfig& config)
{
    if (m_isRunning) {
        return false;
    }

    m_config = config;
    m_tcpServer = new QTcpServer(this);

    connect(m_tcpServer, &QTcpServer::newConnection,
            this, &FTPServerCore::onNewConnection);

    if (!m_tcpServer->listen(QHostAddress::Any, m_config.port)) {
        emit serverError(tr("无法监听端口 %1: %2")
                        .arg(m_config.port)
                        .arg(m_tcpServer->errorString()));
        delete m_tcpServer;
        m_tcpServer = nullptr;
        return false;
    }

    m_isRunning = true;
    emit serverStarted();
    emit logMessage(tr("FTP服务器在端口 %1 启动").arg(m_config.port));
    return true;
}

void FTPServerCore::stopServer()
{
    if (!m_isRunning) {
        return;
    }

    // 关闭所有客户端连接
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        QTcpSocket* socket = it.key();
        FTPClientConnection* connection = it.value();
        socket->disconnectFromHost();
        delete connection;
    }
    m_connections.clear();

    if (m_tcpServer) {
        m_tcpServer->close();
        delete m_tcpServer;
        m_tcpServer = nullptr;
    }

    m_isRunning = false;
    emit serverStopped();
    emit logMessage(tr("FTP服务器已停止"));
}

bool FTPServerCore::isRunning() const
{
    return m_isRunning;
}

void FTPServerCore::addUser(const FTPUserInfo& user)
{
    QMutexLocker locker(&m_mutex);
    m_users[user.username] = user;
}

void FTPServerCore::removeUser(const QString& username)
{
    QMutexLocker locker(&m_mutex);
    m_users.remove(username);
}

void FTPServerCore::updateUser(const FTPUserInfo& user)
{
    QMutexLocker locker(&m_mutex);
    m_users[user.username] = user;
}

FTPUserInfo FTPServerCore::getUser(const QString& username) const
{
    QMutexLocker locker(&m_mutex);
    return m_users.value(username);
}

QList<FTPUserInfo> FTPServerCore::getAllUsers() const
{
    QMutexLocker locker(&m_mutex);
    return m_users.values();
}

QList<FTPServerConnectionInfo> FTPServerCore::getActiveConnections() const
{
    QList<FTPServerConnectionInfo> connections;
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        FTPClientConnection* connection = it.value();
        FTPServerConnectionInfo info;
        info.clientIP = connection->getClientIP();
        info.username = connection->getUsername();
        info.connectTime = connection->getConnectTime();
        info.currentDirectory = connection->getCurrentDirectory();
        info.bytesTransferred = connection->getBytesTransferred();
        info.isActive = true;
        connections.append(info);
    }
    return connections;
}

void FTPServerCore::disconnectClient(const QString& clientIP)
{
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        FTPClientConnection* connection = it.value();
        if (connection->getClientIP() == clientIP) {
            QTcpSocket* socket = it.key();
            socket->disconnectFromHost();
            break;
        }
    }
}

void FTPServerCore::onNewConnection()
{
    while (m_tcpServer->hasPendingConnections()) {
        QTcpSocket* socket = m_tcpServer->nextPendingConnection();
        if (!socket) continue;

        QString clientIP = socket->peerAddress().toString();

        // 检查连接数限制
        if (m_connections.size() >= m_config.maxConnections) {
            socket->write("421 Too many connections.\\r\\n");
            socket->disconnectFromHost();
            continue;
        }

        FTPClientConnection* connection = new FTPClientConnection(socket, m_config, this);

        connect(connection, &FTPClientConnection::disconnected,
                this, &FTPServerCore::onClientDisconnected);
        connect(connection, &FTPClientConnection::logMessage,
                this, &FTPServerCore::logMessage);

        m_connections[socket] = connection;
        emit newConnection(clientIP);
        emit logMessage(tr("接受来自 %1 的连接").arg(clientIP));
    }
}

void FTPServerCore::onClientDisconnected()
{
    FTPClientConnection* connection = qobject_cast<FTPClientConnection*>(sender());
    if (!connection) return;

    QString clientIP = connection->getClientIP();

    // 查找并移除连接
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        if (it.value() == connection) {
            m_connections.erase(it);
            break;
        }
    }

    emit connectionClosed(clientIP);
    connection->deleteLater();
}

// ========== FTPClientConnection Implementation ==========

FTPClientConnection::FTPClientConnection(QTcpSocket* socket, const FTPServerConfig& config,
                                       QObject* parent)
    : QObject(parent)
    , m_controlSocket(socket)
    , m_dataServer(nullptr)
    , m_dataSocket(nullptr)
    , m_config(config)
    , m_clientIP(socket->peerAddress().toString())
    , m_currentDirectory("/")
    , m_connectTime(QDateTime::currentDateTime())
    , m_bytesTransferred(0)
    , m_isAuthenticated(false)
    , m_isPassiveMode(false)
    , m_transferFile(nullptr)
{
    connect(m_controlSocket, &QTcpSocket::readyRead,
            this, &FTPClientConnection::onSocketReadyRead);
    connect(m_controlSocket, &QTcpSocket::disconnected,
            this, &FTPClientConnection::onSocketDisconnected);

    // 发送欢迎消息
    sendResponse(220, QString("%1 ready").arg(m_config.serverName));
}

FTPClientConnection::~FTPClientConnection()
{
    if (m_dataServer) {
        m_dataServer->deleteLater();
    }
    if (m_transferFile) {
        m_transferFile->close();
        delete m_transferFile;
    }
}

QString FTPClientConnection::getClientIP() const
{
    return m_clientIP;
}

QString FTPClientConnection::getUsername() const
{
    return m_username;
}

QString FTPClientConnection::getCurrentDirectory() const
{
    return m_currentDirectory;
}

QDateTime FTPClientConnection::getConnectTime() const
{
    return m_connectTime;
}

qint64 FTPClientConnection::getBytesTransferred() const
{
    return m_bytesTransferred;
}

bool FTPClientConnection::isAuthenticated() const
{
    return m_isAuthenticated;
}

void FTPClientConnection::onSocketReadyRead()
{
    while (m_controlSocket->canReadLine()) {
        QByteArray data = m_controlSocket->readLine();
        QString command = QString::fromUtf8(data).trimmed();

        emit logMessage(QString("[%1] > %2").arg(m_clientIP, command));
        processCommand(command);
    }
}

void FTPClientConnection::onSocketDisconnected()
{
    emit logMessage(QString("[%1] 连接断开").arg(m_clientIP));
    emit disconnected();
}

void FTPClientConnection::onDataSocketConnected()
{
    emit logMessage(QString("[%1] 数据连接建立").arg(m_clientIP));
}

void FTPClientConnection::onDataSocketDisconnected()
{
    emit logMessage(QString("[%1] 数据连接断开").arg(m_clientIP));
    if (m_dataSocket) {
        m_dataSocket->deleteLater();
        m_dataSocket = nullptr;
    }
}

void FTPClientConnection::onDataSocketReadyRead()
{
    // 处理数据传输
    if (m_transferFile && m_transferType == "STOR") {
        QByteArray data = m_dataSocket->readAll();
        qint64 written = m_transferFile->write(data);
        m_bytesTransferred += written;
        emit bytesTransferred(written);
    }
}

void FTPClientConnection::sendResponse(int code, const QString& message)
{
    QString response = QString("%1 %2\\r\\n").arg(code).arg(message);
    m_controlSocket->write(response.toUtf8());
    emit logMessage(QString("[%1] < %2 %3").arg(m_clientIP).arg(code).arg(message));
}

void FTPClientConnection::processCommand(const QString& command)
{
    QStringList parts = command.split(' ', Qt::SkipEmptyParts);
    if (parts.isEmpty()) return;

    QString cmd = parts[0].toUpper();
    QString arg = parts.size() > 1 ? parts.mid(1).join(' ') : "";

    if (cmd == "USER") {
        handleUser(arg);
    } else if (cmd == "PASS") {
        handlePass(arg);
    } else if (!m_isAuthenticated) {
        sendResponse(530, "Please login with USER and PASS");
    } else if (cmd == "PWD") {
        handlePwd();
    } else if (cmd == "CWD") {
        handleCwd(arg);
    } else if (cmd == "LIST") {
        handleList(arg);
    } else if (cmd == "RETR") {
        handleRetr(arg);
    } else if (cmd == "STOR") {
        handleStor(arg);
    } else if (cmd == "DELE") {
        handleDele(arg);
    } else if (cmd == "MKD") {
        handleMkd(arg);
    } else if (cmd == "RMD") {
        handleRmd(arg);
    } else if (cmd == "PASV") {
        handlePasv();
    } else if (cmd == "PORT") {
        handlePort(arg);
    } else if (cmd == "QUIT") {
        handleQuit();
    } else if (cmd == "NOOP") {
        sendResponse(200, "OK");
    } else if (cmd == "SYST") {
        sendResponse(215, "UNIX Type: L8");
    } else if (cmd == "TYPE") {
        sendResponse(200, "Type set to I");
    } else {
        sendResponse(502, "Command not implemented");
    }
}

void FTPClientConnection::handleUser(const QString& username)
{
    m_username = username;
    sendResponse(331, "Password required");
}

void FTPClientConnection::handlePass(const QString& password)
{
    if (authenticateUser(m_username, password)) {
        m_isAuthenticated = true;
        sendResponse(230, "User logged in");
        emit logMessage(QString("[%1] 用户 %2 登录成功").arg(m_clientIP, m_username));
    } else {
        sendResponse(530, "Login incorrect");
        emit logMessage(QString("[%1] 用户 %2 登录失败").arg(m_clientIP, m_username));
    }
}

void FTPClientConnection::handlePwd()
{
    sendResponse(257, QString("\"%1\" is current directory").arg(m_currentDirectory));
}

void FTPClientConnection::handleCwd(const QString& path)
{
    QString newPath = resolvePath(path);
    QDir dir(m_config.rootDirectory + newPath);

    if (dir.exists()) {
        m_currentDirectory = newPath;
        sendResponse(250, "Directory changed");
    } else {
        sendResponse(550, "Directory not found");
    }
}

void FTPClientConnection::handleList(const QString& path)
{
    if (!hasPermission("read")) {
        sendResponse(550, "Permission denied");
        return;
    }

    QString listPath = path.isEmpty() ? m_currentDirectory : resolvePath(path);
    QString fullPath = m_config.rootDirectory + listPath;

    QDir dir(fullPath);
    if (!dir.exists()) {
        sendResponse(550, "Directory not found");
        return;
    }

    if (!m_dataSocket) {
        sendResponse(425, "Use PASV or PORT first");
        return;
    }

    sendResponse(150, "Opening data connection");

    QString listing = formatFileList(fullPath);
    m_dataSocket->write(listing.toUtf8());
    m_dataSocket->disconnectFromHost();

    sendResponse(226, "Transfer complete");
}

void FTPClientConnection::handleRetr(const QString& filename)
{
    if (!hasPermission("read")) {
        sendResponse(550, "Permission denied");
        return;
    }

    QString filePath = m_config.rootDirectory + resolvePath(filename);
    QFile file(filePath);

    if (!file.exists()) {
        sendResponse(550, "File not found");
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        sendResponse(550, "Cannot open file");
        return;
    }

    if (!m_dataSocket) {
        sendResponse(425, "Use PASV or PORT first");
        return;
    }

    sendResponse(150, "Opening data connection");

    QByteArray data = file.readAll();
    m_dataSocket->write(data);
    m_dataSocket->disconnectFromHost();
    m_bytesTransferred += data.size();

    sendResponse(226, "Transfer complete");
    file.close();
}

void FTPClientConnection::handleStor(const QString& filename)
{
    if (!hasPermission("write")) {
        sendResponse(550, "Permission denied");
        return;
    }

    QString filePath = m_config.rootDirectory + resolvePath(filename);

    if (m_transferFile) {
        m_transferFile->close();
        delete m_transferFile;
    }

    m_transferFile = new QFile(filePath);
    if (!m_transferFile->open(QIODevice::WriteOnly)) {
        sendResponse(550, "Cannot create file");
        delete m_transferFile;
        m_transferFile = nullptr;
        return;
    }

    if (!m_dataSocket) {
        sendResponse(425, "Use PASV or PORT first");
        m_transferFile->close();
        delete m_transferFile;
        m_transferFile = nullptr;
        return;
    }

    m_transferType = "STOR";
    sendResponse(150, "Opening data connection");

    // 连接数据套接字信号
    connect(m_dataSocket, &QTcpSocket::readyRead,
            this, &FTPClientConnection::onDataSocketReadyRead);
    connect(m_dataSocket, &QTcpSocket::disconnected, [this]() {
        if (m_transferFile) {
            m_transferFile->close();
            delete m_transferFile;
            m_transferFile = nullptr;
        }
        sendResponse(226, "Transfer complete");
    });
}

void FTPClientConnection::handleDele(const QString& filename)
{
    if (!hasPermission("delete")) {
        sendResponse(550, "Permission denied");
        return;
    }

    QString filePath = m_config.rootDirectory + resolvePath(filename);
    QFile file(filePath);

    if (!file.exists()) {
        sendResponse(550, "File not found");
        return;
    }

    if (file.remove()) {
        sendResponse(250, "File deleted");
    } else {
        sendResponse(550, "Delete failed");
    }
}

void FTPClientConnection::handleMkd(const QString& dirname)
{
    if (!hasPermission("createDir")) {
        sendResponse(550, "Permission denied");
        return;
    }

    QString dirPath = m_config.rootDirectory + resolvePath(dirname);
    QDir dir;

    if (dir.mkpath(dirPath)) {
        sendResponse(257, QString("\"%1\" directory created").arg(dirname));
    } else {
        sendResponse(550, "Create directory failed");
    }
}

void FTPClientConnection::handleRmd(const QString& dirname)
{
    if (!hasPermission("delete")) {
        sendResponse(550, "Permission denied");
        return;
    }

    QString dirPath = m_config.rootDirectory + resolvePath(dirname);
    QDir dir(dirPath);

    if (dir.removeRecursively()) {
        sendResponse(250, "Directory removed");
    } else {
        sendResponse(550, "Remove directory failed");
    }
}

void FTPClientConnection::handlePasv()
{
    if (m_dataServer) {
        m_dataServer->deleteLater();
    }

    m_dataServer = new QTcpServer(this);

    // 监听被动模式端口
    int port = m_config.passivePortMin;
    bool listening = false;

    for (port = m_config.passivePortMin; port <= m_config.passivePortMax; ++port) {
        if (m_dataServer->listen(QHostAddress::Any, port)) {
            listening = true;
            break;
        }
    }

    if (!listening) {
        sendResponse(425, "Cannot open passive connection");
        return;
    }

    connect(m_dataServer, &QTcpServer::newConnection, [this]() {
        m_dataSocket = m_dataServer->nextPendingConnection();
        connect(m_dataSocket, &QTcpSocket::connected,
                this, &FTPClientConnection::onDataSocketConnected);
        connect(m_dataSocket, &QTcpSocket::disconnected,
                this, &FTPClientConnection::onDataSocketDisconnected);
    });

    m_isPassiveMode = true;

    // 获取服务器IP地址
    QHostAddress serverAddr = m_controlSocket->localAddress();
    quint32 addr = serverAddr.toIPv4Address();

    int p1 = port / 256;
    int p2 = port % 256;

    QString response = QString("227 Entering Passive Mode (%1,%2,%3,%4,%5,%6)")
                      .arg((addr >> 24) & 0xFF)
                      .arg((addr >> 16) & 0xFF)
                      .arg((addr >> 8) & 0xFF)
                      .arg(addr & 0xFF)
                      .arg(p1)
                      .arg(p2);

    sendResponse(227, response.mid(4)); // 去掉开头的"227 "
}

void FTPClientConnection::handlePort(const QString& portInfo)
{
    // PORT命令格式: PORT h1,h2,h3,h4,p1,p2
    QStringList parts = portInfo.split(',');
    if (parts.size() != 6) {
        sendResponse(501, "Invalid PORT command");
        return;
    }

    bool ok;
    int h1 = parts[0].toInt(&ok); if (!ok) { sendResponse(501, "Invalid PORT command"); return; }
    int h2 = parts[1].toInt(&ok); if (!ok) { sendResponse(501, "Invalid PORT command"); return; }
    int h3 = parts[2].toInt(&ok); if (!ok) { sendResponse(501, "Invalid PORT command"); return; }
    int h4 = parts[3].toInt(&ok); if (!ok) { sendResponse(501, "Invalid PORT command"); return; }
    int p1 = parts[4].toInt(&ok); if (!ok) { sendResponse(501, "Invalid PORT command"); return; }
    int p2 = parts[5].toInt(&ok); if (!ok) { sendResponse(501, "Invalid PORT command"); return; }

    QString clientIP = QString("%1.%2.%3.%4").arg(h1).arg(h2).arg(h3).arg(h4);
    int clientPort = p1 * 256 + p2;

    // 创建主动模式数据连接
    if (m_dataSocket) {
        m_dataSocket->deleteLater();
    }

    m_dataSocket = new QTcpSocket(this);
    connect(m_dataSocket, &QTcpSocket::connected,
            this, &FTPClientConnection::onDataSocketConnected);
    connect(m_dataSocket, &QTcpSocket::disconnected,
            this, &FTPClientConnection::onDataSocketDisconnected);

    m_dataSocket->connectToHost(clientIP, clientPort);
    m_isPassiveMode = false;

    sendResponse(200, "PORT command successful");
}

void FTPClientConnection::handleQuit()
{
    sendResponse(221, "Goodbye");
    m_controlSocket->disconnectFromHost();
}

bool FTPClientConnection::authenticateUser(const QString& username, const QString& password)
{
    // 这里需要从服务器核心获取用户信息
    // 简化实现：固定用户
    if (username == "admin" && password == "admin") {
        m_userInfo.username = username;
        m_userInfo.homeDirectory = "/";
        m_userInfo.canRead = true;
        m_userInfo.canWrite = true;
        m_userInfo.canDelete = true;
        m_userInfo.canCreateDir = true;
        m_userInfo.isEnabled = true;
        return true;
    }
    return false;
}

bool FTPClientConnection::hasPermission(const QString& operation) const
{
    if (!m_isAuthenticated) return false;

    if (operation == "read") return m_userInfo.canRead;
    if (operation == "write") return m_userInfo.canWrite;
    if (operation == "delete") return m_userInfo.canDelete;
    if (operation == "createDir") return m_userInfo.canCreateDir;

    return false;
}

QString FTPClientConnection::resolvePath(const QString& path) const
{
    QString resolved;

    if (path.startsWith('/')) {
        resolved = path;
    } else {
        resolved = m_currentDirectory;
        if (!resolved.endsWith('/')) {
            resolved += '/';
        }
        resolved += path;
    }

    // 规范化路径
    QStringList parts = resolved.split('/', Qt::SkipEmptyParts);
    QStringList normalizedParts;

    for (const QString& part : parts) {
        if (part == "..") {
            if (!normalizedParts.isEmpty()) {
                normalizedParts.removeLast();
            }
        } else if (part != ".") {
            normalizedParts.append(part);
        }
    }

    resolved = "/" + normalizedParts.join("/");
    if (resolved != "/" && !path.endsWith('/')) {
        // 保持原始路径的尾部斜杠
    }

    return resolved;
}

QString FTPClientConnection::formatFileList(const QString& directory) const
{
    QDir dir(directory);
    QString listing;

    // 添加上级目录
    if (m_currentDirectory != "/") {
        listing += "drwxrwxrwx   1 owner    group            0 Jan 01 12:00 ..\\r\\n";
    }

    // 添加目录
    QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo& info : entries) {
        QString line = QString("drwxrwxrwx   1 owner    group            0 %1 %2\\r\\n")
                      .arg(info.lastModified().toString("MMM dd hh:mm"))
                      .arg(info.fileName());
        listing += line;
    }

    // 添加文件
    entries = dir.entryInfoList(QDir::Files, QDir::Name);
    for (const QFileInfo& info : entries) {
        QString line = QString("-rw-rw-rw-   1 owner    group    %1 %2 %3\\r\\n")
                      .arg(info.size(), 12)
                      .arg(info.lastModified().toString("MMM dd hh:mm"))
                      .arg(info.fileName());
        listing += line;
    }

    return listing;
}

// ========== FTPUserManager Implementation ==========

FTPUserManager::FTPUserManager(QWidget* parent)
    : QWidget(parent)
    , m_layout(nullptr)
    , m_userTree(nullptr)
    , m_serverCore(nullptr)
{
    setupUI();
}

FTPUserManager::~FTPUserManager()
{
}

void FTPUserManager::setupUI()
{
    m_layout = new QVBoxLayout(this);

    // 标题
    QLabel* titleLabel = new QLabel(tr("用户管理"), this);
    titleLabel->setStyleSheet("font-weight: bold; padding: 5px;");
    m_layout->addWidget(titleLabel);

    // 用户列表
    m_userTree = new QTreeWidget(this);
    m_userTree->setHeaderLabels({tr("用户名"), tr("主目录"), tr("权限"), tr("状态")});
    m_userTree->setRootIsDecorated(false);
    m_userTree->setAlternatingRowColors(true);
    m_layout->addWidget(m_userTree);

    // 按钮区域
    QHBoxLayout* buttonLayout = new QHBoxLayout();

    m_addButton = new QPushButton(tr("添加用户"), this);
    m_editButton = new QPushButton(tr("编辑用户"), this);
    m_deleteButton = new QPushButton(tr("删除用户"), this);

    buttonLayout->addWidget(m_addButton);
    buttonLayout->addWidget(m_editButton);
    buttonLayout->addWidget(m_deleteButton);
    buttonLayout->addStretch();

    m_layout->addLayout(buttonLayout);

    // 连接信号
    connect(m_userTree, &QTreeWidget::itemSelectionChanged,
            this, &FTPUserManager::onUserSelectionChanged);

    connect(m_addButton, &QPushButton::clicked,
            this, &FTPUserManager::onAddUser);
    connect(m_editButton, &QPushButton::clicked,
            this, &FTPUserManager::onEditUser);
    connect(m_deleteButton, &QPushButton::clicked,
            this, &FTPUserManager::onDeleteUser);

    // 初始按钮状态
    m_editButton->setEnabled(false);
    m_deleteButton->setEnabled(false);
}

void FTPUserManager::setServerCore(FTPServerCore* serverCore)
{
    m_serverCore = serverCore;
    refreshUserList();
}

void FTPUserManager::refreshUserList()
{
    if (!m_serverCore) return;

    updateUserList();
}

void FTPUserManager::onAddUser()
{
    FTPUserDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        FTPUserInfo user = dialog.getUserInfo();
        if (m_serverCore) {
            m_serverCore->addUser(user);
            emit userAdded(user);
            refreshUserList();
        }
    }
}

void FTPUserManager::onEditUser()
{
    if (m_selectedUsername.isEmpty() || !m_serverCore) return;

    FTPUserInfo user = m_serverCore->getUser(m_selectedUsername);
    FTPUserDialog dialog(user, this);
    if (dialog.exec() == QDialog::Accepted) {
        FTPUserInfo updatedUser = dialog.getUserInfo();
        m_serverCore->updateUser(updatedUser);
        emit userUpdated(updatedUser);
        refreshUserList();
    }
}

void FTPUserManager::onDeleteUser()
{
    if (m_selectedUsername.isEmpty() || !m_serverCore) return;

    int ret = QMessageBox::question(this, tr("确认删除"),
                                   tr("确定要删除用户 %1 吗？").arg(m_selectedUsername),
                                   QMessageBox::Yes | QMessageBox::No);
    if (ret == QMessageBox::Yes) {
        m_serverCore->removeUser(m_selectedUsername);
        emit userRemoved(m_selectedUsername);
        refreshUserList();
    }
}

void FTPUserManager::onUserSelectionChanged()
{
    QList<QTreeWidgetItem*> selected = m_userTree->selectedItems();
    bool hasSelection = !selected.isEmpty();

    m_editButton->setEnabled(hasSelection);
    m_deleteButton->setEnabled(hasSelection);

    if (hasSelection) {
        m_selectedUsername = selected.first()->text(0);
    } else {
        m_selectedUsername.clear();
    }
}

void FTPUserManager::contextMenuEvent(QContextMenuEvent* event)
{
    QTreeWidgetItem* item = m_userTree->itemAt(event->pos());

    QMenu menu(this);

    if (item) {
        menu.addAction(tr("编辑用户"), this, &FTPUserManager::onEditUser);
        menu.addAction(tr("删除用户"), this, &FTPUserManager::onDeleteUser);
        menu.addSeparator();
    }

    menu.addAction(tr("添加用户"), this, &FTPUserManager::onAddUser);

    menu.exec(event->globalPos());
}

void FTPUserManager::updateUserList()
{
    m_userTree->clear();

    if (!m_serverCore) return;

    QList<FTPUserInfo> users = m_serverCore->getAllUsers();
    for (const FTPUserInfo& user : users) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_userTree);
        item->setText(0, user.username);
        item->setText(1, user.homeDirectory);

        QStringList permissions;
        if (user.canRead) permissions << "读";
        if (user.canWrite) permissions << "写";
        if (user.canDelete) permissions << "删";
        if (user.canCreateDir) permissions << "建目录";
        item->setText(2, permissions.join(","));

        item->setText(3, user.isEnabled ? tr("启用") : tr("禁用"));
        item->setData(0, Qt::UserRole, user.username);

        if (!user.isEnabled) {
            item->setForeground(0, Qt::gray);
            item->setForeground(1, Qt::gray);
            item->setForeground(2, Qt::gray);
            item->setForeground(3, Qt::gray);
        }
    }

    m_userTree->resizeColumnToContents(0);
    m_userTree->resizeColumnToContents(1);
    m_userTree->resizeColumnToContents(2);
}

// ========== FTPConnectionMonitor Implementation ==========

FTPConnectionMonitor::FTPConnectionMonitor(QWidget* parent)
    : QWidget(parent)
    , m_layout(nullptr)
    , m_connectionTree(nullptr)
    , m_serverCore(nullptr)
    , m_refreshTimer(nullptr)
{
    setupUI();

    // 设置自动刷新定时器
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(5000); // 5秒刷新一次
    connect(m_refreshTimer, &QTimer::timeout, this, &FTPConnectionMonitor::refreshConnections);
    m_refreshTimer->start();
}

FTPConnectionMonitor::~FTPConnectionMonitor()
{
}

void FTPConnectionMonitor::setupUI()
{
    m_layout = new QVBoxLayout(this);

    // 标题
    QLabel* titleLabel = new QLabel(tr("连接监控"), this);
    titleLabel->setStyleSheet("font-weight: bold; padding: 5px;");
    m_layout->addWidget(titleLabel);

    // 连接列表
    m_connectionTree = new QTreeWidget(this);
    m_connectionTree->setHeaderLabels({tr("客户端IP"), tr("用户名"), tr("连接时间"), tr("当前目录"), tr("传输字节")});
    m_connectionTree->setRootIsDecorated(false);
    m_connectionTree->setAlternatingRowColors(true);
    m_layout->addWidget(m_connectionTree);

    // 统计信息
    m_statisticsLabel = new QLabel(this);
    m_statisticsLabel->setStyleSheet("background-color: #f0f0f0; padding: 5px; border: 1px solid #ccc;");
    m_layout->addWidget(m_statisticsLabel);

    // 按钮区域
    QHBoxLayout* buttonLayout = new QHBoxLayout();

    m_refreshButton = new QPushButton(tr("刷新"), this);
    m_disconnectButton = new QPushButton(tr("断开连接"), this);

    buttonLayout->addWidget(m_refreshButton);
    buttonLayout->addWidget(m_disconnectButton);
    buttonLayout->addStretch();

    m_layout->addLayout(buttonLayout);

    // 连接信号
    connect(m_connectionTree, &QTreeWidget::itemSelectionChanged,
            this, &FTPConnectionMonitor::onConnectionSelectionChanged);

    connect(m_refreshButton, &QPushButton::clicked,
            this, &FTPConnectionMonitor::onRefresh);
    connect(m_disconnectButton, &QPushButton::clicked,
            this, &FTPConnectionMonitor::onDisconnectClient);

    // 初始按钮状态
    m_disconnectButton->setEnabled(false);
}

void FTPConnectionMonitor::setServerCore(FTPServerCore* serverCore)
{
    m_serverCore = serverCore;
    refreshConnections();
}

void FTPConnectionMonitor::refreshConnections()
{
    updateConnectionList();
}

void FTPConnectionMonitor::onRefresh()
{
    refreshConnections();
}

void FTPConnectionMonitor::onDisconnectClient()
{
    if (m_selectedClientIP.isEmpty() || !m_serverCore) return;

    int ret = QMessageBox::question(this, tr("确认断开"),
                                   tr("确定要断开客户端 %1 的连接吗？").arg(m_selectedClientIP),
                                   QMessageBox::Yes | QMessageBox::No);
    if (ret == QMessageBox::Yes) {
        m_serverCore->disconnectClient(m_selectedClientIP);
        refreshConnections();
    }
}

void FTPConnectionMonitor::onConnectionSelectionChanged()
{
    QList<QTreeWidgetItem*> selected = m_connectionTree->selectedItems();
    bool hasSelection = !selected.isEmpty();

    m_disconnectButton->setEnabled(hasSelection);

    if (hasSelection) {
        m_selectedClientIP = selected.first()->text(0);
    } else {
        m_selectedClientIP.clear();
    }
}

void FTPConnectionMonitor::updateConnectionList()
{
    m_connectionTree->clear();

    if (!m_serverCore) return;

    QList<FTPServerConnectionInfo> connections = m_serverCore->getActiveConnections();

    for (const FTPServerConnectionInfo& conn : connections) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_connectionTree);
        item->setText(0, conn.clientIP);
        item->setText(1, conn.username.isEmpty() ? tr("未登录") : conn.username);
        item->setText(2, conn.connectTime.toString("yyyy-MM-dd hh:mm:ss"));
        item->setText(3, conn.currentDirectory);
        item->setText(4, QString::number(conn.bytesTransferred));
        item->setData(0, Qt::UserRole, conn.clientIP);
    }

    // 更新统计信息
    QString stats = tr("活动连接: %1 个").arg(connections.size());
    qint64 totalBytes = 0;
    for (const FTPServerConnectionInfo& conn : connections) {
        totalBytes += conn.bytesTransferred;
    }
    stats += tr(" | 总传输: %1 字节").arg(totalBytes);

    m_statisticsLabel->setText(stats);

    m_connectionTree->resizeColumnToContents(0);
    m_connectionTree->resizeColumnToContents(1);
    m_connectionTree->resizeColumnToContents(2);
    m_connectionTree->resizeColumnToContents(3);
}

// ========== FTPUserDialog Implementation ==========

FTPUserDialog::FTPUserDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("添加用户"));
    setModal(true);
    setupUI();
}

FTPUserDialog::FTPUserDialog(const FTPUserInfo& user, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("编辑用户"));
    setModal(true);
    setupUI();
    setUserInfo(user);
}

void FTPUserDialog::setupUI()
{
    setMinimumSize(500, 400);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // 表单区域
    QGroupBox* formGroup = new QGroupBox(tr("用户信息"), this);
    m_formLayout = new QFormLayout(formGroup);

    m_usernameEdit = new QLineEdit(this);
    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);

    QHBoxLayout* homeDirLayout = new QHBoxLayout();
    m_homeDirEdit = new QLineEdit(this);
    m_browseDirButton = new QPushButton(tr("浏览..."), this);
    homeDirLayout->addWidget(m_homeDirEdit);
    homeDirLayout->addWidget(m_browseDirButton);

    m_formLayout->addRow(tr("用户名:"), m_usernameEdit);
    m_formLayout->addRow(tr("密码:"), m_passwordEdit);
    m_formLayout->addRow(tr("主目录:"), homeDirLayout);

    mainLayout->addWidget(formGroup);

    // 权限区域
    QGroupBox* permGroup = new QGroupBox(tr("权限设置"), this);
    QVBoxLayout* permLayout = new QVBoxLayout(permGroup);

    m_canReadCheckBox = new QCheckBox(tr("读取文件"), this);
    m_canWriteCheckBox = new QCheckBox(tr("写入文件"), this);
    m_canDeleteCheckBox = new QCheckBox(tr("删除文件"), this);
    m_canCreateDirCheckBox = new QCheckBox(tr("创建目录"), this);
    m_isEnabledCheckBox = new QCheckBox(tr("启用用户"), this);

    m_canReadCheckBox->setChecked(true);
    m_isEnabledCheckBox->setChecked(true);

    permLayout->addWidget(m_canReadCheckBox);
    permLayout->addWidget(m_canWriteCheckBox);
    permLayout->addWidget(m_canDeleteCheckBox);
    permLayout->addWidget(m_canCreateDirCheckBox);
    permLayout->addWidget(m_isEnabledCheckBox);

    mainLayout->addWidget(permGroup);

    // 描述区域
    QGroupBox* descGroup = new QGroupBox(tr("描述"), this);
    QVBoxLayout* descLayout = new QVBoxLayout(descGroup);

    m_descriptionEdit = new QTextEdit(this);
    m_descriptionEdit->setMaximumHeight(80);
    descLayout->addWidget(m_descriptionEdit);

    mainLayout->addWidget(descGroup);

    // 按钮区域
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* okButton = new QPushButton(tr("确定"), this);
    QPushButton* cancelButton = new QPushButton(tr("取消"), this);

    okButton->setDefault(true);

    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);

    mainLayout->addLayout(buttonLayout);

    // 连接信号
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_browseDirButton, &QPushButton::clicked, this, &FTPUserDialog::onBrowseHomeDirectory);
}

FTPUserInfo FTPUserDialog::getUserInfo() const
{
    FTPUserInfo user;
    user.username = m_usernameEdit->text().trimmed();
    user.password = m_passwordEdit->text();
    user.homeDirectory = m_homeDirEdit->text().trimmed();
    user.canRead = m_canReadCheckBox->isChecked();
    user.canWrite = m_canWriteCheckBox->isChecked();
    user.canDelete = m_canDeleteCheckBox->isChecked();
    user.canCreateDir = m_canCreateDirCheckBox->isChecked();
    user.isEnabled = m_isEnabledCheckBox->isChecked();
    user.description = m_descriptionEdit->toPlainText().trimmed();

    return user;
}

void FTPUserDialog::setUserInfo(const FTPUserInfo& user)
{
    m_usernameEdit->setText(user.username);
    m_passwordEdit->setText(user.password);
    m_homeDirEdit->setText(user.homeDirectory);
    m_canReadCheckBox->setChecked(user.canRead);
    m_canWriteCheckBox->setChecked(user.canWrite);
    m_canDeleteCheckBox->setChecked(user.canDelete);
    m_canCreateDirCheckBox->setChecked(user.canCreateDir);
    m_isEnabledCheckBox->setChecked(user.isEnabled);
    m_descriptionEdit->setPlainText(user.description);
}

void FTPUserDialog::onBrowseHomeDirectory()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("选择用户主目录"),
                                                   m_homeDirEdit->text());
    if (!dir.isEmpty()) {
        m_homeDirEdit->setText(dir);
    }
}

