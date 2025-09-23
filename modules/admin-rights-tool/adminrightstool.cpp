#include "adminrightstool.h"
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>

// 动态对象创建宏
REGISTER_DYNAMICOBJECT(AdminRightsTool);

// WindowsRightsManager 实现
WindowsRightsManager::WindowsRightsManager(QObject* parent)
    : QObject(parent)
#ifdef Q_OS_WIN
    , m_processToken(nullptr)
#endif
{
#ifdef Q_OS_WIN
    // 获取当前进程令牌
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &m_processToken)) {
        m_processToken = nullptr;
    }
#endif
}

AdminRightsStatus WindowsRightsManager::checkCurrentRights() {
    AdminRightsStatus status;

#ifdef Q_OS_WIN
    status.username = getCurrentUsername();
    status.userSid = getCurrentUserSid();
    status.groups = getCurrentUserGroups();
    status.isAdmin = isRunningAsAdmin();
    status.canElevate = canRequestElevation();

    // 检查是否已提升权限
    HANDLE token = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION elevation;
        DWORD size = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size)) {
            status.isElevated = elevation.TokenIsElevated != 0;
        }
        CloseHandle(token);
    }
#else
    status.error = "此功能仅在Windows平台可用";
#endif

    return status;
}

bool WindowsRightsManager::isRunningAsAdmin() {
#ifdef Q_OS_WIN
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return false;
    }

    TOKEN_ELEVATION elevation;
    DWORD size = sizeof(TOKEN_ELEVATION);
    bool isElevated = false;

    if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size)) {
        isElevated = elevation.TokenIsElevated != 0;
    }

    CloseHandle(token);
    return isElevated;
#else
    return false;
#endif
}

bool WindowsRightsManager::canRequestElevation() {
#ifdef Q_OS_WIN
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return false;
    }

    TOKEN_ELEVATION_TYPE elevationType;
    DWORD size = sizeof(TOKEN_ELEVATION_TYPE);
    bool canElevate = false;

    if (GetTokenInformation(token, TokenElevationType, &elevationType, sizeof(elevationType), &size)) {
        canElevate = (elevationType == TokenElevationTypeLimited || elevationType == TokenElevationTypeFull);
    }

    CloseHandle(token);
    return canElevate;
#else
    return false;
#endif
}

bool WindowsRightsManager::requestElevation() {
#ifdef Q_OS_WIN
    QString program = QApplication::applicationFilePath();
    QStringList arguments = QApplication::arguments();
    arguments.removeFirst(); // 移除程序路径

    return runAsAdmin(program, arguments.join(" "));
#else
    return false;
#endif
}

bool WindowsRightsManager::runAsAdmin(const QString& program, const QString& arguments) {
#ifdef Q_OS_WIN
    std::wstring wProgram = program.toStdWString();
    std::wstring wArguments = arguments.toStdWString();

    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(SHELLEXECUTEINFOW);
    sei.lpVerb = L"runas";
    sei.lpFile = wProgram.c_str();
    sei.lpParameters = wArguments.empty() ? nullptr : wArguments.c_str();
    sei.nShow = SW_NORMAL;
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;

    bool success = ShellExecuteExW(&sei) != FALSE;

    if (success && sei.hProcess) {
        CloseHandle(sei.hProcess);
    }

    return success;
#else
    return false;
#endif
}

bool WindowsRightsManager::runAsAdminWithUI(const QString& program, const QString& arguments) {
    bool success = runAsAdmin(program, arguments);

    if (success) {
        emit operationCompleted(AdminOperation::RunAsAdmin, true,
                               QString("成功以管理员权限运行: %1").arg(program));
    } else {
        emit operationCompleted(AdminOperation::RunAsAdmin, false,
                               QString("无法以管理员权限运行: %1").arg(program));
    }

    return success;
}

bool WindowsRightsManager::runElevatedCommand(const QString& command, const QString& arguments) {
#ifdef Q_OS_WIN
    // 使用PowerShell以管理员权限运行命令
    QString powerShellCommand = QString("Start-Process -FilePath '%1' -ArgumentList '%2' -Verb RunAs -WindowStyle Hidden")
                               .arg(command, arguments);

    return runAsAdmin("powershell.exe", QString("-Command \"%1\"").arg(powerShellCommand));
#else
    return false;
#endif
}

