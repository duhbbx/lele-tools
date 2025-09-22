#ifndef FTPSERVER_H
#define FTPSERVER_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QListWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QGroupBox>
#include <QFormLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QProgressBar>
#include <QFrame>
#include <QStatusBar>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QApplication>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QScrollBar>
#include <QDateTime>
#include <QSettings>
#include <QCloseEvent>
#include <QMenuBar>
#include <QStatusBar>
#include <QStyle>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QDir>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QHeaderView>

#include "../../common/dynamicobjectbase.h"

// Forward declarations
class FTPServerCore;
class FTPClientConnection;
class FTPUserManager;
class FTPConnectionMonitor;

/**
 * @brief FTP用户信息结构
 */
struct FTPUserInfo {
    QString username;
    QString password;
    QString homeDirectory;
    bool canRead;
    bool canWrite;
    bool canDelete;
    bool canCreateDir;
    bool isEnabled;
    QString description;

    FTPUserInfo()
        : canRead(true), canWrite(false), canDelete(false)
        , canCreateDir(false), isEnabled(true) {}

    bool isValid() const {
        return !username.isEmpty() && !password.isEmpty() && !homeDirectory.isEmpty();
    }
};

/**
 * @brief FTP服务器配置结构
 */
struct FTPServerConfig {
    int port;
    QString rootDirectory;
    QString serverName;
    int maxConnections;
    bool allowAnonymous;
    bool usePassiveMode;
    int passivePortMin;
    int passivePortMax;
    bool logConnections;
    bool logCommands;

    FTPServerConfig()
        : port(21), serverName("LeLe FTP Server"), maxConnections(10)
        , allowAnonymous(false), usePassiveMode(true)
        , passivePortMin(50000), passivePortMax(50100)
        , logConnections(true), logCommands(true) {}
};

/**
 * @brief FTP服务器连接信息结构（用于监控）
 */
struct FTPServerConnectionInfo {
    QString clientIP;
    QString username;
    QDateTime connectTime;
    QString currentDirectory;
    QString lastCommand;
    qint64 bytesTransferred;
    bool isActive;

    FTPServerConnectionInfo() : bytesTransferred(0), isActive(true) {}
};

/**
 * @brief FTP服务器主界面
 *
 * 提供FTP服务器启动、用户管理和监控功能
 * 左侧为服务器配置和用户管理，右侧为服务器状态和连接监控
 */
class FTPServer : public QWidget, public DynamicObjectBase {
    Q_OBJECT

public:
    explicit FTPServer(QWidget* parent = nullptr);
    ~FTPServer();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onStartServer();
    void onStopServer();
    void onServerStarted();
    void onServerStopped();
    void onServerError(const QString& error);
    void onNewConnection(const QString& clientIP);
    void onConnectionClosed(const QString& clientIP);
    void onConfigurationChanged();

private:
    void setupUI();
    void setupConfigWidget();
    void setupStatusWidget();
    void setupMenuBar();
    void loadSettings();
    void saveSettings();
    void updateServerStatus();

private:
    QSplitter* m_mainSplitter;
    QTabWidget* m_leftTabs;
    QTabWidget* m_rightTabs;

    // 服务器配置
    QWidget* m_configWidget;
    QSpinBox* m_portSpinBox;
    QLineEdit* m_rootDirEdit;
    QPushButton* m_browseDirButton;
    QLineEdit* m_serverNameEdit;
    QSpinBox* m_maxConnectionsSpinBox;
    QCheckBox* m_allowAnonymousCheckBox;
    QCheckBox* m_usePassiveModeCheckBox;
    QSpinBox* m_passiveMinPortSpinBox;
    QSpinBox* m_passiveMaxPortSpinBox;

    // 用户管理
    FTPUserManager* m_userManager;

    // 服务器状态
    QWidget* m_statusWidget;
    QLabel* m_serverStatusLabel;
    QLabel* m_serverAddressLabel;
    QLabel* m_activeConnectionsLabel;
    QLabel* m_totalConnectionsLabel;
    QPushButton* m_startStopButton;

    // 连接监控
    FTPConnectionMonitor* m_connectionMonitor;

    // 日志
    QTextEdit* m_logTextEdit;

    QMenuBar* m_menuBar;
    QStatusBar* m_statusBar;

    // 服务器核心
    FTPServerCore* m_serverCore;
    FTPServerConfig m_config;

    bool m_isServerRunning;
    int m_activeConnections;
    int m_totalConnections;
};

/**
 * @brief FTP服务器核心
 * 管理FTP服务器的启动、停止和客户端连接
 */
class FTPServerCore : public QObject {
    Q_OBJECT

public:
    explicit FTPServerCore(QObject* parent = nullptr);
    ~FTPServerCore();

    bool startServer(const FTPServerConfig& config);
    void stopServer();
    bool isRunning() const;

    void addUser(const FTPUserInfo& user);
    void removeUser(const QString& username);
    void updateUser(const FTPUserInfo& user);
    FTPUserInfo getUser(const QString& username) const;
    QList<FTPUserInfo> getAllUsers() const;

