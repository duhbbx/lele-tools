#include "httprequeststore.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDebug>
#include "../../common/sqlite/SqliteManager.h"

using namespace SqliteWrapper;


namespace HttpClientStore {

// === HttpRequestEntity JSON序列化辅助方法 ===

QString HttpRequestEntity::parametersToJson(const QList<QVariantMap>& params) {
    QJsonArray array;
    for (const auto& param : params) {
        QJsonObject obj;
        obj["key"] = param.value("key").toString();
        obj["value"] = param.value("value").toString();
        obj["enabled"] = param.value("enabled").toBool();
        array.append(obj);
    }
    return QJsonDocument(array).toJson(QJsonDocument::Compact);
}

QList<QVariantMap> HttpRequestEntity::parametersFromJson(const QString& json) {
    QList<QVariantMap> result;
    if (json.isEmpty()) return result;

    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isArray()) return result;

    QJsonArray array = doc.array();
    for (const auto& value : array) {
        if (value.isObject()) {
            QJsonObject obj = value.toObject();
            QVariantMap param;
            param["key"] = obj["key"].toString();
            param["value"] = obj["value"].toString();
            param["enabled"] = obj["enabled"].toBool();
            result.append(param);
        }
    }
    return result;
}

QString HttpRequestEntity::headersToJson(const QList<QVariantMap>& headers) {
    return parametersToJson(headers); // 格式相同
}

QList<QVariantMap> HttpRequestEntity::headersFromJson(const QString& json) {
    return parametersFromJson(json); // 格式相同
}

QString HttpRequestEntity::cookiesToJson(const QList<QVariantMap>& cookies) {
    return parametersToJson(cookies); // 格式相同
}

QList<QVariantMap> HttpRequestEntity::cookiesFromJson(const QString& json) {
    return parametersFromJson(json); // 格式相同
}

QString HttpRequestEntity::userToJson(const QString& username, const QString& password, bool enabled) {
    QJsonObject obj;
    obj["username"] = username;
    obj["password"] = password;
    obj["enabled"] = enabled;
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

QVariantMap HttpRequestEntity::userFromJson(const QString& json) {
    QVariantMap result;
    result["username"] = "";
    result["password"] = "";
    result["enabled"] = false;

    if (json.isEmpty()) return result;

    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isObject()) return result;

    QJsonObject obj = doc.object();
    result["username"] = obj["username"].toString();
    result["password"] = obj["password"].toString();
    result["enabled"] = obj["enabled"].toBool();

    return result;
}

// === HttpRequestGroupManager 实现 ===

const QString HttpRequestGroupManager::TABLE_NAME = "http_request_groups";

HttpRequestGroupManager::HttpRequestGroupManager(QObject* parent)
    : BaseTableManager(TABLE_NAME, parent) {
    initializeTable();
}

HttpRequestGroupManager::~HttpRequestGroupManager() = default;

void HttpRequestGroupManager::initializeTable() {
    // 定义表结构
    QVariantMap schema;
    schema["id"] = "INTEGER PRIMARY KEY AUTOINCREMENT";
    schema["name"] = "TEXT NOT NULL UNIQUE";
    schema["created_at"] = "TEXT NOT NULL";
    schema["updated_at"] = "TEXT NOT NULL";

    // 使用BaseTableManager的createTable方法
    if (!tableExists()) {
        createTable(schema);

        // 创建索引
        QString indexSql = QString("CREATE INDEX IF NOT EXISTS idx_%1_name ON %1(name)").arg(TABLE_NAME);
        SqliteWrapper::QueryResult result = m_sqliteManager->execute(indexSql);
        if (!result.success) {
            qWarning() << "Failed to create index:" << result.errorMessage;
        }
    }
}

bool HttpRequestGroupManager::saveGroup(const HttpRequestGroup& group) {
    QVariantMap data = toVariantMap(group);
    data.remove("id"); // 让数据库自动生成ID

    bool result = insert(data);
    if (result) {
        emit groupSaved(group);
    }
    return result;
}

HttpRequestGroup HttpRequestGroupManager::getGroupById(int id) {
    QVariantMap data = selectById(id);
    if (!data.isEmpty()) {
        return fromVariantMap(data);
    }
    return HttpRequestGroup();
}

HttpRequestGroup HttpRequestGroupManager::getGroupByName(const QString& name) {
    QVariantMap params;
    params["name"] = name;
    QVariantMap data = findFirst("name = :name", params);
    if (!data.isEmpty()) {
        return fromVariantMap(data);
    }
    return HttpRequestGroup();
}

QList<HttpRequestGroup> HttpRequestGroupManager::getAllGroups() {
    QVariantList dataList = selectAll();
    return fromVariantList(dataList);
}

