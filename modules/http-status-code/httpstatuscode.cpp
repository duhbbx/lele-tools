#include "httpstatuscode.h"
#include <QTextStream>
#include <QStringConverter>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

REGISTER_DYNAMICOBJECT(HttpStatusCode);

// 静态常量定义
const QStringList HttpStatusCode::CATEGORY_NAMES = {
    "全部", "1xx 信息", "2xx 成功", "3xx 重定向", "4xx 客户端错误", "5xx 服务器错误"
};

const QStringList HttpStatusCode::CATEGORY_COLORS = {
    "#6c757d", "#17a2b8", "#28a745", "#ffc107", "#dc3545", "#6f42c1"
};

HttpStatusCode::HttpStatusCode() : QWidget(nullptr), DynamicObjectBase()
{
    setupUI();
    initializeStatusCodes();
    populateStatusTable();
    
    // 应用基本样式
    setStyleSheet("QWidget { font-family: 'Microsoft YaHei', '微软雅黑', sans-serif; }");
}

HttpStatusCode::~HttpStatusCode()
{
}

void HttpStatusCode::setupUI()
{
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);
    
    mainSplitter = new QSplitter(Qt::Horizontal, this);
    
    // 左侧面板
    leftPanel = new QWidget;
    leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    
    // 右侧面板
    rightPanel = new QWidget;
    rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    
    setupSearchArea();
    setupFilterArea();
    setupStatusTableArea();
    setupDetailsArea();
    setupToolbarArea();
    
    leftLayout->addWidget(searchGroup);
    leftLayout->addWidget(filterGroup);
    leftLayout->addWidget(tableGroup);
    
    rightLayout->addWidget(detailsGroup);
    rightLayout->addWidget(toolbarGroup);
    
    mainSplitter->addWidget(leftPanel);
    mainSplitter->addWidget(rightPanel);
    mainSplitter->setSizes({500, 400});
    
    mainLayout->addWidget(mainSplitter, 1); // 添加stretch factor
}

void HttpStatusCode::setupSearchArea()
{
    searchGroup = new QGroupBox("搜索", this);
    searchLayout = new QVBoxLayout(searchGroup);
    
    searchEdit = new QLineEdit;
    searchEdit->setPlaceholderText(tr("输入状态码或描述进行搜索..."));
    searchLayout->addWidget(searchEdit);
    
    QHBoxLayout *searchButtonLayout = new QHBoxLayout;
    quickSearchBtn = new QPushButton(tr("快速查找"));
    searchResultLabel = new QLabel(tr("显示所有状态码"));
    
    searchButtonLayout->addWidget(quickSearchBtn);
    searchButtonLayout->addStretch();
    searchButtonLayout->addWidget(searchResultLabel);
    searchLayout->addLayout(searchButtonLayout);
    
    // 连接信号
    connect(searchEdit, &QLineEdit::textChanged, this, &HttpStatusCode::onSearchTextChanged);
    connect(quickSearchBtn, &QPushButton::clicked, this, &HttpStatusCode::onQuickSearchClicked);
}

void HttpStatusCode::setupFilterArea()
{
    filterGroup = new QGroupBox("筛选", this);
    filterLayout = new QGridLayout(filterGroup);
    
    // 分类筛选
    filterLayout->addWidget(new QLabel(tr("分类:")), 0, 0);
    categoryCombo = new QComboBox;
    categoryCombo->addItems(CATEGORY_NAMES);
    filterLayout->addWidget(categoryCombo, 0, 1, 1, 2);
    
    // 常用状态码筛选
    commonOnlyCheckBox = new QCheckBox("仅显示常用状态码");
    filterLayout->addWidget(commonOnlyCheckBox, 1, 0, 1, 3);
    
    // 分类复选框
    show1xxCheckBox = new QCheckBox("1xx");
    show2xxCheckBox = new QCheckBox("2xx");
    show3xxCheckBox = new QCheckBox("3xx");
    show4xxCheckBox = new QCheckBox("4xx");
    show5xxCheckBox = new QCheckBox("5xx");
    
    // 默认全部选中
    show1xxCheckBox->setChecked(true);
    show2xxCheckBox->setChecked(true);
    show3xxCheckBox->setChecked(true);
    show4xxCheckBox->setChecked(true);
    show5xxCheckBox->setChecked(true);
    
    filterLayout->addWidget(show1xxCheckBox, 2, 0);
    filterLayout->addWidget(show2xxCheckBox, 2, 1);
    filterLayout->addWidget(show3xxCheckBox, 2, 2);
    filterLayout->addWidget(show4xxCheckBox, 3, 0);
    filterLayout->addWidget(show5xxCheckBox, 3, 1);
    
    resetFilterBtn = new QPushButton(tr("重置筛选"));
    filterLayout->addWidget(resetFilterBtn, 3, 2);
    
    // 连接信号
    connect(categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &HttpStatusCode::onCategoryFilterChanged);
    connect(commonOnlyCheckBox, &QCheckBox::toggled, this, &HttpStatusCode::onCommonOnlyToggled);
    connect(show1xxCheckBox, &QCheckBox::toggled, [this]() { filterStatusCodes(); });
    connect(show2xxCheckBox, &QCheckBox::toggled, [this]() { filterStatusCodes(); });
    connect(show3xxCheckBox, &QCheckBox::toggled, [this]() { filterStatusCodes(); });
    connect(show4xxCheckBox, &QCheckBox::toggled, [this]() { filterStatusCodes(); });
    connect(show5xxCheckBox, &QCheckBox::toggled, [this]() { filterStatusCodes(); });
    connect(resetFilterBtn, &QPushButton::clicked, [this]() {
        categoryCombo->setCurrentIndex(0);
        commonOnlyCheckBox->setChecked(false);
        show1xxCheckBox->setChecked(true);
        show2xxCheckBox->setChecked(true);
        show3xxCheckBox->setChecked(true);
        show4xxCheckBox->setChecked(true);
        show5xxCheckBox->setChecked(true);
        searchEdit->clear();
        filterStatusCodes();
    });
}

