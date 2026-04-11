#ifndef LOGGER_H
#define LOGGER_H

#include <QString>
#include <QFile>
#include <QMutex>

class Logger {
public:
    static Logger& instance();

    // 初始化日志系统，安装 Qt 消息处理器
    void init(const QString& logDir = QString());

    // 获取日志文件路径
    QString logFilePath() const;

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    static void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg);

    QFile m_logFile;
    QMutex m_mutex;
    bool m_initialized = false;
};

#endif // LOGGER_H
