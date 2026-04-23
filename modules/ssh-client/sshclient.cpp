#include "sshclient.h"
#include <QStandardPaths>
#include <QDir>
#include <QSplitter>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QStatusBar>

REGISTER_DYNAMICOBJECT(SSHClient);

SSHClient::SSHClient(QWidget* parent)
    : QWidget(parent)
    , DynamicObjectBase()
    , m_mainSplitter(nullptr)
    , m_connectionManager(nullptr)
    , m_rightTabs(nullptr)
    , m_terminal(nullptr)
    , m_sftpBrowser(nullptr)
    , m_menuBar(nullptr)
    , m_statusBar(nullptr)
    , m_isConnected(false)
{
    setupUI();
    loadSettings();

#ifdef WITH_LIBSSH
    // 初始化libssh
    ssh_init();
    qDebug() << "LibSSH initialized. Version:" << ssh_version(0);
#else
    QMessageBox::warning(this, tr("警告"),
        tr("LibSSH支持未启用。SSH功能将受限。\n请在编译时启用WITH_LIBSSH选项。"));
#endif
}

SSHClient::~SSHClient()
{
    saveSettings();

#ifdef WITH_LIBSSH
    ssh_finalize();
#endif
}

void SSHClient::setupUI()
{
    setWindowTitle(tr("SSH客户端"));
    // 主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);

#ifndef Q_OS_MAC
    // 菜单栏
    setupMenuBar();
    mainLayout->addWidget(m_menuBar);
#endif

    // 主分割器
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);

    // 左侧：连接管理器
    m_connectionManager = new ConnectionManager(this);
    m_connectionManager->setMaximumWidth(300);
    m_connectionManager->setMinimumWidth(250);

    // 右侧：标签页容器
    m_rightTabs = new QTabWidget(this);

    // SSH终端标签页
    m_terminal = new SSHTerminal(this);
    m_rightTabs->addTab(m_terminal, tr("SSH终端"));

    // SFTP浏览器标签页
    m_sftpBrowser = new SFTPBrowser(this);
    m_rightTabs->addTab(m_sftpBrowser, tr("SFTP文件管理"));

    // 添加到分割器
    m_mainSplitter->addWidget(m_connectionManager);
    m_mainSplitter->addWidget(m_rightTabs);
    m_mainSplitter->setStretchFactor(0, 0);
    m_mainSplitter->setStretchFactor(1, 1);

    mainLayout->addWidget(m_mainSplitter);

    // 状态栏（用 QLabel 替代 QStatusBar，更紧凑）
    m_statusBar = new QStatusBar(this);
    m_statusBar->setFixedHeight(22);
    m_statusBar->setStyleSheet("QStatusBar { border-top: 1px solid #e9ecef; font-size: 8pt; color: #868e96; }");
    mainLayout->addWidget(m_statusBar);

    // 连接信号
    connect(m_connectionManager, &ConnectionManager::connectionRequested,
            this, &SSHClient::onConnectionRequested);
    connect(m_connectionManager, &ConnectionManager::newConnectionRequested,
            this, &SSHClient::onNewConnection);
    connect(m_connectionManager, &ConnectionManager::editConnectionRequested,
            this, &SSHClient::onEditConnection);
    connect(m_connectionManager, &ConnectionManager::deleteConnectionRequested,
            this, &SSHClient::onDeleteConnection);

    m_statusBar->showMessage(tr("就绪"));
}

