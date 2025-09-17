#include "databasetool.h"
#include "../../common/connx/RedisConnection.h"
#ifdef WITH_MYSQL
#include "../../common/connx/MySQLConnection.h"
#endif
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
#include <QTimer>

// 移除全局using namespace以避免命名冲突
// using namespace SqliteWrapper;

REGISTER_DYNAMICOBJECT(DatabaseTool);

// DatabaseHierarchyManager 实现
DatabaseHierarchyManager& DatabaseHierarchyManager::instance() {
    static DatabaseHierarchyManager instance;
    return instance;
}

DatabaseHierarchyManager::DatabaseHierarchyManager() {
    initializeHierarchies();
}

void DatabaseHierarchyManager::initializeHierarchies() {
    // 初始化节点图标映射
    m_nodeIcons[DbNodeType::Connection] = "database-connection";
    m_nodeIcons[DbNodeType::Database] = "database";
    m_nodeIcons[DbNodeType::Schema] = "schema";
    m_nodeIcons[DbNodeType::TableFolder] = "folder-table";
    m_nodeIcons[DbNodeType::Table] = "table";
    m_nodeIcons[DbNodeType::ViewFolder] = "folder-view";
    m_nodeIcons[DbNodeType::View] = "view";
    m_nodeIcons[DbNodeType::ProcedureFolder] = "folder-procedure";
    m_nodeIcons[DbNodeType::Procedure] = "procedure";
    m_nodeIcons[DbNodeType::FunctionFolder] = "folder-function";
    m_nodeIcons[DbNodeType::Function] = "function";
    m_nodeIcons[DbNodeType::TriggerFolder] = "folder-trigger";
    m_nodeIcons[DbNodeType::Trigger] = "trigger";
    m_nodeIcons[DbNodeType::IndexFolder] = "folder-index";
    m_nodeIcons[DbNodeType::Index] = "index";
    m_nodeIcons[DbNodeType::SequenceFolder] = "folder-sequence";
    m_nodeIcons[DbNodeType::Sequence] = "sequence";
    m_nodeIcons[DbNodeType::UserFolder] = "folder-user";
    m_nodeIcons[DbNodeType::User] = "user";
    m_nodeIcons[DbNodeType::Column] = "column";
    m_nodeIcons[DbNodeType::Key] = "key";
    m_nodeIcons[DbNodeType::RedisKey] = "redis-key";

    // 初始化节点显示名称
    m_nodeNames[DbNodeType::TableFolder] = "表";
    m_nodeNames[DbNodeType::ViewFolder] = "视图";
    m_nodeNames[DbNodeType::ProcedureFolder] = "存储过程";
    m_nodeNames[DbNodeType::FunctionFolder] = "函数";
    m_nodeNames[DbNodeType::TriggerFolder] = "触发器";
    m_nodeNames[DbNodeType::IndexFolder] = "索引";
    m_nodeNames[DbNodeType::SequenceFolder] = "序列";
    m_nodeNames[DbNodeType::UserFolder] = "用户";

    // MySQL层级结构
    QList<DbHierarchyNode> mysqlHierarchy;
    mysqlHierarchy.append(DbHierarchyNode(DbNodeType::TableFolder, "表"));
    mysqlHierarchy.append(DbHierarchyNode(DbNodeType::ViewFolder, "视图"));
    mysqlHierarchy.append(DbHierarchyNode(DbNodeType::ProcedureFolder, "存储过程"));
    mysqlHierarchy.append(DbHierarchyNode(DbNodeType::FunctionFolder, "函数"));
    mysqlHierarchy.append(DbHierarchyNode(DbNodeType::TriggerFolder, "触发器"));
    m_hierarchies[Connx::ConnectionType::MySQL] = mysqlHierarchy;

    // PostgreSQL层级结构
    QList<DbHierarchyNode> postgresHierarchy;
    postgresHierarchy.append(DbHierarchyNode(DbNodeType::TableFolder, "表"));
    postgresHierarchy.append(DbHierarchyNode(DbNodeType::ViewFolder, "视图"));
    postgresHierarchy.append(DbHierarchyNode(DbNodeType::ProcedureFolder, "存储过程"));
    postgresHierarchy.append(DbHierarchyNode(DbNodeType::FunctionFolder, "函数"));
    postgresHierarchy.append(DbHierarchyNode(DbNodeType::TriggerFolder, "触发器"));
    postgresHierarchy.append(DbHierarchyNode(DbNodeType::SequenceFolder, "序列"));
    postgresHierarchy.append(DbHierarchyNode(DbNodeType::IndexFolder, "索引"));
    m_hierarchies[Connx::ConnectionType::PostgreSQL] = postgresHierarchy;

    // SQLite层级结构（简化）
    QList<DbHierarchyNode> sqliteHierarchy;
    sqliteHierarchy.append(DbHierarchyNode(DbNodeType::TableFolder, "表"));
    sqliteHierarchy.append(DbHierarchyNode(DbNodeType::ViewFolder, "视图"));
    sqliteHierarchy.append(DbHierarchyNode(DbNodeType::IndexFolder, "索引"));
    sqliteHierarchy.append(DbHierarchyNode(DbNodeType::TriggerFolder, "触发器"));
    m_hierarchies[Connx::ConnectionType::SQLite] = sqliteHierarchy;

    // Redis层级结构（无层级，直接显示键）
    QList<DbHierarchyNode> redisHierarchy;
    // Redis没有文件夹结构，直接显示键
    m_hierarchies[Connx::ConnectionType::Redis] = redisHierarchy;
}

QList<DbHierarchyNode> DatabaseHierarchyManager::getHierarchy(Connx::ConnectionType dbType) const {
    return m_hierarchies.value(dbType, QList<DbHierarchyNode>());
}

bool DatabaseHierarchyManager::needsLazyLoading(DbNodeType nodeType) const {
    // 文件夹类型和数据库节点需要延迟加载
    return nodeType == DbNodeType::Database ||
           nodeType == DbNodeType::Schema ||
           nodeType == DbNodeType::TableFolder ||
           nodeType == DbNodeType::ViewFolder ||
           nodeType == DbNodeType::ProcedureFolder ||
           nodeType == DbNodeType::FunctionFolder ||
           nodeType == DbNodeType::TriggerFolder ||
           nodeType == DbNodeType::IndexFolder ||
           nodeType == DbNodeType::SequenceFolder ||
           nodeType == DbNodeType::UserFolder;
}

QString DatabaseHierarchyManager::getNodeIcon(DbNodeType nodeType) const {
    return m_nodeIcons.value(nodeType, "default");
}

QString DatabaseHierarchyManager::getNodeDisplayName(DbNodeType nodeType) const {
    return m_nodeNames.value(nodeType, "未知节点");
}

// DbTreeWidgetItem 实现
DbTreeWidgetItem::DbTreeWidgetItem(QTreeWidget* parent, const DbNodeData& data)
    : QTreeWidgetItem(parent), m_nodeData(data) {
    updateDisplay();
}

DbTreeWidgetItem::DbTreeWidgetItem(QTreeWidgetItem* parent, const DbNodeData& data)
    : QTreeWidgetItem(parent), m_nodeData(data) {
    updateDisplay();
}

void DbTreeWidgetItem::updateDisplay() {
    setText(0, m_nodeData.name);
    setIcon(0, QIcon(getNodeIcon()));

    // 设置工具提示
    QString tooltip = m_nodeData.name;
    if (!m_nodeData.fullName.isEmpty() && m_nodeData.fullName != m_nodeData.name) {
        tooltip += QString("\n完整名称: %1").arg(m_nodeData.fullName);
    }
    if (!m_nodeData.database.isEmpty()) {
        tooltip += QString("\n数据库: %1").arg(m_nodeData.database);
    }
    setToolTip(0, tooltip);

    // 设置节点样式
    if (m_nodeData.type == DbNodeType::Loading) {
        setForeground(0, QBrush(QColor(128, 128, 128)));
        setFont(0, QFont("Arial", 9, QFont::Normal, true));
    }
}

QString DbTreeWidgetItem::getNodeIcon() const {
    QString iconName = DatabaseHierarchyManager::instance().getNodeIcon(m_nodeData.type);
    return QString(":/icons/%1.png").arg(iconName);
}

