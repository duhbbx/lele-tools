#include "windowssettings.h"
#include <QApplication>
#include <QMessageBox>
#include <QProcess>
#include <Windows.h>

REGISTER_DYNAMICOBJECT(WindowsSettings);

WindowsSettings::WindowsSettings(QWidget* parent)
    : QWidget(parent), DynamicObjectBase(), m_isAdmin(false) {

    setupUI();
    updateAdminStatus();
}

WindowsSettings::~WindowsSettings() {
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