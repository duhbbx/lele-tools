#include "hostseditortext.h"
#include <QDebug>
#include <string>
#include <exception>
#include <QMessageBox>
#include <QScrollBar>
#include <QFont>
#include <QFontMetrics>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#include <wininet.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shell32.lib")
#else
#include <unistd.h>
#endif
#include <QProcess>

REGISTER_DYNAMICOBJECT(SimpleHostsEditor);

SimpleHostsEditor::SimpleHostsEditor() : QWidget(nullptr), DynamicObjectBase()
{
    hasUnsavedChanges = false;
    
    setupUI();
    hostsFilePath = getHostsFilePath();
    hasAdminRights = checkAdminRights();
    
    // 连接信号槽
    connect(loadBtn, &QPushButton::clicked, this, &SimpleHostsEditor::onLoadHosts);
    connect(saveBtn, &QPushButton::clicked, this, &SimpleHostsEditor::onSaveHosts);
    connect(flushDnsBtn, &QPushButton::clicked, this, &SimpleHostsEditor::onFlushDns);
    connect(resetBtn, &QPushButton::clicked, this, &SimpleHostsEditor::onResetHosts);
    connect(backupBtn, &QPushButton::clicked, this, &SimpleHostsEditor::onBackupHosts);
    connect(hostsTextEdit, &QTextEdit::textChanged, this, &SimpleHostsEditor::onTextChanged);
    
    // 设置状态更新定时器
    statusTimer = new QTimer(this);
    connect(statusTimer, &QTimer::timeout, this, &SimpleHostsEditor::updateStatus);
    statusTimer->start(1000); // 每秒更新一次状态
    
    // 自动加载hosts文件
    loadHostsFile();
}

SimpleHostsEditor::~SimpleHostsEditor() 
{
    if (statusTimer) {
        statusTimer->stop();
    }
}

void SimpleHostsEditor::setupUI()
{
    mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(4);
    mainLayout->setContentsMargins(8, 6, 8, 6);

    setupToolbar();
    setupEditorArea();
    setupStatusArea();

    setStyleSheet(R"(
        QPushButton {
            padding: 4px 10px;
            border: none;
            border-radius: 4px;
            font-size: 9pt;
            color: #495057;
            background: transparent;
        }
        QPushButton:hover {
            background-color: #e9ecef;
        }
        QPushButton:pressed {
            background-color: #dee2e6;
        }
        QPushButton:disabled {
            color: #adb5bd;
        }
        QTextEdit {
            font-family: 'Menlo', 'Consolas', 'Monaco', monospace;
            font-size: 10pt;
            border: 1px solid #dee2e6;
            border-radius: 4px;
            background-color: #fff;
            selection-background-color: #b3d7ff;
        }
        QTextEdit:focus {
            border-color: #80bdff;
        }
    )");
}

void SimpleHostsEditor::setupToolbar()
{
    toolbarLayout = new QHBoxLayout();
    toolbarLayout->setContentsMargins(0, 0, 0, 0);
    toolbarLayout->setSpacing(2);

    loadBtn = new QPushButton(tr("重新加载"));
    saveBtn = new QPushButton(tr("保存"));
    flushDnsBtn = new QPushButton(tr("刷新DNS"));
    resetBtn = new QPushButton(tr("重置"));
    backupBtn = new QPushButton(tr("备份"));

    saveBtn->setEnabled(false);

    toolbarLayout->addWidget(loadBtn);
    toolbarLayout->addWidget(saveBtn);
    toolbarLayout->addWidget(backupBtn);
    toolbarLayout->addWidget(resetBtn);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(flushDnsBtn);

    mainLayout->addLayout(toolbarLayout);
}

void SimpleHostsEditor::setupEditorArea()
{
    hostsTextEdit = new QTextEdit();
    hostsTextEdit->setPlaceholderText(tr("# 格式：IP地址  主机名  # 注释\n# 127.0.0.1    localhost"));

    QFont font("Menlo", 11);
    font.setStyleHint(QFont::Monospace);
    hostsTextEdit->setFont(font);

    mainLayout->addWidget(hostsTextEdit, 1);
}

