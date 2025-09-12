
#include "hostseditortable.h"
#include <QDebug>
#include <string>
#include <exception>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QApplication>
#include <QFileInfo>
#include <QRegularExpression>
#include <QHeaderView>
#include <QScrollBar>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#include <wininet.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shell32.lib")
#endif

REGISTER_DYNAMICOBJECT(HostsEditor);

HostsEditor::HostsEditor() : QWidget(nullptr), DynamicObjectBase() {
    isLoading = false;
    hasUnsavedChanges = false;

    setupUI();
    hostsFilePath = getHostsFilePath();
    hasAdminRights = checkAdminRights();

    // 连接信号槽
    connect(loadBtn, &QPushButton::clicked, this, &HostsEditor::onLoadHosts);
    connect(saveBtn, &QPushButton::clicked, this, &HostsEditor::onSaveHosts);
    connect(flushDnsBtn, &QPushButton::clicked, this, &HostsEditor::onFlushDns);
    connect(quickAddBtn, &QPushButton::clicked, this, &HostsEditor::onQuickAdd);
    connect(addBtn, &QPushButton::clicked, this, &HostsEditor::onAddEntry);
    connect(removeBtn, &QPushButton::clicked, this, &HostsEditor::onRemoveEntry);
    connect(toggleBtn, &QPushButton::clicked, this, &HostsEditor::onToggleEntry);
    connect(clearBtn, &QPushButton::clicked, this, &HostsEditor::onClearAll);

    // 表格信号
    connect(hostsTable, &QTableWidget::itemChanged, this, &HostsEditor::onTableItemChanged);
    connect(hostsTable, &QTableWidget::itemSelectionChanged, this, &HostsEditor::onTableSelectionChanged);

    // 自动加载hosts文件
    qDebug() << "HostsEditor: 开始加载hosts文件，路径:" << hostsFilePath;
    loadHostsFile();
    updateButtonStates();
    qDebug() << "HostsEditor: 初始化完成，加载了" << hostEntries.size() << "个条目";
}

HostsEditor::~HostsEditor() {
    delete fileWatcher;
}

void HostsEditor::setupUI() {
    mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    setupToolbar();
    setupTableArea();
    setupQuickAddArea();
    setupStatusArea();
}

void HostsEditor::setupToolbar() {
    toolbarGroup = new QGroupBox(tr("🛠️ Hosts文件编辑器"));
    toolbarLayout = new QHBoxLayout(toolbarGroup);

    loadBtn = new QPushButton(tr("📂 重新加载"));
    saveBtn = new QPushButton(tr("💾 保存文件"));
    addBtn = new QPushButton(tr("➕ 添加条目"));
    removeBtn = new QPushButton(tr("🗑️ 删除条目"));
    toggleBtn = new QPushButton(tr("🔄 启用/禁用"));
    clearBtn = new QPushButton(tr("🧹 清空全部"));
    flushDnsBtn = new QPushButton(tr("🔄 刷新DNS"));

    // 设置按钮样式
    saveBtn->setStyleSheet(
        "QPushButton { background-color: #28a745; color: white; } QPushButton:hover { background-color: #218838; }");
    flushDnsBtn->setStyleSheet(
        "QPushButton { background-color: #007bff; color: white; } QPushButton:hover { background-color: #0056b3; }");
    removeBtn->setStyleSheet(
        "QPushButton { background-color: #dc3545; color: white; } QPushButton:hover { background-color: #c82333; }");
    clearBtn->setStyleSheet(
        "QPushButton { background-color: #fd7e14; color: white; } QPushButton:hover { background-color: #e66100; }");

    toolbarLayout->addWidget(loadBtn);
    toolbarLayout->addWidget(saveBtn);
    toolbarLayout->addWidget(addBtn);
    toolbarLayout->addWidget(removeBtn);
    toolbarLayout->addWidget(toggleBtn);
    toolbarLayout->addWidget(clearBtn);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(flushDnsBtn);

    mainLayout->addWidget(toolbarGroup);
}

