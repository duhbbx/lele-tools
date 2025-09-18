#include "adminprivilegehelper.h"
#include <QProcess>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <shellapi.h>
#include <shlobj.h>

QString AdminPrivilegeHelper::lastError;

bool AdminPrivilegeHelper::isRunningAsAdmin() {
    BOOL isElevated = FALSE;
    HANDLE hToken = NULL;

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD size = sizeof(TOKEN_ELEVATION);

        if (GetTokenInformation(hToken, TokenElevation, &elevation, size, &size)) {
            isElevated = elevation.TokenIsElevated;
        }

        CloseHandle(hToken);
    }

    return isElevated;
}

bool AdminPrivilegeHelper::executeWithAdmin(const QString& command, const QStringList& arguments) {
    QString args = arguments.join(" ");

    SHELLEXECUTEINFO sei = {0};
    sei.cbSize = sizeof(SHELLEXECUTEINFO);
    sei.lpVerb = L"runas";
    sei.lpFile = reinterpret_cast<LPCWSTR>(command.utf16());
    sei.lpParameters = args.isEmpty() ? NULL : reinterpret_cast<LPCWSTR>(args.utf16());
    sei.nShow = SW_SHOWNORMAL;
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;

    if (ShellExecuteEx(&sei)) {
        WaitForSingleObject(sei.hProcess, INFINITE);

        DWORD exitCode;
        GetExitCodeProcess(sei.hProcess, &exitCode);
        CloseHandle(sei.hProcess);

        return exitCode == 0;
    }

    lastError = QString("Failed to execute with admin privileges: %1").arg(GetLastError());
    return false;
}

bool AdminPrivilegeHelper::executePowerShellAsAdmin(const QString& script) {
    // 将脚本进行Base64编码以避免引号问题
    QByteArray scriptBytes = script.toUtf8();
    QString encodedScript = QString::fromLatin1(scriptBytes.toBase64());

    QStringList args;
    args << "-NoProfile" << "-ExecutionPolicy" << "Bypass"
         << "-EncodedCommand" << encodedScript;

    return executeWithAdmin("powershell.exe", args);
}

bool AdminPrivilegeHelper::executeRegEditAsAdmin(const QString& regFile) {
    if (!QFile::exists(regFile)) {
        lastError = "Registry file does not exist: " + regFile;
        return false;
    }

    QStringList args;
    args << "/s" << regFile;  // /s for silent mode

    return executeWithAdmin("regedit.exe", args);
}

bool AdminPrivilegeHelper::restartAsAdmin() {
    QString appPath = QCoreApplication::applicationFilePath();
    QStringList args = QCoreApplication::arguments();
    args.removeFirst(); // Remove program name

    return executeWithAdmin(appPath, args);
}

bool AdminPrivilegeHelper::executeWithAdminCallback(std::function<bool()> callback) {
    if (isRunningAsAdmin()) {
        return callback();
    } else {
        // 创建临时批处理文件执行回调
        // 注意：这种方式有限制，仅适用于简单操作
        lastError = "Callback execution requires the application to be run as administrator";
        return false;
    }
}

bool AdminPrivilegeHelper::writeRegistry(HKEY hKey, const QString& subKey, const QString& valueName, const QString& data) {
    HKEY hSubKey;
    LONG result = RegCreateKeyEx(
        hKey,
        reinterpret_cast<LPCWSTR>(subKey.utf16()),
        0,
        NULL,
        REG_OPTION_NON_VOLATILE,
        KEY_WRITE,
        NULL,
        &hSubKey,
        NULL
    );

    if (result != ERROR_SUCCESS) {
        lastError = QString("Failed to create/open registry key: %1").arg(result);
        return false;
    }

    result = RegSetValueEx(
        hSubKey,
        reinterpret_cast<LPCWSTR>(valueName.utf16()),
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(data.utf16()),
        (data.length() + 1) * sizeof(wchar_t)
    );

    RegCloseKey(hSubKey);

    if (result != ERROR_SUCCESS) {
        lastError = QString("Failed to set registry value: %1").arg(result);
        return false;
    }

    return true;
}

bool AdminPrivilegeHelper::writeRegistryDWORD(HKEY hKey, const QString& subKey, const QString& valueName, DWORD data) {
    HKEY hSubKey;
    LONG result = RegCreateKeyEx(
        hKey,
        reinterpret_cast<LPCWSTR>(subKey.utf16()),
        0,
        NULL,
        REG_OPTION_NON_VOLATILE,
        KEY_WRITE,
        NULL,
        &hSubKey,
        NULL
    );

    if (result != ERROR_SUCCESS) {
        lastError = QString("Failed to create/open registry key: %1").arg(result);
        return false;
    }

    result = RegSetValueEx(
        hSubKey,
        reinterpret_cast<LPCWSTR>(valueName.utf16()),
        0,
        REG_DWORD,
        reinterpret_cast<const BYTE*>(&data),
        sizeof(DWORD)
    );

    RegCloseKey(hSubKey);

    if (result != ERROR_SUCCESS) {
        lastError = QString("Failed to set registry value: %1").arg(result);
        return false;
    }

    return true;
}