void SimpleHostsEditor::setupStatusArea()
{
    statusLayout = new QHBoxLayout();
    statusLayout->setContentsMargins(0, 0, 0, 0);

    statusLabel = new QLabel(tr("就绪"));
    statusLabel->setStyleSheet("color: #868e96; font-size: 9pt;");

    lineCountLabel = new QLabel(tr("行数: 0"));
    lineCountLabel->setStyleSheet("color: #868e96; font-size: 9pt;");

    adminStatusLabel = new QLabel();
    adminStatusLabel->setStyleSheet("font-size: 9pt;");

    progressBar = new QProgressBar();
    progressBar->setVisible(false);
    progressBar->setMaximumHeight(16);

    statusLayout->addWidget(statusLabel);
    statusLayout->addWidget(lineCountLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(progressBar);
    statusLayout->addWidget(adminStatusLabel);

    mainLayout->addLayout(statusLayout);
}

void SimpleHostsEditor::loadHostsFile()
{
    updateStatusMessage(tr("正在加载hosts文件..."), false);
    
    QFile file(hostsFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        updateStatusMessage(tr("无法读取hosts文件: %1").arg(file.errorString()), true);
        return;
    }
    
    QTextStream in(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    in.setCodec("UTF-8");
#else
    in.setEncoding(QStringConverter::Utf8);
#endif
    
    QString content = in.readAll();
    file.close();
    
    hostsTextEdit->setPlainText(content);
    updateStatusMessage(tr("hosts文件加载完成"), false);
    hasUnsavedChanges = false;
    saveBtn->setEnabled(false);
}

void SimpleHostsEditor::onLoadHosts()
{
    if (hasUnsavedChanges) {
        QMessageBox::StandardButton reply = QMessageBox::question(this, 
            tr("未保存的更改"), 
            tr("当前有未保存的更改，重新加载将丢失这些更改。\n确定要继续吗？"),
            QMessageBox::Yes | QMessageBox::No);
            
        if (reply != QMessageBox::Yes) {
            return;
        }
    }
    
    loadHostsFile();
}

void SimpleHostsEditor::onSaveHosts()
{
    qDebug() << "开始保存hosts文件...";
    
    // 安全检查
    if (!hasUnsavedChanges) {
        updateStatusMessage(tr("没有需要保存的更改"), false);
        return;
    }
    
    // 检查文本内容
    if (hostsTextEdit->toPlainText().trimmed().isEmpty()) {
        updateStatusMessage(tr("hosts文件内容为空，无需保存"), false);
        return;
    }
    
    try {
        if (hasAdminRights) {
            qDebug() << "使用管理员权限直接保存";
            // 直接保存
            if (saveHostsFile()) {
                updateStatusMessage(tr("hosts文件保存成功"), false);
                hasUnsavedChanges = false;
                saveBtn->setEnabled(false);
            } else {
                updateStatusMessage(tr("保存失败：无法写入hosts文件"), true);
            }
        } else {
            qDebug() << "需要提升权限保存";
            // 尝试提升权限保存
            updateStatusMessage(tr("正在申请管理员权限..."), false);
            if (saveWithElevation()) {
                updateStatusMessage(tr("hosts文件保存成功（已提升权限）"), false);
                hasUnsavedChanges = false;
                saveBtn->setEnabled(false);
                // 重新检查权限状态
                hasAdminRights = checkAdminRights();
            } else {
                updateStatusMessage(tr("保存失败：无法获取管理员权限或用户取消操作"), true);
            }
        }
    } catch (const std::exception& e) {
        qDebug() << "保存过程中发生异常:" << e.what();
        updateStatusMessage(tr("保存失败：%1").arg(e.what()), true);
    } catch (...) {
        qDebug() << "保存过程中发生未知异常";
        updateStatusMessage(tr("保存失败：未知错误"), true);
    }
}

bool SimpleHostsEditor::saveHostsFile()
{
    qDebug() << "开始直接保存hosts文件";
    updateStatusMessage(tr("正在保存hosts文件..."), false);
    
    try {
        // 创建备份
        createBackup();
        
        QFile file(hostsFilePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qDebug() << "无法打开hosts文件进行写入:" << file.errorString();
            updateStatusMessage(tr("无法写入hosts文件: %1").arg(file.errorString()), true);
            return false;
        }
        
        QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        out.setCodec("UTF-8");
#else
        out.setEncoding(QStringConverter::Utf8);
#endif
        
        out << hostsTextEdit->toPlainText();
        file.close();
        
        qDebug() << "hosts文件保存成功";
        return true;
    } catch (const std::exception& e) {
        qDebug() << "saveHostsFile异常:" << e.what();
        updateStatusMessage(tr("保存文件时发生异常: %1").arg(e.what()), true);
        return false;
    } catch (...) {
        qDebug() << "saveHostsFile未知异常";
        updateStatusMessage(tr("保存文件时发生未知异常"), true);
        return false;
    }
}

bool SimpleHostsEditor::saveWithElevation()
{
#ifdef Q_OS_WIN
    qDebug() << "开始权限提升保存流程";
    
    try {
        // 创建临时文件
        QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        QString tempFile = tempPath + "/hosts_temp_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".txt";
        
        qDebug() << "临时文件路径:" << tempFile;
        
        // 写入临时文件
        QFile file(tempFile);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qDebug() << "无法创建临时文件:" << file.errorString();
            updateStatusMessage(tr("无法创建临时文件: %1").arg(file.errorString()), true);
            return false;
        }
        
        QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        out.setCodec("UTF-8");
#else
        out.setEncoding(QStringConverter::Utf8);
#endif
        out << hostsTextEdit->toPlainText();
        file.close();
        
        qDebug() << "临时文件写入完成";
    
        // 创建PowerShell脚本来复制文件
        QString psScript = QString(
            "try {\n"
            "    Copy-Item -Path \"%1\" -Destination \"%2\" -Force\n"
            "    Write-Host \"SUCCESS\"\n"
            "    exit 0\n"
            "} catch {\n"
            "    Write-Host \"ERROR: $($_.Exception.Message)\"\n"
            "    exit 1\n"
            "}\n"
        ).arg(tempFile, hostsFilePath);
        
        QString psScriptFile = tempPath + "/copy_hosts_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".ps1";
        QFile scriptFile(psScriptFile);
        if (scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream scriptOut(&scriptFile);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            scriptOut.setCodec("UTF-8");
#else
            scriptOut.setEncoding(QStringConverter::Utf8);
#endif
            scriptOut << psScript;
            scriptFile.close();
            qDebug() << "PowerShell脚本文件创建成功:" << psScriptFile;
        } else {
            qDebug() << "无法创建PowerShell脚本文件";
            QFile::remove(tempFile);
            return false;
        }
    
        // 使用ShellExecuteEx提升权限运行PowerShell
        SHELLEXECUTEINFOW sei = {0};
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
        sei.lpVerb = L"runas";
        sei.lpFile = L"powershell.exe";
        
        QString params = QString("-ExecutionPolicy Bypass -File \"%1\"").arg(psScriptFile);
        std::wstring wParams = params.toStdWString();
        sei.lpParameters = wParams.c_str();
        sei.nShow = SW_HIDE;
        
        bool success = false;
        qDebug() << "调用ShellExecuteExW";
        
        if (ShellExecuteExW(&sei)) {
            qDebug() << "ShellExecuteExW调用成功";
            if (sei.hProcess) {
                qDebug() << "等待PowerShell进程完成";
                // 等待进程完成，最多等待30秒
                DWORD waitResult = WaitForSingleObject(sei.hProcess, 30000);
                if (waitResult == WAIT_OBJECT_0) {
                    DWORD exitCode;
                    if (GetExitCodeProcess(sei.hProcess, &exitCode)) {
                        qDebug() << "PowerShell进程退出代码:" << exitCode;
                        success = (exitCode == 0);
                    }
                } else if (waitResult == WAIT_TIMEOUT) {
                    qDebug() << "PowerShell操作超时";
                    updateStatusMessage(tr("操作超时"), true);
                    TerminateProcess(sei.hProcess, 1);
                }
                CloseHandle(sei.hProcess);
            }
        } else {
            DWORD error = GetLastError();
            qDebug() << "ShellExecuteExW失败，错误代码:" << error;
            if (error == ERROR_CANCELLED) {
                updateStatusMessage(tr("用户取消了权限提升"), true);
            } else {
                updateStatusMessage(tr("权限提升失败，错误代码: %1").arg(error), true);
            }
        }
        
        // 清理临时文件
        QFile::remove(tempFile);
        QFile::remove(psScriptFile);
        
        return success;
    } catch (const std::exception& e) {
        qDebug() << "saveWithElevation异常:" << e.what();
        updateStatusMessage(tr("权限提升失败: %1").arg(e.what()), true);
        return false;
    } catch (...) {
        qDebug() << "saveWithElevation未知异常";
        updateStatusMessage(tr("权限提升失败: 未知错误"), true);
        return false;
    }
#elif defined(Q_OS_MAC)
    // macOS: 写入临时文件，通过 osascript 调用 sudo cp 提权复制到 /etc/hosts
    try {
        QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        QString tempFile = tempPath + "/hosts_temp_" + QString::number(QDateTime::currentMSecsSinceEpoch());

        QFile file(tempFile);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            updateStatusMessage(tr("无法创建临时文件: %1").arg(file.errorString()), true);
            return false;
        }
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        out << hostsTextEdit->toPlainText();
        file.close();

        // osascript 弹出密码框执行 sudo cp
        QString script = QString(
            "do shell script \"cp %1 %2\" with administrator privileges"
        ).arg(tempFile, hostsFilePath);

        QProcess process;
        process.start("osascript", QStringList() << "-e" << script);
        process.waitForFinished(30000);

        QFile::remove(tempFile);

        if (process.exitCode() == 0) {
            return true;
        } else {
            QString err = process.readAllStandardError().trimmed();
            if (err.contains("User canceled") || err.contains("(-128)")) {
                updateStatusMessage(tr("用户取消了权限提升"), true);
            } else {
                updateStatusMessage(tr("权限提升失败: %1").arg(err), true);
            }
            return false;
        }
    } catch (...) {
        updateStatusMessage(tr("权限提升失败: 未知错误"), true);
        return false;
    }
#else
    // Linux: 使用 pkexec
    try {
        QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        QString tempFile = tempPath + "/hosts_temp_" + QString::number(QDateTime::currentMSecsSinceEpoch());

        QFile file(tempFile);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            updateStatusMessage(tr("无法创建临时文件"), true);
            return false;
        }
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        out << hostsTextEdit->toPlainText();
        file.close();

        QProcess process;
        process.start("pkexec", QStringList() << "cp" << tempFile << hostsFilePath);
        process.waitForFinished(30000);

        QFile::remove(tempFile);
        return process.exitCode() == 0;
    } catch (...) {
        updateStatusMessage(tr("权限提升失败: 未知错误"), true);
        return false;
    }