void HostsEditor::setupTableArea() {
    tableGroup = new QGroupBox(tr("📋 Hosts条目列表"));
    tableLayout = new QVBoxLayout(tableGroup);

    hostsTable = new QTableWidget();
    hostsTable->setColumnCount(4);
    hostsTable->setHorizontalHeaderLabels(QStringList() << tr("状态") << tr("IP地址") << tr("主机名") << tr("注释"));

    // 设置表格属性
    hostsTable->setAlternatingRowColors(true);
    hostsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    hostsTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    hostsTable->setSortingEnabled(true);
    hostsTable->verticalHeader()->setVisible(false);

    // 设置列宽
    QHeaderView* header = hostsTable->horizontalHeader();
    header->setStretchLastSection(true);
    hostsTable->setColumnWidth(0, 80);
    hostsTable->setColumnWidth(1, 150);
    hostsTable->setColumnWidth(2, 200);
    
    // 设置行高
    QHeaderView* verticalHeader = hostsTable->verticalHeader();
    verticalHeader->setDefaultSectionSize(35); // 设置默认行高为35像素
    verticalHeader->setMinimumSectionSize(30); // 设置最小行高为30像素
    
    // 设置字体和行高
    QFont tableFont = hostsTable->font();
    tableFont.setPointSize(11);
    hostsTable->setFont(tableFont);
    
    // 设置自定义委托以控制编辑器样式
    HostsTableDelegate* delegate = new HostsTableDelegate(this);
    hostsTable->setItemDelegate(delegate);

    tableLayout->addWidget(hostsTable);
    mainLayout->addWidget(tableGroup);
}

void HostsEditor::setupQuickAddArea() {
    quickAddGroup = new QGroupBox(tr("⚡ 快速添加Hosts条目"));
    quickAddLayout = new QGridLayout(quickAddGroup);

    ipEdit = new QLineEdit();
    ipEdit->setPlaceholderText(tr("输入IP地址 (例: 127.0.0.1)"));

    hostnameEdit = new QLineEdit();
    hostnameEdit->setPlaceholderText(tr("输入主机名 (例: example.com)"));

    commentEdit = new QLineEdit();
    commentEdit->setPlaceholderText(tr("输入注释 (可选)"));

    enabledCheck = new QCheckBox(tr("启用此条目"));
    enabledCheck->setChecked(true);

    quickAddBtn = new QPushButton(tr("➕ 添加到Hosts"));
    quickAddBtn->setStyleSheet(
        "QPushButton { background-color: #28a745; color: white; min-width: 120px; } QPushButton:hover { background-color: #218838; }");

    // 常用hosts下拉框
    commonHostsCombo = new QComboBox();
    commonHostsCombo->addItem(tr("选择常用hosts..."));
    commonHostsCombo->addItem("127.0.0.1 localhost");
    commonHostsCombo->addItem("0.0.0.0 ads.google.com");
    commonHostsCombo->addItem("0.0.0.0 facebook.com");
    commonHostsCombo->addItem("0.0.0.0 twitter.com");

    connect(commonHostsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
        if (index > 0) {
            const QString text = commonHostsCombo->itemText(index);
            if (QStringList parts = text.split(" "); parts.size() >= 2) {
                ipEdit->setText(parts[0]);
                hostnameEdit->setText(parts[1]);
            }
            commonHostsCombo->setCurrentIndex(0);
        }
    });

    quickAddLayout->addWidget(new QLabel(tr("IP地址:")), 0, 0);
    quickAddLayout->addWidget(ipEdit, 0, 1);
    quickAddLayout->addWidget(new QLabel(tr("主机名:")), 0, 2);
    quickAddLayout->addWidget(hostnameEdit, 0, 3);
    quickAddLayout->addWidget(new QLabel(tr("注释:")), 1, 0);
    quickAddLayout->addWidget(commentEdit, 1, 1);
    quickAddLayout->addWidget(enabledCheck, 1, 2);
    quickAddLayout->addWidget(quickAddBtn, 1, 3);
    quickAddLayout->addWidget(new QLabel(tr("常用:")), 2, 0);
    quickAddLayout->addWidget(commonHostsCombo, 2, 1, 1, 3);

    mainLayout->addWidget(quickAddGroup);
}

