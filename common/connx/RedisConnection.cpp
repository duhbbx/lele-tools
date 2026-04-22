#include "RedisConnection.h"
#include <QDebug>
#include <QDateTime>
#include <QRegularExpression>

namespace Connx {

RedisConnection::RedisConnection(const ConnectionConfig& config, QObject* parent)
    : Connection(config, parent)
{
    m_type = ConnectionType::Redis;
    m_currentDatabase = config.database.toInt();
}

RedisConnection::~RedisConnection() {
    disconnectFromServer();
}

// ── RESP protocol helpers ──────────────────────────────────────────

QByteArray RedisConnection::buildRespCommand(const QStringList& args) {
    QByteArray cmd;
    cmd.append("*" + QByteArray::number(args.size()) + "\r\n");
    for (const QString& arg : args) {
        QByteArray bytes = arg.toUtf8();
        cmd.append("$" + QByteArray::number(bytes.size()) + "\r\n");
        cmd.append(bytes + "\r\n");
    }
    return cmd;
}

bool RedisConnection::waitForData(int msecs) {
    if (!m_socket) return false;
    if (m_socket->bytesAvailable() > 0) return true;
    return m_socket->waitForReadyRead(msecs);
}

QByteArray RedisConnection::readLine() {
    // Read until \r\n
    QByteArray line;
    while (true) {
        if (m_socket->bytesAvailable() == 0) {
            if (!m_socket->waitForReadyRead(5000))
                return QByteArray();
        }
        char c;
        if (!m_socket->getChar(&c))
            return QByteArray();
        if (c == '\r') {
            // expect \n
            if (m_socket->bytesAvailable() == 0)
                m_socket->waitForReadyRead(5000);
            char n;
            m_socket->getChar(&n); // consume \n
            return line;
        }
        line.append(c);
    }
}

QByteArray RedisConnection::readBytes(int count) {
    QByteArray data;
    while (data.size() < count) {
        if (m_socket->bytesAvailable() == 0) {
            if (!m_socket->waitForReadyRead(5000))
                return data;
        }
        data.append(m_socket->read(count - data.size()));
    }
    return data;
}

QVariant RedisConnection::readRespReply() {
    QByteArray line = readLine();
    if (line.isEmpty()) return QVariant();

    char type = line.at(0);
    QByteArray data = line.mid(1);

    switch (type) {
    case '+': // Simple String
        return QString::fromUtf8(data);
    case '-': // Error
        qDebug() << "Redis error:" << QString::fromUtf8(data);
        return QVariant();
    case ':': // Integer
        return (qlonglong)data.toLongLong();
    case '$': { // Bulk String
        int len = data.toInt();
        if (len == -1) return QVariant(); // null
        QByteArray bulk = readBytes(len + 2); // +2 for trailing \r\n
        return QString::fromUtf8(bulk.left(len));
    }
    case '*': { // Array
        int count = data.toInt();
        if (count == -1) return QVariant();
        QVariantList list;
        for (int i = 0; i < count; i++)
            list.append(readRespReply());
        return list;
    }
    default:
        return QVariant();
    }
}

QVariant RedisConnection::sendCommand(const QStringList& args) {
    QMutexLocker locker(&m_commandMutex);
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState)
        return QVariant();

    m_socket->write(buildRespCommand(args));
    m_socket->flush();
    if (!waitForData())
        return QVariant();
    return readRespReply();
}