void HttpStatusCode::setupStatusTableArea()
{
    tableGroup = new QGroupBox("HTTP状态码列表", this);
    tableLayout = new QVBoxLayout(tableGroup);
    
    statusTable = new QTableWidget;
    statusTable->setColumnCount(4);
    statusTable->setHorizontalHeaderLabels({"状态码", "状态短语", "分类", "描述"});
    statusTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    statusTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    statusTable->setSortingEnabled(true);
    statusTable->setAlternatingRowColors(true);
    
    // 设置列宽
    statusTable->setColumnWidth(0, 80);
    statusTable->setColumnWidth(1, 200);
    statusTable->setColumnWidth(2, 80);
    statusTable->horizontalHeader()->setStretchLastSection(true);
    
    tableLayout->addWidget(statusTable);
    
    countLabel = new QLabel(tr("共 0 个状态码"));
    countLabel->setAlignment(Qt::AlignRight);
    tableLayout->addWidget(countLabel);
    
    // 连接信号
    connect(statusTable, &QTableWidget::itemClicked, this, &HttpStatusCode::onStatusCodeClicked);
}

void HttpStatusCode::setupDetailsArea()
{
    detailsGroup = new QGroupBox("状态码详细信息", this);
    detailsLayout = new QVBoxLayout(detailsGroup);
    
    // 基本信息
    QWidget *basicInfoWidget = new QWidget;
    QGridLayout *basicInfoLayout = new QGridLayout(basicInfoWidget);
    
    basicInfoLayout->addWidget(new QLabel(tr("状态码:")), 0, 0);
    codeLabel = new QLabel(tr("--"));
    codeLabel->setStyleSheet("font-size: 14pt; font-weight: bold; color: #4CAF50;");
    basicInfoLayout->addWidget(codeLabel, 0, 1);
    
    basicInfoLayout->addWidget(new QLabel(tr("状态短语:")), 1, 0);
    phraseLabel = new QLabel(tr("--"));
    phraseLabel->setStyleSheet("font-size: 12pt; font-weight: bold;");
    basicInfoLayout->addWidget(phraseLabel, 1, 1);
    
    basicInfoLayout->addWidget(new QLabel(tr("分类:")), 2, 0);
    categoryLabel = new QLabel(tr("--"));
    basicInfoLayout->addWidget(categoryLabel, 2, 1);
    
    detailsLayout->addWidget(basicInfoWidget);
    
    // 详细描述
    detailsLayout->addWidget(new QLabel(tr("详细描述:")));
    descriptionEdit = new QTextEdit;
    descriptionEdit->setMaximumHeight(80);
    descriptionEdit->setReadOnly(true);
    descriptionEdit->setPlaceholderText(tr("点击左侧状态码查看详细描述..."));
    detailsLayout->addWidget(descriptionEdit);
    
    // 使用场景
    detailsLayout->addWidget(new QLabel(tr("使用场景:")));
    usageEdit = new QTextEdit;
    usageEdit->setMaximumHeight(60);
    usageEdit->setReadOnly(true);
    usageEdit->setPlaceholderText(tr("使用场景说明..."));
    detailsLayout->addWidget(usageEdit);
    
    // 示例
    detailsLayout->addWidget(new QLabel(tr("示例:")));
    examplesEdit = new QTextEdit;
    examplesEdit->setMaximumHeight(60);
    examplesEdit->setReadOnly(true);
    examplesEdit->setPlaceholderText(tr("代码示例..."));
    detailsLayout->addWidget(examplesEdit);
    
    // RFC标准
    rfcLabel = new QLabel(tr("RFC标准: --"));
    rfcLabel->setStyleSheet("color: #6c757d; font-size: 10pt;");
    detailsLayout->addWidget(rfcLabel);
}

void HttpStatusCode::setupToolbarArea()
{
    toolbarGroup = new QGroupBox("操作", this);
    toolbarLayout = new QHBoxLayout(toolbarGroup);
    
    copyCodeBtn = new QPushButton(tr("复制状态码"));
    copyDescBtn = new QPushButton(tr("复制描述"));
    exportBtn = new QPushButton(tr("导出列表"));
    refreshBtn = new QPushButton(tr("刷新"));
    
    toolbarLayout->addWidget(copyCodeBtn);
    toolbarLayout->addWidget(copyDescBtn);
    toolbarLayout->addWidget(exportBtn);
    toolbarLayout->addWidget(refreshBtn);
    toolbarLayout->addStretch();
    
    // 连接信号
    connect(copyCodeBtn, &QPushButton::clicked, this, &HttpStatusCode::onCopyCodeClicked);
    connect(copyDescBtn, &QPushButton::clicked, this, &HttpStatusCode::onCopyDescriptionClicked);
    connect(exportBtn, &QPushButton::clicked, this, &HttpStatusCode::onExportClicked);
    connect(refreshBtn, &QPushButton::clicked, this, &HttpStatusCode::onRefreshClicked);
}

