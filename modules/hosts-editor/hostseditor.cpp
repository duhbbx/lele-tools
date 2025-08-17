#include "hostseditor.h"
#include <QDebug>
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

HostsEditor::HostsEditor() : QWidget(nullptr), DynamicObjectBase()
{
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
    loadHostsFile();
    updateButtonStates();
}

HostsEditor::~HostsEditor() 
{
    if (fileWatcher) {
        delete fileWatcher;
    }
}

void HostsEditor::setupUI()
{
    mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    
    setupToolbar();
    setupTableArea();
    setupQuickAddArea();
    setupStatusArea();
    
    // 应用全局样式
    setStyleSheet(R"(
        QPushButton {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            padding: 8px 16px;
            border-radius: 4px;
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
            border-radius: 8px;
            margin-top: 1ex;
            padding-top: 10px;
            font-size: 12pt;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px 0 5px;
        }
        QLineEdit {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            padding: 8px;
            border: 2px solid #ced4da;
            border-radius: 4px;
            font-size: 11pt;
            background-color: white;
        }
        QLineEdit:focus {
            border-color: #80bdff;
            outline: 0;
            box-shadow: 0 0 0 0.2rem rgba(0,123,255,.25);
        }
        QTableWidget {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            border: 2px solid #dee2e6;
            border-radius: 8px;
            font-size: 11pt;
            gridline-color: #dee2e6;
            background-color: white;
            alternate-background-color: #f8f9fa;
        }
        QTableWidget::item {
            padding: 8px;
            border-bottom: 1px solid #dee2e6;
        }
        QTableWidget::item:selected {
            background-color: #007bff;
            color: white;
        }
        QHeaderView::section {
            background-color: #f8f9fa;
            padding: 8px;
            border: 1px solid #dee2e6;
            font-weight: bold;
        }
        QLabel {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            font-size: 11pt;
        }
    )");
}

void HostsEditor::setupToolbar()
{
    toolbarGroup = new QGroupBox("🛠️ Hosts文件编辑器");
    toolbarLayout = new QHBoxLayout(toolbarGroup);
    
    loadBtn = new QPushButton("📂 重新加载");
    saveBtn = new QPushButton("💾 保存文件");
    addBtn = new QPushButton("➕ 添加条目");
    removeBtn = new QPushButton("🗑️ 删除条目");
    toggleBtn = new QPushButton("🔄 启用/禁用");
    clearBtn = new QPushButton("🧹 清空全部");
    flushDnsBtn = new QPushButton("🔄 刷新DNS");
    
    // 设置按钮样式
    saveBtn->setStyleSheet("QPushButton { background-color: #28a745; color: white; } QPushButton:hover { background-color: #218838; }");
    flushDnsBtn->setStyleSheet("QPushButton { background-color: #007bff; color: white; } QPushButton:hover { background-color: #0056b3; }");
    removeBtn->setStyleSheet("QPushButton { background-color: #dc3545; color: white; } QPushButton:hover { background-color: #c82333; }");
    clearBtn->setStyleSheet("QPushButton { background-color: #fd7e14; color: white; } QPushButton:hover { background-color: #e66100; }");
    
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

void HostsEditor::setupTableArea()
{
    tableGroup = new QGroupBox("📋 Hosts条目列表");
    tableLayout = new QVBoxLayout(tableGroup);
    
    hostsTable = new QTableWidget();
    hostsTable->setColumnCount(4);
    hostsTable->setHorizontalHeaderLabels(QStringList() << "状态" << "IP地址" << "主机名" << "注释");
    
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
    
    tableLayout->addWidget(hostsTable);
    mainLayout->addWidget(tableGroup);
}

void HostsEditor::setupQuickAddArea()
{
    quickAddGroup = new QGroupBox("⚡ 快速添加Hosts条目");
    quickAddLayout = new QGridLayout(quickAddGroup);
    
    ipEdit = new QLineEdit();
    ipEdit->setPlaceholderText("输入IP地址 (例: 127.0.0.1)");
    
    hostnameEdit = new QLineEdit();
    hostnameEdit->setPlaceholderText("输入主机名 (例: example.com)");
    
    commentEdit = new QLineEdit();
    commentEdit->setPlaceholderText("输入注释 (可选)");
    
    enabledCheck = new QCheckBox("启用此条目");
    enabledCheck->setChecked(true);
    
    quickAddBtn = new QPushButton("➕ 添加到Hosts");
    quickAddBtn->setStyleSheet("QPushButton { background-color: #28a745; color: white; min-width: 120px; } QPushButton:hover { background-color: #218838; }");
    
    // 常用hosts下拉框
    commonHostsCombo = new QComboBox();
    commonHostsCombo->addItem("选择常用hosts...");
    commonHostsCombo->addItem("127.0.0.1 localhost");
    commonHostsCombo->addItem("0.0.0.0 ads.google.com");
    commonHostsCombo->addItem("0.0.0.0 facebook.com");
    commonHostsCombo->addItem("0.0.0.0 twitter.com");
    
    connect(commonHostsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
        if (index > 0) {
            QString text = commonHostsCombo->itemText(index);
            QStringList parts = text.split(" ");
            if (parts.size() >= 2) {
                ipEdit->setText(parts[0]);
                hostnameEdit->setText(parts[1]);
            }
            commonHostsCombo->setCurrentIndex(0);
        }
    });
    
    quickAddLayout->addWidget(new QLabel("IP地址:"), 0, 0);
    quickAddLayout->addWidget(ipEdit, 0, 1);
    quickAddLayout->addWidget(new QLabel("主机名:"), 0, 2);
    quickAddLayout->addWidget(hostnameEdit, 0, 3);
    quickAddLayout->addWidget(new QLabel("注释:"), 1, 0);
    quickAddLayout->addWidget(commentEdit, 1, 1);
    quickAddLayout->addWidget(enabledCheck, 1, 2);
    quickAddLayout->addWidget(quickAddBtn, 1, 3);
    quickAddLayout->addWidget(new QLabel("常用:"), 2, 0);
    quickAddLayout->addWidget(commonHostsCombo, 2, 1, 1, 3);
    
    mainLayout->addWidget(quickAddGroup);
}

