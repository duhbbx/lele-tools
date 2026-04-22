#include "databasetreemodel.h"
#include "nodeeventfactory.h"
#include <QApplication>
#include <QDebug>
#include <QIcon>
#include <QFont>
#include <QPalette>
#include <QtConcurrent>
#include <QThread>

// DatabaseTreeLoader 实现
DatabaseTreeLoader::DatabaseTreeLoader(QObject* parent)
    : QObject(parent)
    , m_loading(false) {

    m_loadTimer = new QTimer(this);
    m_loadTimer->setSingleShot(true);
    m_loadTimer->setInterval(10); // 10ms延迟批处理
    connect(m_loadTimer, &QTimer::timeout, this, &DatabaseTreeLoader::processLoadRequest);

    m_watcher = new QFutureWatcher<void>(this);
    connect(m_watcher, &QFutureWatcher<void>::finished, this, &DatabaseTreeLoader::onLoadFinished);
}

void DatabaseTreeLoader::loadNodeChildren(const QString& connectionId, const NodeChain& nodeChain, DatabaseTreeNode* node) {
    QMutexLocker locker(&m_queueMutex);

    // 检查节点是否已在加载队列中
    for (const auto& request : m_loadQueue) {
        if (request.node == node) {
            return; // 已在队列中，避免重复加载
        }
    }

    m_loadQueue.enqueue(LoadRequest(connectionId, nodeChain, node));
    node->isLoading = true;

    if (!m_loadTimer->isActive()) {
        m_loadTimer->start();
    }
}

void DatabaseTreeLoader::setConnection(const QString& connectionId, Connx::Connection* connection) {
    m_connections[connectionId] = connection;
    qDebug() << "[Loader] setConnection:" << connectionId << "ptr:" << connection << "connected:" << (connection ? connection->isConnected() : false);
}

void DatabaseTreeLoader::processLoadRequest() {
    if (m_loading || m_loadQueue.isEmpty()) {
        return;
    }

    QMutexLocker locker(&m_queueMutex);
    LoadRequest request = m_loadQueue.dequeue();
    locker.unlock();

    m_loading = true;

    // 同步加载（主线程），确保 QTcpSocket（Redis）等可正常使用
    try {
        qDebug() << "[Loader] Processing node:" << request.node->name
                 << "type:" << (int)request.node->type
                 << "connId:" << request.connectionId
                 << "chain:" << request.nodeChain.toString();

        Connx::Connection* connection = m_connections.value(request.connectionId);
        if (!connection) {
            qDebug() << "[Loader] Connection not found for:" << request.connectionId;
            emit nodeLoadFailed(request.node, "Connection not found");
            m_loading = false;
            if (!m_loadQueue.isEmpty()) m_loadTimer->start();
            return;
        }

        auto expandEvent = NodeEventFactory::createExpandEvent(connection, request.nodeChain);
        if (!expandEvent) {
            qDebug() << "[Loader] Failed to create expand event";
            emit nodeLoadFailed(request.node, "Failed to create expand event");
            m_loading = false;
            if (!m_loadQueue.isEmpty()) m_loadTimer->start();
            return;
        }

        ExpandResult result = expandEvent->execute();
        qDebug() << "[Loader] Result: success=" << result.success
                 << "children=" << result.childNodes.size()
                 << "error=" << result.errorMessage;
        if (result.success) {
            emit nodeChildrenLoaded(request.node, result.childNodes);
        } else {
            emit nodeLoadFailed(request.node, result.errorMessage);
        }
    } catch (const std::exception& e) {
        emit nodeLoadFailed(request.node, QString("Exception: %1").arg(e.what()));
    } catch (...) {
        emit nodeLoadFailed(request.node, "Unknown error occurred");
    }

    m_loading = false;
    if (!m_loadQueue.isEmpty()) m_loadTimer->start();
}

