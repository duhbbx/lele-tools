#include "ConnectionConfigTableManager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QTextStream>
#include <QStringConverter>
#include <QDebug>

namespace SqliteWrapper {

// 静态常量定义
const QString ConnectionConfigTableManager::TABLE_NAME = "connection_configs";
const QString ConnectionConfigTableManager::PRIMARY_KEY_FIELD = "id";

ConnectionConfigTableManager::ConnectionConfigTableManager(QObject* parent)
    : BaseTableManager(TABLE_NAME, parent) {
    
    setPrimaryKeyField(PRIMARY_KEY_FIELD);
    initializeTable();
}

ConnectionConfigTableManager::~ConnectionConfigTableManager() {
    // 基类会自动清理
}

void ConnectionConfigTableManager::initializeTable() {
    if (!tableExists()) {
        QVariantMap schema = getTableSchema();
        if (!createTable(schema)) {
            qWarning() << "Failed to create connection_configs table";
        }
    }
}

QVariantMap ConnectionConfigTableManager::getTableSchema() {
    QVariantMap schema;
    schema["id"] = "INTEGER PRIMARY KEY AUTOINCREMENT";
    schema["name"] = "TEXT UNIQUE NOT NULL";
    schema["type"] = "TEXT NOT NULL";
    schema["host"] = "TEXT";
    schema["port"] = "INTEGER";
    schema["username"] = "TEXT";
    schema["password"] = "TEXT";
    schema["database_name"] = "TEXT";
    schema["timeout"] = "INTEGER DEFAULT 30";
    schema["use_ssl"] = "BOOLEAN DEFAULT 0";
    schema["extra_params"] = "TEXT"; // JSON格式存储额外参数
    schema["created_at"] = "DATETIME DEFAULT CURRENT_TIMESTAMP";
    schema["updated_at"] = "DATETIME DEFAULT CURRENT_TIMESTAMP";
    
    return schema;
}

bool ConnectionConfigTableManager::saveConfig(const ConnectionConfigEntity& config) {
    if (!config.isValid()) {
        emit errorOccurred("连接配置无效");
        return false;
    }
    
    QVariantMap data = toVariantMap(config);
    
    // 如果配置已存在，更新；否则插入
    if (configExistsByName(config.name)) {
        return updateConfig(config.name, config);
    } else {
        bool success = insert(data);
        if (success) {
            emit configSaved(config);
        }
        return success;
    }
}

ConnectionConfigEntity ConnectionConfigTableManager::getConfigByName(const QString& name) {
    QVariantMap result = findFirst("name = :name", {{"name", name}});
    return fromVariantMap(result);
}

QList<ConnectionConfigEntity> ConnectionConfigTableManager::getAllConfigs() {
    QVariantList data = selectAll();
    return fromVariantList(data);
}

bool ConnectionConfigTableManager::deleteConfigByName(const QString& name) {
    bool success = deleteWhere("name = :name", {{"name", name}});
    if (success) {
        emit configDeleted(name);
    }
    return success;
}

bool ConnectionConfigTableManager::updateConfig(const QString& name, const ConnectionConfigEntity& config) {
    ConnectionConfigEntity updatedConfig = config;
    updatedConfig.updatedAt = QDateTime::currentDateTime();
    
    QVariantMap data = toVariantMap(updatedConfig);
    data.remove("id"); // 不更新ID
    data.remove("created_at"); // 不更新创建时间
    
    QString where = "name = :name";
    QVariantMap whereParams;
    whereParams["name"] = name;
    
    bool success = updateWhere(data, where, whereParams);
    if (success) {
        emit configUpdated(name, updatedConfig);
    }
    return success;
}

QList<ConnectionConfigEntity> ConnectionConfigTableManager::getConfigsByType(const QString& type) {
    QVariantList data = selectWhere("type = :type", {{"type", type}});
    return fromVariantList(data);
}

QList<ConnectionConfigEntity> ConnectionConfigTableManager::getRecentConfigs(int limit) {
    QString sql = QString("SELECT * FROM %1 ORDER BY updated_at DESC LIMIT %2")
                  .arg(m_tableName).arg(limit);
    
    QueryResult result = m_sqliteManager->select(sql);
    return fromVariantList(result.data);
}

bool ConnectionConfigTableManager::configExistsByName(const QString& name) {
    return existsWhere("name = :name", {{"name", name}});
}

QStringList ConnectionConfigTableManager::getAllConfigNames() {
    QString sql = QString("SELECT name FROM %1 ORDER BY name").arg(m_tableName);
    QueryResult result = m_sqliteManager->select(sql);
    
    QStringList names;
    for (const QVariant& row : result.data) {
        QVariantMap rowMap = row.toMap();
        names.append(rowMap["name"].toString());
    }
    
    return names;
}

int ConnectionConfigTableManager::getConfigCount() {
    return count();
}

bool ConnectionConfigTableManager::saveAllConfigs(const QList<ConnectionConfigEntity>& configs) {
    QVariantList dataList = toVariantList(configs);
    bool success = insertBatch(dataList);
    
    if (success) {
        for (const ConnectionConfigEntity& config : configs) {
            emit configSaved(config);
        }
    }
    
    return success;
}

bool ConnectionConfigTableManager::deleteAllConfigs() {
    return deleteAll();
}

bool ConnectionConfigTableManager::exportToJson(const QString& filePath, bool includePasswords) {
    QList<ConnectionConfigEntity> configs = getAllConfigs();
    
    QJsonArray jsonArray;
    for (const ConnectionConfigEntity& config : configs) {
        QVariantMap configMap = toVariantMap(config);
        
        // 可选择是否包含密码
        if (!includePasswords) {
            configMap.remove("password");
        }
        
        // 移除内部字段
        configMap.remove("id");
        
        QJsonObject configObj = QJsonObject::fromVariantMap(configMap);
        jsonArray.append(configObj);
    }
    
    QJsonDocument doc(jsonArray);
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit errorOccurred(QString("无法创建导出文件: %1").arg(filePath));
        return false;
    }
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << doc.toJson();
    
