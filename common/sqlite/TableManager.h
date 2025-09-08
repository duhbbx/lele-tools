#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <QVariantList>
#include <QStringList>
#include "SqliteManager.h"

namespace SqliteWrapper {

// 通用表管理器基类
template<typename T>
class TableManager : public QObject {
public:
    explicit TableManager(const QString& tableName, QObject* parent = nullptr)
        : QObject(parent), m_tableName(tableName) {
        m_sqliteManager = SqliteManager::getDefaultInstance();
        initializeTable();
    }

    virtual ~TableManager() = default;

    // 基本CRUD操作
    virtual bool save(const T& entity) = 0;
    virtual T getById(const QVariant& id) = 0;
    virtual QList<T> getAll() = 0;
    virtual bool deleteById(const QVariant& id) = 0;
    virtual bool update(const QVariant& id, const T& entity) = 0;
    
    // 查询操作
    virtual QList<T> findBy(const QString& field, const QVariant& value) = 0;
    virtual T findFirst(const QString& where, const QVariantMap& params = QVariantMap()) = 0;
    virtual QList<T> findWhere(const QString& where, const QVariantMap& params = QVariantMap()) = 0;
    virtual int count() = 0;
    virtual bool exists(const QVariant& id) = 0;
    
    // 批量操作
    virtual bool saveAll(const QList<T>& entities) = 0;
    virtual bool deleteAll() = 0;
    virtual bool deleteWhere(const QString& where, const QVariantMap& params = QVariantMap()) = 0;
    
    // 表操作
    QString getTableName() const { return m_tableName; }
    bool dropTable() { return m_sqliteManager->dropTable(m_tableName); }
    bool tableExists() { return m_sqliteManager->tableExists(m_tableName); }
    QStringList getColumns() { return m_sqliteManager->getColumns(m_tableName); }

protected:
    // 子类需要实现的抽象方法
    virtual QVariantMap getTableSchema() = 0;
    virtual QVariantMap entityToMap(const T& entity) = 0;
    virtual T mapToEntity(const QVariantMap& map) = 0;
    virtual QString getPrimaryKeyField() = 0;
    
    // 辅助方法
    bool initializeTable() {
        if (!m_sqliteManager->tableExists(m_tableName)) {
            return m_sqliteManager->createTable(m_tableName, getTableSchema());
        }
        return true;
    }
    
    QueryResult executeQuery(const QString& sql, const QVariantMap& params = QVariantMap()) {
        return m_sqliteManager->execute(sql, params);
    }
    
    QueryResult selectQuery(const QString& sql, const QVariantMap& params = QVariantMap()) {
        return m_sqliteManager->select(sql, params);
    }

    SqliteManager* m_sqliteManager;
    QString m_tableName;
};

// 基础表管理器实现 (为简单的表提供默认实现)
class BaseTableManager : public QObject {
    Q_OBJECT

public:
    explicit BaseTableManager(const QString& tableName, QObject* parent = nullptr);
    virtual ~BaseTableManager();

    // 基本CRUD操作
    bool insert(const QVariantMap& data);
    QVariantMap selectById(const QVariant& id);
    QVariantList selectAll();
    QVariantList selectWhere(const QString& where, const QVariantMap& params = QVariantMap());
    bool updateById(const QVariant& id, const QVariantMap& data);
    bool updateWhere(const QVariantMap& data, const QString& where, const QVariantMap& whereParams = QVariantMap());
    bool deleteById(const QVariant& id);
    bool deleteWhere(const QString& where, const QVariantMap& params = QVariantMap());
    
    // 查询操作
    QVariantList findBy(const QString& field, const QVariant& value);
    QVariantMap findFirst(const QString& where, const QVariantMap& params = QVariantMap());
    QVariantList findWhere(const QString& where, const QVariantMap& params = QVariantMap());
    int count();
    bool exists(const QVariant& id);
    bool existsWhere(const QString& where, const QVariantMap& params = QVariantMap());
    
    // 批量操作
    bool insertBatch(const QVariantList& dataList);
    bool deleteAll();
    
    // 表管理
    bool createTable(const QVariantMap& schema);
    bool dropTable();
    bool tableExists();
    QStringList getColumns();
    
    // 设置表结构
    void setPrimaryKeyField(const QString& field) { m_primaryKeyField = field; }
    QString getPrimaryKeyField() const { return m_primaryKeyField; }
    
    QString getTableName() const { return m_tableName; }

signals:
    void dataInserted(const QVariantMap& data);
    void dataUpdated(const QVariant& id, const QVariantMap& data);
    void dataDeleted(const QVariant& id);
    void errorOccurred(const QString& error);

protected:
    SqliteManager* m_sqliteManager;
    QString m_tableName;
    QString m_primaryKeyField;
};

} // namespace SqliteWrapper
