#ifndef DATABASETREEMODEL_H
#define DATABASETREEMODEL_H

#include <QAbstractItemModel>
#include <QTreeView>
#include <QHash>
#include <QCache>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QFuture>
#include <QFutureWatcher>
#include <QQueue>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <memory>

#include "../../common/connx/Connection.h"
#include "abstractnodeevent.h"

// 高性能树节点数据结构
struct DatabaseTreeNode {
    QString id;                    // 唯一标识
    QString name;                  // 显示名称（不带emoji）
    QString displayName;           // 显示名称（带emoji）
    NodeType type;                 // 节点类型
    QString database;              // 所属数据库
    QString schema;                // 所属模式
    QVariantMap metadata;          // 额外数据

    // 层级关系
    DatabaseTreeNode* parent;
    QList<DatabaseTreeNode*> children;

    // 状态信息
    bool isLoaded;                 // 是否已加载子节点
    bool isLoading;                // 是否正在加载
    bool hasChildrenCached;        // 是否已缓存子节点信息
    bool canExpand;                // 是否可展开（根据类型判断）

    // 连接信息
    QString connectionId;

    DatabaseTreeNode()
        : parent(nullptr)
        , isLoaded(false)
        , isLoading(false)
        , hasChildrenCached(false)
        , canExpand(false) {
    }

    ~DatabaseTreeNode() {
        qDeleteAll(children);
    }

    // 禁用拷贝
    DatabaseTreeNode(const DatabaseTreeNode&) = delete;
    DatabaseTreeNode& operator=(const DatabaseTreeNode&) = delete;

    // 移动构造
    DatabaseTreeNode(DatabaseTreeNode&& other) noexcept
        : id(std::move(other.id))
        , name(std::move(other.name))
        , displayName(std::move(other.displayName))
        , type(other.type)
        , database(std::move(other.database))
        , schema(std::move(other.schema))
        , metadata(std::move(other.metadata))
        , parent(other.parent)
        , children(std::move(other.children))
        , isLoaded(other.isLoaded)
        , isLoading(other.isLoading)
        , hasChildrenCached(other.hasChildrenCached)
        , canExpand(other.canExpand)
        , connectionId(std::move(other.connectionId)) {
        other.parent = nullptr;
    }

    // 计算层级深度
    int depth() const {
        int d = 0;
        DatabaseTreeNode* p = parent;
        while (p) {
            ++d;
            p = p->parent;
        }
        return d;
    }

    // 获取根节点
    DatabaseTreeNode* root() {
        DatabaseTreeNode* r = this;
        while (r->parent) {
            r = r->parent;
        }
        return r;
    }

    // 查找子节点
    DatabaseTreeNode* findChild(const QString& nodeId) const {
        for (DatabaseTreeNode* child : children) {
            if (child->id == nodeId) {
                return child;
            }
        }
        return nullptr;
    }

    // 获取行号
    int row() const {
        if (parent) {
            return parent->children.indexOf(const_cast<DatabaseTreeNode*>(this));
        }
        return 0;
    }
};

// 异步数据加载器
class DatabaseTreeLoader : public QObject {
    Q_OBJECT

public:
    struct LoadRequest {
        QString connectionId;
        NodeChain nodeChain;
        DatabaseTreeNode* node;

        LoadRequest(const QString& connId, const NodeChain& chain, DatabaseTreeNode* n)
            : connectionId(connId), nodeChain(chain), node(n) {}
    };

    explicit DatabaseTreeLoader(QObject* parent = nullptr);

    void loadNodeChildren(const QString& connectionId, const NodeChain& nodeChain, DatabaseTreeNode* node);
    void setConnection(const QString& connectionId, Connx::Connection* connection);

signals:
    void nodeChildrenLoaded(DatabaseTreeNode* node, const QList<Node>& children);
    void nodeLoadFailed(DatabaseTreeNode* node, const QString& error);

private slots:
    void processLoadRequest();
    void onLoadFinished();

private:
    QTimer* m_loadTimer;
    QQueue<LoadRequest> m_loadQueue;
    QHash<QString, Connx::Connection*> m_connections;
    QFutureWatcher<void>* m_watcher;
    QMutex m_queueMutex;
    bool m_loading;
};

// 高性能数据库树模型
class DatabaseTreeModel : public QAbstractItemModel {
    Q_OBJECT

public:
    explicit DatabaseTreeModel(QObject* parent = nullptr);
    ~DatabaseTreeModel() override;