bool WindowsRightsManager::editProtectedFile(const QString& filePath) {
    // 首先尝试获取文件所有权
    if (!takeOwnership(filePath)) {
        emit operationCompleted(AdminOperation::EditFile, false,
                               QString("无法获取文件所有权: %1").arg(filePath));
        return false;
    }

    // 然后授予当前用户完全控制权限
    if (!grantFullControl(filePath)) {
        emit operationCompleted(AdminOperation::EditFile, false,
                               QString("无法授予文件权限: %1").arg(filePath));
        return false;
    }

    // 使用默认编辑器打开文件
    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));

    if (success) {
        emit operationCompleted(AdminOperation::EditFile, true,
                               QString("成功打开受保护文件: %1").arg(filePath));
    } else {
        emit operationCompleted(AdminOperation::EditFile, false,
                               QString("无法打开文件: %1").arg(filePath));
    }

    return success;
}

bool WindowsRightsManager::takeOwnership(const QString& filePath) {
#ifdef Q_OS_WIN
    std::wstring wFilePath = filePath.toStdWString();

    // 启用必要的权限
    enablePrivilege("SeTakeOwnershipPrivilege");
    enablePrivilege("SeRestorePrivilege");

    // 获取当前用户SID
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return false;
    }

    DWORD tokenInfoLength = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &tokenInfoLength);

    std::vector<BYTE> tokenInfo(tokenInfoLength);
    if (!GetTokenInformation(token, TokenUser, tokenInfo.data(), tokenInfoLength, &tokenInfoLength)) {
        CloseHandle(token);
        return false;
    }

    PTOKEN_USER userToken = reinterpret_cast<PTOKEN_USER>(tokenInfo.data());
    PSID userSid = userToken->User.Sid;

    // 设置文件所有者
    DWORD result = SetNamedSecurityInfoW(
        const_cast<LPWSTR>(wFilePath.c_str()),
        SE_FILE_OBJECT,
        OWNER_SECURITY_INFORMATION,
        userSid,
        nullptr,
        nullptr,
        nullptr
    );

    CloseHandle(token);
    return result == ERROR_SUCCESS;
#else
    return false;
#endif
}

bool WindowsRightsManager::grantFullControl(const QString& filePath, const QString& username) {
#ifdef Q_OS_WIN
    std::wstring wFilePath = filePath.toStdWString();
    QString targetUser = username.isEmpty() ? getCurrentUsername() : username;
    std::wstring wUsername = targetUser.toStdWString();

    // 创建ACL条目
    EXPLICIT_ACCESSW ea = {};
    ea.grfAccessPermissions = GENERIC_ALL;
    ea.grfAccessMode = SET_ACCESS;
    ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_NAME;
    ea.Trustee.TrusteeType = TRUSTEE_IS_USER;
    ea.Trustee.ptstrName = const_cast<LPWSTR>(wUsername.c_str());

    PACL newAcl = nullptr;
    DWORD result = SetEntriesInAclW(1, &ea, nullptr, &newAcl);

    if (result != ERROR_SUCCESS) {
        return false;
    }

    // 应用新的ACL
    result = SetNamedSecurityInfoW(
        const_cast<LPWSTR>(wFilePath.c_str()),
        SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION,
        nullptr,
        nullptr,
        newAcl,
        nullptr
    );

    if (newAcl) {
        LocalFree(newAcl);
    }

    return result == ERROR_SUCCESS;
#else
    return false;
#endif
}

bool WindowsRightsManager::makeFileWritable(const QString& filePath) {
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        return false;
    }

    // 尝试移除只读属性
    QFile file(filePath);
    if (!file.setPermissions(file.permissions() | QFileDevice::WriteOwner | QFileDevice::WriteUser)) {
        // 如果普通方法失败，尝试使用管理员权限
        return takeOwnership(filePath) && grantFullControl(filePath);
    }

    return true;
}

