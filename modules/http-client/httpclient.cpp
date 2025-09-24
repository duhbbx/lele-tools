#include "httpclient.h"
#include <algorithm>
#include <functional>

REGISTER_DYNAMICOBJECT(HttpClient);

HttpClient::HttpClient() : QWidget(nullptr), DynamicObjectBase()
{
    m_networkManager = new QNetworkAccessManager(this);
    m_currentReply = nullptr;
    m_requestTimer = new QTimer(this);
    m_requestTimer->setSingleShot(true);
    // m_elapsedTimer 是栈对象，无需new

    setupUI();

    // 连接信号
    connect(m_requestTimer, &QTimer::timeout, this, &HttpClient::onRequestFinished);
}

HttpClient::~HttpClient()
{
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
    }
}

void HttpClient::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    m_mainLayout->setSpacing(10);

    setupToolbar();

    // 创建主分割器
    m_mainSplitter = new QSplitter(Qt::Vertical, this);
    m_mainSplitter->setChildrenCollapsible(false);

    setupRequestArea();
    setupResponseArea();

    m_mainSplitter->addWidget(m_requestWidget);
    m_mainSplitter->addWidget(m_responseWidget);
    m_mainSplitter->setSizes({400, 300});

    m_mainLayout->addWidget(m_mainSplitter);

    // 应用样式
    setStyleSheet(
        "QWidget {"
        "    font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;"
        "    font-size: 12px;"
        "}"
        "QGroupBox {"
        "    font-weight: bold;"
        "    border: 2px solid #cccccc;"
        "    border-radius: 5px;"
        "    margin-top: 10px;"
        "    padding-top: 5px;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 5px 0 5px;"
        "}"
        "QPushButton {"
        "    background-color: #f8f9fa;"
        "    border: 1px solid #dee2e6;"
        "    border-radius: 4px;"
        "    padding: 6px 12px;"
        "    font-weight: normal;"
        "}"
        "QPushButton:hover {"
        "    background-color: #e9ecef;"
        "    border-color: #adb5bd;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #dee2e6;"
        "}"
        "QLineEdit, QTextEdit, QComboBox {"
        "    border: 1px solid #ced4da;"
        "    padding: 6px;"
        "    background-color: #ffffff;"
        "    border-radius: 4px;"
        "}"
        "QLineEdit:focus, QTextEdit:focus, QComboBox:focus {"
        "    border-color: #80bdff;"
        "    outline: 0;"
        "}"
        "QTableWidget {"
        "    gridline-color: #dee2e6;"
        "    background-color: #ffffff;"
        "    alternate-background-color: #f8f9fa;"
        "}"
        "QTableWidget::item {"
        "    padding: 5px;"
        "    border-bottom: 1px solid #dee2e6;"
        "}"
        "QTableWidget::item:selected {"
        "    background-color: #007bff;"
        "    color: white;"
        "}"
        "QTabWidget::pane {"
        "    border: 1px solid #c0c0c0;"
        "    background-color: #ffffff;"
        "}"
        "QTabBar::tab {"
        "    background-color: #f0f0f0;"
        "    color: #333333;"
        "    padding: 8px 16px;"
        "    margin-right: 2px;"
        "    border: 1px solid #c0c0c0;"
        "    border-bottom: none;"
        "    border-top-left-radius: 4px;"
        "    border-top-right-radius: 4px;"
        "}"
        "QTabBar::tab:selected {"
        "    background-color: #ffffff;"
        "    border-bottom: 1px solid #ffffff;"
        "    font-weight: bold;"
        "}"
        "QTabBar::tab:hover {"
        "    background-color: #e0e0e0;"
        "}"
    );
}

void HttpClient::setupToolbar()
{
    // cURL输入区域 - 固定整体高度
    m_curlGroup = new QGroupBox(tr("cURL 命令解析"), this);
    m_curlGroup->setFixedHeight(140); // 固定Group高度
    QVBoxLayout* curlLayout = new QVBoxLayout(m_curlGroup);

    m_curlInput = new QTextEdit();
    m_curlInput->setFixedHeight(80); // 固定高度，避免滚动条
    m_curlInput->setPlaceholderText(tr("在此粘贴 cURL 命令，例如:\ncurl -X POST \"https://api.example.com/data\" -H \"Content-Type: application/json\" -d '{\"key\":\"value\"}'"));
    m_curlInput->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // 禁用垂直滚动条
    m_curlInput->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded); // 水平滚动条按需显示
    curlLayout->addWidget(m_curlInput);

    // 按钮容器，高度根据内容自适应
    QWidget* buttonWidget = new QWidget();
    buttonWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QHBoxLayout* curlButtonLayout = new QHBoxLayout(buttonWidget);
    curlButtonLayout->setContentsMargins(0, 2, 0, 2); // 减少边距

    m_parseCurlBtn = new QPushButton(tr("解析 cURL"));
    m_parseCurlBtn->setStyleSheet("QPushButton { background-color: #007bff; color: white; font-weight: bold; }");
    m_parseCurlBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed); // 高度根据内容

    m_clearAllBtn = new QPushButton(tr("清空全部"));
    m_clearAllBtn->setStyleSheet("QPushButton { background-color: #dc3545; color: white; font-weight: bold; }");
    m_clearAllBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed); // 高度根据内容

    curlButtonLayout->addWidget(m_parseCurlBtn);
    curlButtonLayout->addWidget(m_clearAllBtn);
    curlButtonLayout->addStretch();

    curlLayout->addWidget(buttonWidget);
    m_mainLayout->addWidget(m_curlGroup);

    // 连接信号
    connect(m_curlInput, &QTextEdit::textChanged, this, &HttpClient::onCurlInputChanged);
    connect(m_parseCurlBtn, &QPushButton::clicked, this, &HttpClient::onParseCurlClicked);
    connect(m_clearAllBtn, &QPushButton::clicked, this, &HttpClient::onClearAllClicked);
}

