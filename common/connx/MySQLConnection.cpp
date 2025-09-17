#include "MySQLConnection.h"
#include <QDateTime>
#include <QJsonObject>
#include <QDebug>
#include <QThread>
#include <QCoreApplication>

namespace Connx {

// 静态成员初始化
int MySQLConnection::s_connectionCounter = 0;
QMutex MySQLConnection::s_counterMutex;

MySQLConnection::MySQLConnection(const ConnectionConfig& config, QObject* parent)
    : Connection(config, parent)
    , m_inTransaction(false)
    , m_reconnectCount(0)
    , m_lastQueryTime(0)
{
    m_type = ConnectionType::MySQL;
    m_currentDatabase = config.database;

    // 生成唯一的连接ID
    {
        QMutexLocker locker(&s_counterMutex);
        m_connectionId = QString("MySQLConnection_%1_%2")
                        .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()))
                        .arg(++s_connectionCounter);
    }

#ifdef WITH_MYSQL
    m_mysql = nullptr;
    m_result = nullptr;
#endif

    // 设置默认MySQL配置
    m_mysqlConfig = MySQLConnectionConfig();

    // 从额外参数中读取MySQL特有配置
    if (config.extraParams.contains("charset")) {
        m_mysqlConfig.charset = config.extraParams["charset"].toString();
    }
    if (config.extraParams.contains("autoReconnect")) {
        m_mysqlConfig.autoReconnect = config.extraParams["autoReconnect"].toBool();
    }
    if (config.extraParams.contains("maxReconnects")) {
        m_mysqlConfig.maxReconnects = config.extraParams["maxReconnects"].toInt();
    }
    if (config.extraParams.contains("useCompression")) {
        m_mysqlConfig.useCompression = config.extraParams["useCompression"].toBool();
    }
    if (config.extraParams.contains("initCommand")) {
        m_mysqlConfig.initCommand = config.extraParams["initCommand"].toString();
    }
}

MySQLConnection::~MySQLConnection()
{
    disconnectFromServer();

#ifdef WITH_MYSQL
    if (m_result) {
        mysql_free_result(m_result);
        m_result = nullptr;
    }
    if (m_mysql) {
        mysql_close(m_mysql);
        m_mysql = nullptr;
    }
#endif
}

bool MySQLConnection::connectToServer()
{
    if (isConnected()) {
        return true;
    }

    setState(ConnectionState::Connecting);

#ifdef WITH_MYSQL
    // 直接使用原生MySQL API
    return connectWithNativeAPI();
#else
    setError("MySQL native API not compiled (WITH_MYSQL not defined)");
    setState(ConnectionState::Error);
    return false;
#endif
}

