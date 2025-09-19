#ifndef ADMINRIGHTSTOOL_H
#define ADMINRIGHTSTOOL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QGroupBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QComboBox>
#include <QCheckBox>
#include <QProgressBar>
#include <QTimer>
#include <QProcess>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>
#include <QSplitter>
#include <QHeaderView>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#include <lmcons.h>
#include <sddl.h>
#include <aclapi.h>
#include <winbase.h>
#endif

#include "../../common/dynamicobjectbase.h"

// 权限操作类型
enum class AdminOperation {
    RunAsAdmin,         // 以管理员身份运行程序
    EditFile,           // 编辑受保护文件
    ModifyRegistry,     // 修改注册表
    EditHosts,          // 编辑hosts文件
    ManageServices,     // 管理系统服务
    FilePermissions,    // 修改文件权限
    SystemSettings      // 修改系统设置
};

// 权限检测结果
struct AdminRightsStatus {
    bool isAdmin = false;           // 是否具有管理员权限
    bool isElevated = false;        // 是否已提升权限
    bool canElevate = false;        // 是否可以提升权限
    QString username;               // 当前用户名
    QString userSid;               // 用户SID
    QStringList groups;            // 用户组列表
    QString error;                 // 错误信息

    bool isValid() const {
        return error.isEmpty();
    }
};

// Windows权限管理器
class WindowsRightsManager : public QObject {
    Q_OBJECT

public:
    explicit WindowsRightsManager(QObject* parent = nullptr);

    // 权限检测
    AdminRightsStatus checkCurrentRights();
    bool isRunningAsAdmin();
    bool canRequestElevation();

    // 权限提升
    bool requestElevation();
    bool runAsAdmin(const QString& program, const QString& arguments = QString());
    bool runAsAdminWithUI(const QString& program, const QString& arguments = QString());

    // 文件权限操作
    bool takeOwnership(const QString& filePath);
    bool grantFullControl(const QString& filePath, const QString& username = QString());
    bool makeFileWritable(const QString& filePath);

    // 系统操作
    bool editProtectedFile(const QString& filePath);
    bool modifyHosts(const QString& entry, bool add = true);
    bool runElevatedCommand(const QString& command, const QString& arguments = QString());

signals:
    void rightsChanged(const AdminRightsStatus& status);
    void operationCompleted(AdminOperation operation, bool success, const QString& message);
    void elevationRequested();

private:
    QString getCurrentUsername();
    QString getCurrentUserSid();
    QStringList getCurrentUserGroups();
    bool enablePrivilege(const QString& privilegeName);

#ifdef Q_OS_WIN
    HANDLE m_processToken;
#endif
};

// 程序提升运行组件
class ElevatedRunner : public QWidget {
    Q_OBJECT

public:
    explicit ElevatedRunner(QWidget* parent = nullptr);

private slots:
    void onSelectProgramClicked();
    void onRunAsAdminClicked();
    void onRunCurrentAsAdminClicked();
    void onQuickActionClicked();

private:
    void setupUI();
    void addQuickAction(const QString& name, const QString& program, const QString& args = QString());

    // UI组件
    QVBoxLayout* m_mainLayout;

    // 程序运行区域
    QGroupBox* m_runnerGroup;
    QFormLayout* m_runnerLayout;
    QLineEdit* m_programEdit;
    QPushButton* m_selectProgramBtn;
    QLineEdit* m_argumentsEdit;
    QHBoxLayout* m_runButtonLayout;
    QPushButton* m_runAsAdminBtn;
    QPushButton* m_runCurrentAsAdminBtn;

    // 快捷操作区域
    QGroupBox* m_quickActionsGroup;
    QGridLayout* m_quickActionsLayout;

    // 结果显示区域
    QGroupBox* m_resultGroup;
    QVBoxLayout* m_resultLayout;
    QTextEdit* m_resultText;

    WindowsRightsManager* m_rightsManager;
};

// 文件权限管理组件
class FilePermissionsManager : public QWidget {
    Q_OBJECT

public:
    explicit FilePermissionsManager(QWidget* parent = nullptr);

private slots:
    void onSelectFileClicked();
    void onTakeOwnershipClicked();
    void onGrantFullControlClicked();
    void onMakeWritableClicked();
    void onEditFileClicked();
    void onRestorePermissionsClicked();
    void onAnalyzePermissionsClicked();

private:
    void setupUI();
    void updateFileInfo(const QString& filePath);
    void displayPermissions(const QString& filePath);

    // UI组件
    QVBoxLayout* m_mainLayout;
    QSplitter* m_mainSplitter;

    // 文件选择区域
    QWidget* m_fileWidget;
    QVBoxLayout* m_fileLayout;
    QGroupBox* m_fileGroup;
    QFormLayout* m_fileFormLayout;
    QLineEdit* m_filePathEdit;
    QPushButton* m_selectFileBtn;
    QLabel* m_fileInfoLabel;

