#pragma once

#include <QString>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QMutex>
#include <QMutexLocker>

class Logger
{
public:
    enum LogLevel 
    {
        DEBUG_LEVEL = 0,
        INFO_LEVEL = 1,
        WARNING_LEVEL = 2,
        ERROR_LEVEL = 3
    };

    static Logger& instance() 
    {
        static Logger logger;
        return logger;
    }

    void writeLog(LogLevel level, const QString& module, const QString& message);

private:
    Logger() {}
    ~Logger() {}
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    QString getLevelString(LogLevel level);

    QString logFilePath;
    QMutex mutex;
};

// 便捷宏定义
#define LOG_DEBUG(module, msg) Logger::instance().writeLog(Logger::DEBUG_LEVEL, module, msg)
#define LOG_INFO(module, msg) Logger::instance().writeLog(Logger::INFO_LEVEL, module, msg)
#define LOG_WARNING(module, msg) Logger::instance().writeLog(Logger::WARNING_LEVEL, module, msg)
#define LOG_ERROR(module, msg) Logger::instance().writeLog(Logger::ERROR_LEVEL, module, msg)

// 模块名称定义
#define MODULE_APP "App"
#define MODULE_WIN "Win"
#define MODULE_TOOL "Tool"
#define MODULE_SHAPE "Shape"
#define MODULE_MAIN "Main"
