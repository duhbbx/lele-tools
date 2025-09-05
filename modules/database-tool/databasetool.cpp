#include "databasetool.h"
#include "../../common/connx/RedisConnection.h"
#include <QApplication>
#include <QMessageBox>
#include <QInputDialog>
#include <QHeaderView>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>

REGISTER_DYNAMICOBJECT(DatabaseTool);

// ConnectionDialog 实现
ConnectionDialog::ConnectionDialog(QWidget* parent) : QDialog(parent) {
    setupUI();
    setWindowTitle("新建连接");
}

ConnectionDialog::ConnectionDialog(const ConnectionConfig& config, QWidget* parent) : QDialog(parent) {
    setupUI();
    setConnectionConfig(config);
    setWindowTitle("编辑连接");
}

void ConnectionDialog::setupUI() {
    setModal(true);
    setFixedSize(650, 450); // 增大对话框尺寸
    
    // 主分割器布局
    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    m_mainLayout->setSpacing(10);
    
    m_splitter = new QSplitter(Qt::Horizontal);
    
    setupLeftPanel();
    setupRightPanel();
    
    m_splitter->addWidget(m_leftPanel);
    m_splitter->addWidget(m_rightPanel);
    m_splitter->setStretchFactor(0, 0); // 左侧固定
    m_splitter->setStretchFactor(1, 1); // 右侧拉伸
    m_splitter->setSizes({180, 450});
    
    m_mainLayout->addWidget(m_splitter);
    
    // 设置样式
    setStyleSheet(R"(
        QListWidget {
            border: 1px solid #ddd;
            background: white;
            font-size: 11pt;
        }
        QListWidget::item {
            padding: 8px;
            border-bottom: 1px solid #eee;
        }
        QListWidget::item:selected {
            background: #007acc;
            color: white;
        }
        QLineEdit, QSpinBox, QComboBox {
            padding: 8px;
            border: 2px solid #ddd;
            border-radius: 4px;
            font-size: 11pt;
            min-height: 20px;
        }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus {
            border-color: #007acc;
        }
        QPushButton {
            padding: 8px 16px;
            font-size: 11pt;
            border: 1px solid #ddd;
            border-radius: 4px;
            background: #f8f9fa;
        }
        QPushButton:hover {
            background: #e9ecef;
        }
        QLabel {
            font-size: 11pt;
        }
    )");
}

ConnectionConfig ConnectionDialog::getConnectionConfig() const {
    ConnectionConfig config;
    config.name = m_nameEdit->text().trimmed();
    config.host = m_hostEdit->text().trimmed();
    config.port = m_portSpin->value();
    config.timeout = m_timeoutSpin->value();
    config.useSSL = m_sslCheck->isChecked();
    
    // 根据数据库类型设置特定配置
    if (m_currentType == "Redis") {
        config.username = ""; // Redis通常不使用用户名
        config.password = m_passwordEdit->text();
        config.database = QString::number(m_dbIndexSpin->value());
    } else {
        config.username = m_usernameEdit->text().trimmed();
        config.password = m_passwordEdit->text();
        config.database = m_databaseEdit->text().trimmed();
        
        // MySQL特有配置
        if (m_currentType == "MySQL") {
            config.extraParams["charset"] = m_charsetCombo->currentText();
        }
    }
    
    return config;
}

void ConnectionDialog::setupLeftPanel() {
    m_leftPanel = new QWidget();
    m_leftPanel->setFixedWidth(160);
    m_leftLayout = new QVBoxLayout(m_leftPanel);
    m_leftLayout->setContentsMargins(5, 5, 5, 5);
    m_leftLayout->setSpacing(8);
    
    // 数据库类型标题
    m_typeLabel = new QLabel("数据库类型");
    m_typeLabel->setStyleSheet("font-weight: bold; font-size: 12pt; color: #333; padding: 5px;");
    m_leftLayout->addWidget(m_typeLabel);
    
    // 数据库类型列表
    m_typeList = new QListWidget();
    m_typeList->setFixedWidth(150);
    
    // 添加数据库类型项
    QStringList types = ConnectionFactory::getSupportedTypes();
    for (const QString& type : types) {
        QListWidgetItem* item = new QListWidgetItem(type);
        
        // 设置图标
        if (type == "Redis") {
            item->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
            item->setData(Qt::UserRole, "Redis数据库\n内存型键值存储\n支持多种数据结构");
        } else if (type == "MySQL") {
            item->setIcon(style()->standardIcon(QStyle::SP_DriveHDIcon));
            item->setData(Qt::UserRole, "MySQL数据库\n关系型数据库\n支持SQL查询");
        } else if (type == "PostgreSQL") {
            item->setIcon(style()->standardIcon(QStyle::SP_DriveNetIcon));
            item->setData(Qt::UserRole, "PostgreSQL数据库\n对象关系型数据库\n高级SQL特性");
        } else {
            item->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
            item->setData(Qt::UserRole, type + "数据库");
        }
        
        m_typeList->addItem(item);
    }
    
    // 默认选择Redis
    m_typeList->setCurrentRow(0);
    m_currentType = "Redis";
    
    QObject::connect(m_typeList, &QListWidget::currentTextChanged, 
                    this, &ConnectionDialog::onConnectionTypeChanged);
    
    m_leftLayout->addWidget(m_typeList);
    m_leftLayout->addStretch();
}