#endif
}

void SimpleHostsEditor::onFlushDns()
{
    updateStatusMessage(tr("正在刷新DNS缓存..."), false);
    flushDnsWithAPI();
}

void SimpleHostsEditor::flushDnsWithAPI()
{
#ifdef Q_OS_WIN
    // 使用Windows API刷新DNS缓存
    HMODULE hDnsApi = LoadLibraryW(L"dnsapi.dll");
    if (hDnsApi) {
        typedef BOOL (WINAPI *DnsFlushResolverCacheFunc)();
        DnsFlushResolverCacheFunc DnsFlushResolverCache = 
            (DnsFlushResolverCacheFunc)GetProcAddress(hDnsApi, "DnsFlushResolverCache");
        
        if (DnsFlushResolverCache) {
            if (DnsFlushResolverCache()) {
                updateStatusMessage(tr("DNS缓存刷新成功"), false);
            } else {
                updateStatusMessage(tr("DNS缓存刷新失败"), true);
            }
        } else {
            updateStatusMessage(tr("无法找到DNS刷新函数"), true);
        }
        
        FreeLibrary(hDnsApi);
    } else {
        updateStatusMessage(tr("无法加载DNS API"), true);
    }
#elif defined(Q_OS_MAC)
    // macOS: dscacheutil -flushcache + killall mDNSResponder
    QProcess process;
    process.start("dscacheutil", QStringList() << "-flushcache");
    process.waitForFinished(5000);
    QProcess::execute("killall", QStringList() << "-HUP" << "mDNSResponder");
    updateStatusMessage(tr("DNS缓存刷新成功"), false);
#else
    // Linux: systemd-resolve --flush-caches 或 resolvectl flush-caches
    QProcess process;
    process.start("resolvectl", QStringList() << "flush-caches");
    process.waitForFinished(5000);
    if (process.exitCode() == 0) {
        updateStatusMessage(tr("DNS缓存刷新成功"), false);
    } else {
        // fallback
        QProcess::execute("systemd-resolve", QStringList() << "--flush-caches");
        updateStatusMessage(tr("DNS缓存刷新完成"), false);
    }
#endif
}

