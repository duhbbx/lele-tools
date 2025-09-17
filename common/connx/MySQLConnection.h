#pragma once

// MySQL连接实现 - 使用原生MySQL API以获得最佳性能
// 注意：SQLite连接仍使用Qt SQL (QSQLITE驱动)
#include "Connection.h"
#include <QMutex>
#include <memory>

#ifdef WITH_MYSQL
// 解决Windows下Winsock冲突问题
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#include <mysql.h>
#endif

namespace Connx {

// MySQL特有配置
struct MySQLConnectionConfig {
    QString charset;        // 字符集 (默认: utf8mb4)
    bool autoReconnect;     // 自动重连 (默认: true)
    int maxReconnects;      // 最大重连次数 (默认: 3)
    bool useCompression;    // 使用压缩 (默认: false)
    QString initCommand;    // 初始化命令

    MySQLConnectionConfig()
        : charset("utf8mb4"), autoReconnect(true), maxReconnects(3), useCompression(false) {}
};

// MySQL连接实现
class MySQLConnection : public Connection {
    Q_OBJECT

public:
    explicit MySQLConnection(const ConnectionConfig& config, QObject* parent = nullptr);
    ~MySQLConnection() override;

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

    // MySQL特有方法
    bool selectDatabase(const QString& database);
    QString getCurrentDatabase() const { return m_currentDatabase; }

    // 表结构信息
    struct ColumnInfo {
        QString name;
        QString type;
        bool isNullable;
        QString defaultValue;
        bool isPrimaryKey;
        bool isAutoIncrement;
        QString comment;
    };

    QList<ColumnInfo> getTableSchema(const QString& table, const QString& database = "");
    QStringList getTableIndexes(const QString& table, const QString& database = "");
    QMap<QString, QVariant> getTableStats(const QString& table, const QString& database = "");

    // 数据库管理
    bool createDatabase(const QString& database, const QString& charset = "utf8mb4");
    bool dropDatabase(const QString& database);
    bool createTable(const QString& table, const QList<ColumnInfo>& columns, const QString& database = "");
    bool dropTable(const QString& table, const QString& database = "");

    // 数据操作
    QueryResult insert(const QString& table, const QVariantMap& data, const QString& database = "");
    QueryResult update(const QString& table, const QVariantMap& data, const QString& where, const QVariantList& whereParams = QVariantList(), const QString& database = "");
    QueryResult deleteFrom(const QString& table, const QString& where, const QVariantList& whereParams = QVariantList(), const QString& database = "");

    // 特殊查询
    QVariant getLastInsertId();
    int getAffectedRows();
    QString getLastError() const;

    // 配置
    void setMySQLConfig(const MySQLConnectionConfig& config) { m_mysqlConfig = config; }
    MySQLConnectionConfig getMySQLConfig() const { return m_mysqlConfig; }

protected slots:
    void keepAlive() override;

private:
    // 内部方法
    bool authenticate();
    bool reconnect();
    void handleError(const QString& operation);
    QString buildConnectionString() const;
    QVariant convertMySQLValue(const QString& value, const QString& type);
    QString preparePlaceholder(int index) const;
    QString bindParameters(const QString& sql, const QVariantList& params);

#ifdef WITH_MYSQL
    QueryResult executeNativeQuery(const QString& sql, const QVariantList& params);
    bool connectWithNativeAPI();
#endif

    // 连接标识符
    QString m_connectionId;

#ifdef WITH_MYSQL
    // 原生MySQL连接（主要实现）
    MYSQL* m_mysql;
    MYSQL_RES* m_result;
#endif

    // 状态管理
    QMutex m_queryMutex;
    QString m_currentDatabase;
    bool m_inTransaction;
    MySQLConnectionConfig m_mysqlConfig;
    QString m_serverVersion;
    int m_reconnectCount;
    qint64 m_lastQueryTime;
    QString m_lastNativeError;

    // 连接池支持
    static int s_connectionCounter;
    static QMutex s_counterMutex;
};

} // namespace Connx