void HostsEditor::setupStatusArea() {
    statusLayout = new QHBoxLayout();

    statusLabel = new QLabel(tr("就绪"));
    statusLabel->setStyleSheet("color: #28a745; font-weight: bold;");

    entriesCountLabel = new QLabel(tr("条目: 0"));
    entriesCountLabel->setStyleSheet("color: #6c757d;");

    adminStatusLabel = new QLabel(tr("权限检查中..."));

    progressBar = new QProgressBar();
    progressBar->setVisible(false);
    progressBar->setMaximumHeight(20);

    statusLayout->addWidget(statusLabel);
    statusLayout->addWidget(entriesCountLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(progressBar);
    statusLayout->addWidget(adminStatusLabel);

    mainLayout->addLayout(statusLayout);
}

void HostsEditor::loadHostsFile() {
    isLoading = true;
    updateStatus(tr("正在加载hosts文件..."), false);

    // 检查文件是否存在
    const QFileInfo fileInfo(hostsFilePath);
    if (!fileInfo.exists()) {
        updateStatus(tr("hosts文件不存在: %1").arg(hostsFilePath), true);
        isLoading = false;
        return;
    }

    // 检查文件是否可读
    if (!fileInfo.isReadable()) {
        updateStatus(tr("hosts文件不可读，可能需要管理员权限"), true);
        isLoading = false;
        return;
    }

    QFile file(hostsFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        updateStatus(tr("无法读取hosts文件: %1").arg(file.errorString()), true);
        isLoading = false;
        return;
    }

    hostEntries.clear();
    QTextStream in(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    in.setCodec("UTF-8");
#else
    in.setEncoding(QStringConverter::Utf8);
#endif

    while (!in.atEnd()) {
        QString line = in.readLine();
        parseHostsLine(line);
    }

    file.close();
    isLoading = false;
    updateTable();
    updateStatus(tr("hosts文件加载完成"), false);
    entriesCountLabel->setText(tr("条目: %1").arg(hostEntries.size()));
    hasUnsavedChanges = false;
    updateButtonStates();

}

void HostsEditor::parseHostsLine(const QString& line) {
    const QString trimmedLine = line.trimmed();

    // 跳过空行
    if (trimmedLine.isEmpty()) {
        return;
    }

    bool enabled = true;
    QString workingLine = trimmedLine;

    // 检查是否被注释掉（禁用）
    if (trimmedLine.startsWith("#")) {
        enabled = false;
        workingLine = trimmedLine.mid(1).trimmed();

        // 如果是纯注释行（不是被注释的hosts条目），跳过
        if (workingLine.isEmpty() || !workingLine.contains(QRegularExpression(R"(^\d+\.\d+\.\d+\.\d+)"))) {
            return;
        }
    }

    // 解析IP和主机名
    const QRegularExpression regex(R"(^(\S+)\s+(\S+)(?:\s*#?\s*(.*))?$)");

    if (const QRegularExpressionMatch match = regex.match(workingLine); match.hasMatch()) {
        const QString ip = match.captured(1);
        const QString hostname = match.captured(2);
        const QString comment = match.captured(3).trimmed();

        // 验证IP格式
        if (isValidIP(ip)) {
            hostEntries.append(HostEntry(ip, hostname, comment, enabled));
        }
    }
}

void HostsEditor::onLoadHosts() {
    loadHostsFile();
}

void HostsEditor::onSaveHosts() {
    qDebug() << "开始保存hosts文件...";
    
    // 安全检查
    if (hostEntries.isEmpty()) {
        updateStatus(tr("没有内容需要保存"), true);
        return;
    }
    
    try {
        if (!hasAdminRights) {
            qDebug() << "需要提升权限保存";
            // 尝试提升权限保存
            if (saveWithElevation()) {
                updateStatus(tr("hosts文件保存成功（已提升权限）"), false);
                hasUnsavedChanges = false;
                updateButtonStates();
            } else {
                updateStatus(tr("保存失败：无法获取管理员权限"), true);
            }
            return;
        }

        qDebug() << "使用管理员权限直接保存";
        saveHostsFile();
    } catch (const std::exception& e) {
        qDebug() << "保存过程中发生异常:" << e.what();
        updateStatus(tr("保存失败：%1").arg(e.what()), true);
    } catch (...) {
        qDebug() << "保存过程中发生未知异常";
        updateStatus(tr("保存失败：未知错误"), true);
    }
}

bool HostsEditor::saveWithElevation() {
#ifdef Q_OS_WIN
    qDebug() << "开始权限提升保存流程";
    
    try {
        // 创建临时文件
        const QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        QString tempFile = tempPath + "/hosts_temp_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".txt";
        
        qDebug() << "临时文件路径:" << tempFile;

        // 写入临时文件
        QFile file(tempFile);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            updateStatus(tr("无法创建临时文件: %1").arg(file.errorString()), true);
            return false;
        }

        QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        out.setCodec("UTF-8");
#else
        out.setEncoding(QStringConverter::Utf8);
#endif
        out << generateHostsContent();
        file.close();
        
        qDebug() << "临时文件写入完成";

        // 方法1: 尝试使用PowerShell
        qDebug() << "尝试PowerShell方法";
        bool success = tryPowerShellElevation(tempFile);
        
        if (!success) {
            qDebug() << "PowerShell方法失败，尝试CMD方法";
            // 方法2: 尝试使用cmd复制
            success = tryCmdElevation(tempFile);
        }
        
        if (!success) {
            qDebug() << "CMD方法失败，尝试Robocopy方法";
            // 方法3: 尝试使用robocopy
            success = tryRobocopyElevation(tempFile);
        }

        // 清理临时文件
        QFile::remove(tempFile);
        
        if (!success) {
            qDebug() << "所有自动方法都失败，显示手动保存对话框";
            // 保留临时文件以供手动保存
            QString preservedTempFile = tempFile + "_preserved";
            if (QFile::copy(tempFile, preservedTempFile)) {
                showManualSaveDialog(preservedTempFile);
            }
        }
        
        return success;
    } catch (const std::exception& e) {
        qDebug() << "saveWithElevation异常:" << e.what();
        updateStatus(QString("权限提升失败: %1").arg(e.what()), true);
        return false;
    } catch (...) {
        qDebug() << "saveWithElevation未知异常";
        updateStatus("权限提升失败: 未知错误", true);
        return false;
    }
#else
    // Linux/Mac系统使用pkexec或sudo
    return false;
#endif
}

bool HostsEditor::tryPowerShellElevation(const QString& tempFile) {
#ifdef Q_OS_WIN
    try {
        qDebug() << "开始PowerShell权限提升";
        
        // 创建PowerShell脚本
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
    
        QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
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
                    updateStatus(tr("PowerShell操作超时"), true);
                    TerminateProcess(sei.hProcess, 1);
                }
                CloseHandle(sei.hProcess);
            }
        } else {
            DWORD error = GetLastError();
            qDebug() << "ShellExecuteExW失败，错误代码:" << error;
            if (error == ERROR_CANCELLED) {
                updateStatus(tr("用户取消了权限提升"), true);
            } else {
                updateStatus(tr("PowerShell权限提升失败，错误代码: %1").arg(error), true);
            }
        }
        
        // 清理PowerShell脚本文件
        QFile::remove(psScriptFile);
        
        return success;
    } catch (const std::exception& e) {
        qDebug() << "PowerShell权限提升异常:" << e.what();
        return false;
    } catch (...) {
        qDebug() << "PowerShell权限提升未知异常";
        return false;
    }