void DbTreeWidgetItem::setLoading(bool loading) {
    if (loading) {
        m_nodeData.type = DbNodeType::Loading;
        m_nodeData.name = "加载中...";
    }
    updateDisplay();
}

// ConnectionDialog 实现
ConnectionDialog::ConnectionDialog(QWidget* parent) : QDialog(parent) {
    setupUI();
    setWindowTitle("新建连接");
}

ConnectionDialog::ConnectionDialog(const Connx::ConnectionConfig& config, QWidget* parent) : QDialog(parent) {
    setupUI();
    setConnectionConfig(config);
    setWindowTitle("编辑连接");
}

void ConnectionDialog::setupUI() {
    setModal(true);
    setMinimumSize(750, 520); // 设置最小尺寸而不是固定尺寸
    resize(750, 520); // 设置默认尺寸
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);

    // 主分割器布局
    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    m_splitter = new QSplitter(Qt::Horizontal);
    m_splitter->setHandleWidth(1);

    setupLeftPanel();
    setupRightPanel();

    m_splitter->addWidget(m_leftPanel);
    m_splitter->addWidget(m_rightPanel);
    m_splitter->setStretchFactor(0, 0); // 左侧固定
    m_splitter->setStretchFactor(1, 1); // 右侧拉伸
    m_splitter->setSizes({220, 530}); // 调整合理的初始大小

    m_mainLayout->addWidget(m_splitter);

    // 设置现代化样式
    setStyleSheet(R"(
        QDialog {
            background: #f8f9fa;
        }
        QWidget#leftPanel {
            background: #2c3e50;
            border-right: 1px solid #34495e;
        }
        QLabel#typeLabel {
            color: #ecf0f1;
            font-weight: bold;
            font-size: 13pt;
            padding: 10px;
            background: transparent;
        }
        QListWidget {
            background: transparent;
            border: none;
            outline: none;
            padding: 8px;
            show-decoration-selected: 0;
        }
        QWidget#rightPanel {
            background: white;
            border: none;
        }
        QLineEdit, QSpinBox, QComboBox {
            padding: 10px 12px;
            border: 2px solid #e9ecef;
            border-radius: 6px;
            font-size: 11pt;
            height: 40px;
            min-width: 200px;
            background: white;
        }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus {
            border-color: #3498db;
            outline: none;
        }
        QSpinBox::up-button, QSpinBox::down-button {
            width: 20px;
            height: 18px;
            border: none;
            background: #f8f9fa;
        }
        QSpinBox::up-button:hover, QSpinBox::down-button:hover {
            background: #e9ecef;
        }
        QSpinBox::up-arrow {
            image: none;
            border-style: solid;
            border-width: 4px;
            border-color: transparent transparent #6c757d transparent;
            width: 0px;
            height: 0px;
        }
        QSpinBox::down-arrow {
            image: none;
            border-style: solid;
            border-width: 4px;
            border-color: #6c757d transparent transparent transparent;
            width: 0px;
            height: 0px;
        }
        QComboBox::drop-down {
            border: none;
            width: 20px;
            background: #f8f9fa;
        }
        QComboBox::drop-down:hover {
            background: #e9ecef;
        }
        QComboBox::down-arrow {
            image: none;
            border-style: solid;
            border-width: 4px;
            border-color: #6c757d transparent transparent transparent;
            width: 0px;
            height: 0px;
        }
        QCheckBox {
            font-size: 11pt;
            color: #2c3e50;
        }
        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border: 2px solid #bdc3c7;
            border-radius: 3px;
        }
        QCheckBox::indicator:checked {
            background: #3498db;
            border-color: #3498db;
        }
        QPushButton {
            padding: 10px 20px;
            font-size: 11pt;
            border: none;
            border-radius: 6px;
            font-weight: 500;
        }
        QPushButton#testBtn {
            background: #27ae60;
            color: white;
        }
        QPushButton#testBtn:hover {
            background: #229954;
        }
        QPushButton#testBtn:pressed {
            background: #1e8449;
        }
        QDialogButtonBox QPushButton {
            background: #3498db;
            color: white;
            min-width: 80px;
        }
        QDialogButtonBox QPushButton:hover {
            background: #2980b9;
        }
        QDialogButtonBox QPushButton:pressed {
            background: #21618c;
        }
        QDialogButtonBox QPushButton[text="Cancel"] {
            background: #95a5a6;
        }
        QDialogButtonBox QPushButton[text="Cancel"]:hover {
            background: #7f8c8d;
        }
        QLabel {
            font-size: 11pt;
            color: #2c3e50;
            font-weight: 500;
        }
        QLabel#statusLabel {
            font-style: italic;
            color: #7f8c8d;
            padding: 8px;
        }
        /* 现代化滚动条样式 */
        QScrollBar:vertical {
            background: rgba(0, 0, 0, 0.05);
            width: 12px;
            border-radius: 6px;
            border: none;
        }
        QScrollBar::handle:vertical {
            background: rgba(52, 152, 219, 0.6);
            border-radius: 6px;
            min-height: 20px;
            margin: 0px;
        }
        QScrollBar::handle:vertical:hover {
            background: rgba(52, 152, 219, 0.8);
        }
        QScrollBar::handle:vertical:pressed {
            background: rgba(41, 128, 185, 1.0);
        }
        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical {
            height: 0px;
            width: 0px;
        }
        QScrollBar::add-page:vertical,
        QScrollBar::sub-page:vertical {
            background: transparent;
        }
        QScrollBar:horizontal {
            background: rgba(0, 0, 0, 0.05);
            height: 12px;
            border-radius: 6px;
            border: none;
        }
        QScrollBar::handle:horizontal {
            background: rgba(52, 152, 219, 0.6);
            border-radius: 6px;
            min-width: 20px;
            margin: 0px;
        }
        QScrollBar::handle:horizontal:hover {
            background: rgba(52, 152, 219, 0.8);
        }
        QScrollBar::handle:horizontal:pressed {
            background: rgba(41, 128, 185, 1.0);
        }
        QScrollBar::add-line:horizontal,
        QScrollBar::sub-line:horizontal {
            height: 0px;
            width: 0px;
        }
        QScrollBar::add-page:horizontal,
        QScrollBar::sub-page:horizontal {
            background: transparent;
        }
    )");
}

Connx::ConnectionConfig ConnectionDialog::getConnectionConfig() const {
    Connx::ConnectionConfig config;
    config.name = m_nameEdit->text().trimmed();
    config.host = m_hostEdit->text().trimmed();
    config.port = m_portSpin->value();
    config.timeout = m_timeoutSpin->value();
    config.useSSL = m_sslCheck->isChecked();

    // 设置连接类型
    config.extraParams["connectionType"] = m_currentType;

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
    m_leftPanel->setObjectName("leftPanel");
    m_leftPanel->setMinimumWidth(200);
    m_leftPanel->setMaximumWidth(220);
    m_leftLayout = new QVBoxLayout(m_leftPanel);
    m_leftLayout->setContentsMargins(0, 20, 0, 20);
    m_leftLayout->setSpacing(8);

    // 数据库类型标题
    m_typeLabel = new QLabel("数据库类型");
    m_typeLabel->setObjectName("typeLabel");
    m_leftLayout->addWidget(m_typeLabel);
    
    // 数据库类型列表
    m_typeList = new QListWidget();
    m_typeList->setMinimumWidth(180);
    m_typeList->setMaximumWidth(200);

    // 使用自定义委托代替纯QSS样式
    m_typeList->setItemDelegate(new ConnectionTypeDelegate(this));
    
    // 添加数据库类型项
    QStringList types = Connx::ConnectionFactory::getSupportedTypes();
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
    
    // 默认选择MySQL（如果可用），否则选择第一个
    m_currentType = "Redis"; // 默认fallback
    for (int i = 0; i < m_typeList->count(); ++i) {
        if (m_typeList->item(i)->text() == "MySQL") {
            m_typeList->setCurrentRow(i);
            m_currentType = "MySQL";
            break;
        }
    }
    if (m_currentType == "Redis" && m_typeList->count() > 0) {
        m_typeList->setCurrentRow(0);
        m_currentType = m_typeList->item(0)->text();
    }
    
    QObject::connect(m_typeList, &QListWidget::currentTextChanged, 
                    this, &ConnectionDialog::onConnectionTypeChanged);
    
    m_leftLayout->addWidget(m_typeList);
    m_leftLayout->addStretch();
}