void HttpClient::setupRequestArea()
{
    m_requestWidget = new QWidget();
    m_requestGroup = new QGroupBox(tr("HTTP 请求"), m_requestWidget);
    QVBoxLayout* requestLayout = new QVBoxLayout(m_requestWidget);
    requestLayout->addWidget(m_requestGroup);

    QVBoxLayout* groupLayout = new QVBoxLayout(m_requestGroup);

    // 方法、协议、主机、端口在一行
    QHBoxLayout* topLayout = new QHBoxLayout();

    // 方法
    topLayout->addWidget(new QLabel(tr("方法:")));
    m_methodCombo = new QComboBox();
    m_methodCombo->addItems({"GET", "POST", "PUT", "DELETE", "PATCH", "HEAD", "OPTIONS"});
    m_methodCombo->setMinimumWidth(80);
    m_methodCombo->setMaximumWidth(80);
    topLayout->addWidget(m_methodCombo);

    topLayout->addSpacing(10); // 添加间距

    // 协议
    topLayout->addWidget(new QLabel(tr("协议:")));
    m_protocolCombo = new QComboBox();
    m_protocolCombo->addItems({"https", "http"});
    m_protocolCombo->setMinimumWidth(70);
    m_protocolCombo->setMaximumWidth(70);
    topLayout->addWidget(m_protocolCombo);

    topLayout->addSpacing(10); // 添加间距

    // 端口
    topLayout->addWidget(new QLabel(tr("端口:")));
    m_portSpin = new QSpinBox();
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(443);
    m_portSpin->setMinimumWidth(70);
    m_portSpin->setMaximumWidth(70);
    topLayout->addWidget(m_portSpin);

    topLayout->addSpacing(10); // 添加间距

    // 主机
    topLayout->addWidget(new QLabel(tr("主机:")));
    m_hostEdit = new QLineEdit();
    m_hostEdit->setPlaceholderText(tr("例如: api.example.com"));
    m_hostEdit->setMinimumWidth(200);
    topLayout->addWidget(m_hostEdit, 1); // 拉伸因子为1，占据剩余空间

    groupLayout->addLayout(topLayout);

    // 路径单独一行
    QHBoxLayout* pathLayout = new QHBoxLayout();
    pathLayout->addWidget(new QLabel(tr("路径:")));
    m_pathEdit = new QLineEdit();
    m_pathEdit->setPlaceholderText(tr("例如: /api/v1/users"));
    m_pathEdit->setText("/");
    pathLayout->addWidget(m_pathEdit);

    groupLayout->addLayout(pathLayout);

    // 按钮行
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_sendBtn = new QPushButton(tr("发送请求"));
    m_sendBtn->setStyleSheet("QPushButton { background-color: #28a745; color: white; font-weight: bold; }");
    m_sendBtn->setMinimumWidth(100);
    buttonLayout->addWidget(m_sendBtn);

    m_cancelBtn = new QPushButton(tr("取消"));
    m_cancelBtn->setStyleSheet("QPushButton { background-color: #6c757d; color: white; }");
    m_cancelBtn->setMinimumWidth(80);
    m_cancelBtn->setEnabled(false);
    buttonLayout->addWidget(m_cancelBtn);
    buttonLayout->addStretch();

    groupLayout->addLayout(buttonLayout);

    // 进度条
    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    groupLayout->addWidget(m_progressBar);

    // 请求详情标签页
    m_requestTabs = new QTabWidget();
    setupParametersTab();
    setupHeadersTab();
    setupCookiesTab();
    setupBodyTab();

    groupLayout->addWidget(m_requestTabs);

    // 连接信号
    connect(m_sendBtn, &QPushButton::clicked, this, &HttpClient::onSendRequestClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &HttpClient::onCancelRequestClicked);

    // URL组件信号连接
    connect(m_protocolCombo, &QComboBox::currentTextChanged, this, &HttpClient::onProtocolChanged);
    connect(m_hostEdit, &QLineEdit::textChanged, this, &HttpClient::onHostChanged);
    connect(m_portSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &HttpClient::onPortChanged);
    connect(m_pathEdit, &QLineEdit::textChanged, this, &HttpClient::onPathChanged);
}

void HttpClient::setupParametersTab()
{
    m_paramsTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(m_paramsTab);

    // 按钮行
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_addParamBtn = new QPushButton(tr("添加参数"));
    m_removeParamBtn = new QPushButton(tr("删除参数"));

    buttonLayout->addWidget(m_addParamBtn);
    buttonLayout->addWidget(m_removeParamBtn);
    buttonLayout->addStretch();

    layout->addLayout(buttonLayout);

    // 参数表格
    m_paramsTable = new QTableWidget(0, 3);
    m_paramsTable->setHorizontalHeaderLabels({tr("键"), tr("值"), tr("启用")});
    m_paramsTable->horizontalHeader()->setStretchLastSection(false);
    m_paramsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_paramsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_paramsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_paramsTable->setColumnWidth(0, 150);
    m_paramsTable->setColumnWidth(2, 60);
    m_paramsTable->setAlternatingRowColors(true);
    m_paramsTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    layout->addWidget(m_paramsTable);

    m_requestTabs->addTab(m_paramsTab, tr("参数"));

    // 连接信号
    connect(m_addParamBtn, &QPushButton::clicked, this, &HttpClient::onAddParameterClicked);
    connect(m_removeParamBtn, &QPushButton::clicked, this, &HttpClient::onRemoveParameterClicked);
    connect(m_paramsTable, &QTableWidget::cellChanged, this, &HttpClient::onParameterChanged);
}

