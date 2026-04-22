#include "databasetreeview.h"
#include "databasetool.h"
#include <QMessageBox>
#include <QHeaderView>

// DatabaseTreeView 实现
DatabaseTreeView::DatabaseTreeView(QWidget* parent) : QTreeView(parent) {

    setupUI();
    setupContextMenu();

    // 连接信号
    connect(this, &QTreeView::doubleClicked, this, &DatabaseTreeView::onNodeDoubleClicked);
    connect(this, &QTreeView::customContextMenuRequested, this, &DatabaseTreeView::onCustomContextMenuRequested);
    connect(this, &QTreeView::expanded, this, &DatabaseTreeView::onNodeExpanded);
}

void DatabaseTreeView::setupUI() {
    // 设置头部
    header()->hide();
    // 基本属性
    setContextMenuPolicy(Qt::CustomContextMenu);
    setRootIsDecorated(true);
    setAlternatingRowColors(true);
    setExpandsOnDoubleClick(false); // 禁用默认双击展开，我们自己处理

    // 性能优化设置
    setUniformRowHeights(true);
    setAnimated(true);
    setIndentation(20);

    // 启用虚拟滚动以提高大数据集性能
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

    // 减少重绘次数
    setUpdatesEnabled(true);
}

void DatabaseTreeView::setupContextMenu() {
    m_contextMenu = new QMenu(this);

    m_connectAction = new QAction("🔌 连接", this);
    m_disconnectAction = new QAction("🔌 断开连接", this);
    m_refreshAction = new QAction("🔄 刷新连接", this);
    m_refreshDatabaseAction = new QAction("🔄 刷新数据库", this);
    m_flushDatabaseAction = new QAction("🗑️ 清空数据库", this);
    m_deleteKeyAction = new QAction("❌ 删除键", this);
    m_deleteAction = new QAction("🗑️ 删除连接", this);
    m_newQueryAction = new QAction("📝 新建查询", this);

    m_contextMenu->addAction(m_connectAction);
    m_contextMenu->addAction(m_disconnectAction);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction(m_refreshAction);
    m_contextMenu->addAction(m_refreshDatabaseAction);
    m_contextMenu->addAction(m_flushDatabaseAction);
    m_contextMenu->addAction(m_deleteKeyAction);
    m_contextMenu->addAction(m_newQueryAction);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction(m_deleteAction);
}

void DatabaseTreeView::addConnection(const QString& name, Connx::Connection* connection) const {
    m_treeModel->addConnection(name, connection);
}

void DatabaseTreeView::onNodeDoubleClicked(const QModelIndex& index) {
    if (!index.isValid()) {
        return;
    }

    handleNodeDoubleClick(index);
}

void DatabaseTreeView::handleNodeDoubleClick(const QModelIndex& index) {
    if (!index.isValid()) {
        return;
    }

    // 从模型获取节点信息
    DatabaseTreeNode* node = m_treeModel->getNode(index);
    if (!node) {
        return;
    }

    // 智能展开/折叠逻辑
    if (node->canExpand) {
        if (isExpanded(index)) {
            // 如果已展开，则折叠
            collapse(index);
        } else {
            // 如果未展开，则展开
            if (node->type == NodeType::Connection) {
                // 连接双击时建立连接，连接成功后会自动刷新和展开
                emit connectionRequested(node->connectionId);
                return; // 不在这里 expand，等连接成功后再展开
            }

            // 非连接节点直接展开
            expand(index);
        }
    } else if (node->type == NodeType::Table) {
        // 表双击处理
        emit tableDoubleClicked(node->connectionId, node->database, node->name);
    } else if (node->type == NodeType::RedisKey) {
        // Redis键双击处理
        emit keyDoubleClicked(node->connectionId, node->database, node->name);
    }
}

void DatabaseTreeView::onNodeExpanded(const QModelIndex& index) {
    if (!index.isValid())
        return;

    // 获取节���信息
    DatabaseTreeNode* node = m_treeModel->getNode(index);
    if (!node)
        return;

    qDebug() << "展开节点:" << node->displayName;

    // 模型会自动处理数据加载，这里只需要记录日志或做一些UI更新
}

void DatabaseTreeView::showExpandError(const QString& title, const QString& message) {
    QMessageBox::warning(this, title, message);
}