void SSHClient::setupMenuBar()
{
    m_menuBar = new QMenuBar(this);

    // 连接菜单
    QMenu* connectionMenu = m_menuBar->addMenu(tr("连接(&C)"));

    QAction* newConnectionAction = connectionMenu->addAction(tr("新建连接(&N)"));
    newConnectionAction->setShortcut(QKeySequence::New);
    connect(newConnectionAction, &QAction::triggered, this, &SSHClient::onNewConnection);

    QAction* editConnectionAction = connectionMenu->addAction(tr("编辑连接(&E)"));
    connect(editConnectionAction, &QAction::triggered, this, &SSHClient::onEditConnection);

    connectionMenu->addSeparator();

    QAction* disconnectAction = connectionMenu->addAction(tr("断开连接(&D)"));
    disconnectAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    connect(disconnectAction, &QAction::triggered, this, &SSHClient::onDisconnect);

    connectionMenu->addSeparator();

    QAction* exitAction = connectionMenu->addAction(tr("退出(&X)"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    // 帮助菜单
    QMenu* helpMenu = m_menuBar->addMenu(tr("帮助(&H)"));

    QAction* aboutAction = helpMenu->addAction(tr("关于(&A)"));
    connect(aboutAction, &QAction::triggered, [this]() {
#ifdef WITH_LIBSSH
        QString libsshVersion = QString::fromUtf8(ssh_version(0));
#else
        QString libsshVersion = tr("未编译");
#endif
        QMessageBox::about(this, tr("关于SSH客户端"),
            tr("<h3>SSH客户端</h3>"
               "<p>基于LibSSH的SSH连接和SFTP文件管理工具</p>"
               "<p><b>功能特性：</b></p>"
               "<ul>"
               "<li>SSH远程终端连接</li>"
               "<li>SFTP文件传输</li>"
               "<li>连接配置管理</li>"
               "<li>密钥认证支持</li>"
               "</ul>"
               "<p><b>LibSSH版本：</b> %1</p>").arg(libsshVersion));
    });
}

void SSHClient::onConnectionRequested(const QString& name)
{
    if (m_isConnected) {
        int result = QMessageBox::question(this, tr("确认"),
            tr("当前已有连接，是否断开并建立新连接？"),
            QMessageBox::Yes | QMessageBox::No);
        if (result != QMessageBox::Yes) {
            return;
        }
        onDisconnect();
    }

    SSHConnectionInfo info = m_connectionManager->getConnectionInfo(name);
    if (!info.isValid()) {
        QMessageBox::warning(this, tr("错误"), tr("连接信息无效"));
        return;
    }

    m_statusBar->showMessage(tr("正在连接到 %1...").arg(info.hostname));
    QApplication::processEvents(); // 让状态栏消息立刻显示
    m_currentConnection = name;

    // 创建 SSH 连接（同步阻塞，但设了 10 秒超时）
    auto* connection = new SSHConnection(info, this);
    connection->connectToHost();

    if (connection->isConnected()) {
        onConnectionEstablished(name);
        m_terminal->setConnection(connection);
        m_sftpBrowser->setConnection(connection);

        connect(connection, &SSHConnection::disconnected, this, [this, name]() {
            onConnectionClosed(name);
            m_terminal->setConnection(nullptr);
            m_sftpBrowser->setConnection(nullptr);
        });
    } else {
        onConnectionError(name, connection->getLastError());
        connection->deleteLater();
    }
}

void SSHClient::onConnectionEstablished(const QString& name)
{
    m_isConnected = true;
    m_currentConnection = name;

    SSHConnectionInfo info = m_connectionManager->getConnectionInfo(name);
    m_statusBar->showMessage(tr("已连接到 %1@%2").arg(info.username, info.hostname));
}

void SSHClient::onConnectionClosed(const QString& name)
{
    Q_UNUSED(name)
    m_isConnected = false;
    m_currentConnection.clear();
    m_statusBar->showMessage(tr("连接已断开"));
}

void SSHClient::onConnectionError(const QString& name, const QString& error)
{
    Q_UNUSED(name)
    m_statusBar->showMessage(tr("连接错误: %1").arg(error));
    QMessageBox::critical(this, tr("连接错误"), error);
}

void SSHClient::onNewConnection()
{
    SSHConnectionDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        SSHConnectionInfo info = dialog.getConnectionInfo();
        m_connectionManager->addConnection(info);
    }
}

void SSHClient::onEditConnection()
{
    QString selected = m_connectionManager->selectedConnection();
    if (selected.isEmpty()) {
        QMessageBox::warning(this, tr("提示"), tr("请先选择一个连接"));
        return;
    }

    SSHConnectionInfo info = m_connectionManager->getConnectionInfo(selected);
    SSHConnectionDialog dialog(info, this);
    if (dialog.exec() == QDialog::Accepted) {
        SSHConnectionInfo newInfo = dialog.getConnectionInfo();
        m_connectionManager->editConnection(selected, newInfo);
    }
}

void SSHClient::onDeleteConnection()
{
    QString selected = m_connectionManager->selectedConnection();
    if (selected.isEmpty()) {
        QMessageBox::warning(this, tr("提示"), tr("请先选择一个连接"));
        return;
    }

    if (QMessageBox::question(this, tr("确认"),
        tr("确定删除连接 \"%1\" 吗？").arg(selected)) != QMessageBox::Yes)
        return;

    if (m_isConnected && m_currentConnection == selected)
        onDisconnect();

    m_connectionManager->removeConnection(selected);
}

void SSHClient::onDisconnect()
{
    if (m_isConnected) {
        m_isConnected = false;
        m_currentConnection.clear();
        m_statusBar->showMessage(tr("已断开连接"));

        // 清理终端和SFTP浏览器
        m_terminal->clear();
    }
}

void SSHClient::closeEvent(QCloseEvent* event)
{
    if (m_isConnected) {
        int result = QMessageBox::question(this, tr("确认退出"),
            tr("当前有活动连接，确定要退出吗？"),
            QMessageBox::Yes | QMessageBox::No);
        if (result != QMessageBox::Yes) {
            event->ignore();
            return;
        }
        onDisconnect();
    }

    saveSettings();
    event->accept();
}

void SSHClient::loadSettings()
{
    QSettings settings;
    settings.beginGroup("SSHClient");

    // 恢复窗口大小和位置
    if (settings.contains("geometry")) {
        restoreGeometry(settings.value("geometry").toByteArray());
    }

    // 恢复分割器状态
    if (settings.contains("splitter")) {
        m_mainSplitter->restoreState(settings.value("splitter").toByteArray());
    }

    settings.endGroup();
}

void SSHClient::saveSettings()
{
    QSettings settings;
    settings.beginGroup("SSHClient");

    settings.setValue("geometry", saveGeometry());
    settings.setValue("splitter", m_mainSplitter->saveState());

    settings.endGroup();
}

// ========== ConnectionManager Implementation ==========

ConnectionManager::ConnectionManager(QWidget* parent)
    : QWidget(parent)
    , m_layout(nullptr)
    , m_connectionsTree(nullptr)
    , m_selectedConnection("")
{
    setupUI();
    loadConnections();
}

ConnectionManager::~ConnectionManager()
{
    saveConnections();
}

void ConnectionManager::setupUI()
{
    m_layout = new QVBoxLayout(this);

    // 标题
    QLabel* titleLabel = new QLabel(tr("SSH连接"), this);
    titleLabel->setStyleSheet("font-weight: bold; font-size: 12pt; margin: 5px;");
    m_layout->addWidget(titleLabel);

    // 连接列表
    m_connectionsTree = new QTreeWidget(this);
    m_connectionsTree->setHeaderLabels({tr("名称"), tr("主机")});
    m_connectionsTree->setRootIsDecorated(false);
    m_connectionsTree->setAlternatingRowColors(true);
    m_layout->addWidget(m_connectionsTree);

    // 按钮组
    QHBoxLayout* buttonLayout = new QHBoxLayout;

    m_newButton = new QPushButton(tr("新建"), this);
    m_editButton = new QPushButton(tr("编辑"), this);
    m_deleteButton = new QPushButton(tr("删除"), this);
    m_connectButton = new QPushButton(tr("连接"), this);

    m_editButton->setEnabled(false);
    m_deleteButton->setEnabled(false);
    m_connectButton->setEnabled(false);

    buttonLayout->addWidget(m_newButton);
    buttonLayout->addWidget(m_editButton);
    buttonLayout->addWidget(m_deleteButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_connectButton);

    m_layout->addLayout(buttonLayout);

    // 连接信号
    connect(m_connectionsTree, &QTreeWidget::itemDoubleClicked,
            this, &ConnectionManager::onItemDoubleClicked);
    connect(m_connectionsTree, &QTreeWidget::itemSelectionChanged,
            this, &ConnectionManager::onItemSelectionChanged);

    connect(m_newButton, &QPushButton::clicked,
            this, &ConnectionManager::newConnectionRequested);
    connect(m_editButton, &QPushButton::clicked,
            this, &ConnectionManager::editConnectionRequested);
    connect(m_deleteButton, &QPushButton::clicked,
            this, &ConnectionManager::deleteConnectionRequested);
    connect(m_connectButton, &QPushButton::clicked, [this]() {
        if (!m_selectedConnection.isEmpty()) {
            emit connectionRequested(m_selectedConnection);
        }
    });
}

void ConnectionManager::addConnection(const SSHConnectionInfo& info)
{
    m_connections[info.name] = info;
    saveConnections();
    updateConnectionsList();
}

void ConnectionManager::editConnection(const QString& name, const SSHConnectionInfo& info)
{
    if (m_connections.contains(name)) {
        // 如果名称改变了，需要删除旧的
        if (name != info.name) {
            m_connections.remove(name);
        }
        m_connections[info.name] = info;
        saveConnections();
        updateConnectionsList();
    }
}

void ConnectionManager::removeConnection(const QString& name)
{
    m_connections.remove(name);
    saveConnections();
    updateConnectionsList();
}

SSHConnectionInfo ConnectionManager::getConnectionInfo(const QString& name) const
{
    return m_connections.value(name, SSHConnectionInfo());
}

QStringList ConnectionManager::getConnectionNames() const
{
    return m_connections.keys();
}

void ConnectionManager::onItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column)
    if (item) {
        QString name = item->text(0);
        emit connectionRequested(name);
    }
}