#else
    Q_UNUSED(tempFile)
    return false;
#endif
}

bool HostsEditor::tryCmdElevation(const QString& tempFile) {
#ifdef Q_OS_WIN
    // 使用cmd复制文件
    QString cmdLine = QString("/c copy /Y \"%1\" \"%2\"").arg(tempFile, hostsFilePath);
    
    SHELLEXECUTEINFOW sei = {0};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    sei.lpVerb = L"runas";
    sei.lpFile = L"cmd.exe";
    sei.lpParameters = reinterpret_cast<LPCWSTR>(cmdLine.utf16());
    sei.nShow = SW_HIDE;
    
    bool success = false;
    if (ShellExecuteExW(&sei)) {
        if (sei.hProcess) {
            DWORD waitResult = WaitForSingleObject(sei.hProcess, 15000);
            if (waitResult == WAIT_OBJECT_0) {
                DWORD exitCode;
                if (GetExitCodeProcess(sei.hProcess, &exitCode)) {
                    success = (exitCode == 0);
                }
            } else if (waitResult == WAIT_TIMEOUT) {
                updateStatus(tr("CMD操作超时"), true);
                TerminateProcess(sei.hProcess, 1);
            }
            CloseHandle(sei.hProcess);
        }
    } else {
        DWORD error = GetLastError();
        if (error == ERROR_CANCELLED) {
            updateStatus(tr("用户取消了权限提升"), true);
        }
    }
    
    return success;
#else
    Q_UNUSED(tempFile)
    return false;
#endif
}

