#include "TableManager.h"
#include <QDebug>

namespace SqliteWrapper {

// BaseTableManager 实现
BaseTableManager::BaseTableManager(const QString& tableName, QObject* parent)
    : QObject(parent), m_tableName(tableName), m_primaryKeyField("id") {
    
    m_sqliteManager = SqliteManager::getDefaultInstance();
}

BaseTableManager::~BaseTableManager() {
    // SqliteManager是单例，不需要删除
}

bool BaseTableManager::insert(const QVariantMap& data) {
    QueryResult result = m_sqliteManager->insert(m_tableName, data);
    
    if (result.success) {
        emit dataInserted(data);
    } else {
        emit errorOccurred(result.errorMessage);
    }
    
    return result.success;
}

QVariantMap BaseTableManager::selectById(const QVariant& id) {
    QString sql = QString("SELECT * FROM %1 WHERE %2 = :%2").arg(m_tableName, m_primaryKeyField);
    QVariantMap params;
    params[m_primaryKeyField] = id;
    
    QueryResult result = m_sqliteManager->select(sql, params);
    
    if (result.success && !result.data.isEmpty()) {
        return result.data.first().toMap();
    }
    
    return QVariantMap();
}

QVariantList BaseTableManager::selectAll() {
    QString sql = QString("SELECT * FROM %1 ORDER BY %2").arg(m_tableName, m_primaryKeyField);
    QueryResult result = m_sqliteManager->select(sql);
    
    return result.success ? result.data : QVariantList();
}

QVariantList BaseTableManager::selectWhere(const QString& where, const QVariantMap& params) {
    QString sql = QString("SELECT * FROM %1").arg(m_tableName);
    
    if (!where.isEmpty()) {
        sql += " WHERE " + where;
    }
    
    QueryResult result = m_sqliteManager->select(sql, params);
    return result.success ? result.data : QVariantList();
}

bool BaseTableManager::updateById(const QVariant& id, const QVariantMap& data) {
    QString where = QString("%1 = :%1").arg(m_primaryKeyField);
    QVariantMap whereParams;
    whereParams[m_primaryKeyField] = id;
    
    QueryResult result = m_sqliteManager->update(m_tableName, data, where, whereParams);
    
    if (result.success) {
        emit dataUpdated(id, data);
    } else {
        emit errorOccurred(result.errorMessage);
    }
    
    return result.success;
}

bool BaseTableManager::deleteById(const QVariant& id) {
    QString where = QString("%1 = :%1").arg(m_primaryKeyField);
    QVariantMap params;
    params[m_primaryKeyField] = id;
    
    QueryResult result = m_sqliteManager->remove(m_tableName, where, params);
    
    if (result.success) {
        emit dataDeleted(id);
    } else {
        emit errorOccurred(result.errorMessage);
    }
    
    return result.success;
}

bool BaseTableManager::deleteWhere(const QString& where, const QVariantMap& params) {
    QueryResult result = m_sqliteManager->remove(m_tableName, where, params);
    
    if (!result.success) {
        emit errorOccurred(result.errorMessage);
    }
    
    return result.success;
}

bool BaseTableManager::updateWhere(const QVariantMap& data, const QString& where, const QVariantMap& whereParams) {
    QueryResult result = m_sqliteManager->update(m_tableName, data, where, whereParams);
    
    if (!result.success) {
        emit errorOccurred(result.errorMessage);
    }
    
    return result.success;
}

QVariantList BaseTableManager::findBy(const QString& field, const QVariant& value) {
    QString where = QString("%1 = :%1").arg(field);
    QVariantMap params;
    params[field] = value;
    
    return selectWhere(where, params);
}

QVariantMap BaseTableManager::findFirst(const QString& where, const QVariantMap& params) {
    QVariantList results = selectWhere(where, params);
    return results.isEmpty() ? QVariantMap() : results.first().toMap();
}

QVariantList BaseTableManager::findWhere(const QString& where, const QVariantMap& params) {
    return selectWhere(where, params);
}

int BaseTableManager::count() {
    QString sql = QString("SELECT COUNT(*) as count FROM %1").arg(m_tableName);
    QueryResult result = m_sqliteManager->select(sql);
    
    if (result.success && !result.data.isEmpty()) {
        QVariantMap row = result.data.first().toMap();
        return row["count"].toInt();
    }
    
    return 0;
}

bool BaseTableManager::exists(const QVariant& id) {
    QString sql = QString("SELECT COUNT(*) as count FROM %1 WHERE %2 = :%2")
                  .arg(m_tableName, m_primaryKeyField);
    QVariantMap params;
    params[m_primaryKeyField] = id;
    
    QueryResult result = m_sqliteManager->select(sql, params);
    
    if (result.success && !result.data.isEmpty()) {
        QVariantMap row = result.data.first().toMap();
        return row["count"].toInt() > 0;
    }
    
    return false;
}

bool BaseTableManager::existsWhere(const QString& where, const QVariantMap& params) {
    QString sql = QString("SELECT COUNT(*) as count FROM %1").arg(m_tableName);
    
    if (!where.isEmpty()) {
        sql += " WHERE " + where;
    }
    
    QueryResult result = m_sqliteManager->select(sql, params);
    
    if (result.success && !result.data.isEmpty()) {
        QVariantMap row = result.data.first().toMap();
        return row["count"].toInt() > 0;
    }
    
    return false;
}

bool BaseTableManager::insertBatch(const QVariantList& dataList) {
    if (dataList.isEmpty()) {
        return true;
    }
    
    if (!m_sqliteManager->beginTransaction()) {
        emit errorOccurred("无法开始事务");
        return false;
    }
    
    bool allSuccess = true;
    
    for (const QVariant& dataVariant : dataList) {
        QVariantMap data = dataVariant.toMap();
        if (!insert(data)) {
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

bool BaseTableManager::deleteAll() {
    QueryResult result = m_sqliteManager->execute(QString("DELETE FROM %1").arg(m_tableName));
    
    if (!result.success) {
        emit errorOccurred(result.errorMessage);
    }
    
    return result.success;
}

bool BaseTableManager::createTable(const QVariantMap& schema) {
    return m_sqliteManager->createTable(m_tableName, schema);
}

bool BaseTableManager::dropTable() {
    return m_sqliteManager->dropTable(m_tableName);
}

bool BaseTableManager::tableExists() {
    return m_sqliteManager->tableExists(m_tableName);
}

QStringList BaseTableManager::getColumns() {
    return m_sqliteManager->getColumns(m_tableName);
}

} // namespace SqliteWrapper