void DatabaseTreeView::loadFolderContent(QTreeWidgetItem* folderItem, Connx::Connection* connection, const QString& database, DbNodeType folderType) {
    // Deprecated: Model/View architecture handles this automatically
    Q_UNUSED(folderItem)
    Q_UNUSED(connection)
    Q_UNUSED(database)
    Q_UNUSED(folderType)
}

void DatabaseTreeView::loadTables(QTreeWidgetItem* databaseItem, Connx::Connection* connection, const QString& database) {
    // Deprecated: Model/View architecture handles this automatically
    Q_UNUSED(databaseItem)
    Q_UNUSED(connection)
    Q_UNUSED(database)
}

void DatabaseTreeView::onCustomContextMenuRequested(const QPoint& pos) {
    QModelIndex index = indexAt(pos);
    if (!index.isValid()) {
        return;
    }

    showNodeContextMenu(index, pos);
}

void DatabaseTreeView::showNodeContextMenu(const QModelIndex& index, const QPoint& pos) {
    if (!index.isValid())
        return;

    // 从模型获取节点信���
    DatabaseTreeNode* node = m_treeModel->getNode(index);
    if (!node)
        return;

    // 根据节点类型启用/禁用菜单项
    m_connectAction->setVisible(node->type == NodeType::Connection);
    m_disconnectAction->setVisible(node->type == NodeType::Connection);
    m_deleteAction->setVisible(node->type == NodeType::Connection);
    m_refreshAction->setVisible(node->type == NodeType::Connection);
    m_refreshDatabaseAction->setVisible(node->type == NodeType::Database || node->type == NodeType::RedisDatabase);
    m_flushDatabaseAction->setVisible(node->type == NodeType::Database || node->type == NodeType::RedisDatabase);
    m_deleteKeyAction->setVisible(node->type == NodeType::RedisKey);
    m_newQueryAction->setVisible(
        node->type == NodeType::Connection ||
        node->type == NodeType::Database ||
        node->type == NodeType::RedisDatabase ||
        node->type == NodeType::TableFolder ||
        node->type == NodeType::ViewFolder ||
        node->type == NodeType::FunctionFolder ||
        node->type == NodeType::ProcedureFolder ||
        node->type == NodeType::TriggerFolder ||
        node->type == NodeType::EventFolder ||
        node->type == NodeType::Table ||
        node->type == NodeType::View);

    // 连接信号槽
    QObject::disconnect(m_refreshDatabaseAction, nullptr, nullptr, nullptr);
    QObject::disconnect(m_flushDatabaseAction, nullptr, nullptr, nullptr);
    QObject::disconnect(m_deleteKeyAction, nullptr, nullptr, nullptr);

    if (node->type == NodeType::Database || node->type == NodeType::RedisDatabase) {
        QObject::connect(m_refreshDatabaseAction, &QAction::triggered, [this, index]() {
            // 刷新数据库节点
            m_treeModel->refreshNode(index);
        });
        QObject::connect(m_flushDatabaseAction, &QAction::triggered, [this, node]() {
            // 实现数据库清空逻辑
            // TODO: 实现数据库刷新功能
        });
    } else if (node->type == NodeType::RedisKey) {
        QObject::connect(m_deleteKeyAction, &QAction::triggered, [this, node]() {
            // 实现键删除逻辑
            // TODO: 实现Redis键删除功能
        });
    }

    m_contextMenu->exec(mapToGlobal(pos));
}

void DatabaseTreeView::loadRedisKeys(QTreeWidgetItem* databaseItem, Connx::Connection* connection, const QString& database) {
    // Deprecated: Model/View architecture handles this automatically
    Q_UNUSED(databaseItem)
    Q_UNUSED(connection)
    Q_UNUSED(database)
}

void DatabaseTreeView::refreshDatabase(QTreeWidgetItem* databaseItem) {
    // Deprecated: Model/View architecture handles this automatically
    Q_UNUSED(databaseItem)
}

void DatabaseTreeView::flushDatabase(QTreeWidgetItem* databaseItem) {
    // Deprecated: Model/View architecture handles this automatically
    Q_UNUSED(databaseItem)
}

void DatabaseTreeView::deleteKey(QTreeWidgetItem* keyItem) {
    // Deprecated: Model/View architecture handles this automatically
    Q_UNUSED(keyItem)
}

void DatabaseTreeView::setConnection(Connx::Connection* connection) {
    // 简化实现
    Q_UNUSED(connection)
}

