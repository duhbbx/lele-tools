#include "SqliteManager.h"

// Qt SQL模块头文件
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QSqlDriver>

#include <QDebug>
#include <QDateTime>
#include <QApplication>
#include <QUuid>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QTextStream>
#include <QStringConverter>

namespace SqliteWrapper {

// 静态成员初始化
std::unique_ptr<SqliteManager> SqliteManager::s_defaultInstance = nullptr;
QMutex SqliteManager::s_instanceMutex;
const QString ConnectionConfigManager::TABLE_NAME = "connection_configs";

// SqliteManager 实现
SqliteManager::SqliteManager(const QString& dbPath, QObject* parent)
    : QObject(parent), m_connectionName(generateConnectionName()) {
    
    if (!dbPath.isEmpty()) {
        openDatabase(dbPath);
    }
}

SqliteManager::~SqliteManager() {
    closeDatabase();
}

bool SqliteManager::openDatabase(const QString& dbPath) {
    
    QMutexLocker locker(&m_mutex);
    
    if (isOpen()) {
        closeDatabase();
    }

    const QString actualPath = dbPath.isEmpty() ? getDefaultDatabasePath() : dbPath;
    
    // 确保目录存在
    const QFileInfo fileInfo(actualPath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            emit errorOccurred(QString("无法创建数据库目录: %1").arg(dir.absolutePath()));
            return false;
        }
    }
    
    // 创建数据库连接
    m_database = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    m_database.setDatabaseName(actualPath);
    
    if (!m_database.open()) {
        QString error = QString("无法打开数据库: %1").arg(m_database.lastError().text());
        emit errorOccurred(error);
        return false;
    }
    
    m_databasePath = actualPath;
    
    // 设置SQLite优化参数
    QSqlQuery pragmaQuery(m_database);
    pragmaQuery.exec("PRAGMA foreign_keys = ON");
    pragmaQuery.exec("PRAGMA journal_mode = WAL");
    pragmaQuery.exec("PRAGMA synchronous = NORMAL");
    pragmaQuery.exec("PRAGMA temp_store = MEMORY");
    pragmaQuery.exec("PRAGMA mmap_size = 268435456"); // 256MB
    
    emit databaseOpened(actualPath);
    qDebug() << "SQLite database opened:" << actualPath;
    
    return true;
}

void SqliteManager::closeDatabase() {
    QMutexLocker locker(&m_mutex);
    
    if (m_database.isOpen()) {
        m_database.close();
        emit databaseClosed();
        qDebug() << "SQLite database closed:" << m_databasePath;
    }
    
    QSqlDatabase::removeDatabase(m_connectionName);
    m_databasePath.clear();
}

bool SqliteManager::isOpen() const {
    return m_database.isOpen() && m_database.isValid();
}

QString SqliteManager::getDatabasePath() const {
    return m_databasePath;
}

QueryResult SqliteManager::execute(const QString& sql, const QVariantMap& params) {
    QMutexLocker locker(&m_mutex);
    QueryResult result;
    
    if (!isOpen()) {
        result.errorMessage = "数据库未打开";
        return result;
    }
    
    QSqlQuery query = prepareQuery(sql, params);
    
    if (!query.exec()) {
        result.errorMessage = query.lastError().text();
        emit errorOccurred(result.errorMessage);
        return result;
    }
    
    result = processQueryResult(query);
    emit queryExecuted(result);
    
    return result;
}

QueryResult SqliteManager::select(const QString& sql, const QVariantMap& params) {
    return execute(sql, params);
}

QueryResult SqliteManager::insert(const QString& table, const QVariantMap& data) {
    if (data.isEmpty()) {
        QueryResult result;
        result.errorMessage = "插入数据不能为空";
        return result;
    }
    
    QString sql = buildInsertSql(table, data);
    return execute(sql, data);
}

