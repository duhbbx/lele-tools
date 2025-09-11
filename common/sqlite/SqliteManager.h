#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <QVariantList>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QMutex>
#include <QStandardPaths>
#include <QDir>
#include <memory>

namespace SqliteWrapper {

// 查询结果结构
struct QueryResult {
    bool success;
    QString errorMessage;
    QVariantList data;
    int affectedRows;
    qint64 lastInsertId;
    
    QueryResult() : success(false), affectedRows(0), lastInsertId(0) {}
};

// SQLite数据库管理器
class SqliteManager final : public QObject {
    Q_OBJECT

public:
    explicit SqliteManager(const QString& dbPath = "", QObject* parent = nullptr);
    ~SqliteManager() override;

    // 数据库连接管理
    bool openDatabase(const QString& dbPath = "");
    void closeDatabase();
    bool isOpen() const;
    QString getDatabasePath() const;
    
    // 基本查询操作
    QueryResult execute(const QString& sql, const QVariantMap& params = QVariantMap());
    QueryResult select(const QString& sql, const QVariantMap& params = QVariantMap());
    QueryResult insert(const QString& table, const QVariantMap& data);
    QueryResult update(const QString& table, const QVariantMap& data, const QString& where, const QVariantMap& whereParams = QVariantMap());
    QueryResult remove(const QString& table, const QString& where, const QVariantMap& whereParams = QVariantMap());
    
    // 表操作
    bool createTable(const QString& tableName, const QVariantMap& columns);
    bool dropTable(const QString& tableName);
    bool tableExists(const QString& tableName);
    QStringList getTables();
    QStringList getColumns(const QString& tableName);
    
    // 事务操作
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();
    
    // 工具方法
    QString escapeString(const QString& str);
    QString getLastError() const;
    qint64 getLastInsertId() const;
    
    // 静态工具方法
    static QString getDefaultDatabasePath();
    static bool createDefaultDatabase();
    static SqliteManager* getDefaultInstance();

signals:
    void databaseOpened(const QString& path);
    void databaseClosed();
    void queryExecuted(const SqliteWrapper::QueryResult& result);
    void errorOccurred(const QString& error);

private:
    QString buildInsertSql(const QString& table, const QVariantMap& data);
    QString buildUpdateSql(const QString& table, const QVariantMap& data, const QString& where);
    QString buildDeleteSql(const QString& table, const QString& where);
    QSqlQuery prepareQuery(const QString& sql, const QVariantMap& params = QVariantMap());
    QueryResult processQueryResult(QSqlQuery& query);
    void bindParameters(QSqlQuery& query, const QVariantMap& params);
    QString generateConnectionName();

    QSqlDatabase m_database;
    QString m_connectionName;
    QString m_databasePath;
    QMutex m_mutex;
    
    static std::unique_ptr<SqliteManager> s_defaultInstance;
    static QMutex s_instanceMutex;
};

// 数据库连接配置管理器
class ConnectionConfigManager : public QObject {
    Q_OBJECT

public:
    explicit ConnectionConfigManager(QObject* parent = nullptr);

    ~ConnectionConfigManager() override;

    // 连接配置CRUD操作
    bool saveConnectionConfig(const QString& name, const QVariantMap& config);
    QVariantMap getConnectionConfig(const QString& name);
    QVariantList getAllConnectionConfigs();
    bool deleteConnectionConfig(const QString& name);
    bool updateConnectionConfig(const QString& name, const QVariantMap& config);
    
    // 配置检查
    bool configExists(const QString& name);
    QStringList getConnectionNames();
    int getConnectionCount();
    
    // 批量操作
    bool saveMultipleConfigs(const QVariantList& configs);
    bool clearAllConfigs();
    
    // 导入导出
    bool exportConfigs(const QString& filePath);
    bool importConfigs(const QString& filePath);

signals:
    void configSaved(const QString& name);
    void configDeleted(const QString& name);
    void configUpdated(const QString& name);
    void configsImported(int count);

private:
    bool initializeDatabase();
    bool createConfigTable();
    QVariantMap recordToVariantMap(const QSqlRecord& record);
    
    SqliteManager* m_sqliteManager;
    static const QString TABLE_NAME;
};

} // namespace SqliteWrapper