bool AdminPrivilegeHelper::deleteRegistryKey(HKEY hKey, const QString& subKey) {
    LONG result = RegDeleteTree(hKey, reinterpret_cast<LPCWSTR>(subKey.utf16()));

    if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
        lastError = QString("Failed to delete registry key: %1").arg(result);
        return false;
    }

    return true;
}

bool AdminPrivilegeHelper::deleteRegistryValue(HKEY hKey, const QString& subKey, const QString& valueName) {
    HKEY hSubKey;
    LONG result = RegOpenKeyEx(
        hKey,
        reinterpret_cast<LPCWSTR>(subKey.utf16()),
        0,
        KEY_WRITE,
        &hSubKey
    );

    if (result != ERROR_SUCCESS) {
        lastError = QString("Failed to open registry key: %1").arg(result);
        return false;
    }

    result = RegDeleteValue(hSubKey, reinterpret_cast<LPCWSTR>(valueName.utf16()));
    RegCloseKey(hSubKey);

    if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
        lastError = QString("Failed to delete registry value: %1").arg(result);
        return false;
    }

    return true;
}

bool AdminPrivilegeHelper::setSystemEnvironmentVariable(const QString& name, const QString& value) {
    bool result = writeRegistry(
        HKEY_LOCAL_MACHINE,
        "SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment",
        name,
        value
    );

    if (result) {
        // 通知系统环境变量已更改
        SendMessageTimeout(
            HWND_BROADCAST,
            WM_SETTINGCHANGE,
            0,
            (LPARAM)L"Environment",
            SMTO_ABORTIFHUNG,
            5000,
            NULL
        );
    }

    return result;
}

QString AdminPrivilegeHelper::getLastErrorMessage() {
    return lastError;
}

// ContextMenuManager 实现

bool ContextMenuManager::addFileContextMenu(const QString& menuName, const QString& command, const QString& icon) {
    QString regPath = QString("*\\shell\\%1").arg(menuName);
    QString cmdPath = QString("*\\shell\\%1\\command").arg(menuName);

    if (!AdminPrivilegeHelper::writeRegistry(HKEY_CLASSES_ROOT, regPath, "", menuName)) {
        return false;
    }

    if (!icon.isEmpty()) {
        AdminPrivilegeHelper::writeRegistry(HKEY_CLASSES_ROOT, regPath, "Icon", icon);
    }

    return AdminPrivilegeHelper::writeRegistry(HKEY_CLASSES_ROOT, cmdPath, "", command);
}

bool ContextMenuManager::addFolderContextMenu(const QString& menuName, const QString& command, const QString& icon) {
    QString regPath = QString("Folder\\shell\\%1").arg(menuName);
    QString cmdPath = QString("Folder\\shell\\%1\\command").arg(menuName);

    if (!AdminPrivilegeHelper::writeRegistry(HKEY_CLASSES_ROOT, regPath, "", menuName)) {
        return false;
    }

    if (!icon.isEmpty()) {
        AdminPrivilegeHelper::writeRegistry(HKEY_CLASSES_ROOT, regPath, "Icon", icon);
    }

    return AdminPrivilegeHelper::writeRegistry(HKEY_CLASSES_ROOT, cmdPath, "", command);
}

bool ContextMenuManager::addFolderBackgroundContextMenu(const QString& menuName, const QString& command, const QString& icon) {
    QString regPath = QString("Directory\\Background\\shell\\%1").arg(menuName);
    QString cmdPath = QString("Directory\\Background\\shell\\%1\\command").arg(menuName);

    if (!AdminPrivilegeHelper::writeRegistry(HKEY_CLASSES_ROOT, regPath, "", menuName)) {
        return false;
    }

    if (!icon.isEmpty()) {
        AdminPrivilegeHelper::writeRegistry(HKEY_CLASSES_ROOT, regPath, "Icon", icon);
    }

    return AdminPrivilegeHelper::writeRegistry(HKEY_CLASSES_ROOT, cmdPath, "", command);
}

bool ContextMenuManager::addAllFileTypesContextMenu(const QString& menuName, const QString& command, const QString& icon) {
    return addFileContextMenu(menuName, command, icon);
}

bool ContextMenuManager::removeContextMenu(const QString& menuName, ContextMenuType type) {
    QString regPath = getRegistryPath(type) + "\\" + menuName;
    return AdminPrivilegeHelper::deleteRegistryKey(HKEY_CLASSES_ROOT, regPath);
}