void HostsEditor::setupStatusArea()
{
    statusLayout = new QHBoxLayout();
    
    statusLabel = new QLabel("就绪");
    statusLabel->setStyleSheet("color: #28a745; font-weight: bold;");
    
    entriesCountLabel = new QLabel("条目: 0");
    entriesCountLabel->setStyleSheet("color: #6c757d;");
    
    adminStatusLabel = new QLabel("权限检查中...");
    
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

void HostsEditor::loadHostsFile()
{
    isLoading = true;
    updateStatus("正在加载hosts文件...", false);
    
    QFile file(hostsFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        updateStatus("无法读取hosts文件: " + file.errorString(), true);
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
    updateTable();
    updateStatus("hosts文件加载完成", false);
    entriesCountLabel->setText(QString("条目: %1").arg(hostEntries.size()));
    hasUnsavedChanges = false;
    isLoading = false;
}

void HostsEditor::parseHostsLine(const QString& line)
{
    QString trimmedLine = line.trimmed();
    
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
        if (workingLine.isEmpty() || !workingLine.contains(QRegularExpression("^\\d+\\.\\d+\\.\\d+\\.\\d+"))) {
            return;
        }
    }
    
    // 解析IP和主机名
    QRegularExpression regex("^(\\S+)\\s+(\\S+)(?:\\s*#?\\s*(.*))?$");
    QRegularExpressionMatch match = regex.match(workingLine);
    
    if (match.hasMatch()) {
        QString ip = match.captured(1);
        QString hostname = match.captured(2);
        QString comment = match.captured(3).trimmed();
        
        // 验证IP格式
        if (isValidIP(ip)) {
            hostEntries.append(HostEntry(ip, hostname, comment, enabled));
        }
    }
}

void HostsEditor::onLoadHosts()
{
    loadHostsFile();
}

void HostsEditor::onSaveHosts()
{
    if (!hasAdminRights) {
        // 尝试提升权限保存
        if (saveWithElevation()) {
            updateStatus("hosts文件保存成功（已提升权限）", false);
            hasUnsavedChanges = false;
        } else {
            updateStatus("保存失败：无法获取管理员权限", true);
        }
        return;
    }
    
    saveHostsFile();
}

bool HostsEditor::saveWithElevation()
{
#ifdef Q_OS_WIN
    // 创建临时文件
    QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString tempFile = tempPath + "/hosts_temp_" + QString::number(QDateTime::currentMSecsSinceEpoch());
    
    // 写入临时文件
    QFile file(tempFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
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
    
    // 使用ShellExecuteEx提升权限复制文件
    QString cmdLine = QString("cmd.exe /c copy \"%1\" \"%2\"").arg(tempFile, hostsFilePath);
    
    SHELLEXECUTEINFOW sei = {0};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    sei.lpVerb = L"runas";
    sei.lpFile = L"cmd.exe";
    sei.lpParameters = reinterpret_cast<LPCWSTR>(cmdLine.utf16());
    sei.nShow = SW_HIDE;
    
    if (ShellExecuteExW(&sei)) {
        if (sei.hProcess) {
            WaitForSingleObject(sei.hProcess, INFINITE);
            DWORD exitCode;
            GetExitCodeProcess(sei.hProcess, &exitCode);
            CloseHandle(sei.hProcess);
            
            // 清理临时文件
            QFile::remove(tempFile);
            
            return (exitCode == 0);
        }
    }
    
    // 清理临时文件
    QFile::remove(tempFile);
    return false;
#else
    // Linux/Mac系统使用pkexec或sudo
    return false;
#endif
}

void HostsEditor::saveHostsFile()
{
    updateStatus("正在保存hosts文件...", false);
    
    // 备份原文件
    QString backupPath = hostsFilePath + ".backup." + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
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
}

QString HostsEditor::generateHostsContent()
{
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

void HostsEditor::onFlushDns()
{
    updateStatus("正在刷新DNS缓存...", false);
    flushDnsWithAPI();
}

void HostsEditor::flushDnsWithAPI()
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
                updateStatus("DNS缓存刷新成功", false);
            } else {
                updateStatus("DNS缓存刷新失败", true);
            }
        } else {
            updateStatus("无法找到DNS刷新函数", true);
        }
        
        FreeLibrary(hDnsApi);
    } else {
        updateStatus("无法加载DNS API", true);
    }