#ifdef WITH_MYSQL
bool MySQLConnection::connectWithNativeAPI()
{
    m_mysql = mysql_init(nullptr);
    if (!m_mysql) {
        setError("Failed to initialize MySQL");
        setState(ConnectionState::Error);
        return false;
    }

    // 设置选项
    if (m_config.timeout > 0) {
        unsigned int timeout = static_cast<unsigned int>(m_config.timeout);
        mysql_options(m_mysql, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    }

    if (!m_mysqlConfig.charset.isEmpty()) {
        mysql_options(m_mysql, MYSQL_SET_CHARSET_NAME, m_mysqlConfig.charset.toUtf8().constData());
    }

    if (m_mysqlConfig.autoReconnect) {
        bool reconnect = true;
        mysql_options(m_mysql, MYSQL_OPT_RECONNECT, &reconnect);
    }

    if (m_mysqlConfig.useCompression) {
        mysql_options(m_mysql, MYSQL_OPT_COMPRESS, nullptr);
    }

    // 连接到服务器
    if (!mysql_real_connect(m_mysql,
                           m_config.host.toUtf8().constData(),
                           m_config.username.toUtf8().constData(),
                           m_config.password.toUtf8().constData(),
                           m_config.database.isEmpty() ? nullptr : m_config.database.toUtf8().constData(),
                           m_config.port > 0 ? m_config.port : 3306,
                           nullptr, 0)) {
        QString error = QString("Failed to connect to MySQL: %1").arg(mysql_error(m_mysql));
        setError(error);
        mysql_close(m_mysql);
        m_mysql = nullptr;
        setState(ConnectionState::Error);
        return false;
    }

    // 执行初始化命令
    if (!m_mysqlConfig.initCommand.isEmpty()) {
        if (mysql_query(m_mysql, m_mysqlConfig.initCommand.toUtf8().constData()) != 0) {
            qDebug() << "Warning: Failed to execute init command:" << mysql_error(m_mysql);
        }
    }

    // 获取服务器版本
    m_serverVersion = mysql_get_server_info(m_mysql);

    m_reconnectCount = 0;
    setState(ConnectionState::Connected);
    updateLastActivity();
    emit connected();

    return true;
}
#endif

void MySQLConnection::disconnectFromServer()
{
    if (!isConnected()) {
        return;
    }

    if (m_inTransaction) {
        rollbackTransaction();
    }

#ifdef WITH_MYSQL
    if (m_mysql) {
        mysql_close(m_mysql);
        m_mysql = nullptr;
    }
#endif

    setState(ConnectionState::Disconnected);
    emit disconnected();
}

bool MySQLConnection::isConnected() const
{
#ifdef WITH_MYSQL
    return m_mysql != nullptr && mysql_ping(const_cast<MYSQL*>(m_mysql)) == 0;
#else
    return false;
#endif
}

bool MySQLConnection::ping()
{
    if (!isConnected()) {
        return false;
    }

#ifdef WITH_MYSQL
    return mysql_ping(m_mysql) == 0;
#else
    return false;
#endif
}

QueryResult MySQLConnection::execute(const QString& command, const QVariantList& params)
{
    return query(command, params);
}

QueryResult MySQLConnection::query(const QString& sql, const QVariantList& params)
{
    QueryResult result;

    if (!isConnected()) {
        result.errorMessage = "Not connected to MySQL server";
        return result;
    }

    QMutexLocker locker(&m_queryMutex);

#ifdef WITH_MYSQL
    result = executeNativeQuery(sql, params);
#else
    result.errorMessage = "MySQL native API not compiled (WITH_MYSQL not defined)";
#endif

    updateLastActivity();
    m_lastQueryTime = QDateTime::currentMSecsSinceEpoch();

    emit queryCompleted(result);
    return result;
}

#ifdef WITH_MYSQL
QueryResult MySQLConnection::executeNativeQuery(const QString& sql, const QVariantList& params)
{
    QueryResult result;
    qint64 startTime = QDateTime::currentMSecsSinceEpoch();

    // 简单参数替换（生产环境应使用prepared statements）
    QString finalSql = sql;
    for (int i = 0; i < params.size(); ++i) {
        QString placeholder = QString("?");
        QString value = params[i].toString();
        value = value.replace("'", "\\'"); // 简单转义
        finalSql = finalSql.replace(finalSql.indexOf(placeholder), 1, QString("'%1'").arg(value));
    }

    if (mysql_query(m_mysql, finalSql.toUtf8().constData()) != 0) {
        result.errorMessage = mysql_error(m_mysql);
        result.executionTime = QDateTime::currentMSecsSinceEpoch() - startTime;
        return result;
    }

    result.success = true;
    result.executionTime = QDateTime::currentMSecsSinceEpoch() - startTime;
    result.affectedRows = mysql_affected_rows(m_mysql);

    // 获取结果集
    MYSQL_RES* res = mysql_store_result(m_mysql);
    if (res) {
        int numFields = mysql_num_fields(res);
        MYSQL_FIELD* fields = mysql_fetch_fields(res);

        // 添加列头
        QVariantList headers;
        for (int i = 0; i < numFields; ++i) {
            headers << QString::fromUtf8(fields[i].name);
        }
        result.data << headers;

        // 添加数据行
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res))) {
            QVariantList dataRow;
            for (int i = 0; i < numFields; ++i) {
                dataRow << (row[i] ? QString::fromUtf8(row[i]) : QVariant());
            }
            result.data << dataRow;
        }

        mysql_free_result(res);
    }

    return result;
}
#endif

bool MySQLConnection::beginTransaction()
{
    if (!isConnected() || m_inTransaction) {
        return false;
    }

#ifdef WITH_MYSQL
    if (mysql_query(m_mysql, "START TRANSACTION") == 0) {
        m_inTransaction = true;
        return true;
    }
    return false;
#else
    return false;
#endif
}

bool MySQLConnection::commitTransaction()
{
    if (!isConnected() || !m_inTransaction) {
        return false;
    }

#ifdef WITH_MYSQL
    if (mysql_query(m_mysql, "COMMIT") == 0) {
        m_inTransaction = false;
        return true;
    }
    return false;
#else
    return false;
#endif
}