void DatabaseTreeLoader::onLoadFinished() {
    m_loading = false;

    // 继续处理队列中的下一个请求
    if (!m_loadQueue.isEmpty()) {
        m_loadTimer->start();
    }
}

// DatabaseTreeModel 实现
DatabaseTreeModel::DatabaseTreeModel(QObject* parent)
    : QAbstractItemModel(parent)
    , m_rootNode(std::make_unique<DatabaseTreeNode>())
    , m_loader(new DatabaseTreeLoader(this))
    , m_nodeCache(1000)
    , m_lazyLoadingEnabled(true)
    , m_loadBatchSize(100)
    , m_filterEnabled(false) {
    setupConnections(); // 暂时禁用
    loadRootNodes();
}

DatabaseTreeModel::~DatabaseTreeModel() {
    clearNodeCache();
}

void DatabaseTreeModel::setupConnections() {
    connect(m_loader, &DatabaseTreeLoader::nodeChildrenLoaded, this, &DatabaseTreeModel::onNodeChildrenLoaded);
    connect(m_loader, &DatabaseTreeLoader::nodeLoadFailed, this, &DatabaseTreeModel::onNodeLoadFailed);
}

QModelIndex DatabaseTreeModel::index(const int row, const int column, const QModelIndex& parent) const {
    qDebug() << "=== index() called: row=" << row << "column=" << column << "parent valid=" << parent.isValid();

    // 检查hasIndex
    bool hasIndexResult = hasIndex(row, column, parent);
    qDebug() << "hasIndex(" << row << "," << column << ", parent_valid=" << parent.isValid() << ") =" << hasIndexResult;
    if (!hasIndexResult) {
        qDebug() << "index(): hasIndex returned false, returning invalid";
        return QModelIndex();
    }

    DatabaseTreeNode* parentNode = getNode(parent);
    qDebug() << "parentNode:" << (void*)parentNode << (parentNode ? parentNode->name : "null");
    if (!parentNode) {
        qDebug() << "index(): parentNode is null, returning invalid";
        return QModelIndex();
    }

    qDebug() << "parentNode->children.size():" << parentNode->children.size();
    if (row >= 0 && row < parentNode->children.size()) {
        DatabaseTreeNode* childNode = parentNode->children.at(row);
        QModelIndex result = createIndex(row, column, childNode);
        qDebug() << "Created index for row" << row << "node:" << childNode->name << "result valid:" << result.isValid();
        return result;
    }

    qDebug() << "index(): row out of range, returning invalid index";
    return QModelIndex();
}

QModelIndex DatabaseTreeModel::parent(const QModelIndex& index) const {
    if (!index.isValid()) {
        return QModelIndex();
    }

    DatabaseTreeNode* childNode = getNode(index);
    if (!childNode || !childNode->parent || childNode->parent == m_rootNode.get()) {
        return QModelIndex();
    }

    DatabaseTreeNode* parentNode = childNode->parent;
    return createIndex(parentNode->row(), 0, parentNode);
}

int DatabaseTreeModel::rowCount(const QModelIndex& parent) const {
    DatabaseTreeNode* parentNode = getNode(parent);
    if (!parentNode) {
        qDebug() << "rowCount: parentNode is null";
        return 0;
    }

    int count = parentNode->children.size();
    qDebug() << "rowCount for" << (parent.isValid() ? parentNode->name : "root") << ":" << count;
    return count;
}

int DatabaseTreeModel::columnCount(const QModelIndex& parent) const {
    Q_UNUSED(parent)
    qDebug() << "columnCount() called, returning 1";
    return 1; // 单列显示
}

/**
 * @param index 指定了视图（QTreeView）想要的数据位置。它包含
 *              row() → 第几行
 *              column() → 第几列
 *              parent() → 父节点的 QModelIndex
 * @param role 表示请求的数据类型。Qt 里有很多 角色 (Role)，常用的有
 *             Qt::DisplayRole → 显示在单元格里的文本
 *             Qt::DecorationRole → 图标、前景图
 *             Qt::ToolTipRole → 鼠标悬停提示
 *             Qt::BackgroundRole → 背景颜色
 *             Qt::FontRole → 字体
 *             Qt::CheckStateRole → 复选框状态
 * @return 返回的数据类型用 QVariant（Qt 的万能容器，能存字符串、整数、颜色、图标等）。
 */