#else
    // Linux/Mac系统
    updateStatus("DNS缓存刷新功能仅支持Windows系统", true);
#endif
}

void HostsEditor::onQuickAdd()
{
    QString ip = ipEdit->text().trimmed();
    QString hostname = hostnameEdit->text().trimmed();
    QString comment = commentEdit->text().trimmed();
    bool enabled = enabledCheck->isChecked();
    
    if (ip.isEmpty() || hostname.isEmpty()) {
        updateStatus("IP地址和主机名不能为空", true);
        return;
    }
    
    if (!isValidIP(ip)) {
        updateStatus("IP地址格式不正确", true);
        return;
    }
    
    if (!isValidHostname(hostname)) {
        updateStatus("主机名格式不正确", true);
        return;
    }
    
    // 检查是否已存在
    for (const HostEntry& entry : hostEntries) {
        if (entry.hostname == hostname) {
            updateStatus("主机名已存在", true);
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
    
    updateStatus("已添加新的hosts条目", false);
    entriesCountLabel->setText(QString("条目: %1").arg(hostEntries.size()));
    hasUnsavedChanges = true;
}

void HostsEditor::onAddEntry()
{
    // 添加空条目供用户编辑
    hostEntries.append(HostEntry("127.0.0.1", "example.com", "新条目", true));
    updateTable();
    
    // 选中新添加的行
    int newRow = hostsTable->rowCount() - 1;
    hostsTable->selectRow(newRow);
    hostsTable->scrollToItem(hostsTable->item(newRow, 0));
    
    updateStatus("已添加新条目，请编辑", false);
    entriesCountLabel->setText(QString("条目: %1").arg(hostEntries.size()));
    hasUnsavedChanges = true;
}

void HostsEditor::onRemoveEntry()
{
    QList<int> selectedRows;
    for (QTableWidgetItem* item : hostsTable->selectedItems()) {
        int row = item->row();
        if (!selectedRows.contains(row)) {
            selectedRows.append(row);
        }
    }
    
    if (selectedRows.isEmpty()) {
        updateStatus("请先选择要删除的条目", true);
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
    updateStatus(QString("已删除 %1 个条目").arg(selectedRows.size()), false);
    entriesCountLabel->setText(QString("条目: %1").arg(hostEntries.size()));
    hasUnsavedChanges = true;
}

void HostsEditor::onToggleEntry()
{
    QList<int> selectedRows;
    for (QTableWidgetItem* item : hostsTable->selectedItems()) {
        int row = item->row();
        if (!selectedRows.contains(row)) {
            selectedRows.append(row);
        }
    }
    
    if (selectedRows.isEmpty()) {
        updateStatus("请先选择要启用/禁用的条目", true);
        return;
    }
    
    for (int row : selectedRows) {
        if (row >= 0 && row < hostEntries.size()) {
            hostEntries[row].enabled = !hostEntries[row].enabled;
        }
    }
    
    updateTable();
    updateStatus(QString("已切换 %1 个条目的状态").arg(selectedRows.size()), false);
    hasUnsavedChanges = true;
}

void HostsEditor::onClearAll()
{
    if (hostEntries.isEmpty()) {
        updateStatus("没有条目可清空", true);
        return;
    }
    
    QMessageBox::StandardButton reply = QMessageBox::question(this, 
        "确认清空", 
        QString("确定要清空所有 %1 个hosts条目吗？\n此操作不可撤销！").arg(hostEntries.size()),
        QMessageBox::Yes | QMessageBox::No);
        
    if (reply == QMessageBox::Yes) {
        hostEntries.clear();
        updateTable();
        updateStatus("已清空所有条目", false);
        entriesCountLabel->setText("条目: 0");
        hasUnsavedChanges = true;
    }
}

void HostsEditor::updateTable()
{
    if (isLoading) return;
    
    hostsTable->setRowCount(hostEntries.size());
    
    for (int i = 0; i < hostEntries.size(); ++i) {
        const HostEntry& entry = hostEntries[i];
        
        // 状态列
        QTableWidgetItem* statusItem = new QTableWidgetItem(entry.enabled ? "✅ 启用" : "❌ 禁用");
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

void HostsEditor::updateStatus(const QString& message, bool isError)
{
    statusLabel->setText(message);
    statusLabel->setStyleSheet(isError ? 
        "color: #dc3545; font-weight: bold;" : 
        "color: #28a745; font-weight: bold;");
}

void HostsEditor::updateButtonStates()
{
    bool hasSelection = !hostsTable->selectedItems().isEmpty();
    bool hasEntries = !hostEntries.isEmpty();
    
    removeBtn->setEnabled(hasSelection);
    toggleBtn->setEnabled(hasSelection);
    clearBtn->setEnabled(hasEntries);
    saveBtn->setEnabled(hasUnsavedChanges);
}

QString HostsEditor::getHostsFilePath()
{
#ifdef Q_OS_WIN
    return "C:/Windows/System32/drivers/etc/hosts";
#else
    return "/etc/hosts";
#endif
}

bool HostsEditor::checkAdminRights()
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
    
    adminStatusLabel->setText(isAdmin ? "✅ 管理员权限" : "⚠️ 普通用户权限");
    adminStatusLabel->setStyleSheet(isAdmin ? 
        "color: #28a745; font-weight: bold;" : 
        "color: #ffc107; font-weight: bold;");
    
    return isAdmin;
#else
    bool isRoot = (getuid() == 0);
    adminStatusLabel->setText(isRoot ? "✅ Root权限" : "⚠️ 普通用户权限");
    adminStatusLabel->setStyleSheet(isRoot ? 
        "color: #28a745; font-weight: bold;" : 
        "color: #ffc107; font-weight: bold;");
    return isRoot;
#endif
}

bool HostsEditor::isValidIP(const QString& ip)
{
    QRegularExpression ipRegex("^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$");
    return ipRegex.match(ip).hasMatch();
}

bool HostsEditor::isValidHostname(const QString& hostname)
{
    if (hostname.isEmpty() || hostname.length() > 253) {
        return false;
    }
    
    QRegularExpression hostnameRegex("^[a-zA-Z0-9]([a-zA-Z0-9\\-]{0,61}[a-zA-Z0-9])?(\\.([a-zA-Z0-9]([a-zA-Z0-9\\-]{0,61}[a-zA-Z0-9])?))*$");
    return hostnameRegex.match(hostname).hasMatch();
}

void HostsEditor::onTableItemChanged(QTableWidgetItem* item)
{
    if (isLoading) return;
    
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
                updateStatus("IP地址格式不正确", true);
                item->setText(hostEntries[row].ip); // 恢复原值
            }
            break;
        case 2: // 主机名
            if (isValidHostname(newValue)) {
                hostEntries[row].hostname = newValue;
                hasUnsavedChanges = true;
            } else {
                updateStatus("主机名格式不正确", true);
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

void HostsEditor::onTableSelectionChanged()
{
    updateButtonStates();
}

// 空实现的槽函数
void HostsEditor::onEditEntry() {}
void HostsEditor::onBackupHosts() {}
void HostsEditor::onRestoreHosts() {}
void HostsEditor::onImportHosts() {}
void HostsEditor::onExportHosts() {}
void HostsEditor::onSearchHosts() {}
void HostsEditor::onFileChanged(const QString&) {}
void HostsEditor::onDnsFlushFinished(int, QProcess::ExitStatus) {}
void HostsEditor::parseHostsContent(const QString&) {}
void HostsEditor::addHostEntry(const QString&, const QString&, const QString&, bool) {}
void HostsEditor::removeSelectedEntries() {}
void HostsEditor::toggleSelectedEntries() {}
void HostsEditor::showAdminWarning() {}
bool HostsEditor::requestAdminRights() { return false; }
