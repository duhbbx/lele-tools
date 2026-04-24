#include "sshclient.h"
#include "terminalemulator.h"
#include "terminalwidget.h"
#include <QRegularExpression>
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
    , m_connectionTabs(nullptr)
    , m_menuBar(nullptr)
    , m_statusBar(nullptr)
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
    m_connectionManager->setMinimumWidth(200);
    m_connectionManager->setStyleSheet("ConnectionManager { background-color: #f8f9fa; border-right: 1px solid #e9ecef; }");
    m_connectionManager->setObjectName("sshConnectionManager");

    // 右侧：连接标签页（每个连接一个 tab）
    m_connectionTabs = new QTabWidget(this);
    m_connectionTabs->setTabsClosable(true);
    m_connectionTabs->setMovable(true);
    m_connectionTabs->setDocumentMode(true);
    connect(m_connectionTabs, &QTabWidget::tabCloseRequested, this, &SSHClient::closeConnectionTab);

    // Tab bar context menu for duplicate
    m_connectionTabs->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_connectionTabs->tabBar(), &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        int tabIndex = m_connectionTabs->tabBar()->tabAt(pos);
        if (tabIndex < 0) return;
        QMenu menu;
        menu.addAction(tr("复制标签页"), [this, tabIndex]() {
            duplicateTab(tabIndex);
        });
        menu.exec(m_connectionTabs->tabBar()->mapToGlobal(pos));
    });

    // 添加到分割器
    m_mainSplitter->addWidget(m_connectionManager);
    m_mainSplitter->addWidget(m_connectionTabs);
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
    SSHConnectionInfo info = m_connectionManager->getConnectionInfo(name);
    if (!info.isValid()) {
        QMessageBox::warning(this, tr("错误"), tr("连接信息无效"));
        return;
    }

    m_statusBar->showMessage(tr("正在连接到 %1...").arg(info.hostname));
    QApplication::processEvents();

    // 每个 tab 创建独立的 SSH 连接
    auto* connection = new SSHConnection(info, this);
    connection->connectToHost();

    if (!connection->isConnected()) {
        m_statusBar->showMessage(tr("连接失败: %1").arg(connection->getLastError()));
        QMessageBox::critical(this, tr("连接失败"), connection->getLastError());
        connection->deleteLater();
        return;
    }

    // 创建 tab 页：内含子 tab（终端 + SFTP）
    auto* tabContainer = new QWidget();
    auto* tabLayout = new QVBoxLayout(tabContainer);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->setSpacing(0);

    auto* subTabs = new QTabWidget();
    subTabs->setDocumentMode(true);
    subTabs->tabBar()->setExpanding(false);

    auto* terminal = new SSHTerminal(tabContainer);
    auto* sftp = new SFTPBrowser(tabContainer);

    subTabs->addTab(terminal, tr("终端"));
    subTabs->addTab(sftp, tr("SFTP"));

    tabLayout->addWidget(subTabs);

    // 设置连接
    terminal->setConnection(connection);
    sftp->setConnection(connection);

    // 保存 session
    ConnectionSession session;
    session.name = name;
    session.connection = connection;
    session.terminal = terminal;
    session.sftp = sftp;
    session.tabWidget = tabContainer;
    m_sessions.append(session);

    // 添加 tab
    QString tabTitle = QString("%1@%2").arg(info.username, info.hostname);
    int index = m_connectionTabs->addTab(tabContainer, tabTitle);
    m_connectionTabs->setCurrentIndex(index);

    m_statusBar->showMessage(tr("已连接到 %1").arg(tabTitle));

    // 断开时自动清理
    QWidget* tabPtr = tabContainer;
    connect(connection, &SSHConnection::disconnected, this, [this, tabPtr]() {
        // Find and remove by tabWidget pointer
        for (int i = 0; i < m_sessions.size(); ++i) {
            if (m_sessions[i].tabWidget == tabPtr) {
                // Remove the tab
                for (int t = 0; t < m_connectionTabs->count(); ++t) {
                    if (m_connectionTabs->widget(t) == tabPtr) {
                        m_connectionTabs->removeTab(t);
                        break;
                    }
                }
                auto& s = m_sessions[i];
                if (s.terminal) s.terminal->setConnection(nullptr);
                if (s.sftp) s.sftp->setConnection(nullptr);
                if (s.tabWidget) s.tabWidget->deleteLater();
                if (s.connection) s.connection->deleteLater();
                QString sname = s.name;
                m_sessions.removeAt(i);
                m_statusBar->showMessage(tr("连接已断开: %1").arg(sname));
                break;
            }
        }
    });
}