QStringList RedisConnection::parseCommandString(const QString& command, const QVariantList& params) {
    QStringList args;

    // Split respecting quoted strings
    static const QRegularExpression re(R"(("(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*'|\S+))");
    auto it = re.globalMatch(command);
    while (it.hasNext()) {
        auto match = it.next();
        QString token = match.captured(1);
        // Remove surrounding quotes
        if ((token.startsWith('"') && token.endsWith('"')) ||
            (token.startsWith('\'') && token.endsWith('\''))) {
            token = token.mid(1, token.length() - 2);
            token.replace("\\\"", "\"");
            token.replace("\\'", "'");
            token.replace("\\\\", "\\");
        }
        args.append(token);
    }

    // Append extra params
    for (const auto& param : params) {
        args.append(param.toString());
    }

    return args;
}

// ── Connection interface ───────────────────────────────────────────

bool RedisConnection::connectToServer() {
    if (isConnected()) return true;

    setState(ConnectionState::Connecting);

    if (m_socket) {
        m_socket->deleteLater();
        m_socket = nullptr;
    }

    m_socket = new QTcpSocket(this);
    m_socket->connectToHost(m_config.host, m_config.port);

    if (!m_socket->waitForConnected(m_config.timeout * 1000)) {
        setError(QString("Redis connection timeout: %1").arg(m_socket->errorString()));
        m_connected = false;
        return false;
    }

    // AUTH if password is set
    if (!m_config.password.isEmpty()) {
        QVariant reply = sendCommand({"AUTH", m_config.password});
        if (!reply.isValid() || reply.toString() != "OK") {
            setError("Redis authentication failed");
            m_socket->disconnectFromHost();
            m_connected = false;
            return false;
        }
        m_authenticated = true;
    }

    // SELECT database if not 0
    if (m_currentDatabase != 0) {
        QVariant reply = sendCommand({"SELECT", QString::number(m_currentDatabase)});
        if (!reply.isValid() || reply.toString() != "OK") {
            setError(QString("Redis SELECT db %1 failed").arg(m_currentDatabase));
            m_socket->disconnectFromHost();
            m_connected = false;
            return false;
        }
    }

    // Test with PING
    QVariant pong = sendCommand({"PING"});
    if (!pong.isValid() || pong.toString() != "PONG") {
        setError("Redis PING failed after connect");
        m_socket->disconnectFromHost();
        m_connected = false;
        return false;
    }

    m_connected = true;
    setState(ConnectionState::Connected);
    updateLastActivity();

    // Get server version
    QVariant info = sendCommand({"INFO", "server"});
    if (info.isValid()) {
        m_serverVersion = info.toString();
    }

    qDebug() << "Redis connected successfully via native RESP protocol";
    return true;
}

void RedisConnection::disconnectFromServer() {
    if (m_socket) {
        if (m_socket->state() == QAbstractSocket::ConnectedState) {
            // Send QUIT gracefully
            m_socket->write(buildRespCommand({"QUIT"}));
            m_socket->flush();
            m_socket->waitForBytesWritten(1000);
            m_socket->disconnectFromHost();
        }
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_connected = false;
    setState(ConnectionState::Disconnected);
    qDebug() << "Redis disconnected";
}

bool RedisConnection::isConnected() const {
    return m_connected && m_socket &&
           m_socket->state() == QAbstractSocket::ConnectedState &&
           m_state == ConnectionState::Connected;
}

bool RedisConnection::ping() {
    if (!m_connected || !m_socket) return false;
    QVariant reply = sendCommand({"PING"});
    if (reply.isValid() && reply.toString() == "PONG") {
        updateLastActivity();
        return true;
    }
    return false;
}

QueryResult RedisConnection::execute(const QString& command, const QVariantList& params) {
    QueryResult result;

    if (!isConnected()) {
        result.errorMessage = "Not connected to Redis server";
        return result;
    }

    const qint64 startTime = QDateTime::currentMSecsSinceEpoch();

    QStringList args = parseCommandString(command, params);
    if (args.isEmpty()) {
        result.errorMessage = "Empty command";
        result.executionTime = QDateTime::currentMSecsSinceEpoch() - startTime;
        return result;
    }

    QVariant response = sendCommand(args);
    result.executionTime = QDateTime::currentMSecsSinceEpoch() - startTime;

    if (response.isValid()) {
        result.success = true;
        if (response.typeId() == QMetaType::QVariantList) {
            result.data = response.toList();
        } else {
            result.data.append(response);
        }
        updateLastActivity();
    } else {
        result.success = false;
        result.errorMessage = "Redis command returned no result";
    }

    return result;
}

QueryResult RedisConnection::query(const QString& sql, const QVariantList& params) {
    return execute(sql, params);
}

bool RedisConnection::beginTransaction() {
    if (!isConnected()) return false;
    QVariant reply = sendCommand({"MULTI"});
    return reply.isValid() && reply.toString() == "OK";
}

bool RedisConnection::commitTransaction() {
    if (!isConnected()) return false;
    QVariant reply = sendCommand({"EXEC"});
    return reply.isValid();
}

bool RedisConnection::rollbackTransaction() {
    if (!isConnected()) return false;
    QVariant reply = sendCommand({"DISCARD"});
    return reply.isValid() && reply.toString() == "OK";
}

QStringList RedisConnection::getDatabases() {
    QStringList databases;
    int dbCount = 16; // default

    if (isConnected()) {
        QVariant reply = sendCommand({"CONFIG", "GET", "databases"});
        if (reply.isValid() && reply.typeId() == QMetaType::QVariantList) {
            QVariantList list = reply.toList();
            if (list.size() >= 2) {
                bool ok;
                int n = list[1].toString().toInt(&ok);
                if (ok && n > 0) dbCount = n;
            }
        }
    }

    for (int i = 0; i < dbCount; ++i) {
        databases.append(QString("DB%1").arg(i));
    }
    return databases;
}

QStringList RedisConnection::getTables(const QString& database) {
    Q_UNUSED(database)
    return getKeys("*", 1000);
}

QStringList RedisConnection::getColumns(const QString& table, const QString& database) {
    Q_UNUSED(database)
    QStringList columns;

    if (!isConnected()) return columns;

    // If key is a hash, return its field names as "columns"
    QVariant typeReply = sendCommand({"TYPE", table});
    if (typeReply.isValid() && typeReply.toString() == "hash") {
        QVariant fields = sendCommand({"HKEYS", table});
        if (fields.isValid() && fields.typeId() == QMetaType::QVariantList) {
            for (const auto& f : fields.toList()) {
                columns.append(f.toString());
            }
        }
    }

    return columns;
}

QString RedisConnection::escapeString(const QString& str) {
    QString escaped = str;
    escaped.replace("\\", "\\\\");
    escaped.replace("\"", "\\\"");
    escaped.replace("\n", "\\n");
    escaped.replace("\r", "\\r");
    escaped.replace("\t", "\\t");
    return escaped;
}

QString RedisConnection::getServerInfo() {
    if (!isConnected()) return "";
    QVariant reply = sendCommand({"INFO"});
    return reply.isValid() ? reply.toString() : "";
}

QJsonObject RedisConnection::getConnectionInfo() {
    QJsonObject info;
    info["type"] = "Redis";
    info["host"] = m_config.host;
    info["port"] = m_config.port;
    info["database"] = m_currentDatabase;
    info["connected"] = isConnected();
    info["authenticated"] = m_authenticated;
    info["server_version"] = m_serverVersion;
    info["last_activity"] = QDateTime::fromMSecsSinceEpoch(m_lastActivity).toString();
    info["protocol"] = "Native RESP (QTcpSocket)";
    return info;
}

// ── Redis-specific methods ─────────────────────────────────────────

QStringList RedisConnection::getKeys(const QString& pattern, int limit) {
    QStringList keys;
    if (!isConnected()) return keys;

    QString cursor = "0";
    do {
        QVariant reply = sendCommand({"SCAN", cursor, "MATCH", pattern, "COUNT", "100"});
        if (!reply.isValid() || reply.typeId() != QMetaType::QVariantList) break;

        QVariantList arr = reply.toList();
        if (arr.size() < 2) break;

        cursor = arr[0].toString();

        if (arr[1].typeId() == QMetaType::QVariantList) {
            for (const auto& k : arr[1].toList()) {
                keys.append(k.toString());
                if (keys.size() >= limit) break;
            }
        }
    } while (cursor != "0" && keys.size() < limit);

    return keys;
}

RedisKeyInfo RedisConnection::getKeyInfo(const QString& key) {
    RedisKeyInfo info;
    info.key = key;
    if (!isConnected()) return info;

    // TYPE
    QVariant typeReply = sendCommand({"TYPE", key});
    if (typeReply.isValid()) {
        QString t = typeReply.toString();
        if (t == "string")      info.type = RedisDataType::String;
        else if (t == "hash")   info.type = RedisDataType::Hash;
        else if (t == "list")   info.type = RedisDataType::List;
        else if (t == "set")    info.type = RedisDataType::Set;
        else if (t == "zset")   info.type = RedisDataType::ZSet;
        else if (t == "stream") info.type = RedisDataType::Stream;
        else                    info.type = RedisDataType::Unknown;
    }

    // TTL
    QVariant ttlReply = sendCommand({"TTL", key});
    if (ttlReply.isValid()) {
        info.ttl = ttlReply.toLongLong();
    }

    // Size based on type
    switch (info.type) {
    case RedisDataType::String: {
        QVariant r = sendCommand({"STRLEN", key});
        if (r.isValid()) info.size = r.toLongLong();
        break;
    }
    case RedisDataType::Hash: {
        QVariant r = sendCommand({"HLEN", key});
        if (r.isValid()) info.size = r.toLongLong();
        break;
    }
    case RedisDataType::List: {
        QVariant r = sendCommand({"LLEN", key});
        if (r.isValid()) info.size = r.toLongLong();
        break;
    }
    case RedisDataType::Set: {
        QVariant r = sendCommand({"SCARD", key});
        if (r.isValid()) info.size = r.toLongLong();
        break;
    }
    case RedisDataType::ZSet: {
        QVariant r = sendCommand({"ZCARD", key});
        if (r.isValid()) info.size = r.toLongLong();
        break;
    }
    default:
        info.size = 0;
        break;
    }

    // Encoding
    QVariant encReply = sendCommand({"OBJECT", "ENCODING", key});
    if (encReply.isValid()) {
        info.encoding = encReply.toString();
    }

    return info;
}

QVariant RedisConnection::getValue(const QString& key) {
    if (!isConnected()) return QVariant();

    QVariant typeReply = sendCommand({"TYPE", key});
    if (!typeReply.isValid()) return QVariant();

    QString t = typeReply.toString();

    if (t == "string") {
        return sendCommand({"GET", key});
    } else if (t == "hash") {
        return getHashAll(key);
    } else if (t == "list") {
        return getListRange(key, 0, -1);
    } else if (t == "set") {
        QStringList members = getSetMembers(key);
        QVariantList list;
        for (const auto& m : members) list.append(m);
        return list;
    } else if (t == "zset") {
        return getZSetRange(key, 0, -1, false);
    }

    return QVariant();
}

bool RedisConnection::setValue(const QString& key, const QVariant& value, int ttl) {
    if (!isConnected()) return false;

    QStringList args;
    if (ttl > 0) {
        args = {"SET", key, value.toString(), "EX", QString::number(ttl)};
    } else {
        args = {"SET", key, value.toString()};
    }

    QVariant reply = sendCommand(args);
    if (reply.isValid() && reply.toString() == "OK") {
        updateLastActivity();
        return true;
    }
    return false;
}

bool RedisConnection::deleteKey(const QString& key) {
    if (!isConnected()) return false;
    QVariant reply = sendCommand({"DEL", key});
    if (reply.isValid()) {
        updateLastActivity();
        return reply.toLongLong() > 0;
    }
    return false;
}

QStringList RedisConnection::getHashFields(const QString& key) {
    QStringList fields;
    if (!isConnected()) return fields;

    QVariant reply = sendCommand({"HKEYS", key});
    if (reply.isValid() && reply.typeId() == QMetaType::QVariantList) {
        for (const auto& f : reply.toList()) {
            fields.append(f.toString());
        }
    }
    return fields;
}

QVariantMap RedisConnection::getHashAll(const QString& key) {
    QVariantMap result;
    if (!isConnected()) return result;

    QVariant reply = sendCommand({"HGETALL", key});
    if (reply.isValid() && reply.typeId() == QMetaType::QVariantList) {
        QVariantList list = reply.toList();
        for (int i = 0; i + 1 < list.size(); i += 2) {
            result[list[i].toString()] = list[i + 1].toString();
        }
    }
    return result;
}

QVariantList RedisConnection::getListRange(const QString& key, int start, int end) {
    QVariantList result;
    if (!isConnected()) return result;

    QVariant reply = sendCommand({"LRANGE", key, QString::number(start), QString::number(end)});
    if (reply.isValid() && reply.typeId() == QMetaType::QVariantList) {
        result = reply.toList();
    }
    return result;
}

QStringList RedisConnection::getSetMembers(const QString& key) {
    QStringList members;
    if (!isConnected()) return members;

    QVariant reply = sendCommand({"SMEMBERS", key});
    if (reply.isValid() && reply.typeId() == QMetaType::QVariantList) {
        for (const auto& m : reply.toList()) {
            members.append(m.toString());
        }
    }
    return members;
}

QVariantList RedisConnection::getZSetRange(const QString& key, int start, int end, bool withScores) {
    QVariantList result;
    if (!isConnected()) return result;

    QStringList args = {"ZRANGE", key, QString::number(start), QString::number(end)};
    if (withScores) args.append("WITHSCORES");

    QVariant reply = sendCommand(args);
    if (reply.isValid() && reply.typeId() == QMetaType::QVariantList) {
        result = reply.toList();
    }
    return result;
}

bool RedisConnection::selectDatabase(int dbIndex) {
    if (!isConnected()) return false;

    QVariant reply = sendCommand({"SELECT", QString::number(dbIndex)});
    if (reply.isValid() && reply.toString() == "OK") {
        m_currentDatabase = dbIndex;
        updateLastActivity();
        return true;
    }
    return false;
}

void RedisConnection::keepAlive() {
    if (isConnected()) {
        if (!ping()) {
            setError("Keep-alive ping failed");
        } else {
            updateLastActivity();
        }
    }
}

QMap<QString, QString> RedisConnection::getServerConfig() {
    QMap<QString, QString> config;
    if (!isConnected()) return config;

    QVariant reply = sendCommand({"INFO"});
    if (reply.isValid()) {
        config["info"] = reply.toString();
    }
    return config;
}

QMap<QString, QVariant> RedisConnection::getServerStats() const {
    QMap<QString, QVariant> stats;
    if (!m_connected || !m_socket) return stats;

    // sendCommand is not const, so use const_cast for this read-only operation
    auto* self = const_cast<RedisConnection*>(this);
    QVariant reply = self->sendCommand({"INFO"});
    if (reply.isValid()) {
        stats["info"] = reply.toString();
    }
    return stats;
}

} // namespace Connx