QVariant DatabaseTreeModel::data(const QModelIndex& index, const int role) const {
    qDebug() << "=== data() called: index valid=" << index.isValid() << "role=" << role;

    if (!index.isValid()) {
        qDebug() << "data(): index is invalid, returning empty QVariant";
        return QVariant();
    }

    qDebug() << "data(): index row=" << index.row() << "column=" << index.column();

    DatabaseTreeNode* node = getNode(index);
    qDebug() << "data(): node=" << (void*)node << (node ? node->name : "null");
    if (!node) {
        qDebug() << "data(): node is null for index row" << index.row();
        return QVariant();
    }

    switch (role) {
    case Qt::DisplayRole:
        qDebug() << "data(): returning display text:" << node->displayName;
        return node->displayName;

    case Qt::SizeHintRole: {
        QSize size(200, 25); // 明确设置项目尺寸：宽200px，高25px
        qDebug() << "data(): returning size hint:" << size;
        return size;
    }

    default:
        qDebug() << "data(): using default for role:" << role;
        break;
    }

    return QVariant();
}

Qt::ItemFlags DatabaseTreeModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant DatabaseTreeModel::headerData(const int section, const Qt::Orientation orientation, const int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
            case 0:
                return tr("数据库连接");
            default:
                break;
        }
    }
    return QVariant();
}

bool DatabaseTreeModel::hasChildren(const QModelIndex& parent) const {
    DatabaseTreeNode* node = getNode(parent);
    if (!node) {
        return false;
    }

    // 如果已经加载过子节点，直接返回结果
    if (node->isLoaded || node->hasChildrenCached) {
        return !node->children.isEmpty();
    }

    // 根据节点类型判断是否可能有子节点
    return nodeCanExpand(node->type);
}

bool DatabaseTreeModel::canFetchMore(const QModelIndex& parent) const {
    if (!m_lazyLoadingEnabled || !parent.isValid()) {
        return false;
    }

    DatabaseTreeNode* node = getNode(parent);
    if (!node || node->connectionId.isEmpty()) {
        return false;
    }

    return nodeCanExpand(node->type) && !node->isLoaded && !node->isLoading;
}

void DatabaseTreeModel::fetchMore(const QModelIndex& parent) {
    DatabaseTreeNode* node = getNode(parent);
    if (!node || node->isLoading || node->isLoaded) {
        return;
    }

    qDebug() << "[fetchMore] node:" << node->name << "type:" << (int)node->type
             << "connId:" << node->connectionId << "valid:" << parent.isValid();

    emit nodeLoadStarted(parent);
    loadNodeChildren(node);
}

void DatabaseTreeModel::addConnection(const QString& name, Connx::Connection* connection) {
    qDebug() << "=== DatabaseTreeModel::addConnection:" << name;

    m_connections[name] = connection;

    // 同步到 loader
    if (m_loader)
        m_loader->setConnection(name, connection);

    // 检查根节点
    qDebug() << "Root node before adding:" << m_rootNode.get();
    qDebug() << "Root children count before:" << (m_rootNode ? m_rootNode->children.size() : -1);

    // 使用正确的insertRows信号而不是resetModel
    const int row = m_rootNode->children.size(); // 新行的位置

    qDebug() << "Calling beginInsertRows: parent=invalid, first=" << row << "last=" << row;
    beginInsertRows(QModelIndex(), row, row); // 在根节点下插入

    // 创建并插入节点
    const auto connNode = new DatabaseTreeNode();
    connNode->id = name;
    connNode->name = name;
    connNode->displayName = name; // 去掉emoji
    connNode->type = NodeType::Connection;
    connNode->connectionId = name;
    connNode->canExpand = true;
    connNode->parent = m_rootNode.get();

    m_rootNode->children.append(connNode);
    m_connectionNodes[name] = connNode;

    qDebug() << "Created connection node:" << connNode << "name:" << connNode->name;
    qDebug() << "Root children count after:" << m_rootNode->children.size();

    // 通知视图插入完成
    endInsertRows();
    qDebug() << "endInsertRows completed";
}