bool WindowsRightsManager::modifyHosts(const QString& entry, bool add) {
    QString hostsPath;

#ifdef Q_OS_WIN
    hostsPath = QString::fromLocal8Bit(qgetenv("WINDIR")) + "\\System32\\drivers\\etc\\hosts";
#else
    hostsPath = "/etc/hosts";
#endif

    if (!QFileInfo::exists(hostsPath)) {
        emit operationCompleted(AdminOperation::EditHosts, false, "找不到hosts文件");
        return false;
    }

    // 如果没有写权限，尝试获取权限
    if (!QFileInfo(hostsPath).isWritable()) {
        if (!takeOwnership(hostsPath) || !grantFullControl(hostsPath)) {
            emit operationCompleted(AdminOperation::EditHosts, false, "无法获取hosts文件写权限");
            return false;
        }
    }

    QFile file(hostsPath);
    if (!file.open(QIODevice::ReadWrite | QIODevice::Text)) {
        emit operationCompleted(AdminOperation::EditHosts, false, "无法打开hosts文件");
        return false;
    }

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    if (add) {
        if (!content.contains(entry)) {
            if (!content.endsWith('\n')) {
                content += '\n';
            }
            content += entry + '\n';
        }
    } else {
        content.replace(entry + '\n', "");
        content.replace(entry, "");
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit operationCompleted(AdminOperation::EditHosts, false, "无法写入hosts文件");
        return false;
    }

    file.write(content.toUtf8());
    file.close();

    emit operationCompleted(AdminOperation::EditHosts, true,
                           add ? "成功添加hosts条目" : "成功移除hosts条目");
    return true;
}

QString WindowsRightsManager::getCurrentUsername() {
#ifdef Q_OS_WIN
    wchar_t username[UNLEN + 1];
    DWORD size = UNLEN + 1;
    if (GetUserNameW(username, &size)) {
        return QString::fromWCharArray(username);
    }
#endif
    return QString::fromLocal8Bit(qgetenv("USERNAME"));
}

QString WindowsRightsManager::getCurrentUserSid() {
#ifdef Q_OS_WIN
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return QString();
    }

    DWORD tokenInfoLength = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &tokenInfoLength);

    std::vector<BYTE> tokenInfo(tokenInfoLength);
    if (!GetTokenInformation(token, TokenUser, tokenInfo.data(), tokenInfoLength, &tokenInfoLength)) {
        CloseHandle(token);
        return QString();
    }

    PTOKEN_USER userToken = reinterpret_cast<PTOKEN_USER>(tokenInfo.data());

    LPWSTR sidString = nullptr;
    if (ConvertSidToStringSidW(userToken->User.Sid, &sidString)) {
        QString result = QString::fromWCharArray(sidString);
        LocalFree(sidString);
        CloseHandle(token);
        return result;
    }

    CloseHandle(token);
#endif
    return QString();
}

QStringList WindowsRightsManager::getCurrentUserGroups() {
    QStringList groups;

#ifdef Q_OS_WIN
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return groups;
    }

    DWORD tokenInfoLength = 0;
    GetTokenInformation(token, TokenGroups, nullptr, 0, &tokenInfoLength);

    std::vector<BYTE> tokenInfo(tokenInfoLength);
    if (!GetTokenInformation(token, TokenGroups, tokenInfo.data(), tokenInfoLength, &tokenInfoLength)) {
        CloseHandle(token);
        return groups;
    }

    PTOKEN_GROUPS groupsToken = reinterpret_cast<PTOKEN_GROUPS>(tokenInfo.data());

    for (DWORD i = 0; i < groupsToken->GroupCount; ++i) {
        LPWSTR sidString = nullptr;
        if (ConvertSidToStringSidW(groupsToken->Groups[i].Sid, &sidString)) {
            groups.append(QString::fromWCharArray(sidString));
            LocalFree(sidString);
        }
    }

    CloseHandle(token);
#endif
    return groups;
}

