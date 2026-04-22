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
#include <QGridLayout>
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
#include <QStyledItemDelegate>
#include <QPainter>
#include <QPropertyAnimation>
#include <QGraphicsDropShadowEffect>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QTextDocument>

#include "../../common/dynamicobjectbase.h"
#include "../../common/connx/Connection.h"
#include "../../common/sqlite/ConnectionConfigTableManager.h"
#include "abstractnodeevent.h"
#include "nodeeventfactory.h"
#include "databasetreemodel.h"

#include "connectiondialog.h"
#include "databasetreeview.h"
#include "querytab.h"
#include "tabledesigner.h"
#include "createdialog.h"

// 不使用全局using namespace以避免命名冲突
// using namespace Connx;
// using namespace SqliteWrapper;

// 数据库节点类型枚举
enum class DbNodeType {
    Connection,        // 连��
    Database,          // 数���库
    Schema,           // 模式
    TableFolder,      // 表文件夹
    Table,            // 表
    ViewFolder,       // 视图文件夹
    View,             // 视图
    ProcedureFolder,  // 存储过程文件夹
    Procedure,        // 存储过程
    FunctionFolder,   // 函数文件夹
    Function,         // 函数
    TriggerFolder,    // 触发器文件夹
    Trigger,          // 触发器
    IndexFolder,      // 索引文件夹
    Index,            // 索引
    SequenceFolder,   // 序列文件夹
    Sequence,         // 序列
    UserFolder,       // 用户文件夹
    User,             // 用户
    Column,           // 列
    Key,              // 键
    RedisKey,         // Redis键
    Loading           // 加载��
};

// 数据库节点数据结构
struct DbNodeData {
    DbNodeType type;
    QString id;           // 唯一标识符
    QString name;         // 实际名称（不带emoji，用于查询等逻辑）
    QString displayName;  // 显示名称（带emoji，用于UI显示）
    QString fullName;     // 完整名称（包含schema等）
    QString schema;       // 模式名
    QString database;     // 数据库名
    QVariantMap metadata; // 额外元数据
    bool isLoaded;        // 是否已加载子节点
    bool isExpanded;      // 是否已展开

    DbNodeData() : type(DbNodeType::Loading), isLoaded(false), isExpanded(false) {}
    DbNodeData(DbNodeType t, const QString& nodeId, const QString& nodeName)
        : type(t), id(nodeId), name(nodeName), isLoaded(false), isExpanded(false) {}
};

// 自定义树节点项
class DbTreeWidgetItem : public QTreeWidgetItem {
public:
    DbTreeWidgetItem(QTreeWidget* parent, const DbNodeData& data);
    DbTreeWidgetItem(QTreeWidgetItem* parent, const DbNodeData& data);

    DbNodeData nodeData() const { return m_nodeData; }
    void setNodeData(const DbNodeData& data) { m_nodeData = data; updateDisplay(); }

    void setLoading(bool loading);
    void markAsLoaded() { m_nodeData.isLoaded = true; }
    bool isLoaded() const { return m_nodeData.isLoaded; }

private:
    void updateDisplay();
    QString getNodeIcon() const;
    QString getNodeEmoji() const;

    DbNodeData m_nodeData;
};

// 数据库层级结构定义
struct DbHierarchyNode {
    DbNodeType nodeType;
    QString displayName;
    QString iconPath;
    QList<DbHierarchyNode> children;

    DbHierarchyNode(DbNodeType type, const QString& name, const QString& icon = "")
        : nodeType(type), displayName(name), iconPath(icon) {}
};

// 数据库类型层级管理器
class DatabaseHierarchyManager {
public:
    static DatabaseHierarchyManager& instance();

    // 获取特定数据库类型的层级结构
    QList<DbHierarchyNode> getHierarchy(Connx::ConnectionType dbType) const;

    // 检查节点是否需要延迟加载
    bool needsLazyLoading(DbNodeType nodeType) const;

    // 获取节点图标
    QString getNodeIcon(DbNodeType nodeType) const;

    // 获取节点显示名���
    QString getNodeDisplayName(DbNodeType nodeType) const;

private:
    DatabaseHierarchyManager();
    void initializeHierarchies();

    QMap<Connx::ConnectionType, QList<DbHierarchyNode>> m_hierarchies;
    QMap<DbNodeType, QString> m_nodeIcons;
    QMap<DbNodeType, QString> m_nodeNames;
};

// 主数据库工具类
class DatabaseTool final : public QWidget, public DynamicObjectBase {
    Q_OBJECT

public:
    explicit DatabaseTool(QWidget* parent = nullptr);
    ~DatabaseTool() override;

private slots:
    void onNewConnection();
    void onEditConnection();
    void onDeleteConnection();
    void onConnectToDatabase(const QString& connectionName = "");
    void onDisconnectFromDatabase();
    void onRefreshConnections();
    void onNewQuery();
    void onCloseTab(int index) const;
    void onTabChanged(int index);
    void onConnectionStateChanged(const QString& name, Connx::ConnectionState state);
    void onTableDoubleClicked(const QString& connectionName, const QString& database, const QString& table);
    void onKeyDoubleClicked(const QString& connectionName, const QString& database, const QString& key);
    void onDesignTable(const QString& connectionName, const QString& database, const QString& tableName);
    void onCreateDatabase(const QString& connectionName);

private:
    void setupUI();
    void setupToolbar();
    void setupStatusBar();
    void loadConnections();
    static void saveConnections();
    void updateConnectionStatus() const;

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
    DatabaseTreeView* m_treeView;

    // 右侧查询标签页
    QTabWidget* m_tabWidget;

    // 状态栏
    QWidget* m_statusBar;
    QLabel* m_statusLabel;
    QLabel* m_connectionCountLabel;

    // 数据管理
    QMap<QString, Connx::ConnectionConfig> m_connectionConfigs;
    QMap<QString, Connx::Connection*> m_connections;
    SqliteWrapper::ConnectionConfigTableManager* m_configManager;

    // 当前选中
    QString m_currentConnection;
    QTreeWidgetItem* m_currentItem;
};

#endif // DATABASETOOL_H
