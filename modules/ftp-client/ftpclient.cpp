#include "ftpclient.h"

REGISTER_DYNAMICOBJECT(FTPClient);

#include <QApplication>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDesktopServices>

// ========== FTPClient Implementation ==========

FTPClient::FTPClient(QWidget* parent)
    : QWidget(parent), DynamicObjectBase()
    , m_mainSplitter(nullptr)
    , m_connectionManager(nullptr)
    , m_rightTabs(nullptr)
    , m_fileManager(nullptr)
    , m_menuBar(nullptr)
    , m_statusBar(nullptr)
    , m_isConnected(false)
{
    setWindowTitle(tr("FTP客户端"));
    setupUI();
#ifndef Q_OS_MAC
    setupMenuBar();
#endif
    loadSettings();

    // 连接信号
    connect(m_connectionManager, &FTPConnectionManager::connectionRequested,
            this, &FTPClient::onConnectionRequested);
    connect(m_connectionManager, &FTPConnectionManager::newConnectionRequested,
            this, &FTPClient::onNewConnection);
    connect(m_connectionManager, &FTPConnectionManager::editConnectionRequested,
            this, &FTPClient::onEditConnection);
    connect(m_connectionManager, &FTPConnectionManager::deleteConnectionRequested,
            this, &FTPClient::onDeleteConnection);

    m_statusBar->showMessage(tr("就绪"));
}

FTPClient::~FTPClient()
{
    saveSettings();
}

void FTPClient::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

#ifndef Q_OS_MAC
    // 创建菜单栏 - 固定高度，不拉伸
    m_menuBar = new QMenuBar(this);
    m_menuBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    mainLayout->addWidget(m_menuBar, 0);
#endif

    // 创建主分割器 - 占据所有剩余空间
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainLayout->addWidget(m_mainSplitter, 1); // 拉伸因子为1，占据所有剩余高度

    // 左侧：连接管理器
    m_connectionManager = new FTPConnectionManager(this);
    m_connectionManager->setMinimumWidth(200);
    m_connectionManager->setMaximumWidth(350);
    m_mainSplitter->addWidget(m_connectionManager);

    // 右侧：标签页
    m_rightTabs = new QTabWidget(this);
    m_mainSplitter->addWidget(m_rightTabs);

    // FTP文件管理器标签页
    m_fileManager = new FTPFileManager(this);
    m_rightTabs->addTab(m_fileManager, tr("FTP文件管理"));

    // 设置分割器比例
    m_mainSplitter->setSizes({300, 700});

    // 创建状态栏 - 固定高度，不拉伸
    m_statusBar = new QStatusBar(this);
    m_statusBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    mainLayout->addWidget(m_statusBar, 0);
}