bool ContextMenuManager::setAlwaysShowFullContextMenu(bool enable) {
    // Windows 11 特性：禁用"显示更多选项"
    if (enable) {
        // 创建注册表项以显示完整菜单
        return AdminPrivilegeHelper::writeRegistryDWORD(
            HKEY_CURRENT_USER,
            "Software\\Classes\\CLSID\\{86ca1aa0-34aa-4e8b-a509-50c905bae2a2}\\InprocServer32",
            "",
            0
        );
    } else {
        // 删除注册表项以恢复默认行为
        return AdminPrivilegeHelper::deleteRegistryKey(
            HKEY_CURRENT_USER,
            "Software\\Classes\\CLSID\\{86ca1aa0-34aa-4e8b-a509-50c905bae2a2}"
        );
    }
}

bool ContextMenuManager::addCopyFullPathMenu() {
    // 为所有文件类型添加"复制完整路径"
    bool success = true;

    // 文件
    success &= addFileContextMenu(
        "CopyFullPath",
        "cmd.exe /c echo|set /p=\"%1\"|clip",
        "shell32.dll,-16763"
    );

    // 文件夹
    success &= addFolderContextMenu(
        "CopyFullPath",
        "cmd.exe /c echo|set /p=\"%1\"|clip",
        "shell32.dll,-16763"
    );

    // 文件夹背景
    success &= addFolderBackgroundContextMenu(
        "CopyFullPath",
        "cmd.exe /c echo|set /p=\"%V\"|clip",
        "shell32.dll,-16763"
    );

    // 设置显示名称
    AdminPrivilegeHelper::writeRegistry(
        HKEY_CLASSES_ROOT,
        "*\\shell\\CopyFullPath",
        "MUIVerb",
        "复制完整路径"
    );

    AdminPrivilegeHelper::writeRegistry(
        HKEY_CLASSES_ROOT,
        "Folder\\shell\\CopyFullPath",
        "MUIVerb",
        "复制完整路径"
    );

    AdminPrivilegeHelper::writeRegistry(
        HKEY_CLASSES_ROOT,
        "Directory\\Background\\shell\\CopyFullPath",
        "MUIVerb",
        "复制当前目录路径"
    );

    return success;
}

bool ContextMenuManager::addOpenTerminalHereMenu() {
    // 添加"在此处打开终端"菜单
    QString command = "wt.exe -d \"%V\"";  // Windows Terminal

    bool success = addFolderContextMenu("OpenTerminalHere", command, "wt.exe");
    success &= addFolderBackgroundContextMenu("OpenTerminalHere", command, "wt.exe");

    // 设置显示名称
    AdminPrivilegeHelper::writeRegistry(
        HKEY_CLASSES_ROOT,
        "Folder\\shell\\OpenTerminalHere",
        "MUIVerb",
        "在此处打开终端"
    );

    AdminPrivilegeHelper::writeRegistry(
        HKEY_CLASSES_ROOT,
        "Directory\\Background\\shell\\OpenTerminalHere",
        "MUIVerb",
        "在此处打开终端"
    );

    return success;
}

bool ContextMenuManager::addRunAsAdminMenu() {
    // 为批处理和PowerShell脚本添加"以管理员身份运行"
    QString regPath = "batfile\\shell\\runas";
    AdminPrivilegeHelper::writeRegistry(HKEY_CLASSES_ROOT, regPath, "", "以管理员身份运行");
    AdminPrivilegeHelper::writeRegistry(HKEY_CLASSES_ROOT, regPath, "HasLUAShield", "");

    QString cmdPath = "batfile\\shell\\runas\\command";
    AdminPrivilegeHelper::writeRegistry(HKEY_CLASSES_ROOT, cmdPath, "", "cmd.exe /C \"%1\" %*");

    // PowerShell脚本
    regPath = "Microsoft.PowerShellScript.1\\shell\\runas";
    AdminPrivilegeHelper::writeRegistry(HKEY_CLASSES_ROOT, regPath, "", "以管理员身份运行");
    AdminPrivilegeHelper::writeRegistry(HKEY_CLASSES_ROOT, regPath, "HasLUAShield", "");

    cmdPath = "Microsoft.PowerShellScript.1\\shell\\runas\\command";
    return AdminPrivilegeHelper::writeRegistry(
        HKEY_CLASSES_ROOT,
        cmdPath,
        "",
        "powershell.exe \"-Command\" \"Start-Process powershell -ArgumentList '-ExecutionPolicy Bypass -File \\\"%1\\\"' -Verb RunAs\""
    );
}

bool ContextMenuManager::isMenuExists(const QString& menuName, ContextMenuType type) {
    QString regPath = getRegistryPath(type) + "\\" + menuName;

    HKEY hKey;
    LONG result = RegOpenKeyEx(
        HKEY_CLASSES_ROOT,
        reinterpret_cast<LPCWSTR>(regPath.utf16()),
        0,
        KEY_READ,
        &hKey
    );

    if (result == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return true;
    }

    return false;
}

QString ContextMenuManager::getRegistryPath(ContextMenuType type) {
    switch (type) {
        case FileMenu:
            return "*\\shell";
        case FolderMenu:
            return "Folder\\shell";
        case FolderBackgroundMenu:
            return "Directory\\Background\\shell";
        case AllFileTypesMenu:
            return "*\\shell";
        default:
            return "";
    }
}