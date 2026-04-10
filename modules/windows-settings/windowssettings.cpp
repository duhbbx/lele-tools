#include "windowssettings.h"
#include <QApplication>
#include <QMessageBox>
#include <QProcess>
#include <QTimer>
#ifdef Q_OS_WIN
#include <Windows.h>
#endif

REGISTER_DYNAMICOBJECT(WindowsSettings);

WindowsSettings::WindowsSettings(QWidget* parent)
    : QWidget(parent), DynamicObjectBase(), m_isAdmin(false), m_process(nullptr) {

    setupUI();
    updateAdminStatus();
}

WindowsSettings::~WindowsSettings() {
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
}

void WindowsSettings::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(20);
    m_mainLayout->setContentsMargins(30, 30, 30, 30);

    // 头部区域
    m_titleLabel = new QLabel(tr("⚙️ Windows 系统设置"));
    m_titleLabel->setStyleSheet("font-size: 24pt; font-weight: bold; color: #2c3e50; margin-bottom: 10px;");
    m_titleLabel->setAlignment(Qt::AlignCenter);

    // 管理员状态标签
    m_adminStatusLabel = new QLabel();
    m_adminStatusLabel->setAlignment(Qt::AlignCenter);
    m_adminStatusLabel->setStyleSheet("font-size: 14pt; padding: 10px; border-radius: 8px; margin: 10px 0;");

    // 获取管理员权限按钮
    m_adminRequestBtn = new QPushButton(tr("🔐 获取管理员权限"));
    m_adminRequestBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #3498db;
            color: white;
            border: none;
            padding: 15px 30px;
            font-size: 14pt;
            font-weight: bold;
            border-radius: 8px;
            min-width: 200px;
        }
        QPushButton:hover {
            background-color: #2980b9;
        }
        QPushButton:pressed {
            background-color: #21618c;
        }
        QPushButton:disabled {
            background-color: #bdc3c7;
        }
    )");
    connect(m_adminRequestBtn, &QPushButton::clicked, this, &WindowsSettings::onRequestAdminPrivileges);

    // 测试功能按钮
    m_testBtn = new QPushButton(tr("🧪 测试功能"));
    m_testBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #27ae60;
            color: white;
            border: none;
            padding: 15px 30px;
            font-size: 14pt;
            font-weight: bold;
            border-radius: 8px;
            min-width: 200px;
        }
        QPushButton:hover {
            background-color: #229954;
        }
        QPushButton:pressed {
            background-color: #1e8449;
        }
    )");
    connect(m_testBtn, &QPushButton::clicked, this, &WindowsSettings::onTestFeature);

    // 添加到布局
    m_mainLayout->addWidget(m_titleLabel);
    m_mainLayout->addWidget(m_adminStatusLabel);

    // 按钮居中布局
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_adminRequestBtn);
    buttonLayout->addWidget(m_testBtn);
    buttonLayout->addStretch();

    m_mainLayout->addLayout(buttonLayout);

    // 添加 RunAs 功能区域
    setupRunAsAdminSection();

    m_mainLayout->addStretch();

    // 设置整体样式
    setStyleSheet(R"(
        QWidget {
            background-color: #f8f9fa;
        }
    )");
}

void WindowsSettings::updateAdminStatus() {
    m_isAdmin = isRunningAsAdmin();

    if (m_isAdmin) {
        m_adminStatusLabel->setText("✅ 当前运行在管理员权限下");
        m_adminStatusLabel->setStyleSheet("background-color: #d4edda; color: #155724; font-size: 14pt; padding: 10px; border-radius: 8px; margin: 10px 0;");
        m_adminRequestBtn->setText("✅ 已获得管理员权限");
        m_adminRequestBtn->setEnabled(false);
    } else {
        m_adminStatusLabel->setText("⚠️ 当前运行在普通用户权限下");
        m_adminStatusLabel->setStyleSheet("background-color: #fff3cd; color: #856404; font-size: 14pt; padding: 10px; border-radius: 8px; margin: 10px 0;");
        m_adminRequestBtn->setText("🔐 获取管理员权限");
        m_adminRequestBtn->setEnabled(true);
    }
}

bool WindowsSettings::isRunningAsAdmin() {
#ifdef Q_OS_WIN
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }

    return isAdmin == TRUE;
#else
    return false;
#endif
}