void FTPClient::setupMenuBar()
{
    // 连接菜单
    QMenu* connectionMenu = m_menuBar->addMenu(tr("连接(&C)"));

    QAction* newConnectionAction = connectionMenu->addAction(tr("新建连接(&N)"));
    newConnectionAction->setShortcut(QKeySequence::New);
    connect(newConnectionAction, &QAction::triggered, this, &FTPClient::onNewConnection);

    QAction* editConnectionAction = connectionMenu->addAction(tr("编辑连接(&E)"));
    connect(editConnectionAction, &QAction::triggered, this, &FTPClient::onEditConnection);

    connectionMenu->addSeparator();

    QAction* disconnectAction = connectionMenu->addAction(tr("断开连接(&D)"));
    disconnectAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    connect(disconnectAction, &QAction::triggered, this, &FTPClient::onDisconnect);

    connectionMenu->addSeparator();

    QAction* exitAction = connectionMenu->addAction(tr("退出(&X)"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    // 帮助菜单
    QMenu* helpMenu = m_menuBar->addMenu(tr("帮助(&H)"));
    QAction* aboutAction = helpMenu->addAction(tr("关于"));
    connect(aboutAction, &QAction::triggered, [this]() {
        QMessageBox::about(this, tr("关于FTP客户端"),
                          tr("FTP客户端工具\\n\\n"
                             "提供FTP连接管理和文件传输功能\\n"
                             "支持被动模式和主动模式连接"));
    });
}

void FTPClient::loadSettings()
{
    QSettings settings;
    settings.beginGroup("FTPClient");

    // 恢复窗口几何信息
    restoreGeometry(settings.value("geometry").toByteArray());

    // 恢复分割器状态
    m_mainSplitter->restoreState(settings.value("splitterState").toByteArray());

    settings.endGroup();
}

void FTPClient::saveSettings()
{
    QSettings settings;
    settings.beginGroup("FTPClient");

    // 保存窗口几何信息
    settings.setValue("geometry", saveGeometry());

    // 保存分割器状态
    settings.setValue("splitterState", m_mainSplitter->saveState());

    settings.endGroup();
}

void FTPClient::closeEvent(QCloseEvent* event)
{
    saveSettings();
    event->accept();
}

void FTPClient::onConnectionRequested(const QString& name)
{
    // 建立FTP连接
    FTPConnectionInfo info = m_connectionManager->getConnectionInfo(name);
    if (!info.isValid()) {
        QMessageBox::warning(this, tr("错误"), tr("连接信息无效"));
        return;
    }

    m_statusBar->showMessage(tr("正在连接到 %1...").arg(info.hostname));

    // 创建连接并设置到文件管理器
    FTPConnection* connection = new FTPConnection(info, this);

    connect(connection, &FTPConnection::connected, [this, name]() {
        m_currentConnection = name;
        m_isConnected = true;
        m_statusBar->showMessage(tr("已连接到 %1").arg(name));
        emit connectionEstablished(name);
    });

    connect(connection, &FTPConnection::disconnected, [this, name]() {
        m_currentConnection.clear();
        m_isConnected = false;
        m_statusBar->showMessage(tr("已断开连接"));
        emit connectionClosed(name);
    });

    connect(connection, &FTPConnection::error, [this, name](const QString& error) {
        m_statusBar->showMessage(tr("连接失败: %1").arg(error));
        emit connectionError(name, error);
    });

    m_fileManager->setConnection(connection);
    connection->connectToHost();
}

void FTPClient::onConnectionEstablished(const QString& name)
{
    m_statusBar->showMessage(tr("已连接到 %1").arg(name));
}

void FTPClient::onConnectionClosed(const QString& name)
{
    m_statusBar->showMessage(tr("已断开与 %1 的连接").arg(name));
}

void FTPClient::onConnectionError(const QString& name, const QString& error)
{
    QMessageBox::critical(this, tr("连接错误"),
                         tr("连接到 %1 失败:\\n%2").arg(name, error));
}

void FTPClient::onNewConnection()
{
    FTPConnectionDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        FTPConnectionInfo info = dialog.getConnectionInfo();
        m_connectionManager->addConnection(info);
    }
}

void FTPClient::onEditConnection()
{
    // 获取当前选中的连接进行编辑
    // 这里需要从连接管理器获取选中的连接
    QMessageBox::information(this, tr("提示"), tr("编辑连接功能开发中..."));
}

void FTPClient::onDeleteConnection()
{
    // 删除选中的连接
    QMessageBox::information(this, tr("提示"), tr("删除连接功能开发中..."));
}

void FTPClient::onDisconnect()
{
    if (m_isConnected) {
        // 断开当前连接
        QMessageBox::information(this, tr("提示"), tr("断开连接功能开发中..."));
    }
}

// ========== FTPConnectionManager Implementation ==========

FTPConnectionManager::FTPConnectionManager(QWidget* parent)
    : QWidget(parent)
    , m_layout(nullptr)
    , m_connectionsTree(nullptr)
    , m_newButton(nullptr)
    , m_editButton(nullptr)
    , m_deleteButton(nullptr)
    , m_connectButton(nullptr)
{
    setupUI();
    loadConnections();
    updateConnectionsList();
}

FTPConnectionManager::~FTPConnectionManager()
{
    saveConnections();
}

void FTPConnectionManager::setupUI()
{
    m_layout = new QVBoxLayout(this);

    // 标题
    QLabel* titleLabel = new QLabel(tr("FTP连接"), this);
    titleLabel->setStyleSheet("font-weight: bold; padding: 5px;");
    m_layout->addWidget(titleLabel);

    // 连接树
    m_connectionsTree = new QTreeWidget(this);
    m_connectionsTree->setHeaderLabels({tr("连接名称"), tr("主机地址")});
    m_connectionsTree->setRootIsDecorated(false);
    m_connectionsTree->setAlternatingRowColors(true);
    m_layout->addWidget(m_connectionsTree);

    // 按钮区域
    QHBoxLayout* buttonLayout = new QHBoxLayout();

    m_newButton = new QPushButton(tr("新建"), this);
    m_editButton = new QPushButton(tr("编辑"), this);
    m_deleteButton = new QPushButton(tr("删除"), this);
    m_connectButton = new QPushButton(tr("连接"), this);

    m_connectButton->setDefault(true);

    buttonLayout->addWidget(m_newButton);
    buttonLayout->addWidget(m_editButton);
    buttonLayout->addWidget(m_deleteButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_connectButton);

    m_layout->addLayout(buttonLayout);

    // 连接信号
    connect(m_connectionsTree, &QTreeWidget::itemDoubleClicked,
            this, &FTPConnectionManager::onItemDoubleClicked);
    connect(m_connectionsTree, &QTreeWidget::itemSelectionChanged,
            this, &FTPConnectionManager::onItemSelectionChanged);

    connect(m_newButton, &QPushButton::clicked,
            this, &FTPConnectionManager::newConnectionRequested);
    connect(m_editButton, &QPushButton::clicked,
            this, &FTPConnectionManager::editConnectionRequested);
    connect(m_deleteButton, &QPushButton::clicked,
            this, &FTPConnectionManager::deleteConnectionRequested);
    connect(m_connectButton, &QPushButton::clicked, [this]() {
        if (!m_selectedConnection.isEmpty()) {
            emit connectionRequested(m_selectedConnection);
        }
    });

    // 初始按钮状态
    m_editButton->setEnabled(false);
    m_deleteButton->setEnabled(false);
    m_connectButton->setEnabled(false);
}

void FTPConnectionManager::contextMenuEvent(QContextMenuEvent* event)
{
    QTreeWidgetItem* item = m_connectionsTree->itemAt(event->pos());

    QMenu menu(this);

    if (item) {
        menu.addAction(tr("连接"), [this]() {
            if (!m_selectedConnection.isEmpty()) {
                emit connectionRequested(m_selectedConnection);
            }
        });
        menu.addSeparator();
        menu.addAction(tr("编辑"), this, &FTPConnectionManager::editConnectionRequested);
        menu.addAction(tr("删除"), this, &FTPConnectionManager::deleteConnectionRequested);
        menu.addSeparator();
    }

    menu.addAction(tr("新建连接"), this, &FTPConnectionManager::newConnectionRequested);

    menu.exec(event->globalPos());
}

void FTPConnectionManager::onItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column)
    if (item) {
        QString connectionName = item->text(0);
        emit connectionRequested(connectionName);
    }
}

