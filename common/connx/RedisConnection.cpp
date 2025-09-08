#include "RedisConnection.h"
#include <QTcpSocket>
#include <QSslSocket>
#include <QDebug>
#include <QDateTime>
#include <QRegularExpression>
#include <QThread>
#include <QEventLoop>
#include <QTimer>
#include <unordered_map>
#include <vector>
#include <iterator>

namespace Connx {
    // RedisConnection 实现
    RedisConnection::RedisConnection(const ConnectionConfig& config, QObject* parent)
        : Connection(config, parent), m_currentDatabase(0), m_authenticated(false), m_connected(false) {
        m_type = ConnectionType::Redis;


        // 创建连接选项
        m_connOpts = std::make_unique<sw::redis::ConnectionOptions>();
        m_connOpts->host = config.host.toStdString();
        m_connOpts->port = config.port;
        m_connOpts->password = config.password.toStdString();
        m_connOpts->db = config.database.toInt();
        m_connOpts->connect_timeout = std::chrono::milliseconds(config.timeout * 1000);
        m_connOpts->socket_timeout = std::chrono::milliseconds(config.timeout * 1000);

        // 创建连接池选项
        m_poolOpts = std::make_unique<sw::redis::ConnectionPoolOptions>();
        m_poolOpts->size = 3; // 连接池大小
        m_poolOpts->wait_timeout = std::chrono::milliseconds(config.timeout * 1000);

        // SSL支持
        if (config.useSSL) {
            // redis-plus-plus的TLS配置
            // 创建TLS选项
            sw::redis::tls::TlsOptions tls;
            // 可以配置证书文件路径（可选）
            // tls.cert_file = "/path/to/cert.pem";
            // tls.key_file = "/path/to/key.pem";
            // tls.ca_file = "/path/to/ca.pem";
            // tls.ca_dir = "/path/to/ca/dir";

            m_connOpts->tls = tls;
        }
    }

    RedisConnection::~RedisConnection() {
        disconnectFromServer();
    }

    bool RedisConnection::connectToServer() {
        if (isConnected()) {
            return true;
        }

        setState(ConnectionState::Connecting);

        try {
            // 创建Redis连接
            m_redis = std::make_unique<sw::redis::Redis>(*m_connOpts, *m_poolOpts);

            // 测试连接
            if (const std::string pong = m_redis->ping(); pong == "PONG") {
                m_connected = true;
                m_currentDatabase = m_connOpts->db;
                setState(ConnectionState::Connected);
                updateLastActivity();

                // 获取服务器版本信息
                const auto info = m_redis->info("server");
                m_serverVersion = QString::fromStdString(info);


                qDebug() << "Redis connected successfully, version:" << m_serverVersion;
                return true;
            }
        } catch (const sw::redis::Error& e) {
            setError(QString("Redis connection error: %1").arg(e.what()));
            m_connected = false;
            return false;
        } catch (const std::exception& e) {
            setError(QString("Connection error: %1").arg(e.what()));
            m_connected = false;
            return false;
        }
        return false;
    }

    void RedisConnection::disconnectFromServer() {
#ifdef WITH_REDIS_PLUS_PLUS
        if (m_redis) {
            try {
                // redis-plus-plus会自动管理连接，这里只需要重置指针
                m_redis.reset();
                m_connected = false;
                setState(ConnectionState::Disconnected);
                qDebug() << "Redis disconnected";
            } catch (const std::exception& e) {
                qDebug() << "Error during Redis disconnect:" << e.what();
            }
        }
#endif
    }

    bool RedisConnection::isConnected() const {
        return m_connected && m_state == ConnectionState::Connected;
    }

    bool RedisConnection::ping() {
        if (!isConnected() || !m_redis) {
            return false;
        }

        try {
            const std::string result = m_redis->ping();
            updateLastActivity();
            return result == "PONG";
        } catch (const std::exception& e) {
            qDebug() << "Redis ping failed:" << e.what();
            return false;
        }
    }