void SSHClient::onConnectionEstablished(const QString& name)
{
    Q_UNUSED(name)
}

void SSHClient::onConnectionClosed(const QString& name)
{
    Q_UNUSED(name)
    // Cleanup is now handled by the disconnected signal lambda in onConnectionRequested
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

    // Disconnect all tabs for this connection name
    for (int i = m_sessions.size() - 1; i >= 0; --i) {
        if (m_sessions[i].name == selected && m_sessions[i].connection) {
            m_sessions[i].connection->disconnect();
        }
    }

    m_connectionManager->removeConnection(selected);
}

void SSHClient::onDisconnect()
{
    // 断开当前 tab 对应的连接
    QWidget* current = m_connectionTabs->currentWidget();
    if (!current) return;

    for (int i = 0; i < m_sessions.size(); ++i) {
        if (m_sessions[i].tabWidget == current) {
            if (m_sessions[i].connection) m_sessions[i].connection->disconnect();
            return;
        }
    }
}

void SSHClient::closeConnectionTab(int index)
{
    QWidget* tabWidget = m_connectionTabs->widget(index);
    if (!tabWidget) return;

    for (int i = 0; i < m_sessions.size(); ++i) {
        if (m_sessions[i].tabWidget == tabWidget) {
            if (m_sessions[i].connection) m_sessions[i].connection->disconnect();
            return;
        }
    }
    // 如果没有对应 session（不应该发生），直接移除
    m_connectionTabs->removeTab(index);
}

void SSHClient::duplicateTab(int index)
{
    QWidget* tabWidget = m_connectionTabs->widget(index);
    if (!tabWidget) return;

    // Find the session for this tab
    QString connName;
    for (int i = 0; i < m_sessions.size(); ++i) {
        if (m_sessions[i].tabWidget == tabWidget) {
            connName = m_sessions[i].name;
            break;
        }
    }

    if (!connName.isEmpty()) {
        onConnectionRequested(connName);
    }
}

