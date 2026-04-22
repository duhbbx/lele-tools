#pragma once

#include <QTreeView>
#include <QMenu>
#include <QAction>
#include <QTreeWidgetItem>

#include "../../common/connx/Connection.h"
#include "abstractnodeevent.h"
#include "databasetreemodel.h"

// Forward declarations for types used from databasetool.h
enum class DbNodeType;
struct DbNodeData;

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