    QueryResult RedisConnection::execute(const QString& command, const QVariantList& params) {
        QueryResult result;

        if (!isConnected() || !m_redis) {
            result.errorMessage = "Not connected to Redis server";
            return result;
        }

        const qint64 startTime = QDateTime::currentMSecsSinceEpoch();

        try {
            const QVariant response = executeRedisCommand(command, params);

            result.executionTime = QDateTime::currentMSecsSinceEpoch() - startTime;
            result.success = true;
            
            // 如果response已经是一个列表，直接使用它；否则包装成列表
            if (response.type() == QVariant::List) {
                result.data = response.toList();
            } else {
                result.data.append(response);
            }
            updateLastActivity();
        } catch (const sw::redis::Error& e) {
            result.success = false;
            result.errorMessage = QString("Redis error: %1").arg(e.what());
            result.executionTime = QDateTime::currentMSecsSinceEpoch() - startTime;
        } catch (const std::exception& e) {
            result.success = false;
            result.errorMessage = QString("Error: %1").arg(e.what());
            result.executionTime = QDateTime::currentMSecsSinceEpoch() - startTime;
        }

        return result;
    }

    QueryResult RedisConnection::query(const QString& sql, const QVariantList& params) {
        // Redis不使用SQL，直接调用execute
        return execute(sql, params);
    }

    bool RedisConnection::beginTransaction() {
#ifdef WITH_REDIS_PLUS_PLUS
        if (!isConnected() || !m_redis) {
            return false;
        }

        try {
            auto tx = m_redis->transaction();
            // redis-plus-plus的事务是自动管理的
            return true;
        } catch (const std::exception& e) {
            qDebug() << "Failed to begin transaction:" << e.what();
            return false;
        }
#else
        return false;
#endif
    }

    bool RedisConnection::commitTransaction() {
        // redis-plus-plus的事务在析构时自动提交
        return true;
    }

    bool RedisConnection::rollbackTransaction() {
        // redis-plus-plus的事务可以通过discard取消
        if (!isConnected() || !m_redis) {
            return false;
        }

        try {
            // TODO YANGXU redis的事务不是这么用的
            // m_redis->discard();
            return true;
        } catch (const std::exception& e) {
            qDebug() << "Failed to rollback transaction:" << e.what();
            return false;
        }
    }

    QStringList RedisConnection::getDatabases() {
        QStringList databases;

        if (!isConnected() || !m_redis) {
            return databases;
        }

        int dbCount = 16; // 默认16个数据库
        try {
            if (const auto reply = m_redis->command<std::vector<std::string>>("CONFIG", "GET", "databases"); reply.size() >= 2) {
                dbCount = std::stoi(reply[1]);
            }
        } catch (const std::exception& e) {
            qDebug() << "Failed to get databases:" << e.what();
            // 保持默认 dbCount = 16
        }

        for (int i = 0; i < dbCount; ++i) {
            databases.append(QString("DB%1").arg(i));
        }

        return databases;
    }


    QStringList RedisConnection::getTables(const QString& database) {
        // Redis中的"表"相当于键，这里返回键列表
        return getKeys("*", 1000);
    }

    QStringList RedisConnection::getColumns(const QString& table, const QString& database) {
        // Redis 的 key 并不像关系型数据库有固定的“列”，
        // 如果 key 是一个 HASH 类型，我们可以把它的 field 当作“列名”返回。

        QStringList columns;

        if (!isConnected() || !m_redis) {
            return columns; // 未连接，直接返回空
        }

        try {
            // 获取 key 的类型（string/hash/list/set/zset...）

            if (const auto type = m_redis->type(table.toStdString()); type == "hash") {
                // hkeys 需要一个输出迭代器，把 field 名称存入一个 std::vector<std::string>
                std::vector<std::string> fields;
                m_redis->hkeys(table.toStdString(), std::back_inserter(fields));

                // 转换成 QStringList
                for (const auto& field : fields) {
                    columns.append(QString::fromStdString(field));
                }
            }
        } catch (const std::exception& e) {
            qDebug() << "Failed to get columns for key" << table << ":" << e.what();
        }

        return columns;
    }


    QString RedisConnection::escapeString(const QString& str) {
        // Redis字符串转义
        QString escaped = str;
        escaped.replace("\\", "\\\\");
        escaped.replace("\"", "\\\"");
        escaped.replace("\n", "\\n");
        escaped.replace("\r", "\\r");
        escaped.replace("\t", "\\t");
        return escaped;
    }

    QString RedisConnection::getServerInfo() {
        if (!isConnected() || !m_redis) {
            return "";
        }

        try {
            const auto info = m_redis->info();
            QString result = QString::fromStdString(info);
            return result;
        } catch (const std::exception& e) {
            qDebug() << "Failed to get server info:" << e.what();
            return QString("Error: %1").arg(e.what());
        }
    }