void ConnectionDialog::setupRightPanel() {
    m_rightPanel = new QWidget();
    m_rightLayout = new QVBoxLayout(m_rightPanel);
    m_rightLayout->setContentsMargins(10, 5, 5, 5);
    m_rightLayout->setSpacing(10);
    
    // 表单区域
    m_formWidget = new QWidget();
    m_formLayout = new QFormLayout(m_formWidget);
    m_formLayout->setLabelAlignment(Qt::AlignRight);
    m_formLayout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_formLayout->setVerticalSpacing(12);
    
    // 创建所有表单控件
    createFormControls();
    
    m_rightLayout->addWidget(m_formWidget, 1);
    
    // 底部按钮区域
    m_buttonWidget = new QWidget();
    m_buttonWidget->setFixedHeight(80);
    m_buttonLayout = new QHBoxLayout(m_buttonWidget);
    
    // 测试连接按钮
    m_testBtn = new QPushButton("🔍 测试连接");
    m_testBtn->setStyleSheet("QPushButton { background: #17a2b8; color: white; font-weight: bold; }");
    QObject::connect(m_testBtn, &QPushButton::clicked, this, &ConnectionDialog::onTestConnection);
    m_buttonLayout->addWidget(m_testBtn);
    
    m_buttonLayout->addStretch();
    
    // 对话框按钮
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(m_buttonBox, &QDialogButtonBox::accepted, this, &ConnectionDialog::onAccept);
    QObject::connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    m_buttonLayout->addWidget(m_buttonBox);
    
    m_rightLayout->addWidget(m_buttonWidget);
    
    // 状态标签
    m_statusLabel = new QLabel("");
    m_statusLabel->setStyleSheet("color: #666; font-style: italic; padding: 5px;");
    m_rightLayout->addWidget(m_statusLabel);
    
    // 初始化表单
    updateFormForType("Redis");
}

void ConnectionDialog::createFormControls() {
    // 基本连接信息
    m_nameEdit = new QLineEdit();
    m_nameEdit->setPlaceholderText("为此连接起一个名称");
    
    m_hostEdit = new QLineEdit();
    m_hostEdit->setText("localhost");
    m_hostEdit->setPlaceholderText("服务器地址");
    
    m_portSpin = new QSpinBox();
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(6379);
    
    m_usernameEdit = new QLineEdit();
    m_usernameEdit->setPlaceholderText("用户名 (可选)");
    
    m_passwordEdit = new QLineEdit();
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText("密码 (可选)");
    
    m_databaseEdit = new QLineEdit();
    m_databaseEdit->setText("0");
    
    m_timeoutSpin = new QSpinBox();
    m_timeoutSpin->setRange(5, 300);
    m_timeoutSpin->setValue(30);
    m_timeoutSpin->setSuffix(" 秒");
    
    m_sslCheck = new QCheckBox("使用SSL加密连接");
    
    // Redis特有控件
    m_dbIndexLabel = new QLabel("数据库索引:");
    m_dbIndexSpin = new QSpinBox();
    m_dbIndexSpin->setRange(0, 15);
    m_dbIndexSpin->setValue(0);
    
    // MySQL特有控件
    m_charsetLabel = new QLabel("字符集:");
    m_charsetCombo = new QComboBox();
    m_charsetCombo->addItems({"utf8mb4", "utf8", "latin1", "gbk"});
    m_charsetCombo->setCurrentText("utf8mb4");
}

void ConnectionDialog::setConnectionConfig(const ConnectionConfig& config) {
    m_nameEdit->setText(config.name);
    
    // 根据配置确定类型并选择
    QString typeName = "Redis"; // 默认Redis
    for (int i = 0; i < m_typeList->count(); ++i) {
        if (m_typeList->item(i)->text() == typeName) {
            m_typeList->setCurrentRow(i);
            break;
        }
    }
    
    m_hostEdit->setText(config.host);
    m_portSpin->setValue(config.port);
    m_usernameEdit->setText(config.username);
    m_passwordEdit->setText(config.password);
    m_databaseEdit->setText(config.database);
    m_timeoutSpin->setValue(config.timeout);
    m_sslCheck->setChecked(config.useSSL);
}

void ConnectionDialog::onConnectionTypeChanged() {
    QString selectedType = m_typeList->currentItem() ? m_typeList->currentItem()->text() : "Redis";
    m_currentType = selectedType;
    updateFormForType(selectedType);
}