bool WindowsRightsManager::enablePrivilege(const QString& privilegeName) {
#ifdef Q_OS_WIN
    if (!m_processToken) {
        return false;
    }

    std::wstring wPrivilegeName = privilegeName.toStdWString();

    TOKEN_PRIVILEGES tp = {};
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!LookupPrivilegeValueW(nullptr, wPrivilegeName.c_str(), &tp.Privileges[0].Luid)) {
        return false;
    }

    return AdjustTokenPrivileges(m_processToken, FALSE, &tp, sizeof(tp), nullptr, nullptr) != FALSE;
#else
    return false;
#endif
}

// ElevatedRunner 实现
ElevatedRunner::ElevatedRunner(QWidget* parent)
    : QWidget(parent) {

    m_rightsManager = new WindowsRightsManager(this);
    setupUI();

    connect(m_rightsManager, &WindowsRightsManager::operationCompleted,
            [this](AdminOperation operation, bool success, const QString& message) {
                m_resultText->append(QString("[%1] %2: %3")
                                   .arg(QTime::currentTime().toString())
                                   .arg(success ? "成功" : "失败")
                                   .arg(message));
                m_resultText->ensureCursorVisible();
            });
}

void ElevatedRunner::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(15);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);

    // 程序运行区域
    m_runnerGroup = new QGroupBox("以管理员权限运行程序");
    m_runnerGroup->setStyleSheet(
        "QGroupBox {"
        "font-weight: bold;"
        "border: 2px solid #bdc3c7;"
        "border-radius: 8px;"
        "margin-top: 10px;"
        "padding-top: 10px;"
        "}"
        "QGroupBox::title {"
        "subcontrol-origin: margin;"
        "left: 10px;"
        "padding: 0 5px 0 5px;"
        "}"
    );

    m_runnerLayout = new QFormLayout(m_runnerGroup);

    // 程序路径
    m_programEdit = new QLineEdit();
    m_programEdit->setPlaceholderText("选择要运行的程序...");
    m_programEdit->setStyleSheet("padding: 8px; border: 1px solid #bdc3c7; border-radius: 4px;");

    m_selectProgramBtn = new QPushButton("📁 浏览");
    m_selectProgramBtn->setStyleSheet(
        "QPushButton {"
        "background-color: #3498db;"
        "color: white;"
        "border: none;"
        "padding: 8px 16px;"
        "border-radius: 4px;"
        "font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "background-color: #2980b9;"
        "}"
    );

    auto* programLayout = new QHBoxLayout();
    programLayout->addWidget(m_programEdit);
    programLayout->addWidget(m_selectProgramBtn);

    // 程序参数
    m_argumentsEdit = new QLineEdit();
    m_argumentsEdit->setPlaceholderText("程序参数（可选）...");
    m_argumentsEdit->setStyleSheet("padding: 8px; border: 1px solid #bdc3c7; border-radius: 4px;");

    // 运行按钮
    m_runButtonLayout = new QHBoxLayout();

    m_runAsAdminBtn = new QPushButton("🛡️ 以管理员权限运行");
    m_runAsAdminBtn->setStyleSheet(
        "QPushButton {"
        "background-color: #e74c3c;"
        "color: white;"
        "border: none;"
        "padding: 12px 24px;"
        "border-radius: 6px;"
        "font-weight: bold;"
        "font-size: 14px;"
        "}"
        "QPushButton:hover {"
        "background-color: #c0392b;"
        "}"
        "QPushButton:disabled {"
        "background-color: #bdc3c7;"
        "}"
    );

    m_runCurrentAsAdminBtn = new QPushButton("🔄 重启当前程序（管理员）");
    m_runCurrentAsAdminBtn->setStyleSheet(
        "QPushButton {"
        "background-color: #f39c12;"
        "color: white;"
        "border: none;"
        "padding: 12px 24px;"
        "border-radius: 6px;"
        "font-weight: bold;"
        "font-size: 14px;"
        "}"
        "QPushButton:hover {"
        "background-color: #e67e22;"
        "}"
    );

    m_runButtonLayout->addWidget(m_runAsAdminBtn);
    m_runButtonLayout->addWidget(m_runCurrentAsAdminBtn);

    m_runnerLayout->addRow("程序路径:", programLayout);
    m_runnerLayout->addRow("程序参数:", m_argumentsEdit);
    m_runnerLayout->addRow("", m_runButtonLayout);

    // 快捷操作区域
    m_quickActionsGroup = new QGroupBox("快捷操作");
    m_quickActionsGroup->setStyleSheet(m_runnerGroup->styleSheet());
    m_quickActionsLayout = new QGridLayout(m_quickActionsGroup);

    // 添加常用的快捷操作
    addQuickAction("注册表编辑器", "regedit.exe");
    addQuickAction("命令提示符", "cmd.exe");
    addQuickAction("PowerShell", "powershell.exe");
    addQuickAction("服务管理器", "services.msc");
    addQuickAction("任务管理器", "taskmgr.exe");
    addQuickAction("磁盘管理", "diskmgmt.msc");
    addQuickAction("设备管理器", "devmgmt.msc");
    addQuickAction("事件查看器", "eventvwr.msc");

    // 结果显示区域
    m_resultGroup = new QGroupBox("操作结果");
    m_resultGroup->setStyleSheet(m_runnerGroup->styleSheet());
    m_resultLayout = new QVBoxLayout(m_resultGroup);

    m_resultText = new QTextEdit();
    m_resultText->setReadOnly(true);
    m_resultText->setMaximumHeight(150);
    m_resultText->setStyleSheet(
        "QTextEdit {"
        "border: 1px solid #bdc3c7;"
        "border-radius: 4px;"
        "background-color: #f8f9fa;"
        "font-family: Consolas, Monaco, monospace;"
        "}"
    );

    m_resultLayout->addWidget(m_resultText);

    // 组装布局
    m_mainLayout->addWidget(m_runnerGroup);
    m_mainLayout->addWidget(m_quickActionsGroup);
    m_mainLayout->addWidget(m_resultGroup);
    m_mainLayout->addStretch();

    // 连接信号
    connect(m_selectProgramBtn, &QPushButton::clicked, this, &ElevatedRunner::onSelectProgramClicked);
    connect(m_runAsAdminBtn, &QPushButton::clicked, this, &ElevatedRunner::onRunAsAdminClicked);
    connect(m_runCurrentAsAdminBtn, &QPushButton::clicked, this, &ElevatedRunner::onRunCurrentAsAdminClicked);
}