void DatabaseTreeModel::removeConnection(const QString& name) {
    auto it = m_connectionNodes.find(name);
    if (it == m_connectionNodes.end()) {
        return;
    }

    DatabaseTreeNode* connNode = it.value();
    int row = connNode->row();

    beginRemoveNodes(m_rootNode.get(), row, row);
    m_rootNode->children.removeAt(row);
    delete connNode;
    m_connectionNodes.erase(it);
    m_connections.remove(name);
    endRemoveNodes();
}

void DatabaseTreeModel::refreshConnection(const QString& name) {
    auto it = m_connectionNodes.find(name);
    if (it == m_connectionNodes.end()) {
        return;
    }

    DatabaseTreeNode* connNode = it.value();

    // 清除所有子节点
    if (!connNode->children.isEmpty()) {
        beginRemoveNodes(connNode, 0, connNode->children.size() - 1);
        qDeleteAll(connNode->children);
        connNode->children.clear();
        connNode->isLoaded = false;
        connNode->hasChildrenCached = false;
        endRemoveNodes();
    }

    // 清除相关缓存
    QString cachePattern = name + "_";
    auto cacheKeys = m_nodeCache.keys();
    for (const QString& key : cacheKeys) {
        if (key.startsWith(cachePattern)) {
            m_nodeCache.remove(key);
        }
    }

    QModelIndex connIndex = getNodeIndex(connNode);
    emit dataChanged(connIndex, connIndex);
}

void DatabaseTreeModel::refreshNode(const QModelIndex& index) {
    DatabaseTreeNode* node = getNode(index);
    if (!node) {
        return;
    }

    // 清除子节点
    if (!node->children.isEmpty()) {
        beginRemoveNodes(node, 0, node->children.size() - 1);
        qDeleteAll(node->children);
        node->children.clear();
        node->isLoaded = false;
        node->hasChildrenCached = false;
        endRemoveNodes();
    }

    // 如果节点当前是展开的，重新加载
    if (nodeCanExpand(node->type)) {
        loadNodeChildren(node);
    }

    emit dataChanged(index, index);
}

DatabaseTreeNode* DatabaseTreeModel::getNode(const QModelIndex& index) const {
    if (!index.isValid()) {
        return m_rootNode.get();
    }
    return static_cast<DatabaseTreeNode*>(index.internalPointer());
}

QModelIndex DatabaseTreeModel::getNodeIndex(DatabaseTreeNode* node) const {
    if (!node || node == m_rootNode.get()) {
        return QModelIndex();
    }
    return createIndex(node->row(), 0, node);
}

NodeChain DatabaseTreeModel::buildNodeChain(DatabaseTreeNode* node) const {
    if (!node) {
        return NodeChain();
    }

    QList<Node> nodes;
    DatabaseTreeNode* current = node;

    while (current && current != m_rootNode.get()) {
        Node n;
        n.id = current->id;
        n.name = current->name;
        n.type = current->type;
        n.database = current->database;
        n.schema = current->schema;
        n.metadata = current->metadata;
        nodes.prepend(n);
        current = current->parent;
    }

    return NodeChain(nodes);
}

void DatabaseTreeModel::onNodeChildrenLoaded(DatabaseTreeNode* node, const QList<Node>& children) {
    if (!node) {
        return;
    }

    node->isLoading = false;
    processLoadedChildren(node, children);

    QModelIndex nodeIndex = getNodeIndex(node);
    emit nodeLoadFinished(nodeIndex);
    emit dataChanged(nodeIndex, nodeIndex);
}