void ConnectionDialog::updateFormForType(const QString& type) {
    // 清空现有表单
    QLayoutItem* item;
    while ((item = m_formLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->setParent(nullptr);
        }
        delete item;
    }
    
    // 获取默认配置
    ConnectionType connType = ConnectionFactory::getTypeFromString(type);
    ConnectionConfig defaultConfig = ConnectionFactory::getDefaultConfig(connType);
    
    // 更新默认值
    m_portSpin->setValue(defaultConfig.port);
    m_databaseEdit->setText(defaultConfig.database);
    
    // 基本信息（所有类型都有）
    m_formLayout->addRow("连接名称:", m_nameEdit);
    m_formLayout->addRow("主机地址:", m_hostEdit);
    m_formLayout->addRow("端口:", m_portSpin);
    
    if (type == "Redis") {
        // Redis特有表单
        m_usernameEdit->setVisible(false);
        m_passwordEdit->setPlaceholderText("Redis密码 (可选)");
        m_databaseEdit->setVisible(false);
        
        m_formLayout->addRow("密码:", m_passwordEdit);
        m_formLayout->addRow(m_dbIndexLabel, m_dbIndexSpin);
        m_formLayout->addRow("超时:", m_timeoutSpin);
        m_formLayout->addRow("", m_sslCheck);
        
        // 隐藏MySQL特有控件
        m_charsetLabel->setVisible(false);
        m_charsetCombo->setVisible(false);
        
    } else if (type == "MySQL" || type == "PostgreSQL") {
        // SQL数据库表单
        m_usernameEdit->setVisible(true);
        m_passwordEdit->setPlaceholderText("数据库密码");
        m_databaseEdit->setVisible(true);
        m_databaseEdit->setPlaceholderText("数据库名称");
        
        m_formLayout->addRow("用户名:", m_usernameEdit);
        m_formLayout->addRow("密码:", m_passwordEdit);
        m_formLayout->addRow("数据库:", m_databaseEdit);
        
        if (type == "MySQL") {
            m_charsetLabel->setVisible(true);
            m_charsetCombo->setVisible(true);
            m_formLayout->addRow(m_charsetLabel, m_charsetCombo);
        } else {
            m_charsetLabel->setVisible(false);
            m_charsetCombo->setVisible(false);
        }
        
        m_formLayout->addRow("超时:", m_timeoutSpin);
        m_formLayout->addRow("", m_sslCheck);
        
        // 隐藏Redis特有控件
        m_dbIndexLabel->setVisible(false);
        m_dbIndexSpin->setVisible(false);
        
    } else {
        // 其他数据库类型的通用表单
        m_usernameEdit->setVisible(true);
        m_passwordEdit->setVisible(true);
        m_databaseEdit->setVisible(true);
        
        m_formLayout->addRow("用户名:", m_usernameEdit);
        m_formLayout->addRow("密码:", m_passwordEdit);
        m_formLayout->addRow("数据库:", m_databaseEdit);
        m_formLayout->addRow("超时:", m_timeoutSpin);
        m_formLayout->addRow("", m_sslCheck);
        
        // 隐藏特有控件
        m_dbIndexLabel->setVisible(false);
        m_dbIndexSpin->setVisible(false);
        m_charsetLabel->setVisible(false);
        m_charsetCombo->setVisible(false);
    }
    
    // 更新状态说明
    QListWidgetItem* currentItem = m_typeList->currentItem();
    if (currentItem) {
        QString description = currentItem->data(Qt::UserRole).toString();
        m_statusLabel->setText(description);
    }
}

void ConnectionDialog::onTestConnection() {
    if (!validateInput()) {
        return;
    }
    
    m_testBtn->setEnabled(false);
    m_statusLabel->setText("正在测试连接...");
    
    ConnectionConfig config = getConnectionConfig();
    ConnectionType type = ConnectionFactory::getTypeFromString(m_currentType);
    
    Connection* testConn = ConnectionFactory::createConnection(type, config);
    if (!testConn) {
        m_statusLabel->setText("❌ 不支持的数据库类型");
        m_testBtn->setEnabled(true);
        return;
    }
    
    bool success = testConn->connectToServer();
    
    if (success) {
        m_statusLabel->setText("✅ 连接成功");
        testConn->disconnectFromServer();
    } else {
        m_statusLabel->setText("❌ 连接失败: " + testConn->getLastError());
    }
    
    testConn->deleteLater();
    m_testBtn->setEnabled(true);
}

void ConnectionDialog::onAccept() {
    if (validateInput()) {
        accept();
    }
}

bool ConnectionDialog::validateInput() {
    if (m_nameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "输入错误", "请输入连接名称");
        m_nameEdit->setFocus();
        return false;
    }
    
    if (m_hostEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "输入错误", "请输入主机地址");
        m_hostEdit->setFocus();
        return false;
    }
    
    return true;
}