bool HostsEditor::tryRobocopyElevation(const QString& tempFile) {
#ifdef Q_OS_WIN
    // 使用robocopy复制文件
    QFileInfo tempInfo(tempFile);
    QFileInfo hostsInfo(hostsFilePath);
    
    QString cmdLine = QString("/c robocopy \"%1\" \"%2\" \"%3\" /Y").arg(
        tempInfo.absolutePath(),
        hostsInfo.absolutePath(), 
        tempInfo.fileName()
    );
    
    SHELLEXECUTEINFOW sei = {0};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    sei.lpVerb = L"runas";
    sei.lpFile = L"cmd.exe";
    sei.lpParameters = reinterpret_cast<LPCWSTR>(cmdLine.utf16());
    sei.nShow = SW_HIDE;
    
    bool success = false;
    if (ShellExecuteExW(&sei)) {
        if (sei.hProcess) {
            DWORD waitResult = WaitForSingleObject(sei.hProcess, 15000);
            if (waitResult == WAIT_OBJECT_0) {
                DWORD exitCode;
                if (GetExitCodeProcess(sei.hProcess, &exitCode)) {
                    // robocopy的退出代码: 0-7表示成功
                    success = (exitCode <= 7);
                    if (success) {
                        // 重命名复制的文件
                        QString copiedFile = hostsInfo.absolutePath() + "/" + tempInfo.fileName();
                        QFile::remove(hostsFilePath);
                        QFile::rename(copiedFile, hostsFilePath);
                    }
                }
            } else if (waitResult == WAIT_TIMEOUT) {
                updateStatus(tr("Robocopy操作超时"), true);
                TerminateProcess(sei.hProcess, 1);
            }
            CloseHandle(sei.hProcess);
        }
    }
    
    return success;
#else
    Q_UNUSED(tempFile)
    return false;
#endif
}

void HostsEditor::showManualSaveDialog(const QString& tempFile) {
    QMessageBox msgBox(this);
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setWindowTitle("手动保存hosts文件");
    msgBox.setText("自动权限提升失败，请手动保存hosts文件。");
    msgBox.setInformativeText(QString(
        "请按照以下步骤手动保存：\n\n"
        "1. 以管理员身份运行文件管理器\n"
        "2. 导航到: %1\n"
        "3. 复制临时文件: %2\n"
        "4. 粘贴并替换原hosts文件\n\n"
        "或者点击'复制路径'按钮将路径复制到剪贴板。"
    ).arg(hostsFilePath, tempFile));
    
    QPushButton* copyPathBtn = msgBox.addButton("复制路径", QMessageBox::ActionRole);
    QPushButton* openFolderBtn = msgBox.addButton("打开文件夹", QMessageBox::ActionRole);
    msgBox.addButton("确定", QMessageBox::AcceptRole);
    
    msgBox.exec();
    
    if (msgBox.clickedButton() == copyPathBtn) {
        QApplication::clipboard()->setText(QString("临时文件: %1\n目标位置: %2").arg(tempFile, hostsFilePath));
        updateStatus("路径已复制到剪贴板", false);
    } else if (msgBox.clickedButton() == openFolderBtn) {
        // 打开hosts文件所在文件夹
        QString hostsDir = QFileInfo(hostsFilePath).absolutePath();
        QProcess::startDetached("explorer.exe", QStringList() << hostsDir);
    }
}

