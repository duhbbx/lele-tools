#ifndef TOOLHELP_H
#define TOOLHELP_H

#include <QHash>
#include <QString>

struct ToolHelpInfo {
    QString title;
    QString description;
    QString usage;
    QString principle;   // 实现原理：算法、库、接口
    QString notes;
    QString platforms;
};

class ToolHelp {
public:
    static const ToolHelpInfo* get(const QString& toolTitle);

private:
    static QHash<QString, ToolHelpInfo> s_helpData;
    static bool s_initialized;
    static void init();
};

#endif // TOOLHELP_H