void SSHClient::closeEvent(QCloseEvent* event)
{
    if (!m_sessions.isEmpty()) {
        int result = QMessageBox::question(this, tr("确认退出"),
            tr("当前有活动连接，确定要退出吗？"),
            QMessageBox::Yes | QMessageBox::No);
        if (result != QMessageBox::Yes) {
            event->ignore();
            return;
        }
        // 断开所有连接
        for (int i = m_sessions.size() - 1; i >= 0; --i) {
            if (m_sessions[i].connection)
                m_sessions[i].connection->disconnect();
        }
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
    m_layout->setContentsMargins(4, 4, 4, 4);
    m_layout->setSpacing(4);

    // 连接列表
    m_connectionsTree = new QTreeWidget(this);
    m_connectionsTree->setHeaderLabels({tr("名称"), tr("主机")});
    m_connectionsTree->setHeaderHidden(true);
    m_connectionsTree->setRootIsDecorated(false);
    m_connectionsTree->setAlternatingRowColors(false);
    m_connectionsTree->setStyleSheet(
        "QTreeWidget { border:none; font-size:9pt; background:transparent; }"
    );
    m_layout->addWidget(m_connectionsTree);

    // 按钮组
    QHBoxLayout* buttonLayout = new QHBoxLayout;

    QString cmBtnStyle = "QPushButton { padding:3px 8px; border:none; border-radius:4px; font-size:9pt; color:#495057; background:transparent; } QPushButton:hover { background:#e9ecef; }";
    m_newButton = new QPushButton(tr("新建"), this);
    m_newButton->setStyleSheet(cmBtnStyle);
    m_editButton = new QPushButton(tr("编辑"), this);
    m_editButton->setStyleSheet(cmBtnStyle);
    m_deleteButton = new QPushButton(tr("删除"), this);
    m_deleteButton->setStyleSheet(cmBtnStyle);
    m_connectButton = new QPushButton(tr("连接"), this);
    m_connectButton->setStyleSheet(cmBtnStyle);

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
        info.keepAlive = settings.value("keepAlive", true).toBool();
        info.keepAliveInterval = settings.value("keepAliveInterval", 30).toInt();

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
        settings.setValue("keepAlive", info.keepAlive);
        settings.setValue("keepAliveInterval", info.keepAliveInterval);

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
        menu.addAction(tr("测试连接"), [this]() {
            if (m_selectedConnection.isEmpty()) return;
            SSHConnectionInfo info = m_connections.value(m_selectedConnection);
            if (!info.isValid()) return;
            auto* testConn = new SSHConnection(info, this);
            testConn->connectToHost();
            if (testConn->isConnected()) {
                QMessageBox::information(this, tr("测试连接"), tr("连接成功"));
                testConn->disconnect();
            } else {
                QMessageBox::warning(this, tr("测试连接"), tr("连接失败: %1").arg(testConn->getLastError()));
            }
            testConn->deleteLater();
        });
        menu.addSeparator();
        menu.addAction(tr("编辑"), this, &ConnectionManager::editConnectionRequested);
        menu.addAction(tr("删除"), this, &ConnectionManager::deleteConnectionRequested);
        menu.addSeparator();
    }

    menu.addAction(tr("新建连接"), this, &ConnectionManager::newConnectionRequested);

    menu.exec(event->globalPos());
}

// ========== TerminalTextEdit Implementation ==========

void TerminalTextEdit::keyPressEvent(QKeyEvent* event)
{
    QByteArray data;

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        data = "\r";
    } else if (event->key() == Qt::Key_Backspace) {
        data = "\x7f";  // DEL
    } else if (event->key() == Qt::Key_Tab) {
        data = "\t";
    } else if (event->key() == Qt::Key_Up) {
        data = "\x1b[A";
    } else if (event->key() == Qt::Key_Down) {
        data = "\x1b[B";
    } else if (event->key() == Qt::Key_Right) {
        data = "\x1b[C";
    } else if (event->key() == Qt::Key_Left) {
        data = "\x1b[D";
    } else if (event->key() == Qt::Key_Home) {
        data = "\x1b[H";
    } else if (event->key() == Qt::Key_End) {
        data = "\x1b[F";
    } else if (event->modifiers() & Qt::ControlModifier) {
        int key = event->key();
        if (key >= Qt::Key_A && key <= Qt::Key_Z) {
            char c = key - Qt::Key_A + 1;
            data = QByteArray(1, c);
        }
    } else {
        QString text = event->text();
        if (!text.isEmpty()) {
            data = text.toUtf8();
        }
    }

    if (!data.isEmpty()) {
        emit keyPressed(data);
    }
    // Don't call base class - we handle all input ourselves
}

// ========== SSHTerminal Implementation ==========

SSHTerminal::SSHTerminal(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
}

SSHTerminal::~SSHTerminal()
{
}

