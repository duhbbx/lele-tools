#ifndef ABSTRACTNODEEVENT_H
#define ABSTRACTNODEEVENT_H

#include <QString>
#include <QList>
#include <QVariant>
#include <QVariantMap>
#include <memory>
#include "../../common/connx/Connection.h"

// 节点类型枚举
enum class NodeType {
    // 连接层级
    Connection,

    // 数据库层级
    Database,
    Schema,

    // 容器层级
    TableFolder,
    ViewFolder,
    ProcedureFolder,
    FunctionFolder,
    TriggerFolder,
    EventFolder,
    IndexFolder,
    SequenceFolder,
    UserFolder,

    // 对象层级
    Table,
    View,
    Procedure,
    Function,
    Trigger,
    Event,
    Index,
    Sequence,
    User,
    Column,
    Key,

    // Redis特有
    RedisDatabase,
    RedisKey,
    RedisKeyFolder,

    // 其他
    Unknown
};

// 事件类型枚举
enum class EventType {
    Expand,           // 展开节点
    Collapse,         // 折叠节点
    Delete,           // 删除对象
    Create,           // 创建对象
    Modify,           // 修改对象
    GenerateSQL,      // 生成SQL
    ExecuteQuery,     // 执行查询
    Refresh,          // 刷新节点
    Properties,       // 获取属性
    Export,           // 导出数据
    Import,           // 导入数据
    Backup,           // 备份
    Restore           // 恢复
};

// 节点信息结构
struct Node {
    QString id;                    // 节点唯一标识
    QString qtElementId;           // Qt元素ID，用于UI定位
    NodeType type;                 // 节点类型
    QString name;                  // 节点显示名称
    QString fullName;              // 节点完整名称
    QString schema;                // 所属模式（如果有）
    QString database;              // 所属数据库
    QVariantMap metadata;          // 额外元数据

    Node() : type(NodeType::Unknown) {}

    Node(const QString& nodeId, NodeType nodeType, const QString& nodeName)
        : id(nodeId), type(nodeType), name(nodeName) {}

    Node(const QString& nodeId, const QString& qtId, NodeType nodeType,
         const QString& nodeName, const QString& nodeFullName = QString())
        : id(nodeId), qtElementId(qtId), type(nodeType), name(nodeName), fullName(nodeFullName) {}
};

// 节点链路类 - 从连接到目标节点的完整路径
class NodeChain {
public:
    NodeChain() = default;

    NodeChain(const QList<Node>& nodes) : m_nodes(nodes) {}

    // 添加节点到链路末尾
    void append(const Node& node) {
        m_nodes.append(node);
    }

    // 在指定位置插入节点
    void insert(int index, const Node& node) {
        m_nodes.insert(index, node);
    }

    // 获取链路中的所有节点
    const QList<Node>& nodes() const { return m_nodes; }

    // 获取根节点（连接节点）
    Node rootNode() const {
        return m_nodes.isEmpty() ? Node() : m_nodes.first();
    }

    // 获取目标节点（最后一个节点）
    Node targetNode() const {
        return m_nodes.isEmpty() ? Node() : m_nodes.last();
    }

    // 获取指定类型的第一个节点
    Node nodeByType(NodeType type) const {
        for (const Node& node : m_nodes) {
            if (node.type == type) {
                return node;
            }
        }
        return Node();
    }

    // 获取父节点（倒数第二个节点）
    Node parentNode() const {
        if (m_nodes.size() < 2) {
            return Node();
        }
        int index = m_nodes.size() - 2;
        return (index >= 0 && index < m_nodes.size()) ? m_nodes[index] : Node();
    }

    // 获取连接ID
    QString connectionId() const {
        Node root = rootNode();
        return root.type == NodeType::Connection ? root.id : QString();
    }

    // 获取数据库名
    QString databaseName() const {
        Node dbNode = nodeByType(NodeType::Database);
        if (dbNode.type == NodeType::Unknown) {
            dbNode = nodeByType(NodeType::RedisDatabase);
        }
        return dbNode.name;
    }

    // 获取模式名
    QString schemaName() const {
        Node schemaNode = nodeByType(NodeType::Schema);
        return schemaNode.name;
    }

    // 检查链路是否有效
    bool isValid() const {
        return !m_nodes.isEmpty() && rootNode().type == NodeType::Connection;
    }

    // 获取链路深度
    int depth() const { return m_nodes.size(); }

    // 清空链路
    void clear() { m_nodes.clear(); }

    // 克隆链路
    NodeChain clone() const {
        return NodeChain(m_nodes);
    }

    // 生成链路描述字符串
    QString toString() const {
        QStringList parts;
        for (const Node& node : m_nodes) {
            parts << QString("%1:%2").arg(static_cast<int>(node.type)).arg(node.name);
        }
        return parts.join(" -> ");
    }

private:
    QList<Node> m_nodes;
};

