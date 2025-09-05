#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QMap>
#include <QList>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>
#include <QSslConfiguration>

namespace Connx {

// 连接状态枚举
enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Error,
    Timeout
};

// 连接类型枚举
enum class ConnectionType {
    Redis,
    MySQL,
    PostgreSQL,
    MongoDB,
    SQLite,
    Unknown
};

// 查询结果结构
struct QueryResult {
    bool success;
    QString errorMessage;
    QVariantList data;
    int affectedRows;
    qint64 executionTime; // 执行时间(毫秒)
    
    QueryResult() : success(false), affectedRows(0), executionTime(0) {}
};

// 连接配置结构
struct ConnectionConfig {
    QString name;           // 连接名称
    QString host;           // 主机地址
    int port;              // 端口号
    QString username;       // 用户名
    QString password;       // 密码
    QString database;       // 数据库名
    int timeout;           // 超时时间(秒)
    bool useSSL;           // 是否使用SSL
    QSslConfiguration sslConfig; // SSL配置
    QMap<QString, QVariant> extraParams; // 额外参数
    
    ConnectionConfig() : port(0), timeout(30), useSSL(false) {}
};

// 抽象连接基类
class Connection : public QObject {
    Q_OBJECT

public:
    explicit Connection(const ConnectionConfig& config, QObject* parent = nullptr);
    virtual ~Connection();

    // 基本连接操作
    virtual bool connectToServer() = 0;
    virtual void disconnectFromServer() = 0;
    virtual bool isConnected() const = 0;
    virtual bool ping() = 0;
    
    // 查询操作
    virtual QueryResult execute(const QString& command, const QVariantList& params = QVariantList()) = 0;
    virtual QueryResult query(const QString& sql, const QVariantList& params = QVariantList()) = 0;
    
    // 事务操作
    virtual bool beginTransaction() = 0;
    virtual bool commitTransaction() = 0;
    virtual bool rollbackTransaction() = 0;
    
    // 元数据操作
    virtual QStringList getDatabases() = 0;
    virtual QStringList getTables(const QString& database = "") = 0;
    virtual QStringList getColumns(const QString& table, const QString& database = "") = 0;
    
    // 配置和状态
    ConnectionConfig getConfig() const { return m_config; }
    ConnectionState getState() const { return m_state; }
    ConnectionType getType() const { return m_type; }
    QString getLastError() const { return m_lastError; }
    
    // 工具方法
    virtual QString escapeString(const QString& str) = 0;
    virtual QString getServerInfo() = 0;
    virtual QJsonObject getConnectionInfo() = 0;

signals:
    void stateChanged(ConnectionState state);
    void connected();
    void disconnected();
    void errorOccurred(const QString& error);
    void queryCompleted(const QueryResult& result);

protected:
    void setState(ConnectionState state);
    void setError(const QString& error);
    void updateLastActivity();
    
    ConnectionConfig m_config;
    ConnectionState m_state;
    ConnectionType m_type;
    QString m_lastError;
    QTimer* m_keepAliveTimer;
    qint64 m_lastActivity;

protected slots:
    virtual void keepAlive();
};

// 连接工厂类
class ConnectionFactory {
public:
    static Connection* createConnection(ConnectionType type, const ConnectionConfig& config);
    static QStringList getSupportedTypes();
    static QString getTypeName(ConnectionType type);
    static ConnectionType getTypeFromString(const QString& typeName);
    static ConnectionConfig getDefaultConfig(ConnectionType type);
};

} // namespace Connx