void HostsEditor::saveHostsFile() {
    updateStatus("正在保存hosts文件...", false);

    // 备份原文件
    const QString backupPath = hostsFilePath + ".backup." + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QFile::copy(hostsFilePath, backupPath);

    QFile file(hostsFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        updateStatus("无法写入hosts文件: " + file.errorString(), true);
        return;
    }

    QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    out.setCodec("UTF-8");
#else
    out.setEncoding(QStringConverter::Utf8);
#endif
    out << generateHostsContent();

    file.close();
    updateStatus("hosts文件保存成功", false);
    hasUnsavedChanges = false;
    updateButtonStates();
}

QString HostsEditor::generateHostsContent() {
    QString content;
    QTextStream stream(&content);

    // 添加文件头注释
    stream << "# Hosts file - Modified by 乐乐的工具箱\n";
    stream << "# " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "\n";
    stream << "# \n";
    stream << "# This file contains the mappings of IP addresses to host names.\n";
    stream << "# Each entry should be kept on an individual line.\n";
    stream << "# \n\n";

    // 添加hosts条目
    for (const HostEntry& entry : hostEntries) {
        if (!entry.enabled) {
            stream << "# ";
        }

        stream << entry.ip << "\t" << entry.hostname;

        if (!entry.comment.isEmpty()) {
            stream << "\t# " << entry.comment;
        }

        stream << "\n";
    }

    return content;
}

void HostsEditor::onFlushDns() {
    updateStatus(tr("正在刷新DNS缓存..."), false);
    flushDnsWithAPI();
}

void HostsEditor::flushDnsWithAPI() {
#ifdef Q_OS_WIN
    // 使用Windows API刷新DNS缓存
    HMODULE hDnsApi = LoadLibraryW(L"dnsapi.dll");
    if (hDnsApi) {
        typedef BOOL (WINAPI *DnsFlushResolverCacheFunc)();
        DnsFlushResolverCacheFunc DnsFlushResolverCache =
            (DnsFlushResolverCacheFunc) GetProcAddress(hDnsApi, "DnsFlushResolverCache");

        if (DnsFlushResolverCache) {
            if (DnsFlushResolverCache()) {
                updateStatus(tr("DNS缓存刷新成功"), false);
            } else {
                updateStatus(tr("DNS缓存刷新失败"), true);
            }
        } else {
            updateStatus(tr("无法找到DNS刷新函数"), true);
        }

        FreeLibrary(hDnsApi);
    } else {
        updateStatus(tr("无法加载DNS API"), true);
    }
#else
    // Linux/Mac系统
    updateStatus(tr("DNS缓存刷新功能仅支持Windows系统"), true);
#endif
}

void HostsEditor::onQuickAdd() {
    QString ip = ipEdit->text().trimmed();
    QString hostname = hostnameEdit->text().trimmed();
    QString comment = commentEdit->text().trimmed();
    bool enabled = enabledCheck->isChecked();

    if (ip.isEmpty() || hostname.isEmpty()) {
        updateStatus(tr("IP地址和主机名不能为空"), true);
        return;
    }

    if (!isValidIP(ip)) {
        updateStatus(tr("IP地址格式不正确"), true);
        return;
    }

    if (!isValidHostname(hostname)) {
        updateStatus(tr("主机名格式不正确"), true);
        return;
    }

    // 检查是否已存在
    for (const HostEntry& entry : hostEntries) {
        if (entry.hostname == hostname) {
            updateStatus(tr("主机名已存在"), true);
            return;
        }
    }

    hostEntries.append(HostEntry(ip, hostname, comment, enabled));
    updateTable();

    // 清空输入框
    ipEdit->clear();
    hostnameEdit->clear();
    commentEdit->clear();
    enabledCheck->setChecked(true);

    updateStatus(tr("已添加新的hosts条目"), false);
    entriesCountLabel->setText(tr("条目: %1").arg(hostEntries.size()));
    hasUnsavedChanges = true;
    updateButtonStates();
}