void WindowsSettings::onRequestAdminPrivileges() {
#ifdef Q_OS_WIN
    // 获取当前程序路径
    QString currentPath = QApplication::applicationFilePath();

    // 使用ShellExecuteW以管理员身份重启程序
    LPCWSTR operation = L"runas";
    std::wstring pathW = currentPath.toStdWString();

    HINSTANCE result = ShellExecuteW(NULL, operation, pathW.c_str(), NULL, NULL, SW_SHOWNORMAL);

    if ((INT_PTR)result > 32) {
        // 成功启动管理员进程，关闭当前进程
        QMessageBox::information(this, "提示", "正在以管理员权限重启程序...");
        QApplication::quit();
    } else {
        // 失败处理
        QString errorMsg;
        switch ((INT_PTR)result) {
            case ERROR_CANCELLED:
                errorMsg = "用户取消了UAC提示";
                break;
            case ERROR_FILE_NOT_FOUND:
                errorMsg = "找不到程序文件";
                break;
            case ERROR_ACCESS_DENIED:
                errorMsg = "访问被拒绝";
                break;
            default:
                errorMsg = QString("启动失败，错误代码: %1").arg((INT_PTR)result);
        }

        QMessageBox::warning(this, "错误", "获取管理员权限失败:\n" + errorMsg);
    }
#else
    QMessageBox::information(this, "提示", "此功能仅在Windows系统下可用");
#endif
}

void WindowsSettings::onTestFeature() {
    if (m_isAdmin) {
        QMessageBox::information(this, "测试",
            "✅ 管理员权限测试成功！\n\n"
            "当前拥有管理员权限，可以执行系统级操作：\n"
            "• 修改注册表\n"
            "• 管理系统服务\n"
            "• 修改系统文件\n"
            "• 安装驱动程序");
    } else {
        QMessageBox::warning(this, "测试",
            "⚠️ 当前没有管理员权限\n\n"
            "请先点击 \"获取管理员权限\" 按钮\n"
            "来提升程序权限后再试");
    }
}

void WindowsSettings::setupRunAsAdminSection() {
    // 创建 RunAs 功能组
    m_runAsGroup = new QGroupBox(tr("🚀 以管理员身份运行程序"));
    m_runAsGroup->setStyleSheet(R"(
        QGroupBox {
            font-size: 16pt;
            font-weight: bold;
            color: #2c3e50;
            border: 2px solid #bdc3c7;
            border-radius: 8px;
            margin: 15px 0;
            padding-top: 15px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 10px 0 10px;
        }
    )");

    QVBoxLayout* runAsLayout = new QVBoxLayout(m_runAsGroup);
    runAsLayout->setSpacing(15);

    // 预设命令选择
    QHBoxLayout* presetLayout = new QHBoxLayout();
    presetLayout->addWidget(new QLabel(tr("📋 预设命令:")));

    m_presetCombo = new QComboBox();
    m_presetCombo->addItem(tr("选择预设命令..."), "");
    m_presetCombo->addItem(tr("🗒️ 记事本"), "notepad.exe");
    m_presetCombo->addItem(tr("💻 命令提示符"), "cmd.exe");
    m_presetCombo->addItem(tr("🔧 注册表编辑器"), "regedit.exe");
    m_presetCombo->addItem(tr("🎛️ 任务管理器"), "taskmgr.exe");
    m_presetCombo->addItem(tr("🌐 PowerShell"), "powershell.exe");
    m_presetCombo->addItem(tr("⚙️ 服务管理"), "services.msc");
    m_presetCombo->addItem(tr("🖥️ 计算机管理"), "compmgmt.msc");
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &WindowsSettings::onPresetCommandSelected);

    presetLayout->addWidget(m_presetCombo);

    m_runAsBtn = new QPushButton(tr("▶️ 运行"));
    m_runAsBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #e67e22;
            color: white;
            border: none;
            padding: 8px 20px;
            font-size: 12pt;
            font-weight: bold;
            border-radius: 6px;
            min-width: 80px;
        }
        QPushButton:hover {
            background-color: #d35400;
        }
        QPushButton:pressed {
            background-color: #a0390e;
        }
    )");
    connect(m_runAsBtn, &QPushButton::clicked, this, &WindowsSettings::onRunAsAdmin);
    presetLayout->addWidget(m_runAsBtn);

    runAsLayout->addLayout(presetLayout);

    // 自定义命令输入
    QLabel* customLabel = new QLabel(tr("🛠️ 自定义命令:"));
    runAsLayout->addWidget(customLabel);

    QGridLayout* customLayout = new QGridLayout();

    customLayout->addWidget(new QLabel(tr("程序:")), 0, 0);
    m_commandInput = new QLineEdit();
    m_commandInput->setPlaceholderText(tr("输入程序路径或命令，如: notepad.exe"));
    customLayout->addWidget(m_commandInput, 0, 1);

    customLayout->addWidget(new QLabel(tr("参数:")), 1, 0);
    m_argumentsInput = new QLineEdit();
    m_argumentsInput->setPlaceholderText(tr("输入命令参数 (可选)"));
    customLayout->addWidget(m_argumentsInput, 1, 1);

    m_customRunBtn = new QPushButton(tr("🚀 以管理员身份运行"));
    m_customRunBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #8e44ad;
            color: white;
            border: none;
            padding: 10px 20px;
            font-size: 12pt;
            font-weight: bold;
            border-radius: 6px;
        }
        QPushButton:hover {
            background-color: #732d91;
        }
        QPushButton:pressed {
            background-color: #5b2471;
        }
    )");
    connect(m_customRunBtn, &QPushButton::clicked, this, &WindowsSettings::onRunCustomCommand);
    customLayout->addWidget(m_customRunBtn, 0, 2, 2, 1);

    runAsLayout->addLayout(customLayout);

    // 输出显示
    QLabel* outputLabel = new QLabel(tr("📤 执行输出:"));
    runAsLayout->addWidget(outputLabel);

    m_outputText = new QTextEdit();
    m_outputText->setMaximumHeight(150);
    m_outputText->setStyleSheet(R"(
        QTextEdit {
            background-color: #2c3e50;
            color: #ecf0f1;
            font-family: 'Consolas', 'Monaco', monospace;
            font-size: 10pt;
            border: 1px solid #34495e;
            border-radius: 4px;
        }
    )");
    m_outputText->setPlaceholderText(tr("程序输出将显示在这里..."));
    runAsLayout->addWidget(m_outputText);

    m_mainLayout->addWidget(m_runAsGroup);
}

