#include "tabledesigner.h"
#include <QMessageBox>
#include <QSplitter>
#include <QFormLayout>
#include <QDebug>

const QStringList TableDesigner::MYSQL_TYPES = {
    "tinyint", "smallint", "mediumint", "int", "bigint",
    "float", "double", "decimal",
    "char", "varchar", "tinytext", "text", "mediumtext", "longtext",
    "binary", "varbinary", "tinyblob", "blob", "mediumblob", "longblob",
    "date", "time", "datetime", "timestamp", "year",
    "enum", "set", "json", "bit", "boolean"
};

const QStringList TableDesigner::MYSQL_CHARSETS = {
    "", "utf8mb4", "utf8mb3", "utf8", "latin1", "ascii", "binary", "gbk", "gb2312", "big5"
};

TableDesigner::TableDesigner(Connx::Connection* connection,
                             const QString& database,
                             const QString& tableName,
                             QWidget* parent)
    : QWidget(parent)
    , m_connection(connection)
    , m_database(database)
    , m_tableName(tableName)
    , m_isNewTable(tableName.isEmpty())
{
    setupUI();

    if (!m_isNewTable) {
        loadTableStructure();
        loadIndexes();
        loadForeignKeys();
        loadTriggers();
    }
}

void TableDesigner::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    setStyleSheet(R"(
        QToolBar { border-bottom: 1px solid #dee2e6; background: #f8f9fa; padding: 2px; }
        QToolBar QToolButton { font-size: 9pt; padding: 3px 8px; border: none; border-radius: 4px; color: #495057; }
        QToolBar QToolButton:hover { background: #e9ecef; }
        QTableWidget { border: none; font-size: 9pt; gridline-color: #f1f3f5; selection-background-color: #e7f5ff; selection-color: #212529; }
        QHeaderView::section { background: #f8f9fa; border: none; border-bottom: 1px solid #dee2e6; border-right: 1px solid #f1f3f5; padding: 4px 6px; font-size: 9pt; color: #495057; }
        QTabWidget::pane { border: 1px solid #dee2e6; border-top: none; }
        QTabBar::tab { padding: 4px 12px; font-size: 9pt; border: 1px solid #dee2e6; border-bottom: none; background: #f8f9fa; margin-right: -1px; }
        QTabBar::tab:selected { background: #fff; }
        QLabel { font-size: 9pt; color: #495057; }
        QLineEdit, QComboBox { font-size: 9pt; padding: 3px 6px; border: 1px solid #dee2e6; border-radius: 4px; }
        QLineEdit:focus, QComboBox:focus { border-color: #80bdff; }
        QCheckBox { font-size: 9pt; }
    )");

    setupToolbar();
    mainLayout->addWidget(m_toolbar);

    m_mainTabs = new QTabWidget();
    setupFieldsTab();
    setupIndexesTab();
    setupForeignKeysTab();
    setupTriggersTab();
    setupChecksTab();
    setupOptionsTab();
    setupCommentTab();
    setupSqlPreviewTab();

    mainLayout->addWidget(m_mainTabs, 1);
}

void TableDesigner::setupToolbar()
{
    m_toolbar = new QToolBar();
    m_toolbar->setMovable(false);
    m_toolbar->setIconSize(QSize(16, 16));

    m_saveAction = new QAction("保存", this);
    m_saveAction->setToolTip("保存表结构 (Ctrl+S)");
    m_saveAction->setShortcut(QKeySequence::Save);
    connect(m_saveAction, &QAction::triggered, this, &TableDesigner::onSave);
    m_toolbar->addAction(m_saveAction);

    m_toolbar->addSeparator();

    m_addFieldAction = new QAction("添加字段", this);
    connect(m_addFieldAction, &QAction::triggered, this, &TableDesigner::onAddField);
    m_toolbar->addAction(m_addFieldAction);

    m_insertFieldAction = new QAction("插入字段", this);
    connect(m_insertFieldAction, &QAction::triggered, this, &TableDesigner::onInsertField);
    m_toolbar->addAction(m_insertFieldAction);

    m_deleteFieldAction = new QAction("删除字段", this);
    connect(m_deleteFieldAction, &QAction::triggered, this, &TableDesigner::onDeleteField);
    m_toolbar->addAction(m_deleteFieldAction);
}

void TableDesigner::setupFieldsTab()
{
    m_fieldsWidget = new QWidget();
    auto* layout = new QVBoxLayout(m_fieldsWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 上部：字段表格
    m_fieldsTable = new QTableWidget(0, 8);
    m_fieldsTable->setHorizontalHeaderLabels({
        "名称", "类型", "长度", "小数点", "不是Null", "虚拟", "键", "注释"
    });
    m_fieldsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fieldsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_fieldsTable->verticalHeader()->setVisible(false);
    m_fieldsTable->setShowGrid(false);
    m_fieldsTable->setAlternatingRowColors(true);

    QHeaderView* header = m_fieldsTable->horizontalHeader();
    header->setSectionResizeMode(0, QHeaderView::Stretch);    // 名称 拉伸
    m_fieldsTable->setColumnWidth(1, 100);  // 类型
    m_fieldsTable->setColumnWidth(2, 60);   // 长度
    m_fieldsTable->setColumnWidth(3, 60);   // 小数点
    m_fieldsTable->setColumnWidth(4, 60);   // 不是Null
    m_fieldsTable->setColumnWidth(5, 50);   // 虚拟
    m_fieldsTable->setColumnWidth(6, 70);   // 键
    header->setSectionResizeMode(7, QHeaderView::Stretch);    // 注释 拉伸

    connect(m_fieldsTable, &QTableWidget::currentCellChanged, this, [this](int row) {
        onFieldSelectionChanged();
        Q_UNUSED(row)
    });
    connect(m_fieldsTable, &QTableWidget::cellChanged, this, &TableDesigner::onFieldDataChanged);

    layout->addWidget(m_fieldsTable, 1);

    // 下部：字段详细属性
    m_fieldDetailsWidget = new QWidget();
    m_fieldDetailsWidget->setStyleSheet("background: #f8f9fa; border-top: 1px solid #dee2e6;");
    auto* detailLayout = new QHBoxLayout(m_fieldDetailsWidget);
    detailLayout->setContentsMargins(10, 6, 10, 6);
    detailLayout->setSpacing(12);

    detailLayout->addWidget(new QLabel("默认值:"));
    m_defaultEdit = new QLineEdit();
    m_defaultEdit->setPlaceholderText("NULL");
    m_defaultEdit->setFixedWidth(120);
    detailLayout->addWidget(m_defaultEdit);

    detailLayout->addWidget(new QLabel("字符集:"));
    m_charsetCombo = new QComboBox();
    m_charsetCombo->addItems(MYSQL_CHARSETS);
    m_charsetCombo->setFixedWidth(100);
    detailLayout->addWidget(m_charsetCombo);

    detailLayout->addWidget(new QLabel("排序规则:"));
    m_collationCombo = new QComboBox();
    m_collationCombo->addItem("");
    m_collationCombo->setFixedWidth(140);
    detailLayout->addWidget(m_collationCombo);

    m_binaryCheck = new QCheckBox("二进制");
    detailLayout->addWidget(m_binaryCheck);

    detailLayout->addStretch();

    m_fieldDetailsWidget->setFixedHeight(40);
    layout->addWidget(m_fieldDetailsWidget);

    m_mainTabs->addTab(m_fieldsWidget, "字段");
}

void TableDesigner::setupIndexesTab()
{
    m_indexesTable = new QTableWidget(0, 4);
    m_indexesTable->setHorizontalHeaderLabels({"索引名", "列", "唯一", "类型"});
    m_indexesTable->horizontalHeader()->setStretchLastSection(true);
    m_indexesTable->verticalHeader()->setVisible(false);
    m_indexesTable->setShowGrid(false);
    m_indexesTable->setAlternatingRowColors(true);
    m_indexesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_indexesTable->setColumnWidth(0, 180);
    m_indexesTable->setColumnWidth(1, 200);
    m_indexesTable->setColumnWidth(2, 60);
    m_mainTabs->addTab(m_indexesTable, "索引");
}

void TableDesigner::setupForeignKeysTab()
{
    m_fkeysTable = new QTableWidget(0, 4);
    m_fkeysTable->setHorizontalHeaderLabels({"约束名", "列", "引用表", "引用列"});
    m_fkeysTable->horizontalHeader()->setStretchLastSection(true);
    m_fkeysTable->verticalHeader()->setVisible(false);
    m_fkeysTable->setShowGrid(false);
    m_fkeysTable->setAlternatingRowColors(true);
    m_fkeysTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_fkeysTable->setColumnWidth(0, 180);
    m_fkeysTable->setColumnWidth(1, 120);
    m_fkeysTable->setColumnWidth(2, 120);
    m_mainTabs->addTab(m_fkeysTable, "外键");
}

void TableDesigner::setupTriggersTab()
{
    m_triggersTable = new QTableWidget(0, 4);
    m_triggersTable->setHorizontalHeaderLabels({"触发器名", "事件", "时机", "关联表"});
    m_triggersTable->horizontalHeader()->setStretchLastSection(true);
    m_triggersTable->verticalHeader()->setVisible(false);
    m_triggersTable->setShowGrid(false);
    m_triggersTable->setAlternatingRowColors(true);
    m_triggersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_mainTabs->addTab(m_triggersTable, "触发器");
}

void TableDesigner::setupChecksTab()
{
    m_checksTable = new QTableWidget(0, 2);
    m_checksTable->setHorizontalHeaderLabels({"约束名", "表达式"});
    m_checksTable->horizontalHeader()->setStretchLastSection(true);
    m_checksTable->verticalHeader()->setVisible(false);
    m_checksTable->setShowGrid(false);
    m_checksTable->setAlternatingRowColors(true);
    m_checksTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_checksTable->setColumnWidth(0, 180);
    m_mainTabs->addTab(m_checksTable, "检查");
}

void TableDesigner::setupOptionsTab()
{
    m_optionsWidget = new QWidget();
    auto* layout = new QFormLayout(m_optionsWidget);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(8);

    m_engineCombo = new QComboBox();
    m_engineCombo->addItems({"InnoDB", "MyISAM", "MEMORY", "CSV", "ARCHIVE"});
    layout->addRow("存储引擎:", m_engineCombo);

    m_tableCharsetCombo = new QComboBox();
    m_tableCharsetCombo->addItems(MYSQL_CHARSETS);
    layout->addRow("字符集:", m_tableCharsetCombo);

    m_tableCollationCombo = new QComboBox();
    m_tableCollationCombo->addItem("");
    m_tableCollationCombo->addItem("utf8mb4_general_ci");
    m_tableCollationCombo->addItem("utf8mb4_unicode_ci");
    m_tableCollationCombo->addItem("utf8mb4_bin");
    layout->addRow("排序规则:", m_tableCollationCombo);

    m_autoIncrementEdit = new QLineEdit();
    m_autoIncrementEdit->setPlaceholderText("AUTO_INCREMENT 起始值");
    layout->addRow("自增起始:", m_autoIncrementEdit);

    m_mainTabs->addTab(m_optionsWidget, "选项");
}

void TableDesigner::setupCommentTab()
{
    m_commentEdit = new QTextEdit();
    m_commentEdit->setPlaceholderText("表注释...");
    m_mainTabs->addTab(m_commentEdit, "注释");
}

void TableDesigner::setupSqlPreviewTab()
{
    m_sqlPreview = new QTextEdit();
    m_sqlPreview->setReadOnly(true);
    m_sqlPreview->setFont(QFont("Menlo", 10));
    m_sqlPreview->setStyleSheet("background: #f8f9fa;");
    m_mainTabs->addTab(m_sqlPreview, "SQL预览");

    connect(m_mainTabs, &QTabWidget::currentChanged, this, [this](int index) {
        // 切换到 SQL 预览 tab 时更新
        if (m_mainTabs->tabText(index) == "SQL预览") {
            updateSqlPreview();
        }
    });
}

void TableDesigner::loadTableStructure()
{
    if (!m_connection || m_tableName.isEmpty()) return;

    m_connection->execute(QString("USE %1").arg(m_database));

    QString sql = QString(
        "SELECT COLUMN_NAME, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, NUMERIC_PRECISION, "
        "NUMERIC_SCALE, IS_NULLABLE, COLUMN_DEFAULT, COLUMN_KEY, COLUMN_COMMENT, "
        "CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, EXTRA "
        "FROM information_schema.COLUMNS "
        "WHERE TABLE_SCHEMA = '%1' AND TABLE_NAME = '%2' "
        "ORDER BY ORDINAL_POSITION").arg(m_database, m_tableName);

    Connx::QueryResult result = m_connection->execute(sql);
    if (!result.success) return;

    m_fieldsTable->blockSignals(true);
    for (const QVariantList& row : result.rows) {
        if (row.size() < 13) continue;

        QString name = row[0].toString();
        QString type = row[1].toString();
        QString charLen = row[2].isNull() ? "" : row[2].toString();
        QString numPrec = row[3].isNull() ? "" : row[3].toString();
        QString numScale = row[4].isNull() ? "" : row[4].toString();
        bool notNull = row[5].toString() == "NO";
        QString defaultVal = row[6].isNull() ? "" : row[6].toString();
        QString key = row[7].toString();
        QString comment = row[8].toString();
        QString charset = row[9].isNull() ? "" : row[9].toString();
        QString collation = row[10].isNull() ? "" : row[10].toString();
        QString extra = row[12].toString();

        QString length = !charLen.isEmpty() ? charLen : numPrec;
        QString decimal = numScale;

        bool isVirtual = extra.contains("VIRTUAL") || extra.contains("STORED");

        addFieldRow(name, type, length, decimal, notNull, isVirtual, key, comment,
                    defaultVal, charset, collation, false);
    }
    m_fieldsTable->blockSignals(false);

    // 加载表选项
    QString optSql = QString(
        "SELECT ENGINE, TABLE_COLLATION, TABLE_COMMENT, AUTO_INCREMENT "
        "FROM information_schema.TABLES "
        "WHERE TABLE_SCHEMA = '%1' AND TABLE_NAME = '%2'").arg(m_database, m_tableName);
    Connx::QueryResult optResult = m_connection->execute(optSql);
    if (optResult.success && !optResult.rows.isEmpty()) {
        const QVariantList& r = optResult.rows[0];
        if (r.size() >= 4) {
            int idx = m_engineCombo->findText(r[0].toString());
            if (idx >= 0) m_engineCombo->setCurrentIndex(idx);
            m_commentEdit->setPlainText(r[2].toString());
            if (!r[3].isNull()) m_autoIncrementEdit->setText(r[3].toString());
        }
    }
}

void TableDesigner::loadIndexes()
{
    if (!m_connection) return;

    QString sql = QString(
        "SELECT INDEX_NAME, GROUP_CONCAT(COLUMN_NAME ORDER BY SEQ_IN_INDEX), "
        "CASE WHEN NON_UNIQUE = 0 THEN 'YES' ELSE 'NO' END, INDEX_TYPE "
        "FROM information_schema.STATISTICS "
        "WHERE TABLE_SCHEMA = '%1' AND TABLE_NAME = '%2' "
        "GROUP BY INDEX_NAME, NON_UNIQUE, INDEX_TYPE "
        "ORDER BY INDEX_NAME").arg(m_database, m_tableName);

    Connx::QueryResult result = m_connection->execute(sql);
    if (!result.success) return;

    m_indexesTable->setRowCount(result.rows.size());
    for (int i = 0; i < result.rows.size(); ++i) {
        const auto& row = result.rows[i];
        if (row.size() >= 4) {
            m_indexesTable->setItem(i, 0, new QTableWidgetItem(row[0].toString()));
            m_indexesTable->setItem(i, 1, new QTableWidgetItem(row[1].toString()));
            m_indexesTable->setItem(i, 2, new QTableWidgetItem(row[2].toString()));
            m_indexesTable->setItem(i, 3, new QTableWidgetItem(row[3].toString()));
        }
        m_indexesTable->setRowHeight(i, 28);
    }
}

void TableDesigner::loadForeignKeys()
{
    if (!m_connection) return;

    QString sql = QString(
        "SELECT CONSTRAINT_NAME, COLUMN_NAME, REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME "
        "FROM information_schema.KEY_COLUMN_USAGE "
        "WHERE TABLE_SCHEMA = '%1' AND TABLE_NAME = '%2' "
        "AND REFERENCED_TABLE_NAME IS NOT NULL").arg(m_database, m_tableName);

    Connx::QueryResult result = m_connection->execute(sql);
    if (!result.success) return;

    m_fkeysTable->setRowCount(result.rows.size());
    for (int i = 0; i < result.rows.size(); ++i) {
        const auto& row = result.rows[i];
        if (row.size() >= 4) {
            m_fkeysTable->setItem(i, 0, new QTableWidgetItem(row[0].toString()));
            m_fkeysTable->setItem(i, 1, new QTableWidgetItem(row[1].toString()));
            m_fkeysTable->setItem(i, 2, new QTableWidgetItem(row[2].toString()));
            m_fkeysTable->setItem(i, 3, new QTableWidgetItem(row[3].toString()));
        }
        m_fkeysTable->setRowHeight(i, 28);
    }
}

void TableDesigner::loadTriggers()
{
    if (!m_connection) return;

    QString sql = QString(
        "SELECT TRIGGER_NAME, EVENT_MANIPULATION, ACTION_TIMING, EVENT_OBJECT_TABLE "
        "FROM information_schema.TRIGGERS "
        "WHERE EVENT_OBJECT_SCHEMA = '%1' AND EVENT_OBJECT_TABLE = '%2'").arg(m_database, m_tableName);

    Connx::QueryResult result = m_connection->execute(sql);
    if (!result.success) return;

    m_triggersTable->setRowCount(result.rows.size());
    for (int i = 0; i < result.rows.size(); ++i) {
        const auto& row = result.rows[i];
        if (row.size() >= 4) {
            m_triggersTable->setItem(i, 0, new QTableWidgetItem(row[0].toString()));
            m_triggersTable->setItem(i, 1, new QTableWidgetItem(row[1].toString()));
            m_triggersTable->setItem(i, 2, new QTableWidgetItem(row[2].toString()));
            m_triggersTable->setItem(i, 3, new QTableWidgetItem(row[3].toString()));
        }
        m_triggersTable->setRowHeight(i, 28);
    }
}

void TableDesigner::addFieldRow(const QString& name, const QString& type,
                                 const QString& length, const QString& decimal,
                                 bool notNull, bool isVirtual,
                                 const QString& key, const QString& comment,
                                 const QString& defaultVal, const QString& charset,
                                 const QString& collation, bool binary)
{
    int row = m_fieldsTable->rowCount();
    m_fieldsTable->insertRow(row);

    // 名称
    m_fieldsTable->setItem(row, 0, new QTableWidgetItem(name));

    // 类型（下拉框）
    auto* typeCombo = new QComboBox();
    typeCombo->addItems(MYSQL_TYPES);
    typeCombo->setEditable(true);
    int idx = typeCombo->findText(type, Qt::MatchFixedString);
    if (idx >= 0) typeCombo->setCurrentIndex(idx);
    else typeCombo->setCurrentText(type);
    m_fieldsTable->setCellWidget(row, 1, typeCombo);

    // 长度
    m_fieldsTable->setItem(row, 2, new QTableWidgetItem(length));

    // 小数点
    m_fieldsTable->setItem(row, 3, new QTableWidgetItem(decimal));

    // 不是Null（checkbox）
    auto* notNullWidget = new QWidget();
    auto* notNullLayout = new QHBoxLayout(notNullWidget);
    notNullLayout->setContentsMargins(0, 0, 0, 0);
    notNullLayout->setAlignment(Qt::AlignCenter);
    auto* notNullCheck = new QCheckBox();
    notNullCheck->setChecked(notNull);
    notNullLayout->addWidget(notNullCheck);
    m_fieldsTable->setCellWidget(row, 4, notNullWidget);

    // 虚拟（checkbox）
    auto* virtualWidget = new QWidget();
    auto* virtualLayout = new QHBoxLayout(virtualWidget);
    virtualLayout->setContentsMargins(0, 0, 0, 0);
    virtualLayout->setAlignment(Qt::AlignCenter);
    auto* virtualCheck = new QCheckBox();
    virtualCheck->setChecked(isVirtual);
    virtualLayout->addWidget(virtualCheck);
    m_fieldsTable->setCellWidget(row, 5, virtualWidget);

    // 键（下拉框）
    auto* keyCombo = new QComboBox();
    keyCombo->addItems({"", "PRI", "UNI", "MUL"});
    idx = keyCombo->findText(key);
    if (idx >= 0) keyCombo->setCurrentIndex(idx);
    m_fieldsTable->setCellWidget(row, 6, keyCombo);

    // 注释
    m_fieldsTable->setItem(row, 7, new QTableWidgetItem(comment));

    // 存储额外属性到 item data
    QTableWidgetItem* nameItem = m_fieldsTable->item(row, 0);
    nameItem->setData(Qt::UserRole, defaultVal);       // 默认值
    nameItem->setData(Qt::UserRole + 1, charset);      // 字符集
    nameItem->setData(Qt::UserRole + 2, collation);    // 排序规则
    nameItem->setData(Qt::UserRole + 3, binary);       // 二进制

    m_fieldsTable->setRowHeight(row, 28);
}

void TableDesigner::onAddField()
{
    m_fieldsTable->blockSignals(true);
    addFieldRow();
    m_fieldsTable->blockSignals(false);
    m_fieldsTable->setCurrentCell(m_fieldsTable->rowCount() - 1, 0);
    m_fieldsTable->editItem(m_fieldsTable->item(m_fieldsTable->rowCount() - 1, 0));
    m_modified = true;
}

void TableDesigner::onInsertField()
{
    int currentRow = m_fieldsTable->currentRow();
    if (currentRow < 0) currentRow = 0;

    m_fieldsTable->blockSignals(true);
    m_fieldsTable->insertRow(currentRow);

    m_fieldsTable->setItem(currentRow, 0, new QTableWidgetItem(""));
    auto* typeCombo = new QComboBox();
    typeCombo->addItems(MYSQL_TYPES);
    typeCombo->setEditable(true);
    typeCombo->setCurrentText("varchar");
    m_fieldsTable->setCellWidget(currentRow, 1, typeCombo);
    m_fieldsTable->setItem(currentRow, 2, new QTableWidgetItem("255"));
    m_fieldsTable->setItem(currentRow, 3, new QTableWidgetItem(""));

    auto* notNullWidget = new QWidget();
    auto* notNullLayout = new QHBoxLayout(notNullWidget);
    notNullLayout->setContentsMargins(0, 0, 0, 0);
    notNullLayout->setAlignment(Qt::AlignCenter);
    notNullLayout->addWidget(new QCheckBox());
    m_fieldsTable->setCellWidget(currentRow, 4, notNullWidget);

    auto* virtualWidget = new QWidget();
    auto* virtualLayout = new QHBoxLayout(virtualWidget);
    virtualLayout->setContentsMargins(0, 0, 0, 0);
    virtualLayout->setAlignment(Qt::AlignCenter);
    virtualLayout->addWidget(new QCheckBox());
    m_fieldsTable->setCellWidget(currentRow, 5, virtualWidget);

    auto* keyCombo = new QComboBox();
    keyCombo->addItems({"", "PRI", "UNI", "MUL"});
    m_fieldsTable->setCellWidget(currentRow, 6, keyCombo);
    m_fieldsTable->setItem(currentRow, 7, new QTableWidgetItem(""));
    m_fieldsTable->setRowHeight(currentRow, 28);

    m_fieldsTable->blockSignals(false);
    m_fieldsTable->setCurrentCell(currentRow, 0);
    m_modified = true;
}

void TableDesigner::onDeleteField()
{
    int row = m_fieldsTable->currentRow();
    if (row < 0) return;

    QString name = m_fieldsTable->item(row, 0) ? m_fieldsTable->item(row, 0)->text() : "";
    if (QMessageBox::question(this, "确认", QString("确定删除字段 \"%1\" 吗？").arg(name))
        != QMessageBox::Yes) return;

    m_fieldsTable->removeRow(row);
    m_modified = true;
}

void TableDesigner::onFieldSelectionChanged()
{
    int row = m_fieldsTable->currentRow();
    if (row < 0) return;
    updateFieldDetails(row);
}

void TableDesigner::updateFieldDetails(int row)
{
    QTableWidgetItem* nameItem = m_fieldsTable->item(row, 0);
    if (!nameItem) return;

    m_defaultEdit->blockSignals(true);
    m_charsetCombo->blockSignals(true);
    m_collationCombo->blockSignals(true);
    m_binaryCheck->blockSignals(true);

    m_defaultEdit->setText(nameItem->data(Qt::UserRole).toString());

    QString charset = nameItem->data(Qt::UserRole + 1).toString();
    int idx = m_charsetCombo->findText(charset);
    m_charsetCombo->setCurrentIndex(idx >= 0 ? idx : 0);

    QString collation = nameItem->data(Qt::UserRole + 2).toString();
    idx = m_collationCombo->findText(collation);
    if (idx < 0) { m_collationCombo->addItem(collation); idx = m_collationCombo->count() - 1; }
    m_collationCombo->setCurrentIndex(idx);

    m_binaryCheck->setChecked(nameItem->data(Qt::UserRole + 3).toBool());

    m_defaultEdit->blockSignals(false);
    m_charsetCombo->blockSignals(false);
    m_collationCombo->blockSignals(false);
    m_binaryCheck->blockSignals(false);
}

void TableDesigner::onFieldDataChanged(int row, int column)
{
    Q_UNUSED(row) Q_UNUSED(column)
    m_modified = true;
}

void TableDesigner::updateSqlPreview()
{
    m_sqlPreview->setPlainText(generateAlterSql());
}

QString TableDesigner::generateAlterSql() const
{
    QString sql;

    if (m_isNewTable) {
        sql = QString("CREATE TABLE `%1`.`%2` (\n").arg(m_database, m_tableName.isEmpty() ? "new_table" : m_tableName);
    } else {
        sql = QString("-- 表结构: %1.%2\n\n").arg(m_database, m_tableName);
        sql += QString("CREATE TABLE `%1`.`%2` (\n").arg(m_database, m_tableName);
    }

    QStringList columnDefs;
    QStringList primaryKeys;

    for (int i = 0; i < m_fieldsTable->rowCount(); ++i) {
        QTableWidgetItem* nameItem = m_fieldsTable->item(i, 0);
        if (!nameItem || nameItem->text().trimmed().isEmpty()) continue;

        QString name = nameItem->text().trimmed();
        QString type;
        if (auto* combo = qobject_cast<QComboBox*>(m_fieldsTable->cellWidget(i, 1)))
            type = combo->currentText();
        QString length = m_fieldsTable->item(i, 2) ? m_fieldsTable->item(i, 2)->text().trimmed() : "";
        QString decimal = m_fieldsTable->item(i, 3) ? m_fieldsTable->item(i, 3)->text().trimmed() : "";

        bool notNull = false;
        if (auto* w = m_fieldsTable->cellWidget(i, 4))
            if (auto* cb = w->findChild<QCheckBox*>())
                notNull = cb->isChecked();

        QString key;
        if (auto* combo = qobject_cast<QComboBox*>(m_fieldsTable->cellWidget(i, 6)))
            key = combo->currentText();

        QString comment = m_fieldsTable->item(i, 7) ? m_fieldsTable->item(i, 7)->text().trimmed() : "";
        QString defaultVal = nameItem->data(Qt::UserRole).toString();

        // 构建列定义
        QString colDef = QString("  `%1` %2").arg(name, type);
        if (!length.isEmpty()) {
            if (!decimal.isEmpty())
                colDef += QString("(%1,%2)").arg(length, decimal);
            else
                colDef += QString("(%1)").arg(length);
        }
        if (notNull) colDef += " NOT NULL";
        else colDef += " NULL";
        if (!defaultVal.isEmpty()) colDef += QString(" DEFAULT '%1'").arg(defaultVal);
        if (!comment.isEmpty()) colDef += QString(" COMMENT '%1'").arg(comment);

        columnDefs.append(colDef);

        if (key == "PRI") primaryKeys.append(QString("`%1`").arg(name));
    }

    if (!primaryKeys.isEmpty()) {
        columnDefs.append(QString("  PRIMARY KEY (%1)").arg(primaryKeys.join(", ")));
    }

    sql += columnDefs.join(",\n");
    sql += "\n)";

    // 表选项
    if (m_engineCombo->currentText() != "InnoDB" || !m_engineCombo->currentText().isEmpty())
        sql += QString(" ENGINE=%1").arg(m_engineCombo->currentText());
    if (!m_tableCharsetCombo->currentText().isEmpty())
        sql += QString(" DEFAULT CHARSET=%1").arg(m_tableCharsetCombo->currentText());
    if (!m_commentEdit->toPlainText().trimmed().isEmpty())
        sql += QString(" COMMENT='%1'").arg(m_commentEdit->toPlainText().trimmed());

    sql += ";\n";

    return sql;
}

void TableDesigner::onSave()
{
    if (!m_connection) {
        QMessageBox::warning(this, "错误", "数据库连接无效");
        return;
    }

    QString sql = generateAlterSql();
    if (sql.trimmed().isEmpty()) return;

    // 这里简化处理：对于已有表，先提示用户确认 SQL
    if (!m_isNewTable) {
        QMessageBox::information(this, "SQL 预览",
            "请切换到 SQL 预览 tab 查看生成的 DDL 语句。\n"
            "当前版本暂不支持直接修改表结构，请复制 SQL 手动执行。");
        m_mainTabs->setCurrentIndex(m_mainTabs->count() - 1); // 切换到 SQL 预览
        updateSqlPreview();
        return;
    }

    Connx::QueryResult result = m_connection->execute(sql);
    if (result.success) {
        QMessageBox::information(this, "成功", "表创建成功");
        m_modified = false;
        emit tableSaved();
    } else {
        QMessageBox::critical(this, "错误", "执行失败:\n" + result.errorMessage);
    }
}
