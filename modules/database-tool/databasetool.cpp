#include "databasetool.h"
#include "../../common/connx/RedisConnection.h"
#ifdef WITH_MYSQL
#include "../../common/connx/MySQLConnection.h"
#endif
#include <QApplication>
#include <QMessageBox>
#include <QInputDialog>
#include <QHeaderView>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>
#include <QTimer>
#include <QStringListModel>

// 移除全局using namespace以避免命名冲突
// using namespace SqliteWrapper;

REGISTER_DYNAMICOBJECT(DatabaseTool);

// DatabaseHierarchyManager 实现
DatabaseHierarchyManager& DatabaseHierarchyManager::instance() {
    static DatabaseHierarchyManager instance;
    return instance;
}

DatabaseHierarchyManager::DatabaseHierarchyManager() {
    initializeHierarchies();
}

void DatabaseHierarchyManager::initializeHierarchies() {
    // 初始化节点图标映射
    m_nodeIcons[DbNodeType::Connection] = "database-connection";
    m_nodeIcons[DbNodeType::Database] = "database";
    m_nodeIcons[DbNodeType::Schema] = "schema";
    m_nodeIcons[DbNodeType::TableFolder] = "folder-table";
    m_nodeIcons[DbNodeType::Table] = "table";
    m_nodeIcons[DbNodeType::ViewFolder] = "folder-view";
    m_nodeIcons[DbNodeType::View] = "view";
    m_nodeIcons[DbNodeType::ProcedureFolder] = "folder-procedure";
    m_nodeIcons[DbNodeType::Procedure] = "procedure";
    m_nodeIcons[DbNodeType::FunctionFolder] = "folder-function";
    m_nodeIcons[DbNodeType::Function] = "function";
    m_nodeIcons[DbNodeType::TriggerFolder] = "folder-trigger";
    m_nodeIcons[DbNodeType::Trigger] = "trigger";
    m_nodeIcons[DbNodeType::IndexFolder] = "folder-index";
    m_nodeIcons[DbNodeType::Index] = "index";
    m_nodeIcons[DbNodeType::SequenceFolder] = "folder-sequence";
    m_nodeIcons[DbNodeType::Sequence] = "sequence";
    m_nodeIcons[DbNodeType::UserFolder] = "folder-user";
    m_nodeIcons[DbNodeType::User] = "user";
    m_nodeIcons[DbNodeType::Column] = "column";
    m_nodeIcons[DbNodeType::Key] = "key";
    m_nodeIcons[DbNodeType::RedisKey] = "redis-key";

    // 初始化节点显示名称
    m_nodeNames[DbNodeType::TableFolder] = "表";
    m_nodeNames[DbNodeType::ViewFolder] = "视图";
    m_nodeNames[DbNodeType::ProcedureFolder] = "存储过程";
    m_nodeNames[DbNodeType::FunctionFolder] = "函数";
    m_nodeNames[DbNodeType::TriggerFolder] = "触发器";
    m_nodeNames[DbNodeType::IndexFolder] = "索引";
    m_nodeNames[DbNodeType::SequenceFolder] = "序列";
    m_nodeNames[DbNodeType::UserFolder] = "用户";

    // MySQL层级结构
    QList<DbHierarchyNode> mysqlHierarchy;
    mysqlHierarchy.append(DbHierarchyNode(DbNodeType::TableFolder, "表"));
    mysqlHierarchy.append(DbHierarchyNode(DbNodeType::ViewFolder, "视图"));
    mysqlHierarchy.append(DbHierarchyNode(DbNodeType::ProcedureFolder, "存储过程"));
    mysqlHierarchy.append(DbHierarchyNode(DbNodeType::FunctionFolder, "函数"));
    mysqlHierarchy.append(DbHierarchyNode(DbNodeType::TriggerFolder, "触发器"));
    m_hierarchies[Connx::ConnectionType::MySQL] = mysqlHierarchy;

    // PostgreSQL层级结构
    QList<DbHierarchyNode> postgresHierarchy;
    postgresHierarchy.append(DbHierarchyNode(DbNodeType::TableFolder, "表"));
    postgresHierarchy.append(DbHierarchyNode(DbNodeType::ViewFolder, "视图"));
    postgresHierarchy.append(DbHierarchyNode(DbNodeType::ProcedureFolder, "存储过程"));
    postgresHierarchy.append(DbHierarchyNode(DbNodeType::FunctionFolder, "函数"));
    postgresHierarchy.append(DbHierarchyNode(DbNodeType::TriggerFolder, "触发器"));
    postgresHierarchy.append(DbHierarchyNode(DbNodeType::SequenceFolder, "序列"));
    postgresHierarchy.append(DbHierarchyNode(DbNodeType::IndexFolder, "索引"));
    m_hierarchies[Connx::ConnectionType::PostgreSQL] = postgresHierarchy;

    // SQLite层级结构（简化）
    QList<DbHierarchyNode> sqliteHierarchy;
    sqliteHierarchy.append(DbHierarchyNode(DbNodeType::TableFolder, "表"));
    sqliteHierarchy.append(DbHierarchyNode(DbNodeType::ViewFolder, "视图"));
    sqliteHierarchy.append(DbHierarchyNode(DbNodeType::IndexFolder, "索引"));
    sqliteHierarchy.append(DbHierarchyNode(DbNodeType::TriggerFolder, "触发器"));
    m_hierarchies[Connx::ConnectionType::SQLite] = sqliteHierarchy;

    // Redis层级结构（无层级，直接显示键）
    QList<DbHierarchyNode> redisHierarchy;
    // Redis没有文件夹结构，直接显示键
    m_hierarchies[Connx::ConnectionType::Redis] = redisHierarchy;
}

