#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QPushButton>
#include <QAction>
#include <QHeaderView>

#include "../../common/connx/Connection.h"

class TableDesigner : public QWidget
{
    Q_OBJECT

public:
    explicit TableDesigner(Connx::Connection* connection,
                           const QString& database,
                           const QString& tableName,
                           QWidget* parent = nullptr);

signals:
    void titleChanged(const QString& title);
    void tableSaved();

private slots:
    void onSave();
    void onAddField();
    void onInsertField();
    void onDeleteField();
    void onFieldSelectionChanged();
    void onFieldDataChanged(int row, int column);

private:
    void setupUI();
    void setupToolbar();
    void setupFieldsTab();
    void setupIndexesTab();
    void setupForeignKeysTab();
    void setupTriggersTab();
    void setupChecksTab();
    void setupOptionsTab();
    void setupCommentTab();
    void setupSqlPreviewTab();
    void loadTableStructure();
    void loadIndexes();
    void loadForeignKeys();
    void loadTriggers();
    void updateFieldDetails(int row);
    void updateSqlPreview();
    QString generateAlterSql() const;
    void addFieldRow(const QString& name = "", const QString& type = "varchar",
                     const QString& length = "255", const QString& decimal = "",
                     bool notNull = false, bool isVirtual = false,
                     const QString& key = "", const QString& comment = "",
                     const QString& defaultVal = "", const QString& charset = "",
                     const QString& collation = "", bool binary = false);

    Connx::Connection* m_connection;
    QString m_database;
    QString m_tableName;
    bool m_isNewTable;
    bool m_modified = false;

    // Toolbar
    QToolBar* m_toolbar;
    QAction* m_saveAction;
    QAction* m_addFieldAction;
    QAction* m_insertFieldAction;
    QAction* m_deleteFieldAction;

    // Main tabs
    QTabWidget* m_mainTabs;

    // Fields tab
    QWidget* m_fieldsWidget;
    QTableWidget* m_fieldsTable;
    // Field details (bottom panel)
    QWidget* m_fieldDetailsWidget;
    QLineEdit* m_defaultEdit;
    QComboBox* m_charsetCombo;
    QComboBox* m_collationCombo;
    QCheckBox* m_binaryCheck;

    // Index tab
    QTableWidget* m_indexesTable;

    // Foreign keys tab
    QTableWidget* m_fkeysTable;

    // Triggers tab
    QTableWidget* m_triggersTable;

    // Checks tab
    QTableWidget* m_checksTable;

    // Options tab
    QWidget* m_optionsWidget;
    QComboBox* m_engineCombo;
    QComboBox* m_tableCharsetCombo;
    QComboBox* m_tableCollationCombo;
    QLineEdit* m_autoIncrementEdit;

    // Comment tab
    QTextEdit* m_commentEdit;

    // SQL preview tab
    QTextEdit* m_sqlPreview;

    // MySQL type list
    static const QStringList MYSQL_TYPES;
    static const QStringList MYSQL_CHARSETS;
};
