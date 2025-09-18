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
}

void DatabaseTreeLoader::processLoadRequest() {
    if (m_loading || m_loadQueue.isEmpty()) {
        return;
    }

    QMutexLocker locker(&m_queueMutex);
    LoadRequest request = m_loadQueue.dequeue();
    locker.unlock();

    m_loading = true;

    // 在线程池中异步加载数据
    QFuture<void> future = QtConcurrent::run([this, request]() {
        try {
            Connx::Connection* connection = m_connections.value(request.connectionId);
            if (!connection || !connection->isConnected()) {
                emit nodeLoadFailed(request.node, "Connection not available");
                return;
            }

            // 创建展开事件
            auto expandEvent = NodeEventFactory::createExpandEvent(connection, request.nodeChain);
            if (!expandEvent) {
                emit nodeLoadFailed(request.node, "Failed to create expand event");
                return;
            }

            // 执行展开操作
            ExpandResult result = expandEvent->execute();
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
    });

    m_watcher->setFuture(future);
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
    , m_nodeCache(1000) // 缓存1000个节点的数据
    , m_lazyLoadingEnabled(true)
    , m_loadBatchSize(100)
    , m_filterEnabled(false) {

    setupConnections();
    loadRootNodes();
}

DatabaseTreeModel::~DatabaseTreeModel() {
    clearNodeCache();
}

void DatabaseTreeModel::setupConnections() {
    connect(m_loader, &DatabaseTreeLoader::nodeChildrenLoaded,
            this, &DatabaseTreeModel::onNodeChildrenLoaded);
    connect(m_loader, &DatabaseTreeLoader::nodeLoadFailed,
            this, &DatabaseTreeModel::onNodeLoadFailed);
}

QModelIndex DatabaseTreeModel::index(int row, int column, const QModelIndex& parent) const {
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    DatabaseTreeNode* parentNode = getNode(parent);
    if (!parentNode || row >= parentNode->children.size()) {
        return QModelIndex();
    }

    DatabaseTreeNode* childNode = parentNode->children.at(row);
    return createIndex(row, column, childNode);
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
    return parentNode ? parentNode->children.size() : 0;
}

int DatabaseTreeModel::columnCount(const QModelIndex& parent) const {
    Q_UNUSED(parent)
    return 1; // 单列显示
}

QVariant DatabaseTreeModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) {
        return QVariant();
    }

    DatabaseTreeNode* node = getNode(index);
    if (!node) {
        return QVariant();
    }

    switch (role) {
        case Qt::DisplayRole:
            return node->displayName.isEmpty() ? node->name : node->displayName;

        case Qt::ToolTipRole: {
            QString tooltip = node->name;
            if (!node->database.isEmpty()) {
                tooltip += QString("\n数据库: %1").arg(node->database);
            }
            if (!node->schema.isEmpty()) {
                tooltip += QString("\n模式: %1").arg(node->schema);
            }
            if (node->isLoading) {
                tooltip += "\n正在加载...";
            }
            return tooltip;
        }

        case Qt::DecorationRole:
            return QIcon(getNodeIcon(node->type));

        case Qt::FontRole: {
            QFont font;
            if (node->isLoading) {
                font.setItalic(true);
            }
            if (node->type == NodeType::Connection) {
                font.setBold(true);
            }
            return font;
        }

        case Qt::ForegroundRole: {
            if (node->isLoading) {
                return QApplication::palette().color(QPalette::Disabled, QPalette::Text);
            }
            break;
        }

        case Qt::UserRole:
            return static_cast<int>(node->type);

        case Qt::UserRole + 1:
            return node->id;

        case Qt::UserRole + 2:
            return node->database;

        case Qt::UserRole + 3:
            return node->connectionId;

        default:
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

QVariant DatabaseTreeModel::headerData(int section, Qt::Orientation orientation, int role) const {
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
    if (!m_lazyLoadingEnabled) {
        return false;
    }

    DatabaseTreeNode* node = getNode(parent);
    if (!node) {
        return false;
    }

    // 可以获取更多数据的条件：
    // 1. 节点类型支持展开
    // 2. 尚未加载
    // 3. 不在加载中
    return nodeCanExpand(node->type) && !node->isLoaded && !node->isLoading;
}

void DatabaseTreeModel::fetchMore(const QModelIndex& parent) {
    DatabaseTreeNode* node = getNode(parent);
    if (!node || node->isLoading || node->isLoaded) {
        return;
    }

    emit nodeLoadStarted(parent);
    loadNodeChildren(node);
}

void DatabaseTreeModel::addConnection(const QString& name, Connx::Connection* connection) {
    m_connections[name] = connection;
    m_loader->setConnection(name, connection);

    // 创建连接节点
    Node connectionNode;
    connectionNode.id = name;
    connectionNode.name = name;
    connectionNode.type = NodeType::Connection;

    beginInsertNodes(m_rootNode.get(), m_rootNode->children.size(), m_rootNode->children.size());
    DatabaseTreeNode* connNode = createNode(connectionNode, m_rootNode.get());
    m_connectionNodes[name] = connNode;
    endInsertNodes();
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
    updateNodeData(node, nodeData);
    node->parent = parent;
    node->canExpand = nodeCanExpand(node->type);

    if (parent) {
        parent->children.append(node);
    }

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
        case NodeType::IndexFolder:
        case NodeType::SequenceFolder:
        case NodeType::UserFolder:
            return true;
        default:
            return false;
    }
}

QString DatabaseTreeModel::getNodeIcon(NodeType type) const {
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

void DatabaseTreeModel::beginInsertNodes(DatabaseTreeNode* parent, int first, int last) {
    QModelIndex parentIndex = getNodeIndex(parent);
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

// DatabaseTreeView 实现
DatabaseTreeView::DatabaseTreeView(QWidget* parent)
    : QTreeView(parent)
    , m_model(nullptr) {

    setupPerformanceOptimizations();
}

void DatabaseTreeView::setModel(DatabaseTreeModel* model) {
    m_model = model;
    QTreeView::setModel(model);

    if (model) {
        connect(model, &DatabaseTreeModel::nodeLoadStarted,
                this, &DatabaseTreeView::onLoadStarted);
        connect(model, &DatabaseTreeModel::nodeLoadFinished,
                this, &DatabaseTreeView::onLoadFinished);
        connect(this, &QTreeView::expanded, this, &DatabaseTreeView::onExpanded);
        connect(this, &QTreeView::collapsed, this, &DatabaseTreeView::onCollapsed);
    }
}

DatabaseTreeModel* DatabaseTreeView::databaseModel() const {
    return m_model;
}

void DatabaseTreeView::setupPerformanceOptimizations() {
    // 启用性能优化选项
    setUniformRowHeights(true);               // 统一行高，提升性能
    setAlternatingRowColors(false);           // 禁用交替行颜色
    setRootIsDecorated(true);                 // 显示根节点装饰
    setItemsExpandable(true);                 // 允许展开
    setExpandsOnDoubleClick(false);           // 禁用双击展开，使用自定义逻辑
    setHeaderHidden(true);                    // 隐藏表头

    // 选择模式
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);

    // 设置样式
    setStyleSheet(R"(
        QTreeView {
            border: 1px solid #e1e1e1;
            border-radius: 6px;
            background-color: #ffffff;
            selection-background-color: #3498db;
            show-decoration-selected: 1;
        }
        QTreeView::item {
            height: 28px;
            padding: 2px 8px;
            border: none;
        }
        QTreeView::item:hover {
            background-color: #f0f8ff;
        }
        QTreeView::item:selected {
            background-color: #3498db;
            color: white;
        }
        QTreeView::branch:has-children:!has-siblings:closed,
        QTreeView::branch:closed:has-children:has-siblings {
            border-image: none;
            image: url(:/icons/branch-closed.png);
        }
        QTreeView::branch:open:has-children:!has-siblings,
        QTreeView::branch:open:has-children:has-siblings {
            border-image: none;
            image: url(:/icons/branch-open.png);
        }
    )");
}

void DatabaseTreeView::setVirtualizationEnabled(bool enabled) {
    // Qt的QTreeView默认就是虚拟化的
    Q_UNUSED(enabled)
}

void DatabaseTreeView::setUniformRowHeights(bool uniform) {
    QTreeView::setUniformRowHeights(uniform);
}

void DatabaseTreeView::setItemsExpandable(bool expandable) {
    QTreeView::setItemsExpandable(expandable);
}

void DatabaseTreeView::mouseDoubleClickEvent(QMouseEvent* event) {
    QModelIndex index = indexAt(event->pos());
    if (index.isValid()) {
        emit nodeDoubleClicked(index);
    }
    // 不调用基类的实现，避免默认的展开行为
}

void DatabaseTreeView::contextMenuEvent(QContextMenuEvent* event) {
    QModelIndex index = indexAt(event->pos());
    emit nodeContextMenuRequested(index, event->globalPos());
}

void DatabaseTreeView::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        QModelIndex current = currentIndex();
        if (current.isValid()) {
            emit nodeDoubleClicked(current);
        }
        return;
    }
    QTreeView::keyPressEvent(event);
}