void SSHTerminal::setupUI()
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);

    m_emulator = new TerminalEmulator(80, 24, this);
    m_termWidget = new TerminalWidget(this);
    m_termWidget->setEmulator(m_emulator);
    m_layout->addWidget(m_termWidget);

    // Wire keyboard input from widget
    connect(m_termWidget, &TerminalWidget::keyPressed, this, [this](const QByteArray& data) {
        if (m_connection)
            m_connection->writeToChannel(data);
    });

    // Wire terminal size changes
    connect(m_termWidget, &TerminalWidget::sizeChanged, this, [this](int cols, int rows) {
        m_emulator->resize(cols, rows);
        if (m_connection)
            m_connection->resizePty(cols, rows);
    });

    // Repaint when emulator screen changes
    connect(m_emulator, &TerminalEmulator::screenChanged,
            m_termWidget, QOverload<>::of(&QWidget::update));
}

void SSHTerminal::setConnection(SSHConnection* connection)
{
    m_connection = connection;
    if (m_connection) {
        // Feed SSH data into the VT100 emulator
        connect(m_connection, &SSHConnection::dataReceived,
                m_emulator, &TerminalEmulator::processData);
        connect(m_connection, &SSHConnection::error,
                this, &SSHTerminal::onConnectionError);

        m_emulator->reset();
        m_termWidget->setFocus();

        m_connection->requestPtyShell();
    }
}

void SSHTerminal::clear()
{
    m_emulator->reset();
}