void ConnectionDialog::setupRightPanel() {
    m_rightPanel = new QWidget();
    m_rightPanel->setObjectName("rightPanel");
    m_rightLayout = new QVBoxLayout(m_rightPanel);
    m_rightLayout->setContentsMargins(30, 30, 30, 20);
    m_rightLayout->setSpacing(20);

    // 表单区域 - 使用网格布局代替FormLayout
    m_formWidget = new QWidget();
    m_gridLayout = new QGridLayout(m_formWidget);
    m_gridLayout->setVerticalSpacing(18);
    m_gridLayout->setHorizontalSpacing(15);
    m_gridLayout->setColumnStretch(1, 1); // 第二列（输入控件）可拉伸
    
    // 创建所有表单控件
    createFormControls();
    
    m_rightLayout->addWidget(m_formWidget, 1);
    
    // 底部按钮区域
    m_buttonWidget = new QWidget();
    m_buttonWidget->setFixedHeight(80);
    m_buttonLayout = new QHBoxLayout(m_buttonWidget);
    
    // 测试连接按钮
    m_testBtn = new QPushButton("🔍 测试连接");
    m_testBtn->setObjectName("testBtn");
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
    m_statusLabel->setObjectName("statusLabel");
    m_rightLayout->addWidget(m_statusLabel);
    
    // 初始化表单
    updateFormForType(m_currentType);
}

void ConnectionDialog::createFormControls() {
    const int CONTROL_HEIGHT = 40; // 统一控件高度

    // 基本连接信息
    m_nameEdit = new QLineEdit();
    m_nameEdit->setPlaceholderText("为此连接起一个名称");
    m_nameEdit->setFixedHeight(CONTROL_HEIGHT);

    m_hostEdit = new QLineEdit();
    m_hostEdit->setText("localhost");
    m_hostEdit->setPlaceholderText("服务器地址");
    m_hostEdit->setFixedHeight(CONTROL_HEIGHT);

    m_portSpin = new QSpinBox();
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(6379);
    m_portSpin->setFixedHeight(CONTROL_HEIGHT);

    m_usernameEdit = new QLineEdit();
    m_usernameEdit->setPlaceholderText("用户名 (可选)");
    m_usernameEdit->setFixedHeight(CONTROL_HEIGHT);

    m_passwordEdit = new QLineEdit();
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText("密码 (可选)");
    m_passwordEdit->setFixedHeight(CONTROL_HEIGHT);

    m_databaseEdit = new QLineEdit();
    m_databaseEdit->setText("0");
    m_databaseEdit->setFixedHeight(CONTROL_HEIGHT);

    m_timeoutSpin = new QSpinBox();
    m_timeoutSpin->setRange(5, 300);
    m_timeoutSpin->setValue(30);
    m_timeoutSpin->setSuffix(" 秒");
    m_timeoutSpin->setFixedHeight(CONTROL_HEIGHT);

    m_sslCheck = new QCheckBox("使用SSL加密连接");
    m_sslCheck->setFixedHeight(CONTROL_HEIGHT);

    // Redis特有控件
    m_dbIndexLabel = new QLabel("数据库索引:");
    m_dbIndexSpin = new QSpinBox();
    m_dbIndexSpin->setRange(0, 15);
    m_dbIndexSpin->setValue(0);
    m_dbIndexSpin->setFixedHeight(CONTROL_HEIGHT);

    // MySQL特有控件
    m_charsetLabel = new QLabel("字符集:");
    m_charsetCombo = new QComboBox();
    m_charsetCombo->addItems({"utf8mb4", "utf8", "latin1", "gbk"});
    m_charsetCombo->setCurrentText("utf8mb4");
    m_charsetCombo->setFixedHeight(CONTROL_HEIGHT);
}

void ConnectionDialog::setConnectionConfig(const Connx::ConnectionConfig& config) {
    m_nameEdit->setText(config.name);

    // 根据配置确定类型并选择
    QString typeName = config.extraParams.value("connectionType", "Redis").toString();
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
    // 清空现有网格布局
    QLayoutItem* item;
    while ((item = m_gridLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->setParent(nullptr);
        }
        delete item;
    }

    // 获取默认配置
    Connx::ConnectionType connType = Connx::ConnectionFactory::getTypeFromString(type);
    Connx::ConnectionConfig defaultConfig = Connx::ConnectionFactory::getDefaultConfig(connType);

    // 更新默认值
    m_portSpin->setValue(defaultConfig.port);
    m_databaseEdit->setText(defaultConfig.database);

    int row = 0;

    // 创建标签的辅助函数
    auto createLabel = [this](const QString& text) -> QLabel* {
        QLabel* label = new QLabel(text);
        label->setStyleSheet("QLabel { font-weight: 500; color: #2c3e50; margin-bottom: 5px; }");
        return label;
    };

    // 基本信息（所有类型都有）
    m_gridLayout->addWidget(createLabel("连接名称:"), row, 0, Qt::AlignTop);
    m_gridLayout->addWidget(m_nameEdit, row++, 1);

    m_gridLayout->addWidget(createLabel("主机地址:"), row, 0, Qt::AlignTop);
    m_gridLayout->addWidget(m_hostEdit, row++, 1);

    m_gridLayout->addWidget(createLabel("端口:"), row, 0, Qt::AlignTop);
    m_gridLayout->addWidget(m_portSpin, row++, 1);

    if (type == "Redis") {
        // Redis特有表单
        m_passwordEdit->setPlaceholderText("Redis密码 (可选)");

        m_gridLayout->addWidget(createLabel("密码:"), row, 0, Qt::AlignTop);
        m_gridLayout->addWidget(m_passwordEdit, row++, 1);

        m_gridLayout->addWidget(createLabel("数据库索引:"), row, 0, Qt::AlignTop);
        m_gridLayout->addWidget(m_dbIndexSpin, row++, 1);

        m_gridLayout->addWidget(createLabel("超时:"), row, 0, Qt::AlignTop);
        m_gridLayout->addWidget(m_timeoutSpin, row++, 1);

        m_gridLayout->addWidget(new QLabel(""), row, 0); // 空标签占位
        m_gridLayout->addWidget(m_sslCheck, row++, 1);

    } else if (type == "MySQL" || type == "PostgreSQL") {
        // SQL数据库表单
        m_passwordEdit->setPlaceholderText("数据库密码");
        m_databaseEdit->setPlaceholderText("数据库名称");

        m_gridLayout->addWidget(createLabel("用户名:"), row, 0, Qt::AlignTop);
        m_gridLayout->addWidget(m_usernameEdit, row++, 1);

        m_gridLayout->addWidget(createLabel("密码:"), row, 0, Qt::AlignTop);
        m_gridLayout->addWidget(m_passwordEdit, row++, 1);

        m_gridLayout->addWidget(createLabel("数据库:"), row, 0, Qt::AlignTop);
        m_gridLayout->addWidget(m_databaseEdit, row++, 1);

        if (type == "MySQL") {
            m_gridLayout->addWidget(createLabel("字符集:"), row, 0, Qt::AlignTop);
            m_gridLayout->addWidget(m_charsetCombo, row++, 1);
        }

        m_gridLayout->addWidget(createLabel("超时:"), row, 0, Qt::AlignTop);
        m_gridLayout->addWidget(m_timeoutSpin, row++, 1);

        m_gridLayout->addWidget(new QLabel(""), row, 0); // 空标签占位
        m_gridLayout->addWidget(m_sslCheck, row++, 1);

    } else {
        // 其他数据库类型的通用表单
        m_gridLayout->addWidget(createLabel("用户名:"), row, 0, Qt::AlignTop);
        m_gridLayout->addWidget(m_usernameEdit, row++, 1);

        m_gridLayout->addWidget(createLabel("密码:"), row, 0, Qt::AlignTop);
        m_gridLayout->addWidget(m_passwordEdit, row++, 1);

        m_gridLayout->addWidget(createLabel("数据库:"), row, 0, Qt::AlignTop);
        m_gridLayout->addWidget(m_databaseEdit, row++, 1);

        m_gridLayout->addWidget(createLabel("超时:"), row, 0, Qt::AlignTop);
        m_gridLayout->addWidget(m_timeoutSpin, row++, 1);

        m_gridLayout->addWidget(new QLabel(""), row, 0); // 空标签占位
        m_gridLayout->addWidget(m_sslCheck, row++, 1);
    }

    // 添加弹性空间
    m_gridLayout->setRowStretch(row, 1);

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
    
    Connx::ConnectionConfig config = getConnectionConfig();
    Connx::ConnectionType type = Connx::ConnectionFactory::getTypeFromString(m_currentType);
    
    Connx::Connection* testConn = Connx::ConnectionFactory::createConnection(type, config);
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
        m_statusLabel->setText("❌ 请输入连接名称");
        m_nameEdit->setFocus();
        return false;
    }

    if (m_hostEdit->text().trimmed().isEmpty()) {
        m_statusLabel->setText("❌ 请输入主机地址");
        m_hostEdit->setFocus();
        return false;
    }

    // 清除错误信息
    m_statusLabel->setText("");
    return true;
}