void HttpClient::setupHeadersTab()
{
    m_headersTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(m_headersTab);

    // 按钮行
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_addHeaderBtn = new QPushButton(tr("添加头部"));
    m_removeHeaderBtn = new QPushButton(tr("删除头部"));

    buttonLayout->addWidget(m_addHeaderBtn);
    buttonLayout->addWidget(m_removeHeaderBtn);
    buttonLayout->addStretch();

    layout->addLayout(buttonLayout);

    // 头部表格
    m_headersTable = new QTableWidget(0, 3);
    m_headersTable->setHorizontalHeaderLabels({tr("键"), tr("值"), tr("启用")});
    m_headersTable->horizontalHeader()->setStretchLastSection(false);
    m_headersTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_headersTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_headersTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_headersTable->setColumnWidth(0, 150);
    m_headersTable->setColumnWidth(2, 60);
    m_headersTable->setAlternatingRowColors(true);
    m_headersTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    layout->addWidget(m_headersTable);

    m_requestTabs->addTab(m_headersTab, tr("请求头"));

    // 连接信号
    connect(m_addHeaderBtn, &QPushButton::clicked, this, &HttpClient::onAddHeaderClicked);
    connect(m_removeHeaderBtn, &QPushButton::clicked, this, &HttpClient::onRemoveHeaderClicked);
    connect(m_headersTable, &QTableWidget::cellChanged, this, &HttpClient::onHeaderChanged);
}

void HttpClient::setupCookiesTab()
{
    m_cookiesTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(m_cookiesTab);

    // 按钮行
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_addCookieBtn = new QPushButton(tr("添加 Cookie"));
    m_removeCookieBtn = new QPushButton(tr("删除 Cookie"));

    buttonLayout->addWidget(m_addCookieBtn);
    buttonLayout->addWidget(m_removeCookieBtn);
    buttonLayout->addStretch();

    layout->addLayout(buttonLayout);

    // Cookie表格
    m_cookiesTable = new QTableWidget(0, 3);
    m_cookiesTable->setHorizontalHeaderLabels({tr("键"), tr("值"), tr("启用")});
    m_cookiesTable->horizontalHeader()->setStretchLastSection(false);
    m_cookiesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_cookiesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_cookiesTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_cookiesTable->setColumnWidth(0, 150);
    m_cookiesTable->setColumnWidth(2, 60);
    m_cookiesTable->setAlternatingRowColors(true);
    m_cookiesTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    layout->addWidget(m_cookiesTable);

    m_requestTabs->addTab(m_cookiesTab, tr("Cookies"));

    // 连接信号
    connect(m_addCookieBtn, &QPushButton::clicked, this, &HttpClient::onAddCookieClicked);
    connect(m_removeCookieBtn, &QPushButton::clicked, this, &HttpClient::onRemoveCookieClicked);
    connect(m_cookiesTable, &QTableWidget::cellChanged, this, &HttpClient::onCookieChanged);
}

void HttpClient::setupBodyTab()
{
    m_bodyTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(m_bodyTab);

    // 请求体类型选择和格式化按钮在一行
    QHBoxLayout* typeLayout = new QHBoxLayout();
    typeLayout->addWidget(new QLabel(tr("请求体类型:")));

    m_bodyTypeCombo = new QComboBox();
    m_bodyTypeCombo->addItems({
        tr("无请求体"),
        tr("原始文本 (raw)"),
        tr("JSON"),
        tr("XML"),
        tr("表单数据 (form-data)"),
        tr("URL编码 (x-www-form-urlencoded)")
    });
    typeLayout->addWidget(m_bodyTypeCombo);

    // 格式化按钮紧挨着下拉框
    m_formatBodyBtn = new QPushButton(tr("格式化"));
    m_formatBodyBtn->setStyleSheet("QPushButton { background-color: #17a2b8; color: white; font-weight: bold; }");
    m_formatBodyBtn->setMaximumWidth(80);
    m_formatBodyBtn->setMinimumHeight(28); // 调整高度匹配下拉框
    typeLayout->addWidget(m_formatBodyBtn);

    typeLayout->addStretch(); // 推到左边

    layout->addLayout(typeLayout);

    // 请求体内容区域（简化布局）
    m_bodyEdit = new QTextEdit();
    m_bodyEdit->setPlaceholderText(tr("在此输入请求体内容..."));
    layout->addWidget(m_bodyEdit);

    m_requestTabs->addTab(m_bodyTab, tr("请求体"));

    // 连接格式化按钮信号
    connect(m_formatBodyBtn, &QPushButton::clicked, this, &HttpClient::onFormatBodyClicked);
    connect(m_bodyTypeCombo, &QComboBox::currentTextChanged, [this]() {
        // 根据请求体类型显示/隐藏格式化按钮
        QString bodyType = m_bodyTypeCombo->currentText();
        bool shouldShow = bodyType.contains("JSON", Qt::CaseInsensitive) || bodyType.contains("XML", Qt::CaseInsensitive);
        m_formatBodyBtn->setVisible(shouldShow);
    });
}