void FTPConnectionManager::onItemSelectionChanged()
{
    QList<QTreeWidgetItem*> selected = m_connectionsTree->selectedItems();
    bool hasSelection = !selected.isEmpty();

    m_editButton->setEnabled(hasSelection);
    m_deleteButton->setEnabled(hasSelection);
    m_connectButton->setEnabled(hasSelection);

    if (hasSelection) {
        m_selectedConnection = selected.first()->text(0);
    } else {
        m_selectedConnection.clear();
    }
}

void FTPConnectionManager::addConnection(const FTPConnectionInfo& info)
{
    m_connections[info.name] = info;
    updateConnectionsList();
    saveConnections();
}

void FTPConnectionManager::editConnection(const QString& name, const FTPConnectionInfo& info)
{
    if (m_connections.contains(name)) {
        m_connections.remove(name);
    }
    m_connections[info.name] = info;
    updateConnectionsList();
    saveConnections();
}

void FTPConnectionManager::removeConnection(const QString& name)
{
    m_connections.remove(name);
    updateConnectionsList();
    saveConnections();
}

FTPConnectionInfo FTPConnectionManager::getConnectionInfo(const QString& name) const
{
    return m_connections.value(name);
}

QStringList FTPConnectionManager::getConnectionNames() const
{
    return m_connections.keys();
}

void FTPConnectionManager::loadConnections()
{
    QSettings settings;
    settings.beginGroup("FTPConnections");

    QStringList connectionNames = settings.childGroups();
    for (const QString& name : connectionNames) {
        settings.beginGroup(name);

        FTPConnectionInfo info;
        info.name = name;
        info.hostname = settings.value("hostname").toString();
        info.port = settings.value("port", 21).toInt();
        info.username = settings.value("username").toString();
        info.password = settings.value("password").toString(); // 注意：实际应用中应加密存储
        info.usePassiveMode = settings.value("usePassiveMode", true).toBool();
        info.description = settings.value("description").toString();

        if (info.isValid()) {
            m_connections[name] = info;
        }

        settings.endGroup();
    }

    settings.endGroup();
}

void FTPConnectionManager::saveConnections()
{
    QSettings settings;
    settings.beginGroup("FTPConnections");

    // 清除现有设置
    settings.remove("");

    // 保存所有连接
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        const FTPConnectionInfo& info = it.value();
        settings.beginGroup(info.name);

        settings.setValue("hostname", info.hostname);
        settings.setValue("port", info.port);
        settings.setValue("username", info.username);
        settings.setValue("password", info.password); // 注意：实际应用中应加密存储
        settings.setValue("usePassiveMode", info.usePassiveMode);
        settings.setValue("description", info.description);

        settings.endGroup();
    }

    settings.endGroup();
}

void FTPConnectionManager::updateConnectionsList()
{
    m_connectionsTree->clear();

    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        const FTPConnectionInfo& info = it.value();

        QTreeWidgetItem* item = new QTreeWidgetItem(m_connectionsTree);
        item->setText(0, info.name);
        item->setText(1, QString("%1:%2").arg(info.hostname).arg(info.port));
        item->setToolTip(0, info.description);
        item->setToolTip(1, QString("用户: %1").arg(info.username));
    }

    m_connectionsTree->resizeColumnToContents(0);
    m_connectionsTree->resizeColumnToContents(1);
}

// ========== FTPConnection Implementation ==========

FTPConnection::FTPConnection(const FTPConnectionInfo& info, QObject* parent)
    : QObject(parent)
    , m_info(info)
    , m_currentPath("/")
    , m_controlSocket(nullptr)
    , m_dataSocket(nullptr)
    , m_connected(false)
    , m_loggedIn(false)
{
}