void HostsEditor::onAddEntry() {
    // 添加空条目供用户编辑
    hostEntries.append(HostEntry("127.0.0.1", "example.com", "新条目", true));
    updateTable();

    // 选中新添加的行
    int newRow = hostsTable->rowCount() - 1;
    hostsTable->selectRow(newRow);
    hostsTable->scrollToItem(hostsTable->item(newRow, 0));

    updateStatus(tr("已添加新条目，请编辑"), false);
    entriesCountLabel->setText(tr("条目: %1").arg(hostEntries.size()));
    hasUnsavedChanges = true;
    updateButtonStates();
}

void HostsEditor::onRemoveEntry() {
    QList<int> selectedRows;
    for (QTableWidgetItem* item : hostsTable->selectedItems()) {
        int row = item->row();
        if (!selectedRows.contains(row)) {
            selectedRows.append(row);
        }
    }

    if (selectedRows.isEmpty()) {
        updateStatus(tr("请先选择要删除的条目"), true);
        return;
    }

    // 按行号倒序排列，从后往前删除
    std::sort(selectedRows.begin(), selectedRows.end(), std::greater<int>());

    for (int row : selectedRows) {
        if (row >= 0 && row < hostEntries.size()) {
            hostEntries.removeAt(row);
        }
    }

    updateTable();
    updateStatus(tr("已删除 %1 个条目").arg(selectedRows.size()), false);
    entriesCountLabel->setText(tr("条目: %1").arg(hostEntries.size()));
    hasUnsavedChanges = true;
    updateButtonStates();
}

void HostsEditor::onToggleEntry() {
    QList<int> selectedRows;
    for (QTableWidgetItem* item : hostsTable->selectedItems()) {
        int row = item->row();
        if (!selectedRows.contains(row)) {
            selectedRows.append(row);
        }
    }

    if (selectedRows.isEmpty()) {
        updateStatus(tr("请先选择要启用/禁用的条目"), true);
        return;
    }

    for (int row : selectedRows) {
        if (row >= 0 && row < hostEntries.size()) {
            hostEntries[row].enabled = !hostEntries[row].enabled;
        }
    }

    updateTable();
    updateStatus(tr("已切换 %1 个条目的状态").arg(selectedRows.size()), false);
    hasUnsavedChanges = true;
    updateButtonStates();
}

