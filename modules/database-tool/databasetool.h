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

// 不使用全局using namespace以避免命名冲突
// using namespace Connx;
// using namespace SqliteWrapper;

// 数据库节点类型枚举
enum class DbNodeType {
    Connection,        // 连接
    Database,          // 数据库
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
    Loading           // 加载中
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

    // 获取节点显示名称
    QString getNodeDisplayName(DbNodeType nodeType) const;

private:
    DatabaseHierarchyManager();
    void initializeHierarchies();

    QMap<Connx::ConnectionType, QList<DbHierarchyNode>> m_hierarchies;
    QMap<DbNodeType, QString> m_nodeIcons;
    QMap<DbNodeType, QString> m_nodeNames;
};

// SQL语法高亮器
class SqlSyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit SqlSyntaxHighlighter(QTextDocument* parent = nullptr);

protected:
    void highlightBlock(const QString& text) override;

private:
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<HighlightingRule> highlightingRules;

    QTextCharFormat keywordFormat;
    QTextCharFormat functionFormat;
    QTextCharFormat stringFormat;
    QTextCharFormat commentFormat;
    QTextCharFormat numberFormat;
    QTextCharFormat operatorFormat;
};

// 现代化的连接类型列表项委托
class ConnectionTypeDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit ConnectionTypeDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        painter->setRenderHint(QPainter::Antialiasing);

        QRect rect = option.rect;
        QString text = index.data(Qt::DisplayRole).toString();
        QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();

        // 背景绘制
        QColor backgroundColor;
        if (option.state & QStyle::State_Selected) {
            backgroundColor = QColor(0xe7, 0xf5, 0xff);  // 浅蓝
        } else if (option.state & QStyle::State_MouseOver) {
            backgroundColor = QColor(0xe9, 0xec, 0xef);
        } else {
            backgroundColor = Qt::transparent;
        }

        painter->setBrush(backgroundColor);
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(rect.adjusted(4, 2, -4, -2), 4, 4);

        // 文字绘制
        QColor textColor = (option.state & QStyle::State_Selected) ?
                          QColor(0x22, 0x8b, 0xe6) : QColor(0x49, 0x50, 0x57);
        painter->setPen(textColor);

        QFont font = painter->font();
        font.setPointSize(9);
        font.setWeight((option.state & QStyle::State_Selected) ? QFont::Bold : QFont::Normal);
        painter->setFont(font);

        QRect textRect = rect.adjusted(36, 0, -6, 0);
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text);

        // 图标绘制
        if (!icon.isNull()) {
            QRect iconRect(rect.left() + 12, rect.center().y() - 7, 14, 14);
            icon.paint(painter, iconRect);
        }
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        Q_UNUSED(option)
        Q_UNUSED(index)
        return QSize(140, 32);
    }
};

// 连接配置对话框
class ConnectionDialog : public QDialog {
    Q_OBJECT

public:
    explicit ConnectionDialog(QWidget* parent = nullptr);
    explicit ConnectionDialog(const Connx::ConnectionConfig& config, QWidget* parent = nullptr);
    
    Connx::ConnectionConfig getConnectionConfig() const;
    void setConnectionConfig(const Connx::ConnectionConfig& config) const;

private slots:
    void onConnectionTypeChanged();
    void onTestConnection();
    void onAccept();

private:
    void setupUI();
    void setupLeftPanel();
    void setupRightPanel();
    void createFormControls();
    void updateFormForType(const QString& type) const;
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
    QGridLayout* m_gridLayout;
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
    explicit QueryTab(Connx::Connection* connection, QWidget* parent = nullptr);
    
    void setQuery(const QString& query);
    QString getQuery() const;
    void executeQuery();

signals:
    void titleChanged(const QString& title);
    void queryExecuted(const Connx::QueryResult& result);

private slots:
    void onExecuteClicked();
    void onClearClicked() const;
    void onFormatClicked() const;

private:
    void setupUI();
    void updateResults(const Connx::QueryResult& result) const;
    