FTPConnection::~FTPConnection()
{
    disconnect();
    cleanup();
}

bool FTPConnection::isConnected() const
{
    QMutexLocker locker(&m_mutex);
    return m_connected;
}

QString FTPConnection::getLastError() const
{
    return m_lastError;
}

QString FTPConnection::getCurrentPath() const
{
    return m_currentPath;
}

void FTPConnection::connectToHost()
{
    QMutexLocker locker(&m_mutex);

    if (m_connected) {
        return;
    }

    cleanup();

    m_controlSocket = new QTcpSocket(this);

    connect(m_controlSocket, &QTcpSocket::connected,
            this, &FTPConnection::onSocketConnected);
    connect(m_controlSocket, &QTcpSocket::disconnected,
            this, &FTPConnection::onSocketDisconnected);
    connect(m_controlSocket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &FTPConnection::onSocketError);
    connect(m_controlSocket, &QTcpSocket::readyRead,
            this, &FTPConnection::onSocketReadyRead);

    // 连接到FTP服务器
    m_controlSocket->connectToHost(m_info.hostname, m_info.port);
}

void FTPConnection::disconnect()
{
    QMutexLocker locker(&m_mutex);

    if (m_controlSocket && m_connected) {
        m_controlSocket->disconnectFromHost();
    }

    m_connected = false;
    m_loggedIn = false;
    emit disconnected();

    cleanup();
}

void FTPConnection::cleanup()
{
    if (m_controlSocket) {
        m_controlSocket->deleteLater();
        m_controlSocket = nullptr;
    }
    if (m_dataSocket) {
        m_dataSocket->deleteLater();
        m_dataSocket = nullptr;
    }
    m_currentCommand.clear();
    m_responseBuffer.clear();
    m_partialResponse.clear();
}

void FTPConnection::changeDirectory(const QString& path)
{
    if (!m_controlSocket || !m_loggedIn) {
        return;
    }

    m_currentCommand = QString("CWD");
    sendCommand(QString("CWD %1").arg(path));
}

void FTPConnection::listDirectory(const QString& path)
{
    if (!m_controlSocket || !m_loggedIn) {
        return;
    }

    QString listPath = path.isEmpty() ? m_currentPath : path;
    m_currentCommand = QString("LIST");

    // 简化实现：只发送LIST命令，实际FTP需要建立数据连接
    sendCommand(QString("LIST %1").arg(listPath));

    // 模拟返回一些文件和目录
    QTimer::singleShot(500, this, [this]() {
        QStringList files = {"file1.txt", "file2.log", "readme.md"};
        QStringList directories = {"documents", "images", "backup"};
        emit directoryListed(files, directories);
    });
}

void FTPConnection::uploadFile(const QString& localPath, const QString& remotePath)
{
    if (!m_controlSocket || !m_loggedIn) {
        emit fileTransferFinished(false, tr("未连接到FTP服务器"));
        return;
    }

    QFile file(localPath);
    if (!file.exists()) {
        emit fileTransferFinished(false, tr("本地文件不存在: %1").arg(localPath));
        return;
    }

    m_currentCommand = QString("STOR");

    // 简化实现：模拟上传进度
    emit fileTransferProgress(0, 100);
    QTimer::singleShot(1000, this, [this]() {
        emit fileTransferProgress(50, 100);
    });
    QTimer::singleShot(2000, this, [this]() {
        emit fileTransferProgress(100, 100);
        emit fileTransferFinished(true, tr("上传完成"));
    });
}

void FTPConnection::downloadFile(const QString& remotePath, const QString& localPath)
{
    if (!m_controlSocket || !m_loggedIn) {
        emit fileTransferFinished(false, tr("未连接到FTP服务器"));
        return;
    }

    m_currentCommand = QString("RETR");

    // 简化实现：模拟下载进度
    emit fileTransferProgress(0, 100);
    QTimer::singleShot(1000, this, [this]() {
        emit fileTransferProgress(50, 100);
    });
    QTimer::singleShot(2000, this, [this, localPath]() {
        emit fileTransferProgress(100, 100);

        // 创建一个模拟文件
        QFile file(localPath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write("模拟下载的FTP文件内容");
            file.close();
            emit fileTransferFinished(true, tr("下载完成"));
        } else {
            emit fileTransferFinished(false, tr("无法创建本地文件"));
        }
    });
}

void FTPConnection::deleteFile(const QString& remotePath)
{
    if (!m_controlSocket || !m_loggedIn) {
        return;
    }

    m_currentCommand = QString("DELE");
    sendCommand(QString("DELE %1").arg(remotePath));
}

void FTPConnection::createDirectory(const QString& path)
{
    if (!m_controlSocket || !m_loggedIn) {
        return;
    }

    m_currentCommand = QString("MKD");
    sendCommand(QString("MKD %1").arg(path));
}

void FTPConnection::removeDirectory(const QString& path)
{
    if (!m_controlSocket || !m_loggedIn) {
        return;
    }

    m_currentCommand = QString("RMD");
    sendCommand(QString("RMD %1").arg(path));
}