// DatabaseTreeWidget 实现
DatabaseTreeWidget::DatabaseTreeWidget(QWidget* parent) : QTreeWidget(parent) {
    setupContextMenu();
    
    setHeaderLabels({"连接"});
    setContextMenuPolicy(Qt::CustomContextMenu);
    setRootIsDecorated(true);
    setAlternatingRowColors(true);
    
    QObject::connect(this, &QTreeWidget::itemDoubleClicked, this, &DatabaseTreeWidget::onItemDoubleClicked);
    QObject::connect(this, &QTreeWidget::itemExpanded, this, &DatabaseTreeWidget::onItemExpanded);
    QObject::connect(this, &QTreeWidget::customContextMenuRequested, this, &DatabaseTreeWidget::onCustomContextMenuRequested);
}

void DatabaseTreeWidget::setupContextMenu() {
    m_contextMenu = new QMenu(this);
    
    m_connectAction = new QAction("🔌 连接", this);
    m_disconnectAction = new QAction("🔌 断开连接", this);
    m_refreshAction = new QAction("🔄 刷新", this);
    m_deleteAction = new QAction("🗑️ 删除连接", this);
    m_newQueryAction = new QAction("📝 新建查询", this);
    
    m_contextMenu->addAction(m_connectAction);
    m_contextMenu->addAction(m_disconnectAction);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction(m_refreshAction);
    m_contextMenu->addAction(m_newQueryAction);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction(m_deleteAction);
}

void DatabaseTreeWidget::addConnection(const QString& name, Connection* connection) {
    m_connections[name] = connection;
    
    QTreeWidgetItem* connectionItem = new QTreeWidgetItem(this);
    connectionItem->setText(0, name);
    connectionItem->setIcon(0, style()->standardIcon(QStyle::SP_ComputerIcon));
    connectionItem->setData(0, Qt::UserRole, "connection");
    connectionItem->setData(0, Qt::UserRole + 1, name);
    
    m_itemConnectionMap[connectionItem] = name;
    
    // 添加占位符子项，使其可展开
    QTreeWidgetItem* placeholder = new QTreeWidgetItem(connectionItem);
    placeholder->setText(0, "加载中...");
    placeholder->setData(0, Qt::UserRole, "placeholder");
}

void DatabaseTreeWidget::onItemDoubleClicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column)
    
    QString itemType = item->data(0, Qt::UserRole).toString();
    QString connectionName = item->data(0, Qt::UserRole + 1).toString();
    
    if (itemType == "connection") {
        emit connectionRequested(connectionName);
    } else if (itemType == "table" || itemType == "key") {
        QString database = item->parent()->text(0);
        QString tableName = item->text(0);
        
        if (itemType == "table") {
            emit tableDoubleClicked(connectionName, database, tableName);
        } else {
            emit keyDoubleClicked(connectionName, database, tableName);
        }
    }
}

void DatabaseTreeWidget::onItemExpanded(QTreeWidgetItem* item) {
    QString itemType = item->data(0, Qt::UserRole).toString();
    QString connectionName = m_itemConnectionMap.value(item, "");
    
    if (itemType == "connection" && m_connections.contains(connectionName)) {
        // 清除占位符
        qDeleteAll(item->takeChildren());
        
        // 加载数据库列表
        Connection* conn = m_connections[connectionName];
        loadDatabases(item, conn);
    } else if (itemType == "database") {
        // 清除占位符
        qDeleteAll(item->takeChildren());
        
        // 加载表列表
        QTreeWidgetItem* connectionItem = item->parent();
        QString connName = m_itemConnectionMap.value(connectionItem, "");
        if (m_connections.contains(connName)) {
            Connection* conn = m_connections[connName];
            QString database = item->text(0);
            loadTables(item, conn, database);
        }
    }
}

void DatabaseTreeWidget::loadDatabases(QTreeWidgetItem* connectionItem, Connection* connection) {
    if (!connection->isConnected()) {
        QTreeWidgetItem* errorItem = new QTreeWidgetItem(connectionItem);
        errorItem->setText(0, "未连接");
        errorItem->setIcon(0, style()->standardIcon(QStyle::SP_MessageBoxWarning));
        return;
    }
    
    QStringList databases = connection->getDatabases();
    
    for (const QString& db : databases) {
        QTreeWidgetItem* dbItem = new QTreeWidgetItem(connectionItem);
        dbItem->setText(0, db);
        dbItem->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
        dbItem->setData(0, Qt::UserRole, "database");
        dbItem->setData(0, Qt::UserRole + 1, m_itemConnectionMap[connectionItem]);
        
        // 添加占位符
        QTreeWidgetItem* placeholder = new QTreeWidgetItem(dbItem);
        placeholder->setText(0, "加载中...");
        placeholder->setData(0, Qt::UserRole, "placeholder");
    }
}