    QJsonObject RedisConnection::getConnectionInfo() {
        QJsonObject info;
        info["type"] = "Redis";
        info["host"] = m_config.host;
        info["port"] = m_config.port;
        info["database"] = m_currentDatabase;
        info["connected"] = isConnected();
        info["authenticated"] = m_authenticated;
        info["server_version"] = m_serverVersion;
        info["last_activity"] = QDateTime::fromMSecsSinceEpoch(m_lastActivity).toString();

        return info;
    }

    // Redis特有方法实现
    QStringList RedisConnection::getKeys(const QString& pattern, int limit) {
        QStringList keys;

        if (!isConnected() || !m_redis) {
            return keys;
        }

        // try {
        //     long long cursor = 0;                 // SCAN命令的游标
        //     std::vector<std::string> keyBatch;    // 临时存放每次SCAN返回的键
        //
        //     do {
        //         keyBatch.clear();
        //         // scan 返回新的 cursor，keyBatch 用 back_inserter 接收结果
        //         cursor = m_redis->scan(cursor, pattern.toStdString(),
        //                                std::back_inserter(keyBatch), 100);
        //
        //         for (const auto &key : keyBatch) {
        //             keys.append(QString::fromStdString(key));
        //             if (keys.size() >= limit)
        //                 break;  // 达到限制数量就停止
        //         }
        //     } while (cursor != 0 && keys.size() < limit);
        //
        // } catch (const std::exception &e) {
        //     qDebug() << "Failed to get keys:" << e.what();
        // }

        return keys;
    }


    RedisKeyInfo RedisConnection::getKeyInfo(const QString& key) {
        RedisKeyInfo info;
        info.key = key;


        if (!isConnected() || !m_redis) {
            return info;
        }

        // try {
        //     std::string keyStr = key.toStdString();
        //
        //     // 获取键类型
        //     auto type = m_redis->type(keyStr);
        //     switch (type) {
        //     case sw::redis::KeyType::STRING: info.type = RedisDataType::String;
        //         break;
        //     case sw::redis::KeyType::HASH: info.type = RedisDataType::Hash;
        //         break;
        //     case sw::redis::KeyType::LIST: info.type = RedisDataType::List;
        //         break;
        //     case sw::redis::KeyType::SET: info.type = RedisDataType::Set;
        //         break;
        //     case sw::redis::KeyType::ZSET: info.type = RedisDataType::ZSet;
        //         break;
        //     case sw::redis::KeyType::STREAM: info.type = RedisDataType::Stream;
        //         break;
        //     default: info.type = RedisDataType::Unknown;
        //         break;
        //     }
        //
        //     // 获取TTL
        //     info.ttl = m_redis->ttl(keyStr);
        //
        //     // 获取大小
        //     switch (info.type) {
        //     case RedisDataType::String:
        //         info.size = m_redis->strlen(keyStr);
        //         break;
        //     case RedisDataType::Hash:
        //         info.size = m_redis->hlen(keyStr);
        //         break;
        //     case RedisDataType::List:
        //         info.size = m_redis->llen(keyStr);
        //         break;
        //     case RedisDataType::Set:
        //         info.size = m_redis->scard(keyStr);
        //         break;
        //     case RedisDataType::ZSet:
        //         info.size = m_redis->zcard(keyStr);
        //         break;
        //     default:
        //         info.size = 0;
        //         break;
        //     }
        // } catch (const std::exception& e) {
        //     qDebug() << "Failed to get key info for" << key << ":" << e.what();
        // }

        return info;
    }

