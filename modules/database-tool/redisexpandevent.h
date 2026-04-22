#ifndef REDISEXPANDEVENT_H
#define REDISEXPANDEVENT_H

#include "abstractnodeevent.h"
#include <QRegularExpression>

// Redis展开事件实现
class RedisExpandEvent : public AbstractExpandEvent {
public:
    RedisExpandEvent(const QString& connectionId, const NodeChain& nodeChain)
        : AbstractExpandEvent(connectionId, nodeChain) {}

    RedisExpandEvent(Connx::Connection* connection, const NodeChain& nodeChain)
        : AbstractExpandEvent(connection, nodeChain) {}

protected:
    ExpandResult expandNode(NodeType nodeType) override {
        switch (nodeType) {
            case NodeType::Connection:
                return expandConnection();
            case NodeType::RedisDatabase:
                return expandRedisDatabase();
            case NodeType::RedisKeyFolder:
                return expandKeyFolder();
            default:
                return ExpandResult(QString("不支持的Redis节点类型: %1").arg(static_cast<int>(nodeType)));
        }
    }

private:
    // 展开连接 - 获取所有Redis数据库
    ExpandResult expandConnection() {
        // 获取Redis数据库数量配置
        QVariant result = executeRedisCommand("CONFIG GET databases");

        int dbCount = 16; // 默认16个数据库
        if (!result.isNull()) {
            // 解析CONFIG GET的结果，通常是数组格式 ["databases", "16"]
            QVariantList configResult = result.toList();
            if (configResult.size() >= 2) {
                bool ok;
                int count = configResult[1].toInt(&ok);
                if (ok && count > 0) {
                    dbCount = count;
                }
            }
        }

        // 使用 INFO keyspace 一次性获取所有库的 key 数量
        QMap<int, int> dbKeyCounts;
        QVariant infoResult = executeRedisCommand("INFO keyspace");
        if (!infoResult.isNull()) {
            // INFO keyspace 返回格式: "# Keyspace\r\ndb0:keys=15,expires=2,...\r\ndb1:keys=3,...\r\n"
            // executeRedisCommand 可能返回 QVariantList 或 QString
            QString infoStr;
            if (infoResult.typeId() == QMetaType::QVariantList) {
                QVariantList list = infoResult.toList();
                if (!list.isEmpty()) infoStr = list.first().toString();
            } else {
                infoStr = infoResult.toString();
            }
            QStringList lines = infoStr.split('\n');
            for (const QString& line : lines) {
                QString trimmed = line.trimmed();
                if (trimmed.startsWith("db")) {
                    // 解析 "db0:keys=15,expires=2,avg_ttl=0"
                    int colonIdx = trimmed.indexOf(':');
                    if (colonIdx > 0) {
                        bool ok;
                        int dbIdx = trimmed.mid(2, colonIdx - 2).toInt(&ok);
                        if (ok) {
                            QString params = trimmed.mid(colonIdx + 1);
                            for (const QString& param : params.split(',')) {
                                if (param.startsWith("keys=")) {
                                    int keys = param.mid(5).toInt();
                                    dbKeyCounts[dbIdx] = keys;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }

        QList<Node> databases;
        for (int i = 0; i < dbCount; i++) {
            int keyCount = dbKeyCounts.value(i, 0);

            QString dbName = QString("db%1").arg(i);
            Node dbNode(dbName, NodeType::RedisDatabase, dbName);
            dbNode.database = dbName;
            dbNode.metadata["index"] = i;
            dbNode.metadata["keyCount"] = keyCount;

            databases.append(dbNode);
        }

        return ExpandResult(true, databases);
    }

    // 展开Redis数据库 - 直接列出所有 key
    ExpandResult expandRedisDatabase() {
        Node dbNode = nodeChain().targetNode();
        int dbIndex = dbNode.metadata.value("index", 0).toInt();
        QString dbName = dbNode.database;

        // 切换到指定数据库
        QVariant selectResult = executeRedisCommand(QString("SELECT %1").arg(dbIndex));
        if (selectResult.isNull()) {
            return ExpandResult("切换到数据库失败");
        }

        // 获取所有键
        QVariant keysResult = executeRedisCommand("KEYS *");
        if (keysResult.isNull()) {
            return ExpandResult("获取键列表失败");
        }

        QStringList allKeys;
        QVariantList keysList = keysResult.toList();
        for (const QVariant& key : keysList) {
            allKeys.append(key.toString());
        }

        // 按字母排序
        allKeys.sort(Qt::CaseInsensitive);

        QList<Node> nodes;
        for (const QString& key : allKeys) {
            Node keyNode(dbName + "_" + key, NodeType::RedisKey, key);
            keyNode.database = dbName;
            keyNode.fullName = key;
            keyNode.metadata["dbIndex"] = dbIndex;

            // 获取键的类型
            QVariant typeResult = executeRedisCommand(QString("TYPE %1").arg(key));
            if (!typeResult.isNull()) {
                keyNode.metadata["type"] = typeResult.toString();
            }

            nodes.append(keyNode);
        }

        return ExpandResult(true, nodes);
    }

    // 展开键文件夹 - 获取特定前缀的所有键
    ExpandResult expandKeyFolder() {
        Node folderNode = nodeChain().targetNode();
        QString prefix = folderNode.metadata.value("prefix").toString();
        QString dbName = folderNode.database;

        // 获取指定前缀的键
        QString pattern = prefix + "*";
        QVariant keysResult = executeRedisCommand(QString("KEYS %1").arg(pattern));

        if (keysResult.isNull()) {
            return ExpandResult("获取键列表失败");
        }

        QList<Node> keys;
        QVariantList keysList = keysResult.toList();

        for (const QVariant& keyVar : keysList) {
            QString key = keyVar.toString();

            Node keyNode(dbName + "_" + key, NodeType::RedisKey, key);
            keyNode.database = dbName;
            keyNode.fullName = key;

            // 获取键的类型
            QVariant typeResult = executeRedisCommand(QString("TYPE %1").arg(key));
            if (!typeResult.isNull()) {
                QString keyType = typeResult.toString();
                keyNode.metadata["type"] = keyType;

                // 根据类型获取额外信息
                populateKeyMetadata(keyNode, key, keyType);
            }

            // 获取TTL
            QVariant ttlResult = executeRedisCommand(QString("TTL %1").arg(key));
            if (!ttlResult.isNull()) {
                keyNode.metadata["ttl"] = ttlResult.toInt();
            }

            keys.append(keyNode);
        }

        return ExpandResult(true, keys);
    }

    // 提取键的前缀（用于分组）
    QString extractKeyPrefix(const QString& key) {
        // 常见的分隔符：:, _, -, .
        QRegularExpression regex("^([^:_\\-\\.]+)[:_\\-\\.]");
        QRegularExpressionMatch match = regex.match(key);

        if (match.hasMatch()) {
            return match.captured(1);
        }

        return QString(); // 没有明显的前缀
    }

    // 根据键类型填充元数据
    void populateKeyMetadata(Node& keyNode, const QString& key, const QString& keyType) {
        if (keyType == "string") {
            QVariant lenResult = executeRedisCommand(QString("STRLEN %1").arg(key));
            if (!lenResult.isNull()) {
                keyNode.metadata["length"] = lenResult.toInt();
            }
        } else if (keyType == "list") {
            QVariant lenResult = executeRedisCommand(QString("LLEN %1").arg(key));
            if (!lenResult.isNull()) {
                keyNode.metadata["length"] = lenResult.toInt();
            }
        } else if (keyType == "set") {
            QVariant cardResult = executeRedisCommand(QString("SCARD %1").arg(key));
            if (!cardResult.isNull()) {
                keyNode.metadata["cardinality"] = cardResult.toInt();
            }
        } else if (keyType == "zset") {
            QVariant cardResult = executeRedisCommand(QString("ZCARD %1").arg(key));
            if (!cardResult.isNull()) {
                keyNode.metadata["cardinality"] = cardResult.toInt();
            }
        } else if (keyType == "hash") {
            QVariant lenResult = executeRedisCommand(QString("HLEN %1").arg(key));
            if (!lenResult.isNull()) {
                keyNode.metadata["fieldCount"] = lenResult.toInt();
            }
        }
    }
};

#endif // REDISEXPANDEVENT_H