void HttpStatusCode::initializeStatusCodes()
{
    m_allStatusCodes.clear();
    
    // 1xx 信息响应
    m_allStatusCodes.append(HttpStatusInfo(100, "Continue", 
        "服务器已接收到请求头，客户端应继续发送请求主体", "1xx", "信息响应", true, "RFC 7231", 
        "用于大文件上传或需要确认的请求", {"POST /upload HTTP/1.1", "Expect: 100-continue"}));
    
    m_allStatusCodes.append(HttpStatusInfo(101, "Switching Protocols", 
        "服务器已理解并准备切换协议", "1xx", "信息响应", false, "RFC 7231", 
        "WebSocket握手过程中使用", {"Upgrade: websocket", "Connection: Upgrade"}));
    
    m_allStatusCodes.append(HttpStatusInfo(102, "Processing", 
        "服务器正在处理请求，但尚未完成", "1xx", "信息响应", false, "RFC 2518", 
        "WebDAV中用于长时间处理", {"PROPFIND请求处理中"}));
    
    m_allStatusCodes.append(HttpStatusInfo(103, "Early Hints", 
        "服务器在最终响应前提供一些响应头", "1xx", "信息响应", false, "RFC 8297", 
        "提前发送Link头以优化加载", {"Link: </style.css>; rel=preload"}));
    
    // 2xx 成功响应
    m_allStatusCodes.append(HttpStatusInfo(200, "OK", 
        "请求成功，服务器返回请求的数据", "2xx", "成功响应", true, "RFC 7231", 
        "最常见的成功响应，适用于GET、POST等", {"GET /api/users -> 返回用户列表", "POST /api/login -> 登录成功"}));
    
    m_allStatusCodes.append(HttpStatusInfo(201, "Created", 
        "请求成功，服务器创建了新的资源", "2xx", "成功响应", true, "RFC 7231", 
        "创建新资源后返回，通常用于POST请求", {"POST /api/users -> 创建新用户", "PUT /api/articles/123 -> 创建新文章"}));
    
    m_allStatusCodes.append(HttpStatusInfo(202, "Accepted", 
        "请求已接受，但处理尚未完成", "2xx", "成功响应", true, "RFC 7231", 
        "异步处理请求时使用", {"POST /api/batch-process -> 批处理任务已接受"}));
    
    m_allStatusCodes.append(HttpStatusInfo(203, "Non-Authoritative Information", 
        "请求成功，但返回的信息可能来自第三方", "2xx", "成功响应", false, "RFC 7231", 
        "代理服务器修改了响应", {"通过代理获取的缓存内容"}));
    
    m_allStatusCodes.append(HttpStatusInfo(204, "No Content", 
        "请求成功，但没有返回内容", "2xx", "成功响应", true, "RFC 7231", 
        "删除操作或不需要返回数据的更新", {"DELETE /api/users/123", "PUT /api/settings -> 更新成功但无需返回数据"}));
    
    m_allStatusCodes.append(HttpStatusInfo(205, "Reset Content", 
        "请求成功，客户端应重置文档视图", "2xx", "成功响应", false, "RFC 7231", 
        "表单提交后重置表单", {"表单提交成功后清空输入"}));
    
    m_allStatusCodes.append(HttpStatusInfo(206, "Partial Content", 
        "服务器成功处理了部分GET请求", "2xx", "成功响应", false, "RFC 7233", 
        "断点续传或视频流播放", {"Range: bytes=200-1023", "视频分片下载"}));
    
    // 3xx 重定向
    m_allStatusCodes.append(HttpStatusInfo(300, "Multiple Choices", 
        "请求有多个可能的响应", "3xx", "重定向", false, "RFC 7231", 
        "多个资源选择", {"多种格式的资源可供选择"}));
    
    m_allStatusCodes.append(HttpStatusInfo(301, "Moved Permanently", 
        "请求的资源已永久移动到新位置", "3xx", "重定向", true, "RFC 7231", 
        "网站迁移或URL结构变更", {"Location: https://new-domain.com/page", "SEO友好的永久重定向"}));
    
    m_allStatusCodes.append(HttpStatusInfo(302, "Found", 
        "请求的资源临时移动到新位置", "3xx", "重定向", true, "RFC 7231", 
        "临时重定向，搜索引擎不会更新索引", {"Location: /login", "临时维护页面重定向"}));
    
    m_allStatusCodes.append(HttpStatusInfo(303, "See Other", 
        "对应当前请求的响应可以在另一个URI上被找到", "3xx", "重定向", false, "RFC 7231", 
        "POST请求后重定向到GET页面", {"POST后重定向到结果页面"}));
    
    m_allStatusCodes.append(HttpStatusInfo(304, "Not Modified", 
        "资源未修改，可以使用缓存版本", "3xx", "重定向", true, "RFC 7232", 
        "条件请求中资源未变化", {"If-Modified-Since头检查", "浏览器缓存验证"}));
    
    m_allStatusCodes.append(HttpStatusInfo(307, "Temporary Redirect", 
        "临时重定向，且不能改变HTTP方法", "3xx", "重定向", false, "RFC 7231", 
        "保持原请求方法的临时重定向", {"POST请求保持为POST重定向"}));
    
    m_allStatusCodes.append(HttpStatusInfo(308, "Permanent Redirect", 
        "永久重定向，且不能改变HTTP方法", "3xx", "重定向", false, "RFC 7538", 
        "保持原请求方法的永久重定向", {"POST请求保持为POST重定向"}));
    
    // 4xx 客户端错误
    m_allStatusCodes.append(HttpStatusInfo(400, "Bad Request", 
        "请求语法错误或请求无法被服务器理解", "4xx", "客户端错误", true, "RFC 7231", 
        "请求参数错误或格式不正确", {"缺少必需参数", "JSON格式错误", "请求体过大"}));
    
    m_allStatusCodes.append(HttpStatusInfo(401, "Unauthorized", 
        "请求需要身份验证", "4xx", "客户端错误", true, "RFC 7235", 
        "用户未登录或认证失败", {"WWW-Authenticate: Bearer", "Token过期或无效"}));
    
    m_allStatusCodes.append(HttpStatusInfo(402, "Payment Required", 
        "保留状态码，用于未来的付费系统", "4xx", "客户端错误", false, "RFC 7231", 
        "付费内容或服务", {"数字支付系统"}));
    
    m_allStatusCodes.append(HttpStatusInfo(403, "Forbidden", 
        "服务器理解请求但拒绝执行", "4xx", "客户端错误", true, "RFC 7231", 
        "权限不足或访问被禁止", {"无权限访问资源", "IP被封禁", "账户被冻结"}));
    
    m_allStatusCodes.append(HttpStatusInfo(404, "Not Found", 
        "请求的资源不存在", "4xx", "客户端错误", true, "RFC 7231", 
        "最常见的错误，资源不存在", {"页面不存在", "API接口不存在", "文件未找到"}));
    
    m_allStatusCodes.append(HttpStatusInfo(405, "Method Not Allowed", 
        "请求方法不被允许", "4xx", "客户端错误", true, "RFC 7231", 
        "HTTP方法不支持", {"Allow: GET, POST", "对只读资源使用POST"}));
    
    m_allStatusCodes.append(HttpStatusInfo(406, "Not Acceptable", 
        "服务器无法根据Accept头提供合适的响应", "4xx", "客户端错误", false, "RFC 7231", 
        "内容协商失败", {"Accept: application/json但只有XML"}));
    
    m_allStatusCodes.append(HttpStatusInfo(407, "Proxy Authentication Required", 
        "代理服务器需要身份验证", "4xx", "客户端错误", false, "RFC 7235", 
        "代理服务器认证", {"Proxy-Authenticate头"}));
    
    m_allStatusCodes.append(HttpStatusInfo(408, "Request Timeout", 
        "服务器等待请求超时", "4xx", "客户端错误", false, "RFC 7231", 
        "请求处理超时", {"长时间无数据传输"}));
    
    m_allStatusCodes.append(HttpStatusInfo(409, "Conflict", 
        "请求与服务器当前状态冲突", "4xx", "客户端错误", true, "RFC 7231", 
        "资源冲突或并发问题", {"重复创建资源", "版本冲突", "并发修改冲突"}));
    
    m_allStatusCodes.append(HttpStatusInfo(410, "Gone", 
        "请求的资源已永久删除", "4xx", "客户端错误", false, "RFC 7231", 
        "资源已被永久删除", {"内容已下线", "API版本已废弃"}));
    
    m_allStatusCodes.append(HttpStatusInfo(411, "Length Required", 
        "服务器需要Content-Length头", "4xx", "客户端错误", false, "RFC 7231", 
        "缺少Content-Length头", {"POST请求需要指定长度"}));
    
    m_allStatusCodes.append(HttpStatusInfo(412, "Precondition Failed", 
        "前置条件失败", "4xx", "客户端错误", false, "RFC 7232", 
        "条件请求失败", {"If-Match条件不满足"}));
    
    m_allStatusCodes.append(HttpStatusInfo(413, "Payload Too Large", 
        "请求实体过大", "4xx", "客户端错误", true, "RFC 7231", 
        "上传文件或数据过大", {"文件上传超过限制", "请求体过大"}));
    
    m_allStatusCodes.append(HttpStatusInfo(414, "URI Too Long", 
        "请求URI过长", "4xx", "客户端错误", false, "RFC 7231", 
        "URL长度超过限制", {"GET参数过多"}));
    
    m_allStatusCodes.append(HttpStatusInfo(415, "Unsupported Media Type", 
        "不支持的媒体类型", "4xx", "客户端错误", true, "RFC 7231", 
        "Content-Type不支持", {"上传不支持的文件格式"}));
    
    m_allStatusCodes.append(HttpStatusInfo(416, "Range Not Satisfiable", 
        "请求的范围无法满足", "4xx", "客户端错误", false, "RFC 7233", 
        "Range请求超出范围", {"断点续传范围错误"}));
    
    m_allStatusCodes.append(HttpStatusInfo(417, "Expectation Failed", 
        "Expect请求头字段无法满足", "4xx", "客户端错误", false, "RFC 7231", 
        "Expect头处理失败", {"100-continue失败"}));
    
    m_allStatusCodes.append(HttpStatusInfo(418, "I'm a teapot", 
        "愚人节玩笑状态码", "4xx", "客户端错误", false, "RFC 2324", 
        "茶壶拒绝煮咖啡", {"HTCPCP协议彩蛋"}));
    
    m_allStatusCodes.append(HttpStatusInfo(421, "Misdirected Request", 
        "请求被误导到无法响应的服务器", "4xx", "客户端错误", false, "RFC 7540", 
        "HTTP/2中的服务器选择错误", {"服务器无法为请求的域名提供响应"}));
    
    m_allStatusCodes.append(HttpStatusInfo(422, "Unprocessable Entity", 
        "请求格式正确但语义错误", "4xx", "客户端错误", true, "RFC 4918", 
        "表单验证失败", {"字段验证错误", "业务逻辑验证失败"}));
    
    m_allStatusCodes.append(HttpStatusInfo(423, "Locked", 
        "资源被锁定", "4xx", "客户端错误", false, "RFC 4918", 
        "WebDAV资源锁定", {"文件被其他用户锁定"}));
    
    m_allStatusCodes.append(HttpStatusInfo(424, "Failed Dependency", 
        "依赖的请求失败", "4xx", "客户端错误", false, "RFC 4918", 
        "WebDAV依赖操作失败", {"批量操作中某项失败"}));
    
    m_allStatusCodes.append(HttpStatusInfo(425, "Too Early", 
        "服务器不愿意处理可能被重放的请求", "4xx", "客户端错误", false, "RFC 8470", 
        "TLS早期数据安全问题", {"0-RTT数据重放攻击防护"}));
    
    m_allStatusCodes.append(HttpStatusInfo(426, "Upgrade Required", 
        "客户端需要升级协议", "4xx", "客户端错误", false, "RFC 7231", 
        "强制协议升级", {"Upgrade: TLS/1.2"}));
    
    m_allStatusCodes.append(HttpStatusInfo(428, "Precondition Required", 
        "请求需要前置条件", "4xx", "客户端错误", false, "RFC 6585", 
        "防止丢失更新问题", {"需要If-Match头"}));
    
    m_allStatusCodes.append(HttpStatusInfo(429, "Too Many Requests", 
        "请求过于频繁", "4xx", "客户端错误", true, "RFC 6585", 
        "API限流和防止滥用", {"Retry-After: 60", "超出API调用限制"}));
    
    m_allStatusCodes.append(HttpStatusInfo(431, "Request Header Fields Too Large", 
        "请求头字段过大", "4xx", "客户端错误", false, "RFC 6585", 
        "请求头超过服务器限制", {"Cookie过大", "自定义头过多"}));
    
    m_allStatusCodes.append(HttpStatusInfo(451, "Unavailable For Legal Reasons", 
        "因法律原因不可用", "4xx", "客户端错误", false, "RFC 7725", 
        "内容被法律禁止", {"政府审查", "版权限制"}));
    
    // 5xx 服务器错误
    m_allStatusCodes.append(HttpStatusInfo(500, "Internal Server Error", 
        "服务器内部错误", "5xx", "服务器错误", true, "RFC 7231", 
        "最常见的服务器错误", {"代码异常", "数据库连接失败", "未处理的异常"}));
    
    m_allStatusCodes.append(HttpStatusInfo(501, "Not Implemented", 
        "服务器不支持请求的功能", "5xx", "服务器错误", false, "RFC 7231", 
        "功能未实现", {"不支持的HTTP方法", "功能开发中"}));
    
    m_allStatusCodes.append(HttpStatusInfo(502, "Bad Gateway", 
        "网关或代理服务器错误", "5xx", "服务器错误", true, "RFC 7231", 
        "上游服务器返回无效响应", {"负载均衡器错误", "反向代理连接失败"}));
    
    m_allStatusCodes.append(HttpStatusInfo(503, "Service Unavailable", 
        "服务暂时不可用", "5xx", "服务器错误", true, "RFC 7231", 
        "服务器过载或维护", {"Retry-After: 120", "系统维护中", "服务器过载"}));
    
    m_allStatusCodes.append(HttpStatusInfo(504, "Gateway Timeout", 
        "网关超时", "5xx", "服务器错误", true, "RFC 7231", 
        "上游服务器响应超时", {"代理服务器超时", "微服务调用超时"}));
    
    m_allStatusCodes.append(HttpStatusInfo(505, "HTTP Version Not Supported", 
        "不支持的HTTP版本", "5xx", "服务器错误", false, "RFC 7231", 
        "HTTP协议版本不支持", {"使用了过新的HTTP版本"}));
    
    m_allStatusCodes.append(HttpStatusInfo(506, "Variant Also Negotiates", 
        "内容协商配置错误", "5xx", "服务器错误", false, "RFC 2295", 
        "服务器配置错误导致循环", {"内容协商死循环"}));
    
    m_allStatusCodes.append(HttpStatusInfo(507, "Insufficient Storage", 
        "存储空间不足", "5xx", "服务器错误", false, "RFC 4918", 
        "服务器存储空间不够", {"磁盘空间已满"}));
    
    m_allStatusCodes.append(HttpStatusInfo(508, "Loop Detected", 
        "检测到无限循环", "5xx", "服务器错误", false, "RFC 5842", 
        "WebDAV处理中检测到循环", {"资源引用循环"}));
    
    m_allStatusCodes.append(HttpStatusInfo(510, "Not Extended", 
        "需要扩展请求", "5xx", "服务器错误", false, "RFC 2774", 
        "请求需要进一步扩展", {"缺少必需的扩展"}));
    
    m_allStatusCodes.append(HttpStatusInfo(511, "Network Authentication Required", 
        "需要网络认证", "5xx", "服务器错误", false, "RFC 6585", 
        "网络接入需要认证", {"WiFi登录页面", "网络代理认证"}));
    
    // 一些非标准但常见的状态码
    m_allStatusCodes.append(HttpStatusInfo(520, "Web Server Returned an Unknown Error", 
        "Web服务器返回了未知错误", "5xx", "服务器错误", false, "Cloudflare", 
        "Cloudflare代理错误", {"源服务器返回空响应"}));
    
    m_allStatusCodes.append(HttpStatusInfo(521, "Web Server Is Down", 
        "Web服务器宕机", "5xx", "服务器错误", false, "Cloudflare", 
        "源服务器拒绝连接", {"服务器维护或宕机"}));
    
    m_allStatusCodes.append(HttpStatusInfo(522, "Connection Timed Out", 
        "连接超时", "5xx", "服务器错误", false, "Cloudflare", 
        "TCP握手超时", {"服务器响应太慢"}));
    
    m_allStatusCodes.append(HttpStatusInfo(523, "Origin Is Unreachable", 
        "源服务器不可达", "5xx", "服务器错误", false, "Cloudflare", 
        "无法到达源服务器", {"DNS解析失败", "网络路由问题"}));
    
    m_allStatusCodes.append(HttpStatusInfo(524, "A Timeout Occurred", 
        "发生超时", "5xx", "服务器错误", false, "Cloudflare", 
        "TCP连接成功但HTTP超时", {"服务器处理请求超时"}));
    
    m_allStatusCodes.append(HttpStatusInfo(525, "SSL Handshake Failed", 
        "SSL握手失败", "5xx", "服务器错误", false, "Cloudflare", 
        "SSL/TLS握手失败", {"证书问题", "协议版本不匹配"}));
    
    m_allStatusCodes.append(HttpStatusInfo(526, "Invalid SSL Certificate", 
        "无效的SSL证书", "5xx", "服务器错误", false, "Cloudflare", 
        "SSL证书无效", {"证书过期", "证书域名不匹配"}));
    
    // 微软IIS特有状态码
    m_allStatusCodes.append(HttpStatusInfo(440, "Login Time-out", 
        "登录超时", "4xx", "客户端错误", false, "IIS", 
        "用户登录会话超时", {"需要重新登录"}));
    
    m_allStatusCodes.append(HttpStatusInfo(449, "Retry With", 
        "需要重试", "4xx", "客户端错误", false, "IIS", 
        "请求需要重试", {"服务器忙碌"}));
    
    // nginx特有状态码
    m_allStatusCodes.append(HttpStatusInfo(444, "No Response", 
        "无响应", "4xx", "客户端错误", false, "nginx", 
        "服务器不返回信息并关闭连接", {"恶意请求拦截"}));
    
    m_allStatusCodes.append(HttpStatusInfo(494, "Request Header Too Large", 
        "请求头过大", "4xx", "客户端错误", false, "nginx", 
        "请求头超过nginx限制", {"client_header_buffer_size限制"}));
    
    m_allStatusCodes.append(HttpStatusInfo(495, "SSL Certificate Error", 
        "SSL证书错误", "4xx", "客户端错误", false, "nginx", 
        "客户端证书错误", {"客户端证书验证失败"}));
    
    m_allStatusCodes.append(HttpStatusInfo(496, "SSL Certificate Required", 
        "需要SSL证书", "4xx", "客户端错误", false, "nginx", 
        "需要客户端提供证书", {"双向SSL认证"}));
    
    m_allStatusCodes.append(HttpStatusInfo(497, "HTTP Request Sent to HTTPS Port", 
        "HTTP请求发送到HTTPS端口", "4xx", "客户端错误", false, "nginx", 
        "协议错误", {"使用HTTP访问HTTPS端口"}));
    
    m_allStatusCodes.append(HttpStatusInfo(499, "Client Closed Request", 
        "客户端关闭请求", "4xx", "客户端错误", false, "nginx", 
        "客户端主动断开连接", {"用户取消请求"}));
    
    m_filteredStatusCodes = m_allStatusCodes;
}

