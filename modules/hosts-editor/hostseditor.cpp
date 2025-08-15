#include "hostseditor.h"
#include <QDebug>
#include <QDir>
#include <QStandardPaths>

REGISTER_DYNAMICOBJECT(HostsEditor);

HostsEditor::HostsEditor() : QWidget(nullptr), DynamicObjectBase()
{
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // 标题标签
    QLabel *titleLabel = new QLabel("Hosts 文件编辑器");
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #333;");
    mainLayout->addWidget(titleLabel);

    // 按钮布局
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    
    refreshButton = new QPushButton("刷新");
    refreshButton->setFixedSize(80, 30);
    refreshButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #4CAF50;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 4px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background-color: #45a049;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #3d8b40;"
        "}"
    );

    saveButton = new QPushButton("保存");
    saveButton->setFixedSize(80, 30);
    saveButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #008CBA;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 4px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background-color: #007B9A;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #006B8A;"
        "}"
    );

    flushDnsButton = new QPushButton("刷新DNS");
    flushDnsButton->setFixedSize(80, 30);
    flushDnsButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #FF6B35;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 4px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background-color: #E55A2B;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #CC4E21;"
        "}"
    );

    buttonLayout->addWidget(refreshButton);
    buttonLayout->addWidget(saveButton);
    buttonLayout->addWidget(flushDnsButton);
    buttonLayout->addStretch();
    buttonLayout->setAlignment(Qt::AlignLeft);

    mainLayout->addLayout(buttonLayout);

    // Hosts 文件编辑区域
    hostsTextEdit = new QPlainTextEdit;
    hostsTextEdit->setStyleSheet(
        "QPlainTextEdit {"
        "    font-family: 'Consolas', 'Monaco', 'Courier New', monospace;"
        "    font-size: 12px;"
        "    border: 1px solid #ccc;"
        "    border-radius: 4px;"
        "    padding: 8px;"
        "    background-color: #f9f9f9;"
        "}"
    );
    hostsTextEdit->setPlaceholderText("Hosts 文件内容将显示在这里...");
    
    mainLayout->addWidget(hostsTextEdit);

    // 状态标签
    statusLabel = new QLabel("就绪");
    statusLabel->setStyleSheet("color: #666; font-size: 12px;");
    mainLayout->addWidget(statusLabel);

    // 连接信号和槽
    connect(refreshButton, &QPushButton::clicked, this, &HostsEditor::loadHostsFile);
    connect(saveButton, &QPushButton::clicked, this, &HostsEditor::saveHostsFile);
    connect(flushDnsButton, &QPushButton::clicked, this, &HostsEditor::flushDns);

    this->setLayout(mainLayout);

    // 自动加载 hosts 文件
    loadHostsFile();
}

QString HostsEditor::getHostsFilePath()
{
#ifdef Q_OS_WIN
    return "C:/Windows/System32/drivers/etc/hosts";
#elif defined(Q_OS_MAC) || defined(Q_OS_LINUX)
    return "/etc/hosts";
#else
    return "";
#endif
}

