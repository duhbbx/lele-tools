#pragma once

#include "Connection.h"
#include <QMutex>
#include <memory>

#ifdef WITH_REDIS_PLUS_PLUS
#include <sw/redis++/redis++.h>
#endif

namespace Connx {

// Redis数据类型
enum class RedisDataType {
    String,
    Hash,
    List,
    Set,
    ZSet,
    Stream,
    Unknown
};

// Redis键信息
struct RedisKeyInfo {
    QString key;
    RedisDataType type;
    qint64 ttl;
    qint64 size;
    QString encoding;
    
    RedisKeyInfo() : type(RedisDataType::Unknown), ttl(-1), size(0) {}
};


// Redis连接实现
class RedisConnection : public Connection {
    Q_OBJECT

public:
    explicit RedisConnection(const ConnectionConfig& config, QObject* parent = nullptr);
    ~RedisConnection() override;

    // 基本连接操作
    bool connectToServer() override;
    void disconnectFromServer() override;
    bool isConnected() const override;
    bool ping() override;
    
    // 查询操作
    QueryResult execute(const QString& command, const QVariantList& params = QVariantList()) override;
    QueryResult query(const QString& sql, const QVariantList& params = QVariantList()) override;
    
    // 事务操作
    bool beginTransaction() override;
    bool commitTransaction() override;
    bool rollbackTransaction() override;
    
    // 元数据操作
    QStringList getDatabases() override;
    QStringList getTables(const QString& database = "") override;
    QStringList getColumns(const QString& table, const QString& database = "") override;
    
    // 工具方法
    QString escapeString(const QString& str) override;
    QString getServerInfo() override;
    QJsonObject getConnectionInfo() override;

    // Redis特有方法
    QStringList getKeys(const QString& pattern = "*", int limit = 1000);
    RedisKeyInfo getKeyInfo(const QString& key);
    QVariant getValue(const QString& key);
    bool setValue(const QString& key, const QVariant& value, int ttl = -1);
    bool deleteKey(const QString& key);
    QStringList getHashFields(const QString& key);
    QVariantMap getHashAll(const QString& key);
    QVariantList getListRange(const QString& key, int start = 0, int end = -1);
    QStringList getSetMembers(const QString& key);
    QVariantList getZSetRange(const QString& key, int start = 0, int end = -1, bool withScores = false);
    
    // 数据库操作
    bool selectDatabase(int dbIndex);
    int getCurrentDatabase() const { return m_currentDatabase; }
    QMap<QString, QString> getServerConfig();
    QMap<QString, QVariant> getServerStats() const;

protected slots:
    void keepAlive() override;

private:

    QVariant parseRedisReply(redisReply* reply);
    // 内部方法
    bool authenticate();
    QVariant executeRedisCommand(const QString& command, const QVariantList& params = QVariantList());
    QString buildConnectionString() const;
#ifdef WITH_REDIS_PLUS_PLUS
    QVariant convertRedisValue(const sw::redis::OptionalString& value);
#endif
#ifdef WITH_REDIS_PLUS_PLUS
    QVariantList convertRedisArray(const std::vector<std::string>& values);
#endif
    
    // Redis客户端

    std::unique_ptr<sw::redis::Redis> m_redis;
    std::unique_ptr<sw::redis::ConnectionOptions> m_connOpts;
    std::unique_ptr<sw::redis::ConnectionPoolOptions> m_poolOpts;

    // 状态管理
    QMutex m_commandMutex;
    int m_currentDatabase;
    bool m_authenticated;
    QString m_serverVersion;
    bool m_connected;
};

} // namespace Connx
