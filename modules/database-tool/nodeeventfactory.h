#ifndef NODEEVENTFACTORY_H
#define NODEEVENTFACTORY_H

#include "abstractnodeevent.h"
#include "mysqlexpandevent.h"
#include "redisexpandevent.h"
#include <QTreeWidgetItem>
#include <memory>

// 节点事件工厂类
class NodeEventFactory {
public:
    // 创建展开事件
    static std::unique_ptr<AbstractExpandEvent> createExpandEvent(
        Connx::Connection* connection,
        const NodeChain& nodeChain) {

        if (!connection) {
            return nullptr;
        }

        Connx::ConnectionType dbType = connection->getType();

        switch (dbType) {
            case Connx::ConnectionType::MySQL:
                return std::make_unique<MySqlExpandEvent>(connection, nodeChain);

            case Connx::ConnectionType::Redis:
                return std::make_unique<RedisExpandEvent>(connection, nodeChain);

            case Connx::ConnectionType::PostgreSQL:
                // PostgreSQL实现留待扩展
                return nullptr;

            case Connx::ConnectionType::SQLite:
                // SQLite实现留待扩展
                return nullptr;

            default:
                return nullptr;
        }
    }

    // 通过连接ID创建展开事件
    static std::unique_ptr<AbstractExpandEvent> createExpandEvent(
        const QString& connectionId,
        const NodeChain& nodeChain,
        Connx::ConnectionType dbType) {

        switch (dbType) {
            case Connx::ConnectionType::MySQL:
                return std::make_unique<MySqlExpandEvent>(connectionId, nodeChain);

            case Connx::ConnectionType::Redis:
                return std::make_unique<RedisExpandEvent>(connectionId, nodeChain);

            case Connx::ConnectionType::PostgreSQL:
                // PostgreSQL实现留待扩展
                return nullptr;

            case Connx::ConnectionType::SQLite:
                // SQLite实现留待扩展
                return nullptr;

            default:
                return nullptr;
        }
    }

    // 辅助方法：从QTreeWidgetItem构建NodeChain
    static NodeChain buildNodeChainFromItem(QTreeWidgetItem* item) {
        if (!item) {
            return NodeChain();
        }

        QList<Node> nodes;
        QTreeWidgetItem* current = item;

        // 从当前项向上遍历到根节点
        while (current) {
            // 从Qt项目数据中构建Node
            Node node = createNodeFromItem(current);
            nodes.prepend(node); // 插入到前面，保持从根到叶的顺序

            current = current->parent();
        }

        return NodeChain(nodes);
    }

    // 辅助方法：从Qt项目创建Node
    static Node createNodeFromItem(QTreeWidgetItem* item) {
        if (!item) {
            return Node();
        }

        // 假设QTreeWidgetItem的data中存储了节点信息
        QString id = item->data(0, Qt::UserRole).toString();
        QString name = item->text(0);

        // 根据存储的类型信息确定NodeType
        int typeInt = item->data(0, Qt::UserRole + 1).toInt();
        NodeType type = static_cast<NodeType>(typeInt);

        Node node(id, name, type, name);

        // 获取额外的元数据
        QString database = item->data(0, Qt::UserRole + 2).toString();
        QString schema = item->data(0, Qt::UserRole + 3).toString();
        QString fullName = item->data(0, Qt::UserRole + 4).toString();

        node.database = database;
        node.schema = schema;
        node.fullName = fullName;

        return node;
    }

    // 辅助方法：将Node转换为Qt树项
    static void populateTreeItem(QTreeWidgetItem* item, const Node& node) {
        if (!item) {
            return;
        }

        item->setText(0, node.name);
        item->setData(0, Qt::UserRole, node.id);
        item->setData(0, Qt::UserRole + 1, static_cast<int>(node.type));
        item->setData(0, Qt::UserRole + 2, node.database);
        item->setData(0, Qt::UserRole + 3, node.schema);
        item->setData(0, Qt::UserRole + 4, node.fullName);

        // 设置图标（可以根据节点类型设置不同图标）
        item->setIcon(0, getIconForNodeType(node.type));

        // 设置工具提示
        QString tooltip = QString("类型: %1").arg(getNodeTypeDisplayName(node.type));
        if (!node.fullName.isEmpty()) {
            tooltip += QString("\n完整名称: %1").arg(node.fullName);
        }
        if (node.metadata.contains("comment") && !node.metadata["comment"].toString().isEmpty()) {
            tooltip += QString("\n注释: %1").arg(node.metadata["comment"].toString());
        }
        item->setToolTip(0, tooltip);
    }

    // 获取节点类型显示名称
    static QString getNodeTypeDisplayName(NodeType type) {
        switch (type) {
            case NodeType::Connection: return "连接";
            case NodeType::Database: return "数据库";
            case NodeType::Schema: return "模式";
            case NodeType::TableFolder: return "表文件夹";
            case NodeType::Table: return "表";
            case NodeType::ViewFolder: return "视图文件夹";
            case NodeType::View: return "视图";
            case NodeType::ProcedureFolder: return "存储过程文件夹";
            case NodeType::Procedure: return "存储过程";
            case NodeType::FunctionFolder: return "函数文件夹";
            case NodeType::Function: return "函数";
            case NodeType::TriggerFolder: return "触发器文件夹";
            case NodeType::Trigger: return "触发器";
            case NodeType::IndexFolder: return "索引文件夹";
            case NodeType::Index: return "索引";
            case NodeType::SequenceFolder: return "序列文件夹";
            case NodeType::Sequence: return "序列";
            case NodeType::UserFolder: return "用户文件夹";
            case NodeType::User: return "用户";
            case NodeType::Column: return "列";
            case NodeType::Key: return "键";
            case NodeType::RedisDatabase: return "Redis数据库";
            case NodeType::RedisKey: return "Redis键";
            case NodeType::RedisKeyFolder: return "Redis键文件夹";
            default: return "未知";
        }
    }

    // 获取节点类型对应的图标
    static QIcon getIconForNodeType(NodeType type) {
        // 这里可以根据节点类型返回相应的图标
        // 暂时返回空图标，实际使用时可以配置图标资源
        switch (type) {
            case NodeType::Connection:
                return QIcon(":/icons/connection.png");
            case NodeType::Database:
            case NodeType::RedisDatabase:
                return QIcon(":/icons/database.png");
            case NodeType::TableFolder:
                return QIcon(":/icons/table_folder.png");
            case NodeType::Table:
                return QIcon(":/icons/table.png");
            case NodeType::ViewFolder:
                return QIcon(":/icons/view_folder.png");
            case NodeType::View:
                return QIcon(":/icons/view.png");
            case NodeType::Column:
                return QIcon(":/icons/column.png");
            case NodeType::RedisKeyFolder:
                return QIcon(":/icons/key_folder.png");
            case NodeType::RedisKey:
                return QIcon(":/icons/key.png");
            default:
                return QIcon(":/icons/default.png");
        }
    }

    // 检查节点是否支持展开
    static bool canExpand(NodeType type) {
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

    // 检查节点是否为叶节点
    static bool isLeafNode(NodeType type) {
        switch (type) {
            case NodeType::Column:
            case NodeType::RedisKey:
            case NodeType::Key:
            case NodeType::Procedure:
            case NodeType::Function:
            case NodeType::Trigger:
            case NodeType::Index:
            case NodeType::Sequence:
            case NodeType::User:
                return true;
            default:
                return false;
        }
    }
};

#endif // NODEEVENTFACTORY_H