    QVariant RedisConnection::getValue(const QString& key) {
        if (!isConnected() || !m_redis) {
            return QVariant();
        }

        try {
            std::string keyStr = key.toStdString();
            auto type = m_redis->type(keyStr);

            // switch (type) {
            // case sw::redis::KeyType::STRING: {
            //     auto value = m_redis->get(keyStr);
            //     return value ? QString::fromStdString(*value) : QVariant();
            // }
            // case sw::redis::KeyType::HASH: {
            //     std::unordered_map<std::string, std::string> hash;
            //     m_redis->hgetall(keyStr, std::inserter(hash, hash.begin()));
            //     QVariantMap result;
            //     for (const auto& pair : hash) {
            //         result[QString::fromStdString(pair.first)] = QString::fromStdString(pair.second);
            //     }
            //     return result;
            // }
            // case sw::redis::KeyType::LIST: {
            //     std::vector<std::string> list;
            //     m_redis->lrange(keyStr, 0, -1, std::back_inserter(list));
            //     return convertRedisArray(list);
            // }
            // case sw::redis::KeyType::SET: {
            //     std::vector<std::string> set;
            //     m_redis->smembers(keyStr, std::back_inserter(set));
            //     return convertRedisArray(set);
            // }
            // case sw::redis::KeyType::ZSET: {
            //     std::vector<std::string> zset;
            //     m_redis->zrange(keyStr, 0, -1, std::back_inserter(zset));
            //     return convertRedisArray(zset);
            // }
            // default:
            //     return QVariant();
            // }

            return QVariant();
        } catch (const std::exception& e) {
            qDebug() << "Failed to get value for key" << key << ":" << e.what();
            return QVariant();
        }
    }

    bool RedisConnection::setValue(const QString& key, const QVariant& value, int ttl) {
        if (!isConnected() || !m_redis) {
            return false;
        }

        try {
            const std::string keyStr = key.toStdString();
            const std::string valueStr = value.toString().toStdString();

            if (ttl > 0) {
                m_redis->setex(keyStr, std::chrono::seconds(ttl), valueStr);
            } else {
                m_redis->set(keyStr, valueStr);
            }

            updateLastActivity();
            return true;
        } catch (const std::exception& e) {
            qDebug() << "Failed to set value for key" << key << ":" << e.what();
            return false;
        }
    }

    bool RedisConnection::deleteKey(const QString& key) {
#ifdef WITH_REDIS_PLUS_PLUS
        if (!isConnected() || !m_redis) {
            return false;
        }

        try {
            long long deleted = m_redis->del(key.toStdString());
            updateLastActivity();
            return deleted > 0;
        } catch (const std::exception& e) {
            qDebug() << "Failed to delete key" << key << ":" << e.what();
            return false;
        }
#else
        Q_UNUSED(key)
        return false;
#endif
    }

    QStringList RedisConnection::getHashFields(const QString& key) {
        QStringList fields;

#ifdef WITH_REDIS_PLUS_PLUS
        if (!isConnected() || !m_redis) {
            return fields;
        }

        try {
            std::vector<std::string> redisFields;
            m_redis->hkeys(key.toStdString(), std::back_inserter(redisFields));
            for (const auto& field : redisFields) {
                fields.append(QString::fromStdString(field));
            }
        } catch (const std::exception& e) {
            qDebug() << "Failed to get hash fields for" << key << ":" << e.what();
        }
#endif

        return fields;
    }

    QVariantMap RedisConnection::getHashAll(const QString& key) {
        QVariantMap result;

#ifdef WITH_REDIS_PLUS_PLUS
        if (!isConnected() || !m_redis) {
            return result;
        }

        try {
            std::unordered_map<std::string, std::string> hash;
            m_redis->hgetall(key.toStdString(), std::inserter(hash, hash.begin()));
            for (const auto& pair : hash) {
                result[QString::fromStdString(pair.first)] = QString::fromStdString(pair.second);
            }
        } catch (const std::exception& e) {
            qDebug() << "Failed to get hash for" << key << ":" << e.what();
        }
#endif

        return result;
    }

    QVariantList RedisConnection::getListRange(const QString& key, int start, int end) {
        QVariantList result;

#ifdef WITH_REDIS_PLUS_PLUS
        if (!isConnected() || !m_redis) {
            return result;
        }

        try {
            std::vector<std::string> list;
            m_redis->lrange(key.toStdString(), start, end, std::back_inserter(list));
            result = convertRedisArray(list);
        } catch (const std::exception& e) {
            qDebug() << "Failed to get list range for" << key << ":" << e.what();
        }
#endif

        return result;
    }

    QStringList RedisConnection::getSetMembers(const QString& key) {
        QStringList members;

#ifdef WITH_REDIS_PLUS_PLUS
        if (!isConnected() || !m_redis) {
            return members;
        }

        try {
            std::vector<std::string> set;
            m_redis->smembers(key.toStdString(), std::back_inserter(set));
            for (const auto& member : set) {
                members.append(QString::fromStdString(member));
            }
        } catch (const std::exception& e) {
            qDebug() << "Failed to get set members for" << key << ":" << e.what();
        }
#endif

        return members;
    }