void SimpleHostsEditor::onResetHosts()
{
    QMessageBox::StandardButton reply = QMessageBox::question(this, 
        tr("重置hosts文件"), 
        tr("确定要重置hosts文件为默认内容吗？\n当前内容将被清空！"),
        QMessageBox::Yes | QMessageBox::No);
        
    if (reply == QMessageBox::Yes) {
        QString defaultContent = 
            "# Copyright (c) 1993-2009 Microsoft Corp.\n"
            "#\n"
            "# This is a sample HOSTS file used by Microsoft TCP/IP for Windows.\n"
            "#\n"
            "# This file contains the mappings of IP addresses to host names. Each\n"
            "# entry should be kept on an individual line. The IP address should\n"
            "# be placed in the first column followed by the corresponding host name.\n"
            "# The IP address and the host name should be separated by at least one\n"
            "# space.\n"
            "#\n"
            "# Additionally, comments (such as these) may be inserted on individual\n"
            "# lines or following the machine name denoted by a '#' symbol.\n"
            "#\n"
            "# For example:\n"
            "#\n"
            "#      102.54.94.97     rhino.acme.com          # source server\n"
            "#       38.25.63.10     x.acme.com              # x client host\n"
            "#\n"
            "# localhost name resolution is handled within DNS itself.\n"
            "#\t127.0.0.1       localhost\n"
            "#\t::1             localhost\n";
            
        hostsTextEdit->setPlainText(defaultContent);
        hasUnsavedChanges = true;
        saveBtn->setEnabled(true);
        updateStatusMessage(tr("已重置为默认hosts内容"), false);
    }
}