void WindowsSettings::onPresetCommandSelected() {
    QString command = m_presetCombo->currentData().toString();
    if (!command.isEmpty()) {
        m_commandInput->setText(command);
        m_argumentsInput->clear();
    }
}

void WindowsSettings::onRunAsAdmin() {
    QString command = m_presetCombo->currentData().toString();
    if (command.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请选择一个预设命令"));
        return;
    }

    executeAsAdmin(command);
}

void WindowsSettings::onRunCustomCommand() {
    QString command = m_commandInput->text().trimmed();
    if (command.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请输入要执行的程序"));
        return;
    }

    QString arguments = m_argumentsInput->text().trimmed();
    executeAsAdmin(command, arguments);
}

void WindowsSettings::executeAsAdmin(const QString& command, const QString& arguments) {
#ifdef Q_OS_WIN
    if (m_process && m_process->state() != QProcess::NotRunning) {
        QMessageBox::warning(this, tr("警告"), tr("已有程序在运行中，请稍候..."));
        return;
    }

    // 清空输出
    m_outputText->clear();
    m_outputText->append(tr("🚀 正在以管理员权限启动: %1 %2").arg(command, arguments));

    // 使用PowerShell的Start-Process -Verb RunAs命令
    QString powershellCommand;
    if (arguments.isEmpty()) {
        powershellCommand = QString("Start-Process \"%1\" -Verb RunAs").arg(command);
    } else {
        powershellCommand = QString("Start-Process \"%1\" -ArgumentList \"%2\" -Verb RunAs").arg(command, arguments);
    }

    m_process = new QProcess(this);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &WindowsSettings::onProcessFinished);

    // 启动PowerShell进程
    QStringList powershellArgs;
    powershellArgs << "-Command" << powershellCommand;

    m_outputText->append(tr("💻 执行命令: powershell.exe %1").arg(powershellArgs.join(" ")));

    m_process->start("powershell.exe", powershellArgs);

    if (!m_process->waitForStarted(5000)) {
        m_outputText->append(tr("❌ 启动PowerShell失败: %1").arg(m_process->errorString()));
        delete m_process;
        m_process = nullptr;
    } else {
        m_outputText->append(tr("✅ PowerShell进程已启动"));

        // 启用定时器来检查进程状态
        QTimer::singleShot(2000, [this]() {
            if (m_process && m_process->state() == QProcess::Running) {
                m_outputText->append(tr("⏳ 等待用户确认UAC对话框..."));
            }
        });
    }

#else
    m_outputText->append(tr("❌ 此功能仅在Windows系统下可用"));
    QMessageBox::information(this, tr("提示"), tr("此功能仅在Windows系统下可用"));
#endif
}

void WindowsSettings::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    if (!m_process) return;

    if (exitStatus == QProcess::CrashExit) {
        m_outputText->append(tr("❌ PowerShell进程异常终止"));
    } else {
        if (exitCode == 0) {
            m_outputText->append(tr("✅ 程序已成功以管理员权限启动"));
        } else {
            m_outputText->append(tr("⚠️ PowerShell执行完成，退出代码: %1").arg(exitCode));

            // 读取错误输出
            QString errorOutput = m_process->readAllStandardError();
            if (!errorOutput.isEmpty()) {
                m_outputText->append(tr("错误信息: %1").arg(errorOutput));
            }
        }
    }

    // 读取标准输出
    QString output = m_process->readAllStandardOutput();
    if (!output.isEmpty()) {
        m_outputText->append(tr("输出: %1").arg(output));
    }

    m_process->deleteLater();
    m_process = nullptr;
}