QList<DbHierarchyNode> DatabaseHierarchyManager::getHierarchy(Connx::ConnectionType dbType) const {
    return m_hierarchies.value(dbType, QList<DbHierarchyNode>());
}

bool DatabaseHierarchyManager::needsLazyLoading(DbNodeType nodeType) const {
    // 文件夹类型和数据库节点需要延迟加载
    return nodeType == DbNodeType::Database ||
        nodeType == DbNodeType::Schema ||
        nodeType == DbNodeType::TableFolder ||
        nodeType == DbNodeType::ViewFolder ||
        nodeType == DbNodeType::ProcedureFolder ||
        nodeType == DbNodeType::FunctionFolder ||
        nodeType == DbNodeType::TriggerFolder ||
        nodeType == DbNodeType::IndexFolder ||
        nodeType == DbNodeType::SequenceFolder ||
        nodeType == DbNodeType::UserFolder;
}

QString DatabaseHierarchyManager::getNodeIcon(DbNodeType nodeType) const {
    return m_nodeIcons.value(nodeType, "default");
}

QString DatabaseHierarchyManager::getNodeDisplayName(DbNodeType nodeType) const {
    return m_nodeNames.value(nodeType, "未知节点");
}

// DbTreeWidgetItem 实现
DbTreeWidgetItem::DbTreeWidgetItem(QTreeWidget* parent, const DbNodeData& data)
    : QTreeWidgetItem(parent), m_nodeData(data) {
    updateDisplay();
}

DbTreeWidgetItem::DbTreeWidgetItem(QTreeWidgetItem* parent, const DbNodeData& data)
    : QTreeWidgetItem(parent), m_nodeData(data) {
    updateDisplay();
}

