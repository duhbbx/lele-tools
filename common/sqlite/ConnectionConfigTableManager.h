#pragma once

#include "TableManager.h"
#include <QDateTime>

namespace SqliteWrapper {

// 连接配置数据结构
struct ConnectionConfigEntity {
    int id;
    QString name;
    QString type;
    QString host;
    int port;
    QString username;
    QString password;
    QString databaseName;
    int timeout;
    bool useSSL;
    QVariantMap extraParams;
    QDateTime createdAt;
    QDateTime updatedAt;
    
    ConnectionConfigEntity() : id(0), port(0), timeout(30), useSSL(false) {}
    
    // 便捷构造函数
    ConnectionConfigEntity(const QString& name, const QString& type, const QString& host, int port)
        : id(0), name(name), type(type), host(host), port(port), timeout(30), useSSL(false) {
        createdAt = QDateTime::currentDateTime();
        updatedAt = createdAt;
    }
    
    bool isValid() const {
        return !name.isEmpty() && !type.isEmpty() && !host.isEmpty() && port > 0;
    }
};

// 连接配置表管理器
class ConnectionConfigTableManager : public BaseTableManager {
    Q_OBJECT

public:
    explicit ConnectionConfigTableManager(QObject* parent = nullptr);
    ~ConnectionConfigTableManager();

    // 连接配置特有的操作
    bool saveConfig(const ConnectionConfigEntity& config);
    ConnectionConfigEntity getConfigByName(const QString& name);
    QList<ConnectionConfigEntity> getAllConfigs();
    bool deleteConfigByName(const QString& name);
    bool updateConfig(const QString& name, const ConnectionConfigEntity& config);
    
    // 查询操作
    QList<ConnectionConfigEntity> getConfigsByType(const QString& type);
    QList<ConnectionConfigEntity> getRecentConfigs(int limit = 10);
    bool configExistsByName(const QString& name);
    QStringList getAllConfigNames();
    int getConfigCount();
    
    // 批量操作
    bool saveAllConfigs(const QList<ConnectionConfigEntity>& configs);
    bool deleteAllConfigs();
    
    // 导入导出
    bool exportToJson(const QString& filePath, bool includePasswords = false);
    bool importFromJson(const QString& filePath);
    
    // 实体转换
    static ConnectionConfigEntity fromVariantMap(const QVariantMap& map);
    static QVariantMap toVariantMap(const ConnectionConfigEntity& entity);
    static QList<ConnectionConfigEntity> fromVariantList(const QVariantList& list);
    static QVariantList toVariantList(const QList<ConnectionConfigEntity>& list);

signals:
    void configSaved(const ConnectionConfigEntity& config);
    void configDeleted(const QString& name);
    void configUpdated(const QString& name, const ConnectionConfigEntity& config);
    void configsImported(int count);
    void configsExported(int count);

private:
    void initializeTable();
    QVariantMap getTableSchema();
    
    static const QString TABLE_NAME;
    static const QString PRIMARY_KEY_FIELD;
};

} // namespace SqliteWrapper