void DatabaseTreeModel::onNodeLoadFailed(DatabaseTreeNode* node, const QString& error) {
    if (!node) {
        return;
    }

    node->isLoading = false;
    QModelIndex nodeIndex = getNodeIndex(node);
    emit nodeLoadFailed(nodeIndex, error);
    emit dataChanged(nodeIndex, nodeIndex);
}

DatabaseTreeNode* DatabaseTreeModel::createNode(const Node& nodeData, DatabaseTreeNode* parent) {
    auto node = new DatabaseTreeNode();
    node->parent = parent; // 先设 parent，updateNodeData 中会从 parent 继承 connectionId
    updateNodeData(node, nodeData);
    node->canExpand = nodeCanExpand(node->type);

    if (parent) {
        parent->children.append(node);
        // qDebug() << "createNode: added node" << node->name << "to parent, parent now has" << parent->children.size() << "children";
    }

    qDebug() << "createNode: created node" << node->name << "type:" << (int)node->type
             << "displayName:" << node->displayName;
    return node;
}

void DatabaseTreeModel::updateNodeData(DatabaseTreeNode* node, const Node& nodeData) {
    node->id = nodeData.id;
    node->name = nodeData.name;
    node->type = nodeData.type;
    node->database = nodeData.database;
    node->schema = nodeData.schema;
    node->metadata = nodeData.metadata;

    // 生成带emoji的显示名称
    QString emoji = getNodeEmoji(node->type);
    node->displayName = emoji.isEmpty() ? node->name : (emoji + " " + node->name);

    // 设置连接ID
    if (node->type == NodeType::Connection) {
        node->connectionId = node->id;
    } else if (node->parent) {
        node->connectionId = node->parent->connectionId;
    }
}

bool DatabaseTreeModel::nodeCanExpand(NodeType type) const {
    switch (type) {
        case NodeType::Connection:
        case NodeType::Database:
        case NodeType::RedisDatabase:
        case NodeType::Schema:
        case NodeType::TableFolder:
        case NodeType::ViewFolder:
        case NodeType::ProcedureFolder:
        case NodeType::FunctionFolder:
        case NodeType::TriggerFolder:
        case NodeType::EventFolder:
        case NodeType::IndexFolder:
        case NodeType::SequenceFolder:
        case NodeType::UserFolder:
        case NodeType::Table:
        case NodeType::View:
            return true;
        default:
            return false;
    }
}

QString DatabaseTreeModel::getNodeIcon(NodeType type) {
    // 返回图标路径，可以根据需要调整
    switch (type) {
        case NodeType::Connection:
            return ":/icons/connection.png";
        case NodeType::Database:
        case NodeType::RedisDatabase:
            return ":/icons/database.png";
        case NodeType::Schema:
            return ":/icons/schema.png";
        case NodeType::Table:
            return ":/icons/table.png";
        case NodeType::View:
            return ":/icons/view.png";
        case NodeType::Procedure:
        case NodeType::Function:
            return ":/icons/function.png";
        case NodeType::Trigger:
            return ":/icons/trigger.png";
        case NodeType::Index:
            return ":/icons/index.png";
        case NodeType::Column:
            return ":/icons/column.png";
        case NodeType::Key:
        case NodeType::RedisKey:
            return ":/icons/key.png";
        default:
            return ":/icons/default.png";
    }
}