void DbTreeWidgetItem::updateDisplay() {
    // 如果displayName为空，则生成带emoji的显示名称
    if (m_nodeData.displayName.isEmpty()) {
        QString emoji = getNodeEmoji();
        m_nodeData.displayName = emoji.isEmpty() ? m_nodeData.name : (emoji + " " + m_nodeData.name);
    }

    setText(0, m_nodeData.displayName);
    setIcon(0, QIcon(getNodeIcon()));

    // 设置工具提示（使用实际名称）
    QString tooltip = m_nodeData.name;
    if (!m_nodeData.fullName.isEmpty() && m_nodeData.fullName != m_nodeData.name) {
        tooltip += QObject::tr("\n完整名称: %1").arg(m_nodeData.fullName);
    }
    if (!m_nodeData.database.isEmpty()) {
        tooltip += QObject::tr("\n数据库: %1").arg(m_nodeData.database);
    }
    setToolTip(0, tooltip);

    // 设置节点样式
    if (m_nodeData.type == DbNodeType::Loading) {
        setForeground(0, QBrush(QColor(128, 128, 128)));
        setFont(0, QFont("Arial", 9, QFont::Normal, true));
    }
}

QString DbTreeWidgetItem::getNodeIcon() const {
    QString iconName = DatabaseHierarchyManager::instance().getNodeIcon(m_nodeData.type);
    return QString(":/icons/%1.png").arg(iconName);
}

QString DbTreeWidgetItem::getNodeEmoji() const {
    switch (m_nodeData.type) {
    case DbNodeType::Connection:
        return "🔗";
    case DbNodeType::Database:
        return "🗃️";
    case DbNodeType::Schema:
        return "📁";
    case DbNodeType::TableFolder:
    case DbNodeType::ViewFolder:
    case DbNodeType::ProcedureFolder:
    case DbNodeType::FunctionFolder:
    case DbNodeType::TriggerFolder:
    case DbNodeType::IndexFolder:
    case DbNodeType::SequenceFolder:
    case DbNodeType::UserFolder:
        return "📁";
    case DbNodeType::Table:
        return "📋";
    case DbNodeType::View:
        return "👁";
    case DbNodeType::Procedure:
    case DbNodeType::Function:
        return "⚡";
    case DbNodeType::Trigger:
        return "🔧";
    case DbNodeType::Index:
        return "🗂";
    case DbNodeType::Sequence:
        return "🔢";
    case DbNodeType::User:
        return "👤";
    case DbNodeType::Column:
        return "🏷";
    case DbNodeType::Key:
    case DbNodeType::RedisKey:
        return "🔑";
    case DbNodeType::Loading:
    default:
        return "";
    }
}

void DbTreeWidgetItem::setLoading(bool loading) {
    if (loading) {
        m_nodeData.type = DbNodeType::Loading;
        m_nodeData.name = "加载中...";
    }
    updateDisplay();
}

// DatabaseTool 主类实现
DatabaseTool::DatabaseTool(QWidget* parent) : QWidget(parent), DynamicObjectBase(), m_currentItem(nullptr) {
    // 初始化连接配置表管理器
    m_configManager = new SqliteWrapper::ConnectionConfigTableManager(this);

    setupUI();
    loadConnections();
}

DatabaseTool::~DatabaseTool() {
    saveConnections();

    // 清理连接
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        it.value()->disconnectFromServer();
        it.value()->deleteLater();
    }
}