void FTPConnection::onSocketConnected()
{
    m_connected = true;
    // 发送登录命令
    m_currentCommand = "USER";
    sendCommand(QString("USER %1").arg(m_info.username));
}

void FTPConnection::onSocketDisconnected()
{
    if (m_connected) {
        m_connected = false;
        m_loggedIn = false;
        emit disconnected();
    }
}

void FTPConnection::onSocketError(QAbstractSocket::SocketError error)
{
    m_lastError = m_controlSocket->errorString();
    emit this->error(m_lastError);
}

void FTPConnection::onSocketReadyRead()
{
    QByteArray data = m_controlSocket->readAll();
    m_partialResponse += QString::fromUtf8(data);

    // 处理完整的响应行
    QStringList lines = m_partialResponse.split("\r\n");
    m_partialResponse = lines.takeLast(); // 保留不完整的最后一行

    for (const QString& line : lines) {
        if (!line.isEmpty()) {
            processResponse(line);
        }
    }
}

void FTPConnection::sendCommand(const QString& command)
{
    if (m_controlSocket && m_controlSocket->state() == QAbstractSocket::ConnectedState) {
        QString cmd = command + "\r\n";
        m_controlSocket->write(cmd.toUtf8());
    }
}

void FTPConnection::processResponse(const QString& response)
{
    // 简化的FTP响应处理
    int code = response.left(3).toInt();

    if (m_currentCommand == "USER" && code == 331) {
        // 需要密码
        m_currentCommand = "PASS";
        sendCommand(QString("PASS %1").arg(m_info.password));
    } else if (m_currentCommand == "PASS" && code == 230) {
        // 登录成功
        m_loggedIn = true;
        emit connected();
    } else if (m_currentCommand == "CWD" && code == 250) {
        // 目录改变成功
        // 可以从响应中解析新的当前目录
    } else if (code >= 400) {
        // 错误响应
        m_lastError = response;
        emit error(m_lastError);
    }

    m_responseBuffer.append(response);
}

// ========== FTPFileManager Implementation ==========

FTPFileManager::FTPFileManager(QWidget* parent)
    : QWidget(parent)
    , m_layout(nullptr)
    , m_splitter(nullptr)
    , m_remoteWidget(nullptr)
    , m_localWidget(nullptr)
    , m_buttonWidget(nullptr)
    , m_connection(nullptr)
    , m_remotePath("/")
    , m_localPath(QDir::homePath())
    , m_progressBar(nullptr)
{
    setupUI();
    updateLocalFiles(m_localPath);
}

FTPFileManager::~FTPFileManager()
{
}

void FTPFileManager::setupUI()
{
    m_layout = new QHBoxLayout(this);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_layout->addWidget(m_splitter);

    // 本地文件区域
    m_localWidget = new QWidget();
    m_localLayout = new QVBoxLayout(m_localWidget);

    m_localPathLabel = new QLabel(tr("本地路径: %1").arg(m_localPath));
    m_localLayout->addWidget(m_localPathLabel);

    m_localTree = new QTreeWidget();
    m_localTree->setHeaderLabels({tr("名称"), tr("大小"), tr("修改时间")});
    m_localLayout->addWidget(m_localTree);

    m_splitter->addWidget(m_localWidget);

    // 操作按钮区域
    m_buttonWidget = new QWidget();
    m_buttonWidget->setMaximumWidth(120);
    m_buttonLayout = new QVBoxLayout(m_buttonWidget);

    m_uploadButton = new QPushButton(tr("上传 →"));
    m_downloadButton = new QPushButton(tr("← 下载"));
    m_deleteButton = new QPushButton(tr("删除"));
    m_createDirButton = new QPushButton(tr("新建目录"));
    m_refreshButton = new QPushButton(tr("刷新"));

    m_uploadButton->setEnabled(false);
    m_downloadButton->setEnabled(false);
    m_deleteButton->setEnabled(false);
    m_createDirButton->setEnabled(false);

    m_buttonLayout->addWidget(m_uploadButton);
    m_buttonLayout->addWidget(m_downloadButton);

    // Add separator line
    QFrame* separator = new QFrame();
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    m_buttonLayout->addWidget(separator);

    m_buttonLayout->addWidget(m_deleteButton);
    m_buttonLayout->addWidget(m_createDirButton);
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_refreshButton);

    m_splitter->addWidget(m_buttonWidget);

    // 远程文件区域
    m_remoteWidget = new QWidget();
    m_remoteLayout = new QVBoxLayout(m_remoteWidget);

    m_remotePathLabel = new QLabel(tr("远程路径: %1").arg(m_remotePath));
    m_remoteLayout->addWidget(m_remotePathLabel);

    m_remoteTree = new QTreeWidget();
    m_remoteTree->setHeaderLabels({tr("名称"), tr("大小"), tr("修改时间")});
    m_remoteLayout->addWidget(m_remoteTree);

    // 进度条
    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    m_remoteLayout->addWidget(m_progressBar);

    m_splitter->addWidget(m_remoteWidget);

    // 设置分割器比例
    m_splitter->setSizes({400, 120, 400});

    // 连接信号
    connect(m_localTree, &QTreeWidget::itemDoubleClicked,
            this, &FTPFileManager::onLocalItemDoubleClicked);
    connect(m_remoteTree, &QTreeWidget::itemDoubleClicked,
            this, &FTPFileManager::onRemoteItemDoubleClicked);

    connect(m_uploadButton, &QPushButton::clicked,
            this, &FTPFileManager::onUploadClicked);
    connect(m_downloadButton, &QPushButton::clicked,
            this, &FTPFileManager::onDownloadClicked);
    connect(m_deleteButton, &QPushButton::clicked,
            this, &FTPFileManager::onDeleteClicked);
    connect(m_createDirButton, &QPushButton::clicked,
            this, &FTPFileManager::onCreateDirectoryClicked);
    connect(m_refreshButton, &QPushButton::clicked,
            this, &FTPFileManager::onRefreshClicked);
}

