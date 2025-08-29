#include "Logger.h"

void Logger::writeLog(LogLevel level, const QString& module, const QString& message) 
{
    QMutexLocker locker(&mutex);
    
    if (logFilePath.isEmpty()) {
        QString currentDir = QDir::currentPath();
        logFilePath = currentDir + "/ScreenCapture.log";
    }
    
    QFile logFile(logFilePath);
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        QTextStream stream(&logFile);
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
        QString levelStr = getLevelString(level);
        stream << "[" << timestamp << "] " << levelStr << " [" << module << "] " << message << Qt::endl;
        logFile.close();
    }
}

QString Logger::getLevelString(LogLevel level) 
{
    switch (level) {
        case DEBUG_LEVEL: return "DEBUG";
        case INFO_LEVEL: return "INFO ";
        case WARNING_LEVEL: return "WARN ";
        case ERROR_LEVEL: return "ERROR";
        default: return "UNKN ";
    }
}