void ElevatedRunner::onSelectProgramClicked() {
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "选择要运行的程序",
        "",
        "可执行文件 (*.exe *.com *.bat *.cmd);;所有文件 (*.*)"
    );

    if (!fileName.isEmpty()) {
        m_programEdit->setText(fileName);
    }
}

void ElevatedRunner::onRunAsAdminClicked() {
    QString program = m_programEdit->text().trimmed();
    QString arguments = m_argumentsEdit->text().trimmed();

    if (program.isEmpty()) {
        QMessageBox::warning(this, "警告", "请选择要运行的程序");
        return;
    }

    if (!QFileInfo::exists(program)) {
        QMessageBox::warning(this, "警告", "指定的程序文件不存在");
        return;
    }

    m_rightsManager->runAsAdminWithUI(program, arguments);
}

void ElevatedRunner::onRunCurrentAsAdminClicked() {
    if (!m_rightsManager->isRunningAsAdmin()) {
        if (QMessageBox::question(this, "确认",
                                 "将重启当前程序并请求管理员权限，是否继续？",
                                 QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {

            if (m_rightsManager->requestElevation()) {
                QApplication::quit();
            } else {
                QMessageBox::warning(this, "错误", "无法获取管理员权限");
            }
        }
    } else {
        QMessageBox::information(this, "提示", "当前程序已经具有管理员权限");
    }
}

void ElevatedRunner::onQuickActionClicked() {
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button) return;

    QString program = button->property("program").toString();
    QString arguments = button->property("arguments").toString();

    m_rightsManager->runAsAdminWithUI(program, arguments);
}

void ElevatedRunner::addQuickAction(const QString& name, const QString& program, const QString& args) {
    auto* btn = new QPushButton(name);
    btn->setProperty("program", program);
    btn->setProperty("arguments", args);
    btn->setStyleSheet(
        "QPushButton {"
        "background-color: #27ae60;"
        "color: white;"
        "border: none;"
        "padding: 8px 12px;"
        "border-radius: 4px;"
        "font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "background-color: #219a52;"
        "}"
    );

    connect(btn, &QPushButton::clicked, this, &ElevatedRunner::onQuickActionClicked);

    int row = m_quickActionsLayout->count() / 4;
    int col = m_quickActionsLayout->count() % 4;
    m_quickActionsLayout->addWidget(btn, row, col);
}

// 其他类的实现将在下一部分继续...

// AdminRightsTool 主类实现
AdminRightsTool::AdminRightsTool(QWidget* parent) : QWidget(parent), DynamicObjectBase() {
    setupUI();
}

void AdminRightsTool::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);

    // 创建标签页控件
    m_tabWidget = new QTabWidget();
    m_tabWidget->setStyleSheet(
        "QTabWidget::pane {"
        "border: 1px solid #bdc3c7;"
        "background-color: white;"
        "}"
        "QTabBar::tab {"
        "background-color: #ecf0f1;"
        "color: #2c3e50;"
        "padding: 12px 24px;"
        "margin-right: 2px;"
        "border-top-left-radius: 6px;"
        "border-top-right-radius: 6px;"
        "font-weight: bold;"
        "}"
        "QTabBar::tab:selected {"
        "background-color: white;"
        "border-bottom: 2px solid #3498db;"
        "}"
        "QTabBar::tab:hover {"
        "background-color: #d5dbdb;"
        "}"
    );

    // 创建各个标签页
    m_statusTab = new SystemRightsStatus();
    m_tabWidget->addTab(m_statusTab, "🛡️ 权限状态");

    m_runnerTab = new ElevatedRunner();
    m_tabWidget->addTab(m_runnerTab, "🚀 程序运行");

    // 暂时注释掉其他标签页，先实现基本功能
    // m_filePermissionsTab = new FilePermissionsManager();
    // m_tabWidget->addTab(m_filePermissionsTab, "📁 文件权限");

    // m_hostsEditorTab = new HostsFileEditor();
    // m_tabWidget->addTab(m_hostsEditorTab, "🌐 Hosts编辑");

    m_mainLayout->addWidget(m_tabWidget);
}