void HttpStatusCode::populateStatusTable()
{
    statusTable->setRowCount(m_filteredStatusCodes.size());
    
    for (int i = 0; i < m_filteredStatusCodes.size(); ++i) {
        const HttpStatusInfo &info = m_filteredStatusCodes[i];
        
        // 状态码列
        QTableWidgetItem *codeItem = new QTableWidgetItem(QString::number(info.code));
        codeItem->setTextAlignment(Qt::AlignCenter);
        codeItem->setData(Qt::UserRole, i); // 存储原始索引
        statusTable->setItem(i, 0, codeItem);
        
        // 状态短语列
        QTableWidgetItem *phraseItem = new QTableWidgetItem(info.phrase);
        statusTable->setItem(i, 1, phraseItem);
        
        // 分类列
        QTableWidgetItem *categoryItem = new QTableWidgetItem(info.category);
        categoryItem->setTextAlignment(Qt::AlignCenter);
        QString categoryColor = getColorForCategory(info.category);
        categoryItem->setBackground(QBrush(QColor(categoryColor)));
        statusTable->setItem(i, 2, categoryItem);
        
        // 描述列
        QTableWidgetItem *descItem = new QTableWidgetItem(info.description);
        statusTable->setItem(i, 3, descItem);
        
        // 如果是常用状态码，加粗显示
        if (info.isCommon) {
            QFont font = codeItem->font();
            font.setBold(true);
            codeItem->setFont(font);
            phraseItem->setFont(font);
        }
    }
    
    updateCountLabel();
}