void SimpleHostsEditor::onBackupHosts()
{
    createBackup();
    updateStatusMessage(tr("备份创建完成"), false);
}

void SimpleHostsEditor::createBackup()
{
    QString backupPath = hostsFilePath + ".backup." + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    if (QFile::copy(hostsFilePath, backupPath)) {
        updateStatusMessage(tr("已创建备份: %1").arg(QFileInfo(backupPath).fileName()), false);
    } else {
        updateStatusMessage(tr("备份创建失败"), true);
    }
}

void SimpleHostsEditor::onTextChanged()
{
    if (!hasUnsavedChanges) {
        hasUnsavedChanges = true;
        saveBtn->setEnabled(true);
    }
}

void SimpleHostsEditor::updateStatus()
{
    // 更新行数统计
    int lineCount = hostsTextEdit->toPlainText().split('\n').count();
    lineCountLabel->setText(tr("行数: %1").arg(lineCount));
    
    // 更新保存按钮状态
    saveBtn->setEnabled(hasUnsavedChanges);
}

void SimpleHostsEditor::updateStatusMessage(const QString& message, bool isError)
{
    statusLabel->setText(message);
    statusLabel->setStyleSheet(isError ? 
        "color: #dc3545; font-weight: bold;" : 
        "color: #28a745; font-weight: bold;");
}

QString SimpleHostsEditor::getHostsFilePath()
{
#ifdef Q_OS_WIN
    return "C:/Windows/System32/drivers/etc/hosts";
#else
    return "/etc/hosts";
#endif
}

bool SimpleHostsEditor::checkAdminRights()
{
#ifdef Q_OS_WIN
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    
    adminStatusLabel->setText(isAdmin ? tr("✅ 管理员权限") : tr("⚠️ 普通用户权限"));
    adminStatusLabel->setStyleSheet(isAdmin ? 
        "color: #28a745; font-weight: bold;" : 
        "color: #ffc107; font-weight: bold;");
    
    return isAdmin;
#else
    bool isRoot = (getuid() == 0);
    adminStatusLabel->setText(isRoot ? tr("✅ Root权限") : tr("⚠️ 普通用户权限"));
    adminStatusLabel->setStyleSheet(isRoot ? 
        "color: #28a745; font-weight: bold;" : 
        "color: #ffc107; font-weight: bold;");
    return isRoot;
#endif
}
