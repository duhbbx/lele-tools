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
#endif

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
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    
    setupToolbar();
    setupEditorArea();
    setupStatusArea();
    
    // 应用全局样式
    setStyleSheet(R"(
        QPushButton {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            padding: 8px 16px;
            border-radius: 0px;
            border: 1px solid #ccc;
            font-size: 11pt;
            font-weight: bold;
            min-width: 80px;
            background-color: #f8f9fa;
        }
        QPushButton:hover { 
            background-color: #e9ecef; 
            border-color: #adb5bd;
        }
        QPushButton:pressed {
            background-color: #dee2e6;
        }
        QPushButton:disabled {
            background-color: #e9ecef;
            color: #6c757d;
            border-color: #dee2e6;
        }
        QGroupBox {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            font-weight: bold;
            border: 2px solid #dee2e6;
            border-radius: 0px;
            margin-top: 1ex;
            padding-top: 10px;
            font-size: 12pt;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px 0 5px;
        }
        QTextEdit {
            font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
            font-size: 11pt;
            border: 2px solid #ced4da;
            border-radius: 0px;
            background-color: white;
            line-height: 1.4;
        }
        QTextEdit:focus {
            border-color: #80bdff;
            outline: 0;
        }
        QLabel {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            font-size: 11pt;
        }
    )");
}

void SimpleHostsEditor::setupToolbar()
{
    toolbarGroup = new QGroupBox(tr("📝 Hosts文本编辑器"));
    toolbarLayout = new QHBoxLayout(toolbarGroup);
    
    loadBtn = new QPushButton(tr("📂 重新加载"));
    saveBtn = new QPushButton(tr("💾 保存文件"));
    flushDnsBtn = new QPushButton(tr("🔄 刷新DNS"));
    resetBtn = new QPushButton(tr("🔄 重置为默认"));
    backupBtn = new QPushButton(tr("📋 创建备份"));
    
    // 设置按钮样式
    saveBtn->setStyleSheet("QPushButton { background-color: #28a745; color: white; } QPushButton:hover { background-color: #218838; } QPushButton:disabled { background-color: #6c757d; }");
    flushDnsBtn->setStyleSheet("QPushButton { background-color: #007bff; color: white; } QPushButton:hover { background-color: #0056b3; }");
    resetBtn->setStyleSheet("QPushButton { background-color: #ffc107; color: #212529; } QPushButton:hover { background-color: #e0a800; }");
    backupBtn->setStyleSheet("QPushButton { background-color: #6f42c1; color: white; } QPushButton:hover { background-color: #5a32a3; }");
    
    toolbarLayout->addWidget(loadBtn);
    toolbarLayout->addWidget(saveBtn);
    toolbarLayout->addWidget(backupBtn);
    toolbarLayout->addWidget(resetBtn);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(flushDnsBtn);
    
    mainLayout->addWidget(toolbarGroup);
}

void SimpleHostsEditor::setupEditorArea()
{
    editorGroup = new QGroupBox(tr("📄 Hosts文件内容"));
    editorLayout = new QVBoxLayout(editorGroup);
    
    // 信息标签
    infoLabel = new QLabel(tr("💡 提示：直接编辑hosts文件内容，格式为：IP地址 主机名 # 注释"));
    infoLabel->setStyleSheet("color: #6c757d; font-style: italic; padding: 5px; background-color: #f8f9fa; border-radius: 0px;");
    infoLabel->setWordWrap(true);
    
    // 文本编辑器
    hostsTextEdit = new QTextEdit();
    hostsTextEdit->setMinimumHeight(400);
    hostsTextEdit->setPlaceholderText(tr("# Hosts文件内容将在这里显示\n# 格式示例：\n# 127.0.0.1    localhost\n# 0.0.0.0      example.com    # 阻止访问example.com"));
    
    // 设置等宽字体
    QFont font("Consolas", 11);
    font.setStyleHint(QFont::Monospace);
    hostsTextEdit->setFont(font);
    
    editorLayout->addWidget(infoLabel);
    editorLayout->addWidget(hostsTextEdit);
    
    mainLayout->addWidget(editorGroup);
}

void SimpleHostsEditor::setupStatusArea()
{
    statusLayout = new QHBoxLayout();
    
    statusLabel = new QLabel(tr("就绪"));
    statusLabel->setStyleSheet("color: #28a745; font-weight: bold;");
    
    lineCountLabel = new QLabel(tr("行数: 0"));
    lineCountLabel->setStyleSheet("color: #6c757d;");
    
    adminStatusLabel = new QLabel(tr("权限检查中..."));
    
    progressBar = new QProgressBar();
    progressBar->setVisible(false);
    progressBar->setMaximumHeight(20);
    
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
#else
    // Linux/Mac系统使用pkexec或sudo
    updateStatusMessage(tr("Linux/Mac系统暂不支持权限提升"), true);
    return false;
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
#else
    // Linux/Mac系统
    updateStatusMessage(tr("DNS缓存刷新功能仅支持Windows系统"), true);
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