void ConnectionManager::onItemSelectionChanged()
{
    QTreeWidgetItem* currentItem = m_connectionsTree->currentItem();
    if (currentItem) {
        m_selectedConnection = currentItem->text(0);
        m_editButton->setEnabled(true);
        m_deleteButton->setEnabled(true);
        m_connectButton->setEnabled(true);
    } else {
        m_selectedConnection.clear();
        m_editButton->setEnabled(false);
        m_deleteButton->setEnabled(false);
        m_connectButton->setEnabled(false);
    }
}

void ConnectionManager::updateConnectionsList()
{
    m_connectionsTree->clear();

    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        const SSHConnectionInfo& info = it.value();
        QTreeWidgetItem* item = new QTreeWidgetItem;
        item->setText(0, info.name);
        item->setText(1, QString("%1:%2").arg(info.hostname).arg(info.port));

        if (!info.description.isEmpty()) {
            item->setToolTip(0, info.description);
            item->setToolTip(1, info.description);
        }

        m_connectionsTree->addTopLevelItem(item);
    }
}

void ConnectionManager::loadConnections()
{
    QSettings settings;
    settings.beginGroup("SSHConnections");

    QStringList connectionNames = settings.childGroups();
    for (const QString& name : connectionNames) {
        settings.beginGroup(name);

        SSHConnectionInfo info;
        info.name = name;
        info.hostname = settings.value("hostname").toString();
        info.port = settings.value("port", 22).toInt();
        info.username = settings.value("username").toString();
        info.password = settings.value("password").toString(); // 注意：安全性问题
        info.privateKeyPath = settings.value("privateKeyPath").toString();
        info.usePrivateKey = settings.value("usePrivateKey", false).toBool();
        info.description = settings.value("description").toString();

        if (info.isValid()) {
            m_connections[name] = info;
        }

        settings.endGroup();
    }

    settings.endGroup();
    updateConnectionsList();
}

void ConnectionManager::saveConnections()
{
    QSettings settings;
    settings.beginGroup("SSHConnections");

    // 清除旧的设置
    settings.remove("");

    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        const SSHConnectionInfo& info = it.value();
        settings.beginGroup(info.name);

        settings.setValue("hostname", info.hostname);
        settings.setValue("port", info.port);
        settings.setValue("username", info.username);
        settings.setValue("password", info.password); // 注意：安全性问题
        settings.setValue("privateKeyPath", info.privateKeyPath);
        settings.setValue("usePrivateKey", info.usePrivateKey);
        settings.setValue("description", info.description);

        settings.endGroup();
    }

    settings.endGroup();
}