void HostsEditor::onClearAll() {
    if (hostEntries.isEmpty()) {
        updateStatus(tr("没有条目可清空"), true);
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(this,
                                                              "确认清空",
                                                              QString("确定要清空所有 %1 个hosts条目吗？\n此操作不可撤销！").arg(
                                                                  hostEntries.size()),
                                                              QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        hostEntries.clear();
        updateTable();
        updateStatus(tr("已清空所有条目"), false);
        entriesCountLabel->setText(tr("条目: 0"));
        hasUnsavedChanges = true;
        updateButtonStates();
    }
}

void HostsEditor::updateTable() {
    if (isLoading)
        return;

    hostsTable->setRowCount(hostEntries.size());

    for (int i = 0; i < hostEntries.size(); ++i) {
        const HostEntry& entry = hostEntries[i];

        // 状态列
        QTableWidgetItem* statusItem = new QTableWidgetItem(entry.enabled ? tr("✅ 启用") : tr("❌ 禁用"));
        statusItem->setFlags(statusItem->flags() & ~Qt::ItemIsEditable);
        if (!entry.enabled) {
            statusItem->setForeground(QBrush(QColor("#6c757d")));
        }
        hostsTable->setItem(i, 0, statusItem);

        // IP地址列
        QTableWidgetItem* ipItem = new QTableWidgetItem(entry.ip);
        if (!entry.enabled) {
            ipItem->setForeground(QBrush(QColor("#6c757d")));
        }
        hostsTable->setItem(i, 1, ipItem);

        // 主机名列
        QTableWidgetItem* hostnameItem = new QTableWidgetItem(entry.hostname);
        if (!entry.enabled) {
            hostnameItem->setForeground(QBrush(QColor("#6c757d")));
        }
        hostsTable->setItem(i, 2, hostnameItem);

        // 注释列
        QTableWidgetItem* commentItem = new QTableWidgetItem(entry.comment);
        if (!entry.enabled) {
            commentItem->setForeground(QBrush(QColor("#6c757d")));
        }
        hostsTable->setItem(i, 3, commentItem);
    }

    updateButtonStates();
}

void HostsEditor::updateStatus(const QString& message, bool isError) {
    statusLabel->setText(message);
    statusLabel->setStyleSheet(isError ? "color: #dc3545; font-weight: bold;" : "color: #28a745; font-weight: bold;");
}

void HostsEditor::updateButtonStates() {
    bool hasSelection = !hostsTable->selectedItems().isEmpty();
    bool hasEntries = !hostEntries.isEmpty();

    removeBtn->setEnabled(hasSelection);
    toggleBtn->setEnabled(hasSelection);
    clearBtn->setEnabled(hasEntries);
    saveBtn->setEnabled(hasUnsavedChanges);
}

QString HostsEditor::getHostsFilePath() {
#ifdef Q_OS_WIN
    return "C:/Windows/System32/drivers/etc/hosts";
#else
    return "/etc/hosts";
#endif
}

bool HostsEditor::checkAdminRights() {
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
    adminStatusLabel->setStyleSheet(isAdmin
                                        ? "color: #28a745; font-weight: bold;"
                                        : "color: #ffc107; font-weight: bold;");

    return isAdmin;
#else
    bool isRoot = (getuid() == 0);
    adminStatusLabel->setText(isRoot ? tr("✅ Root权限") : tr("⚠️ 普通用户权限"));
    adminStatusLabel->setStyleSheet(
        isRoot ? "color: #28a745; font-weight: bold;" : "color: #ffc107; font-weight: bold;");
    return isRoot;
#endif
}

bool HostsEditor::isValidIP(const QString& ip) {
    QRegularExpression ipRegex(
        "^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$");
    return ipRegex.match(ip).hasMatch();
}

bool HostsEditor::isValidHostname(const QString& hostname) {
    if (hostname.isEmpty() || hostname.length() > 253) {
        return false;
    }

    QRegularExpression hostnameRegex(
        "^[a-zA-Z0-9]([a-zA-Z0-9\\-]{0,61}[a-zA-Z0-9])?(\\.([a-zA-Z0-9]([a-zA-Z0-9\\-]{0,61}[a-zA-Z0-9])?))*$");
    return hostnameRegex.match(hostname).hasMatch();
}

void HostsEditor::onTableItemChanged(QTableWidgetItem* item) {
    if (isLoading)
        return;

    int row = item->row();
    int column = item->column();

    if (row >= 0 && row < hostEntries.size()) {
        QString newValue = item->text().trimmed();

        switch (column) {
        case 1: // IP地址
            if (isValidIP(newValue)) {
                hostEntries[row].ip = newValue;
                hasUnsavedChanges = true;
            } else {
                updateStatus(tr("IP地址格式不正确"), true);
                item->setText(hostEntries[row].ip); // 恢复原值
            }
            break;
        case 2: // 主机名
            if (isValidHostname(newValue)) {
                hostEntries[row].hostname = newValue;
                hasUnsavedChanges = true;
            } else {
                updateStatus(tr("主机名格式不正确"), true);
                item->setText(hostEntries[row].hostname); // 恢复原值
            }
            break;
        case 3: // 注释
            hostEntries[row].comment = newValue;
            hasUnsavedChanges = true;
            break;
        }

        updateButtonStates();
    }
}

void HostsEditor::onTableSelectionChanged() {
    updateButtonStates();
}

// 空实现的槽函数
void HostsEditor::onEditEntry() {
}

void HostsEditor::onBackupHosts() {
}

void HostsEditor::onRestoreHosts() {
}

void HostsEditor::onImportHosts() {
}

void HostsEditor::onExportHosts() {
}

void HostsEditor::onSearchHosts() {
}

void HostsEditor::onFileChanged(const QString&) {
}

void HostsEditor::onDnsFlushFinished(int, QProcess::ExitStatus) {
}

void HostsEditor::parseHostsContent(const QString&) {
}

void HostsEditor::addHostEntry(const QString&, const QString&, const QString&, bool) {
}

void HostsEditor::removeSelectedEntries() {
}

void HostsEditor::toggleSelectedEntries() {
}

void HostsEditor::showAdminWarning() {
}

bool HostsEditor::requestAdminRights() {
    return false;
}