    Connx::Connection* m_connection;
    QVBoxLayout* m_layout;
    
    // 工具栏
    QToolBar* m_toolbar;
    QAction* m_executeAction;
    QAction* m_clearAction;
    QAction* m_formatAction;
    
    // 查询区域
    QTextEdit* m_queryEdit;
    SqlSyntaxHighlighter* m_syntaxHighlighter;

    // 结果区域
    QTabWidget* m_resultTabs;
    QTableWidget* m_resultTable;
    QTextEdit* m_messagesText;
    
    // 状态栏
    QLabel* m_statusLabel;
    QLabel* m_timeLabel;
    QLabel* m_rowsLabel;
};

// 高性能数据库浏览器树
class DatabaseTreeView : public QTreeView {
    Q_OBJECT

public:
    explicit DatabaseTreeView(QWidget* parent = nullptr);

    void refreshConnection(const QString& connectionName);
    void addConnection(const QString& name, Connx::Connection* connection) const;
    void removeConnection(const QString& name);

signals:
    void tableDoubleClicked(const QString& connectionName, const QString& database, const QString& table);
    void keyDoubleClicked(const QString& connectionName, const QString& database, const QString& key);
    void connectionRequested(const QString& connectionName);
    void disconnectionRequested(const QString& connectionName);

private slots:
    void onNodeDoubleClicked(const QModelIndex& index);
    void onCustomContextMenuRequested(const QPoint& pos);
    void onNodeExpanded(const QModelIndex& index);

private:
    void setupUI();
    void setupContextMenu();
    void handleNodeDoubleClick(const QModelIndex& index);
    void showNodeContextMenu(const QModelIndex& index, const QPoint& pos);

    // Helper methods for tree operations
    NodeChain buildNodeChainFromIndex(const QModelIndex& index);
    void showExpandError(const QString& title, const QString& message);
    NodeType convertDbNodeTypeToNodeType(DbNodeType dbNodeType);
    bool canExpandNodeType(NodeType type);
    bool canNodeExpand(NodeType nodeType);

    // Deprecated methods (kept for compatibility)
    void loadFolderContent(QTreeWidgetItem* folderItem, Connx::Connection* connection, const QString& database, DbNodeType folderType);
    void loadTables(QTreeWidgetItem* databaseItem, Connx::Connection* connection, const QString& database);
    void loadRedisKeys(QTreeWidgetItem* databaseItem, Connx::Connection* connection, const QString& database);
    void refreshDatabase(QTreeWidgetItem* databaseItem);
    void flushDatabase(QTreeWidgetItem* databaseItem);
    void deleteKey(QTreeWidgetItem* keyItem);
    void setConnection(Connx::Connection* connection);
    void updateTreeItemWithNodes(QTreeWidgetItem* parentItem, const QList<Node>& nodes);
    void populateTreeItemData(QTreeWidgetItem* item, const Node& node);
    void setNodeIcon(QTreeWidgetItem* item, NodeType nodeType);
    NodeChain buildNodeChainFromTreeItem_DEPRECATED(QTreeWidgetItem* item);
    Node createNodeFromTreeItem(QTreeWidgetItem* item);
    DbNodeType convertNodeTypeToDbNodeType(NodeType nodeType);
    QString generateNodeTooltip(const Node& node);
    QString getNodeTypeDisplayName(NodeType type);

public:
    DatabaseTreeModel* m_treeModel;  // Made public for access from DatabaseTool
    QAction* m_newQueryAction;

private:

    QMenu* m_contextMenu;
    QAction* m_connectAction;
    QAction* m_disconnectAction;
    QAction* m_refreshAction;
    QAction* m_refreshDatabaseAction;
    QAction* m_flushDatabaseAction;
    QAction* m_deleteKeyAction;
    QAction* m_deleteAction;
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