void DatabaseTool::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(5, 5, 5, 5);
    m_mainLayout->setSpacing(5);

    setupToolbar();

    // 主分割器 - 左右布局
    m_mainSplitter = new QSplitter(Qt::Horizontal);

    // 左侧连接树
    m_treeView = new DatabaseTreeView(m_mainSplitter);

    // 创建高性能树模型
    const auto m_treeModel = new DatabaseTreeModel(this);
    m_treeView->m_treeModel = m_treeModel;
    m_treeView->setModel(m_treeModel);

    // 强制设置基本显示属性
    m_treeView->setVisible(true);
    m_treeView->setMinimumHeight(100);
    m_treeView->setMinimumWidth(100);
    m_treeView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QObject::connect(m_treeView, &DatabaseTreeView::connectionRequested, this, [this](const QString& name) {
        onConnectToDatabase(name);
    });
    QObject::connect(m_treeView, &DatabaseTreeView::tableDoubleClicked, this, &DatabaseTool::onTableDoubleClicked);
    QObject::connect(m_treeView, &DatabaseTreeView::keyDoubleClicked, this, &DatabaseTool::onKeyDoubleClicked);

    // 连接 TreeView 右键菜单中的「新建查询」action
    QObject::connect(m_treeView->m_newQueryAction, &QAction::triggered, this, &DatabaseTool::onNewQuery);
    QObject::connect(m_treeView, &DatabaseTreeView::designTableRequested, this, &DatabaseTool::onDesignTable);

    // 右侧查询标签页
    m_tabWidget = new QTabWidget();
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setMovable(true);

    QObject::connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &DatabaseTool::onCloseTab);
    QObject::connect(m_tabWidget, &QTabWidget::currentChanged, this, &DatabaseTool::onTabChanged);

    m_mainSplitter->addWidget(m_treeView);
    m_mainSplitter->addWidget(m_tabWidget);
    // 设置分割器比例 - 给左侧一些空间
    m_mainSplitter->setSizes(QList { 250, 800 });
    m_mainSplitter->setStretchFactor(0, 0); // 左侧固定
    m_mainSplitter->setStretchFactor(1, 1); // 右侧拉伸

    m_mainLayout->addWidget(m_mainSplitter, 1);

    setupStatusBar();
}

void DatabaseTool::setupToolbar() {
    m_toolbar = new QToolBar();
    m_toolbar->setFixedHeight(40);

    m_newConnectionAction = new QAction("➕ 新建连接", this);
    QObject::connect(m_newConnectionAction, &QAction::triggered, this, &DatabaseTool::onNewConnection);
    m_toolbar->addAction(m_newConnectionAction);

    m_editConnectionAction = new QAction("✏️ 编辑连接", this);
    QObject::connect(m_editConnectionAction, &QAction::triggered, this, &DatabaseTool::onEditConnection);
    m_toolbar->addAction(m_editConnectionAction);

    m_deleteConnectionAction = new QAction("🗑️ 删除连接", this);
    QObject::connect(m_deleteConnectionAction, &QAction::triggered, this, &DatabaseTool::onDeleteConnection);
    m_toolbar->addAction(m_deleteConnectionAction);

    m_toolbar->addSeparator();

    m_connectAction = new QAction("🔌 连接", this);
    QObject::connect(m_connectAction, &QAction::triggered, this, [this]() {
        onConnectToDatabase();
    });
    m_toolbar->addAction(m_connectAction);

    m_disconnectAction = new QAction("🔌 断开", this);
    QObject::connect(m_disconnectAction, &QAction::triggered, this, &DatabaseTool::onDisconnectFromDatabase);
    m_toolbar->addAction(m_disconnectAction);

    m_refreshAction = new QAction("🔄 刷新", this);
    QObject::connect(m_refreshAction, &QAction::triggered, this, &DatabaseTool::onRefreshConnections);
    m_toolbar->addAction(m_refreshAction);

    m_toolbar->addSeparator();

    m_newQueryAction = new QAction("📝 新建查询", this);
    QObject::connect(m_newQueryAction, &QAction::triggered, this, &DatabaseTool::onNewQuery);
    m_toolbar->addAction(m_newQueryAction);

    m_mainLayout->addWidget(m_toolbar);
}

