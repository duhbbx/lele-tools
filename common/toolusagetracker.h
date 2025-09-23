#ifndef TOOLUSAGETRACKER_H
#define TOOLUSAGETRACKER_H

#include <QString>
#include <QDateTime>
#include <QDebug>
#include <QMutex>
#include <QMutexLocker>
#include <memory>
#include "sqlite/SqliteManager.h"

struct ToolUsageRecord {
    QString toolName;           // 工具类名
    QString toolDisplayName;    // 工具显示名称
    QDateTime usageTimestamp;   // 使用时间戳
    QString sessionId;          // 会话ID
    int durationSeconds;        // 使用时长
    QString actionType;         // 操作类型：open, close, action
};

struct ToolUsageStats {
    QString toolName;
    QString toolDisplayName;
    int usageCount;             // 使用次数
    QDateTime lastUsed;         // 最后使用时间
    int activeDays;             // 活跃天数
};

class ToolUsageTracker
{
public:
    enum ActionType {
        Open = 0,       // 打开工具
        Close,          // 关闭工具
        Action          // 执行操作
    };

    static ToolUsageTracker* instance();

    // 为了支持 std::unique_ptr，需要公开析构函数
    ~ToolUsageTracker();

    // 记录工具使用
    void recordUsage(const QString& toolName,
                    const QString& toolDisplayName = QString(),
                    ActionType actionType = Open,
                    const QString& sessionId = QString(),
                    int durationSeconds = 0);

    // 获取最近一周的使用统计（按使用次数排序）
    QList<ToolUsageStats> getWeeklyUsageStats();

    // 获取所有时间的使用统计
    QList<ToolUsageStats> getAllTimeUsageStats();

    // 获取工具的使用次数（最近7天）
    int getWeeklyUsageCount(const QString& toolName);

    // 清理过期数据（超过90天的记录）
    void cleanupOldRecords();

private:
    ToolUsageTracker();

    void initializeDatabase();
    void createTables();
    QString actionTypeToString(ActionType type);

    static std::unique_ptr<ToolUsageTracker> m_instance;
    static QMutex m_mutex;

    SqliteWrapper::SqliteManager* m_sqliteManager;
};

#endif // TOOLUSAGETRACKER_H