void DatabaseTreeWidget::loadTables(QTreeWidgetItem* databaseItem, Connection* connection, const QString& database) {
    // 对于Redis，这里加载键列表
    if (connection->getType() == ConnectionType::Redis) {
        // 简化实现：直接显示占位符，实际键通过查询获取
        QTreeWidgetItem* keysItem = new QTreeWidgetItem(databaseItem);
        keysItem->setText(0, "Keys (请使用查询获取)");
        keysItem->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
        keysItem->setData(0, Qt::UserRole, "keys_placeholder");
        keysItem->setData(0, Qt::UserRole + 1, m_itemConnectionMap[databaseItem->parent()]);
    } else {
        // 其他数据库类型加载表列表
        QStringList tables = connection->getTables(database);
        
        for (const QString& table : tables) {
            QTreeWidgetItem* tableItem = new QTreeWidgetItem(databaseItem);
            tableItem->setText(0, table);
            tableItem->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
            tableItem->setData(0, Qt::UserRole, "table");
            tableItem->setData(0, Qt::UserRole + 1, m_itemConnectionMap[databaseItem->parent()]);
        }
    }
}

void DatabaseTreeWidget::onCustomContextMenuRequested(const QPoint& pos) {
    QTreeWidgetItem* item = itemAt(pos);
    if (!item) return;
    
    QString itemType = item->data(0, Qt::UserRole).toString();
    
    // 根据项目类型启用/禁用菜单项
    m_connectAction->setVisible(itemType == "connection");
    m_disconnectAction->setVisible(itemType == "connection");
    m_deleteAction->setVisible(itemType == "connection");
    m_newQueryAction->setVisible(itemType == "connection" || itemType == "database");
    
    m_contextMenu->exec(mapToGlobal(pos));
}

void DatabaseTreeWidget::setConnection(Connection* connection) {
    // 简化实现
    Q_UNUSED(connection)
}

void DatabaseTreeWidget::refreshConnection(const QString& connectionName) {
    // 简化实现 - 找到连接项并刷新
    for (int i = 0; i < topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = topLevelItem(i);
        if (item->text(0) == connectionName) {
            // 清除子项并重新加载
            qDeleteAll(item->takeChildren());
            
            if (m_connections.contains(connectionName)) {
                Connection* conn = m_connections[connectionName];
                if (conn->isConnected()) {
                    loadDatabases(item, conn);
                }
            }
            break;
        }
    }
}

void DatabaseTreeWidget::removeConnection(const QString& name) {
    // 简化实现
    m_connections.remove(name);
    
    for (int i = 0; i < topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = topLevelItem(i);
        if (item->text(0) == name) {
            delete takeTopLevelItem(i);
            break;
        }
    }
}

// QueryTab 实现
QueryTab::QueryTab(Connection* connection, QWidget* parent)
    : QWidget(parent), m_connection(connection) {
    setupUI();
}

void QueryTab::setupUI() {
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(5, 5, 5, 5);
    m_layout->setSpacing(5);
    
    // 工具栏
    m_toolbar = new QToolBar();
    m_toolbar->setIconSize(QSize(16, 16));
    
    m_executeAction = new QAction("▶️ 执行", this);
    m_executeAction->setShortcut(QKeySequence("F5"));
    m_executeAction->setToolTip("执行查询 (F5)");
    QObject::connect(m_executeAction, &QAction::triggered, this, &QueryTab::onExecuteClicked);
    m_toolbar->addAction(m_executeAction);
    
    m_clearAction = new QAction("🗑️ 清空", this);
    m_clearAction->setToolTip("清空查询");
    QObject::connect(m_clearAction, &QAction::triggered, this, &QueryTab::onClearClicked);
    m_toolbar->addAction(m_clearAction);
    
    m_formatAction = new QAction("🎨 格式化", this);
    m_formatAction->setToolTip("格式化查询");
    QObject::connect(m_formatAction, &QAction::triggered, this, &QueryTab::onFormatClicked);
    m_toolbar->addAction(m_formatAction);
    
    m_layout->addWidget(m_toolbar);
    
    // 查询编辑器
    m_queryEdit = new QTextEdit();
    m_queryEdit->setFont(QFont("Consolas", 11));
    m_queryEdit->setPlaceholderText("在此输入Redis命令，如：KEYS *");
    m_queryEdit->setMaximumHeight(150);
    m_layout->addWidget(m_queryEdit);
    
    // 结果区域
    m_resultTabs = new QTabWidget();
    
    // 结果表格
    m_resultTable = new QTableWidget();
    m_resultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultTable->setAlternatingRowColors(true);
    m_resultTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultTabs->addTab(m_resultTable, "📊 结果");
    
    // 消息文本
    m_messagesText = new QTextEdit();
    m_messagesText->setReadOnly(true);
    m_messagesText->setFont(QFont("Consolas", 10));
    m_resultTabs->addTab(m_messagesText, "📝 消息");
    
    m_layout->addWidget(m_resultTabs, 1); // 拉伸因子为1
    
    // 状态栏
    QHBoxLayout* statusLayout = new QHBoxLayout();
    m_statusLabel = new QLabel("就绪");
    m_timeLabel = new QLabel("执行时间: -");
    m_rowsLabel = new QLabel("行数: 0");
    
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(m_timeLabel);
    statusLayout->addWidget(m_rowsLabel);
    
    m_layout->addLayout(statusLayout);
}