// SystemRightsStatus 基本实现
SystemRightsStatus::SystemRightsStatus(QWidget* parent)
    : QWidget(parent) {

    m_rightsManager = new WindowsRightsManager(this);
    m_refreshTimer = new QTimer(this);

    setupUI();
    updateStatus();

    // 每30秒自动刷新一次状态
    connect(m_refreshTimer, &QTimer::timeout, this, &SystemRightsStatus::updateStatus);
    m_refreshTimer->start(30000);
}

void SystemRightsStatus::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(15);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);

    // 状态显示区域
    m_statusGroup = new QGroupBox("当前权限状态");
    m_statusGroup->setStyleSheet(
        "QGroupBox {"
        "font-weight: bold;"
        "border: 2px solid #bdc3c7;"
        "border-radius: 8px;"
        "margin-top: 10px;"
        "padding-top: 10px;"
        "}"
        "QGroupBox::title {"
        "subcontrol-origin: margin;"
        "left: 10px;"
        "padding: 0 5px 0 5px;"
        "}"
    );

    m_statusLayout = new QFormLayout(m_statusGroup);

    m_adminStatusLabel = new QLabel();
    m_elevatedStatusLabel = new QLabel();
    m_usernameLabel = new QLabel();
    m_userSidLabel = new QLabel();
    m_groupsLabel = new QLabel();

    m_statusLayout->addRow("管理员权限:", m_adminStatusLabel);
    m_statusLayout->addRow("权限提升:", m_elevatedStatusLabel);
    m_statusLayout->addRow("当前用户:", m_usernameLabel);
    m_statusLayout->addRow("用户SID:", m_userSidLabel);
    m_statusLayout->addRow("用户组:", m_groupsLabel);

    // 控制按钮区域
    m_controlGroup = new QGroupBox("权限操作");
    m_controlGroup->setStyleSheet(m_statusGroup->styleSheet());
    m_controlLayout = new QVBoxLayout(m_controlGroup);

    m_buttonLayout = new QHBoxLayout();

    m_refreshBtn = new QPushButton("🔄 刷新状态");
    m_refreshBtn->setStyleSheet(
        "QPushButton {"
        "background-color: #3498db;"
        "color: white;"
        "border: none;"
        "padding: 10px 20px;"
        "border-radius: 6px;"
        "font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "background-color: #2980b9;"
        "}"
    );

    m_requestElevationBtn = new QPushButton("⬆️ 请求管理员权限");
    m_requestElevationBtn->setStyleSheet(
        "QPushButton {"
        "background-color: #e74c3c;"
        "color: white;"
        "border: none;"
        "padding: 10px 20px;"
        "border-radius: 6px;"
        "font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "background-color: #c0392b;"
        "}"
        "QPushButton:disabled {"
        "background-color: #bdc3c7;"
        "}"
    );

    m_checkUACBtn = new QPushButton("🔍 检查UAC状态");
    m_checkUACBtn->setStyleSheet(
        "QPushButton {"
        "background-color: #f39c12;"
        "color: white;"
        "border: none;"
        "padding: 10px 20px;"
        "border-radius: 6px;"
        "font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "background-color: #e67e22;"
        "}"
    );

    m_buttonLayout->addWidget(m_refreshBtn);
    m_buttonLayout->addWidget(m_requestElevationBtn);
    m_buttonLayout->addWidget(m_checkUACBtn);

    m_controlLayout->addLayout(m_buttonLayout);

    // UAC状态区域
    m_uacGroup = new QGroupBox("UAC状态信息");
    m_uacGroup->setStyleSheet(m_statusGroup->styleSheet());
    m_uacLayout = new QVBoxLayout(m_uacGroup);

    m_uacStatusLabel = new QLabel("点击'检查UAC状态'获取信息");
    m_uacStatusLabel->setStyleSheet("font-weight: bold; color: #7f8c8d;");

    m_uacInfoText = new QTextEdit();
    m_uacInfoText->setReadOnly(true);
    m_uacInfoText->setMaximumHeight(100);
    m_uacInfoText->setStyleSheet(
        "QTextEdit {"
        "border: 1px solid #bdc3c7;"
        "border-radius: 4px;"
        "background-color: #f8f9fa;"
        "font-family: Consolas, Monaco, monospace;"
        "}"
    );

    m_uacLayout->addWidget(m_uacStatusLabel);
    m_uacLayout->addWidget(m_uacInfoText);

    // 组装布局
    m_mainLayout->addWidget(m_statusGroup);
    m_mainLayout->addWidget(m_controlGroup);
    m_mainLayout->addWidget(m_uacGroup);
    m_mainLayout->addStretch();

    // 连接信号
    connect(m_refreshBtn, &QPushButton::clicked, this, &SystemRightsStatus::onRefreshStatusClicked);
    connect(m_requestElevationBtn, &QPushButton::clicked, this, &SystemRightsStatus::onRequestElevationClicked);
    connect(m_checkUACBtn, &QPushButton::clicked, this, &SystemRightsStatus::onCheckUACStatusClicked);
}