void DatabaseTool::setupStatusBar() {
    m_statusBar = new QWidget();
    m_statusBar->setFixedHeight(25);

    QHBoxLayout* statusLayout = new QHBoxLayout(m_statusBar);
    statusLayout->setContentsMargins(5, 2, 5, 2);

    m_statusLabel = new QLabel(tr("就绪"));
    m_connectionCountLabel = new QLabel(tr("连接数: 0"));

    statusLayout->addWidget(m_statusLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(m_connectionCountLabel);

    m_mainLayout->addWidget(m_statusBar);
}

void DatabaseTool::onNewConnection() {
    ConnectionDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        Connx::ConnectionConfig config = dialog.getConnectionConfig();

        if (m_connectionConfigs.contains(config.name)) {
            m_statusLabel->setText(tr("❌ 连接名称已存在，请使用不同的名称"));
            return;
        }

        m_connectionConfigs[config.name] = config;

        // 保存到SQLite数据库
        SqliteWrapper::ConnectionConfigEntity configEntity;
        configEntity.name = config.name;
        configEntity.type = config.extraParams.value("connectionType", "Redis").toString();
        configEntity.host = config.host;
        configEntity.port = config.port;
        configEntity.username = config.username;
        configEntity.password = config.password;
        configEntity.databaseName = config.database;
        configEntity.timeout = config.timeout;
        configEntity.useSSL = config.useSSL;
        configEntity.extraParams = config.extraParams;
        configEntity.createdAt = QDateTime::currentDateTime();
        configEntity.updatedAt = QDateTime::currentDateTime();

        bool saved = m_configManager->saveConfig(configEntity);
        if (!saved) {
            m_statusLabel->setText(tr("❌ 无法保存连接配置到数据库"));
            return;
        }

        // 创建连接对象
        QString typeString = config.extraParams.value("connectionType", "Redis").toString();
        Connx::ConnectionType type = Connx::ConnectionFactory::getTypeFromString(typeString);
        if (Connx::Connection* connection = Connx::ConnectionFactory::createConnection(type, config)) {
            m_connections[config.name] = connection;
            m_treeView->addConnection(config.name, connection);

            QObject::connect(connection, &Connx::Connection::stateChanged, [this, config](Connx::ConnectionState state) {
                onConnectionStateChanged(config.name, state);
            });
        }

        updateConnectionStatus();
        m_statusLabel->setText("连接已保存: " + config.name);
    }
}

void DatabaseTool::onNewQuery() {
    QString connectionName = "默认";
    QString databaseName;

    // 从当前选中节点获取连接名和数据库名
    const QModelIndex currentIdx = m_treeView->currentIndex();
    if (currentIdx.isValid()) {
        const DatabaseTreeNode* current = m_treeView->m_treeModel->getNode(currentIdx);
        if (current) {
            // 获取数据库名（当前节点或父节点的 database 属性）
            databaseName = current->database;

            // 向上查找连接节点
            const DatabaseTreeNode* node = current;
            while (node && node->type != NodeType::Connection) {
                node = node->parent;
            }
            if (node) {
                connectionName = node->name;
            }
        }
    }

    QueryTab* tab = createQueryTab(connectionName);
    if (tab) {
        // 如果有数据库名，直接 USE 切换（不显示在编辑器中）
        if (!databaseName.isEmpty()) {
            tab->selectDatabase(databaseName);
        }
        QString tabTitle = databaseName.isEmpty()
            ? tr("查询 %1").arg(m_tabWidget->count() + 1)
            : tr("查询 - %1").arg(databaseName);
        int index = m_tabWidget->addTab(tab, tabTitle);
        m_tabWidget->setCurrentIndex(index);
    }
}

QueryTab* DatabaseTool::createQueryTab(const QString& connectionName) {
    Connx::Connection* connection = m_connections.value(connectionName, nullptr);

    QueryTab* tab = new QueryTab(connection);

    QObject::connect(tab, &QueryTab::titleChanged, [this, tab](const QString& title) {
        int index = m_tabWidget->indexOf(tab);
        if (index >= 0) {
            m_tabWidget->setTabText(index, title);
        }
    });

    return tab;
}

void DatabaseTool::onCloseTab(int index) const {
    QWidget* tab = m_tabWidget->widget(index);
    m_tabWidget->removeTab(index);
    tab->deleteLater();
}

