#include "connectiondialog.h"
#include <QMessageBox>

// ConnectionDialog 实现
ConnectionDialog::ConnectionDialog(QWidget* parent) : QDialog(parent) {
    setupUI();
    setWindowTitle(tr("新建连接"));
}

ConnectionDialog::ConnectionDialog(const Connx::ConnectionConfig& config, QWidget* parent) : QDialog(parent) {
    setupUI();
    setConnectionConfig(config);
    setWindowTitle(tr("编辑连接"));
}

void ConnectionDialog::setupUI() {
    setModal(true);
    setMinimumSize(580, 400);
    resize(580, 420);
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
    m_splitter->setSizes({ 160, 420 });

    m_mainLayout->addWidget(m_splitter);

    setStyleSheet(R"(
        QDialog { background: #fff; }
        QWidget#leftPanel {
            background: #f8f9fa;
            border-right: 1px solid #e9ecef;
        }
        QLabel#typeLabel {
            color: #212529;
            font-weight: bold;
            font-size: 10pt;
            padding: 10px;
        }
        QListWidget {
            background: transparent;
            border: none;
            outline: none;
            padding: 4px;
            font-size: 9pt;
        }
        QWidget#rightPanel { background: #fff; }
        QLineEdit, QSpinBox, QComboBox {
            padding: 4px 8px;
            border: 1px solid #dee2e6;
            border-radius: 4px;
            font-size: 9pt;
            min-width: 180px;
            height: 20px;
            background: #fff;
            color: #495057;
        }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus { border-color: #80bdff; }
        QSpinBox::up-button, QSpinBox::down-button { width: 0; height: 0; border: none; }
        QComboBox::drop-down { border: none; width: 20px; }
        QComboBox::down-arrow {
            image: none;
            border-left: 4px solid transparent;
            border-right: 4px solid transparent;
            border-top: 5px solid #868e96;
            margin-right: 6px;
        }
        QCheckBox { font-size: 9pt; color: #495057; }
        QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid #dee2e6; border-radius: 3px; }
        QCheckBox::indicator:checked { background: #228be6; border-color: #228be6; }
        QPushButton {
            padding: 6px 14px;
            font-size: 9pt;
            border: none;
            border-radius: 4px;
            color: #495057;
            background: transparent;
        }
        QPushButton:hover { background: #e9ecef; }
        QPushButton#testBtn { color: #228be6; font-weight: bold; }
        QPushButton#testBtn:hover { background: #e7f5ff; }
        QDialogButtonBox QPushButton {
            background: #228be6;
            color: white;
            min-width: 70px;
            padding: 6px 16px;
        }
        QDialogButtonBox QPushButton:hover { background: #1c7ed6; }
        QDialogButtonBox QPushButton[text="Cancel"] {
            background: transparent;
            color: #495057;
            border: 1px solid #dee2e6;
        }
        QDialogButtonBox QPushButton[text="Cancel"]:hover { background: #e9ecef; }
        QLabel { font-size: 9pt; color: #495057; }
        QLabel#statusLabel { color: #868e96; font-size: 8pt; padding: 4px; }
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
    m_leftPanel->setFixedWidth(160);
    m_leftLayout = new QVBoxLayout(m_leftPanel);
    m_leftLayout->setContentsMargins(0, 20, 0, 20);
    m_leftLayout->setSpacing(8);

    // 数据库类型标题
    m_typeLabel = new QLabel(tr("数据库类型"));
    m_typeLabel->setObjectName("typeLabel");
    m_leftLayout->addWidget(m_typeLabel);

    // 数据库类型列表
    m_typeList = new QListWidget();
    m_typeList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

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
    m_rightLayout->setContentsMargins(20, 16, 20, 12);
    m_rightLayout->setSpacing(10);

    // 表单区域 - 使用网格布局代替FormLayout
    m_formWidget = new QWidget();
    m_gridLayout = new QGridLayout(m_formWidget);
    m_gridLayout->setVerticalSpacing(6);
    m_gridLayout->setHorizontalSpacing(10);
    m_gridLayout->setColumnStretch(1, 1); // 第二列（输入控件）可拉伸

    // 创建所有表单控件
    createFormControls();

    m_rightLayout->addWidget(m_formWidget, 1);

    // 底部按钮区域
    m_buttonWidget = new QWidget();
    m_buttonWidget->setFixedHeight(80);
    m_buttonLayout = new QHBoxLayout(m_buttonWidget);

    // 测试连接按钮
    m_testBtn = new QPushButton(tr("🔍 测试连接"));
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
    m_nameEdit->setPlaceholderText(tr("为此连接起一个名称"));
    m_nameEdit->setFixedHeight(CONTROL_HEIGHT);

    m_hostEdit = new QLineEdit();
    m_hostEdit->setText(tr("localhost"));
    m_hostEdit->setPlaceholderText(tr("服务器地址"));
    m_hostEdit->setFixedHeight(CONTROL_HEIGHT);

    m_portSpin = new QSpinBox();
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(6379);
    m_portSpin->setFixedHeight(CONTROL_HEIGHT);

    m_usernameEdit = new QLineEdit();
    m_usernameEdit->setPlaceholderText(tr("用户名 (可选)"));
    m_usernameEdit->setFixedHeight(CONTROL_HEIGHT);

    m_passwordEdit = new QLineEdit();
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText(tr("密码 (可选)"));
    m_passwordEdit->setFixedHeight(CONTROL_HEIGHT);

    m_databaseEdit = new QLineEdit();
    m_databaseEdit->setText(tr("0"));
    m_databaseEdit->setFixedHeight(CONTROL_HEIGHT);

    m_timeoutSpin = new QSpinBox();
    m_timeoutSpin->setRange(5, 300);
    m_timeoutSpin->setValue(30);
    m_timeoutSpin->setSuffix(" 秒");
    m_timeoutSpin->setFixedHeight(CONTROL_HEIGHT);

    m_sslCheck = new QCheckBox("使用SSL加密连接");
    m_sslCheck->setFixedHeight(CONTROL_HEIGHT);

    // Redis特有控件
    m_dbIndexLabel = new QLabel(tr("数据库索引:"));
    m_dbIndexSpin = new QSpinBox();
    m_dbIndexSpin->setRange(0, 15);
    m_dbIndexSpin->setValue(0);
    m_dbIndexSpin->setFixedHeight(CONTROL_HEIGHT);

    // MySQL特有控件
    m_charsetLabel = new QLabel(tr("字符集:"));
    m_charsetCombo = new QComboBox();
    m_charsetCombo->addItems({ "utf8mb4", "utf8", "latin1", "gbk" });
    m_charsetCombo->setCurrentText("utf8mb4");
    m_charsetCombo->setFixedHeight(CONTROL_HEIGHT);
}

void ConnectionDialog::setConnectionConfig(const Connx::ConnectionConfig& config) const {
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
    const QString selectedType = m_typeList->currentItem() ? m_typeList->currentItem()->text() : "Redis";
    m_currentType = selectedType;
    updateFormForType(selectedType);
}

void ConnectionDialog::updateFormForType(const QString& type) const {
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
        label->setStyleSheet("QLabel { font-size: 9pt; color: #495057; }");
        label->setFixedHeight(28);
        return label;
    };

    // 基本信息（所有类型都有）
    m_gridLayout->addWidget(createLabel("连接名称:"), row, 0, Qt::AlignVCenter | Qt::AlignRight);
    m_gridLayout->addWidget(m_nameEdit, row++, 1);

    m_gridLayout->addWidget(createLabel("主机地址:"), row, 0, Qt::AlignVCenter | Qt::AlignRight);
    m_gridLayout->addWidget(m_hostEdit, row++, 1);

    m_gridLayout->addWidget(createLabel("端口:"), row, 0, Qt::AlignVCenter | Qt::AlignRight);
    m_gridLayout->addWidget(m_portSpin, row++, 1);

    if (type == "Redis") {
        // Redis特有表单
        m_passwordEdit->setPlaceholderText(tr("Redis密码 (可选)"));

        m_gridLayout->addWidget(createLabel("密码:"), row, 0, Qt::AlignVCenter | Qt::AlignRight);
        m_gridLayout->addWidget(m_passwordEdit, row++, 1);

        m_gridLayout->addWidget(createLabel("数据库索引:"), row, 0, Qt::AlignVCenter | Qt::AlignRight);
        m_gridLayout->addWidget(m_dbIndexSpin, row++, 1);

        m_gridLayout->addWidget(createLabel("超时:"), row, 0, Qt::AlignVCenter | Qt::AlignRight);
        m_gridLayout->addWidget(m_timeoutSpin, row++, 1);

        m_gridLayout->addWidget(new QLabel(""), row, 0); // 空标签占位
        m_gridLayout->addWidget(m_sslCheck, row++, 1);
    } else if (type == "MySQL" || type == "PostgreSQL") {
        // SQL数据库表单
        m_passwordEdit->setPlaceholderText(tr("数据库密码"));
        m_databaseEdit->setPlaceholderText(tr("数据库名称"));

        m_gridLayout->addWidget(createLabel("用户名:"), row, 0, Qt::AlignVCenter | Qt::AlignRight);
        m_gridLayout->addWidget(m_usernameEdit, row++, 1);

        m_gridLayout->addWidget(createLabel("密码:"), row, 0, Qt::AlignVCenter | Qt::AlignRight);
        m_gridLayout->addWidget(m_passwordEdit, row++, 1);

        m_gridLayout->addWidget(createLabel("数据库:"), row, 0, Qt::AlignVCenter | Qt::AlignRight);
        m_gridLayout->addWidget(m_databaseEdit, row++, 1);

        if (type == "MySQL") {
            m_gridLayout->addWidget(createLabel("字符集:"), row, 0, Qt::AlignVCenter | Qt::AlignRight);
            m_gridLayout->addWidget(m_charsetCombo, row++, 1);
        }

        m_gridLayout->addWidget(createLabel("超时:"), row, 0, Qt::AlignVCenter | Qt::AlignRight);
        m_gridLayout->addWidget(m_timeoutSpin, row++, 1);

        m_gridLayout->addWidget(new QLabel(""), row, 0); // 空标签占位
        m_gridLayout->addWidget(m_sslCheck, row++, 1);
    } else {
        // 其他数据库类型的通用表单
        m_gridLayout->addWidget(createLabel("用户名:"), row, 0, Qt::AlignVCenter | Qt::AlignRight);
        m_gridLayout->addWidget(m_usernameEdit, row++, 1);

        m_gridLayout->addWidget(createLabel("密码:"), row, 0, Qt::AlignVCenter | Qt::AlignRight);
        m_gridLayout->addWidget(m_passwordEdit, row++, 1);

        m_gridLayout->addWidget(createLabel("数据库:"), row, 0, Qt::AlignVCenter | Qt::AlignRight);
        m_gridLayout->addWidget(m_databaseEdit, row++, 1);

        m_gridLayout->addWidget(createLabel("超时:"), row, 0, Qt::AlignVCenter | Qt::AlignRight);
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
    m_statusLabel->setText(tr("正在测试连接..."));

    Connx::ConnectionConfig config = getConnectionConfig();
    Connx::ConnectionType type = Connx::ConnectionFactory::getTypeFromString(m_currentType);

    Connx::Connection* testConn = Connx::ConnectionFactory::createConnection(type, config);
    if (!testConn) {
        m_statusLabel->setText(tr("❌ 不支持的数据库类型"));
        m_testBtn->setEnabled(true);
        return;
    }

    bool success = testConn->connectToServer();

    if (success) {
        m_statusLabel->setText(tr("✅ 连接成功"));
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
        m_statusLabel->setText(tr("❌ 请输入连接名称"));
        m_nameEdit->setFocus();
        return false;
    }

    if (m_hostEdit->text().trimmed().isEmpty()) {
        m_statusLabel->setText(tr("❌ 请输入主机地址"));
        m_hostEdit->setFocus();
        return false;
    }

    // 清除错误信息
    m_statusLabel->setText("");
    return true;
}