void ConnectionManager::contextMenuEvent(QContextMenuEvent* event)
{
    QTreeWidgetItem* item = m_connectionsTree->itemAt(
        m_connectionsTree->mapFromParent(event->pos()));

    QMenu menu(this);

    if (item) {
        menu.addAction(tr("连接"), [this]() {
            if (!m_selectedConnection.isEmpty()) {
                emit connectionRequested(m_selectedConnection);
            }
        });
        menu.addSeparator();
        menu.addAction(tr("编辑"), this, &ConnectionManager::editConnectionRequested);
        menu.addAction(tr("删除"), this, &ConnectionManager::deleteConnectionRequested);
        menu.addSeparator();
    }

    menu.addAction(tr("新建连接"), this, &ConnectionManager::newConnectionRequested);

    menu.exec(event->globalPos());
}

// ========== SSHTerminal Implementation ==========

SSHTerminal::SSHTerminal(QWidget* parent)
    : QWidget(parent)
    , m_connection(nullptr)
    , m_historyIndex(-1)
{
    setupUI();
}

SSHTerminal::~SSHTerminal()
{
}

void SSHTerminal::setupUI()
{
    m_layout = new QVBoxLayout(this);

    // 输出区域
    m_output = new QTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setFont(QFont("Consolas", 9));
    m_output->setStyleSheet(
        "QTextEdit {"
        "  background-color: #1e1e1e;"
        "  color: #ffffff;"
        "  border: 1px solid #555555;"
        "}"
    );
    m_layout->addWidget(m_output);

    // 输入区域
    m_input = new QLineEdit(this);
    m_input->setFont(QFont("Consolas", 9));
    m_input->setStyleSheet(
        "QLineEdit {"
        "  background-color: #2d2d2d;"
        "  color: #ffffff;"
        "  border: 1px solid #555555;"
        "  padding: 2px;"
        "}"
    );
    m_input->setPlaceholderText(tr("请先建立SSH连接..."));
    m_input->setEnabled(false);
    m_layout->addWidget(m_input);

    connect(m_input, &QLineEdit::returnPressed, this, &SSHTerminal::onReturnPressed);

    // 初始化显示
    appendOutput(tr("SSH终端 - 请从左侧连接列表选择并连接到SSH服务器"), Qt::yellow);
}

void SSHTerminal::setConnection(SSHConnection* connection)
{
    m_connection = connection;
    if (m_connection) {
        m_input->setEnabled(true);
        m_input->setPlaceholderText(tr("输入命令..."));
        m_input->setFocus();

        connect(m_connection, &SSHConnection::commandOutput,
                this, &SSHTerminal::onCommandOutput);
        connect(m_connection, &SSHConnection::error,
                this, &SSHTerminal::onConnectionError);

        appendOutput(tr("SSH连接已建立，可以输入命令"), Qt::green);
        m_prompt = "$ ";
        appendOutput(m_prompt, Qt::cyan);
    } else {
        m_input->setEnabled(false);
        m_input->setPlaceholderText(tr("请先建立SSH连接..."));
    }
}

void SSHTerminal::clear()
{
    m_output->clear();
    m_input->clear();
    m_commandHistory.clear();
    m_historyIndex = -1;

    if (!m_connection) {
        appendOutput(tr("SSH终端 - 请从左侧连接列表选择并连接到SSH服务器"), Qt::yellow);
        m_input->setEnabled(false);
    }
}

void SSHTerminal::onReturnPressed()
{
    QString command = m_input->text().trimmed();
    if (command.isEmpty()) {
        return;
    }

    // 添加到历史记录
    m_commandHistory.append(command);
    m_historyIndex = m_commandHistory.size();

    // 显示命令
    appendOutput(m_prompt + command, Qt::white);

    // 执行命令
    processCommand(command);

    m_input->clear();
}

void SSHTerminal::onCommandOutput(const QString& output)
{
    appendOutput(output, Qt::lightGray);
    appendOutput(m_prompt, Qt::cyan);
}

void SSHTerminal::onConnectionError(const QString& error)
{
    appendOutput(tr("错误: %1").arg(error), Qt::red);
    appendOutput(m_prompt, Qt::cyan);
}

void SSHTerminal::appendOutput(const QString& text, const QColor& color)
{
    QTextCursor cursor = m_output->textCursor();
    cursor.movePosition(QTextCursor::End);

    QTextCharFormat format;
    format.setForeground(color);
    cursor.setCharFormat(format);

    cursor.insertText(text + "\n");

    // 滚动到底部
    QScrollBar* scrollBar = m_output->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void SSHTerminal::processCommand(const QString& command)
{
    if (m_connection) {
        // 发送命令到SSH连接
        m_connection->executeCommand(command);
    } else {
        // 模拟一些基本命令
        if (command == "help") {
            appendOutput(tr("可用命令: help, clear, echo"), Qt::yellow);
        } else if (command == "clear") {
            m_output->clear();
        } else if (command.startsWith("echo ")) {
            appendOutput(command.mid(5), Qt::white);
        } else {
            appendOutput(tr("未连接SSH - 命令无法执行: %1").arg(command), Qt::red);
        }
        appendOutput(m_prompt, Qt::cyan);
    }
}

void SSHTerminal::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Up) {
        // 历史记录向上
        if (m_historyIndex > 0) {
            m_historyIndex--;
            m_input->setText(m_commandHistory[m_historyIndex]);
        }
    } else if (event->key() == Qt::Key_Down) {
        // 历史记录向下
        if (m_historyIndex < m_commandHistory.size() - 1) {
            m_historyIndex++;
            m_input->setText(m_commandHistory[m_historyIndex]);
        } else {
            m_historyIndex = m_commandHistory.size();
            m_input->clear();
        }
    } else {
        QWidget::keyPressEvent(event);
    }
}

// ========== SFTPBrowser Implementation ==========