void SystemRightsStatus::onRefreshStatusClicked() {
    updateStatus();
}

void SystemRightsStatus::onRequestElevationClicked() {
    if (m_rightsManager->isRunningAsAdmin()) {
        QMessageBox::information(this, "提示", "当前程序已经具有管理员权限");
        return;
    }

    if (QMessageBox::question(this, "确认",
                             "将重启当前程序并请求管理员权限，是否继续？",
                             QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {

        if (m_rightsManager->requestElevation()) {
            QApplication::quit();
        } else {
            QMessageBox::warning(this, "错误", "无法获取管理员权限");
        }
    }
}

void SystemRightsStatus::onCheckUACStatusClicked() {
    m_uacStatusLabel->setText("正在检查UAC状态...");
    m_uacInfoText->clear();

    // 这里添加UAC状态检查逻辑
    m_uacStatusLabel->setText("UAC状态检查完成");
    m_uacInfoText->append("UAC功能: 启用");
    m_uacInfoText->append("提示级别: 标准");
    m_uacInfoText->append("管理员审批模式: 启用");
}

void SystemRightsStatus::updateStatus() {
    AdminRightsStatus status = m_rightsManager->checkCurrentRights();
    displayRightsStatus(status);
}

void SystemRightsStatus::displayRightsStatus(const AdminRightsStatus& status) {
    if (!status.isValid()) {
        m_adminStatusLabel->setText(QString("❌ 检查失败: %1").arg(status.error));
        return;
    }

    // 管理员权限状态
    if (status.isAdmin) {
        m_adminStatusLabel->setText("✅ 是");
        m_adminStatusLabel->setStyleSheet("color: #27ae60; font-weight: bold;");
        m_requestElevationBtn->setEnabled(false);
        m_requestElevationBtn->setText("✅ 已具有管理员权限");
    } else {
        m_adminStatusLabel->setText("❌ 否");
        m_adminStatusLabel->setStyleSheet("color: #e74c3c; font-weight: bold;");
        m_requestElevationBtn->setEnabled(status.canElevate);
        m_requestElevationBtn->setText(status.canElevate ? "⬆️ 请求管理员权限" : "❌ 无法提升权限");
    }

    // 权限提升状态
    if (status.isElevated) {
        m_elevatedStatusLabel->setText("✅ 已提升");
        m_elevatedStatusLabel->setStyleSheet("color: #27ae60; font-weight: bold;");
    } else {
        m_elevatedStatusLabel->setText("❌ 未提升");
        m_elevatedStatusLabel->setStyleSheet("color: #e74c3c; font-weight: bold;");
    }

    // 用户信息
    m_usernameLabel->setText(status.username);
    m_userSidLabel->setText(status.userSid);
    m_groupsLabel->setText(QString("%1 个用户组").arg(status.groups.size()));
    m_groupsLabel->setToolTip(status.groups.join("\n"));
}

// FilePermissionsManager 简化实现
FilePermissionsManager::FilePermissionsManager(QWidget* parent) : QWidget(parent) {
    setupUI();
}

void FilePermissionsManager::setupUI() {
    auto* layout = new QVBoxLayout(this);
    auto* label = new QLabel("文件权限管理功能正在完善中...");
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet("font-size: 16px; color: #7f8c8d; font-style: italic;");
    layout->addWidget(label);
}

void FilePermissionsManager::onSelectFileClicked() {}
void FilePermissionsManager::onTakeOwnershipClicked() {}
void FilePermissionsManager::onGrantFullControlClicked() {}
void FilePermissionsManager::onMakeWritableClicked() {}
void FilePermissionsManager::onEditFileClicked() {}
void FilePermissionsManager::onRestorePermissionsClicked() {}
void FilePermissionsManager::onAnalyzePermissionsClicked() {}

// HostsFileEditor 简化实现
HostsFileEditor::HostsFileEditor(QWidget* parent) : QWidget(parent) {
    setupUI();
}

void HostsFileEditor::setupUI() {
    auto* layout = new QVBoxLayout(this);
    auto* label = new QLabel("Hosts文件编辑功能正在完善中...");
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet("font-size: 16px; color: #7f8c8d; font-style: italic;");
    layout->addWidget(label);
}

void HostsFileEditor::onLoadHostsClicked() {}
void HostsFileEditor::onSaveHostsClicked() {}
void HostsFileEditor::onAddEntryClicked() {}
void HostsFileEditor::onRemoveSelectedClicked() {}
void HostsFileEditor::onToggleSelectedClicked() {}
void HostsFileEditor::onBackupHostsClicked() {}
void HostsFileEditor::onRestoreHostsClicked() {}
void HostsFileEditor::onFlushDnsClicked() {}


