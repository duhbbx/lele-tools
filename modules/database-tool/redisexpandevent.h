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

        QList<Node> databases;
        for (int i = 0; i < dbCount; i++) {
            // 获取每个数据库的键数量
            QVariant dbSizeResult = executeRedisCommand(QString("SELECT %1").arg(i));
            QVariant keysCount = executeRedisCommand("DBSIZE");

            int keyCount = 0;
            if (!keysCount.isNull()) {
                keyCount = keysCount.toInt();
            }

            QString dbName = QString("db%1").arg(i);
            Node dbNode(dbName, NodeType::RedisDatabase, dbName);
            dbNode.database = dbName;
            dbNode.metadata["index"] = i;
            dbNode.metadata["keyCount"] = keyCount;

            databases.append(dbNode);
        }

        return ExpandResult(true, databases);
    }

    // 展开Redis数据库 - 按键模式分组
    ExpandResult expandRedisDatabase() {
        Node dbNode = nodeChain().targetNode();
        int dbIndex = dbNode.metadata.value("index", 0).toInt();

        // 切换到指定数据库
        QVariant selectResult = executeRedisCommand(QString("SELECT %1").arg(dbIndex));
        if (selectResult.isNull()) {
            return ExpandResult("切换到数据库失败");
        }

        // 获取所有键并按模式分组
        QVariant keysResult = executeRedisCommand("KEYS *");
        if (keysResult.isNull()) {
            return ExpandResult("获取键列表失败");
        }

        QStringList allKeys;
        QVariantList keysList = keysResult.toList();
        for (const QVariant& key : keysList) {
            allKeys.append(key.toString());
        }

        // 按照键的前缀进行分组
        QMap<QString, QStringList> keyGroups;
        QStringList ungroupedKeys;

        for (const QString& key : allKeys) {
            QString prefix = extractKeyPrefix(key);
            if (!prefix.isEmpty()) {
                keyGroups[prefix].append(key);
            } else {
                ungroupedKeys.append(key);
            }
        }

        QList<Node> folders;
        QString dbName = dbNode.database;

        // 为每个前缀创建文件夹
        for (auto it = keyGroups.begin(); it != keyGroups.end(); ++it) {
            QString prefix = it.key();
            QStringList keys = it.value();

            if (keys.size() > 1) { // 只有多个键才创建文件夹
                Node folderNode(dbName + "_" + prefix, NodeType::RedisKeyFolder, prefix + "*");
                folderNode.database = dbName;
                folderNode.metadata["prefix"] = prefix;
                folderNode.metadata["keyCount"] = keys.size();
                folders.append(folderNode);
            } else {
                // 单个键直接添加到未分组中
                ungroupedKeys.append(keys);
            }
        }

        // 添加未分组的键
        for (const QString& key : ungroupedKeys) {
            Node keyNode(dbName + "_" + key, NodeType::RedisKey, key);
            keyNode.database = dbName;
            keyNode.fullName = key;

            // 获取键的类型
            QVariant typeResult = executeRedisCommand(QString("TYPE %1").arg(key));
            if (!typeResult.isNull()) {
                keyNode.metadata["type"] = typeResult.toString();
            }

            // 获取TTL
            QVariant ttlResult = executeRedisCommand(QString("TTL %1").arg(key));
            if (!ttlResult.isNull()) {
                keyNode.metadata["ttl"] = ttlResult.toInt();
            }

            folders.append(keyNode);
        }

        return ExpandResult(true, folders);
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