#ifndef FTPCLIENT_H
#define FTPCLIENT_H

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
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QTcpSocket>
#include <QHostAddress>

#include "../../common/dynamicobjectbase.h"

// Forward declarations
class FTPConnection;
class FTPFileManager;
class FTPConnectionManager;

/**
 * @brief FTP客户端主界面
 *
 * 提供FTP连接管理和文件传输功能
 * 左侧为连接管理面板，右侧为FTP文件管理界面
 */
class FTPClient : public QWidget, public DynamicObjectBase {
    Q_OBJECT

public:
    explicit FTPClient(QWidget* parent = nullptr);
    ~FTPClient();

signals:
    void connectionEstablished(const QString& name);
    void connectionClosed(const QString& name);
    void connectionError(const QString& name, const QString& error);

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

private:
    QSplitter* m_mainSplitter;
    FTPConnectionManager* m_connectionManager;
    QTabWidget* m_rightTabs;
    FTPFileManager* m_fileManager;

    QMenuBar* m_menuBar;
    QStatusBar* m_statusBar;

    // 当前连接状态
    QString m_currentConnection;
    bool m_isConnected;
};

/**
 * @brief FTP连接信息结构
 */
struct FTPConnectionInfo {
    QString name;
    QString hostname;
    int port;
    QString username;
    QString password;        // 注意：实际使用时应考虑安全存储
    bool usePassiveMode;
    QString description;

    FTPConnectionInfo() : port(21), usePassiveMode(true) {}

    bool isValid() const {
        return !name.isEmpty() && !hostname.isEmpty() && !username.isEmpty();
    }
};

/**
 * @brief FTP连接管理器
 * 管理FTP连接配置和连接状态
 */
class FTPConnectionManager : public QWidget {
    Q_OBJECT

public:
    explicit FTPConnectionManager(QWidget* parent = nullptr);
    ~FTPConnectionManager();

    void addConnection(const FTPConnectionInfo& info);
    void editConnection(const QString& name, const FTPConnectionInfo& info);
    void removeConnection(const QString& name);
    FTPConnectionInfo getConnectionInfo(const QString& name) const;
    QStringList getConnectionNames() const;

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

    QMap<QString, FTPConnectionInfo> m_connections;
    QString m_selectedConnection;
};

/**
 * @brief FTP连接对象
 * 管理单个FTP连接的生命周期
 */
class FTPConnection : public QObject {
    Q_OBJECT

public:
    explicit FTPConnection(const FTPConnectionInfo& info, QObject* parent = nullptr);
    ~FTPConnection();

    bool isConnected() const;
    QString getLastError() const;
    QString getCurrentPath() const;

public slots:
    void connectToHost();
    void disconnect();
    void changeDirectory(const QString& path);
    void listDirectory(const QString& path = "");
    void uploadFile(const QString& localPath, const QString& remotePath);
    void downloadFile(const QString& remotePath, const QString& localPath);
    void deleteFile(const QString& remotePath);
    void createDirectory(const QString& path);
    void removeDirectory(const QString& path);

signals:
    void connected();
    void disconnected();
    void error(const QString& message);
    void directoryListed(const QStringList& files, const QStringList& directories);
    void fileTransferProgress(int bytesTransferred, int totalBytes);
    void fileTransferFinished(bool success, const QString& message);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    void onSocketReadyRead();

private:
    void sendCommand(const QString& command);
    void processResponse(const QString& response);
    void cleanup();

private:
    FTPConnectionInfo m_info;
    QString m_lastError;
    QString m_currentPath;

    QTcpSocket* m_controlSocket;
    QTcpSocket* m_dataSocket;
    bool m_connected;
    bool m_loggedIn;
    mutable QMutex m_mutex;

    // Command tracking
    QString m_currentCommand;
    QStringList m_responseBuffer;
    QString m_partialResponse;
};

/**
 * @brief FTP文件管理器
 * 提供图形化的FTP文件管理功能
 */
class FTPFileManager : public QWidget {
    Q_OBJECT

public:
    explicit FTPFileManager(QWidget* parent = nullptr);
    ~FTPFileManager();

    void setConnection(FTPConnection* connection);
    void refresh();

private slots:
    void onRemoteItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onLocalItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onUploadClicked();
    void onDownloadClicked();
    void onDeleteClicked();
    void onCreateDirectoryClicked();
    void onRefreshClicked();
    void onDirectoryListed(const QStringList& files, const QStringList& directories);
    void onFileTransferProgress(int bytesTransferred, int totalBytes);
    void onFileTransferFinished(bool success, const QString& message);

private:
    void setupUI();
    void updateRemoteFiles();
    void updateLocalFiles(const QString& path);
    void uploadFile(const QString& localPath, const QString& remotePath);
    void downloadFile(const QString& remotePath, const QString& localPath);
    void createRemoteDirectory(const QString& name);
    void deleteRemoteItem(const QString& path);

private:
    QHBoxLayout* m_layout;
    QSplitter* m_splitter;

    // 远程文件区域
    QWidget* m_remoteWidget;
    QVBoxLayout* m_remoteLayout;
    QLabel* m_remotePathLabel;
    QTreeWidget* m_remoteTree;
    QPushButton* m_refreshButton;

    // 本地文件区域
    QWidget* m_localWidget;
    QVBoxLayout* m_localLayout;
    QLabel* m_localPathLabel;
    QTreeWidget* m_localTree;

    // 操作按钮
    QWidget* m_buttonWidget;
    QVBoxLayout* m_buttonLayout;
    QPushButton* m_uploadButton;
    QPushButton* m_downloadButton;
    QPushButton* m_deleteButton;
    QPushButton* m_createDirButton;

    FTPConnection* m_connection;
    QString m_remotePath;
    QString m_localPath;

    QProgressBar* m_progressBar;
};

/**
 * @brief FTP连接配置对话框
 */
class FTPConnectionDialog : public QDialog {
    Q_OBJECT

public:
    explicit FTPConnectionDialog(QWidget* parent = nullptr);
    explicit FTPConnectionDialog(const FTPConnectionInfo& info, QWidget* parent = nullptr);

    FTPConnectionInfo getConnectionInfo() const;

private slots:
    void onTestConnection();

private:
    void setupUI();
    void setConnectionInfo(const FTPConnectionInfo& info);

private:
    QFormLayout* m_formLayout;
    QLineEdit* m_nameEdit;
    QLineEdit* m_hostnameEdit;
    QSpinBox* m_portSpinBox;
    QLineEdit* m_usernameEdit;
    QLineEdit* m_passwordEdit;
    QCheckBox* m_passiveModeCheckBox;
    QTextEdit* m_descriptionEdit;
    QPushButton* m_testButton;
    QProgressBar* m_testProgress;
    QLabel* m_testResultLabel;
};

#endif // FTPCLIENT_H