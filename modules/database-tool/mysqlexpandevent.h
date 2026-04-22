#ifndef MYSQLEXPANDEVENT_H
#define MYSQLEXPANDEVENT_H

#include "abstractnodeevent.h"

// MySQL展开事件实现
class MySqlExpandEvent : public AbstractExpandEvent {
public:
    MySqlExpandEvent(const QString& connectionId, const NodeChain& nodeChain)
        : AbstractExpandEvent(connectionId, nodeChain) {}

    MySqlExpandEvent(Connx::Connection* connection, const NodeChain& nodeChain)
        : AbstractExpandEvent(connection, nodeChain) {}

protected:
    ExpandResult expandNode(NodeType nodeType) override {
        switch (nodeType) {
            case NodeType::Connection:
                return expandConnection();
            case NodeType::Database:
                return expandDatabase();
            case NodeType::TableFolder:
                return expandTables();
            case NodeType::ViewFolder:
                return expandViews();
            case NodeType::ProcedureFolder:
                return expandProcedures();
            case NodeType::FunctionFolder:
                return expandFunctions();
            case NodeType::TriggerFolder:
                return expandTriggers();
            case NodeType::EventFolder:
                return expandEvents();
            case NodeType::Table:
                return expandTableColumns();
            case NodeType::View:
                return expandViewColumns();
            default:
                return ExpandResult(QString("不支持的MySQL节点类型: %1").arg(static_cast<int>(nodeType)));
        }
    }

private:
    // 展开连接 - 获取所有数据库
    ExpandResult expandConnection() {
        QString sql = "SHOW DATABASES";
        Connx::QueryResult result = executeQuery(sql);

        if (!result.success) {
            return ExpandResult("获取数据库列表失败: " + result.errorMessage);
        }

        QList<Node> databases;
        for (const QVariantList& row : result.rows) {
            if (!row.isEmpty()) {
                QString dbName = row[0].toString();
                // 过滤系统数据库
                if (dbName != "information_schema" && dbName != "performance_schema" &&
                    dbName != "mysql" && dbName != "sys") {

                    Node dbNode(dbName, NodeType::Database, dbName);
                    dbNode.database = dbName;
                    databases.append(dbNode);
                }
            }
        }

        return ExpandResult(true, databases);
    }

    // 展开数据库 - 获取文件夹结构
    ExpandResult expandDatabase() {
        QList<Node> folders;
        QString dbName = nodeChain().databaseName();

        // 创建文件夹节点
        Node tableFolder(dbName + "_tables", NodeType::TableFolder, "表");
        tableFolder.database = dbName;
        folders.append(tableFolder);

        Node viewFolder(dbName + "_views", NodeType::ViewFolder, "视图");
        viewFolder.database = dbName;
        folders.append(viewFolder);

        Node procFolder(dbName + "_procedures", NodeType::ProcedureFolder, "存储过程");
        procFolder.database = dbName;
        folders.append(procFolder);

        Node funcFolder(dbName + "_functions", NodeType::FunctionFolder, "函数");
        funcFolder.database = dbName;
        folders.append(funcFolder);

        Node triggerFolder(dbName + "_triggers", NodeType::TriggerFolder, "触发器");
        triggerFolder.database = dbName;
        folders.append(triggerFolder);

        Node eventFolder(dbName + "_events", NodeType::EventFolder, "事件");
        eventFolder.database = dbName;
        folders.append(eventFolder);

        return ExpandResult(true, folders);
    }

    // 展开表文件夹 - 获取所有表
    ExpandResult expandTables() {
        QString dbName = nodeChain().databaseName();
        QString sql = QString("SELECT TABLE_NAME, TABLE_COMMENT FROM information_schema.TABLES "
                             "WHERE BINARY TABLE_SCHEMA = BINARY '%1' AND TABLE_TYPE = 'BASE TABLE' "
                             "ORDER BY TABLE_NAME").arg(dbName);

        Connx::QueryResult result = executeQuery(sql);
        if (!result.success) {
            return ExpandResult("获取表列表失败: " + result.errorMessage);
        }

        QList<Node> tables;
        for (const QVariantList& row : result.rows) {
            if (row.size() >= 1 && !row[0].isNull()) {
                QString tableName = row[0].toString();
                QString comment = (row.size() > 1 && !row[1].isNull()) ? row[1].toString() : QString();

                Node tableNode(dbName + "." + tableName, NodeType::Table, tableName);
                tableNode.database = dbName;
                tableNode.fullName = dbName + "." + tableName;
                if (!comment.isEmpty()) {
                    tableNode.metadata["comment"] = comment;
                }
                tables.append(tableNode);
            }
        }

        return ExpandResult(true, tables);
    }