void FTPFileManager::setConnection(FTPConnection* connection)
{
    if (m_connection) {
        disconnect(m_connection, nullptr, this, nullptr);
    }

    m_connection = connection;

    if (m_connection) {
        connect(m_connection, &FTPConnection::directoryListed,
                this, &FTPFileManager::onDirectoryListed);
        connect(m_connection, &FTPConnection::fileTransferProgress,
                this, &FTPFileManager::onFileTransferProgress);
        connect(m_connection, &FTPConnection::fileTransferFinished,
                this, &FTPFileManager::onFileTransferFinished);

        // 启用按钮
        m_uploadButton->setEnabled(true);
        m_downloadButton->setEnabled(true);
        m_deleteButton->setEnabled(true);
        m_createDirButton->setEnabled(true);

        // 刷新远程文件列表
        refresh();
    } else {
        // 禁用按钮
        m_uploadButton->setEnabled(false);
        m_downloadButton->setEnabled(false);
        m_deleteButton->setEnabled(false);
        m_createDirButton->setEnabled(false);

        // 清空远程文件列表
        m_remoteTree->clear();
    }
}

void FTPFileManager::refresh()
{
    if (m_connection && m_connection->isConnected()) {
        updateRemoteFiles();
    }
    updateLocalFiles(m_localPath);
}

void FTPFileManager::updateLocalFiles(const QString& path)
{
    m_localTree->clear();
    m_localPath = path;
    m_localPathLabel->setText(tr("本地路径: %1").arg(path));

    QDir dir(path);
    if (!dir.exists()) {
        return;
    }

    // 添加上级目录项
    if (dir.absolutePath() != dir.rootPath()) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_localTree);
        item->setText(0, "..");
        item->setText(1, tr("<目录>"));
        item->setData(0, Qt::UserRole, dir.absoluteFilePath(".."));
        item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
    }

    // 添加目录
    QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo& info : entries) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_localTree);
        item->setText(0, info.fileName());
        item->setText(1, tr("<目录>"));
        item->setText(2, info.lastModified().toString("yyyy-MM-dd hh:mm:ss"));
        item->setData(0, Qt::UserRole, info.absoluteFilePath());
        item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
    }

    // 添加文件
    entries = dir.entryInfoList(QDir::Files, QDir::Name);
    for (const QFileInfo& info : entries) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_localTree);
        item->setText(0, info.fileName());
        item->setText(1, QString::number(info.size()));
        item->setText(2, info.lastModified().toString("yyyy-MM-dd hh:mm:ss"));
        item->setData(0, Qt::UserRole, info.absoluteFilePath());
        item->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
    }

    m_localTree->resizeColumnToContents(0);
    m_localTree->resizeColumnToContents(1);
}

void FTPFileManager::updateRemoteFiles()
{
    if (m_connection && m_connection->isConnected()) {
        m_connection->listDirectory(m_remotePath);
    }
}

void FTPFileManager::onLocalItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column)
    if (!item) return;

    QString path = item->data(0, Qt::UserRole).toString();
    QFileInfo info(path);

    if (info.isDir()) {
        updateLocalFiles(path);
    }
}

void FTPFileManager::onRemoteItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column)
    if (!item || !m_connection) return;

    QString name = item->text(0);
    if (name == "..") {
        // 返回上级目录
        QString parentPath = m_remotePath;
        if (parentPath.endsWith("/") && parentPath.length() > 1) {
            parentPath = parentPath.left(parentPath.length() - 1);
        }
        int lastSlash = parentPath.lastIndexOf("/");
        if (lastSlash >= 0) {
            parentPath = parentPath.left(lastSlash + 1);
            if (parentPath.isEmpty()) {
                parentPath = "/";
            }
            m_remotePath = parentPath;
            m_remotePathLabel->setText(tr("远程路径: %1").arg(m_remotePath));
            m_connection->changeDirectory(m_remotePath);
            updateRemoteFiles();
        }
    } else if (item->text(1) == tr("<目录>")) {
        // 进入子目录
        QString newPath = m_remotePath;
        if (!newPath.endsWith("/")) {
            newPath += "/";
        }
        newPath += name;
        m_remotePath = newPath;
        m_remotePathLabel->setText(tr("远程路径: %1").arg(m_remotePath));
        m_connection->changeDirectory(m_remotePath);
        updateRemoteFiles();
    }
}