void DatabaseTreeView::refreshConnection(const QString& connectionName) {
    // 通过模型刷新连接
    m_treeModel->refreshConnection(connectionName);
}

void DatabaseTreeView::removeConnection(const QString& name) {
    // 通过模型移除连接
    m_treeModel->removeConnection(name);
}

// DatabaseTreeView 新增的辅助方法实现

NodeChain DatabaseTreeView::buildNodeChainFromIndex(const QModelIndex& index) {
    if (!index.isValid()) {
        return NodeChain();
    }

    // 使用模型的方法构建节点链
    DatabaseTreeNode* node = m_treeModel->getNode(index);
    if (!node) {
        return NodeChain();
    }

    return m_treeModel->buildNodeChain(node);
}

// 重命名的旧方法，现在用作占位符
NodeChain DatabaseTreeView::buildNodeChainFromTreeItem_DEPRECATED(QTreeWidgetItem* item) {
    Q_UNUSED(item)
    return NodeChain(); // 简化实现
}

Node DatabaseTreeView::createNodeFromTreeItem(QTreeWidgetItem* item) {
    // Deprecated: 简化实现避免编译错误
    Q_UNUSED(item)
    return Node();
}


NodeType DatabaseTreeView::convertDbNodeTypeToNodeType(DbNodeType dbNodeType) {
    switch (dbNodeType) {
    case DbNodeType::Connection: return NodeType::Connection;
    case DbNodeType::Database: return NodeType::Database;
    case DbNodeType::Schema: return NodeType::Schema;
    case DbNodeType::TableFolder: return NodeType::TableFolder;
    case DbNodeType::Table: return NodeType::Table;
    case DbNodeType::ViewFolder: return NodeType::ViewFolder;
    case DbNodeType::View: return NodeType::View;
    case DbNodeType::ProcedureFolder: return NodeType::ProcedureFolder;
    case DbNodeType::Procedure: return NodeType::Procedure;
    case DbNodeType::FunctionFolder: return NodeType::FunctionFolder;
    case DbNodeType::Function: return NodeType::Function;
    case DbNodeType::TriggerFolder: return NodeType::TriggerFolder;
    case DbNodeType::Trigger: return NodeType::Trigger;
    case DbNodeType::IndexFolder: return NodeType::IndexFolder;
    case DbNodeType::Index: return NodeType::Index;
    case DbNodeType::SequenceFolder: return NodeType::SequenceFolder;
    case DbNodeType::Sequence: return NodeType::Sequence;
    case DbNodeType::UserFolder: return NodeType::UserFolder;
    case DbNodeType::User: return NodeType::User;
    case DbNodeType::Column: return NodeType::Column;
    case DbNodeType::Key: return NodeType::Key;
    case DbNodeType::RedisKey: return NodeType::RedisKey;
    default: return NodeType::Unknown;
    }
}

bool DatabaseTreeView::canExpandNodeType(NodeType type) {
    switch (type) {
    case NodeType::Connection:
    case NodeType::Database:
    case NodeType::RedisDatabase:
    case NodeType::TableFolder:
    case NodeType::ViewFolder:
    case NodeType::ProcedureFolder:
    case NodeType::FunctionFolder:
    case NodeType::TriggerFolder:
    case NodeType::IndexFolder:
    case NodeType::SequenceFolder:
    case NodeType::UserFolder:
    case NodeType::Table:
    case NodeType::View:
    case NodeType::RedisKeyFolder:
        return true;
    default:
        return false;
    }
}

void DatabaseTreeView::updateTreeItemWithNodes(QTreeWidgetItem* parentItem, const QList<Node>& nodes) {
    // Deprecated: Model/View architecture handles this automatically
    Q_UNUSED(parentItem)
    Q_UNUSED(nodes)
}


void DatabaseTreeView::populateTreeItemData(QTreeWidgetItem* item, const Node& node) {
    // Deprecated: Model/View architecture handles this automatically
    Q_UNUSED(item)
    Q_UNUSED(node)
}