// 抽象节点事件基类
template<typename T>
class AbstractNodeEvent {
public:
    // 构造函数
    AbstractNodeEvent(const QString& connectionId,
                      EventType eventType,
                      const NodeChain& nodeChain)
        : m_connectionId(connectionId)
        , m_eventType(eventType)
        , m_nodeChain(nodeChain)
        , m_connection(nullptr)
        , m_ownsConnection(false) {

        initializeConnection();
    }

    // 带现有连接的构造函数
    AbstractNodeEvent(Connx::Connection* connection,
                      EventType eventType,
                      const NodeChain& nodeChain)
        : m_connectionId(connection ? connection->getConfig().name : QString())
        , m_eventType(eventType)
        , m_nodeChain(nodeChain)
        , m_connection(connection)
        , m_ownsConnection(false) {}

    virtual ~AbstractNodeEvent() {
        if (m_ownsConnection && m_connection) {
            m_connection->disconnectFromServer();
            delete m_connection;
        }
    }

    // 执行事件 - 纯虚函数，子类必须实现
    virtual T execute() = 0;

    // 验证事件是否可以执行
    virtual bool canExecute() const {
        return m_connection && m_connection->isConnected() && m_nodeChain.isValid();
    }

    // 获取错误信息
    virtual QString getLastError() const { return m_lastError; }

    // 获取事件类型
    EventType eventType() const { return m_eventType; }

    // 获取节点链路
    const NodeChain& nodeChain() const { return m_nodeChain; }

    // 获取连接
    Connx::Connection* connection() const { return m_connection; }

    // 获取连接ID
    QString connectionId() const { return m_connectionId; }

    // 获取目标节点
    Node targetNode() const { return m_nodeChain.targetNode(); }

    // 获取目标节点类型
    NodeType targetNodeType() const { return targetNode().type; }

protected:
    // 设置错误信息
    void setError(const QString& error) { m_lastError = error; }

    // 获取数据库类型
    Connx::ConnectionType getDatabaseType() const {
        return m_connection ? m_connection->getType() : Connx::ConnectionType::Unknown;
    }

    // 执行SQL查询（用于关系数据库）
    Connx::QueryResult executeQuery(const QString& sql) {
        if (!m_connection || !m_connection->isConnected()) {
            Connx::QueryResult result;
            result.success = false;
            result.errorMessage = "数据库连接无效";
            return result;
        }
        return m_connection->query(sql);
    }

    // 执行Redis命令（用于Redis）
    QVariant executeRedisCommand(const QString& command) {
        if (!m_connection || !m_connection->isConnected()) {
            return QVariant();
        }

        Connx::QueryResult result = m_connection->execute(command);
        return result.success ? result.data : QVariant();
    }

private:
    // 初始化数据库连接
    void initializeConnection() {
        if (!m_connectionId.isEmpty()) {
            // 这里应该从连接管理器获取现有连接，或创建新连接
            // 暂时留空，具体实现需要根据你的连接管理器来
            // m_connection = ConnectionManager::instance()->getConnection(m_connectionId);
            // if (!m_connection) {
            //     // 创建新连接
            //     m_connection = ConnectionManager::instance()->createConnection(m_connectionId);
            //     m_ownsConnection = true;
            // }
        }
    }

protected:
    QString m_connectionId;
    EventType m_eventType;
    NodeChain m_nodeChain;
    Connx::Connection* m_connection;
    bool m_ownsConnection;
    QString m_lastError;
};

// 展开事件结果结构
struct ExpandResult {
    bool success;
    QString errorMessage;
    QList<Node> childNodes;

    ExpandResult() : success(false) {}
    ExpandResult(bool ok, const QList<Node>& nodes = QList<Node>())
        : success(ok), childNodes(nodes) {}
    ExpandResult(const QString& error)
        : success(false), errorMessage(error) {}
};

// 展开事件抽象基类
class AbstractExpandEvent : public AbstractNodeEvent<ExpandResult> {
public:
    AbstractExpandEvent(const QString& connectionId, const NodeChain& nodeChain)
        : AbstractNodeEvent<ExpandResult>(connectionId, EventType::Expand, nodeChain) {}

    AbstractExpandEvent(Connx::Connection* connection, const NodeChain& nodeChain)
        : AbstractNodeEvent<ExpandResult>(connection, EventType::Expand, nodeChain) {}

    // 执行展开操作
    ExpandResult execute() override {
        if (!canExecute()) {
            return ExpandResult("无法执行展开操作：" + getLastError());
        }

        NodeType nodeType = targetNodeType();
        return expandNode(nodeType);
    }

protected:
    // 根据节点类型展开 - 子类实现具体逻辑
    virtual ExpandResult expandNode(NodeType nodeType) = 0;
};

#endif // ABSTRACTNODEEVENT_H