void QueryTab::setQuery(const QString& query) {
    m_queryEdit->setPlainText(query);
}

QString QueryTab::getQuery() const {
    return m_queryEdit->toPlainText();
}

void QueryTab::executeQuery() {
    onExecuteClicked();
}

void QueryTab::onExecuteClicked() {
    if (!m_connection || !m_connection->isConnected()) {
        QMessageBox::warning(this, "执行错误", "数据库连接无效");
        return;
    }
    
    QString query = m_queryEdit->toPlainText().trimmed();
    if (query.isEmpty()) {
        QMessageBox::information(this, "提示", "请输入要执行的命令");
        return;
    }
    
    m_statusLabel->setText("正在执行...");
    
    // 解析命令和参数
    QStringList parts = query.split(' ', Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        return;
    }
    
    QString command = parts.takeFirst().toUpper();
    QVariantList params;
    for (const QString& part : parts) {
        params.append(part);
    }
    
    QueryResult result = m_connection->execute(command, params);
    updateResults(result);
    
    emit queryExecuted(result);
}

void QueryTab::onClearClicked() {
    m_queryEdit->clear();
    m_resultTable->setRowCount(0);
    m_resultTable->setColumnCount(0);
    m_messagesText->clear();
    m_statusLabel->setText("就绪");
    m_timeLabel->setText("执行时间: -");
    m_rowsLabel->setText("行数: 0");
}

void QueryTab::onFormatClicked() {
    // Redis命令格式化（简单实现）
    QString query = m_queryEdit->toPlainText().trimmed();
    if (query.isEmpty()) return;
    
    // 转换为大写并格式化
    QStringList parts = query.split(' ', Qt::SkipEmptyParts);
    if (!parts.isEmpty()) {
        parts[0] = parts[0].toUpper();
        m_queryEdit->setPlainText(parts.join(' '));
    }
}

void QueryTab::updateResults(const QueryResult& result) {
    m_timeLabel->setText(QString("执行时间: %1ms").arg(result.executionTime));
    
    if (result.success) {
        m_statusLabel->setText("✅ 执行成功");
        
        // 更新结果表格
        if (!result.data.isEmpty()) {
            QVariant data = result.data.first();
            
            if (data.typeId() == QMetaType::QVariantList) {
                // 数组结果
                QVariantList list = data.toList();
                m_resultTable->setRowCount(list.size());
                m_resultTable->setColumnCount(1);
                m_resultTable->setHorizontalHeaderLabels({"值"});
                
                for (int i = 0; i < list.size(); ++i) {
                    QTableWidgetItem* item = new QTableWidgetItem(list[i].toString());
                    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                    m_resultTable->setItem(i, 0, item);
                }
                
                m_rowsLabel->setText(QString("行数: %1").arg(list.size()));
            } else {
                // 单一结果
                m_resultTable->setRowCount(1);
                m_resultTable->setColumnCount(1);
                m_resultTable->setHorizontalHeaderLabels({"结果"});
                
                QTableWidgetItem* item = new QTableWidgetItem(data.toString());
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                m_resultTable->setItem(0, 0, item);
                
                m_rowsLabel->setText("行数: 1");
            }
        } else {
            m_resultTable->setRowCount(0);
            m_resultTable->setColumnCount(0);
            m_rowsLabel->setText("行数: 0");
        }
        
        m_messagesText->append(QString("[%1] 命令执行成功")
                              .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
    } else {
        m_statusLabel->setText("❌ 执行失败");
        m_messagesText->append(QString("[%1] 错误: %2")
                              .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                              .arg(result.errorMessage));
        
        m_resultTable->setRowCount(0);
        m_resultTable->setColumnCount(0);
        m_rowsLabel->setText("行数: 0");
    }
}

// DatabaseTool 主类实现
DatabaseTool::DatabaseTool(QWidget* parent)
    : QWidget(parent), DynamicObjectBase(), m_currentItem(nullptr) {
    
    m_settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, 
                              "LeleTools", "DatabaseTool", this);
    
    setupUI();
    loadConnections();
}

DatabaseTool::~DatabaseTool() {
    saveConnections();
    
    // 清理连接
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        it.value()->disconnectFromServer();
        it.value()->deleteLater();
    }
}