void DatabaseTool::loadConnections() {
    // 从SQLite加载所有连接配置
    QList<SqliteWrapper::ConnectionConfigEntity> configs = m_configManager->getAllConfigs();

    for (const SqliteWrapper::ConnectionConfigEntity& configEntity : configs) {
        Connx::ConnectionConfig config;
        config.name = configEntity.name;
        config.host = configEntity.host;
        config.port = configEntity.port;
        config.username = configEntity.username;
        config.password = configEntity.password;
        config.database = configEntity.databaseName;
        config.timeout = configEntity.timeout;
        config.useSSL = configEntity.useSSL;
        config.extraParams = configEntity.extraParams;

        m_connectionConfigs[config.name] = config;

        // 创建连接对象
        QString typeString = configEntity.type;

        // 修复历史数据中可能错误的连接类型
        if (typeString.isEmpty() || typeString == "Unknown" || (typeString == "Redis" && config.port != 6379 && !config.database.contains(QRegularExpression("^\\d+$")))) {
            // 根据端口和配置推断连接类型
            if (config.port == 3306 && !config.database.isEmpty()) {
                typeString = "MySQL";
            } else if (config.port == 5432) {
                typeString = "PostgreSQL";
            } else if (config.port == 6379 || config.database.contains(QRegularExpression("^\\d+$"))) {
                typeString = "Redis";
            } else if (config.extraParams.contains("connectionType")) {
                typeString = config.extraParams["connectionType"].toString();
            } else {
                // 最后的推断：如果有数据库名且端口不是Redis默认端口，推测为SQL数据库
                if (!config.database.isEmpty() && !config.database.contains(QRegularExpression("^\\d+$"))) {
                    typeString = "MySQL"; // MySQL是最常见的SQL数据库
                } else {
                    typeString = "Redis"; // 默认
                }
            }

            qDebug() << "Connection type corrected for" << config.name << "from" << configEntity.type << "to" << typeString;
        }

        const Connx::ConnectionType type = Connx::ConnectionFactory::getTypeFromString(typeString);
        if (Connx::Connection* connection = Connx::ConnectionFactory::createConnection(type, config)) {
            m_connections[config.name] = connection;
            m_treeView->addConnection(config.name, connection);

            QObject::connect(connection, &Connx::Connection::stateChanged, [this, config](const Connx::ConnectionState state) {
                onConnectionStateChanged(config.name, state);
            });
        }
    }

    updateConnectionStatus();
    m_statusLabel->setText(tr("已加载 %1 个连接配置").arg(configs.size()));
}

void DatabaseTool::saveConnections() {
    // 连接配置已经实时保存到SQLite，这里不需要额外操作
    qDebug() << "连接配置已持久化到SQLite数据库";
}

void DatabaseTool::updateConnectionStatus() const {
    m_connectionCountLabel->setText(tr("连接数: %1").arg(m_connections.size()));
}