void SSHTerminal::onConnectionError(const QString& error)
{
    Q_UNUSED(error)
    // Errors could be displayed in a status bar or overlay;
    // for now the emulator grid is the only display.
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
    m_layout->setContentsMargins(2, 2, 2, 2);
    m_layout->setSpacing(2);

    // 主分割器
    m_splitter = new QSplitter(Qt::Horizontal, this);

    // 远程文件区域
    m_remoteWidget = new QWidget;
    m_remoteLayout = new QVBoxLayout(m_remoteWidget);
    m_remoteLayout->setContentsMargins(2, 2, 2, 2);
    m_remoteLayout->setSpacing(2);

    m_remotePathEdit = new QLineEdit(m_remoteWidget);
    m_remotePathEdit->setPlaceholderText(tr("远程目录路径..."));
    m_remotePathEdit->setStyleSheet("font-size:9pt; padding:2px 6px; border:1px solid #dee2e6; border-radius:4px;");
    connect(m_remotePathEdit, &QLineEdit::returnPressed, this, [this]() {
        m_remotePath = m_remotePathEdit->text().trimmed();
        if (!m_remotePath.isEmpty()) updateRemoteFiles(m_remotePath);
    });
    m_remoteLayout->addWidget(m_remotePathEdit);

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
    m_localLayout->setContentsMargins(2, 2, 2, 2);
    m_localLayout->setSpacing(2);

    m_localPathEdit = new QLineEdit(m_localPath, m_localWidget);
    m_localPathEdit->setStyleSheet("font-size:9pt; padding:2px 6px; border:1px solid #dee2e6; border-radius:4px;");
    connect(m_localPathEdit, &QLineEdit::returnPressed, this, [this]() {
        QString path = m_localPathEdit->text().trimmed();
        if (QDir(path).exists()) {
            m_localPath = path;
            updateLocalFiles(m_localPath);
        }
    });
    m_localLayout->addWidget(m_localPathEdit);

    m_localTree = new QTreeWidget(m_localWidget);
    m_localTree->setHeaderLabels({tr("名称"), tr("大小"), tr("修改时间")});
    m_localLayout->addWidget(m_localTree);

    // 操作按钮区域
    m_buttonWidget = new QWidget;
    m_buttonLayout = new QVBoxLayout(m_buttonWidget);
    m_buttonLayout->setContentsMargins(2, 2, 2, 2);
    m_buttonLayout->setSpacing(2);
    m_buttonWidget->setMaximumWidth(100);

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

    // Apply flat modern styles
    setStyleSheet(
        "SFTPBrowser QPushButton { padding:3px 8px; border:none; border-radius:4px; font-size:9pt; color:#495057; background:transparent; }"
        "SFTPBrowser QPushButton:hover { background:#e9ecef; }"
        "SFTPBrowser QTreeWidget { border:1px solid #dee2e6; border-radius:4px; font-size:9pt; }"
        "SFTPBrowser QHeaderView::section { background:#f8f9fa; border:none; border-bottom:1px solid #dee2e6; padding:3px 6px; font-size:9pt; }"
        "SFTPBrowser QLabel { font-size:9pt; color:#495057; }"
        "SFTPBrowser QProgressBar { border:1px solid #dee2e6; border-radius:4px; height:6px; }"
    );

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
        m_remotePathEdit->clear();
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
    m_remotePathEdit->setText(m_remotePath);

    m_remoteTree->clear();

    // 添加返回上级目录项
    if (m_remotePath != "/") {
        QTreeWidgetItem* parentItem = new QTreeWidgetItem;
        parentItem->setText(0, "..");
        parentItem->setText(1, tr("<目录>"));
        parentItem->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
        m_remoteTree->addTopLevelItem(parentItem);
    }

    if (!m_connection) return;

    QList<SFTPFileInfo> files = m_connection->listDirectory(m_remotePath);
    if (files.isEmpty() && m_remotePath != "/") {
        // Might be a permission error or empty dir; show nothing extra
    }

    // Sort: directories first, then files, alphabetically
    std::sort(files.begin(), files.end(), [](const SFTPFileInfo& a, const SFTPFileInfo& b) {
        if (a.isDir != b.isDir) return a.isDir > b.isDir;
        return a.name.toLower() < b.name.toLower();
    });

    for (const SFTPFileInfo& fi : files) {
        QTreeWidgetItem* item = new QTreeWidgetItem;
        item->setText(0, fi.name);

        if (fi.isDir) {
            item->setText(1, tr("<目录>"));
            item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
        } else {
            // Human-readable size
            if (fi.size < 1024)
                item->setText(1, QString::number(fi.size) + " B");
            else if (fi.size < 1024 * 1024)
                item->setText(1, QString::number(fi.size / 1024.0, 'f', 1) + " KB");
            else if (fi.size < 1024LL * 1024 * 1024)
                item->setText(1, QString::number(fi.size / (1024.0 * 1024.0), 'f', 1) + " MB");
            else
                item->setText(1, QString::number(fi.size / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB");
            item->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
        }

        item->setText(2, fi.modified.toString("yyyy-MM-dd hh:mm:ss"));
        item->setText(3, fi.permissions);

        m_remoteTree->addTopLevelItem(item);
    }
}

void SFTPBrowser::updateLocalFiles(const QString& path)
{
    m_localPath = path;
    m_localPathEdit->setText(m_localPath);

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
        QString parent = m_remotePath;
        if (parent.endsWith('/') && parent.length() > 1)
            parent.chop(1);
        int lastSlash = parent.lastIndexOf('/');
        if (lastSlash >= 0)
            parent = parent.left(lastSlash);
        if (parent.isEmpty())
            parent = "/";
        updateRemoteFiles(parent);
    } else if (item->text(1) == tr("<目录>")) {
        // 进入子目录
        QString newPath = m_remotePath + (m_remotePath.endsWith('/') ? "" : "/") + fileName;
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
    if (!m_connection) return;

    QTreeWidgetItem* localItem = m_localTree->currentItem();
    if (!localItem || localItem->text(0) == ".." || localItem->text(1) == tr("<目录>")) {
        QMessageBox::information(this, tr("提示"), tr("请选择要上传的文件"));
        return;
    }

    QString localFile = QDir(m_localPath).absoluteFilePath(localItem->text(0));
    QString remoteFile = m_remotePath + (m_remotePath.endsWith('/') ? "" : "/") + localItem->text(0);

    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0); // indeterminate
    QApplication::processEvents();

    bool ok = m_connection->uploadFile(localFile, remoteFile);

    m_progressBar->setVisible(false);

    if (ok) {
        QMessageBox::information(this, tr("上传"), tr("文件上传成功"));
        refresh();
    } else {
        QMessageBox::critical(this, tr("上传失败"),
            tr("上传文件失败: %1").arg(m_connection->sftpError()));
    }
}

void SFTPBrowser::onDownloadClicked()
{
    if (!m_connection) return;

    QTreeWidgetItem* remoteItem = m_remoteTree->currentItem();
    if (!remoteItem || remoteItem->text(0) == ".." || remoteItem->text(1) == tr("<目录>")) {
        QMessageBox::information(this, tr("提示"), tr("请选择要下载的文件"));
        return;
    }

    QString remoteFile = m_remotePath + (m_remotePath.endsWith('/') ? "" : "/") + remoteItem->text(0);
    QString localFile = QDir(m_localPath).absoluteFilePath(remoteItem->text(0));

    // Let user choose save location
    QString savePath = QFileDialog::getSaveFileName(this, tr("保存文件"), localFile);
    if (savePath.isEmpty()) return;

    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0);
    QApplication::processEvents();

    bool ok = m_connection->downloadFile(remoteFile, savePath);

    m_progressBar->setVisible(false);

    if (ok) {
        QMessageBox::information(this, tr("下载"), tr("文件下载成功"));
        updateLocalFiles(m_localPath);
    } else {
        QMessageBox::critical(this, tr("下载失败"),
            tr("下载文件失败: %1").arg(m_connection->sftpError()));
    }
}