void HttpStatusCode::filterStatusCodes()
{
    m_filteredStatusCodes.clear();
    QString searchText = searchEdit->text().trimmed().toLower();
    int categoryIndex = categoryCombo->currentIndex();
    bool commonOnly = commonOnlyCheckBox->isChecked();
    
    for (const HttpStatusInfo &info : m_allStatusCodes) {
        bool matchesSearch = searchText.isEmpty() || 
                           QString::number(info.code).contains(searchText) ||
                           info.phrase.toLower().contains(searchText) ||
                           info.description.toLower().contains(searchText) ||
                           info.categoryName.toLower().contains(searchText);
        
        bool matchesCategory = (categoryIndex == 0) || // "全部"
                              (categoryIndex == 1 && info.category == "1xx") ||
                              (categoryIndex == 2 && info.category == "2xx") ||
                              (categoryIndex == 3 && info.category == "3xx") ||
                              (categoryIndex == 4 && info.category == "4xx") ||
                              (categoryIndex == 5 && info.category == "5xx");
        
        bool matchesCommon = !commonOnly || info.isCommon;
        
        bool matchesCheckboxes = (info.category == "1xx" && show1xxCheckBox->isChecked()) ||
                                (info.category == "2xx" && show2xxCheckBox->isChecked()) ||
                                (info.category == "3xx" && show3xxCheckBox->isChecked()) ||
                                (info.category == "4xx" && show4xxCheckBox->isChecked()) ||
                                (info.category == "5xx" && show5xxCheckBox->isChecked());
        
        if (matchesSearch && matchesCategory && matchesCommon && matchesCheckboxes) {
            m_filteredStatusCodes.append(info);
        }
    }
    
    populateStatusTable();
    updateSearchResultLabel();
}