    // QAbstractItemModel 接口
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;
    bool canFetchMore(const QModelIndex& parent) const override;
    void fetchMore(const QModelIndex& parent) override;

    // 数据库连接管理
    void addConnection(const QString& name, Connx::Connection* connection);
    void removeConnection(const QString& name);
    void refreshConnection(const QString& name);
    void refreshNode(const QModelIndex& index);

    // 节点操作
    DatabaseTreeNode* getNode(const QModelIndex& index) const;
    QModelIndex getNodeIndex(DatabaseTreeNode* node) const;
    NodeChain buildNodeChain(DatabaseTreeNode* node) const;

    // 性能优化选项
    void setLazyLoadingEnabled(bool enabled) { m_lazyLoadingEnabled = enabled; }
    void setCacheSize(int size) { m_nodeCache.setMaxCost(size); }
    void setLoadBatchSize(int size) { m_loadBatchSize = size; }

    // 搜索和过滤
    QModelIndexList findNodes(const QString& name, NodeType type = NodeType::Unknown) const;
    void setFilter(const QString& filter);
    void clearFilter();

signals:
    void nodeLoadStarted(const QModelIndex& index);
    void nodeLoadFinished(const QModelIndex& index);
    void nodeLoadFailed(const QModelIndex& index, const QString& error);

private slots:
    void onNodeChildrenLoaded(DatabaseTreeNode* node, const QList<Node>& children);
    void onNodeLoadFailed(DatabaseTreeNode* node, const QString& error);

private:
    // 初始化
    void setupConnections();

    // 节点管理
    DatabaseTreeNode* createNode(const Node& nodeData, DatabaseTreeNode* parent = nullptr);
    void updateNodeData(DatabaseTreeNode* node, const Node& nodeData);
    bool nodeCanExpand(NodeType type) const;
    QString getNodeIcon(NodeType type) const;
    QString getNodeEmoji(NodeType type) const;

    // 数据加载
    void loadRootNodes();
    void loadNodeChildren(DatabaseTreeNode* node);
    void processLoadedChildren(DatabaseTreeNode* parent, const QList<Node>& children);

    // 缓存管理
    QString getCacheKey(const QString& connectionId, const NodeChain& nodeChain) const;
    void cacheNodeChildren(const QString& key, const QList<Node>& children);
    QList<Node> getCachedNodeChildren(const QString& key) const;
    void clearNodeCache();

    // 工具方法
    void beginInsertNodes(DatabaseTreeNode* parent, int first, int last);
    void endInsertNodes();
    void beginRemoveNodes(DatabaseTreeNode* parent, int first, int last);
    void endRemoveNodes();

    // 数据成员
    std::unique_ptr<DatabaseTreeNode> m_rootNode;
    QHash<QString, Connx::Connection*> m_connections;
    QHash<QString, DatabaseTreeNode*> m_connectionNodes;

    // 性能优化
    DatabaseTreeLoader* m_loader;
    QCache<QString, QList<Node>> m_nodeCache;
    bool m_lazyLoadingEnabled;
    int m_loadBatchSize;

    // 过滤
    QString m_filterText;
    bool m_filterEnabled;

    // 线程安全
    mutable QMutex m_modelMutex;
};

// 高性能数据库树视图
class DatabaseTreeView : public QTreeView {
    Q_OBJECT

public:
    explicit DatabaseTreeView(QWidget* parent = nullptr);

    void setModel(DatabaseTreeModel* model);
    DatabaseTreeModel* databaseModel() const;

    // 性能优化设置
    void setVirtualizationEnabled(bool enabled);
    void setUniformRowHeights(bool uniform);
    void setItemsExpandable(bool expandable);

    // 搜索功能
    void searchNodes(const QString& text);
    void clearSearch();
    void expandToNode(const QModelIndex& index);

signals:
    void nodeDoubleClicked(const QModelIndex& index);
    void nodeContextMenuRequested(const QModelIndex& index, const QPoint& pos);

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onExpanded(const QModelIndex& index);
    void onCollapsed(const QModelIndex& index);
    void onLoadStarted(const QModelIndex& index);
    void onLoadFinished(const QModelIndex& index);

private:
    void setupPerformanceOptimizations();
    void updateLoadingState(const QModelIndex& index, bool loading);

    DatabaseTreeModel* m_model;
    QHash<QModelIndex, bool> m_loadingStates;
};

#endif // DATABASETREEMODEL_H