void SFTPBrowser::onDeleteClicked()
{
    if (!m_connection) return;

    QTreeWidgetItem* remoteItem = m_remoteTree->currentItem();
    if (!remoteItem || remoteItem->text(0) == "..") {
        QMessageBox::information(this, tr("提示"), tr("请选择要删除的文件或目录"));
        return;
    }

    int result = QMessageBox::question(this, tr("确认删除"),
        tr("确定要删除 '%1' 吗？").arg(remoteItem->text(0)),
        QMessageBox::Yes | QMessageBox::No);

    if (result == QMessageBox::Yes) {
        QString remotePath = m_remotePath + (m_remotePath.endsWith('/') ? "" : "/") + remoteItem->text(0);
        bool ok = m_connection->deleteRemoteFile(remotePath);
        if (ok) {
            refresh();
        } else {
            QMessageBox::critical(this, tr("删除失败"),
                tr("删除失败: %1").arg(m_connection->sftpError()));
        }
    }
}

void SFTPBrowser::onCreateDirectoryClicked()
{
    if (!m_connection) return;

    bool ok;
    QString dirName = QInputDialog::getText(this, tr("新建目录"),
        tr("目录名称:"), QLineEdit::Normal, "", &ok);

    if (ok && !dirName.isEmpty()) {
        QString remotePath = m_remotePath + (m_remotePath.endsWith('/') ? "" : "/") + dirName;
        bool success = m_connection->createRemoteDirectory(remotePath);
        if (success) {
            refresh();
        } else {
            QMessageBox::critical(this, tr("创建目录失败"),
                tr("创建目录失败: %1").arg(m_connection->sftpError()));
        }
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

    // 保活设置
    m_keepAliveCheckBox = new QCheckBox(tr("启用保活"));
    m_keepAliveCheckBox->setChecked(true);
    m_formLayout->addRow("", m_keepAliveCheckBox);

    QHBoxLayout* keepAliveLayout = new QHBoxLayout;
    m_keepAliveIntervalSpinBox = new QSpinBox;
    m_keepAliveIntervalSpinBox->setRange(5, 600);
    m_keepAliveIntervalSpinBox->setValue(30);
    m_keepAliveIntervalSpinBox->setSuffix(tr(" 秒"));
    keepAliveLayout->addWidget(m_keepAliveIntervalSpinBox);
    keepAliveLayout->addStretch();
    m_formLayout->addRow(tr("保活间隔:"), keepAliveLayout);

    connect(m_keepAliveCheckBox, &QCheckBox::toggled, m_keepAliveIntervalSpinBox, &QSpinBox::setEnabled);

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
    info.keepAlive = m_keepAliveCheckBox->isChecked();
    info.keepAliveInterval = m_keepAliveIntervalSpinBox->value();

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
    m_keepAliveCheckBox->setChecked(info.keepAlive);
    m_keepAliveIntervalSpinBox->setValue(info.keepAliveInterval);

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

    // Start keep-alive timer if enabled
    if (m_info.keepAlive && m_info.keepAliveInterval > 0) {
        m_keepAliveTimer = new QTimer(this);
        connect(m_keepAliveTimer, &QTimer::timeout, this, [this]() {
            if (m_session && m_connected) {
                ssh_send_ignore(m_session, "keepalive");
            }
        });
        m_keepAliveTimer->start(m_info.keepAliveInterval * 1000);
    }

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

void SSHConnection::resizePty(int cols, int rows)
{
#ifdef WITH_LIBSSH
    if (m_channel && ssh_channel_is_open(m_channel))
        ssh_channel_change_pty_size(m_channel, cols, rows);
#else
    Q_UNUSED(cols)
    Q_UNUSED(rows)
#endif
}

void SSHConnection::requestPtyShell()
{
#ifdef WITH_LIBSSH
    if (!m_session || !m_channel) return;

    int rc = ssh_channel_request_pty_size(m_channel, "xterm-256color", 80, 24);
    if (rc != SSH_OK) {
        emit error(tr("Failed to request PTY"));
        return;
    }

    rc = ssh_channel_request_shell(m_channel);
    if (rc != SSH_OK) {
        emit error(tr("Failed to request shell"));
        return;
    }

    m_shellActive = true;

    m_readThread = QThread::create([this]() {
        char buffer[4096];
        while (m_shellActive && m_channel && ssh_channel_is_open(m_channel)) {
            int nbytes = ssh_channel_read_timeout(m_channel, buffer, sizeof(buffer), 0, 100);
            if (nbytes > 0) {
                emit dataReceived(QByteArray(buffer, nbytes));
            } else if (nbytes == SSH_ERROR) {
                break;
            }
            nbytes = ssh_channel_read_timeout(m_channel, buffer, sizeof(buffer), 1, 10);
            if (nbytes > 0) {
                emit dataReceived(QByteArray(buffer, nbytes));
            }
        }
    });
    m_readThread->start();
#endif
}

void SSHConnection::writeToChannel(const QByteArray& data)
{
#ifdef WITH_LIBSSH
    if (m_channel && ssh_channel_is_open(m_channel)) {
        ssh_channel_write(m_channel, data.constData(), data.size());
    }
#endif
}

bool SSHConnection::initSftp()
{
#ifdef WITH_LIBSSH
    if (m_sftpSession) return true;
    m_sftpSession = sftp_new(m_session);
    if (!m_sftpSession) return false;
    if (sftp_init(m_sftpSession) != SSH_OK) {
        sftp_free(m_sftpSession);
        m_sftpSession = nullptr;
        return false;
    }
    return true;
#else
    return false;
#endif
}

QList<SFTPFileInfo> SSHConnection::listDirectory(const QString& path)
{
    QList<SFTPFileInfo> files;
#ifdef WITH_LIBSSH
    if (!initSftp()) return files;

    sftp_dir dir = sftp_opendir(m_sftpSession, path.toUtf8().constData());
    if (!dir) return files;

    sftp_attributes attrs;
    while ((attrs = sftp_readdir(m_sftpSession, dir)) != nullptr) {
        QString name = QString::fromUtf8(attrs->name);
        if (name == "." || name == "..") {
            sftp_attributes_free(attrs);
            continue;
        }
        SFTPFileInfo info;
        info.name = name;
        info.path = path + (path.endsWith('/') ? "" : "/") + name;
        info.size = attrs->size;
        info.isDir = (attrs->type == SSH_FILEXFER_TYPE_DIRECTORY);
        info.permissions = QString::number(attrs->permissions & 0777, 8);
        info.modified = QDateTime::fromSecsSinceEpoch(attrs->mtime);
        files.append(info);
        sftp_attributes_free(attrs);
    }
    // Check if loop ended due to error (not EOF)
    if (!sftp_dir_eof(dir)) {
        qWarning() << "SFTP readdir error on path:" << path;
    }
    sftp_closedir(dir);
#else
    Q_UNUSED(path)
#endif
    return files;
}

bool SSHConnection::downloadFile(const QString& remotePath, const QString& localPath)
{
#ifdef WITH_LIBSSH
    if (!initSftp()) return false;

    sftp_file file = sftp_open(m_sftpSession, remotePath.toUtf8().constData(), O_RDONLY, 0);
    if (!file) {
        m_lastError = QString::fromUtf8(ssh_get_error(m_session));
        return false;
    }

    QFile localFile(localPath);
    if (!localFile.open(QIODevice::WriteOnly)) {
        m_lastError = tr("无法创建本地文件: %1").arg(localPath);
        sftp_close(file);
        return false;
    }

    char buffer[65536];
    int nbytes;
    bool success = true;
    while ((nbytes = sftp_read(file, buffer, sizeof(buffer))) > 0) {
        localFile.write(buffer, nbytes);
    }
    if (nbytes < 0) {
        m_lastError = QString::fromUtf8(ssh_get_error(m_session));
        success = false;
    }

    localFile.close();
    sftp_close(file);
    return success;
#else
    Q_UNUSED(remotePath)
    Q_UNUSED(localPath)
    return false;
#endif
}

bool SSHConnection::uploadFile(const QString& localPath, const QString& remotePath)
{
#ifdef WITH_LIBSSH
    if (!initSftp()) return false;

    QFile localFile(localPath);
    if (!localFile.open(QIODevice::ReadOnly)) return false;

    sftp_file file = sftp_open(m_sftpSession, remotePath.toUtf8().constData(),
                               O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (!file) {
        m_lastError = QString::fromUtf8(ssh_get_error(m_session));
        localFile.close();
        return false;
    }

    char buffer[65536];
    qint64 nbytes;
    while ((nbytes = localFile.read(buffer, sizeof(buffer))) > 0) {
        if (sftp_write(file, buffer, nbytes) != nbytes) {
            sftp_close(file);
            localFile.close();
            return false;
        }
    }

    sftp_close(file);
    localFile.close();
    return true;
#else
    Q_UNUSED(localPath)
    Q_UNUSED(remotePath)
    return false;
#endif
}

bool SSHConnection::deleteRemoteFile(const QString& path)
{
#ifdef WITH_LIBSSH
    if (!initSftp()) return false;
    return sftp_unlink(m_sftpSession, path.toUtf8().constData()) == SSH_OK;
#else
    Q_UNUSED(path)
    return false;
#endif
}

bool SSHConnection::createRemoteDirectory(const QString& path)
{
#ifdef WITH_LIBSSH
    if (!initSftp()) return false;
    return sftp_mkdir(m_sftpSession, path.toUtf8().constData(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == SSH_OK;
#else
    Q_UNUSED(path)
    return false;
#endif
}

QString SSHConnection::sftpError() const
{
#ifdef WITH_LIBSSH
    if (!m_lastError.isEmpty()) return m_lastError;
    if (m_session) return QString::fromUtf8(ssh_get_error(m_session));
    return tr("连接无效");
#else
    return tr("LibSSH not compiled");
#endif
}

void SSHConnection::cleanup()
{
    if (m_keepAliveTimer) {
        m_keepAliveTimer->stop();
        delete m_keepAliveTimer;
        m_keepAliveTimer = nullptr;
    }

    m_shellActive = false;
    if (m_readThread) {
        m_readThread->quit();
        m_readThread->wait(2000);
        delete m_readThread;
        m_readThread = nullptr;
    }

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