DbNodeType DatabaseTreeView::convertNodeTypeToDbNodeType(NodeType nodeType) {
    switch (nodeType) {
    case NodeType::Connection: return DbNodeType::Connection;
    case NodeType::Database: return DbNodeType::Database;
    case NodeType::Schema: return DbNodeType::Schema;
    case NodeType::TableFolder: return DbNodeType::TableFolder;
    case NodeType::Table: return DbNodeType::Table;
    case NodeType::ViewFolder: return DbNodeType::ViewFolder;
    case NodeType::View: return DbNodeType::View;
    case NodeType::ProcedureFolder: return DbNodeType::ProcedureFolder;
    case NodeType::Procedure: return DbNodeType::Procedure;
    case NodeType::FunctionFolder: return DbNodeType::FunctionFolder;
    case NodeType::Function: return DbNodeType::Function;
    case NodeType::TriggerFolder: return DbNodeType::TriggerFolder;
    case NodeType::Trigger: return DbNodeType::Trigger;
    case NodeType::IndexFolder: return DbNodeType::IndexFolder;
    case NodeType::Index: return DbNodeType::Index;
    case NodeType::SequenceFolder: return DbNodeType::SequenceFolder;
    case NodeType::Sequence: return DbNodeType::Sequence;
    case NodeType::UserFolder: return DbNodeType::UserFolder;
    case NodeType::User: return DbNodeType::User;
    case NodeType::Column: return DbNodeType::Column;
    case NodeType::Key: return DbNodeType::Key;
    case NodeType::RedisKey: return DbNodeType::RedisKey;
    default: return DbNodeType::Loading; // 使用Loading作为默认值
    }
}

void DatabaseTreeView::setNodeIcon(QTreeWidgetItem* item, NodeType nodeType) {
    // Deprecated: Model/View architecture handles this automatically
    Q_UNUSED(item)
    Q_UNUSED(nodeType)
}

bool DatabaseTreeView::canNodeExpand(NodeType nodeType) {
    switch (nodeType) {
    case NodeType::Connection:
    case NodeType::Database:
    case NodeType::RedisDatabase:
    case NodeType::TableFolder:
    case NodeType::ViewFolder:
    case NodeType::ProcedureFolder:
    case NodeType::FunctionFolder:
    case NodeType::TriggerFolder:
    case NodeType::IndexFolder:
    case NodeType::SequenceFolder:
    case NodeType::UserFolder:
    case NodeType::Table:
    case NodeType::View:
    case NodeType::RedisKeyFolder:
        return true;
    default:
        return false;
    }
}

QString DatabaseTreeView::generateNodeTooltip(const Node& node) {
    QStringList parts;

    parts << tr("类型: %1").arg(getNodeTypeDisplayName(node.type));

    if (!node.fullName.isEmpty()) {
        parts << tr("完整名称: %1").arg(node.fullName);
    }

    if (!node.database.isEmpty()) {
        parts << tr("数据库: %1").arg(node.database);
    }

    if (node.metadata.contains("comment") && !node.metadata["comment"].toString().isEmpty()) {
        parts << tr("注释: %1").arg(node.metadata["comment"].toString());
    }

    return parts.join("\n");
}

QString DatabaseTreeView::getNodeTypeDisplayName(NodeType nodeType) {
    switch (nodeType) {
    case NodeType::Connection: return tr("连���");
    case NodeType::Database: return tr("数据库");
    case NodeType::Schema: return tr("模式");
    case NodeType::TableFolder: return tr("表文件夹");
    case NodeType::Table: return tr("表");
    case NodeType::ViewFolder: return tr("视图文件夹");
    case NodeType::View: return tr("视图");
    case NodeType::ProcedureFolder: return tr("存储过程文件夹");
    case NodeType::Procedure: return tr("存储过程");
    case NodeType::FunctionFolder: return tr("函数文件夹");
    case NodeType::Function: return tr("函数");
    case NodeType::TriggerFolder: return tr("触发器文件夹");
    case NodeType::Trigger: return tr("触发器");
    case NodeType::IndexFolder: return tr("索���文件夹");
    case NodeType::Index: return tr("索引");
    case NodeType::SequenceFolder: return tr("序列文件夹");
    case NodeType::Sequence: return tr("序列");
    case NodeType::UserFolder: return tr("用户文件夹");
    case NodeType::User: return tr("用户");
    case NodeType::Column: return tr("列");
    case NodeType::Key: return tr("键");
    case NodeType::RedisDatabase: return tr("Redis数据库");
    case NodeType::RedisKey: return tr("Redis键");
    case NodeType::RedisKeyFolder: return tr("Redis键文件夹");
    default: return tr("未知");
    }
}
