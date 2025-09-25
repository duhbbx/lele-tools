#ifndef HTTPREQUESTSTORE_H
#define HTTPREQUESTSTORE_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QVariantMap>
#include <QVariantList>
#include <QJsonObject>
#include <QJsonDocument>
#include "../../common/sqlite/TableManager.h"
#include "../../common/sqlite/SqliteManager.h"

namespace HttpClientStore {

// HTTP请求分组实体
struct HttpRequestGroup {
    int id;
    QString name;
    QDateTime createdAt;
    QDateTime updatedAt;

    HttpRequestGroup() : id(0) {}

    HttpRequestGroup(const QString& groupName)
        : id(0), name(groupName) {
        createdAt = QDateTime::currentDateTime();
        updatedAt = createdAt;
    }

    bool isValid() const {
        return !name.isEmpty();
    }
};

// HTTP请求实体
struct HttpRequestEntity {
    int id;
    int groupId;
    QString name;
    QString method;
    QString protocol;
    QString host;
    int port;
    QString path;
    QString curlCommand;
    QString parameters;     // JSON格式存储参数
    QString headers;        // JSON格式存储请求头
    QString cookies;        // JSON格式存储cookies
    QString body;
    QString bodyType;
    QDateTime createdAt;
    QDateTime updatedAt;

    HttpRequestEntity() : id(0), groupId(0), port(80) {}

    HttpRequestEntity(int gId, const QString& reqName)
        : id(0), groupId(gId), name(reqName), port(80) {
        createdAt = QDateTime::currentDateTime();
        updatedAt = createdAt;
    }

    bool isValid() const {
        return !name.isEmpty() && groupId > 0;
    }

    // JSON序列化辅助方法
    static QString parametersToJson(const QList<QVariantMap>& params);
    static QList<QVariantMap> parametersFromJson(const QString& json);
    static QString headersToJson(const QList<QVariantMap>& headers);
    static QList<QVariantMap> headersFromJson(const QString& json);
    static QString cookiesToJson(const QList<QVariantMap>& cookies);
    static QList<QVariantMap> cookiesFromJson(const QString& json);
};

// HTTP请求分组表管理器
class HttpRequestGroupManager : public SqliteWrapper::BaseTableManager {
    Q_OBJECT

public:
    explicit HttpRequestGroupManager(QObject* parent = nullptr);
    ~HttpRequestGroupManager();

    // 分组管理操作
    bool saveGroup(const HttpRequestGroup& group);
    HttpRequestGroup getGroupById(int id);
    HttpRequestGroup getGroupByName(const QString& name);
    QList<HttpRequestGroup> getAllGroups();
    bool deleteGroup(int id);
    bool updateGroup(int id, const HttpRequestGroup& group);
    bool renameGroup(int id, const QString& newName);

    // 查询操作
    bool groupExists(const QString& name);
    int getGroupCount();

    // 实体转换
    static HttpRequestGroup fromVariantMap(const QVariantMap& map);
    static QVariantMap toVariantMap(const HttpRequestGroup& group);
    static QList<HttpRequestGroup> fromVariantList(const QVariantList& list);

signals:
    void groupSaved(const HttpRequestGroup& group);
    void groupDeleted(int id);
    void groupUpdated(int id, const HttpRequestGroup& group);

private:
    void initializeTable();

    static const QString TABLE_NAME;
};

// HTTP请求表管理器
class HttpRequestManager : public SqliteWrapper::BaseTableManager {
    Q_OBJECT

public:
    explicit HttpRequestManager(QObject* parent = nullptr);
    ~HttpRequestManager();

    // 请求管理操作
    bool saveRequest(const HttpRequestEntity& request);
    HttpRequestEntity getRequestById(int id);
    QList<HttpRequestEntity> getRequestsByGroupId(int groupId);
    QList<HttpRequestEntity> getAllRequests();
    bool deleteRequest(int id);
    bool deleteRequestsByGroupId(int groupId);
    bool updateRequest(int id, const HttpRequestEntity& request);
    bool renameRequest(int id, const QString& newName);

    // 查询操作
    bool requestExists(const QString& name, int groupId);
    int getRequestCount();
    int getRequestCountByGroup(int groupId);

    // 实体转换
    static HttpRequestEntity fromVariantMap(const QVariantMap& map);
    static QVariantMap toVariantMap(const HttpRequestEntity& request);
    static QList<HttpRequestEntity> fromVariantList(const QVariantList& list);

signals:
    void requestSaved(const HttpRequestEntity& request);
    void requestDeleted(int id);
    void requestUpdated(int id, const HttpRequestEntity& request);

private:
    void initializeTable();

    static const QString TABLE_NAME;
};

} // namespace HttpClientStore

// 让Qt的元对象系统知道这些类型
Q_DECLARE_METATYPE(HttpClientStore::HttpRequestGroup)
Q_DECLARE_METATYPE(HttpClientStore::HttpRequestEntity)

#endif // HTTPREQUESTSTORE_H