SFTPBrowser::SFTPBrowser(QWidget* parent)
    : QWidget(parent)
    , m_connection(nullptr)
    , m_remotePath("/")
{
    setupUI();

    // 初始化本地路径
    m_localPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    updateLocalFiles(m_localPath);
}

SFTPBrowser::~SFTPBrowser()
{
}

void SFTPBrowser::setupUI()
{
    m_layout = new QHBoxLayout(this);

    // 主分割器
    m_splitter = new QSplitter(Qt::Horizontal, this);

    // 远程文件区域
    m_remoteWidget = new QWidget;
    m_remoteLayout = new QVBoxLayout(m_remoteWidget);

    m_remotePathLabel = new QLabel(tr("远程目录: 未连接"), m_remoteWidget);
    m_remotePathLabel->setStyleSheet("font-weight: bold; background-color: #f0f0f0; padding: 2px;");
    m_remoteLayout->addWidget(m_remotePathLabel);

    m_remoteTree = new QTreeWidget(m_remoteWidget);
    m_remoteTree->setHeaderLabels({tr("名称"), tr("大小"), tr("修改时间"), tr("权限")});
    m_remoteTree->setEnabled(false);
    m_remoteLayout->addWidget(m_remoteTree);

    m_refreshButton = new QPushButton(tr("刷新"), m_remoteWidget);
    m_refreshButton->setEnabled(false);
    m_remoteLayout->addWidget(m_refreshButton);

    // 本地文件区域
    m_localWidget = new QWidget;
    m_localLayout = new QVBoxLayout(m_localWidget);

    m_localPathLabel = new QLabel(tr("本地目录: %1").arg(m_localPath), m_localWidget);
    m_localPathLabel->setStyleSheet("font-weight: bold; background-color: #f0f0f0; padding: 2px;");
    m_localLayout->addWidget(m_localPathLabel);

    m_localTree = new QTreeWidget(m_localWidget);
    m_localTree->setHeaderLabels({tr("名称"), tr("大小"), tr("修改时间")});
    m_localLayout->addWidget(m_localTree);

    // 操作按钮区域
    m_buttonWidget = new QWidget;
    m_buttonLayout = new QVBoxLayout(m_buttonWidget);
    m_buttonWidget->setMaximumWidth(120);

    m_uploadButton = new QPushButton(tr("上传 →"), m_buttonWidget);
    m_downloadButton = new QPushButton(tr("← 下载"), m_buttonWidget);
    m_deleteButton = new QPushButton(tr("删除"), m_buttonWidget);
    m_createDirButton = new QPushButton(tr("新建目录"), m_buttonWidget);

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

    // 进度条
    m_progressBar = new QProgressBar(m_buttonWidget);
    m_progressBar->setVisible(false);
    m_buttonLayout->addWidget(m_progressBar);

    // 添加到分割器
    m_splitter->addWidget(m_localWidget);
    m_splitter->addWidget(m_buttonWidget);
    m_splitter->addWidget(m_remoteWidget);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 0);
    m_splitter->setStretchFactor(2, 1);

    m_layout->addWidget(m_splitter);

    // 连接信号
    connect(m_remoteTree, &QTreeWidget::itemDoubleClicked,
            this, &SFTPBrowser::onRemoteItemDoubleClicked);
    connect(m_localTree, &QTreeWidget::itemDoubleClicked,
            this, &SFTPBrowser::onLocalItemDoubleClicked);
    connect(m_uploadButton, &QPushButton::clicked,
            this, &SFTPBrowser::onUploadClicked);
    connect(m_downloadButton, &QPushButton::clicked,
            this, &SFTPBrowser::onDownloadClicked);
    connect(m_deleteButton, &QPushButton::clicked,
            this, &SFTPBrowser::onDeleteClicked);
    connect(m_createDirButton, &QPushButton::clicked,
            this, &SFTPBrowser::onCreateDirectoryClicked);
    connect(m_refreshButton, &QPushButton::clicked,
            this, &SFTPBrowser::onRefreshClicked);
}

void SFTPBrowser::setConnection(SSHConnection* connection)
{
    m_connection = connection;
    if (m_connection) {
        m_remoteTree->setEnabled(true);
        m_refreshButton->setEnabled(true);
        m_uploadButton->setEnabled(true);
        m_downloadButton->setEnabled(true);
        m_deleteButton->setEnabled(true);
        m_createDirButton->setEnabled(true);

        refresh();
    } else {
        m_remoteTree->setEnabled(false);
        m_refreshButton->setEnabled(false);
        m_uploadButton->setEnabled(false);
        m_downloadButton->setEnabled(false);
        m_deleteButton->setEnabled(false);
        m_createDirButton->setEnabled(false);

        m_remoteTree->clear();
        m_remotePathLabel->setText(tr("远程目录: 未连接"));
    }
}

void SFTPBrowser::refresh()
{
    if (m_connection) {
        updateRemoteFiles(m_remotePath);
    }
    updateLocalFiles(m_localPath);
}

void SFTPBrowser::updateRemoteFiles(const QString& path)
{
    m_remotePath = path;
    m_remotePathLabel->setText(tr("远程目录: %1").arg(m_remotePath));

    m_remoteTree->clear();

    // 添加返回上级目录项
    if (m_remotePath != "/") {
        QTreeWidgetItem* parentItem = new QTreeWidgetItem;
        parentItem->setText(0, "..");
        parentItem->setText(1, tr("<目录>"));
        parentItem->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
        m_remoteTree->addTopLevelItem(parentItem);
    }

    // 这里应该通过SFTP获取远程文件列表
    // 为了演示，添加一些示例文件
    QStringList demoFiles = {"home", "etc", "var", "tmp", "file1.txt", "file2.log"};
    for (const QString& fileName : demoFiles) {
        QTreeWidgetItem* item = new QTreeWidgetItem;
        item->setText(0, fileName);

        if (fileName.contains('.')) {
            // 文件
            item->setText(1, "1024 B");
            item->setText(2, "2024-01-01 10:00:00");
            item->setText(3, "-rw-r--r--");
            item->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
        } else {
            // 目录
            item->setText(1, tr("<目录>"));
            item->setText(2, "2024-01-01 10:00:00");
            item->setText(3, "drwxr-xr-x");
            item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
        }

        m_remoteTree->addTopLevelItem(item);
    }
}