// DatabaseTreeWidget 实现
DatabaseTreeWidget::DatabaseTreeWidget(QWidget* parent) : QTreeWidget(parent) {
    setupContextMenu();

    setHeaderLabels({"连接"});
    setContextMenuPolicy(Qt::CustomContextMenu);
    setRootIsDecorated(true);
    setAlternatingRowColors(true);
    setExpandsOnDoubleClick(false); // 禁用默认双击展开，我们自己处理

    // 性能优化设置
    setUniformRowHeights(true);
    setAnimated(true);
    setIndentation(20);

    // 启用虚拟滚动以提高大数据集性能
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

    // 减少重绘次数
    setUpdatesEnabled(true);

    QObject::connect(this, &QTreeWidget::itemDoubleClicked, this, &DatabaseTreeWidget::onItemDoubleClicked);
    QObject::connect(this, &QTreeWidget::itemExpanded, this, &DatabaseTreeWidget::onItemExpanded);
    QObject::connect(this, &QTreeWidget::customContextMenuRequested, this, &DatabaseTreeWidget::onCustomContextMenuRequested);
}

void DatabaseTreeWidget::setupContextMenu() {
    m_contextMenu = new QMenu(this);
    
    m_connectAction = new QAction("🔌 连接", this);
    m_disconnectAction = new QAction("🔌 断开连接", this);
    m_refreshAction = new QAction("🔄 刷新连接", this);
    m_refreshDatabaseAction = new QAction("🔄 刷新数据库", this);
    m_flushDatabaseAction = new QAction("🗑️ 清空数据库", this);
    m_deleteKeyAction = new QAction("❌ 删除键", this);
    m_deleteAction = new QAction("🗑️ 删除连接", this);
    m_newQueryAction = new QAction("📝 新建查询", this);
    
    m_contextMenu->addAction(m_connectAction);
    m_contextMenu->addAction(m_disconnectAction);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction(m_refreshAction);
    m_contextMenu->addAction(m_refreshDatabaseAction);
    m_contextMenu->addAction(m_flushDatabaseAction);
    m_contextMenu->addAction(m_deleteKeyAction);
    m_contextMenu->addAction(m_newQueryAction);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction(m_deleteAction);
}