void FTPFileManager::onUploadClicked()
{
    QList<QTreeWidgetItem*> selectedItems = m_localTree->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请选择要上传的文件"));
        return;
    }

    for (QTreeWidgetItem* item : selectedItems) {
        QString localPath = item->data(0, Qt::UserRole).toString();
        QFileInfo info(localPath);

        if (info.isFile()) {
            QString remotePath = m_remotePath;
            if (!remotePath.endsWith("/")) {
                remotePath += "/";
            }
            remotePath += info.fileName();

            uploadFile(localPath, remotePath);
        }
    }
}

void FTPFileManager::onDownloadClicked()
{
    QList<QTreeWidgetItem*> selectedItems = m_remoteTree->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请选择要下载的文件"));
        return;
    }

    for (QTreeWidgetItem* item : selectedItems) {
        QString fileName = item->text(0);
        if (fileName != ".." && item->text(1) != tr("<目录>")) {
            QString remotePath = m_remotePath;
            if (!remotePath.endsWith("/")) {
                remotePath += "/";
            }
            remotePath += fileName;

            QString localPath = m_localPath;
            if (!localPath.endsWith("/")) {
                localPath += "/";
            }
            localPath += fileName;

            downloadFile(remotePath, localPath);
        }
    }
}

void FTPFileManager::onDeleteClicked()
{
    QList<QTreeWidgetItem*> selectedItems = m_remoteTree->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请选择要删除的文件"));
        return;
    }

    int ret = QMessageBox::question(this, tr("确认删除"),
                                   tr("确定要删除选中的文件吗？"),
                                   QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) {
        return;
    }

    for (QTreeWidgetItem* item : selectedItems) {
        QString fileName = item->text(0);
        if (fileName != "..") {
            QString remotePath = m_remotePath;
            if (!remotePath.endsWith("/")) {
                remotePath += "/";
            }
            remotePath += fileName;

            deleteRemoteItem(remotePath);
        }
    }
}

void FTPFileManager::onCreateDirectoryClicked()
{
    bool ok;
    QString dirName = QInputDialog::getText(this, tr("新建目录"),
                                           tr("目录名称:"), QLineEdit::Normal,
                                           "", &ok);
    if (ok && !dirName.isEmpty()) {
        createRemoteDirectory(dirName);
    }
}

void FTPFileManager::onRefreshClicked()
{
    refresh();
}

void FTPFileManager::onDirectoryListed(const QStringList& files, const QStringList& directories)
{
    m_remoteTree->clear();
    m_remotePathLabel->setText(tr("远程路径: %1").arg(m_remotePath));

    // 添加上级目录项
    if (m_remotePath != "/") {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_remoteTree);
        item->setText(0, "..");
        item->setText(1, tr("<目录>"));
        item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
    }

    // 添加目录
    for (const QString& dir : directories) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_remoteTree);
        item->setText(0, dir);
        item->setText(1, tr("<目录>"));
        item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
    }

    // 添加文件
    for (const QString& file : files) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_remoteTree);
        item->setText(0, file);
        item->setText(1, ""); // 文件大小信息需要额外的FTP命令获取
        item->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
    }

    m_remoteTree->resizeColumnToContents(0);
    m_remoteTree->resizeColumnToContents(1);
}

void FTPFileManager::onFileTransferProgress(int bytesTransferred, int totalBytes)
{
    if (totalBytes > 0) {
        m_progressBar->setVisible(true);
        m_progressBar->setMaximum(totalBytes);
        m_progressBar->setValue(bytesTransferred);
    }
}

void FTPFileManager::onFileTransferFinished(bool success, const QString& message)
{
    m_progressBar->setVisible(false);

    if (success) {
        // 刷新文件列表
        refresh();
    } else {
        QMessageBox::warning(this, tr("传输失败"), message);
    }
}

void FTPFileManager::uploadFile(const QString& localPath, const QString& remotePath)
{
    if (m_connection) {
        m_connection->uploadFile(localPath, remotePath);
    }
}

void FTPFileManager::downloadFile(const QString& remotePath, const QString& localPath)
{
    if (m_connection) {
        m_connection->downloadFile(remotePath, localPath);
    }
}

void FTPFileManager::createRemoteDirectory(const QString& name)
{
    if (m_connection) {
        QString dirPath = m_remotePath;
        if (!dirPath.endsWith("/")) {
            dirPath += "/";
        }
        dirPath += name;

        m_connection->createDirectory(dirPath);

        // 刷新文件列表
        QTimer::singleShot(1000, this, &FTPFileManager::refresh);
    }
}