void HttpStatusCode::updateDetailsPanel(const HttpStatusInfo &info)
{
    m_currentStatus = info;
    
    codeLabel->setText(QString::number(info.code));
    phraseLabel->setText(info.phrase);
    categoryLabel->setText(info.categoryName);
    
    QString categoryColor = getColorForCategory(info.category);
    codeLabel->setStyleSheet(QString("font-size: 14pt; font-weight: bold; color: %1;").arg(categoryColor));
    
    descriptionEdit->setText(info.description);
    usageEdit->setText(info.usage);
    examplesEdit->setText(info.examples.join("\n"));
    rfcLabel->setText(QString("RFC标准: %1").arg(info.rfc.isEmpty() ? "无" : info.rfc));
}

void HttpStatusCode::clearDetailsPanel()
{
    m_currentStatus = HttpStatusInfo();
    codeLabel->setText(tr("--"));
    phraseLabel->setText(tr("--"));
    categoryLabel->setText(tr("--"));
    descriptionEdit->clear();
    usageEdit->clear();
    examplesEdit->clear();
    rfcLabel->setText(tr("RFC标准: --"));
}

void HttpStatusCode::updateCountLabel()
{
    countLabel->setText(QString("共 %1 个状态码").arg(m_filteredStatusCodes.size()));
}