QString DatabaseTreeModel::getNodeEmoji(NodeType type) const {
    switch (type) {
        case NodeType::Connection:
            return "🔗";
        case NodeType::Database:
        case NodeType::RedisDatabase:
            return "🗃️";
        case NodeType::Schema:
        case NodeType::TableFolder:
        case NodeType::ViewFolder:
        case NodeType::ProcedureFolder:
        case NodeType::FunctionFolder:
        case NodeType::TriggerFolder:
        case NodeType::EventFolder:
        case NodeType::IndexFolder:
        case NodeType::SequenceFolder:
        case NodeType::UserFolder:
            return "📁";
        case NodeType::Table:
            return "📋";
        case NodeType::View:
            return "👁";
        case NodeType::Procedure:
        case NodeType::Function:
            return "⚡";
        case NodeType::Trigger:
            return "🔧";
        case NodeType::Index:
            return "🗂";
        case NodeType::Sequence:
            return "🔢";
        case NodeType::User:
            return "👤";
        case NodeType::Column:
            return "🏷";
        case NodeType::Key:
        case NodeType::RedisKey:
            return "🔑";
        default:
            return "";
    }
}

void DatabaseTreeModel::loadRootNodes() {
    // 根节点已在构造函数中创建，这里无需特殊操作
}

void DatabaseTreeModel::loadNodeChildren(DatabaseTreeNode* node) {
    if (!node || node->isLoading) {
        return;
    }

    // 构建节点链
    NodeChain nodeChain = buildNodeChain(node);

    // 检查缓存
    QString cacheKey = getCacheKey(node->connectionId, nodeChain);
    QList<Node> cachedChildren = getCachedNodeChildren(cacheKey);
    if (!cachedChildren.isEmpty()) {
        processLoadedChildren(node, cachedChildren);
        return;
    }

    // 异步加载
    m_loader->loadNodeChildren(node->connectionId, nodeChain, node);
}

void DatabaseTreeModel::processLoadedChildren(DatabaseTreeNode* parent, const QList<Node>& children) {
    if (children.isEmpty()) {
        parent->isLoaded = true;
        parent->hasChildrenCached = true;
        return;
    }

    beginInsertNodes(parent, 0, children.size() - 1);

    for (const Node& childData : children) {
        createNode(childData, parent);
    }

    parent->isLoaded = true;
    parent->hasChildrenCached = true;

    endInsertNodes();

    // 缓存数据
    NodeChain parentChain = buildNodeChain(parent);
    QString cacheKey = getCacheKey(parent->connectionId, parentChain);
    cacheNodeChildren(cacheKey, children);
}

QString DatabaseTreeModel::getCacheKey(const QString& connectionId, const NodeChain& nodeChain) const {
    return connectionId + "_" + nodeChain.toString();
}

void DatabaseTreeModel::cacheNodeChildren(const QString& key, const QList<Node>& children) {
    m_nodeCache.insert(key, new QList<Node>(children));
}

QList<Node> DatabaseTreeModel::getCachedNodeChildren(const QString& key) const {
    QList<Node>* cached = m_nodeCache.object(key);
    return cached ? *cached : QList<Node>();
}

void DatabaseTreeModel::clearNodeCache() {
    m_nodeCache.clear();
}

void DatabaseTreeModel::beginInsertNodes(DatabaseTreeNode* parent, const int first, const int last) {
    const QModelIndex parentIndex = getNodeIndex(parent);
    beginInsertRows(parentIndex, first, last);
}

void DatabaseTreeModel::endInsertNodes() {
    endInsertRows();
}

void DatabaseTreeModel::beginRemoveNodes(DatabaseTreeNode* parent, int first, int last) {
    QModelIndex parentIndex = getNodeIndex(parent);
    beginRemoveRows(parentIndex, first, last);
}

void DatabaseTreeModel::endRemoveNodes() {
    endRemoveRows();
}

QModelIndexList DatabaseTreeModel::findNodes(const QString& name, NodeType type) const {
    // 搜索功能实现
    QModelIndexList results;
    // TODO: 实现递归搜索
    return results;
}

void DatabaseTreeModel::setFilter(const QString& filter) {
    m_filterText = filter;
    m_filterEnabled = !filter.isEmpty();
    // TODO: 实现过滤逻辑
}

void DatabaseTreeModel::clearFilter() {
    m_filterText.clear();
    m_filterEnabled = false;
    // TODO: 清除过滤
}