void HttpClient::setupResponseArea()
{
    m_responseWidget = new QWidget();
    m_responseGroup = new QGroupBox(tr("HTTP 响应"), m_responseWidget);
    QVBoxLayout* responseLayout = new QVBoxLayout(m_responseWidget);
    responseLayout->addWidget(m_responseGroup);

    QVBoxLayout* groupLayout = new QVBoxLayout(m_responseGroup);

    // 响应标签页
    m_responseTabs = new QTabWidget();

    // 创建标签页右侧的控件容器
    QWidget* cornerWidget = new QWidget();
    QHBoxLayout* cornerLayout = new QHBoxLayout(cornerWidget);
    cornerLayout->setContentsMargins(5, 2, 5, 2); // 减少边距
    cornerLayout->setSpacing(8);

    // 状态标签（紧凑版）
    m_statusLabel = new QLabel(tr("就绪"));
    m_statusLabel->setStyleSheet("font-weight: bold; color: #6c757d; font-size: 11px;");
    m_timeLabel = new QLabel(tr("--"));
    m_timeLabel->setStyleSheet("color: #666; font-size: 10px;");
    m_sizeLabel = new QLabel(tr("--"));
    m_sizeLabel->setStyleSheet("color: #666; font-size: 10px;");

    // 操作按钮（紧凑版）
    m_copyResponseBtn = new QPushButton(tr("复制"));
    m_copyResponseBtn->setMaximumHeight(24);
    m_copyResponseBtn->setStyleSheet("QPushButton { padding: 2px 6px; font-size: 10px; }");

    m_formatResponseBtn = new QPushButton(tr("格式化"));
    m_formatResponseBtn->setMaximumHeight(24);
    m_formatResponseBtn->setStyleSheet("QPushButton { padding: 2px 6px; font-size: 10px; }");

    m_saveResponseBtn = new QPushButton(tr("保存"));
    m_saveResponseBtn->setMaximumHeight(24);
    m_saveResponseBtn->setStyleSheet("QPushButton { padding: 2px 6px; font-size: 10px; }");

    // 添加到布局
    cornerLayout->addWidget(m_statusLabel);
    cornerLayout->addWidget(new QLabel("|")); // 分隔符
    cornerLayout->addWidget(m_timeLabel);
    cornerLayout->addWidget(m_sizeLabel);
    cornerLayout->addWidget(new QLabel("|")); // 分隔符
    cornerLayout->addWidget(m_copyResponseBtn);
    cornerLayout->addWidget(m_formatResponseBtn);
    cornerLayout->addWidget(m_saveResponseBtn);

    cornerWidget->setMaximumHeight(30); // 控制整体高度

    // 设置到标签页右上角
    m_responseTabs->setCornerWidget(cornerWidget, Qt::TopRightCorner);

    // 响应体标签页
    m_responseBodyEdit = new QTextEdit();
    m_responseBodyEdit->setReadOnly(true);
    m_responseBodyEdit->setPlaceholderText(tr("响应内容将显示在这里..."));
    m_responseTabs->addTab(m_responseBodyEdit, tr("响应体"));

    // 原始响应标签页
    m_rawResponseEdit = new QTextEdit();
    m_rawResponseEdit->setReadOnly(true);
    m_rawResponseEdit->setPlaceholderText(tr("原始响应数据将显示在这里..."));
    m_rawResponseEdit->setFont(QFont("Consolas", 10));
    m_responseTabs->addTab(m_rawResponseEdit, tr("原始响应"));

    groupLayout->addWidget(m_responseTabs);

    // 连接信号
    connect(m_copyResponseBtn, &QPushButton::clicked, this, &HttpClient::onCopyResponseClicked);
    connect(m_formatResponseBtn, &QPushButton::clicked, this, &HttpClient::onFormatResponseClicked);
    connect(m_saveResponseBtn, &QPushButton::clicked, this, &HttpClient::onSaveResponseClicked);
}

// 槽函数实现
void HttpClient::onCurlInputChanged()
{
    QString curlText = m_curlInput->toPlainText().trimmed();
    m_parseCurlBtn->setEnabled(!curlText.isEmpty());
}

void HttpClient::onParseCurlClicked()
{
    QString curlCommand = m_curlInput->toPlainText().trimmed();
    if (curlCommand.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请输入 cURL 命令"));
        return;
    }

    if (parseCurlCommand(curlCommand)) {
        QMessageBox::information(this, tr("成功"), tr("cURL 命令解析成功！"));
    } else {
        QMessageBox::warning(this, tr("错误"), tr("cURL 命令解析失败，请检查格式"));
    }
}

void HttpClient::onClearAllClicked()
{
    int ret = QMessageBox::question(this, tr("确认"), tr("确定要清空所有设置吗？"),
                                   QMessageBox::Yes | QMessageBox::No);
    if (ret == QMessageBox::Yes) {
        m_curlInput->clear();
        m_protocolCombo->setCurrentIndex(0);
        m_hostEdit->clear();
        m_portSpin->setValue(443);
        m_pathEdit->setText("/");
        m_methodCombo->setCurrentIndex(0);
        m_bodyEdit->clear();
        m_bodyTypeCombo->setCurrentIndex(0);

        m_paramsTable->setRowCount(0);
        m_headersTable->setRowCount(0);
        m_cookiesTable->setRowCount(0);

        m_responseBodyEdit->clear();
        m_rawResponseEdit->clear();
        m_statusLabel->setText(tr("状态: 准备就绪"));
        m_timeLabel->setText(tr("时间: --"));
        m_sizeLabel->setText(tr("大小: --"));

        m_requestInfo = HttpRequestInfo();
        m_responseInfo = HttpResponseInfo();
    }
}

void HttpClient::onSendRequestClicked()
{
    QString host = m_hostEdit->text().trimmed();
    if (host.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请输入主机地址"));
        return;
    }

    buildRequest();
    sendHttpRequest();
}

void HttpClient::onCancelRequestClicked()
{
    if (m_currentReply) {
        m_currentReply->abort();
    }
    m_requestTimer->stop();

    m_sendBtn->setEnabled(true);
    m_cancelBtn->setEnabled(false);
    m_progressBar->setVisible(false);
    m_statusLabel->setText(tr("状态: 已取消"));
}

// 参数管理
void HttpClient::onAddParameterClicked()
{
    int row = m_paramsTable->rowCount();
    m_paramsTable->insertRow(row);

    m_paramsTable->setItem(row, 0, new QTableWidgetItem(""));
    m_paramsTable->setItem(row, 1, new QTableWidgetItem(""));

    QCheckBox* checkBox = new QCheckBox();
    checkBox->setChecked(true);
    m_paramsTable->setCellWidget(row, 2, checkBox);
}

void HttpClient::onRemoveParameterClicked()
{
    removeSelectedTableRows(m_paramsTable);
}

void HttpClient::onParameterChanged()
{
    // 实时更新请求信息
    m_requestInfo.parameters = getTableData(m_paramsTable);
}

// 头部管理
void HttpClient::onAddHeaderClicked()
{
    int row = m_headersTable->rowCount();
    m_headersTable->insertRow(row);

    m_headersTable->setItem(row, 0, new QTableWidgetItem(""));
    m_headersTable->setItem(row, 1, new QTableWidgetItem(""));

    QCheckBox* checkBox = new QCheckBox();
    checkBox->setChecked(true);
    m_headersTable->setCellWidget(row, 2, checkBox);
}