void DatabaseTool::onConnectToDatabase(const QString& connName) {
    QString connectionName = connName;

    // 如果没有传入连接名，从当前选中节点获取
    if (connectionName.isEmpty()) {
        QModelIndex currentIdx = m_treeView->currentIndex();
        DatabaseTreeNode* current = currentIdx.isValid() ? m_treeView->m_treeModel->getNode(currentIdx) : nullptr;
        if (!current || current->type != NodeType::Connection)
            return;
        connectionName = current->name;
    }

    if (!m_connections.contains(connectionName))
        return;

    Connx::Connection* connection = m_connections[connectionName];
    if (connection->isConnected()) {
        m_statusLabel->setText("已连接到: " + connectionName);
        return;
    }

    m_statusLabel->setText(tr("正在连接..."));

    bool success = connection->connectToServer();
    if (success) {
        m_statusLabel->setText("连接成功: " + connectionName);
        m_treeView->refreshConnection(connectionName);

        // 连接成功后自动展开连接节点
        QModelIndexList indexes = m_treeView->m_treeModel->findNodes(connectionName, NodeType::Connection);
        if (indexes.isEmpty()) {
            // findNodes 可能未实现，手动查找
            auto* rootNode = m_treeView->m_treeModel->getNode(QModelIndex());
            if (rootNode) {
                for (int i = 0; i < rootNode->children.size(); ++i) {
                    if (rootNode->children[i]->name == connectionName) {
                        QModelIndex idx = m_treeView->m_treeModel->index(i, 0, QModelIndex());
                        m_treeView->expand(idx);
                        break;
                    }
                }
            }
        } else {
            m_treeView->expand(indexes.first());
        }
    } else {
        m_statusLabel->setText("连接失败: " + connection->getLastError());

        // 连接失败时折叠节点，防止箭头闪动
        auto* rootNode = m_treeView->m_treeModel->getNode(QModelIndex());
        if (rootNode) {
            for (int i = 0; i < rootNode->children.size(); ++i) {
                if (rootNode->children[i]->name == connectionName) {
                    QModelIndex idx = m_treeView->m_treeModel->index(i, 0, QModelIndex());
                    m_treeView->collapse(idx);
                    break;
                }
            }
        }
    }
}

void DatabaseTool::onTableDoubleClicked(const QString& connectionName, const QString& database, const QString& table) {
    QueryTab* tab = createQueryTab(connectionName);
    if (tab) {
        tab->selectDatabase(database);
        tab->setQuery(QString("SELECT * FROM %1 LIMIT 100").arg(table));
        int index = m_tabWidget->addTab(tab, QString("%1.%2").arg(database, table));
        m_tabWidget->setCurrentIndex(index);
    }
}

void DatabaseTool::onKeyDoubleClicked(const QString& connectionName, const QString& database, const QString& key) {
    // 为Redis键创建查询
    QueryTab* tab = createQueryTab(connectionName);
    if (tab) {
        tab->setQuery(QString("GET %1").arg(key));
        int index = m_tabWidget->addTab(tab, QString("Key: %1").arg(key));
        m_tabWidget->setCurrentIndex(index);
    }
}

void DatabaseTool::onDesignTable(const QString& connectionName, const QString& database, const QString& tableName) {
    Connx::Connection* connection = m_connections.value(connectionName, nullptr);
    if (!connection) return;

    auto* designer = new TableDesigner(connection, database, tableName);
    QString tabTitle = QString("设计: %1.%2").arg(database, tableName);
    int index = m_tabWidget->addTab(designer, tabTitle);
    m_tabWidget->setCurrentIndex(index);

    connect(designer, &TableDesigner::tableSaved, this, [this, connectionName]() {
        m_treeView->refreshConnection(connectionName);
    });
}

void DatabaseTool::onConnectionStateChanged(const QString& name, Connx::ConnectionState state) {
    Q_UNUSED(name)
    Q_UNUSED(state)
    updateConnectionStatus();
}

