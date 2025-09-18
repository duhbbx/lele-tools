#ifndef ADMINPRIVILEGEHELPER_H
#define ADMINPRIVILEGEHELPER_H

#include <QString>
#include <QStringList>
#include <functional>
#include <Windows.h>

class AdminPrivilegeHelper {
public:
    // 检查当前进程是否有管理员权限
    static bool isRunningAsAdmin();

    // 以管理员权限执行命令
    static bool executeWithAdmin(const QString& command, const QStringList& arguments = QStringList());

    // 以管理员权限执行PowerShell脚本
    static bool executePowerShellAsAdmin(const QString& script);

    // 以管理员权限执行注册表操作
    static bool executeRegEditAsAdmin(const QString& regFile);

    // 请求管理员权限并重启程序
    static bool restartAsAdmin();

    // 使用回调函数的方式执行需要管理员权限的操作
    static bool executeWithAdminCallback(std::function<bool()> callback);

    // 使用Lambda的方式执行需要管理员权限的操作
    template<typename Lambda>
    static bool executeWithAdminLambda(Lambda lambda) {
        if (isRunningAsAdmin()) {
            return lambda();
        } else {
            // 需要提升权限
            return requestAdminAndExecute(lambda);
        }
    }

    // 写入注册表（需要管理员权限）
    static bool writeRegistry(HKEY hKey, const QString& subKey, const QString& valueName, const QString& data);
    static bool writeRegistryDWORD(HKEY hKey, const QString& subKey, const QString& valueName, DWORD data);

    // 删除注册表项（需要管理员权限）
    static bool deleteRegistryKey(HKEY hKey, const QString& subKey);
    static bool deleteRegistryValue(HKEY hKey, const QString& subKey, const QString& valueName);

    // 创建Windows任务计划（需要管理员权限）
    static bool createScheduledTask(const QString& taskName, const QString& executable, const QString& arguments = QString());

    // 修改系统环境变量（需要管理员权限）
    static bool setSystemEnvironmentVariable(const QString& name, const QString& value);

    // 注册/注销COM组件（需要管理员权限）
    static bool registerCOM(const QString& dllPath);
    static bool unregisterCOM(const QString& dllPath);

    // 错误信息获取
    static QString getLastErrorMessage();

private:
    template<typename Lambda>
    static bool requestAdminAndExecute(Lambda lambda);

    static QString lastError;
};

// Windows右键菜单管理类
class ContextMenuManager {
public:
    enum ContextMenuType {
        FileMenu,
        FolderMenu,
        FolderBackgroundMenu,
        AllFileTypesMenu
    };

    // 添加文件右键菜单项
    static bool addFileContextMenu(const QString& menuName, const QString& command, const QString& icon = QString());

    // 添加文件夹右键菜单项
    static bool addFolderContextMenu(const QString& menuName, const QString& command, const QString& icon = QString());

    // 添加文件夹背景右键菜单项（在文件夹空白处右键）
    static bool addFolderBackgroundContextMenu(const QString& menuName, const QString& command, const QString& icon = QString());

    // 添加所有文件类型的右键菜单
    static bool addAllFileTypesContextMenu(const QString& menuName, const QString& command, const QString& icon = QString());

    // 移除右键菜单项
    static bool removeContextMenu(const QString& menuName, ContextMenuType type);

    // 启用/禁用"显示更多选项"
    static bool setAlwaysShowFullContextMenu(bool enable);

    // 添加"复制完整路径"菜单项
    static bool addCopyFullPathMenu();

    // 添加"在此处打开终端"菜单项
    static bool addOpenTerminalHereMenu();

    // 添加"以管理员身份运行"菜单项（对于批处理和脚本）
    static bool addRunAsAdminMenu();

    // 检查菜单项是否存在
    static bool isMenuExists(const QString& menuName, ContextMenuType type);

private:
    static QString getRegistryPath(ContextMenuType type);
};

#endif // ADMINPRIVILEGEHELPER_H