    emit configsExported(configs.size());
    return true;
}

bool ConnectionConfigTableManager::importFromJson(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit errorOccurred(QString("无法读取导入文件: %1").arg(filePath));
        return false;
    }
    
    const QByteArray data = file.readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    
    if (!doc.isArray()) {
        emit errorOccurred("导入文件格式错误，应为JSON数组");
        return false;
    }
    
    QJsonArray jsonArray = doc.array();
    QList<ConnectionConfigEntity> configs;
    
    for (const QJsonValue& value : jsonArray) {
        if (value.isObject()) {
            QVariantMap configMap = value.toObject().toVariantMap();
            ConnectionConfigEntity config = fromVariantMap(configMap);
            
            if (config.isValid()) {
                configs.append(config);
            }
        }
    }
    
    if (configs.isEmpty()) {
        emit errorOccurred("导入文件中没有有效的连接配置");
        return false;
    }
    
    bool success = saveAllConfigs(configs);
    if (success) {
        emit configsImported(configs.size());
    }
    
    return success;
}

// 静态转换方法
ConnectionConfigEntity ConnectionConfigTableManager::fromVariantMap(const QVariantMap& map) {
    ConnectionConfigEntity config;
    
    config.id = map.value("id", 0).toInt();
    config.name = map.value("name").toString();
    config.type = map.value("type").toString();
    config.host = map.value("host").toString();
    config.port = map.value("port").toInt();
    config.username = map.value("username").toString();
    config.password = map.value("password").toString();
    config.databaseName = map.value("database_name").toString();
    config.timeout = map.value("timeout", 30).toInt();
    config.useSSL = map.value("use_ssl", false).toBool();
    
    // 解析额外参数
    QString extraParamsJson = map.value("extra_params").toString();
    if (!extraParamsJson.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(extraParamsJson.toUtf8());
        if (doc.isObject()) {
            config.extraParams = doc.object().toVariantMap();
        }
    }
    
    config.createdAt = map.value("created_at").toDateTime();
    config.updatedAt = map.value("updated_at").toDateTime();
    
    return config;
}

QVariantMap ConnectionConfigTableManager::toVariantMap(const ConnectionConfigEntity& entity) {
    QVariantMap map;
    
    if (entity.id > 0) {
        map["id"] = entity.id;
    }
    
    map["name"] = entity.name;
    map["type"] = entity.type;
    map["host"] = entity.host;
    map["port"] = entity.port;
    map["username"] = entity.username;
    map["password"] = entity.password;
    map["database_name"] = entity.databaseName;
    map["timeout"] = entity.timeout;
    map["use_ssl"] = entity.useSSL;
    
    // 额外参数转为JSON
    if (!entity.extraParams.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromVariant(entity.extraParams);
        map["extra_params"] = doc.toJson(QJsonDocument::Compact);
    }
    
    if (entity.createdAt.isValid()) {
        map["created_at"] = entity.createdAt;
    }
    if (entity.updatedAt.isValid()) {
        map["updated_at"] = entity.updatedAt;
    }
    
    return map;
}

QList<ConnectionConfigEntity> ConnectionConfigTableManager::fromVariantList(const QVariantList& list) {
    QList<ConnectionConfigEntity> configs;
    
    for (const QVariant& variant : list) {
        QVariantMap map = variant.toMap();
        ConnectionConfigEntity config = fromVariantMap(map);
        if (config.isValid()) {
            configs.append(config);
        }
    }
    
    return configs;
}

QVariantList ConnectionConfigTableManager::toVariantList(const QList<ConnectionConfigEntity>& list) {
    QVariantList variants;
    
    for (const ConnectionConfigEntity& config : list) {
        variants.append(toVariantMap(config));
    }
    
    return variants;
}

} // namespace SqliteWrapper