    // 展开视图文件夹 - 获取所有视图
    ExpandResult expandViews() {
        QString dbName = nodeChain().databaseName();
        QString sql = QString("SELECT TABLE_NAME, TABLE_COMMENT FROM information_schema.VIEWS "
                             "WHERE BINARY TABLE_SCHEMA = BINARY '%1' ORDER BY TABLE_NAME").arg(dbName);

        Connx::QueryResult result = executeQuery(sql);
        if (!result.success) {
            return ExpandResult("获取视图列表失败: " + result.errorMessage);
        }

        QList<Node> views;
        for (const QVariantList& row : result.rows) {
            if (row.size() >= 1) {
                QString viewName = row[0].toString();
                QString comment = row.size() > 1 ? row[1].toString() : QString();

                Node viewNode(dbName + "." + viewName, NodeType::View, viewName);
                viewNode.database = dbName;
                viewNode.fullName = dbName + "." + viewName;
                if (!comment.isEmpty()) {
                    viewNode.metadata["comment"] = comment;
                }
                views.append(viewNode);
            }
        }

        return ExpandResult(true, views);
    }

    // 展开存储过程文件夹
    ExpandResult expandProcedures() {
        QString dbName = nodeChain().databaseName();
        QString sql = QString("SELECT ROUTINE_NAME, ROUTINE_COMMENT FROM information_schema.ROUTINES "
                             "WHERE BINARY ROUTINE_SCHEMA = BINARY '%1' AND ROUTINE_TYPE = 'PROCEDURE' "
                             "ORDER BY ROUTINE_NAME").arg(dbName);

        Connx::QueryResult result = executeQuery(sql);
        if (!result.success) {
            return ExpandResult("获取存储过程列表失败: " + result.errorMessage);
        }

        QList<Node> procedures;
        for (const QVariantList& row : result.rows) {
            if (row.size() >= 1) {
                QString procName = row[0].toString();
                QString comment = row.size() > 1 ? row[1].toString() : QString();

                Node procNode(dbName + "." + procName, NodeType::Procedure, procName);
                procNode.database = dbName;
                procNode.fullName = dbName + "." + procName;
                if (!comment.isEmpty()) {
                    procNode.metadata["comment"] = comment;
                }
                procedures.append(procNode);
            }
        }

        return ExpandResult(true, procedures);
    }

    // 展开函数文件夹
    ExpandResult expandFunctions() {
        QString dbName = nodeChain().databaseName();
        QString sql = QString("SELECT ROUTINE_NAME, ROUTINE_COMMENT FROM information_schema.ROUTINES "
                             "WHERE BINARY ROUTINE_SCHEMA = BINARY '%1' AND ROUTINE_TYPE = 'FUNCTION' "
                             "ORDER BY ROUTINE_NAME").arg(dbName);

        Connx::QueryResult result = executeQuery(sql);
        if (!result.success) {
            return ExpandResult("获取函数列表失败: " + result.errorMessage);
        }

        QList<Node> functions;
        for (const QVariantList& row : result.rows) {
            if (row.size() >= 1) {
                QString funcName = row[0].toString();
                QString comment = row.size() > 1 ? row[1].toString() : QString();

                Node funcNode(dbName + "." + funcName, NodeType::Function, funcName);
                funcNode.database = dbName;
                funcNode.fullName = dbName + "." + funcName;
                if (!comment.isEmpty()) {
                    funcNode.metadata["comment"] = comment;
                }
                functions.append(funcNode);
            }
        }

        return ExpandResult(true, functions);
    }