void DatabaseTreeView::onExpanded(const QModelIndex& index) {
    if (m_model) {
        // 当节点展开时，确保数据已加载
        if (m_model->canFetchMore(index)) {
            m_model->fetchMore(index);
        }
    }
}

void DatabaseTreeView::onCollapsed(const QModelIndex& index) {
    // 节点折叠时的处理逻辑
    Q_UNUSED(index)
}

void DatabaseTreeView::onLoadStarted(const QModelIndex& index) {
    updateLoadingState(index, true);
}

void DatabaseTreeView::onLoadFinished(const QModelIndex& index) {
    updateLoadingState(index, false);
}

void DatabaseTreeView::updateLoadingState(const QModelIndex& index, bool loading) {
    m_loadingStates[index] = loading;
    update(index); // 触发重绘
}

void DatabaseTreeView::searchNodes(const QString& text) {
    if (m_model) {
        m_model->setFilter(text);
    }
}

void DatabaseTreeView::clearSearch() {
    if (m_model) {
        m_model->clearFilter();
    }
}

void DatabaseTreeView::expandToNode(const QModelIndex& index) {
    if (!index.isValid()) {
        return;
    }

    // 展开到指定节点的路径
    QModelIndex current = index.parent();
    while (current.isValid()) {
        expand(current);
        current = current.parent();
    }

    // 滚动到目标节点
    scrollTo(index, QAbstractItemView::EnsureVisible);
    setCurrentIndex(index);
}

// MOC include removed - not needed since classes are in header