void SFTPBrowser::updateLocalFiles(const QString& path)
{
    m_localPath = path;
    m_localPathLabel->setText(tr("本地目录: %1").arg(m_localPath));

    m_localTree->clear();

    QDir dir(m_localPath);
    if (!dir.exists()) {
        return;
    }

    // 添加返回上级目录项
    if (dir.absolutePath() != dir.rootPath()) {
        QTreeWidgetItem* parentItem = new QTreeWidgetItem;
        parentItem->setText(0, "..");
        parentItem->setText(1, tr("<目录>"));
        parentItem->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
        m_localTree->addTopLevelItem(parentItem);
    }

    // 添加目录
    QFileInfoList dirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo& info : dirs) {
        QTreeWidgetItem* item = new QTreeWidgetItem;
        item->setText(0, info.fileName());
        item->setText(1, tr("<目录>"));
        item->setText(2, info.lastModified().toString("yyyy-MM-dd hh:mm:ss"));
        item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
        m_localTree->addTopLevelItem(item);
    }

    // 添加文件
    QFileInfoList files = dir.entryInfoList(QDir::Files, QDir::Name);
    for (const QFileInfo& info : files) {
        QTreeWidgetItem* item = new QTreeWidgetItem;
        item->setText(0, info.fileName());
        item->setText(1, QString::number(info.size()) + " B");
        item->setText(2, info.lastModified().toString("yyyy-MM-dd hh:mm:ss"));
        item->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
        m_localTree->addTopLevelItem(item);
    }
}

void SFTPBrowser::onRemoteItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column)
    if (!item) return;

    QString fileName = item->text(0);
    if (fileName == "..") {
        // 返回上级目录
        QDir dir(m_remotePath);
        dir.cdUp();
        updateRemoteFiles(dir.absolutePath());
    } else if (item->text(1) == tr("<目录>")) {
        // 进入子目录
        QString newPath = m_remotePath + "/" + fileName;
        updateRemoteFiles(newPath);
    }
}

void SFTPBrowser::onLocalItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column)
    if (!item) return;

    QString fileName = item->text(0);
    if (fileName == "..") {
        // 返回上级目录
        QDir dir(m_localPath);
        dir.cdUp();
        updateLocalFiles(dir.absolutePath());
    } else if (item->text(1) == tr("<目录>")) {
        // 进入子目录
        QString newPath = QDir(m_localPath).absoluteFilePath(fileName);
        updateLocalFiles(newPath);
    }
}

void SFTPBrowser::onUploadClicked()
{
    QTreeWidgetItem* localItem = m_localTree->currentItem();
    if (!localItem || localItem->text(0) == ".." || localItem->text(1) == tr("<目录>")) {
        QMessageBox::information(this, tr("提示"), tr("请选择要上传的文件"));
        return;
    }

    QString localFile = QDir(m_localPath).absoluteFilePath(localItem->text(0));
    QString remoteFile = m_remotePath + "/" + localItem->text(0);

    QMessageBox::information(this, tr("上传"),
        tr("模拟上传文件:\n从: %1\n到: %2").arg(localFile, remoteFile));
}

void SFTPBrowser::onDownloadClicked()
{
    QTreeWidgetItem* remoteItem = m_remoteTree->currentItem();
    if (!remoteItem || remoteItem->text(0) == ".." || remoteItem->text(1) == tr("<目录>")) {
        QMessageBox::information(this, tr("提示"), tr("请选择要下载的文件"));
        return;
    }

    QString remoteFile = m_remotePath + "/" + remoteItem->text(0);
    QString localFile = QDir(m_localPath).absoluteFilePath(remoteItem->text(0));

    QMessageBox::information(this, tr("下载"),
        tr("模拟下载文件:\n从: %1\n到: %2").arg(remoteFile, localFile));
}

void SFTPBrowser::onDeleteClicked()
{
    QTreeWidgetItem* remoteItem = m_remoteTree->currentItem();
    if (!remoteItem || remoteItem->text(0) == "..") {
        QMessageBox::information(this, tr("提示"), tr("请选择要删除的文件或目录"));
        return;
    }

    int result = QMessageBox::question(this, tr("确认删除"),
        tr("确定要删除 '%1' 吗？").arg(remoteItem->text(0)),
        QMessageBox::Yes | QMessageBox::No);

    if (result == QMessageBox::Yes) {
        QMessageBox::information(this, tr("删除"), tr("模拟删除操作"));
        // 这里应该执行实际的删除操作
        refresh();
    }
}

void SFTPBrowser::onCreateDirectoryClicked()
{
    bool ok;
    QString dirName = QInputDialog::getText(this, tr("新建目录"),
        tr("目录名称:"), QLineEdit::Normal, "", &ok);

    if (ok && !dirName.isEmpty()) {
        QMessageBox::information(this, tr("新建目录"),
            tr("模拟创建目录: %1").arg(dirName));
        // 这里应该执行实际的创建目录操作
        refresh();
    }
}

void SFTPBrowser::onRefreshClicked()
{
    refresh();
}