void HttpStatusCode::updateSearchResultLabel()
{
    QString text;
    if (searchEdit->text().trimmed().isEmpty()) {
        text = QString("显示所有状态码 (%1 个)").arg(m_filteredStatusCodes.size());
    } else {
        text = QString("搜索结果: %1 个").arg(m_filteredStatusCodes.size());
    }
    searchResultLabel->setText(text);
}

QString HttpStatusCode::getColorForCategory(const QString &category)
{
    if (category == "1xx") return "#17a2b8";
    if (category == "2xx") return "#28a745";
    if (category == "3xx") return "#ffc107";
    if (category == "4xx") return "#dc3545";
    if (category == "5xx") return "#6f42c1";
    return "#6c757d";
}

QString HttpStatusCode::formatStatusCode(int code)
{
    return QString::number(code);
}

// 槽函数实现
void HttpStatusCode::onSearchTextChanged()
{
    filterStatusCodes();
}

void HttpStatusCode::onCategoryFilterChanged()
{
    filterStatusCodes();
}

void HttpStatusCode::onCommonOnlyToggled(bool checked)
{
    Q_UNUSED(checked)
    filterStatusCodes();
}

void HttpStatusCode::onStatusCodeClicked(QTableWidgetItem *item)
{
    if (!item) return;
    
    int row = item->row();
    if (row >= 0 && row < m_filteredStatusCodes.size()) {
        updateDetailsPanel(m_filteredStatusCodes[row]);
    }
}