    QList<FTPServerConnectionInfo> getActiveConnections() const;
    void disconnectClient(const QString& clientIP);

signals:
    void serverStarted();
    void serverStopped();
    void serverError(const QString& error);
    void newConnection(const QString& clientIP);
    void connectionClosed(const QString& clientIP);
    void logMessage(const QString& message);

private slots:
    void onNewConnection();
    void onClientDisconnected();

private:
    QTcpServer* m_tcpServer;
    FTPServerConfig m_config;
    QMap<QString, FTPUserInfo> m_users;
    QMap<QTcpSocket*, FTPClientConnection*> m_connections;
    mutable QMutex m_mutex;
    bool m_isRunning;
};

/**
 * @brief FTP客户端连接处理
 * 处理单个FTP客户端的连接和命令
 */
class FTPClientConnection : public QObject {
    Q_OBJECT

public:
    explicit FTPClientConnection(QTcpSocket* socket, const FTPServerConfig& config,
                                QObject* parent = nullptr);
    ~FTPClientConnection();

    QString getClientIP() const;
    QString getUsername() const;
    QString getCurrentDirectory() const;
    QDateTime getConnectTime() const;
    qint64 getBytesTransferred() const;
    bool isAuthenticated() const;

signals:
    void disconnected();
    void logMessage(const QString& message);
    void bytesTransferred(qint64 bytes);

private slots:
    void onSocketReadyRead();
    void onSocketDisconnected();
    void onDataSocketConnected();
    void onDataSocketDisconnected();
    void onDataSocketReadyRead();

private:
    void sendResponse(int code, const QString& message);
    void processCommand(const QString& command);
    void handleUser(const QString& username);
    void handlePass(const QString& password);
    void handlePwd();
    void handleCwd(const QString& path);
    void handleList(const QString& path = "");
    void handleRetr(const QString& filename);
    void handleStor(const QString& filename);
    void handleDele(const QString& filename);
    void handleMkd(const QString& dirname);
    void handleRmd(const QString& dirname);
    void handlePasv();
    void handlePort(const QString& portInfo);
    void handleQuit();

    bool authenticateUser(const QString& username, const QString& password);
    bool hasPermission(const QString& operation) const;
    QString resolvePath(const QString& path) const;
    QString formatFileList(const QString& directory) const;

private:
    QTcpSocket* m_controlSocket;
    QTcpServer* m_dataServer;
    QTcpSocket* m_dataSocket;
    FTPServerConfig m_config;

    QString m_clientIP;
    QString m_username;
    QString m_currentDirectory;
    QDateTime m_connectTime;
    qint64 m_bytesTransferred;
    bool m_isAuthenticated;
    FTPUserInfo m_userInfo;

    // 数据传输状态
    bool m_isPassiveMode;
    QString m_transferType;
    QFile* m_transferFile;
};

/**
 * @brief FTP用户管理器
 * 提供用户的增删改查界面
 */
class FTPUserManager : public QWidget {
    Q_OBJECT

public:
    explicit FTPUserManager(QWidget* parent = nullptr);
    ~FTPUserManager();

    void setServerCore(FTPServerCore* serverCore);
    void refreshUserList();

signals:
    void userAdded(const FTPUserInfo& user);
    void userUpdated(const FTPUserInfo& user);
    void userRemoved(const QString& username);

public slots:
    void onAddUser();

private slots:
    void onEditUser();
    void onDeleteUser();
    void onUserSelectionChanged();

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void setupUI();
    void updateUserList();

private:
    QVBoxLayout* m_layout;
    QTreeWidget* m_userTree;
    QPushButton* m_addButton;
    QPushButton* m_editButton;
    QPushButton* m_deleteButton;

    FTPServerCore* m_serverCore;
    QString m_selectedUsername;
};

/**
 * @brief FTP连接监控器
 * 显示活动连接和传输统计
 */
class FTPConnectionMonitor : public QWidget {
    Q_OBJECT

public:
    explicit FTPConnectionMonitor(QWidget* parent = nullptr);
    ~FTPConnectionMonitor();

    void setServerCore(FTPServerCore* serverCore);
    void refreshConnections();

private slots:
    void onRefresh();
    void onDisconnectClient();
    void onConnectionSelectionChanged();

private:
    void setupUI();
    void updateConnectionList();

private:
    QVBoxLayout* m_layout;
    QTreeWidget* m_connectionTree;
    QPushButton* m_refreshButton;
    QPushButton* m_disconnectButton;
    QLabel* m_statisticsLabel;

    FTPServerCore* m_serverCore;
    QString m_selectedClientIP;
    QTimer* m_refreshTimer;
};

/**
 * @brief FTP用户编辑对话框
 */
class FTPUserDialog : public QDialog {
    Q_OBJECT

public:
    explicit FTPUserDialog(QWidget* parent = nullptr);
    explicit FTPUserDialog(const FTPUserInfo& user, QWidget* parent = nullptr);

    FTPUserInfo getUserInfo() const;

private slots:
    void onBrowseHomeDirectory();

private:
    void setupUI();
    void setUserInfo(const FTPUserInfo& user);

private:
    QFormLayout* m_formLayout;
    QLineEdit* m_usernameEdit;
    QLineEdit* m_passwordEdit;
    QLineEdit* m_homeDirEdit;
    QPushButton* m_browseDirButton;
    QCheckBox* m_canReadCheckBox;
    QCheckBox* m_canWriteCheckBox;
    QCheckBox* m_canDeleteCheckBox;
    QCheckBox* m_canCreateDirCheckBox;
    QCheckBox* m_isEnabledCheckBox;
    QTextEdit* m_descriptionEdit;
};

#endif // FTPSERVER_H