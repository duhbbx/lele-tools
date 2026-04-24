#ifndef SSHCLIENT_H
#define SSHCLIENT_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTabWidget>
#include <QTabBar>
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
#include <QFileSystemModel>
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
#include <QRegularExpression>

#include <fcntl.h>
#include <sys/stat.h>

#ifdef WITH_LIBSSH
#include <libssh/libssh.h>
#include <libssh/sftp.h>
#endif

#include "../../common/dynamicobjectbase.h"

// Forward declarations
class SSHConnection;
class SSHTerminal;
class SFTPBrowser;
class ConnectionManager;
class TerminalTextEdit;
class TerminalEmulator;
class TerminalWidget;

/**
 * @brief SSH客户端主界面
 *
 * 提供SSH连接管理、终端操作和SFTP文件传输功能
 * 左侧为连接管理面板，右侧为SSH终端和SFTP文件浏览器
 */
class SSHClient : public QWidget, public DynamicObjectBase {
    Q_OBJECT

public:
    explicit SSHClient(QWidget* parent = nullptr);
    ~SSHClient();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onConnectionRequested(const QString& name);
    void onConnectionEstablished(const QString& name);
    void onConnectionClosed(const QString& name);
    void onConnectionError(const QString& name, const QString& error);
    void onNewConnection();
    void onEditConnection();
    void onDeleteConnection();
    void onDisconnect();

private:
    void setupUI();
    void setupMenuBar();
    void loadSettings();
    void saveSettings();

    void closeConnectionTab(int index);

private:
    QSplitter* m_mainSplitter;
    ConnectionManager* m_connectionManager;
    QTabWidget* m_connectionTabs; // 每个连接一个 tab

    QMenuBar* m_menuBar;
    QStatusBar* m_statusBar;

    // 连接管理：每个 tab 一个 session，同一连接名可有多个 tab
    struct ConnectionSession {
        QString name;           // connection config name
        SSHConnection* connection = nullptr;
        SSHTerminal* terminal = nullptr;
        SFTPBrowser* sftp = nullptr;
        QWidget* tabWidget = nullptr; // tab 页内的容器
    };
    QList<ConnectionSession> m_sessions;

    void duplicateTab(int index);
};

/**
 * @brief SSH连接信息结构
 */
struct SSHConnectionInfo {
    QString name;
    QString hostname;
    int port;
    QString username;
    QString password;        // 注意：实际使用时应考虑安全存储
    QString privateKeyPath;
    bool usePrivateKey;
    QString description;
    bool keepAlive = true;
    int keepAliveInterval = 30; // seconds

    SSHConnectionInfo() : port(22), usePrivateKey(false) {}

    bool isValid() const {
        return !name.isEmpty() && !hostname.isEmpty() && !username.isEmpty() &&
               (!password.isEmpty() || (usePrivateKey && !privateKeyPath.isEmpty()));
    }
};

/**
 * @brief 连接管理器
 * 管理SSH连接配置和连接状态
 */
class ConnectionManager : public QWidget {
    Q_OBJECT

public:
    explicit ConnectionManager(QWidget* parent = nullptr);
    ~ConnectionManager();

    void addConnection(const SSHConnectionInfo& info);
    void editConnection(const QString& name, const SSHConnectionInfo& info);
    void removeConnection(const QString& name);
    SSHConnectionInfo getConnectionInfo(const QString& name) const;
    QStringList getConnectionNames() const;
    QString selectedConnection() const { return m_selectedConnection; }

signals:
    void connectionRequested(const QString& name);
    void newConnectionRequested();
    void editConnectionRequested();
    void deleteConnectionRequested();

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private slots:
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onItemSelectionChanged();

private:
    void setupUI();
    void loadConnections();
    void saveConnections();
    void updateConnectionsList();

private:
    QVBoxLayout* m_layout;
    QTreeWidget* m_connectionsTree;
    QPushButton* m_newButton;
    QPushButton* m_editButton;
    QPushButton* m_deleteButton;
    QPushButton* m_connectButton;

    QMap<QString, SSHConnectionInfo> m_connections;
    QString m_selectedConnection;
};

/**
 * @brief SFTP文件信息结构
 */
struct SFTPFileInfo {
    QString name;
    QString path;
    qint64 size;
    bool isDir;
    QString permissions;
    QDateTime modified;
};

/**
 * @brief SSH连接对象
 * 管理单个SSH连接的生命周期
 */
class SSHConnection : public QObject {
    Q_OBJECT

public:
    explicit SSHConnection(const SSHConnectionInfo& info, QObject* parent = nullptr);
    ~SSHConnection();

    bool isConnected() const;
    QString getLastError() const;
    SSHConnectionInfo connectionInfo() const { return m_info; }

