#include "Connection.h"
#include "RedisConnection.h"
#include <QDateTime>
#include <QDebug>

namespace Connx {

// Connection 基类实现
Connection::Connection(const ConnectionConfig& config, QObject* parent)
    : QObject(parent), m_config(config), m_state(ConnectionState::Disconnected),
      m_type(ConnectionType::Unknown), m_lastActivity(0) {
    
    // 创建保活定时器
    m_keepAliveTimer = new QTimer(this);
    m_keepAliveTimer->setInterval(30000); // 30秒保活
    QObject::connect(m_keepAliveTimer, &QTimer::timeout, this, &Connection::keepAlive);
}

Connection::~Connection() {
    if (m_keepAliveTimer) {
        m_keepAliveTimer->stop();
    }
    // 不在析构函数中调用纯虚函数，由子类负责清理
}

void Connection::setState(ConnectionState state) {
    if (m_state != state) {
        ConnectionState oldState = m_state;
        m_state = state;
        
        qDebug() << "Connection state changed from" << (int)oldState << "to" << (int)state;
        emit stateChanged(state);
        
        switch (state) {
        case ConnectionState::Connected:
            m_keepAliveTimer->start();
            emit connected();
            break;
        case ConnectionState::Disconnected:
            m_keepAliveTimer->stop();
            emit disconnected();
            break;
        case ConnectionState::Error:
            m_keepAliveTimer->stop();
            break;
        default:
            break;
        }
    }
}

void Connection::setError(const QString& error) {
    m_lastError = error;
    qDebug() << "Connection error:" << error;
    emit errorOccurred(error);
    setState(ConnectionState::Error);
}

void Connection::updateLastActivity() {
    m_lastActivity = QDateTime::currentMSecsSinceEpoch();
}

void Connection::keepAlive() {
    // 默认空实现，由子类重写
    // 避免在基类中调用纯虚函数
}

// ConnectionFactory 实现
Connection* ConnectionFactory::createConnection(ConnectionType type, const ConnectionConfig& config) {
    switch (type) {
    case ConnectionType::Redis:
        return new RedisConnection(config);
    case ConnectionType::MySQL:
        // TODO: 实现MySQL连接
        qWarning() << "MySQL connection not implemented yet";
        return nullptr;
    case ConnectionType::PostgreSQL:
        // TODO: 实现PostgreSQL连接
        qWarning() << "PostgreSQL connection not implemented yet";
        return nullptr;
    case ConnectionType::MongoDB:
        // TODO: 实现MongoDB连接
        qWarning() << "MongoDB connection not implemented yet";
        return nullptr;
    case ConnectionType::SQLite:
        // TODO: 实现SQLite连接
        qWarning() << "SQLite connection not implemented yet";
        return nullptr;
    default:
        qWarning() << "Unknown connection type:" << (int)type;
        return nullptr;
    }
}

QStringList ConnectionFactory::getSupportedTypes() {
    return {"Redis", "MySQL", "PostgreSQL", "MongoDB", "SQLite"};
}

QString ConnectionFactory::getTypeName(ConnectionType type) {
    switch (type) {
    case ConnectionType::Redis: return "Redis";
    case ConnectionType::MySQL: return "MySQL";
    case ConnectionType::PostgreSQL: return "PostgreSQL";
    case ConnectionType::MongoDB: return "MongoDB";
    case ConnectionType::SQLite: return "SQLite";
    default: return "Unknown";
    }
}

ConnectionType ConnectionFactory::getTypeFromString(const QString& typeName) {
    if (typeName == "Redis") return ConnectionType::Redis;
    if (typeName == "MySQL") return ConnectionType::MySQL;
    if (typeName == "PostgreSQL") return ConnectionType::PostgreSQL;
    if (typeName == "MongoDB") return ConnectionType::MongoDB;
    if (typeName == "SQLite") return ConnectionType::SQLite;
    return ConnectionType::Unknown;
}

ConnectionConfig ConnectionFactory::getDefaultConfig(ConnectionType type) {
    ConnectionConfig config;
    
    switch (type) {
    case ConnectionType::Redis:
        config.host = "localhost";
        config.port = 6379;
        config.database = "0";
        config.timeout = 30;
        break;
    case ConnectionType::MySQL:
        config.host = "localhost";
        config.port = 3306;
        config.database = "mysql";
        config.timeout = 30;
        break;
    case ConnectionType::PostgreSQL:
        config.host = "localhost";
        config.port = 5432;
        config.database = "postgres";
        config.timeout = 30;
        break;
    case ConnectionType::MongoDB:
        config.host = "localhost";
        config.port = 27017;
        config.database = "admin";
        config.timeout = 30;
        break;
    case ConnectionType::SQLite:
        config.host = "";
        config.port = 0;
        config.database = "database.sqlite";
        config.timeout = 30;
        break;
    default:
        break;
    }
    
    return config;
}

} // namespace Connx