void DatabaseTool::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(5, 5, 5, 5);
    m_mainLayout->setSpacing(5);
    
    setupToolbar();
    
    // 主分割器 - 左右布局
    m_mainSplitter = new QSplitter(Qt::Horizontal);
    
    // 左侧连接树
    m_treeWidget = new DatabaseTreeWidget();
    m_treeWidget->setMaximumWidth(300);
    m_treeWidget->setMinimumWidth(200);
    
    QObject::connect(m_treeWidget, &DatabaseTreeWidget::connectionRequested,
            this, &DatabaseTool::onConnectToDatabase);
    QObject::connect(m_treeWidget, &DatabaseTreeWidget::tableDoubleClicked,
            this, &DatabaseTool::onTableDoubleClicked);
    QObject::connect(m_treeWidget, &DatabaseTreeWidget::keyDoubleClicked,
            this, &DatabaseTool::onKeyDoubleClicked);
    
    // 右侧查询标签页
    m_tabWidget = new QTabWidget();
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setMovable(true);
    
    QObject::connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &DatabaseTool::onCloseTab);
    QObject::connect(m_tabWidget, &QTabWidget::currentChanged, this, &DatabaseTool::onTabChanged);
    
    m_mainSplitter->addWidget(m_treeWidget);
    m_mainSplitter->addWidget(m_tabWidget);
    m_mainSplitter->setStretchFactor(0, 0); // 左侧固定
    m_mainSplitter->setStretchFactor(1, 1); // 右侧拉伸
    
    m_mainLayout->addWidget(m_mainSplitter, 1);
    
    setupStatusBar();
    
    // 设置样式
    setStyleSheet(R"(
        QToolBar {
            border: 1px solid #ddd;
            background: #f8f9fa;
            spacing: 3px;
        }
        QTreeWidget {
            border: 1px solid #ddd;
            background: white;
        }
        QTreeWidget::item {
            height: 24px;
            padding: 2px;
        }
        QTreeWidget::item:selected {
            background: #007acc;
            color: white;
        }
        QTabWidget::pane {
            border: 1px solid #ddd;
            background: white;
        }
        QTabBar::tab {
            background: #f1f1f1;
            padding: 8px 12px;
            margin-right: 2px;
        }
        QTabBar::tab:selected {
            background: white;
            border-bottom: 2px solid #007acc;
        }
    )");
}

void DatabaseTool::setupToolbar() {
    m_toolbar = new QToolBar();
    m_toolbar->setFixedHeight(40);
    
    m_newConnectionAction = new QAction("➕ 新建连接", this);
    QObject::connect(m_newConnectionAction, &QAction::triggered, this, &DatabaseTool::onNewConnection);
    m_toolbar->addAction(m_newConnectionAction);
    
    m_editConnectionAction = new QAction("✏️ 编辑连接", this);
    QObject::connect(m_editConnectionAction, &QAction::triggered, this, &DatabaseTool::onEditConnection);
    m_toolbar->addAction(m_editConnectionAction);
    
    m_deleteConnectionAction = new QAction("🗑️ 删除连接", this);
    QObject::connect(m_deleteConnectionAction, &QAction::triggered, this, &DatabaseTool::onDeleteConnection);
    m_toolbar->addAction(m_deleteConnectionAction);
    
    m_toolbar->addSeparator();
    
    m_connectAction = new QAction("🔌 连接", this);
    QObject::connect(m_connectAction, &QAction::triggered, this, &DatabaseTool::onConnectToDatabase);
    m_toolbar->addAction(m_connectAction);
    
    m_disconnectAction = new QAction("🔌 断开", this);
    QObject::connect(m_disconnectAction, &QAction::triggered, this, &DatabaseTool::onDisconnectFromDatabase);
    m_toolbar->addAction(m_disconnectAction);
    
    m_refreshAction = new QAction("🔄 刷新", this);
    QObject::connect(m_refreshAction, &QAction::triggered, this, &DatabaseTool::onRefreshConnections);
    m_toolbar->addAction(m_refreshAction);
    
    m_toolbar->addSeparator();
    
    m_newQueryAction = new QAction("📝 新建查询", this);
    QObject::connect(m_newQueryAction, &QAction::triggered, this, &DatabaseTool::onNewQuery);
    m_toolbar->addAction(m_newQueryAction);
    
    m_mainLayout->addWidget(m_toolbar);
}

void DatabaseTool::setupStatusBar() {
    m_statusBar = new QWidget();
    m_statusBar->setFixedHeight(25);
    
    QHBoxLayout* statusLayout = new QHBoxLayout(m_statusBar);
    statusLayout->setContentsMargins(5, 2, 5, 2);
    
    m_statusLabel = new QLabel("就绪");
    m_connectionCountLabel = new QLabel("连接数: 0");
    
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(m_connectionCountLabel);
    
    m_mainLayout->addWidget(m_statusBar);
}