    // SFTP operations
    bool initSftp();
    QList<SFTPFileInfo> listDirectory(const QString& path);
    bool downloadFile(const QString& remotePath, const QString& localPath);
    bool uploadFile(const QString& localPath, const QString& remotePath);
    bool deleteRemoteFile(const QString& path);
    bool createRemoteDirectory(const QString& path);
    QString sftpError() const;

public slots:
    void connectToHost();
    void disconnect();
    void executeCommand(const QString& command);
    void requestPtyShell();
    void writeToChannel(const QByteArray& data);
    void resizePty(int cols, int rows);

signals:
    void connected();
    void disconnected();
    void error(const QString& message);
    void commandOutput(const QString& output);
    void authenticationRequired(const QString& prompt);
    void dataReceived(const QByteArray& data);

private:
    void cleanup();

private:
    SSHConnectionInfo m_info;
    QString m_lastError;

#ifdef WITH_LIBSSH
    ssh_session m_session;
    ssh_channel m_channel;
    sftp_session m_sftpSession;
#endif

    bool m_connected;
    QMutex m_mutex;
    QThread* m_readThread = nullptr;
    bool m_shellActive = false;
    QTimer* m_keepAliveTimer = nullptr;
};

/**
 * @brief Terminal text edit with key capture for PTY interaction
 */
class TerminalTextEdit : public QTextEdit {
    Q_OBJECT
public:
    explicit TerminalTextEdit(QWidget* parent = nullptr) : QTextEdit(parent) {}

signals:
    void keyPressed(const QByteArray& data);

protected:
    void keyPressEvent(QKeyEvent* event) override;
};

/**
 * @brief SSH终端组件
 * 提供交互式SSH终端功能（PTY模式）
 * 使用VT100终端仿真器 + QPainter渲染
 */
class SSHTerminal : public QWidget {
    Q_OBJECT

public:
    explicit SSHTerminal(QWidget* parent = nullptr);
    ~SSHTerminal() override;

    void setConnection(SSHConnection* connection);
    void clear();

private slots:
    void onConnectionError(const QString& error);

private:
    void setupUI();

    QVBoxLayout* m_layout;
    TerminalEmulator* m_emulator;
    TerminalWidget* m_termWidget;
    SSHConnection* m_connection = nullptr;
};

/**
 * @brief SFTP文件浏览器
 * 提供图形化的SFTP文件管理功能
 */
class SFTPBrowser : public QWidget {
    Q_OBJECT

public:
    explicit SFTPBrowser(QWidget* parent = nullptr);
    ~SFTPBrowser();

    void setConnection(SSHConnection* connection);
    void refresh();

private slots:
    void onRemoteItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onLocalItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onUploadClicked();
    void onDownloadClicked();
    void onDeleteClicked();
    void onCreateDirectoryClicked();
    void onRefreshClicked();

private:
    void setupUI();
    void updateRemoteFiles(const QString& path = ".");
    void updateLocalFiles(const QString& path);
    void uploadFile(const QString& localPath, const QString& remotePath);
    void downloadFile(const QString& remotePath, const QString& localPath);
    void createRemoteDirectory(const QString& path, const QString& name);
    void deleteRemoteItem(const QString& path);

private:
    QHBoxLayout* m_layout;
    QSplitter* m_splitter;

    // 远程文件区域
    QWidget* m_remoteWidget;
    QVBoxLayout* m_remoteLayout;
    QLineEdit* m_remotePathEdit;
    QTreeWidget* m_remoteTree;
    QPushButton* m_refreshButton;

    // 本地文件区域
    QWidget* m_localWidget;
    QVBoxLayout* m_localLayout;
    QLineEdit* m_localPathEdit;
    QTreeWidget* m_localTree;

    // 操作按钮
    QWidget* m_buttonWidget;
    QVBoxLayout* m_buttonLayout;
    QPushButton* m_uploadButton;
    QPushButton* m_downloadButton;
    QPushButton* m_deleteButton;
    QPushButton* m_createDirButton;

    SSHConnection* m_connection;
    QString m_remotePath;
    QString m_localPath;

    QProgressBar* m_progressBar;
};

/**
 * @brief SSH连接配置对话框
 */
class SSHConnectionDialog : public QDialog {
    Q_OBJECT

public:
    explicit SSHConnectionDialog(QWidget* parent = nullptr);
    explicit SSHConnectionDialog(const SSHConnectionInfo& info, QWidget* parent = nullptr);

    SSHConnectionInfo getConnectionInfo() const;

private slots:
    void onAuthMethodChanged();
    void onBrowsePrivateKey();
    void onTestConnection();

private:
    void setupUI();
    void setConnectionInfo(const SSHConnectionInfo& info);

private:
    QFormLayout* m_formLayout;
    QLineEdit* m_nameEdit;
    QLineEdit* m_hostnameEdit;
    QSpinBox* m_portSpinBox;
    QLineEdit* m_usernameEdit;
    QLineEdit* m_passwordEdit;
    QLineEdit* m_privateKeyEdit;
    QPushButton* m_browseKeyButton;
    QCheckBox* m_usePrivateKeyCheckBox;
    QTextEdit* m_descriptionEdit;
    QCheckBox* m_keepAliveCheckBox;
    QSpinBox* m_keepAliveIntervalSpinBox;
    QPushButton* m_testButton;
    QProgressBar* m_testProgress;
    QLabel* m_testResultLabel;
};

#endif // SSHCLIENT_H