void HttpClient::onRemoveHeaderClicked()
{
    removeSelectedTableRows(m_headersTable);
}

void HttpClient::onHeaderChanged()
{
    m_requestInfo.headers = getTableData(m_headersTable);
}

// Cookie管理
void HttpClient::onAddCookieClicked()
{
    int row = m_cookiesTable->rowCount();
    m_cookiesTable->insertRow(row);

    m_cookiesTable->setItem(row, 0, new QTableWidgetItem(""));
    m_cookiesTable->setItem(row, 1, new QTableWidgetItem(""));

    QCheckBox* checkBox = new QCheckBox();
    checkBox->setChecked(true);
    m_cookiesTable->setCellWidget(row, 2, checkBox);
}

void HttpClient::onRemoveCookieClicked()
{
    removeSelectedTableRows(m_cookiesTable);
}

void HttpClient::onCookieChanged()
{
    m_requestInfo.cookies = getTableData(m_cookiesTable);
}

// 响应处理
void HttpClient::onRequestFinished()
{
    if (!m_currentReply) return;

    processResponse(m_currentReply);
    displayResponse();

    m_currentReply->deleteLater();
    m_currentReply = nullptr;

    m_sendBtn->setEnabled(true);
    m_cancelBtn->setEnabled(false);
    m_progressBar->setVisible(false);
}

void HttpClient::onRequestError(QNetworkReply::NetworkError error)
{
    Q_UNUSED(error);

    if (m_currentReply) {
        QString errorString = m_currentReply->errorString();
        m_statusLabel->setText(tr("状态: 错误 - %1").arg(errorString));
        m_responseBodyEdit->setText(tr("请求错误: %1").arg(errorString));
    }

    m_sendBtn->setEnabled(true);
    m_cancelBtn->setEnabled(false);
    m_progressBar->setVisible(false);
}

void HttpClient::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0) {
        int progress = static_cast<int>((bytesReceived * 100) / bytesTotal);
        m_progressBar->setValue(progress);
    }
}

// 实用功能
void HttpClient::onCopyResponseClicked()
{
    QString response = m_responseBodyEdit->toPlainText();
    if (response.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("没有响应数据可复制"));
        return;
    }

    QApplication::clipboard()->setText(response);
    QMessageBox::information(this, tr("成功"), tr("响应已复制到剪贴板"));
}

void HttpClient::onFormatResponseClicked()
{
    QString response = m_responseBodyEdit->toPlainText();
    if (response.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("没有响应数据可格式化"));
        return;
    }

    QString formatted;
    if (m_responseInfo.contentType.contains("json", Qt::CaseInsensitive)) {
        formatted = formatJsonResponse(response);
    } else if (m_responseInfo.contentType.contains("xml", Qt::CaseInsensitive)) {
        formatted = formatXmlResponse(response);
    } else {
        formatted = response;
    }

    m_responseBodyEdit->setText(formatted);
}

void HttpClient::onSaveResponseClicked()
{
    // 保存响应到文件
    QMessageBox::information(this, tr("提示"), tr("保存功能开发中..."));
}

// 工具函数实现
QString HttpClient::httpMethodToString(HttpMethod method)
{
    switch (method) {
        case HttpMethod::GET: return "GET";
        case HttpMethod::POST: return "POST";
        case HttpMethod::PUT: return "PUT";
        case HttpMethod::HTTP_DELETE: return "DELETE";
        case HttpMethod::PATCH: return "PATCH";
        case HttpMethod::HEAD: return "HEAD";
        case HttpMethod::OPTIONS: return "OPTIONS";
        default: return "GET";
    }
}

HttpMethod HttpClient::stringToHttpMethod(const QString& method)
{
    QString upper = method.toUpper();
    if (upper == "POST") return HttpMethod::POST;
    if (upper == "PUT") return HttpMethod::PUT;
    if (upper == "DELETE") return HttpMethod::HTTP_DELETE;
    if (upper == "PATCH") return HttpMethod::PATCH;
    if (upper == "HEAD") return HttpMethod::HEAD;
    if (upper == "OPTIONS") return HttpMethod::OPTIONS;
    return HttpMethod::GET;
}

QString HttpClient::formatFileSize(qint64 bytes)
{
    const qint64 KB = 1024;
    const qint64 MB = KB * 1024;
    const qint64 GB = MB * 1024;

    if (bytes >= GB) {
        return QString::number(bytes / GB, 'f', 2) + " GB";
    } else if (bytes >= MB) {
        return QString::number(bytes / MB, 'f', 2) + " MB";
    } else if (bytes >= KB) {
        return QString::number(bytes / KB, 'f', 2) + " KB";
    } else {
        return QString::number(bytes) + " B";
    }
}

QString HttpClient::formatTime(qint64 milliseconds)
{
    if (milliseconds < 1000) {
        return QString::number(milliseconds) + " ms";
    } else {
        return QString::number(milliseconds / 1000.0, 'f', 2) + " s";
    }
}