void DatabaseTreeWidget::addConnection(const QString& name, Connx::Connection* connection) {
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

    // 智能展开/折叠逻辑
    if (itemType == "connection" || itemType == "database" ||
        itemType.endsWith("Folder") || itemType == "placeholder") {

        if (item->isExpanded()) {
            // 如果已展开，则折叠
            item->setExpanded(false);
        } else {
            // 如果未展开，则展开并加载数据
            if (itemType == "connection") {
                // 连接双击时建立连接
                emit connectionRequested(connectionName);
            }

            // 展开节点
            item->setExpanded(true);

            // 如果是占位符节点，触发数据加载
            if (item->childCount() == 1) {
                QTreeWidgetItem* firstChild = item->child(0);
                if (firstChild && firstChild->data(0, Qt::UserRole).toString() == "placeholder") {
                    onItemExpanded(item);
                }
            }
        }
    } else if (itemType == "table" || itemType == "key") {
        // 表和键的双击处理
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
    if (!item) return;

    QString itemType = item->data(0, Qt::UserRole).toString();

    if (itemType == "connection") {
        QString connectionName = m_itemConnectionMap.value(item, "");
        if (m_connections.contains(connectionName)) {
            // 清除占位符
            qDeleteAll(item->takeChildren());

            // 加载数据库列表
            Connx::Connection* conn = m_connections[connectionName];
            loadDatabases(item, conn);
        }
    } else if (itemType == "database") {
        // 数据库节点已经在loadDatabases中创建了层级结构，不需要再次加载
        return;
    } else {
        // 处理文件夹类型节点
        bool isNumeric;
        int nodeTypeInt = itemType.toInt(&isNumeric);
        if (isNumeric) {
            DbNodeType nodeType = static_cast<DbNodeType>(nodeTypeInt);

            // 获取连接信息
            QString connectionName = item->data(0, Qt::UserRole + 1).toString();
            if (!m_connections.contains(connectionName)) {
                qDebug() << "连接不存在:" << connectionName;
                return;
            }

            Connx::Connection* conn = m_connections[connectionName];

            // 获取数据库名称
            QString database;
            QTreeWidgetItem* dbItem = item->parent();
            if (dbItem) {
                database = dbItem->text(0);
            }

            // 清除占位符
            qDeleteAll(item->takeChildren());

            // 根据节点类型加载相应内容
            loadFolderContent(item, conn, database, nodeType);
        }
    }
}

void DatabaseTreeWidget::loadDatabases(QTreeWidgetItem* connectionItem, Connx::Connection* connection) {
    if (!connectionItem || !connection) {
        qDebug() << "loadDatabases: 参数为空";
        return;
    }

    if (!connection->isConnected()) {
        QTreeWidgetItem* errorItem = new QTreeWidgetItem(connectionItem);
        errorItem->setText(0, "未连接");
        errorItem->setIcon(0, style()->standardIcon(QStyle::SP_MessageBoxWarning));
        return;
    }

    // 禁用更新以提高性能
    setUpdatesEnabled(false);

    QStringList databases;
    try {
        databases = connection->getDatabases();
        qDebug() << "获取到数据库列表:" << databases.size() << "个数据库";
    } catch (...) {
        qDebug() << "获取数据库列表时发生异常";
        setUpdatesEnabled(true);
        return;
    }

    // 获取数据库类型的层级结构
    QList<DbHierarchyNode> hierarchy = DatabaseHierarchyManager::instance().getHierarchy(connection->getType());

    // 批量创建数据库节点
    QList<QTreeWidgetItem*> dbItems;
    dbItems.reserve(databases.size());

    QString parentConnectionName = m_itemConnectionMap.value(connectionItem, "");

    for (const QString& db : databases) {
        // 创建数据库节点数据
        DbNodeData dbNodeData(DbNodeType::Database, QString("db_%1").arg(db), db);
        dbNodeData.database = db;
        dbNodeData.fullName = db;

        DbTreeWidgetItem* dbItem = new DbTreeWidgetItem(nullptr, dbNodeData);
        dbItem->setData(0, Qt::UserRole, "database");
        dbItem->setData(0, Qt::UserRole + 1, parentConnectionName);

        // 添加到映射中
        m_itemConnectionMap[dbItem] = parentConnectionName;

        // 根据数据库类型添加层级结构
        if (connection->getType() == Connx::ConnectionType::Redis) {
            // Redis直接显示键，添加占位符
            QTreeWidgetItem* placeholder = new QTreeWidgetItem(dbItem);
            placeholder->setText(0, "加载中...");
            placeholder->setData(0, Qt::UserRole, "placeholder");
        } else {
            // 其他数据库类型添加文件夹结构
            for (const DbHierarchyNode& node : hierarchy) {
                DbNodeData folderData(node.nodeType, QString("%1_%2_%3").arg(db).arg(static_cast<int>(node.nodeType)).arg(node.displayName), node.displayName);
                folderData.database = db;
                folderData.fullName = QString("%1.%2").arg(db).arg(node.displayName);

                DbTreeWidgetItem* folderItem = new DbTreeWidgetItem(dbItem, folderData);
                folderItem->setData(0, Qt::UserRole, QString::number(static_cast<int>(node.nodeType)));
                folderItem->setData(0, Qt::UserRole + 1, parentConnectionName);

                // 为需要延迟加载的节点添加占位符
                if (DatabaseHierarchyManager::instance().needsLazyLoading(node.nodeType)) {
                    QTreeWidgetItem* placeholder = new QTreeWidgetItem(folderItem);
                    placeholder->setText(0, "加载中...");
                    placeholder->setData(0, Qt::UserRole, "placeholder");
                }
            }
        }

        dbItems.append(dbItem);
    }

    // 批量添加到父节点
    connectionItem->addChildren(dbItems);

    // 重新启用更新
    setUpdatesEnabled(true);
}

void DatabaseTreeWidget::loadFolderContent(QTreeWidgetItem* folderItem, Connx::Connection* connection, const QString& database, DbNodeType folderType) {
    if (!folderItem || !connection) {
        qDebug() << "loadFolderContent: 参数为空";
        return;
    }

    // 禁用更新以提高性能
    setUpdatesEnabled(false);

    QString connectionName = folderItem->data(0, Qt::UserRole + 1).toString();

    try {
        switch (folderType) {
            case DbNodeType::TableFolder: {
                QStringList tables = connection->getTables(database);
                QList<QTreeWidgetItem*> tableItems;
                tableItems.reserve(tables.size());

                for (const QString& table : tables) {
                    DbNodeData tableData(DbNodeType::Table, QString("table_%1_%2").arg(database).arg(table), table);
                    tableData.database = database;
                    tableData.fullName = QString("%1.%2").arg(database).arg(table);

                    DbTreeWidgetItem* tableItem = new DbTreeWidgetItem(nullptr, tableData);
                    tableItem->setData(0, Qt::UserRole, "table");
                    tableItem->setData(0, Qt::UserRole + 1, connectionName);
                    tableItems.append(tableItem);
                }

                folderItem->addChildren(tableItems);
                break;
            }

            case DbNodeType::ViewFolder: {
                // 获取视图列表（如果数据库支持）
                QStringList views;
                if (connection->getType() == Connx::ConnectionType::MySQL) {
                    // MySQL获取视图的SQL
                    Connx::QueryResult result = connection->query(
                        QString("SELECT TABLE_NAME FROM information_schema.VIEWS WHERE TABLE_SCHEMA = '%1'").arg(database)
                    );
                    if (result.success) {
                        for (const QVariantList& row : result.rows) {
                            if (!row.isEmpty()) {
                                views << row.first().toString();
                            }
                        }
                    }
                }

                QList<QTreeWidgetItem*> viewItems;
                viewItems.reserve(views.size());

                for (const QString& view : views) {
                    DbNodeData viewData(DbNodeType::View, QString("view_%1_%2").arg(database).arg(view), view);
                    viewData.database = database;
                    viewData.fullName = QString("%1.%2").arg(database).arg(view);

                    DbTreeWidgetItem* viewItem = new DbTreeWidgetItem(nullptr, viewData);
                    viewItem->setData(0, Qt::UserRole, "view");
                    viewItem->setData(0, Qt::UserRole + 1, connectionName);
                    viewItems.append(viewItem);
                }

                folderItem->addChildren(viewItems);
                break;
            }

            case DbNodeType::ProcedureFolder: {
                // 获取存储过程列表
                QStringList procedures;
                if (connection->getType() == Connx::ConnectionType::MySQL) {
                    Connx::QueryResult result = connection->query(
                        QString("SELECT ROUTINE_NAME FROM information_schema.ROUTINES WHERE ROUTINE_SCHEMA = '%1' AND ROUTINE_TYPE = 'PROCEDURE'").arg(database)
                    );
                    if (result.success) {
                        for (const QVariantList& row : result.rows) {
                            if (!row.isEmpty()) {
                                procedures << row.first().toString();
                            }
                        }
                    }
                }

                QList<QTreeWidgetItem*> procedureItems;
                procedureItems.reserve(procedures.size());

                for (const QString& procedure : procedures) {
                    DbNodeData procData(DbNodeType::Procedure, QString("proc_%1_%2").arg(database).arg(procedure), procedure);
                    procData.database = database;
                    procData.fullName = QString("%1.%2").arg(database).arg(procedure);

                    DbTreeWidgetItem* procItem = new DbTreeWidgetItem(nullptr, procData);
                    procItem->setData(0, Qt::UserRole, "procedure");
                    procItem->setData(0, Qt::UserRole + 1, connectionName);
                    procedureItems.append(procItem);
                }

                folderItem->addChildren(procedureItems);
                break;
            }

            case DbNodeType::FunctionFolder: {
                // 获取函数列表
                QStringList functions;
                if (connection->getType() == Connx::ConnectionType::MySQL) {
                    Connx::QueryResult result = connection->query(
                        QString("SELECT ROUTINE_NAME FROM information_schema.ROUTINES WHERE ROUTINE_SCHEMA = '%1' AND ROUTINE_TYPE = 'FUNCTION'").arg(database)
                    );
                    if (result.success) {
                        for (const QVariantList& row : result.rows) {
                            if (!row.isEmpty()) {
                                functions << row.first().toString();
                            }
                        }
                    }
                }

                QList<QTreeWidgetItem*> functionItems;
                functionItems.reserve(functions.size());

                for (const QString& function : functions) {
                    DbNodeData funcData(DbNodeType::Function, QString("func_%1_%2").arg(database).arg(function), function);
                    funcData.database = database;
                    funcData.fullName = QString("%1.%2").arg(database).arg(function);

                    DbTreeWidgetItem* funcItem = new DbTreeWidgetItem(nullptr, funcData);
                    funcItem->setData(0, Qt::UserRole, "function");
                    funcItem->setData(0, Qt::UserRole + 1, connectionName);
                    functionItems.append(funcItem);
                }

                folderItem->addChildren(functionItems);
                break;
            }

            case DbNodeType::TriggerFolder: {
                // 获取触发器列表
                QStringList triggers;
                if (connection->getType() == Connx::ConnectionType::MySQL) {
                    Connx::QueryResult result = connection->query(
                        QString("SELECT TRIGGER_NAME FROM information_schema.TRIGGERS WHERE TRIGGER_SCHEMA = '%1'").arg(database)
                    );
                    if (result.success) {
                        for (const QVariantList& row : result.rows) {
                            if (!row.isEmpty()) {
                                triggers << row.first().toString();
                            }
                        }
                    }
                }

                QList<QTreeWidgetItem*> triggerItems;
                triggerItems.reserve(triggers.size());

                for (const QString& trigger : triggers) {
                    DbNodeData triggerData(DbNodeType::Trigger, QString("trigger_%1_%2").arg(database).arg(trigger), trigger);
                    triggerData.database = database;
                    triggerData.fullName = QString("%1.%2").arg(database).arg(trigger);

                    DbTreeWidgetItem* triggerItem = new DbTreeWidgetItem(nullptr, triggerData);
                    triggerItem->setData(0, Qt::UserRole, "trigger");
                    triggerItem->setData(0, Qt::UserRole + 1, connectionName);
                    triggerItems.append(triggerItem);
                }

                folderItem->addChildren(triggerItems);
                break;
            }

            default:
                qDebug() << "不支持的文件夹类型:" << static_cast<int>(folderType);
                break;
        }

    } catch (...) {
        qDebug() << "加载文件夹内容时发生异常:" << static_cast<int>(folderType);
    }

    // 重新启用更新
    setUpdatesEnabled(true);
}

void DatabaseTreeWidget::loadTables(QTreeWidgetItem* databaseItem, Connx::Connection* connection, const QString& database) {
    if (!databaseItem || !connection) {
        qDebug() << "loadTables: 参数为空";
        return;
    }

    // 对于Redis，加载键列表
    if (connection->getType() == Connx::ConnectionType::Redis) {
        loadRedisKeys(databaseItem, connection, database);
    } else {
        // 其他数据库类型加载表列表
        QStringList tables = connection->getTables(database);

        // 获取连接名称
        QTreeWidgetItem* connectionItem = databaseItem->parent();
        QString connectionName;
        if (connectionItem) {
            connectionName = m_itemConnectionMap.value(connectionItem, "");
        }

        // 禁用更新以提高性能
        setUpdatesEnabled(false);

        // 批量创建表节点
        QList<QTreeWidgetItem*> tableItems;
        tableItems.reserve(tables.size());

        for (const QString& table : tables) {
            QTreeWidgetItem* tableItem = new QTreeWidgetItem();
            tableItem->setText(0, table);
            tableItem->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
            tableItem->setData(0, Qt::UserRole, "table");
            tableItem->setData(0, Qt::UserRole + 1, connectionName);
            tableItems.append(tableItem);
        }

        // 批量添加到父节点
        databaseItem->addChildren(tableItems);

        // 重新启用更新
        setUpdatesEnabled(true);
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
    m_refreshAction->setVisible(itemType == "connection");
    m_refreshDatabaseAction->setVisible(itemType == "database");
    m_flushDatabaseAction->setVisible(itemType == "database");
    m_deleteKeyAction->setVisible(itemType == "key");
    m_newQueryAction->setVisible(itemType == "connection" || itemType == "database");
    
    // 连接信号槽
    QObject::disconnect(m_refreshDatabaseAction, nullptr, nullptr, nullptr);
    QObject::disconnect(m_flushDatabaseAction, nullptr, nullptr, nullptr);
    QObject::disconnect(m_deleteKeyAction, nullptr, nullptr, nullptr);
    
    if (itemType == "database") {
        QObject::connect(m_refreshDatabaseAction, &QAction::triggered, [this, item]() {
            refreshDatabase(item);
        });
        QObject::connect(m_flushDatabaseAction, &QAction::triggered, [this, item]() {
            flushDatabase(item);
        });
    } else if (itemType == "key") {
        QObject::connect(m_deleteKeyAction, &QAction::triggered, [this, item]() {
            deleteKey(item);
        });
    }
    
    m_contextMenu->exec(mapToGlobal(pos));
}

void DatabaseTreeWidget::loadRedisKeys(QTreeWidgetItem* databaseItem, Connx::Connection* connection, const QString& database) {
    if (!connection || !connection->isConnected()) {
        return;
    }
    
    try {
        // 从数据库显示名称中提取数据库编号 (如 "DB0" -> "0")
        QString dbNumber = database;
        if (database.startsWith("DB")) {
            dbNumber = database.mid(2); // 去掉前面的 "DB"
        }
        
        // 切换到指定的数据库
        QVariantList selectParams;
        selectParams.append(dbNumber);
        connection->execute("SELECT", selectParams);
        
        // 使用SCAN命令获取所有键，避免KEYS *可能造成的阻塞
        QStringList allKeys;
        QString cursor = "0";
        
        do {
            QVariantList scanParams;
            scanParams.append(cursor);
            scanParams.append("MATCH");
            scanParams.append("*");
            scanParams.append("COUNT");
            scanParams.append(100); // 每次扫描100个键
            
            Connx::QueryResult result = connection->execute("SCAN", scanParams);
            if (result.success && result.data.size() >= 2) {
                cursor = result.data[0].toString();
                QVariantList keys = result.data[1].toList();
                
                for (const QVariant& key : keys) {
                    allKeys.append(key.toString());
                }
            } else {
                break;
            }
        } while (cursor != "0" && allKeys.size() < 1000); // 限制最多显示1000个键
        
        // 创建键节点
        for (const QString& key : allKeys) {
            QTreeWidgetItem* keyItem = new QTreeWidgetItem(databaseItem);
            keyItem->setText(0, key);
            keyItem->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
            keyItem->setData(0, Qt::UserRole, "key");
            keyItem->setData(0, Qt::UserRole + 1, m_itemConnectionMap[databaseItem->parent()]);
            keyItem->setData(0, Qt::UserRole + 2, database);
            keyItem->setData(0, Qt::UserRole + 3, key);
        }
        
        // 如果键太多，添加提示
        if (allKeys.size() >= 1000) {
            QTreeWidgetItem* moreItem = new QTreeWidgetItem(databaseItem);
            moreItem->setText(0, "... (更多键，请使用查询)");
            moreItem->setIcon(0, style()->standardIcon(QStyle::SP_MessageBoxInformation));
            moreItem->setData(0, Qt::UserRole, "more_keys");
        }
        
    } catch (...) {
        // 如果出错，显示错误提示
        QTreeWidgetItem* errorItem = new QTreeWidgetItem(databaseItem);
        errorItem->setText(0, "加载键失败");
        errorItem->setIcon(0, style()->standardIcon(QStyle::SP_MessageBoxCritical));
        errorItem->setData(0, Qt::UserRole, "error");
    }
}

void DatabaseTreeWidget::refreshDatabase(QTreeWidgetItem* databaseItem) {
    if (!databaseItem) return;
    
    // 获取连接信息
    QTreeWidgetItem* connectionItem = databaseItem->parent();
    if (!connectionItem) return;
    
    QString connectionName = m_itemConnectionMap.value(connectionItem, "");
    if (connectionName.isEmpty() || !m_connections.contains(connectionName)) return;
    
    Connx::Connection* connection = m_connections[connectionName];
    if (!connection || !connection->isConnected()) return;
    
    QString database = databaseItem->text(0);
    
    // 清除所有子项
    qDeleteAll(databaseItem->takeChildren());
    
    // 重新加载
    if (connection->getType() == Connx::ConnectionType::Redis) {
        loadRedisKeys(databaseItem, connection, database);
    } else {
        loadTables(databaseItem, connection, database);
    }
    
    // 展开节点以显示新内容
    databaseItem->setExpanded(true);
}

void DatabaseTreeWidget::flushDatabase(QTreeWidgetItem* databaseItem) {
    if (!databaseItem) return;
    
    // 获取连接信息
    QTreeWidgetItem* connectionItem = databaseItem->parent();
    if (!connectionItem) return;
    
    QString connectionName = m_itemConnectionMap.value(connectionItem, "");
    if (connectionName.isEmpty() || !m_connections.contains(connectionName)) return;
    
    Connx::Connection* connection = m_connections[connectionName];
    if (!connection || !connection->isConnected()) return;
    
    QString database = databaseItem->text(0);
    
    // 确认对话框
    QMessageBox msgBox(this);
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setWindowTitle("确认清空数据库");
    msgBox.setText(QString("您确定要清空数据库 %1 吗？").arg(database));
    msgBox.setInformativeText("此操作将删除该数据库中的所有键，且不可恢复！");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    
    if (msgBox.exec() != QMessageBox::Yes) {
        return;
    }
    
    try {
        // 从数据库显示名称中提取数据库编号
        QString dbNumber = database;
        if (database.startsWith("DB")) {
            dbNumber = database.mid(2);
        }
        
        // 切换到指定的数据库
        QVariantList selectParams;
        selectParams.append(dbNumber);
        connection->execute("SELECT", selectParams);
        
        // 执行FLUSHDB命令清空当前数据库
        Connx::QueryResult result = connection->execute("FLUSHDB");
        if (result.success) {
            // 清空成功，刷新显示
            qDeleteAll(databaseItem->takeChildren());
            QMessageBox::information(this, "成功", QString("数据库 %1 已清空").arg(database));
        } else {
            QMessageBox::critical(this, "错误", QString("清空数据库失败：%1").arg(result.errorMessage));
        }
        
    } catch (...) {
        QMessageBox::critical(this, "错误", "清空数据库时发生未知错误");
    }
}

void DatabaseTreeWidget::deleteKey(QTreeWidgetItem* keyItem) {
    if (!keyItem) return;
    
    // 获取键信息
    QString keyName = keyItem->data(0, Qt::UserRole + 3).toString();
    QString database = keyItem->data(0, Qt::UserRole + 2).toString();
    QString connectionName = keyItem->data(0, Qt::UserRole + 1).toString();
    
    if (keyName.isEmpty() || connectionName.isEmpty() || !m_connections.contains(connectionName)) return;
    
    Connx::Connection* connection = m_connections[connectionName];
    if (!connection || !connection->isConnected()) return;
    
    // 确认对话框
    QMessageBox msgBox(this);
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setWindowTitle("确认删除键");
    msgBox.setText(QString("您确定要删除键 '%1' 吗？").arg(keyName));
    msgBox.setInformativeText("此操作不可恢复！");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    
    if (msgBox.exec() != QMessageBox::Yes) {
        return;
    }
    
    try {
        // 从数据库显示名称中提取数据库编号
        QString dbNumber = database;
        if (database.startsWith("DB")) {
            dbNumber = database.mid(2);
        }
        
        // 切换到指定的数据库
        QVariantList selectParams;
        selectParams.append(dbNumber);
        connection->execute("SELECT", selectParams);
        
        // 执行DEL命令删除键
        QVariantList delParams;
        delParams.append(keyName);
        Connx::QueryResult result = connection->execute("DEL", delParams);
        
        if (result.success) {
            // 删除成功，从树中移除该项
            QTreeWidgetItem* parent = keyItem->parent();
            if (parent) {
                parent->removeChild(keyItem);
                delete keyItem;
            }
            QMessageBox::information(this, "成功", QString("键 '%1' 已删除").arg(keyName));
        } else {
            QMessageBox::critical(this, "错误", QString("删除键失败：%1").arg(result.errorMessage));
        }
        
    } catch (...) {
        QMessageBox::critical(this, "错误", "删除键时发生未知错误");
    }
}

void DatabaseTreeWidget::setConnection(Connx::Connection* connection) {
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
                Connx::Connection* conn = m_connections[connectionName];
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
QueryTab::QueryTab(Connx::Connection* connection, QWidget* parent)
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

    // 设置表头调整策略
    m_resultTable->horizontalHeader()->setStretchLastSection(false);
    m_resultTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_resultTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_resultTable->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

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

    QString query;
    if (m_queryEdit->textCursor().hasSelection()) {
        query = m_queryEdit->textCursor().selectedText();
    } else {
        query = m_queryEdit->toPlainText();
    }

    query = query.trimmed();
    if (query.isEmpty()) {
        QMessageBox::information(this, "提示", "请输入要执行的命令");
        return;
    }

    m_statusLabel->setText("正在执行...");

    // 不再做手工解析，直接交给连接执行
    Connx::QueryResult result = m_connection->execute(query);
    updateResults(result);

    emit queryExecuted(result);
}


void QueryTab::onClearClicked() const {
    m_queryEdit->clear();
    m_resultTable->setRowCount(0);
    m_resultTable->setColumnCount(0);
    m_messagesText->clear();
    m_statusLabel->setText("就绪");
    m_timeLabel->setText("执行时间: -");
    m_rowsLabel->setText("行数: 0");
}

void QueryTab::onFormatClicked() const {
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

void QueryTab::updateResults(const Connx::QueryResult& result) {
    m_timeLabel->setText(QString("执行时间: %1ms").arg(result.executionTime));

    if (result.success) {
        m_statusLabel->setText("✅ 执行成功");

        // 调试输出
        qDebug() << "QueryResult data size:" << result.data.size();
        for (int i = 0; i < qMin(3, result.data.size()); ++i) {
            qDebug() << "Row" << i << ":" << result.data[i];
            if (result.data[i].typeId() == QMetaType::QVariantList) {
                QVariantList row = result.data[i].toList();
                qDebug() << "  Row as list size:" << row.size();
                for (int j = 0; j < qMin(5, row.size()); ++j) {
                    qDebug() << "    Col" << j << ":" << row[j];
                }
            }
        }

        // 更新结果表格 - 性能优化版本
        if (!result.columns.isEmpty() && !result.rows.isEmpty()) {
            // 新的结构化数据
            int columnCount = result.columns.size();
            int rowCount = result.rows.size();

            // 禁用排序和更新以提高性能
            m_resultTable->setSortingEnabled(false);
            m_resultTable->setUpdatesEnabled(false);

            // 清除旧数据
            m_resultTable->clear();

            // 设置行列数
            m_resultTable->setRowCount(rowCount);
            m_resultTable->setColumnCount(columnCount);

            // 设置列头
            m_resultTable->setHorizontalHeaderLabels(result.columns);

            // 批量填充数据行 - 性能优化
            QVector<QTableWidgetItem*> items;
            items.reserve(rowCount * columnCount);

            for (int i = 0; i < rowCount; ++i) {
                const QVariantList& row = result.rows[i];
                for (int j = 0; j < qMin(row.size(), columnCount); ++j) {
                    QTableWidgetItem* item = new QTableWidgetItem(row[j].toString());
                    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                    items.append(item);
                    m_resultTable->setItem(i, j, item);
                }
            }

            // 重新启用更新
            m_resultTable->setUpdatesEnabled(true);

            // 延迟调整列宽以避免阻塞UI
            QTimer::singleShot(0, [this, columnCount]() {
                // 智能列宽调整 - 只对前几列进行自动调整
                for (int j = 0; j < qMin(columnCount, 10); ++j) {
                    m_resultTable->resizeColumnToContents(j);
                    int width = m_resultTable->columnWidth(j);
                    if (width > 300) {
                        m_resultTable->setColumnWidth(j, 300);
                    } else if (width < 80) {
                        m_resultTable->setColumnWidth(j, 80);
                    }
                }

                // 其余列设置默认宽度
                for (int j = 10; j < columnCount; ++j) {
                    m_resultTable->setColumnWidth(j, 120);
                }
            });

            // 确保表头可见
            m_resultTable->horizontalHeader()->setVisible(true);
            m_resultTable->verticalHeader()->setVisible(true);

            // 启用排序
            m_resultTable->setSortingEnabled(true);

            m_rowsLabel->setText(QString("行数: %1").arg(rowCount));
        } else if (!result.data.isEmpty()) {
            // 兼容旧格式 - 处理Redis等其他类型的结果
            QVariant firstData = result.data.first();

            if (firstData.typeId() == QMetaType::QVariantList) {
                // 数组结果（单列数据）
                QVariantList list = firstData.toList();
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
                // 单一结果（非数组）
                m_resultTable->setRowCount(1);
                m_resultTable->setColumnCount(1);
                m_resultTable->setHorizontalHeaderLabels({"结果"});

                QTableWidgetItem* item = new QTableWidgetItem(firstData.toString());
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
    
    // 初始化连接配置表管理器
    m_configManager = new SqliteWrapper::ConnectionConfigTableManager(this);
    
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
        Connx::ConnectionConfig config = dialog.getConnectionConfig();
        
        if (m_connectionConfigs.contains(config.name)) {
            m_statusLabel->setText("❌ 连接名称已存在，请使用不同的名称");
            return;
        }
        
        m_connectionConfigs[config.name] = config;
        
        // 保存到SQLite数据库
        SqliteWrapper::ConnectionConfigEntity configEntity;
        configEntity.name = config.name;
        configEntity.type = config.extraParams.value("connectionType", "Redis").toString();
        configEntity.host = config.host;
        configEntity.port = config.port;
        configEntity.username = config.username;
        configEntity.password = config.password;
        configEntity.databaseName = config.database;
        configEntity.timeout = config.timeout;
        configEntity.useSSL = config.useSSL;
        configEntity.extraParams = config.extraParams;
        configEntity.createdAt = QDateTime::currentDateTime();
        configEntity.updatedAt = QDateTime::currentDateTime();
        
        bool saved = m_configManager->saveConfig(configEntity);
        if (!saved) {
            m_statusLabel->setText("❌ 无法保存连接配置到数据库");
            return;
        }
        
        // 创建连接对象
        QString typeString = config.extraParams.value("connectionType", "Redis").toString();
        Connx::ConnectionType type = Connx::ConnectionFactory::getTypeFromString(typeString);
        Connx::Connection* connection = Connx::ConnectionFactory::createConnection(type, config);
        if (connection) {
            m_connections[config.name] = connection;
            m_treeWidget->addConnection(config.name, connection);
            
            QObject::connect(connection, &Connx::Connection::stateChanged, [this, config](Connx::ConnectionState state) {
                onConnectionStateChanged(config.name, state);
            });
        }
        
        updateConnectionStatus();
        m_statusLabel->setText("连接已保存: " + config.name);
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
    Connx::Connection* connection = m_connections.value(connectionName, nullptr);
    
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
    // 从SQLite加载所有连接配置
    QList<SqliteWrapper::ConnectionConfigEntity> configs = m_configManager->getAllConfigs();
    
    for (const SqliteWrapper::ConnectionConfigEntity& configEntity : configs) {
        Connx::ConnectionConfig config;
        config.name = configEntity.name;
        config.host = configEntity.host;
        config.port = configEntity.port;
        config.username = configEntity.username;
        config.password = configEntity.password;
        config.database = configEntity.databaseName;
        config.timeout = configEntity.timeout;
        config.useSSL = configEntity.useSSL;
        config.extraParams = configEntity.extraParams;
        
        m_connectionConfigs[config.name] = config;
        
        // 创建连接对象
        QString typeString = configEntity.type;

        // 修复历史数据中可能错误的连接类型
        if (typeString.isEmpty() || typeString == "Unknown" ||
            (typeString == "Redis" && config.port != 6379 && !config.database.contains(QRegularExpression("^\\d+$")))) {

            // 根据端口和配置推断连接类型
            if (config.port == 3306 && !config.database.isEmpty()) {
                typeString = "MySQL";
            } else if (config.port == 5432) {
                typeString = "PostgreSQL";
            } else if (config.port == 6379 || config.database.contains(QRegularExpression("^\\d+$"))) {
                typeString = "Redis";
            } else if (config.extraParams.contains("connectionType")) {
                typeString = config.extraParams["connectionType"].toString();
            } else {
                // 最后的推断：如果有数据库名且端口不是Redis默认端口，推测为SQL数据库
                if (!config.database.isEmpty() && !config.database.contains(QRegularExpression("^\\d+$"))) {
                    typeString = "MySQL"; // MySQL是最常见的SQL数据库
                } else {
                    typeString = "Redis"; // 默认
                }
            }

            qDebug() << "Connection type corrected for" << config.name << "from" << configEntity.type << "to" << typeString;
        }

        Connx::ConnectionType type = Connx::ConnectionFactory::getTypeFromString(typeString);
        Connx::Connection* connection = Connx::ConnectionFactory::createConnection(type, config);
        if (connection) {
            m_connections[config.name] = connection;
            m_treeWidget->addConnection(config.name, connection);
            
            QObject::connect(connection, &Connx::Connection::stateChanged, [this, config](Connx::ConnectionState state) {
                onConnectionStateChanged(config.name, state);
            });
        }
    }
    
    updateConnectionStatus();
    m_statusLabel->setText(QString("已加载 %1 个连接配置").arg(configs.size()));
}

void DatabaseTool::saveConnections() {
    // 连接配置已经实时保存到SQLite，这里不需要额外操作
    qDebug() << "连接配置已持久化到SQLite数据库";
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
    
    Connx::Connection* connection = m_connections[connectionName];
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

void DatabaseTool::onConnectionStateChanged(const QString& name, Connx::ConnectionState state) {
    Q_UNUSED(name)
    Q_UNUSED(state)
    updateConnectionStatus();
}

// 其他槽函数实现
void DatabaseTool::onEditConnection() {
    QTreeWidgetItem* current = m_treeWidget->currentItem();
    if (!current) return;
    
    QString itemType = current->data(0, Qt::UserRole).toString();
    if (itemType != "connection") return;
    
    QString connectionName = current->text(0);
    if (!m_connectionConfigs.contains(connectionName)) return;
    
    Connx::ConnectionConfig config = m_connectionConfigs[connectionName];
    
    ConnectionDialog dialog(config, this);
    if (dialog.exec() == QDialog::Accepted) {
        Connx::ConnectionConfig newConfig = dialog.getConnectionConfig();
        
        // 更新配置
        m_connectionConfigs[connectionName] = newConfig;
        
        // 保存到SQLite
        QVariantMap configMap;
        configMap["name"] = newConfig.name;
        configMap["type"] = newConfig.extraParams.value("connectionType", "Redis").toString();
        configMap["host"] = newConfig.host;
        configMap["port"] = newConfig.port;
        configMap["username"] = newConfig.username;
        configMap["password"] = newConfig.password;
        configMap["database"] = newConfig.database;
        configMap["timeout"] = newConfig.timeout;
        configMap["useSSL"] = newConfig.useSSL;
        configMap["extraParams"] = QVariant::fromValue(newConfig.extraParams);
        
        // 将configMap转换为ConnectionConfigEntity
        SqliteWrapper::ConnectionConfigEntity entity;
        entity.name = connectionName;
        entity.type = configMap["type"].toString();
        entity.host = configMap["host"].toString();
        entity.port = configMap["port"].toInt();
        entity.username = configMap["username"].toString();
        entity.password = configMap["password"].toString();
        entity.databaseName = configMap["database"].toString();
        entity.timeout = configMap["timeout"].toInt();
        entity.useSSL = configMap["useSSL"].toBool();
        entity.extraParams = configMap["extraParams"].toMap();
        
        m_configManager->updateConfig(connectionName, entity);
        m_statusLabel->setText("连接已更新: " + connectionName);
    }
}

void DatabaseTool::onDeleteConnection() {
    QTreeWidgetItem* current = m_treeWidget->currentItem();
    if (!current) return;
    
    QString itemType = current->data(0, Qt::UserRole).toString();
    if (itemType != "connection") return;
    
    QString connectionName = current->text(0);
    
    int ret = QMessageBox::question(this, "删除连接", 
                                   QString("确定要删除连接 '%1' 吗？").arg(connectionName),
                                   QMessageBox::Yes | QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        // 从SQLite删除
        if (m_configManager->deleteConfigByName(connectionName)) {
            // 断开并删除连接
            if (m_connections.contains(connectionName)) {
                m_connections[connectionName]->disconnectFromServer();
                m_connections[connectionName]->deleteLater();
                m_connections.remove(connectionName);
            }
            
            m_connectionConfigs.remove(connectionName);
            m_treeWidget->removeConnection(connectionName);
            
            updateConnectionStatus();
            m_statusLabel->setText("连接已删除: " + connectionName);
        } else {
            m_statusLabel->setText("❌ 无法从数据库删除连接配置");
        }
    }
}

void DatabaseTool::onDisconnectFromDatabase() {
    QTreeWidgetItem* current = m_treeWidget->currentItem();
    if (!current) return;
    
    QString itemType = current->data(0, Qt::UserRole).toString();
    if (itemType != "connection") return;
    
    QString connectionName = current->text(0);
    if (!m_connections.contains(connectionName)) return;
    
    Connx::Connection* connection = m_connections[connectionName];
    if (connection->isConnected()) {
        connection->disconnectFromServer();
        m_statusLabel->setText("已断开连接: " + connectionName);
        m_treeWidget->refreshConnection(connectionName);
    }
}

void DatabaseTool::onRefreshConnections() {
    // 重新加载所有连接配置
    m_connectionConfigs.clear();
    
    // 清理现有连接
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        it.value()->disconnectFromServer();
        it.value()->deleteLater();
    }
    m_connections.clear();
    
    // 清空树
    m_treeWidget->clear();
    
    // 重新加载
    loadConnections();
    
    m_statusLabel->setText("连接列表已刷新");
}

void DatabaseTool::onTabChanged(int index) { 
    Q_UNUSED(index)
    // 可以在这里添加标签页切换的逻辑
}

QueryTab* DatabaseTool::getCurrentQueryTab() {
    return qobject_cast<QueryTab*>(m_tabWidget->currentWidget());
}

#include "databasetool.moc"