    QVariantList RedisConnection::getZSetRange(const QString& key, int start, int end, bool withScores) {
        QVariantList result;

#ifdef WITH_REDIS_PLUS_PLUS
        if (!isConnected() || !m_redis) {
            return result;
        }

        try {
            std::string keyStr = key.toStdString();

            if (withScores) {
                // 获取带分数的有序集合
                std::vector<std::pair<std::string, double>> zsetWithScores;
                m_redis->zrange(keyStr, start, end, std::back_inserter(zsetWithScores));

                for (const auto& pair : zsetWithScores) {
                    result.append(QString::fromStdString(pair.first));
                    result.append(pair.second);
                }
            } else {
                // 只获取成员，不要分数
                std::vector<std::string> zsetMembers;
                m_redis->zrange(keyStr, start, end, std::back_inserter(zsetMembers));
                result = convertRedisArray(zsetMembers);
            }
        } catch (const std::exception& e) {
            qDebug() << "Failed to get zset range for" << key << ":" << e.what();
        }
#endif

        return result;
    }

    bool RedisConnection::selectDatabase(int dbIndex) {
        if (!isConnected() || !m_redis) {
            return false;
        }

        // try {
        //     m_redis->select(dbIndex);  // v3 直接支持 SELECT
        //     m_currentDatabase = dbIndex;
        //     updateLastActivity();
        //     return true;
        // } catch (const std::exception &e) {
        //     qDebug() << "Failed to select database" << dbIndex << ":" << e.what();
        //     return false;
        // }
        return false;
    }


    // 私有辅助方法
    QVariant RedisConnection::executeRedisCommand(const QString& command, const QVariantList& params) {
        if (!m_redis)
            return { };

        std::vector<std::string> args;
        args.push_back(command.toStdString());
        for (const auto& param : params) {
            args.push_back(param.toString().toStdString());
        }

        try {
            auto reply = m_redis->command(args.begin(), args.end());
            if (!reply)
                return { };

            return parseRedisReply(reply.get());
        } catch (const std::exception& e) {
            qDebug() << "Redis command failed:" << e.what();
            return { };
        }
    }

    // 辅助递归解析redisReply
    QVariant RedisConnection::parseRedisReply(redisReply* reply) {
        switch (reply->type) {
        case REDIS_REPLY_STRING:
        case REDIS_REPLY_STATUS:
            return QString::fromStdString(reply->str ? reply->str : "");
        case REDIS_REPLY_INTEGER:
            return static_cast<qlonglong>(reply->integer);
        case REDIS_REPLY_ARRAY: {
            QVariantList list;
            for (size_t i = 0; i < reply->elements; ++i) {
                list.append(parseRedisReply(reply->element[i]));
            }
            return list;
        }
        case REDIS_REPLY_NIL:
        case REDIS_REPLY_ERROR:
        default:
            return QVariant();
        }
    }


    QVariantList RedisConnection::convertRedisArray(const std::vector<std::string>& values) {
        QVariantList result;
        for (const auto& value : values) {
            result.append(QString::fromStdString(value));
        }
        return result;
    }


    void RedisConnection::keepAlive() {
        if (isConnected()) {
            if (!ping()) {
                setError("Keep-alive ping failed");
            } else {
                updateLastActivity();
            }
        }
    }

    QMap<QString, QString> RedisConnection::getServerConfig() {
        QMap<QString, QString> config;

        if (!isConnected() || !m_redis) {
            return config;
        }

        try {
            const auto info = m_redis->info();
            config["info"] = QString::fromStdString(info);
        } catch (const std::exception& e) {
            qDebug() << "Failed to get server stats:" << e.what();
        }

        return config;
    }

    QMap<QString, QVariant> RedisConnection::getServerStats() const {
        QMap<QString, QVariant> stats;

        if (!isConnected() || !m_redis) {
            return stats;
        }

        try {
            const auto info = m_redis->info();
            stats["info"] = QString::fromStdString(info);
        } catch (const std::exception& e) {
            qDebug() << "Failed to get server stats:" << e.what();
        }

        return stats;
    }
} // namespace Connx
