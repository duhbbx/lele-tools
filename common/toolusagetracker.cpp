#include "toolusagetracker.h"
#include <QUuid>
#include <QVariantMap>
#include <QVariantList>

std::unique_ptr<ToolUsageTracker> ToolUsageTracker::m_instance = nullptr;
QMutex ToolUsageTracker::m_mutex;

ToolUsageTracker::ToolUsageTracker()
{
    m_sqliteManager = SqliteWrapper::SqliteManager::getDefaultInstance();
    initializeDatabase();
}

ToolUsageTracker::~ToolUsageTracker()
{
    // SqliteManager 由静态实例管理，无需手动释放
}

ToolUsageTracker* ToolUsageTracker::instance()
{
    QMutexLocker locker(&m_mutex);
    if (!m_instance) {
        m_instance = std::unique_ptr<ToolUsageTracker>(new ToolUsageTracker());
    }
    return m_instance.get();
}

void ToolUsageTracker::initializeDatabase()
{
    if (!m_sqliteManager->isOpen()) {
        qWarning() << "Failed to access SQLite database manager";
        return;
    }

    createTables();
}

void ToolUsageTracker::createTables()
{
    // 使用现有的SqliteManager创建表
    QString createTableSql = R"(
        CREATE TABLE IF NOT EXISTS tool_usage_records (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            tool_name VARCHAR(100) NOT NULL,
            tool_display_name VARCHAR(200),
            usage_timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            session_id VARCHAR(50),
            duration_seconds INTEGER DEFAULT 0,
            action_type VARCHAR(50) DEFAULT 'open',
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";

    auto result = m_sqliteManager->execute(createTableSql);
    if (!result.success) {
        qWarning() << "Failed to create tool_usage_records table:" << result.errorMessage;
        return;
    }

    // 创建索引
    m_sqliteManager->execute("CREATE INDEX IF NOT EXISTS idx_tool_usage_tool_name ON tool_usage_records(tool_name)");
    m_sqliteManager->execute("CREATE INDEX IF NOT EXISTS idx_tool_usage_timestamp ON tool_usage_records(usage_timestamp)");
    m_sqliteManager->execute("CREATE INDEX IF NOT EXISTS idx_tool_usage_recent ON tool_usage_records(tool_name, usage_timestamp)");

    qDebug() << "Tool usage tracking table initialized successfully";
}

void ToolUsageTracker::recordUsage(const QString& toolName,
                                 const QString& toolDisplayName,
                                 ActionType actionType,
                                 const QString& sessionId,
                                 int durationSeconds)
{
    if (!m_sqliteManager->isOpen()) {
        return;
    }

    QMutexLocker locker(&m_mutex);

    QVariantMap data;
    data["tool_name"] = toolName;
    data["tool_display_name"] = toolDisplayName.isEmpty() ? toolName : toolDisplayName;
    data["usage_timestamp"] = QDateTime::currentDateTime();
    data["session_id"] = sessionId.isEmpty() ? QUuid::createUuid().toString() : sessionId;
    data["duration_seconds"] = durationSeconds;
    data["action_type"] = actionTypeToString(actionType);

    auto result = m_sqliteManager->insert("tool_usage_records", data);
    if (!result.success) {
        qWarning() << "Failed to record tool usage:" << result.errorMessage;
    } else {
        qDebug() << "Recorded usage for tool:" << toolName << "action:" << actionTypeToString(actionType);
    }
}

QList<ToolUsageStats> ToolUsageTracker::getWeeklyUsageStats()
{
    QList<ToolUsageStats> stats;

    if (!m_sqliteManager->isOpen()) {
        return stats;
    }

    QString sql = R"(
        SELECT
            tool_name,
            tool_display_name,
            COUNT(*) as usage_count,
            MAX(usage_timestamp) as last_used,
            COUNT(DISTINCT DATE(usage_timestamp)) as active_days
        FROM tool_usage_records
        WHERE usage_timestamp >= datetime('now', '-7 days')
        GROUP BY tool_name, tool_display_name
        ORDER BY usage_count DESC, last_used DESC
    )";

    auto result = m_sqliteManager->select(sql);
    if (!result.success) {
        qWarning() << "Failed to get weekly usage stats:" << result.errorMessage;
        return stats;
    }

    for (const auto& item : result.data) {
        if (item.canConvert<QVariantMap>()) {
            QVariantMap record = item.toMap();
            ToolUsageStats stat;
            stat.toolName = record["tool_name"].toString();
            stat.toolDisplayName = record["tool_display_name"].toString();
            stat.usageCount = record["usage_count"].toInt();
            stat.lastUsed = record["last_used"].toDateTime();
            stat.activeDays = record["active_days"].toInt();
            stats.append(stat);
        }
    }

    return stats;
}

QList<ToolUsageStats> ToolUsageTracker::getAllTimeUsageStats()
{
    QList<ToolUsageStats> stats;

    if (!m_sqliteManager->isOpen()) {
        return stats;
    }

    QString sql = R"(
        SELECT
            tool_name,
            tool_display_name,
            COUNT(*) as usage_count,
            MAX(usage_timestamp) as last_used,
            COUNT(DISTINCT DATE(usage_timestamp)) as active_days
        FROM tool_usage_records
        GROUP BY tool_name, tool_display_name
        ORDER BY usage_count DESC, last_used DESC
    )";

    auto result = m_sqliteManager->select(sql);
    if (!result.success) {
        qWarning() << "Failed to get all time usage stats:" << result.errorMessage;
        return stats;
    }

    for (const auto& item : result.data) {
        if (item.canConvert<QVariantMap>()) {
            QVariantMap record = item.toMap();
            ToolUsageStats stat;
            stat.toolName = record["tool_name"].toString();
            stat.toolDisplayName = record["tool_display_name"].toString();
            stat.usageCount = record["usage_count"].toInt();
            stat.lastUsed = record["last_used"].toDateTime();
            stat.activeDays = record["active_days"].toInt();
            stats.append(stat);
        }
    }

    return stats;
}

int ToolUsageTracker::getWeeklyUsageCount(const QString& toolName)
{
    if (!m_sqliteManager->isOpen()) {
        return 0;
    }

    QString sql = R"(
        SELECT COUNT(*) as usage_count
        FROM tool_usage_records
        WHERE tool_name = :tool_name AND usage_timestamp >= datetime('now', '-7 days')
    )";

    QVariantMap params;
    params["tool_name"] = toolName;

    auto result = m_sqliteManager->select(sql, params);
    if (result.success && !result.data.isEmpty()) {
        QVariantMap record = result.data.first().toMap();
        return record["usage_count"].toInt();
    }

    return 0;
}

void ToolUsageTracker::cleanupOldRecords()
{
    if (!m_sqliteManager->isOpen()) {
        return;
    }

    QString sql = R"(
        DELETE FROM tool_usage_records
        WHERE usage_timestamp < datetime('now', '-90 days')
    )";

    auto result = m_sqliteManager->execute(sql);
    if (result.success) {
        if (result.affectedRows > 0) {
            qDebug() << "Cleaned up" << result.affectedRows << "old usage records";
        }
    } else {
        qWarning() << "Failed to cleanup old records:" << result.errorMessage;
    }
}

QString ToolUsageTracker::actionTypeToString(ActionType type)
{
    switch (type) {
        case Open: return "open";
        case Close: return "close";
        case Action: return "action";
        default: return "unknown";
    }
}