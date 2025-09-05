#ifndef DATABASETOOL_H
#define DATABASETOOL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTableWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QGroupBox>
#include <QListWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QProgressBar>
#include <QStatusBar>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QToolBar>
#include <QSplashScreen>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QSettings>
#include <QStyle>
#include <QMetaType>

#include "../../common/dynamicobjectbase.h"
#include "../../common/connx/Connection.h"

using namespace Connx;

// 连接配置对话框
class ConnectionDialog : public QDialog {
    Q_OBJECT

public:
    explicit ConnectionDialog(QWidget* parent = nullptr);
    explicit ConnectionDialog(const ConnectionConfig& config, QWidget* parent = nullptr);
    
    ConnectionConfig getConnectionConfig() const;
    void setConnectionConfig(const ConnectionConfig& config);

private slots:
    void onConnectionTypeChanged();
    void onTestConnection();
    void onAccept();

private:
    void setupUI();
    void setupLeftPanel();
    void setupRightPanel();
    void createFormControls();
    void updateFormForType(const QString& type);
    bool validateInput();
    
    // 主布局
    QHBoxLayout* m_mainLayout;
    QSplitter* m_splitter;
    
    // 左侧面板
    QWidget* m_leftPanel;
    QVBoxLayout* m_leftLayout;
    QLabel* m_typeLabel;
    QListWidget* m_typeList;
    
    // 右侧面板
    QWidget* m_rightPanel;
    QVBoxLayout* m_rightLayout;
    QFormLayout* m_formLayout;
    QWidget* m_formWidget;
    
    // 表单控件
    QLineEdit* m_nameEdit;
    QLineEdit* m_hostEdit;
    QSpinBox* m_portSpin;
    QLineEdit* m_usernameEdit;
    QLineEdit* m_passwordEdit;
    QLineEdit* m_databaseEdit;
    QSpinBox* m_timeoutSpin;
    QCheckBox* m_sslCheck;
    
    // Redis特有控件
    QLabel* m_dbIndexLabel;
    QSpinBox* m_dbIndexSpin;
    
    // MySQL特有控件
    QLabel* m_charsetLabel;
    QComboBox* m_charsetCombo;
    
    // 底部按钮
    QWidget* m_buttonWidget;
    QHBoxLayout* m_buttonLayout;
    QPushButton* m_testBtn;
    QDialogButtonBox* m_buttonBox;
    QLabel* m_statusLabel;
    
    QString m_currentType;
};

// 查询标签页
class QueryTab : public QWidget {
    Q_OBJECT

public:
    explicit QueryTab(Connection* connection, QWidget* parent = nullptr);
    
    void setQuery(const QString& query);
    QString getQuery() const;
    void executeQuery();

signals:
    void titleChanged(const QString& title);
    void queryExecuted(const QueryResult& result);

private slots:
    void onExecuteClicked();
    void onClearClicked();
    void onFormatClicked();

private:
    void setupUI();
    void updateResults(const QueryResult& result);
    
    Connection* m_connection;
    QVBoxLayout* m_layout;
    
    // 工具栏
    QToolBar* m_toolbar;
    QAction* m_executeAction;
    QAction* m_clearAction;
    QAction* m_formatAction;
    
    // 查询区域
    QTextEdit* m_queryEdit;
    
    // 结果区域
    QTabWidget* m_resultTabs;
    QTableWidget* m_resultTable;
    QTextEdit* m_messagesText;
    
    // 状态栏
    QLabel* m_statusLabel;
    QLabel* m_timeLabel;
    QLabel* m_rowsLabel;
};

// 数据库浏览器树
class DatabaseTreeWidget : public QTreeWidget {
    Q_OBJECT

public:
    explicit DatabaseTreeWidget(QWidget* parent = nullptr);
    
    void setConnection(Connection* connection);
    void refreshConnection(const QString& connectionName);
    void addConnection(const QString& name, Connection* connection);
    void removeConnection(const QString& name);

signals:
    void tableDoubleClicked(const QString& connectionName, const QString& database, const QString& table);
    void keyDoubleClicked(const QString& connectionName, const QString& database, const QString& key);
    void connectionRequested(const QString& connectionName);
    void disconnectionRequested(const QString& connectionName);

private slots:
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onItemExpanded(QTreeWidgetItem* item);
    void onCustomContextMenuRequested(const QPoint& pos);

private:
    void setupContextMenu();
    void loadDatabases(QTreeWidgetItem* connectionItem, Connection* connection);
    void loadTables(QTreeWidgetItem* databaseItem, Connection* connection, const QString& database);
    
    QMap<QString, Connection*> m_connections;
    QMap<QTreeWidgetItem*, QString> m_itemConnectionMap;
    
    QMenu* m_contextMenu;
    QAction* m_connectAction;
    QAction* m_disconnectAction;
    QAction* m_refreshAction;
    QAction* m_deleteAction;
    QAction* m_newQueryAction;
};

// 主数据库工具类
class DatabaseTool : public QWidget, public DynamicObjectBase {
    Q_OBJECT

public:
    explicit DatabaseTool(QWidget* parent = nullptr);
    ~DatabaseTool();

private slots:
    void onNewConnection();
    void onEditConnection();
    void onDeleteConnection();
    void onConnectToDatabase();
    void onDisconnectFromDatabase();
    void onRefreshConnections();
    void onNewQuery();
    void onCloseTab(int index);
    void onTabChanged(int index);
    void onConnectionStateChanged(const QString& name, ConnectionState state);
    void onTableDoubleClicked(const QString& connectionName, const QString& database, const QString& table);
    void onKeyDoubleClicked(const QString& connectionName, const QString& database, const QString& key);

private:
    void setupUI();
    void setupToolbar();
    void setupStatusBar();
    void loadConnections();
    void saveConnections();
    void updateConnectionStatus();
    
    QueryTab* createQueryTab(const QString& connectionName);
    QueryTab* getCurrentQueryTab();
    
    // UI组件
    QVBoxLayout* m_mainLayout;
    QSplitter* m_mainSplitter;
    
    // 工具栏
    QToolBar* m_toolbar;
    QAction* m_newConnectionAction;
    QAction* m_editConnectionAction;
    QAction* m_deleteConnectionAction;
    QAction* m_connectAction;
    QAction* m_disconnectAction;
    QAction* m_refreshAction;
    QAction* m_newQueryAction;
    
    // 左侧连接树
    DatabaseTreeWidget* m_treeWidget;
    
    // 右侧查询标签页
    QTabWidget* m_tabWidget;
    
    // 状态栏
    QWidget* m_statusBar;
    QLabel* m_statusLabel;
    QLabel* m_connectionCountLabel;
    
    // 数据管理
    QMap<QString, ConnectionConfig> m_connectionConfigs;
    QMap<QString, Connection*> m_connections;
    QSettings* m_settings;
    
    // 当前选中
    QString m_currentConnection;
    QTreeWidgetItem* m_currentItem;
};

#endif // DATABASETOOL_H