bool HttpClient::parseCurlCommand(const QString& curlCommand)
{
    try {
        QString cleanCommand = curlCommand.trimmed();

        // 移除开头的 curl 命令
        if (cleanCommand.startsWith("curl", Qt::CaseInsensitive)) {
            cleanCommand = cleanCommand.mid(4).trimmed();
        }

        // 解析 URL
        QString url = extractUrl(cleanCommand);
        if (!url.isEmpty()) {
            QUrl parsedUrl(url);
            if (parsedUrl.isValid()) {
                m_protocolCombo->setCurrentText(parsedUrl.scheme());
                m_hostEdit->setText(parsedUrl.host());
                int port = parsedUrl.port();
                if (port == -1) {
                    port = (parsedUrl.scheme() == "https") ? 443 : 80;
                }
                m_portSpin->setValue(port);
                m_pathEdit->setText(parsedUrl.path().isEmpty() ? "/" : parsedUrl.path());
            }
        }

        // 解析方法
        HttpMethod method = extractMethod(cleanCommand);
        m_methodCombo->setCurrentText(httpMethodToString(method));
        m_requestInfo.method = method;

        // 解析头部
        QList<KeyValuePair> headers = extractHeaders(cleanCommand);
        if (!headers.isEmpty()) {
            m_headersTable->setRowCount(0);
            for (const auto& header : headers) {
                addTableRow(m_headersTable, header);
            }
            m_requestInfo.headers = headers;
        }

        // 解析请求体
        QString body = extractBody(cleanCommand);
        if (!body.isEmpty()) {
            m_bodyEdit->setText(body);
            m_requestInfo.body = body;

            // 根据Content-Type设置body类型
            for (const auto& header : headers) {
                if (header.key.toLower() == "content-type") {
                    if (header.value.contains("application/json")) {
                        m_bodyTypeCombo->setCurrentText(tr("JSON"));
                    } else if (header.value.contains("application/xml") || header.value.contains("text/xml")) {
                        m_bodyTypeCombo->setCurrentText(tr("XML"));
                    } else if (header.value.contains("application/x-www-form-urlencoded")) {
                        m_bodyTypeCombo->setCurrentText(tr("URL编码 (x-www-form-urlencoded)"));
                    }
                    break;
                }
            }
        }

        // 解析 Cookies
        QList<KeyValuePair> cookies = extractCookies(cleanCommand);
        if (!cookies.isEmpty()) {
            m_cookiesTable->setRowCount(0);
            for (const auto& cookie : cookies) {
                addTableRow(m_cookiesTable, cookie);
            }
            m_requestInfo.cookies = cookies;
        }

        return true;
    } catch (...) {
        return false;
    }
}

QString HttpClient::extractUrl(const QString& curlCommand)
{
    // 使用正则表达式匹配URL
    QRegularExpression urlRegex("(?:\\\"([^\\\"]+)\\\"|'([^']+)'|([^\\s]+))");
    QRegularExpressionMatchIterator i = urlRegex.globalMatch(curlCommand);

    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        QString url = match.captured(1).isEmpty() ?
                     (match.captured(2).isEmpty() ? match.captured(3) : match.captured(2)) :
                     match.captured(1);

        // 检查是否是HTTP URL
        if (url.startsWith("http://") || url.startsWith("https://")) {
            return url;
        }
    }

    return QString();
}

HttpMethod HttpClient::extractMethod(const QString& curlCommand)
{
    // 查找 -X 或 --request 参数
    QRegularExpression methodRegex("(?:-X|--request)\\s+([A-Z]+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = methodRegex.match(curlCommand);

    if (match.hasMatch()) {
        return stringToHttpMethod(match.captured(1));
    }

    // 如果有 -d, --data, --data-raw 参数，默认为 POST
    if (curlCommand.contains(QRegularExpression("(-d|--data|--data-raw)\\s"))) {
        return HttpMethod::POST;
    }

    return HttpMethod::GET;
}

QList<KeyValuePair> HttpClient::extractHeaders(const QString& curlCommand)
{
    QList<KeyValuePair> headers;

    // 查找 -H 或 --header 参数
    QRegularExpression headerRegex("(?:-H|--header)\\s+(?:\\\"([^\\\"]+)\\\"|'([^']+)'|([^\\s]+))");
    QRegularExpressionMatchIterator i = headerRegex.globalMatch(curlCommand);

    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        QString headerStr = match.captured(1).isEmpty() ?
                           (match.captured(2).isEmpty() ? match.captured(3) : match.captured(2)) :
                           match.captured(1);

        // 解析 "Key: Value" 格式
        int colonIndex = headerStr.indexOf(':');
        if (colonIndex > 0) {
            QString key = headerStr.left(colonIndex).trimmed();
            QString value = headerStr.mid(colonIndex + 1).trimmed();
            headers.append(KeyValuePair(key, value, true));
        }
    }

    return headers;
}

QString HttpClient::extractBody(const QString& curlCommand)
{
    // 查找 -d, --data, --data-raw 参数
    QRegularExpression bodyRegex("(?:-d|--data|--data-raw)\\s+(?:\\\"([^\\\"]+)\\\"|'([^']+)'|([^\\s]+))");
    QRegularExpressionMatch match = bodyRegex.match(curlCommand);

    if (match.hasMatch()) {
        QString body = match.captured(1).isEmpty() ?
                      (match.captured(2).isEmpty() ? match.captured(3) : match.captured(2)) :
                      match.captured(1);
        return body;
    }

    return QString();
}

QList<KeyValuePair> HttpClient::extractCookies(const QString& curlCommand)
{
    QList<KeyValuePair> cookies;

    // 查找 -b 或 --cookie 参数
    QRegularExpression cookieRegex("(?:-b|--cookie)\\s+(?:\\\"([^\\\"]+)\\\"|'([^']+)'|([^\\s]+))");
    QRegularExpressionMatch match = cookieRegex.match(curlCommand);

    if (match.hasMatch()) {
        QString cookieStr = match.captured(1).isEmpty() ?
                           (match.captured(2).isEmpty() ? match.captured(3) : match.captured(2)) :
                           match.captured(1);

        // 解析 "key1=value1; key2=value2" 格式
        QStringList cookiePairs = cookieStr.split(';');
        for (const QString& pair : cookiePairs) {
            int equalIndex = pair.indexOf('=');
            if (equalIndex > 0) {
                QString key = pair.left(equalIndex).trimmed();
                QString value = pair.mid(equalIndex + 1).trimmed();
                cookies.append(KeyValuePair(key, value, true));
            }
        }
    }

    return cookies;
}