QueryResult SqliteManager::update(const QString& table, const QVariantMap& data, const QString& where, const QVariantMap& whereParams) {
    if (data.isEmpty()) {
        QueryResult result;
        result.errorMessage = "更新数据不能为空";
        return result;
    }
    
    QString sql = buildUpdateSql(table, data, where);
    
    QVariantMap allParams = data;
    for (auto it = whereParams.begin(); it != whereParams.end(); ++it) {
        allParams[it.key()] = it.value();
    }
    
    return execute(sql, allParams);
}

QueryResult SqliteManager::remove(const QString& table, const QString& where, const QVariantMap& whereParams) {
    QString sql = buildDeleteSql(table, where);
    return execute(sql, whereParams);
}

bool SqliteManager::createTable(const QString& tableName, const QVariantMap& columns) {
    if (columns.isEmpty()) {
        return false;
    }
    
    QString sql = QString("CREATE TABLE IF NOT EXISTS %1 (").arg(tableName);
    QStringList columnDefs;
    
    for (auto it = columns.begin(); it != columns.end(); ++it) {
        columnDefs << QString("%1 %2").arg(it.key(), it.value().toString());
    }
    
    sql += columnDefs.join(", ") + ")";
    
    QueryResult result = execute(sql);
    return result.success;
}

bool SqliteManager::dropTable(const QString& tableName) {
    QString sql = QString("DROP TABLE IF EXISTS %1").arg(tableName);
    QueryResult result = execute(sql);
    return result.success;
}

bool SqliteManager::tableExists(const QString& tableName) {
    QString sql = "SELECT name FROM sqlite_master WHERE type='table' AND name=:tableName";
    QVariantMap params;
    params["tableName"] = tableName;
    
    QueryResult result = select(sql, params);
    return result.success && !result.data.isEmpty();
}

QStringList SqliteManager::getTables() {
    QStringList tables;
    QString sql = "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name";
    
    QueryResult result = select(sql);
    if (result.success) {
        for (const QVariant& row : result.data) {
            QVariantMap rowMap = row.toMap();
            tables.append(rowMap["name"].toString());
        }
    }
    
    return tables;
}

QStringList SqliteManager::getColumns(const QString& tableName) {
    QStringList columns;
    QString sql = QString("PRAGMA table_info(%1)").arg(tableName);
    
    QueryResult result = execute(sql);
    if (result.success) {
        for (const QVariant& row : result.data) {
            QVariantMap rowMap = row.toMap();
            columns.append(rowMap["name"].toString());
        }
    }
    
    return columns;
}

bool SqliteManager::beginTransaction() {
    if (!isOpen()) {
        return false;
    }
    
    return m_database.transaction();
}

bool SqliteManager::commitTransaction() {
    if (!isOpen()) {
        return false;
    }
    
    return m_database.commit();
}

bool SqliteManager::rollbackTransaction() {
    if (!isOpen()) {
        return false;
    }
    
    return m_database.rollback();
}

QString SqliteManager::escapeString(const QString& str) {
    QString escaped = str;
    escaped.replace("'", "''");
    return escaped;
}

QString SqliteManager::getLastError() const {
    return m_database.lastError().text();
}

qint64 SqliteManager::getLastInsertId() const {
    QSqlQuery query("SELECT last_insert_rowid()", m_database);
    if (query.exec() && query.next()) {
        return query.value(0).toLongLong();
    }
    return 0;
}

QString SqliteManager::getDefaultDatabasePath() {
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appDataPath);
    return QDir(appDataPath).filePath("lele-tools.db");
}

bool SqliteManager::createDefaultDatabase() {
    QString dbPath = getDefaultDatabasePath();
    SqliteManager manager;
    return manager.openDatabase(dbPath);
}

SqliteManager* SqliteManager::getDefaultInstance() {
    QMutexLocker locker(&s_instanceMutex);
    
    if (!s_defaultInstance) {
        s_defaultInstance = std::make_unique<SqliteManager>(getDefaultDatabasePath());
    }
    
    return s_defaultInstance.get();
}

