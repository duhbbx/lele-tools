#include "logger.h"
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QTextStream>
#include <QMutexLocker>
#include <iostream>

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

Logger::~Logger() {
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
}

void Logger::init(const QString& logDir) {
    if (m_initialized) return;

    // 确定日志目录
    QString dir = logDir;
    if (dir.isEmpty()) {
        dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";
    }
    QDir().mkpath(dir);

    // 日志文件名：lele-tools_2026-04-11.log
    QString date = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    QString filePath = dir + "/lele-tools_" + date + ".log";

    m_logFile.setFileName(filePath);
    if (!m_logFile.open(QIODevice::Append | QIODevice::Text)) {
        std::cerr << "Failed to open log file: " << filePath.toStdString() << std::endl;
        return;
    }

    // 清理 7 天前的旧日志
    QDir logDir_(dir);
    for (const QFileInfo& fi : logDir_.entryInfoList(QStringList() << "lele-tools_*.log", QDir::Files)) {
        if (fi.lastModified().daysTo(QDateTime::currentDateTime()) > 7) {
            QFile::remove(fi.absoluteFilePath());
        }
    }

    m_initialized = true;

    // 安装 Qt 消息处理器
    qInstallMessageHandler(messageHandler);

    // 写启��分隔线
    QTextStream stream(&m_logFile);
    stream << "\n========== " << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")
           << " Lele Tools Started ==========\n";
    stream.flush();
}

QString Logger::logFilePath() const {
    return m_logFile.fileName();
}

void Logger::messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg) {
    Logger& logger = instance();
    QMutexLocker locker(&logger.m_mutex);

    QString level;
    switch (type) {
    case QtDebugMsg:    level = "DEBUG"; break;
    case QtInfoMsg:     level = "INFO "; break;
    case QtWarningMsg:  level = "WARN "; break;
    case QtCriticalMsg: level = "ERROR"; break;
    case QtFatalMsg:    level = "FATAL"; break;
    }

    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    QString file = context.file ? QFileInfo(context.file).fileName() : "";
    int line = context.line;

    QString logLine;
    if (!file.isEmpty() && line > 0) {
        logLine = QString("[%1] %2 %3:%4 - %5").arg(timestamp, level, file).arg(line).arg(msg);
    } else {
        logLine = QString("[%1] %2 - %3").arg(timestamp, level, msg);
    }

    // 写到文件
    if (logger.m_logFile.isOpen()) {
        QTextStream stream(&logger.m_logFile);
        stream << logLine << "\n";
        stream.flush();
    }

    // 同时输出到 stderr（开发时可见）
    std::cerr << logLine.toStdString() << std::endl;
}