// 其他槽函数实现
void DatabaseTool::onEditConnection() {
    QModelIndex currentIdx = m_treeView->currentIndex();
    DatabaseTreeNode* current = currentIdx.isValid() ? m_treeView->m_treeModel->getNode(currentIdx) : nullptr;
    if (!current)
        return;

    if (!current || current->type != NodeType::Connection)
        return;

    QString connectionName = current->name;
    if (!m_connectionConfigs.contains(connectionName))
        return;

    Connx::ConnectionConfig config = m_connectionConfigs[connectionName];

    ConnectionDialog dialog(config, this);
    if (dialog.exec() == QDialog::Accepted) {
        Connx::ConnectionConfig newConfig = dialog.getConnectionConfig();

        // 更新配置
        m_connectionConfigs[connectionName] = newConfig;

        // 保存到SQLite
        QVariantMap configMap;
        configMap["name"] = newConfig.name;
        configMap["type"] = newConfig.extraParams.value("connectionType", "Redis").toString();
        configMap["host"] = newConfig.host;
        configMap["port"] = newConfig.port;
        configMap["username"] = newConfig.username;
        configMap["password"] = newConfig.password;
        configMap["database"] = newConfig.database;
        configMap["timeout"] = newConfig.timeout;
        configMap["useSSL"] = newConfig.useSSL;
        configMap["extraParams"] = QVariant::fromValue(newConfig.extraParams);

        // 将configMap转换为ConnectionConfigEntity
        SqliteWrapper::ConnectionConfigEntity entity;
        entity.name = connectionName;
        entity.type = configMap["type"].toString();
        entity.host = configMap["host"].toString();
        entity.port = configMap["port"].toInt();
        entity.username = configMap["username"].toString();
        entity.password = configMap["password"].toString();
        entity.databaseName = configMap["database"].toString();
        entity.timeout = configMap["timeout"].toInt();
        entity.useSSL = configMap["useSSL"].toBool();
        entity.extraParams = configMap["extraParams"].toMap();

        m_configManager->updateConfig(connectionName, entity);
        m_statusLabel->setText("连接已更新: " + connectionName);
    }
}

void DatabaseTool::onDeleteConnection() {
    QModelIndex currentIdx = m_treeView->currentIndex();
    DatabaseTreeNode* current = currentIdx.isValid() ? m_treeView->m_treeModel->getNode(currentIdx) : nullptr;
    if (!current)
        return;

    if (!current || current->type != NodeType::Connection)
        return;

    QString connectionName = current->name;

    int ret = QMessageBox::question(this, "删除连接",
                                    tr("确定要删除连接 '%1' 吗？").arg(connectionName),
                                    QMessageBox::Yes | QMessageBox::No);

    if (ret == QMessageBox::Yes) {
        // 从SQLite删除
        if (m_configManager->deleteConfigByName(connectionName)) {
            // 断开并删除连接
            if (m_connections.contains(connectionName)) {
                m_connections[connectionName]->disconnectFromServer();
                m_connections[connectionName]->deleteLater();
                m_connections.remove(connectionName);
            }

            m_connectionConfigs.remove(connectionName);
            m_treeView->removeConnection(connectionName);

            updateConnectionStatus();
            m_statusLabel->setText("连接已删除: " + connectionName);
        } else {
            m_statusLabel->setText(tr("❌ 无法从数据库删除连接配置"));
        }
    }
}

void DatabaseTool::onDisconnectFromDatabase() {
    QModelIndex currentIdx = m_treeView->currentIndex();
    DatabaseTreeNode* current = currentIdx.isValid() ? m_treeView->m_treeModel->getNode(currentIdx) : nullptr;
    if (!current)
        return;

    if (!current || current->type != NodeType::Connection)
        return;

    QString connectionName = current->name;
    if (!m_connections.contains(connectionName))
        return;

    Connx::Connection* connection = m_connections[connectionName];
    if (connection->isConnected()) {
        connection->disconnectFromServer();
        m_statusLabel->setText("已断开连接: " + connectionName);
        m_treeView->refreshConnection(connectionName);
    }
}

void DatabaseTool::onRefreshConnections() {
    // 重新加载所有连接配置
    m_connectionConfigs.clear();

    // 清理现有连接
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        it.value()->disconnectFromServer();
        it.value()->deleteLater();
    }
    m_connections.clear();

    // 清空树
    // 清空模型数据 - 在 Model/View 架构中由模型处理
    // m_treeWidget->m_treeModel->clear(); // TODO: 实现模型的 clear 方法

    // 重新加载
    loadConnections();

    m_statusLabel->setText(tr("连接列表已刷新"));
}

void DatabaseTool::onTabChanged(int index) {
    Q_UNUSED(index)
    // 可以在这里添加标签页切换的逻辑
}

QueryTab* DatabaseTool::getCurrentQueryTab() {
    return qobject_cast<QueryTab*>(m_tabWidget->currentWidget());
}