void HttpClient::buildRequest()
{
    m_requestInfo.method = stringToHttpMethod(m_methodCombo->currentText());
    m_requestInfo.body = m_bodyEdit->toPlainText();
    m_requestInfo.parameters = getTableData(m_paramsTable);
    m_requestInfo.headers = getTableData(m_headersTable);
    m_requestInfo.cookies = getTableData(m_cookiesTable);

    // 从分离的组件获取URL信息
    m_requestInfo.protocol = m_protocolCombo->currentText();
    m_requestInfo.host = m_hostEdit->text().trimmed();
    m_requestInfo.port = m_portSpin->value();
    m_requestInfo.path = m_pathEdit->text().trimmed();
}

QUrl HttpClient::buildUrl() const
{
    QString protocol = m_protocolCombo->currentText();
    QString host = m_hostEdit->text().trimmed();
    int port = m_portSpin->value();
    QString path = m_pathEdit->text().trimmed();

    if (host.isEmpty()) {
        return QUrl();
    }

    if (path.isEmpty() || !path.startsWith("/")) {
        path = "/" + path;
    }

    // 构建基础URL
    QString urlString = QString("%1://%2:%3%4").arg(protocol).arg(host).arg(port).arg(path);
    QUrl url(urlString);

    if (!url.isValid()) {
        return QUrl();
    }

    // 添加查询参数
    QUrlQuery query;
    for (const auto& param : m_requestInfo.parameters) {
        if (param.enabled && !param.key.isEmpty()) {
            query.addQueryItem(param.key, param.value);
        }
    }

    if (!query.isEmpty()) {
        url.setQuery(query);
    }

    return url;
}

void HttpClient::setRequestHeaders(QNetworkRequest& request)
{
    // 设置请求头
    for (const auto& header : m_requestInfo.headers) {
        if (header.enabled && !header.key.isEmpty()) {
            request.setRawHeader(header.key.toUtf8(), header.value.toUtf8());
        }
    }

    // 设置 Cookies
    if (!m_requestInfo.cookies.isEmpty()) {
        QStringList cookieStrings;
        for (const auto& cookie : m_requestInfo.cookies) {
            if (cookie.enabled && !cookie.key.isEmpty()) {
                cookieStrings.append(QString("%1=%2").arg(cookie.key, cookie.value));
            }
        }
        if (!cookieStrings.isEmpty()) {
            request.setRawHeader("Cookie", cookieStrings.join("; ").toUtf8());
        }
    }
}

QByteArray HttpClient::buildRequestBody()
{
    QString bodyType = m_bodyTypeCombo->currentText();
    QString body = m_bodyEdit->toPlainText();

    if (bodyType == tr("无请求体") || body.isEmpty()) {
        return QByteArray();
    }

    return body.toUtf8();
}

void HttpClient::sendHttpRequest()
{
    QUrl url = buildUrl();
    if (!url.isValid()) {
        QMessageBox::warning(this, tr("错误"), tr("无效的 URL"));
        return;
    }

    QNetworkRequest request(url);
    setRequestHeaders(request);

    // 设置User-Agent
    request.setRawHeader("User-Agent", "HTTP-Client-Tool/1.0");

    // 获取请求体
    QByteArray requestBody = buildRequestBody();

    // 发送请求
    QString method = m_methodCombo->currentText();
    if (method == "GET") {
        m_currentReply = m_networkManager->get(request);
    } else if (method == "POST") {
        m_currentReply = m_networkManager->post(request, requestBody);
    } else if (method == "PUT") {
        m_currentReply = m_networkManager->put(request, requestBody);
    } else if (method == "DELETE") {
        m_currentReply = m_networkManager->deleteResource(request);
    } else if (method == "HEAD") {
        m_currentReply = m_networkManager->head(request);
    } else {
        // 对于其他方法（PATCH, OPTIONS），使用自定义请求
        m_currentReply = m_networkManager->sendCustomRequest(request, method.toUtf8(), requestBody);
    }

    if (!m_currentReply) {
        return;
    }

    // 连接信号
    connect(m_currentReply, &QNetworkReply::finished, this, &HttpClient::onRequestFinished);
    connect(m_currentReply, &QNetworkReply::errorOccurred, this, &HttpClient::onRequestError);
    connect(m_currentReply, &QNetworkReply::downloadProgress, this, &HttpClient::onDownloadProgress);

    // 更新UI状态
    m_sendBtn->setEnabled(false);
    m_cancelBtn->setEnabled(true);
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    m_statusLabel->setText(tr("状态: 发送请求中..."));

    // 启动计时器
    m_elapsedTimer.start();
    m_requestTimer->start(30000); // 30秒超时
}

void HttpClient::processResponse(QNetworkReply* reply)
{
    if (!reply) return;

    // 获取响应码
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QString statusText = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();

    m_responseInfo.statusCode = statusCode;
    m_responseInfo.statusText = statusText;

    // 获取响应头
    m_responseInfo.headers.clear();
    QList<QNetworkReply::RawHeaderPair> rawHeaders = reply->rawHeaderPairs();
    for (const auto& headerPair : rawHeaders) {
        QString key = QString::fromUtf8(headerPair.first);
        QString value = QString::fromUtf8(headerPair.second);
        m_responseInfo.headers.append(KeyValuePair(key, value, true));

        // 获取Content-Type
        if (key.toLower() == "content-type") {
            m_responseInfo.contentType = value;
        }
    }

    // 获取响应体
    m_responseInfo.body = reply->readAll();
    m_responseInfo.contentLength = m_responseInfo.body.size();

    // 计算响应时间
    m_responseInfo.responseTime = m_elapsedTimer.elapsed();
}