    // 权限操作区域
    QGroupBox* m_operationsGroup;
    QVBoxLayout* m_operationsLayout;
    QPushButton* m_takeOwnershipBtn;
    QPushButton* m_grantFullControlBtn;
    QPushButton* m_makeWritableBtn;
    QPushButton* m_editFileBtn;
    QPushButton* m_restorePermissionsBtn;

    // 权限分析区域
    QWidget* m_permissionsWidget;
    QVBoxLayout* m_permissionsLayout;
    QGroupBox* m_permissionsGroup;
    QPushButton* m_analyzePermissionsBtn;
    QTableWidget* m_permissionsTable;

    // 结果显示区域
    QGroupBox* m_resultGroup;
    QVBoxLayout* m_resultLayout;
    QTextEdit* m_resultText;

    WindowsRightsManager* m_rightsManager;
    QString m_currentFilePath;
};

// Hosts文件编辑器组件
class HostsFileEditor : public QWidget {
    Q_OBJECT

public:
    explicit HostsFileEditor(QWidget* parent = nullptr);

private slots:
    void onLoadHostsClicked();
    void onSaveHostsClicked();
    void onAddEntryClicked();
    void onRemoveSelectedClicked();
    void onToggleSelectedClicked();
    void onBackupHostsClicked();
    void onRestoreHostsClicked();
    void onFlushDnsClicked();

private:
    void setupUI();
    void loadHostsFile();
    void saveHostsFile();
    void parseHostsContent(const QString& content);
    QString generateHostsContent();
    void addHostEntry(const QString& ip, const QString& hostname, bool enabled = true);

    // UI组件
    QVBoxLayout* m_mainLayout;
    QSplitter* m_mainSplitter;

    // 控制区域
    QWidget* m_controlWidget;
    QVBoxLayout* m_controlLayout;

    QGroupBox* m_fileGroup;
    QVBoxLayout* m_fileLayout;
    QHBoxLayout* m_fileButtonLayout;
    QPushButton* m_loadHostsBtn;
    QPushButton* m_saveHostsBtn;
    QPushButton* m_backupHostsBtn;
    QPushButton* m_restoreHostsBtn;

    QGroupBox* m_entryGroup;
    QFormLayout* m_entryLayout;
    QLineEdit* m_ipEdit;
    QLineEdit* m_hostnameEdit;
    QHBoxLayout* m_entryButtonLayout;
    QPushButton* m_addEntryBtn;
    QPushButton* m_removeSelectedBtn;
    QPushButton* m_toggleSelectedBtn;

    QHBoxLayout* m_dnsButtonLayout;
    QPushButton* m_flushDnsBtn;

    // Hosts条目表格
    QWidget* m_hostsWidget;
    QVBoxLayout* m_hostsLayout;
    QLabel* m_hostsLabel;
    QTableWidget* m_hostsTable;

    WindowsRightsManager* m_rightsManager;
    QString m_hostsFilePath;
    QString m_backupFilePath;
};

// 系统权限状态显示组件
class SystemRightsStatus : public QWidget {
    Q_OBJECT

public:
    explicit SystemRightsStatus(QWidget* parent = nullptr);

private slots:
    void onRefreshStatusClicked();
    void onRequestElevationClicked();
    void onCheckUACStatusClicked();
    void updateStatus();

private:
    void setupUI();
    void displayRightsStatus(const AdminRightsStatus& status);

    // UI组件
    QVBoxLayout* m_mainLayout;

    // 状态显示区域
    QGroupBox* m_statusGroup;
    QFormLayout* m_statusLayout;
    QLabel* m_adminStatusLabel;
    QLabel* m_elevatedStatusLabel;
    QLabel* m_usernameLabel;
    QLabel* m_userSidLabel;
    QLabel* m_groupsLabel;

    // 控制按钮区域
    QGroupBox* m_controlGroup;
    QVBoxLayout* m_controlLayout;
    QHBoxLayout* m_buttonLayout;
    QPushButton* m_refreshBtn;
    QPushButton* m_requestElevationBtn;
    QPushButton* m_checkUACBtn;

    // UAC状态区域
    QGroupBox* m_uacGroup;
    QVBoxLayout* m_uacLayout;
    QLabel* m_uacStatusLabel;
    QTextEdit* m_uacInfoText;

    WindowsRightsManager* m_rightsManager;
    QTimer* m_refreshTimer;
};

// 管理员权限工具主类
class AdminRightsTool final : public QWidget, public DynamicObjectBase {
    Q_OBJECT

public:
    explicit AdminRightsTool(QWidget* parent = nullptr);
    ~AdminRightsTool() override = default;

private:
    void setupUI();

    // UI组件
    QVBoxLayout* m_mainLayout;
    QTabWidget* m_tabWidget;
    SystemRightsStatus* m_statusTab;
    ElevatedRunner* m_runnerTab;
    FilePermissionsManager* m_filePermissionsTab;
    HostsFileEditor* m_hostsEditorTab;
};

#endif // ADMINRIGHTSTOOL_H