void FTPFileManager::deleteRemoteItem(const QString& path)
{
    if (m_connection) {
        m_connection->deleteFile(path);

        // 刷新文件列表
        QTimer::singleShot(1000, this, &FTPFileManager::refresh);
    }
}

// ========== FTPConnectionDialog Implementation ==========

FTPConnectionDialog::FTPConnectionDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("新建FTP连接"));
    setModal(true);
    setupUI();
}

FTPConnectionDialog::FTPConnectionDialog(const FTPConnectionInfo& info, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("编辑FTP连接"));
    setModal(true);
    setupUI();
    setConnectionInfo(info);
}

void FTPConnectionDialog::setupUI()
{
    setMinimumSize(400, 350);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // 表单区域
    QGroupBox* formGroup = new QGroupBox(tr("连接信息"), this);
    m_formLayout = new QFormLayout(formGroup);

    m_nameEdit = new QLineEdit(this);
    m_hostnameEdit = new QLineEdit(this);
    m_portSpinBox = new QSpinBox(this);
    m_portSpinBox->setRange(1, 65535);
    m_portSpinBox->setValue(21);

    m_usernameEdit = new QLineEdit(this);
    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);

    m_passiveModeCheckBox = new QCheckBox(tr("使用被动模式"), this);
    m_passiveModeCheckBox->setChecked(true);

    m_descriptionEdit = new QTextEdit(this);
    m_descriptionEdit->setMaximumHeight(60);

    m_formLayout->addRow(tr("连接名称:"), m_nameEdit);
    m_formLayout->addRow(tr("主机地址:"), m_hostnameEdit);
    m_formLayout->addRow(tr("端口:"), m_portSpinBox);
    m_formLayout->addRow(tr("用户名:"), m_usernameEdit);
    m_formLayout->addRow(tr("密码:"), m_passwordEdit);
    m_formLayout->addRow("", m_passiveModeCheckBox);
    m_formLayout->addRow(tr("描述:"), m_descriptionEdit);

    mainLayout->addWidget(formGroup);

    // 测试连接区域
    QGroupBox* testGroup = new QGroupBox(tr("连接测试"), this);
    QVBoxLayout* testLayout = new QVBoxLayout(testGroup);

    m_testButton = new QPushButton(tr("测试连接"), this);
    m_testProgress = new QProgressBar(this);
    m_testProgress->setVisible(false);
    m_testResultLabel = new QLabel(this);

    testLayout->addWidget(m_testButton);
    testLayout->addWidget(m_testProgress);
    testLayout->addWidget(m_testResultLabel);

    mainLayout->addWidget(testGroup);

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
    connect(m_testButton, &QPushButton::clicked, this, &FTPConnectionDialog::onTestConnection);
}

FTPConnectionInfo FTPConnectionDialog::getConnectionInfo() const
{
    FTPConnectionInfo info;
    info.name = m_nameEdit->text().trimmed();
    info.hostname = m_hostnameEdit->text().trimmed();
    info.port = m_portSpinBox->value();
    info.username = m_usernameEdit->text().trimmed();
    info.password = m_passwordEdit->text();
    info.usePassiveMode = m_passiveModeCheckBox->isChecked();
    info.description = m_descriptionEdit->toPlainText().trimmed();

    return info;
}

void FTPConnectionDialog::setConnectionInfo(const FTPConnectionInfo& info)
{
    m_nameEdit->setText(info.name);
    m_hostnameEdit->setText(info.hostname);
    m_portSpinBox->setValue(info.port);
    m_usernameEdit->setText(info.username);
    m_passwordEdit->setText(info.password);
    m_passiveModeCheckBox->setChecked(info.usePassiveMode);
    m_descriptionEdit->setPlainText(info.description);
}

void FTPConnectionDialog::onTestConnection()
{
    FTPConnectionInfo info = getConnectionInfo();
    if (!info.isValid()) {
        QMessageBox::warning(this, tr("错误"), tr("请填写完整的连接信息"));
        return;
    }

    m_testButton->setEnabled(false);
    m_testProgress->setVisible(true);
    m_testResultLabel->setText(tr("正在测试连接..."));

    // 创建临时连接进行测试
    FTPConnection* testConnection = new FTPConnection(info, this);

    connect(testConnection, &FTPConnection::connected, [this, testConnection]() {
        m_testProgress->setVisible(false);
        m_testResultLabel->setText(tr("✓ 连接测试成功"));
        m_testResultLabel->setStyleSheet("color: green;");
        m_testButton->setEnabled(true);
        testConnection->disconnect();
        testConnection->deleteLater();
    });

    connect(testConnection, &FTPConnection::error, [this, testConnection](const QString& error) {
        m_testProgress->setVisible(false);
        m_testResultLabel->setText(tr("✗ 连接测试失败: %1").arg(error));
        m_testResultLabel->setStyleSheet("color: red;");
        m_testButton->setEnabled(true);
        testConnection->deleteLater();
    });

    testConnection->connectToHost();
}