// 私有方法实现
QString SqliteManager::buildInsertSql(const QString& table, const QVariantMap& data) {
    QStringList columns;
    QStringList placeholders;
    
    for (auto it = data.begin(); it != data.end(); ++it) {
        columns << it.key();
        placeholders << QString(":%1").arg(it.key());
    }
    
    return QString("INSERT INTO %1 (%2) VALUES (%3)")
           .arg(table, columns.join(", "), placeholders.join(", "));
}

QString SqliteManager::buildUpdateSql(const QString& table, const QVariantMap& data, const QString& where) {
    QStringList setPairs;
    
    for (auto it = data.begin(); it != data.end(); ++it) {
        setPairs << QString("%1 = :%1").arg(it.key());
    }
    
    QString sql = QString("UPDATE %1 SET %2").arg(table, setPairs.join(", "));
    
    if (!where.isEmpty()) {
        sql += " WHERE " + where;
    }
    
    return sql;
}

QString SqliteManager::buildDeleteSql(const QString& table, const QString& where) {
    QString sql = QString("DELETE FROM %1").arg(table);
    
    if (!where.isEmpty()) {
        sql += " WHERE " + where;
    }
    
    return sql;
}

QSqlQuery SqliteManager::prepareQuery(const QString& sql, const QVariantMap& params) {
    QSqlQuery query(m_database);
    query.prepare(sql);
    
    bindParameters(query, params);
    
    return query;
}

QueryResult SqliteManager::processQueryResult(QSqlQuery& query) {
    QueryResult result;
    result.success = true;
    result.affectedRows = query.numRowsAffected();
    result.lastInsertId = query.lastInsertId().toLongLong();
    
    // 如果是SELECT查询，获取结果数据
    if (query.isSelect()) {
        while (query.next()) {
            QVariantMap row;
            QSqlRecord record = query.record();
            
            for (int i = 0; i < record.count(); ++i) {
                row[record.fieldName(i)] = record.value(i);
            }
            
            result.data.append(row);
        }
    }
    
    return result;
}

void SqliteManager::bindParameters(QSqlQuery& query, const QVariantMap& params) {
    for (auto it = params.begin(); it != params.end(); ++it) {
        query.bindValue(":" + it.key(), it.value());
    }
}

QString SqliteManager::generateConnectionName() {
    return QString("sqlite_conn_%1_%2")
           .arg(QDateTime::currentMSecsSinceEpoch())
           .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
}

// ConnectionConfigManager 实现
ConnectionConfigManager::ConnectionConfigManager(QObject* parent)
    : QObject(parent) {
    
    m_sqliteManager = SqliteManager::getDefaultInstance();
    initializeDatabase();
}

ConnectionConfigManager::~ConnectionConfigManager() {
    // 不需要删除m_sqliteManager，因为它是单例
}

bool ConnectionConfigManager::initializeDatabase() {
    if (!m_sqliteManager->isOpen()) {
        return false;
    }
    
    return createConfigTable();
}

bool ConnectionConfigManager::createConfigTable() {
    QVariantMap columns;
    columns["id"] = "INTEGER PRIMARY KEY AUTOINCREMENT";
    columns["name"] = "TEXT UNIQUE NOT NULL";
    columns["type"] = "TEXT NOT NULL";
    columns["host"] = "TEXT";
    columns["port"] = "INTEGER";
    columns["username"] = "TEXT";
    columns["password"] = "TEXT";
    columns["database_name"] = "TEXT";
    columns["timeout"] = "INTEGER DEFAULT 30";
    columns["use_ssl"] = "BOOLEAN DEFAULT 0";
    columns["extra_params"] = "TEXT"; // JSON格式存储额外参数
    columns["created_at"] = "DATETIME DEFAULT CURRENT_TIMESTAMP";
    columns["updated_at"] = "DATETIME DEFAULT CURRENT_TIMESTAMP";
    
    return m_sqliteManager->createTable(TABLE_NAME, columns);
}