    // 展开触发器文件夹
    ExpandResult expandTriggers() {
        QString dbName = nodeChain().databaseName();
        QString sql = QString("SELECT TRIGGER_NAME, EVENT_OBJECT_TABLE FROM information_schema.TRIGGERS "
                             "WHERE BINARY TRIGGER_SCHEMA = BINARY '%1' ORDER BY TRIGGER_NAME").arg(dbName);

        Connx::QueryResult result = executeQuery(sql);
        if (!result.success) {
            return ExpandResult("获取触发器列表失败: " + result.errorMessage);
        }

        QList<Node> triggers;
        for (const QVariantList& row : result.rows) {
            if (row.size() >= 2) {
                QString triggerName = row[0].toString();
                QString tableName = row[1].toString();

                Node triggerNode(dbName + "." + triggerName, NodeType::Trigger, triggerName);
                triggerNode.database = dbName;
                triggerNode.fullName = dbName + "." + triggerName;
                triggerNode.metadata["table"] = tableName;
                triggers.append(triggerNode);
            }
        }

        return ExpandResult(true, triggers);
    }

    // 展开事件文件夹
    ExpandResult expandEvents() {
        QString dbName = nodeChain().databaseName();
        QString sql = QString("SELECT EVENT_NAME, EVENT_TYPE, STATUS FROM information_schema.EVENTS "
                             "WHERE BINARY EVENT_SCHEMA = BINARY '%1' ORDER BY EVENT_NAME").arg(dbName);

        Connx::QueryResult result = executeQuery(sql);
        if (!result.success) {
            return ExpandResult("获取事件列表失败: " + result.errorMessage);
        }

        QList<Node> events;
        for (const QVariantList& row : result.rows) {
            if (row.size() >= 1 && !row[0].isNull()) {
                QString eventName = row[0].toString();
                QString eventType = row.size() > 1 && !row[1].isNull() ? row[1].toString() : QString();
                QString status = row.size() > 2 && !row[2].isNull() ? row[2].toString() : QString();

                Node eventNode(dbName + "." + eventName, NodeType::Event, eventName);
                eventNode.database = dbName;
                eventNode.fullName = dbName + "." + eventName;
                eventNode.metadata["eventType"] = eventType;
                eventNode.metadata["status"] = status;
                events.append(eventNode);
            }
        }

        return ExpandResult(true, events);
    }

    // 展开表列
    ExpandResult expandTableColumns() {
        QString dbName = nodeChain().databaseName();
        Node target = nodeChain().targetNode();
        QString tableName = target.name;

        QString sql = QString("SELECT COLUMN_NAME, DATA_TYPE, IS_NULLABLE, COLUMN_DEFAULT, "
                             "COLUMN_COMMENT, COLUMN_KEY FROM information_schema.COLUMNS "
                             "WHERE BINARY TABLE_SCHEMA = BINARY '%1' AND BINARY TABLE_NAME = BINARY '%2' "
                             "ORDER BY ORDINAL_POSITION").arg(dbName, tableName);

        Connx::QueryResult result = executeQuery(sql);
        if (!result.success) {
            return ExpandResult("获取表列信息失败: " + result.errorMessage);
        }

        QList<Node> columns;
        for (const QVariantList& row : result.rows) {
            if (row.size() >= 6 && !row[0].isNull()) {
                QString columnName = row[0].toString();
                QString dataType = !row[1].isNull() ? row[1].toString() : QString();
                QString nullable = !row[2].isNull() ? row[2].toString() : QString();
                QString defaultVal = !row[3].isNull() ? row[3].toString() : QString();
                QString comment = !row[4].isNull() ? row[4].toString() : QString();
                QString columnKey = !row[5].isNull() ? row[5].toString() : QString();

                Node columnNode(dbName + "." + tableName + "." + columnName,
                               NodeType::Column, columnName);
                columnNode.database = dbName;
                columnNode.fullName = dbName + "." + tableName + "." + columnName;
                columnNode.metadata["dataType"] = dataType;
                columnNode.metadata["nullable"] = nullable;
                columnNode.metadata["default"] = defaultVal;
                columnNode.metadata["comment"] = comment;
                columnNode.metadata["key"] = columnKey;

                columns.append(columnNode);
            }
        }

        return ExpandResult(true, columns);
    }

    // 展开视图列
    ExpandResult expandViewColumns() {
        // 视图列的查询与表列相同
        return expandTableColumns();
    }
};

#endif // MYSQLEXPANDEVENT_H