void HostsEditor::loadHostsFile()
{
    QString hostsPath = getHostsFilePath();
    if (hostsPath.isEmpty()) {
        showMessage("不支持的操作系统", true);
        return;
    }

    QFile file(hostsPath);
    if (!file.exists()) {
        showMessage("Hosts 文件不存在: " + hostsPath, true);
        return;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        showMessage("无法打开 Hosts 文件，可能需要管理员权限", true);
        return;
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    hostsTextEdit->setPlainText(content);
    showMessage("Hosts 文件加载成功");
}

void HostsEditor::saveHostsFile()
{
    QString hostsPath = getHostsFilePath();
    if (hostsPath.isEmpty()) {
        showMessage("不支持的操作系统", true);
        return;
    }

    QString content = hostsTextEdit->toPlainText();
    showMessage(QString("准备保存到: %1").arg(hostsPath));
    
    // 尝试直接写入
    QFile file(hostsPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << content;
        file.close();
        showMessage("Hosts 文件直接保存成功");
        return;
    }
    
    showMessage("直接保存失败，尝试管理员权限...");

    // 如果直接写入失败，尝试以管理员权限运行
#ifdef Q_OS_WIN
    // 在 Windows 上，创建临时文件并使用管理员权限复制
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString tempFilePath = tempDir + "/hosts_temp.txt";
    
    QFile tempFile(tempFilePath);
    if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        showMessage("无法创建临时文件", true);
        return;
    }
    
    QTextStream out(&tempFile);
    out << content;
    tempFile.close();

    // 使用 Windows API 直接提权运行 cmd 复制命令
    showMessage("正在请求管理员权限...");
    
    // 构建 cmd 复制命令，使用标准化路径
    QString normalizedTempPath = QDir::toNativeSeparators(tempFilePath);
    QString normalizedHostsPath = QDir::toNativeSeparators(hostsPath);
    QString cmdArgs = QString("/c copy /Y \"%1\" \"%2\"").arg(normalizedTempPath, normalizedHostsPath);
    
    showMessage(QString("执行命令: cmd.exe %1").arg(cmdArgs));
    
    // 使用 ShellExecuteEx 以管理员权限运行 cmd 命令
    SHELLEXECUTEINFOW shellInfo;
    memset(&shellInfo, 0, sizeof(shellInfo));
    shellInfo.cbSize = sizeof(shellInfo);
    shellInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
    shellInfo.lpVerb = L"runas";  // 以管理员身份运行
    shellInfo.lpFile = L"cmd.exe";
    shellInfo.lpParameters = reinterpret_cast<LPCWSTR>(cmdArgs.utf16());
    shellInfo.nShow = SW_HIDE;  // 隐藏窗口
    
    if (ShellExecuteExW(&shellInfo)) {
        if (shellInfo.hProcess) {
            // 等待进程完成
            DWORD waitResult = WaitForSingleObject(shellInfo.hProcess, 30000); // 等待30秒
            
            DWORD exitCode = 0;
            GetExitCodeProcess(shellInfo.hProcess, &exitCode);
            CloseHandle(shellInfo.hProcess);
            
            if (waitResult == WAIT_OBJECT_0) {
                // 等待文件系统同步
                QThread::msleep(1000);
                
                // 验证保存是否成功
                QFile checkFile(hostsPath);
                if (checkFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QTextStream checkStream(&checkFile);
                    QString savedContent = checkStream.readAll();
                    checkFile.close();
                    
                    if (savedContent.trimmed() == content.trimmed()) {
                        showMessage("Hosts 文件保存成功！");
                        loadHostsFile(); // 重新加载
                    } else {
                        showMessage(QString("保存可能未完成，退出代码: %1").arg(exitCode), true);
                    }
                } else {
                    if (exitCode == 0) {
                        // 命令成功但无法读取，可能是权限问题
                        QThread::msleep(2000);
                        loadHostsFile();
                        showMessage("保存完成，正在刷新...");
                    } else {
                        showMessage(QString("复制命令失败，退出代码: %1").arg(exitCode), true);
                    }
                }
            } else if (waitResult == WAIT_TIMEOUT) {
                showMessage("保存操作超时", true);
            } else {
                showMessage("保存操作异常结束", true);
            }
        } else {
            showMessage("无法获取进程句柄", true);
        }
    } else {
        DWORD error = GetLastError();
        if (error == ERROR_CANCELLED) {
            showMessage("用户取消了权限提升", true);
        } else {
            showMessage(QString("权限提升失败，错误代码: %1").arg(error), true);
        }
    }
    
    // 清理临时文件
    QFile::remove(tempFilePath);
#else
    // Linux/Mac 系统提示用户使用 sudo
    showMessage("保存失败，请以管理员权限运行程序或使用 sudo", true);
#endif
}

void HostsEditor::flushDns()
{
#ifdef Q_OS_WIN
    // Windows 刷新 DNS
    QProcess process;
    process.start("ipconfig", QStringList() << "/flushdns");
    process.waitForFinished(5000);
    
    if (process.exitCode() == 0) {
        showMessage("DNS 缓存刷新成功");
    } else {
        showMessage("DNS 缓存刷新失败", true);
    }
#elif defined(Q_OS_MAC)
    // macOS 刷新 DNS
    QProcess process;
    process.start("sudo", QStringList() << "dscacheutil" << "-flushcache");
    process.waitForFinished(5000);
    
    if (process.exitCode() == 0) {
        showMessage("DNS 缓存刷新成功");
    } else {
        showMessage("DNS 缓存刷新失败，可能需要管理员权限", true);
    }
#elif defined(Q_OS_LINUX)
    // Linux 刷新 DNS (适用于大多数发行版)
    QProcess process;
    
    // 尝试 systemd-resolve
    process.start("systemd-resolve", QStringList() << "--flush-caches");
    process.waitForFinished(3000);
    
    if (process.exitCode() == 0) {
        showMessage("DNS 缓存刷新成功");
        return;
    }
    
    // 尝试重启 nscd
    process.start("sudo", QStringList() << "systemctl" << "restart" << "nscd");
    process.waitForFinished(3000);
    
    if (process.exitCode() == 0) {
        showMessage("DNS 缓存刷新成功");
    } else {
        showMessage("DNS 缓存刷新失败，可能需要管理员权限", true);
    }
#else
    showMessage("不支持的操作系统", true);
#endif
}

bool HostsEditor::isRunningAsAdmin()
{
#ifdef Q_OS_WIN
    // 在 Windows 上检查是否以管理员身份运行
    QProcess process;
    process.start("net", QStringList() << "session");
    process.waitForFinished(1000);
    return process.exitCode() == 0;
#else
    // 在 Unix 系统上检查是否为 root 用户
    return geteuid() == 0;
#endif
}



void HostsEditor::showMessage(const QString &message, bool isError)
{
    if (isError) {
        statusLabel->setStyleSheet("color: #d32f2f; font-size: 12px;");
    } else {
        statusLabel->setStyleSheet("color: #388e3c; font-size: 12px;");
    }
    statusLabel->setText(message);
}