bool ConnectionConfigManager::saveConnectionConfig(const QString& name, const QVariantMap& config) {
    if (name.isEmpty() || config.isEmpty()) {
        return false;
    }
    
    QVariantMap dbData;
    dbData["name"] = name;
    dbData["type"] = config.value("type", "Redis");
    dbData["host"] = config.value("host", "localhost");
    dbData["port"] = config.value("port", 6379);
    dbData["username"] = config.value("username", "");
    dbData["password"] = config.value("password", "");
    dbData["database_name"] = config.value("database", "0");
    dbData["timeout"] = config.value("timeout", 30);
    dbData["use_ssl"] = config.value("useSSL", false);
    
    // 额外参数转为JSON
    QVariantMap extraParams = config.value("extraParams").toMap();
    if (!extraParams.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromVariant(extraParams);
        dbData["extra_params"] = doc.toJson(QJsonDocument::Compact);
    }
    
    dbData["updated_at"] = QDateTime::currentDateTime();
    
    QueryResult result;
    
    if (configExists(name)) {
        // 更新现有配置
        result = m_sqliteManager->update(TABLE_NAME, dbData, "name = :name", {{"name", name}});
        if (result.success) {
            emit configUpdated(name);
        }
    } else {
        // 插入新配置
        result = m_sqliteManager->insert(TABLE_NAME, dbData);
        if (result.success) {
            emit configSaved(name);
        }
    }
    
    return result.success;
}

QVariantMap ConnectionConfigManager::getConnectionConfig(const QString& name) {
    QVariantMap config;
    
    if (name.isEmpty()) {
        return config;
    }

    const QString sql = "SELECT * FROM " + TABLE_NAME + " WHERE name = :name";
    QVariantMap params;
    params["name"] = name;
    
    QueryResult result = m_sqliteManager->select(sql, params);
    
    if (result.success && !result.data.isEmpty()) {
        QVariantMap dbData = result.data.first().toMap();
        
        // 转换为连接配置格式
        config["name"] = dbData["name"];
        config["type"] = dbData["type"];
        config["host"] = dbData["host"];
        config["port"] = dbData["port"];
        config["username"] = dbData["username"];
        config["password"] = dbData["password"];
        config["database"] = dbData["database_name"];
        config["timeout"] = dbData["timeout"];
        config["useSSL"] = dbData["use_ssl"];
        
        // 解析额外参数
        QString extraParamsJson = dbData["extra_params"].toString();
        if (!extraParamsJson.isEmpty()) {
            QJsonDocument doc = QJsonDocument::fromJson(extraParamsJson.toUtf8());
            config["extraParams"] = doc.toVariant();
        }
        
        config["createdAt"] = dbData["created_at"];
        config["updatedAt"] = dbData["updated_at"];
    }
    
    return config;
}

QVariantList ConnectionConfigManager::getAllConnectionConfigs() {
    QVariantList configs;
    
    QString sql = "SELECT * FROM " + TABLE_NAME + " ORDER BY created_at DESC";
    QueryResult result = m_sqliteManager->select(sql);
    
    if (result.success) {
        for (const QVariant& row : result.data) {
            QVariantMap dbData = row.toMap();
            
            QVariantMap config;
            config["name"] = dbData["name"];
            config["type"] = dbData["type"];
            config["host"] = dbData["host"];
            config["port"] = dbData["port"];
            config["username"] = dbData["username"];
            config["password"] = dbData["password"];
            config["database"] = dbData["database_name"];
            config["timeout"] = dbData["timeout"];
            config["useSSL"] = dbData["use_ssl"];
            
            // 解析额外参数
            QString extraParamsJson = dbData["extra_params"].toString();
            if (!extraParamsJson.isEmpty()) {
                QJsonDocument doc = QJsonDocument::fromJson(extraParamsJson.toUtf8());
                config["extraParams"] = doc.toVariant();
            }
            
            config["createdAt"] = dbData["created_at"];
            config["updatedAt"] = dbData["updated_at"];
            
            configs.append(config);
        }
    }
    
    return configs;
}