void HttpClient::displayResponse()
{
    // 更新状态标签
    QString statusColor = "#28a745"; // 绿色表示成功
    if (m_responseInfo.statusCode >= 400) {
        statusColor = "#dc3545"; // 红色表示错误
    } else if (m_responseInfo.statusCode >= 300) {
        statusColor = "#ffc107"; // 黄色表示重定向
    }

    m_statusLabel->setText(tr("状态: %1 %2").arg(m_responseInfo.statusCode).arg(m_responseInfo.statusText));
    m_statusLabel->setStyleSheet(QString("font-weight: bold; color: %1;").arg(statusColor));

    m_timeLabel->setText(tr("时间: %1").arg(formatTime(m_responseInfo.responseTime)));
    m_sizeLabel->setText(tr("大小: %1").arg(formatFileSize(m_responseInfo.contentLength)));

    // 显示响应体
    QString responseBody = QString::fromUtf8(m_responseInfo.body);
    m_responseBodyEdit->setText(responseBody);

    // 显示原始响应
    QString rawResponse = QString("状态行: HTTP/1.1 %1 %2\n")
                         .arg(m_responseInfo.statusCode)
                         .arg(m_responseInfo.statusText);

    rawResponse += "响应头:\n";
    for (const auto& header : m_responseInfo.headers) {
        rawResponse += QString("%1: %2\n").arg(header.key, header.value);
    }

    rawResponse += "\n响应体:\n";
    rawResponse += responseBody;

    m_rawResponseEdit->setText(rawResponse);
}

QString HttpClient::formatJsonResponse(const QString& json)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &error);

    if (error.error != QJsonParseError::NoError) {
        return json; // 返回原始 JSON
    }

    return doc.toJson(QJsonDocument::Indented);
}

QString HttpClient::formatXmlResponse(const QString& xml)
{
    // 简单的XML格式化（可以后续使用QDomDocument改进）
    QString formatted = xml;
    formatted.replace("><", ">\n<");
    return formatted;
}

void HttpClient::populateParametersTable()
{
    // TODO: 填充参数表格
}

void HttpClient::populateHeadersTable()
{
    // TODO: 填充头部表格
}

void HttpClient::populateCookiesTable()
{
    // TODO: 填充Cookie表格
}

void HttpClient::addTableRow(QTableWidget* table, const KeyValuePair& pair)
{
    int row = table->rowCount();
    table->insertRow(row);

    table->setItem(row, 0, new QTableWidgetItem(pair.key));
    table->setItem(row, 1, new QTableWidgetItem(pair.value));

    QCheckBox* checkBox = new QCheckBox();
    checkBox->setChecked(pair.enabled);
    table->setCellWidget(row, 2, checkBox);
}

void HttpClient::removeSelectedTableRows(QTableWidget* table)
{
    QList<int> selectedRows;
    for (int i = 0; i < table->rowCount(); ++i) {
        if (table->item(i, 0) && table->item(i, 0)->isSelected()) {
            selectedRows.append(i);
        }
    }

    // 从后往前删除以避免索引变化
    std::sort(selectedRows.begin(), selectedRows.end(), std::greater<int>());
    for (int row : selectedRows) {
        table->removeRow(row);
    }

    // 如果没有选中行，删除当前行
    if (selectedRows.isEmpty() && table->currentRow() >= 0) {
        table->removeRow(table->currentRow());
    }
}

QList<KeyValuePair> HttpClient::getTableData(QTableWidget* table)
{
    QList<KeyValuePair> data;

    for (int i = 0; i < table->rowCount(); ++i) {
        QTableWidgetItem* keyItem = table->item(i, 0);
        QTableWidgetItem* valueItem = table->item(i, 1);
        QCheckBox* enabledBox = qobject_cast<QCheckBox*>(table->cellWidget(i, 2));

        if (keyItem && valueItem) {
            QString key = keyItem->text().trimmed();
            QString value = valueItem->text().trimmed();
            bool enabled = enabledBox ? enabledBox->isChecked() : true;

            if (!key.isEmpty()) {
                data.append(KeyValuePair(key, value, enabled));
            }
        }
    }

    return data;
}

// 新增的槽函数实现
void HttpClient::onProtocolChanged()
{
    QString protocol = m_protocolCombo->currentText();
    // 自动设置端口默认值
    if (protocol == "https") {
        m_portSpin->setValue(443);
    } else if (protocol == "http") {
        m_portSpin->setValue(80);
    }
    updateFullUrl();
}

void HttpClient::onHostChanged()
{
    updateFullUrl();
}

void HttpClient::onPortChanged()
{
    updateFullUrl();
}

void HttpClient::onPathChanged()
{
    updateFullUrl();
}

void HttpClient::updateFullUrl()
{
    QString protocol = m_protocolCombo->currentText();
    QString host = m_hostEdit->text().trimmed();
    int port = m_portSpin->value();
    QString path = m_pathEdit->text().trimmed();

    if (host.isEmpty()) {
        return;
    }

    if (path.isEmpty() || !path.startsWith("/")) {
        path = "/" + path;
    }

    // 构建完整URL显示（用于调试或显示，实际请求时使用组件）
    QString fullUrl = QString("%1://%2:%3%4").arg(protocol).arg(host).arg(port).arg(path);

    // 可以在这里更新一个隐藏的完整URL标签用于显示
    setToolTip(fullUrl); // 暂时用tooltip显示完整URL
}

void HttpClient::onFormatBodyClicked()
{
    QString content = m_bodyEdit->toPlainText().trimmed();
    if (content.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请先输入内容"));
        return;
    }

    QString bodyType = m_bodyTypeCombo->currentText();
    QString formatted;
    QString successMsg;
    QString errorMsg;

    if (bodyType.contains("JSON", Qt::CaseInsensitive)) {
        formatted = formatJsonResponse(content);
        successMsg = tr("JSON格式化完成");
        errorMsg = tr("无效的JSON格式");
    } else if (bodyType.contains("XML", Qt::CaseInsensitive)) {
        formatted = formatXmlResponse(content);
        successMsg = tr("XML格式化完成");
        errorMsg = tr("无效的XML格式");
    }

    if (formatted != content) {
        m_bodyEdit->setText(formatted);
        // 格式化成功不再弹出提示
    } else {
        QMessageBox::warning(this, tr("错误"), errorMsg);
    }
}