void DatabaseTool::onNewConnection() {
    ConnectionDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        ConnectionConfig config = dialog.getConnectionConfig();
        
        if (m_connectionConfigs.contains(config.name)) {
            QMessageBox::warning(this, "连接已存在", "已存在同名连接，请使用不同的名称");
            return;
        }
        
        m_connectionConfigs[config.name] = config;
        saveConnections();
        
        // 根据配置确定连接类型
        ConnectionType type = ConnectionType::Redis; // 目前只支持Redis
        Connection* connection = ConnectionFactory::createConnection(type, config);
        if (connection) {
            m_connections[config.name] = connection;
            m_treeWidget->addConnection(config.name, connection);
            
            QObject::connect(connection, &Connection::stateChanged, [this, config](ConnectionState state) {
                onConnectionStateChanged(config.name, state);
            });
        }
        
        updateConnectionStatus();
        m_statusLabel->setText("连接已添加: " + config.name);
    }
}

void DatabaseTool::onNewQuery() {
    // 创建新查询标签页
    QString connectionName = "默认";
    
    // 如果有选中的连接，使用该连接
    QTreeWidgetItem* current = m_treeWidget->currentItem();
    if (current) {
        QString itemType = current->data(0, Qt::UserRole).toString();
        if (itemType == "connection") {
            connectionName = current->text(0);
        } else {
            // 找到父连接
            while (current && current->data(0, Qt::UserRole).toString() != "connection") {
                current = current->parent();
            }
            if (current) {
                connectionName = current->text(0);
            }
        }
    }
    
    QueryTab* tab = createQueryTab(connectionName);
    if (tab) {
        int index = m_tabWidget->addTab(tab, QString("查询 %1").arg(m_tabWidget->count() + 1));
        m_tabWidget->setCurrentIndex(index);
    }
}

QueryTab* DatabaseTool::createQueryTab(const QString& connectionName) {
    Connection* connection = m_connections.value(connectionName, nullptr);
    
    QueryTab* tab = new QueryTab(connection);
    
    QObject::connect(tab, &QueryTab::titleChanged, [this, tab](const QString& title) {
        int index = m_tabWidget->indexOf(tab);
        if (index >= 0) {
            m_tabWidget->setTabText(index, title);
        }
    });
    
    return tab;
}

void DatabaseTool::onCloseTab(int index) {
    QWidget* tab = m_tabWidget->widget(index);
    m_tabWidget->removeTab(index);
    tab->deleteLater();
}

void DatabaseTool::loadConnections() {
    // 简化实现，后续可以从配置文件加载
}

void DatabaseTool::saveConnections() {
    // 简化实现，后续可以保存到配置文件
}

void DatabaseTool::updateConnectionStatus() {
    m_connectionCountLabel->setText(QString("连接数: %1").arg(m_connections.size()));
}

void DatabaseTool::onConnectToDatabase() {
    QTreeWidgetItem* current = m_treeWidget->currentItem();
    if (!current) return;
    
    QString itemType = current->data(0, Qt::UserRole).toString();
    if (itemType != "connection") return;
    
    QString connectionName = current->text(0);
    if (!m_connections.contains(connectionName)) return;
    
    Connection* connection = m_connections[connectionName];
    if (connection->isConnected()) {
        m_statusLabel->setText("已连接到: " + connectionName);
        return;
    }
    
    m_statusLabel->setText("正在连接...");
    
    bool success = connection->connectToServer();
    if (success) {
        m_statusLabel->setText("✅ 连接成功: " + connectionName);
        m_treeWidget->refreshConnection(connectionName);
    } else {
        m_statusLabel->setText("❌ 连接失败: " + connection->getLastError());
    }
}

void DatabaseTool::onTableDoubleClicked(const QString& connectionName, const QString& database, const QString& table) {
    // 创建查询标签页并设置默认查询
    QueryTab* tab = createQueryTab(connectionName);
    if (tab) {
        tab->setQuery(QString("SELECT * FROM %1 LIMIT 100").arg(table));
        int index = m_tabWidget->addTab(tab, QString("%1.%2").arg(database, table));
        m_tabWidget->setCurrentIndex(index);
    }
}

void DatabaseTool::onKeyDoubleClicked(const QString& connectionName, const QString& database, const QString& key) {
    // 为Redis键创建查询
    QueryTab* tab = createQueryTab(connectionName);
    if (tab) {
        tab->setQuery(QString("GET %1").arg(key));
        int index = m_tabWidget->addTab(tab, QString("Key: %1").arg(key));
        m_tabWidget->setCurrentIndex(index);
    }
}

void DatabaseTool::onConnectionStateChanged(const QString& name, ConnectionState state) {
    Q_UNUSED(name)
    Q_UNUSED(state)
    updateConnectionStatus();
}

// 其他槽函数的简化实现
void DatabaseTool::onEditConnection() {}
void DatabaseTool::onDeleteConnection() {}
void DatabaseTool::onDisconnectFromDatabase() {}
void DatabaseTool::onRefreshConnections() {}
void DatabaseTool::onTabChanged(int index) { Q_UNUSED(index) }

QueryTab* DatabaseTool::getCurrentQueryTab() {
    return qobject_cast<QueryTab*>(m_tabWidget->currentWidget());
}

#include "databasetool.moc"