// ========== SSHConnectionDialog Implementation ==========

SSHConnectionDialog::SSHConnectionDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUI();
    setWindowTitle(tr("新建SSH连接"));
}

SSHConnectionDialog::SSHConnectionDialog(const SSHConnectionInfo& info, QWidget* parent)
    : QDialog(parent)
{
    setupUI();
    setConnectionInfo(info);
    setWindowTitle(tr("编辑SSH连接"));
}

void SSHConnectionDialog::setupUI()
{
    setModal(true);
    setFixedSize(400, 500);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // 表单布局
    QWidget* formWidget = new QWidget;
    m_formLayout = new QFormLayout(formWidget);

    // 连接名称
    m_nameEdit = new QLineEdit;
    m_formLayout->addRow(tr("连接名称:"), m_nameEdit);

    // 主机名
    m_hostnameEdit = new QLineEdit;
    m_formLayout->addRow(tr("主机名:"), m_hostnameEdit);

    // 端口
    m_portSpinBox = new QSpinBox;
    m_portSpinBox->setRange(1, 65535);
    m_portSpinBox->setValue(22);
    m_formLayout->addRow(tr("端口:"), m_portSpinBox);

    // 用户名
    m_usernameEdit = new QLineEdit;
    m_formLayout->addRow(tr("用户名:"), m_usernameEdit);

    // 认证方式
    m_usePrivateKeyCheckBox = new QCheckBox(tr("使用私钥认证"));
    m_formLayout->addRow("", m_usePrivateKeyCheckBox);

    // 密码
    m_passwordEdit = new QLineEdit;
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_formLayout->addRow(tr("密码:"), m_passwordEdit);

    // 私钥文件
    QHBoxLayout* keyLayout = new QHBoxLayout;
    m_privateKeyEdit = new QLineEdit;
    m_privateKeyEdit->setEnabled(false);
    m_browseKeyButton = new QPushButton(tr("浏览..."));
    m_browseKeyButton->setEnabled(false);
    keyLayout->addWidget(m_privateKeyEdit);
    keyLayout->addWidget(m_browseKeyButton);
    m_formLayout->addRow(tr("私钥文件:"), keyLayout);

    // 描述
    m_descriptionEdit = new QTextEdit;
    m_descriptionEdit->setMaximumHeight(60);
    m_formLayout->addRow(tr("描述:"), m_descriptionEdit);

    mainLayout->addWidget(formWidget);

    // 测试连接
    QHBoxLayout* testLayout = new QHBoxLayout;
    m_testButton = new QPushButton(tr("测试连接"));
    m_testProgress = new QProgressBar;
    m_testProgress->setVisible(false);
    m_testResultLabel = new QLabel;
    testLayout->addWidget(m_testButton);
    testLayout->addWidget(m_testProgress);
    testLayout->addStretch();
    mainLayout->addLayout(testLayout);
    mainLayout->addWidget(m_testResultLabel);

    // 按钮
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    QPushButton* okButton = new QPushButton(tr("确定"));
    QPushButton* cancelButton = new QPushButton(tr("取消"));
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    mainLayout->addLayout(buttonLayout);

    // 连接信号
    connect(m_usePrivateKeyCheckBox, &QCheckBox::toggled,
            this, &SSHConnectionDialog::onAuthMethodChanged);
    connect(m_browseKeyButton, &QPushButton::clicked,
            this, &SSHConnectionDialog::onBrowsePrivateKey);
    connect(m_testButton, &QPushButton::clicked,
            this, &SSHConnectionDialog::onTestConnection);
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

SSHConnectionInfo SSHConnectionDialog::getConnectionInfo() const
{
    SSHConnectionInfo info;
    info.name = m_nameEdit->text().trimmed();
    info.hostname = m_hostnameEdit->text().trimmed();
    info.port = m_portSpinBox->value();
    info.username = m_usernameEdit->text().trimmed();
    info.password = m_passwordEdit->text();
    info.privateKeyPath = m_privateKeyEdit->text().trimmed();
    info.usePrivateKey = m_usePrivateKeyCheckBox->isChecked();
    info.description = m_descriptionEdit->toPlainText().trimmed();

    return info;
}

void SSHConnectionDialog::setConnectionInfo(const SSHConnectionInfo& info)
{
    m_nameEdit->setText(info.name);
    m_hostnameEdit->setText(info.hostname);
    m_portSpinBox->setValue(info.port);
    m_usernameEdit->setText(info.username);
    m_passwordEdit->setText(info.password);
    m_privateKeyEdit->setText(info.privateKeyPath);
    m_usePrivateKeyCheckBox->setChecked(info.usePrivateKey);
    m_descriptionEdit->setPlainText(info.description);

    onAuthMethodChanged();
}

void SSHConnectionDialog::onAuthMethodChanged()
{
    bool usePrivateKey = m_usePrivateKeyCheckBox->isChecked();
    m_passwordEdit->setEnabled(!usePrivateKey);
    m_privateKeyEdit->setEnabled(usePrivateKey);
    m_browseKeyButton->setEnabled(usePrivateKey);
}

void SSHConnectionDialog::onBrowsePrivateKey()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        tr("选择私钥文件"), QDir::homePath(),
        tr("私钥文件 (*.pem *.key *.ppk);;所有文件 (*.*)"));

    if (!fileName.isEmpty()) {
        m_privateKeyEdit->setText(fileName);
    }
}

