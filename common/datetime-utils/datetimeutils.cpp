#include "datetimeutils.h"

#include <QString>
#include <QDateTime>

QString secondTimestampToDate(const QString& timestampStr) {
    // 字符串形式的时间戳
    // QString timestampStr = "1630809600"; // 示例时间戳，以秒为单位

    // 将字符串形式的时间戳转换为 long long 型
    bool ok;
    qint64 timestamp = timestampStr.toLongLong(&ok);

    if (ok) {
        // 使用 fromSecsSinceEpoch 将时间戳转换为 QDateTime 对象
        QDateTime dateTime = QDateTime::fromSecsSinceEpoch(timestamp);

        // 将 QDateTime 对象格式化为日期字符串
        QString formattedDate = dateTime.toString("yyyy-MM-dd hh:mm:ss");

        return formattedDate;

        // 输出日期字符串
        qDebug() << "日期字符串:" << formattedDate;
    } else {
        qDebug() << "无效的时间戳字符串:" << timestampStr;
    }

    return "";
}


QString dateToSecondTimestamp(const QString& dateString) {
    // 日期字符串
    // QString dateString = "2023-09-04 15:30:00"; // 示例日期字符串

    // 使用 fromString 将日期字符串解析为 QDateTime 对象
    QDateTime dateTime = QDateTime::fromString(dateString, "yyyy-MM-dd hh:mm:ss");

    if (dateTime.isValid()) {
        // 使用 toSecsSinceEpoch 将 QDateTime 转换为时间戳（秒）
        qint64 timestamp = dateTime.toSecsSinceEpoch();

        // 将时间戳转换为字符串
        QString timestampString = QString::number(timestamp);

        return timestampString;

        // 输出时间戳字符串
        qDebug() << "时间戳字符串:" << timestampString;
    } else {
        qDebug() << "无效的日期字符串:" << dateString;
    }

    return "";
}