bool ConnectionConfigManager::deleteConnectionConfig(const QString& name) {
    if (name.isEmpty()) {
        return false;
    }
    
    QueryResult result = m_sqliteManager->remove(TABLE_NAME, "name = :name", {{"name", name}});
    
    if (result.success) {
        emit configDeleted(name);
    }
    
    return result.success;
}

bool ConnectionConfigManager::updateConnectionConfig(const QString& name, const QVariantMap& config) {
    return saveConnectionConfig(name, config);
}

bool ConnectionConfigManager::configExists(const QString& name) {
    QString sql = "SELECT COUNT(*) as count FROM " + TABLE_NAME + " WHERE name = :name";
    QVariantMap params;
    params["name"] = name;
    
    QueryResult result = m_sqliteManager->select(sql, params);
    
    if (result.success && !result.data.isEmpty()) {
        QVariantMap row = result.data.first().toMap();
        return row["count"].toInt() > 0;
    }
    
    return false;
}

QStringList ConnectionConfigManager::getConnectionNames() {
    QStringList names;
    
    QString sql = "SELECT name FROM " + TABLE_NAME + " ORDER BY name";
    QueryResult result = m_sqliteManager->select(sql);
    
    if (result.success) {
        for (const QVariant& row : result.data) {
            QVariantMap rowMap = row.toMap();
            names.append(rowMap["name"].toString());
        }
    }
    
    return names;
}

int ConnectionConfigManager::getConnectionCount() {
    QString sql = "SELECT COUNT(*) as count FROM " + TABLE_NAME;
    QueryResult result = m_sqliteManager->select(sql);
    
    if (result.success && !result.data.isEmpty()) {
        QVariantMap row = result.data.first().toMap();
        return row["count"].toInt();
    }
    
    return 0;
}

bool ConnectionConfigManager::saveMultipleConfigs(const QVariantList& configs) {
    if (!m_sqliteManager->beginTransaction()) {
        return false;
    }
    
    bool allSuccess = true;
    
    for (const QVariant& configVariant : configs) {
        QVariantMap config = configVariant.toMap();
        QString name = config.value("name").toString();
        
        if (!saveConnectionConfig(name, config)) {
            allSuccess = false;
            break;
        }
    }
    
    if (allSuccess) {
        return m_sqliteManager->commitTransaction();
    } else {
        m_sqliteManager->rollbackTransaction();
        return false;
    }
}

bool ConnectionConfigManager::clearAllConfigs() {
    QueryResult result = m_sqliteManager->execute("DELETE FROM " + TABLE_NAME);
    return result.success;
}

bool ConnectionConfigManager::exportConfigs(const QString& filePath) {
    QVariantList configs = getAllConnectionConfigs();
    
    // 移除敏感信息
    for (QVariant& configVariant : configs) {
        QVariantMap config = configVariant.toMap();
        config.remove("password"); // 导出时不包含密码
        configVariant = config;
    }
    
    QJsonDocument doc = QJsonDocument::fromVariant(configs);
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << doc.toJson();
    
    return true;
}

bool ConnectionConfigManager::importConfigs(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    const QByteArray data = file.readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    
    if (!doc.isArray()) {
        return false;
    }
    
    QVariantList configs = doc.toVariant().toList();
    int importedCount = 0;
    
    for (const QVariant& configVariant : configs) {
        QVariantMap config = configVariant.toMap();
        QString name = config.value("name").toString();
        
        if (!name.isEmpty() && saveConnectionConfig(name, config)) {
            importedCount++;
        }
    }
    
    if (importedCount > 0) {
        emit configsImported(importedCount);
    }
    
    return importedCount > 0;
}

} // namespace SqliteWrapper