void SSHConnectionDialog::onTestConnection()
{
    SSHConnectionInfo info = getConnectionInfo();
    if (!info.isValid()) {
        QMessageBox::warning(this, tr("错误"), tr("请填写完整的连接信息"));
        return;
    }

    m_testButton->setEnabled(false);
    m_testProgress->setVisible(true);
    m_testProgress->setRange(0, 0);
    m_testResultLabel->setText(tr("正在测试连接..."));
    m_testResultLabel->setStyleSheet("color: #495057;");

    // 真正测试连接
    auto* testConn = new SSHConnection(info, this);

    connect(testConn, &SSHConnection::connected, this, [this, testConn]() {
        m_testButton->setEnabled(true);
        m_testProgress->setVisible(false);
        m_testResultLabel->setText(tr("连接测试成功"));
        m_testResultLabel->setStyleSheet("color: green;");
        testConn->disconnect();
        testConn->deleteLater();
    });

    connect(testConn, &SSHConnection::error, this, [this, testConn](const QString& err) {
        m_testButton->setEnabled(true);
        m_testProgress->setVisible(false);
        m_testResultLabel->setText(tr("连接失败: %1").arg(err));
        m_testResultLabel->setStyleSheet("color: red;");
        testConn->deleteLater();
    });

    testConn->connectToHost();
}

// ========== SSHConnection Implementation ==========

SSHConnection::SSHConnection(const SSHConnectionInfo& info, QObject* parent)
    : QObject(parent)
    , m_info(info)
    , m_connected(false)
{
#ifdef WITH_LIBSSH
    m_session = nullptr;
    m_channel = nullptr;
    m_sftpSession = nullptr;
#endif
}

SSHConnection::~SSHConnection()
{
    cleanup();
}

bool SSHConnection::isConnected() const
{
    return m_connected;
}

QString SSHConnection::getLastError() const
{
    return m_lastError;
}

void SSHConnection::connectToHost()
{
#ifdef WITH_LIBSSH
    QMutexLocker locker(&m_mutex);

    // 创建SSH会话
    m_session = ssh_new();
    if (!m_session) {
        m_lastError = tr("无法创建SSH会话");
        emit error(m_lastError);
        return;
    }

    // 设置连接参数
    ssh_options_set(m_session, SSH_OPTIONS_HOST, m_info.hostname.toUtf8().constData());
    ssh_options_set(m_session, SSH_OPTIONS_PORT, &m_info.port);
    ssh_options_set(m_session, SSH_OPTIONS_USER, m_info.username.toUtf8().constData());

    // 设置超时（秒）
    long timeout = 10;
    ssh_options_set(m_session, SSH_OPTIONS_TIMEOUT, &timeout);

    // 连接
    int rc = ssh_connect(m_session);
    if (rc != SSH_OK) {
        m_lastError = QString::fromUtf8(ssh_get_error(m_session));
        cleanup();
        emit error(m_lastError);
        return;
    }

    // 认证
    if (m_info.usePrivateKey) {
        // 使用私钥认证
        ssh_key privateKey;
        rc = ssh_pki_import_privkey_file(m_info.privateKeyPath.toUtf8().constData(),
                                         m_info.password.toUtf8().constData(),
                                         nullptr, nullptr, &privateKey);
        if (rc != SSH_OK) {
            m_lastError = tr("无法加载私钥文件");
            cleanup();
            emit error(m_lastError);
            return;
        }

        rc = ssh_userauth_publickey(m_session, nullptr, privateKey);
        ssh_key_free(privateKey);
    } else {
        // 使用密码认证
        rc = ssh_userauth_password(m_session, nullptr, m_info.password.toUtf8().constData());
    }

    if (rc != SSH_AUTH_SUCCESS) {
        m_lastError = tr("认证失败");
        cleanup();
        emit error(m_lastError);
        return;
    }

    // 创建通道
    m_channel = ssh_channel_new(m_session);
    if (!m_channel) {
        m_lastError = tr("无法创建SSH通道");
        cleanup();
        emit error(m_lastError);
        return;
    }

    rc = ssh_channel_open_session(m_channel);
    if (rc != SSH_OK) {
        m_lastError = tr("无法打开SSH会话");
        cleanup();
        emit error(m_lastError);
        return;
    }

    m_connected = true;
    emit connected();

#else
    m_lastError = tr("LibSSH支持未编译");
    emit error(m_lastError);
#endif
}

void SSHConnection::disconnect()
{
    cleanup();
    m_connected = false;
    emit disconnected();
}

void SSHConnection::executeCommand(const QString& command)
{
#ifdef WITH_LIBSSH
    if (!m_connected || !m_channel) {
        emit commandOutput(tr("错误: 未连接"));
        return;
    }

    QMutexLocker locker(&m_mutex);

    int rc = ssh_channel_request_exec(m_channel, command.toUtf8().constData());
    if (rc != SSH_OK) {
        emit commandOutput(tr("命令执行失败"));
        return;
    }

    // 读取输出
    char buffer[1024];
    int nbytes = ssh_channel_read(m_channel, buffer, sizeof(buffer), 0);
    if (nbytes > 0) {
        QString output = QString::fromUtf8(buffer, nbytes);
        emit commandOutput(output);
    }

#else
    emit commandOutput(tr("模拟执行命令: %1").arg(command));
#endif
}

void SSHConnection::cleanup()
{
#ifdef WITH_LIBSSH
    if (m_sftpSession) {
        sftp_free(m_sftpSession);
        m_sftpSession = nullptr;
    }

    if (m_channel) {
        ssh_channel_close(m_channel);
        ssh_channel_free(m_channel);
        m_channel = nullptr;
    }

    if (m_session) {
        ssh_disconnect(m_session);
        ssh_free(m_session);
        m_session = nullptr;
    }
#endif
}