bool MySQLConnection::rollbackTransaction()
{
    if (!isConnected() || !m_inTransaction) {
        return false;
    }

#ifdef WITH_MYSQL
    if (mysql_query(m_mysql, "ROLLBACK") == 0) {
        m_inTransaction = false;
        return true;
    }
    return false;
#else
    return false;
#endif
}

QStringList MySQLConnection::getDatabases()
{
    QStringList databases;

    QueryResult result = query("SHOW DATABASES");
    if (result.success && result.data.size() > 1) {
        for (int i = 1; i < result.data.size(); ++i) {
            QVariantList row = result.data[i].toList();
            if (!row.isEmpty()) {
                databases << row[0].toString();
            }
        }
    }

    return databases;
}

QStringList MySQLConnection::getTables(const QString& database)
{
    QStringList tables;
    QString db = database.isEmpty() ? m_currentDatabase : database;

    QString sql = db.isEmpty() ? "SHOW TABLES" : QString("SHOW TABLES FROM `%1`").arg(db);
    QueryResult result = query(sql);

    if (result.success && result.data.size() > 1) {
        for (int i = 1; i < result.data.size(); ++i) {
            QVariantList row = result.data[i].toList();
            if (!row.isEmpty()) {
                tables << row[0].toString();
            }
        }
    }

    return tables;
}

QStringList MySQLConnection::getColumns(const QString& table, const QString& database)
{
    QStringList columns;
    QString db = database.isEmpty() ? m_currentDatabase : database;

    QString sql = db.isEmpty() ?
        QString("SHOW COLUMNS FROM `%1`").arg(table) :
        QString("SHOW COLUMNS FROM `%1`.`%2`").arg(db, table);

    QueryResult result = query(sql);

    if (result.success && result.data.size() > 1) {
        for (int i = 1; i < result.data.size(); ++i) {
            QVariantList row = result.data[i].toList();
            if (!row.isEmpty()) {
                columns << row[0].toString(); // Field name
            }
        }
    }

    return columns;
}

QString MySQLConnection::escapeString(const QString& str)
{
#ifdef WITH_MYSQL
    if (m_mysql) {
        QByteArray input = str.toUtf8();
        char* escaped = new char[input.length() * 2 + 1];
        mysql_real_escape_string(m_mysql, escaped, input.constData(), input.length());
        QString result = QString::fromUtf8(escaped);
        delete[] escaped;
        return result;
    }
#endif

    // 简单的转义实现
    QString escaped = str;
    escaped.replace("\\", "\\\\");
    escaped.replace("'", "\\'");
    escaped.replace("\"", "\\\"");
    escaped.replace("\n", "\\n");
    escaped.replace("\r", "\\r");
    escaped.replace("\t", "\\t");
    return escaped;
}

QString MySQLConnection::getServerInfo()
{
    if (!m_serverVersion.isEmpty()) {
        return QString("MySQL Server %1").arg(m_serverVersion);
    }

    QueryResult result = query("SELECT VERSION()");
    if (result.success && result.data.size() > 1) {
        QVariantList row = result.data[1].toList();
        if (!row.isEmpty()) {
            m_serverVersion = row[0].toString();
            return QString("MySQL Server %1").arg(m_serverVersion);
        }
    }

    return "MySQL Server (version unknown)";
}

QJsonObject MySQLConnection::getConnectionInfo()
{
    QJsonObject info;
    info["type"] = "MySQL";
    info["host"] = m_config.host;
    info["port"] = m_config.port;
    info["username"] = m_config.username;
    info["database"] = m_currentDatabase;
    info["connected"] = isConnected();
    info["serverVersion"] = m_serverVersion;
    info["charset"] = m_mysqlConfig.charset;
    info["inTransaction"] = m_inTransaction;

    if (isConnected()) {
        info["serverInfo"] = getServerInfo();
    }

    return info;
}

bool MySQLConnection::selectDatabase(const QString& database)
{
    if (!isConnected()) {
        return false;
    }

#ifdef WITH_MYSQL
    if (mysql_select_db(m_mysql, database.toUtf8().constData()) == 0) {
        m_currentDatabase = database;
        return true;
    }
    return false;
#else
    QueryResult result = query(QString("USE `%1`").arg(database));
    if (result.success) {
        m_currentDatabase = database;
        return true;
    }
    return false;
#endif
}

void MySQLConnection::keepAlive()
{
    if (isConnected()) {
        ping();
    }
}

} // namespace Connx