void HttpStatusCode::onCopyCodeClicked()
{
    if (m_currentStatus.code == 0) {
        QMessageBox::information(this, tr("提示"), tr("请先选择一个状态码"));
        return;
    }
    
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(QString::number(m_currentStatus.code));
    QMessageBox::information(this, tr("成功"), tr("状态码已复制到剪贴板"));
}

void HttpStatusCode::onCopyDescriptionClicked()
{
    if (m_currentStatus.code == 0) {
        QMessageBox::information(this, tr("提示"), tr("请先选择一个状态码"));
        return;
    }
    
    QClipboard *clipboard = QApplication::clipboard();
    QString text = QString("%1 %2\n%3").arg(m_currentStatus.code)
                                      .arg(m_currentStatus.phrase)
                                      .arg(m_currentStatus.description);
    clipboard->setText(text);
    QMessageBox::information(this, tr("成功"), tr("状态码描述已复制到剪贴板"));
}

void HttpStatusCode::onExportClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, 
        "导出HTTP状态码列表", 
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/http_status_codes.json",
        "JSON文件 (*.json);;CSV文件 (*.csv);;文本文件 (*.txt)");
    
    if (fileName.isEmpty()) return;
    
    exportToFile(fileName);
}

void HttpStatusCode::onRefreshClicked()
{
    initializeStatusCodes();
    filterStatusCodes();
    clearDetailsPanel();
    QMessageBox::information(this, tr("成功"), tr("数据已刷新"));
}

void HttpStatusCode::onQuickSearchClicked()
{
    QString searchTerm = searchEdit->text().trimmed();
    if (searchTerm.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请输入搜索关键词"));
        return;
    }
    performQuickSearch(searchTerm);
}

void HttpStatusCode::exportToFile(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "错误", "无法创建文件: " + fileName);
        return;
    }
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    
    if (fileName.endsWith(".json")) {
        // 导出为JSON格式
        QJsonArray jsonArray;
        for (const HttpStatusInfo &info : m_filteredStatusCodes) {
            QJsonObject obj;
            obj["code"] = info.code;
            obj["phrase"] = info.phrase;
            obj["description"] = info.description;
            obj["category"] = info.category;
            obj["categoryName"] = info.categoryName;
            obj["isCommon"] = info.isCommon;
            obj["rfc"] = info.rfc;
            obj["usage"] = info.usage;
            obj["examples"] = QJsonArray::fromStringList(info.examples);
            jsonArray.append(obj);
        }
        
        QJsonDocument doc(jsonArray);
        out << doc.toJson();
    } else if (fileName.endsWith(".csv")) {
        // 导出为CSV格式
        out << "状态码,状态短语,分类,描述,是否常用,RFC标准,使用场景\n";
        for (const HttpStatusInfo &info : m_filteredStatusCodes) {
            out << QString("%1,%2,%3,\"%4\",%5,%6,\"%7\"\n")
                   .arg(info.code)
                   .arg(info.phrase)
                   .arg(info.category)
                   .arg(info.description)
                   .arg(info.isCommon ? "是" : "否")
                   .arg(info.rfc)
                   .arg(info.usage);
        }
    } else {
        // 导出为文本格式
        out << "HTTP状态码列表\n";
        out << "================\n\n";
        
        QString currentCategory;
        for (const HttpStatusInfo &info : m_filteredStatusCodes) {
            if (info.categoryName != currentCategory) {
                currentCategory = info.categoryName;
                out << "\n" << currentCategory << ":\n";
                out << QString("-").repeated(currentCategory.length() + 1) << "\n";
            }
            
            out << QString("%1 %2\n").arg(info.code).arg(info.phrase);
            out << QString("  描述: %1\n").arg(info.description);
            if (!info.usage.isEmpty()) {
                out << QString("  使用场景: %1\n").arg(info.usage);
            }
            if (!info.rfc.isEmpty()) {
                out << QString("  RFC标准: %1\n").arg(info.rfc);
            }
            out << "\n";
        }
    }
    
    file.close();
    QMessageBox::information(this, "成功", QString("已导出 %1 个状态码到文件: %2")
                                         .arg(m_filteredStatusCodes.size())
                                         .arg(fileName));
}

void HttpStatusCode::performQuickSearch(const QString &searchTerm)
{
    // 重置所有筛选条件
    categoryCombo->setCurrentIndex(0);
    commonOnlyCheckBox->setChecked(false);
    show1xxCheckBox->setChecked(true);
    show2xxCheckBox->setChecked(true);
    show3xxCheckBox->setChecked(true);
    show4xxCheckBox->setChecked(true);
    show5xxCheckBox->setChecked(true);
    
    // 设置搜索文本并执行搜索
    searchEdit->setText(searchTerm);
    filterStatusCodes();
    
    // 如果只有一个结果，自动选中
    if (m_filteredStatusCodes.size() == 1) {
        statusTable->selectRow(0);
        updateDetailsPanel(m_filteredStatusCodes[0]);
    }
}