bool HttpRequestGroupManager::deleteGroup(int id) {
    bool result = deleteById(id);
    if (result) {
        emit groupDeleted(id);
    }
    return result;
}

bool HttpRequestGroupManager::updateGroup(int id, const HttpRequestGroup& group) {
    QVariantMap data = toVariantMap(group);
    data["updated_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    bool result = updateById(id, data);
    if (result) {
        emit groupUpdated(id, group);
    }
    return result;
}

bool HttpRequestGroupManager::renameGroup(int id, const QString& newName) {
    QVariantMap data;
    data["name"] = newName;
    data["updated_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    return updateById(id, data);
}

bool HttpRequestGroupManager::groupExists(const QString& name) {
    return !getGroupByName(name).name.isEmpty();
}

int HttpRequestGroupManager::getGroupCount() {
    return count();
}

HttpRequestGroup HttpRequestGroupManager::fromVariantMap(const QVariantMap& map) {
    HttpRequestGroup group;
    group.id = map.value("id").toInt();
    group.name = map.value("name").toString();
    group.createdAt = QDateTime::fromString(map.value("created_at").toString(), Qt::ISODate);
    group.updatedAt = QDateTime::fromString(map.value("updated_at").toString(), Qt::ISODate);
    return group;
}

QVariantMap HttpRequestGroupManager::toVariantMap(const HttpRequestGroup& group) {
    QVariantMap map;
    if (group.id > 0) map["id"] = group.id;
    map["name"] = group.name;
    map["created_at"] = group.createdAt.toString(Qt::ISODate);
    map["updated_at"] = group.updatedAt.toString(Qt::ISODate);
    return map;
}

QList<HttpRequestGroup> HttpRequestGroupManager::fromVariantList(const QVariantList& list) {
    QList<HttpRequestGroup> result;
    for (const auto& item : list) {
        if (item.canConvert<QVariantMap>()) {
            result.append(fromVariantMap(item.toMap()));
        }
    }
    return result;
}

// === HttpRequestManager 实现 ===

const QString HttpRequestManager::TABLE_NAME = "http_requests";

HttpRequestManager::HttpRequestManager(QObject* parent)
    : BaseTableManager(TABLE_NAME, parent) {
    initializeTable();
}

HttpRequestManager::~HttpRequestManager() = default;

void HttpRequestManager::initializeTable() {
    // 定义表结构
    QVariantMap schema;
    schema["id"] = "INTEGER PRIMARY KEY AUTOINCREMENT";
    schema["group_id"] = "INTEGER NOT NULL";
    schema["name"] = "TEXT NOT NULL";
    schema["method"] = "TEXT NOT NULL DEFAULT 'GET'";
    schema["protocol"] = "TEXT NOT NULL DEFAULT 'https'";
    schema["host"] = "TEXT NOT NULL DEFAULT ''";
    schema["port"] = "INTEGER NOT NULL DEFAULT 443";
    schema["path"] = "TEXT NOT NULL DEFAULT '/'";
    schema["curl_command"] = "TEXT DEFAULT ''";
    schema["parameters"] = "TEXT DEFAULT ''";
    schema["headers"] = "TEXT DEFAULT ''";
    schema["cookies"] = "TEXT DEFAULT ''";
    schema["body"] = "TEXT DEFAULT ''";
    schema["body_type"] = "TEXT NOT NULL DEFAULT 'raw'";
    schema["user"] = "TEXT DEFAULT ''";
    schema["created_at"] = "TEXT NOT NULL";
    schema["updated_at"] = "TEXT NOT NULL";

    // 使用BaseTableManager的createTable方法
    if (!tableExists()) {
        createTable(schema);

        // 创建索引和约束（需要单独执行）
        QString groupIndexSql = QString("CREATE INDEX IF NOT EXISTS idx_%1_group_id ON %1(group_id)").arg(TABLE_NAME);
        SqliteWrapper::QueryResult result1 = m_sqliteManager->execute(groupIndexSql);
        if (!result1.success) {
            qWarning() << "Failed to create group_id index:" << result1.errorMessage;
        }

        QString nameIndexSql = QString("CREATE INDEX IF NOT EXISTS idx_%1_name ON %1(name)").arg(TABLE_NAME);
        SqliteWrapper::QueryResult result2 = m_sqliteManager->execute(nameIndexSql);
        if (!result2.success) {
            qWarning() << "Failed to create name index:" << result2.errorMessage;
        }
    } else {
        // 检查是否需要添加新字段
        QStringList existingColumns = m_sqliteManager->getColumns(TABLE_NAME);

        // 检查user字段是否存在
        if (!existingColumns.contains("user", Qt::CaseInsensitive)) {
            QString addColumnSql = QString("ALTER TABLE %1 ADD COLUMN user TEXT DEFAULT ''").arg(TABLE_NAME);
            SqliteWrapper::QueryResult result = m_sqliteManager->execute(addColumnSql);
            if (!result.success) {
                qWarning() << "Failed to add user column:" << result.errorMessage;
            } else {
                qDebug() << "Successfully added user column to" << TABLE_NAME;
            }
        }
    }
}

bool HttpRequestManager::saveRequest(const HttpRequestEntity& request) {
    QVariantMap data = toVariantMap(request);
    data.remove("id"); // 让数据库自动生成ID

    bool result = insert(data);
    if (result) {
        emit requestSaved(request);
    }
    return result;
}

HttpRequestEntity HttpRequestManager::getRequestById(int id) {
    QVariantMap data = selectById(id);
    if (!data.isEmpty()) {
        return fromVariantMap(data);
    }
    return HttpRequestEntity();
}

QList<HttpRequestEntity> HttpRequestManager::getRequestsByGroupId(int groupId) {
    QVariantList dataList = findBy("group_id", groupId);
    return fromVariantList(dataList);
}

QList<HttpRequestEntity> HttpRequestManager::getAllRequests() {
    QVariantList dataList = selectAll();
    return fromVariantList(dataList);
}

bool HttpRequestManager::deleteRequest(int id) {
    bool result = deleteById(id);
    if (result) {
        emit requestDeleted(id);
    }
    return result;
}

bool HttpRequestManager::deleteRequestsByGroupId(int groupId) {
    return deleteWhere("group_id = :group_id", {{"group_id", groupId}});
}

bool HttpRequestManager::updateRequest(int id, const HttpRequestEntity& request) {
    QVariantMap data = toVariantMap(request);
    data["updated_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    bool result = updateById(id, data);
    if (result) {
        emit requestUpdated(id, request);
    }
    return result;
}

bool HttpRequestManager::renameRequest(int id, const QString& newName) {
    QVariantMap data;
    data["name"] = newName;
    data["updated_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    return updateById(id, data);
}

bool HttpRequestManager::requestExists(const QString& name, int groupId) {
    QVariantMap params;
    params["name"] = name;
    params["group_id"] = groupId;

    QVariantMap result = findFirst("name = :name AND group_id = :group_id", params);
    return !result.isEmpty();
}

int HttpRequestManager::getRequestCount() {
    return count();
}

int HttpRequestManager::getRequestCountByGroup(int groupId) {
    QVariantList results = findBy("group_id", groupId);
    return results.size();
}

HttpRequestEntity HttpRequestManager::fromVariantMap(const QVariantMap& map) {
    HttpRequestEntity request;
    request.id = map.value("id").toInt();
    request.groupId = map.value("group_id").toInt();
    request.name = map.value("name").toString();
    request.method = map.value("method").toString();
    request.protocol = map.value("protocol").toString();
    request.host = map.value("host").toString();
    request.port = map.value("port").toInt();
    request.path = map.value("path").toString();
    request.curlCommand = map.value("curl_command").toString();
    request.parameters = map.value("parameters").toString();
    request.headers = map.value("headers").toString();
    request.cookies = map.value("cookies").toString();
    request.body = map.value("body").toString();
    request.bodyType = map.value("body_type").toString();
    request.user = map.value("user").toString();
    request.createdAt = QDateTime::fromString(map.value("created_at").toString(), Qt::ISODate);
    request.updatedAt = QDateTime::fromString(map.value("updated_at").toString(), Qt::ISODate);
    return request;
}

QVariantMap HttpRequestManager::toVariantMap(const HttpRequestEntity& request) {
    QVariantMap map;
    if (request.id > 0) map["id"] = request.id;
    map["group_id"] = request.groupId;
    map["name"] = request.name;
    map["method"] = request.method;
    map["protocol"] = request.protocol;
    map["host"] = request.host;
    map["port"] = request.port;
    map["path"] = request.path;
    map["curl_command"] = request.curlCommand;
    map["parameters"] = request.parameters;
    map["headers"] = request.headers;
    map["cookies"] = request.cookies;
    map["body"] = request.body;
    map["body_type"] = request.bodyType;
    map["user"] = request.user;
    map["created_at"] = request.createdAt.toString(Qt::ISODate);
    map["updated_at"] = request.updatedAt.toString(Qt::ISODate);
    return map;
}

QList<HttpRequestEntity> HttpRequestManager::fromVariantList(const QVariantList& list) {
    QList<HttpRequestEntity> result;
    for (const auto& item : list) {
        if (item.canConvert<QVariantMap>()) {
            result.append(fromVariantMap(item.toMap()));
        }
    }
    return result;
}

} // namespace HttpClientStore