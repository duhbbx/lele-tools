#include "iplookuptool.h"
#include <QHostInfo>
#include <QNetworkInterface>
#include <QNetworkRequest>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

extern "C" {
#include "../../common/thirdparty/ip2region/xdb_api.h"
}

// 动态对象创建宏
REGISTER_DYNAMICOBJECT(IpLookupTool);

// ── ip2region 离线查询 ──

Ip2RegionLookup& Ip2RegionLookup::instance() {
    static Ip2RegionLookup inst;
    return inst;
}

Ip2RegionLookup::Ip2RegionLookup() {
    // 查找 xdb 数据库文件
    QStringList candidates = {
        QCoreApplication::applicationDirPath() + "/ip2region.xdb",
        QCoreApplication::applicationDirPath() + "/../Resources/ip2region.xdb",
    };
    // 开发模式
    QString srcPath = QString(__FILE__);
    QString srcDir = QFileInfo(srcPath).absolutePath();
    candidates.prepend(srcDir + "/../../common/thirdparty/ip2region/ip2region.xdb");

    for (const auto& p : candidates) {
        if (QFile::exists(p)) {
            xdb_content_t* content = xdb_load_content_from_file(p.toUtf8().constData());
            if (content) {
                m_content = content;
                m_loaded = true;
            }
            break;
        }
    }
}

Ip2RegionLookup::~Ip2RegionLookup() {
    if (m_content) {
        xdb_free_content(m_content);
    }
}

IpLookupResult Ip2RegionLookup::lookup(const QString& ip) {
    IpLookupResult result;
    result.ip = ip;

    if (!m_loaded || !m_content) {
        result.error = "ip2region database not loaded";
        return result;
    }

    xdb_searcher_t searcher;
    int err = xdb_new_with_buffer(XDB_IPv4, &searcher, static_cast<xdb_content_t*>(m_content));
    if (err != 0) {
        result.error = "Failed to create searcher";
        return result;
    }

    char region_buf[256] = {0};
    xdb_region_buffer_t region;
    xdb_region_buffer_init(&region, region_buf, sizeof(region_buf));

    err = xdb_search_by_string(&searcher, ip.toUtf8().constData(), &region);
    xdb_close(&searcher);

    if (err != 0) {
        result.error = "IP lookup failed";
        return result;
    }

    // 格式: "国家|省份|城市|ISP|ISO编码" 或 "国家|区域|省份|城市|ISP"
    QString regionStr = QString::fromUtf8(region.value);
    QStringList parts = regionStr.split('|');

    // ip2region v2 格式: 国家|区域|省份|城市|ISP
    if (parts.size() >= 5) {
        result.country = parts[0] == "0" ? "" : parts[0];
        result.region = parts[2] == "0" ? "" : parts[2];
        result.city = parts[3] == "0" ? "" : parts[3];
        result.isp = parts[4] == "0" ? "" : parts[4];
        // parts[1] 是区域（如"华南"），放到 organization
        result.organization = parts[1] == "0" ? "" : parts[1];
    }

    return result;
}

// SingleIpLookup 实现
SingleIpLookup::SingleIpLookup(QWidget* parent)
    : QWidget(parent)
      , m_currentReply(nullptr) {
    m_networkManager = new QNetworkAccessManager(this);
    setupUI();
}

void SingleIpLookup::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(8);
    m_mainLayout->setContentsMargins(12, 8, 12, 8);

    // 输入栏（一行：输入框 + 按钮）
    m_inputLayout = new QHBoxLayout();
    m_inputLayout->setSpacing(0);
    const int barH = 32;

    m_ipInput = new QLineEdit();
    m_ipInput->setPlaceholderText(tr("输入 IP 地址或域名，如 8.8.8.8 / google.com"));
    m_ipInput->setFixedHeight(barH);
    m_ipInput->setStyleSheet(
        "QLineEdit { border: 1px solid #dee2e6; border-radius: 4px 0 0 4px; padding: 0 10px; font-size: 13px; }");

    m_lookupBtn = new QPushButton(tr("查询"));
    m_lookupBtn->setFixedSize(60, barH);
    m_lookupBtn->setStyleSheet(
        "QPushButton { background: #228be6; color: white; border: none; border-radius: 0;"
        "  font-weight: 600; font-size: 12px; }"
        "QPushButton:hover { background: #1c7ed6; }"
        "QPushButton:disabled { background: #adb5bd; }");

    m_getMyIpBtn = new QPushButton(tr("我的IP"));
    m_getMyIpBtn->setFixedSize(60, barH);
    m_getMyIpBtn->setStyleSheet(
        "QPushButton { background: #40c057; color: white; border: none; border-radius: 0 4px 4px 0;"
        "  font-size: 12px; }"
        "QPushButton:hover { background: #37b24d; }");

    m_clearBtn = new QPushButton(tr("清空"));
    m_clearBtn->setFixedHeight(barH);
    m_clearBtn->setStyleSheet("QPushButton { color: #868e96; font-size: 12px; padding: 0 8px; }");

    m_inputLayout->addWidget(m_ipInput, 1);
    m_inputLayout->addWidget(m_lookupBtn);
    m_inputLayout->addWidget(m_getMyIpBtn);
    m_inputLayout->addSpacing(8);
    m_inputLayout->addWidget(m_clearBtn);

    // 离线数据库状态
    auto* dbStatus = new QLabel();
    if (Ip2RegionLookup::instance().isAvailable()) {
        dbStatus->setText(tr("ip2region 离线库已加载"));
        dbStatus->setStyleSheet("color: #40c057; font-size: 11px; padding: 2px 0;");
    } else {
        dbStatus->setText(tr("离线库未加载，使用在线查询"));
        dbStatus->setStyleSheet("color: #868e96; font-size: 11px; padding: 2px 0;");
    }

    // 结果区域（扁平化，用 QGridLayout 确保左对齐）
    m_resultGroup = new QGroupBox();
    auto* grid = new QGridLayout(m_resultGroup);
    grid->setSpacing(6);
    grid->setContentsMargins(0, 4, 0, 0);
    grid->setColumnMinimumWidth(0, 50);
    m_resultLayout = nullptr; // 不再使用 QFormLayout

    m_ipLabel = new QLabel("-");
    m_countryLabel = new QLabel("-");
    m_regionLabel = new QLabel("-");
    m_cityLabel = new QLabel("-");
    m_ispLabel = new QLabel("-");
    m_orgLabel = new QLabel("-");
    m_timezoneLabel = new QLabel("-");
    m_coordinatesLabel = new QLabel("-");
    m_asnLabel = new QLabel("-");

    auto setValStyle = [](QLabel* label) {
        label->setStyleSheet("QLabel { padding: 4px 8px; background: #f8f9fa; border-radius: 4px; color: #212529; }");
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    };
    for (auto* l : {m_ipLabel, m_countryLabel, m_regionLabel, m_cityLabel,
                    m_ispLabel, m_orgLabel, m_timezoneLabel, m_coordinatesLabel, m_asnLabel})
        setValStyle(l);

    auto makeKey = [](const QString& text) {
        auto* l = new QLabel(text);
        l->setStyleSheet("color: #868e96; font-size: 12px;");
        l->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        l->setFixedWidth(50);
        return l;
    };

    struct { QString name; QLabel* val; } rows[] = {
        {tr("IP"), m_ipLabel}, {tr("国家"), m_countryLabel}, {tr("省/州"), m_regionLabel},
        {tr("城市"), m_cityLabel}, {tr("ISP"), m_ispLabel}, {tr("组织"), m_orgLabel},
        {tr("时区"), m_timezoneLabel}, {tr("坐标"), m_coordinatesLabel}, {tr("ASN"), m_asnLabel}
    };
    for (int i = 0; i < 9; ++i) {
        grid->addWidget(makeKey(rows[i].name), i, 0);
        grid->addWidget(rows[i].val, i, 1);
    }
    grid->setColumnStretch(1, 1);

    // 复制按钮
    m_buttonLayout = new QHBoxLayout();
    m_copyResultBtn = new QPushButton(tr("复制结果"));
    m_copyResultBtn->setStyleSheet(
        "QPushButton { color: #228be6; font-size: 12px; padding: 4px 10px; }"
        "QPushButton:hover { background: #e7f5ff; border-radius: 4px; }");

    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_copyResultBtn);

    // 组装布局
    m_mainLayout->addLayout(m_inputLayout);
    m_mainLayout->addWidget(dbStatus);
    m_mainLayout->addWidget(m_resultGroup);
    m_mainLayout->addLayout(m_buttonLayout);
    m_mainLayout->addStretch();

    // 连接信号
    connect(m_lookupBtn, &QPushButton::clicked, this, &SingleIpLookup::onLookupClicked);
    connect(m_clearBtn, &QPushButton::clicked, this, &SingleIpLookup::onClearClicked);
    connect(m_copyResultBtn, &QPushButton::clicked, this, &SingleIpLookup::onCopyResultClicked);
    connect(m_getMyIpBtn, &QPushButton::clicked, this, &SingleIpLookup::onGetMyIpClicked);
    connect(m_ipInput, &QLineEdit::returnPressed, this, &SingleIpLookup::onLookupClicked);
}

void SingleIpLookup::onLookupClicked() {
    QString input = m_ipInput->text().trimmed();
    if (input.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入IP地址或域名");
        return;
    }

    // 如果不是IP地址，尝试解析域名
    if (!isValidIp(input)) {
        QString resolvedIp = resolveHostname(input);
        if (resolvedIp.isEmpty()) {
            displayError("无法解析域名: " + input);
            return;
        }
        input = resolvedIp;
    }

    lookupIp(input);
}

void SingleIpLookup::onClearClicked() {
    m_ipInput->clear();
    m_ipLabel->setText("-");
    m_countryLabel->setText("-");
    m_regionLabel->setText("-");
    m_cityLabel->setText("-");
    m_ispLabel->setText("-");
    m_orgLabel->setText("-");
    m_timezoneLabel->setText("-");
    m_coordinatesLabel->setText("-");
    m_asnLabel->setText("-");
}

void SingleIpLookup::onCopyResultClicked() {
    QString result = QString("IP地址: %1\n国家: %2\n省/州: %3\n城市: %4\nISP: %5\n组织: %6\n时区: %7\n坐标: %8\nASN: %9")
                     .arg(m_ipLabel->text())
                     .arg(m_countryLabel->text())
                     .arg(m_regionLabel->text())
                     .arg(m_cityLabel->text())
                     .arg(m_ispLabel->text())
                     .arg(m_orgLabel->text())
                     .arg(m_timezoneLabel->text())
                     .arg(m_coordinatesLabel->text())
                     .arg(m_asnLabel->text());

    QApplication::clipboard()->setText(result);
    QMessageBox::information(this, "成功", "查询结果已复制到剪贴板");
}

void SingleIpLookup::onGetMyIpClicked() {
    // 获取外网IP
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    m_lookupBtn->setEnabled(false);
    m_getMyIpBtn->setEnabled(false);
    m_lookupBtn->setText(tr("获取中..."));

    QNetworkRequest networkRequest(QUrl("https://api.ipify.org"));
    auto* reply = m_networkManager->get(networkRequest);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_lookupBtn->setEnabled(true);
        m_getMyIpBtn->setEnabled(true);
        m_lookupBtn->setText(tr("查询"));

        if (reply->error() == QNetworkReply::NoError) {
            QString myIp = QString::fromUtf8(reply->readAll()).trimmed();
            m_ipInput->setText(myIp);
            lookupIp(myIp);
        } else {
            displayError(tr("获取外网IP失败: ") + reply->errorString());
        }
    });
}

void SingleIpLookup::lookupIp(const QString& ip) {
    m_currentIp = ip;

    // 优先使用 ip2region 离线查询（微秒级）
    auto& db = Ip2RegionLookup::instance();
    if (db.isAvailable()) {
        IpLookupResult result = db.lookup(ip);
        if (result.error.isEmpty() && !result.country.isEmpty()) {
            displayResult(result);
            m_lookupBtn->setEnabled(true);
            m_lookupBtn->setText(tr("查询"));
            return;
        }
    }

    // 回退到在线查询
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
    }

    m_lookupBtn->setEnabled(false);
    m_lookupBtn->setText(tr("查询中..."));

    QString url = QString("http://ip-api.com/json/%1?fields=status,message,country,regionName,city,isp,org,timezone,lat,lon,as,query").arg(ip);
    QNetworkRequest networkRequest { QUrl(url) };
    networkRequest.setRawHeader(QByteArray("User-Agent"), QByteArray("IP Lookup Tool 1.0"));

    m_currentReply = m_networkManager->get(networkRequest);
    connect(m_currentReply, &QNetworkReply::finished, this, &SingleIpLookup::onNetworkReplyFinished);
}

void SingleIpLookup::onNetworkReplyFinished() {
    m_lookupBtn->setEnabled(true);
    m_lookupBtn->setText(tr("查询"));

    if (!m_currentReply)
        return;

    if (m_currentReply->error() == QNetworkReply::NoError) {
        QByteArray data = m_currentReply->readAll();
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(data, &error);

        if (error.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject obj = doc.object();

            if (obj.value("status").toString() == "success") {
                IpLookupResult result;
                result.ip = obj.value("query").toString();
                result.country = obj.value("country").toString();
                result.region = obj.value("regionName").toString();
                result.city = obj.value("city").toString();
                result.isp = obj.value("isp").toString();
                result.organization = obj.value("org").toString();
                result.timezone = obj.value("timezone").toString();
                result.latitude = QString::number(obj.value("lat").toDouble());
                result.longitude = QString::number(obj.value("lon").toDouble());
                result.asn = obj.value("as").toString();

                displayResult(result);
            } else {
                displayError(obj.value("message").toString());
            }
        } else {
            displayError("解析响应数据失败");
        }
    } else {
        displayError("网络请求失败: " + m_currentReply->errorString());
    }

    m_currentReply->deleteLater();
    m_currentReply = nullptr;
}

void SingleIpLookup::displayResult(const IpLookupResult& result) {
    m_ipLabel->setText(result.ip);
    m_countryLabel->setText(result.country.isEmpty() ? "-" : result.country);
    m_regionLabel->setText(result.region.isEmpty() ? "-" : result.region);
    m_cityLabel->setText(result.city.isEmpty() ? "-" : result.city);
    m_ispLabel->setText(result.isp.isEmpty() ? "-" : result.isp);
    m_orgLabel->setText(result.organization.isEmpty() ? "-" : result.organization);
    m_timezoneLabel->setText(result.timezone.isEmpty() ? "-" : result.timezone);

    QString coordinates = "-";
    if (!result.latitude.isEmpty() && !result.longitude.isEmpty()) {
        coordinates = QString("%1, %2").arg(result.latitude, result.longitude);
    }
    m_coordinatesLabel->setText(coordinates);

    m_asnLabel->setText(result.asn.isEmpty() ? "-" : result.asn);
}

void SingleIpLookup::displayError(const QString& error) {
    m_ipLabel->setText("查询失败");
    m_countryLabel->setText(error);
    m_regionLabel->setText("-");
    m_cityLabel->setText("-");
    m_ispLabel->setText("-");
    m_orgLabel->setText("-");
    m_timezoneLabel->setText("-");
    m_coordinatesLabel->setText("-");
    m_asnLabel->setText("-");
}

bool SingleIpLookup::isValidIp(const QString& ip) {
    QRegularExpression ipRegex(R"(^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)");
    return ipRegex.match(ip).hasMatch();
}

QString SingleIpLookup::resolveHostname(const QString& hostname) {
    QHostInfo hostInfo = QHostInfo::fromName(hostname);
    if (hostInfo.error() == QHostInfo::NoError && !hostInfo.addresses().isEmpty()) {
        for (const QHostAddress& address : hostInfo.addresses()) {
            if (address.protocol() == QAbstractSocket::IPv4Protocol) {
                return address.toString();
            }
        }
    }
    return QString();
}

// BatchIpLookup 实现
BatchIpLookup::BatchIpLookup(QWidget* parent)
    : QWidget(parent)
      , m_currentIndex(0)
      , m_isRunning(false)
      , m_currentReply(nullptr) {
    m_networkManager = new QNetworkAccessManager(this);
    m_batchTimer = new QTimer(this);
    m_batchTimer->setSingleShot(true);
    connect(m_batchTimer, &QTimer::timeout, this, &BatchIpLookup::processNextIp);

    setupUI();
}

void BatchIpLookup::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(6);
    m_mainLayout->setContentsMargins(8, 6, 8, 6);

    // 控制栏
    m_controlLayout = new QHBoxLayout();
    m_controlLayout->setSpacing(4);

    m_startBtn = new QPushButton(tr("开始查询"));
    m_startBtn->setStyleSheet(
        "QPushButton { background: #228be6; color: white; border: none; border-radius: 4px;"
        "  padding: 5px 14px; font-weight: 600; font-size: 12px; }"
        "QPushButton:hover { background: #1c7ed6; }"
        "QPushButton:disabled { background: #adb5bd; }");

    m_stopBtn = new QPushButton(tr("停止"));
    m_stopBtn->setEnabled(false);
    m_stopBtn->setStyleSheet(
        "QPushButton { color: #e03131; font-size: 12px; padding: 5px 10px; }"
        "QPushButton:hover { background: #fff5f5; border-radius: 4px; }"
        "QPushButton:disabled { color: #adb5bd; }");

    m_importBtn = new QPushButton(tr("导入文件"));
    m_importBtn->setStyleSheet("QPushButton { color: #495057; font-size: 12px; padding: 5px 10px; }"
        "QPushButton:hover { background: #f1f3f5; border-radius: 4px; }");

    m_clearAllBtn = new QPushButton(tr("清空"));
    m_clearAllBtn->setStyleSheet("QPushButton { color: #868e96; font-size: 12px; padding: 5px 8px; }"
        "QPushButton:hover { background: #f1f3f5; border-radius: 4px; }");

    m_progressBar = new QProgressBar();
    m_progressBar->setFixedHeight(6);
    m_progressBar->setTextVisible(false);
    m_progressBar->setStyleSheet(
        "QProgressBar { border: none; border-radius: 3px; background: #e9ecef; }"
        "QProgressBar::chunk { background: #228be6; border-radius: 3px; }");

    m_statusLabel = new QLabel(tr("就绪"));
    m_statusLabel->setStyleSheet("color: #868e96; font-size: 11px;");

    m_controlLayout->addWidget(m_startBtn);
    m_controlLayout->addWidget(m_stopBtn);
    m_controlLayout->addWidget(m_importBtn);
    m_controlLayout->addWidget(m_clearAllBtn);
    m_controlLayout->addWidget(m_progressBar, 1);
    m_controlLayout->addWidget(m_statusLabel);

    m_mainLayout->addLayout(m_controlLayout);

    // 主分割器（左：输入，右：结果）
    m_mainSplitter = new QSplitter(Qt::Horizontal);

    // 输入区域
    m_inputWidget = new QWidget();
    m_inputLayout = new QVBoxLayout(m_inputWidget);
    m_inputLayout->setContentsMargins(0, 0, 0, 0);
    m_inputLayout->setSpacing(2);

    m_inputLabel = new QLabel(tr("IP 列表（每行一个，或导入文件自动提取）"));
    m_inputLabel->setStyleSheet("color: #868e96; font-size: 11px; padding: 2px 0;");

    m_ipListInput = new QPlainTextEdit();
    m_ipListInput->setPlaceholderText("8.8.8.8\n1.1.1.1\n114.114.114.114\n...");
#ifdef Q_OS_MACOS
    m_ipListInput->setFont(QFont("Menlo", 12));
#else
    m_ipListInput->setFont(QFont("Consolas", 10));
#endif
    m_ipListInput->setStyleSheet(
        "QPlainTextEdit { border: 1px solid #dee2e6; border-radius: 4px; padding: 6px; }");

    m_inputButtonLayout = new QHBoxLayout();
    // 按钮已移到控制栏

    m_inputLayout->addWidget(m_inputLabel);
    m_inputLayout->addWidget(m_ipListInput);

    // 结果区域
    m_resultWidget = new QWidget();
    m_resultLayout = new QVBoxLayout(m_resultWidget);
    m_resultLayout->setContentsMargins(0, 0, 0, 0);
    m_resultLayout->setSpacing(4);

    m_resultTable = new QTableWidget();
    m_resultTable->setColumnCount(6);
    m_resultTable->setHorizontalHeaderLabels({tr("IP"), tr("国家"), tr("省/州"), tr("城市"), tr("ISP"), tr("状态")});
    m_resultTable->horizontalHeader()->setStretchLastSection(false);
    m_resultTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_resultTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_resultTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_resultTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_resultTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_resultTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_resultTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultTable->setAlternatingRowColors(true);
    m_resultTable->verticalHeader()->setDefaultSectionSize(26);

    m_resultButtonLayout = new QHBoxLayout();
    m_resultButtonLayout->setSpacing(4);

    m_exportBtn = new QPushButton(tr("导出"));
    m_exportBtn->setStyleSheet("QPushButton { color: #495057; font-size: 11px; padding: 3px 8px; }"
        "QPushButton:hover { background: #f1f3f5; border-radius: 4px; }");

    m_copyAllBtn = new QPushButton(tr("复制全部"));
    m_copyAllBtn->setStyleSheet("QPushButton { color: #228be6; font-size: 11px; padding: 3px 8px; }"
        "QPushButton:hover { background: #e7f5ff; border-radius: 4px; }");

    m_resultButtonLayout->addStretch();
    m_resultButtonLayout->addWidget(m_exportBtn);
    m_resultButtonLayout->addWidget(m_copyAllBtn);

    m_resultLayout->addWidget(m_resultTable);
    m_resultLayout->addLayout(m_resultButtonLayout);

    // 组装
    m_mainSplitter->addWidget(m_inputWidget);
    m_mainSplitter->addWidget(m_resultWidget);
    m_mainSplitter->setSizes({250, 550});
    m_mainSplitter->setChildrenCollapsible(false);

    m_mainLayout->addWidget(m_mainSplitter, 1);

    // 连接信号
    connect(m_startBtn, &QPushButton::clicked, this, &BatchIpLookup::onStartBatchClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &BatchIpLookup::onStopBatchClicked);
    connect(m_clearAllBtn, &QPushButton::clicked, this, &BatchIpLookup::onClearAllClicked);
    connect(m_importBtn, &QPushButton::clicked, this, &BatchIpLookup::onImportFromFileClicked);
    connect(m_exportBtn, &QPushButton::clicked, this, &BatchIpLookup::onExportResultsClicked);
    connect(m_copyAllBtn, &QPushButton::clicked, this, &BatchIpLookup::onCopyAllResultsClicked);
}

void BatchIpLookup::onStartBatchClicked() {
    QString text = m_ipListInput->toPlainText().trimmed();
    if (text.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入要查询的IP地址列表");
        return;
    }

    m_ipList = parseIpList(text);
    if (m_ipList.isEmpty()) {
        QMessageBox::warning(this, "提示", "没有找到有效的IP地址或域名");
        return;
    }

    startBatchLookup();
}

void BatchIpLookup::onStopBatchClicked() {
    stopBatchLookup();
}

void BatchIpLookup::onClearAllClicked() {
    m_ipListInput->clear();
    m_resultTable->setRowCount(0);
    m_results.clear();
}

void BatchIpLookup::onImportFromFileClicked() {
    QString fileName = QFileDialog::getOpenFileName(this, tr("导入文件"),
        "", tr("所有文件 (*);;文本文件 (*.txt *.log *.csv)"));
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("错误"), tr("无法打开文件: ") + fileName);
        return;
    }

    QString content = QTextStream(&file).readAll();
    file.close();

    // 用正则从任意格式文件中提取所有 IPv4 地址
    QRegularExpression ipRe(R"(\b(?:(?:25[0-5]|2[0-4]\d|[01]?\d\d?)\.){3}(?:25[0-5]|2[0-4]\d|[01]?\d\d?)\b)");
    auto it = ipRe.globalMatch(content);

    QStringList ips;
    QSet<QString> seen;
    while (it.hasNext()) {
        QString ip = it.next().captured(0);
        if (!seen.contains(ip)) {
            seen.insert(ip);
            ips.append(ip);
        }
    }

    if (ips.isEmpty()) {
        QMessageBox::information(this, tr("提示"),
            tr("未从文件中提取到 IP 地址\n\n文件: %1\n大小: %2 字节")
                .arg(QFileInfo(fileName).fileName())
                .arg(content.size()));
        return;
    }

    m_ipListInput->setPlainText(ips.join('\n'));
    m_statusLabel->setText(tr("已从文件提取 %1 个唯一 IP").arg(ips.size()));
}

void BatchIpLookup::onExportResultsClicked() {
    if (m_results.isEmpty()) {
        QMessageBox::information(this, "提示", "没有查询结果可以导出");
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this, "导出查询结果", "ip_lookup_results.txt", "文本文件 (*.txt);;CSV文件 (*.csv)");
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);

            // 写入标题行
            if (fileName.endsWith(".csv")) {
                out << "IP地址,国家,省/州,城市,ISP,组织,时区,坐标,ASN,状态\n";
            } else {
                out << "IP地址查询结果\n";
                out << "================\n\n";
            }

            // 写入数据
            for (const auto& pair : m_results) {
                const QString& ip = pair.first;
                const IpLookupResult& result = pair.second;

                if (fileName.endsWith(".csv")) {
                    out << QString("%1,%2,%3,%4,%5,%6,%7,\"%8,%9\",%10,%11\n")
                           .arg(ip)
                           .arg(result.country)
                           .arg(result.region)
                           .arg(result.city)
                           .arg(result.isp)
                           .arg(result.organization)
                           .arg(result.timezone)
                           .arg(result.latitude)
                           .arg(result.longitude)
                           .arg(result.asn)
                           .arg(result.isValid() ? "成功" : "失败");
                } else {
                    out << QString("IP: %1\n").arg(ip);
                    if (result.isValid()) {
                        out << QString("  国家: %1\n").arg(result.country);
                        out << QString("  省/州: %1\n").arg(result.region);
                        out << QString("  城市: %1\n").arg(result.city);
                        out << QString("  ISP: %1\n").arg(result.isp);
                        out << QString("  组织: %1\n").arg(result.organization);
                        out << QString("  时区: %1\n").arg(result.timezone);
                        out << QString("  坐标: %1, %2\n").arg(result.latitude, result.longitude);
                        out << QString("  ASN: %1\n").arg(result.asn);
                    } else {
                        out << QString("  错误: %1\n").arg(result.error);
                    }
                    out << "\n";
                }
            }

            QMessageBox::information(this, "成功", "查询结果已导出到: " + fileName);
        } else {
            QMessageBox::warning(this, "错误", "无法写入文件: " + fileName);
        }
    }
}

void BatchIpLookup::onCopyAllResultsClicked() {
    if (m_results.isEmpty()) {
        QMessageBox::information(this, "提示", "没有查询结果可以复制");
        return;
    }

    QString text = "IP地址查询结果\n================\n\n";
    for (const auto& pair : m_results) {
        const QString& ip = pair.first;
        const IpLookupResult& result = pair.second;

        text += QString("IP: %1\n").arg(ip);
        if (result.isValid()) {
            text += QString("  归属地: %1\n").arg(result.toString());
            text += QString("  ISP: %1\n").arg(result.isp);
        } else {
            text += QString("  错误: %1\n").arg(result.error);
        }
        text += "\n";
    }

    QApplication::clipboard()->setText(text);
    QMessageBox::information(this, "成功", "查询结果已复制到剪贴板");
}

void BatchIpLookup::startBatchLookup() {
    m_isRunning = true;
    m_currentIndex = 0;
    m_results.clear();
    m_resultTable->setRowCount(0);

    m_startBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_progressBar->setMaximum(m_ipList.size());
    m_progressBar->setValue(0);
    m_statusLabel->setText("正在查询...");

    processNextIp();
}

void BatchIpLookup::stopBatchLookup() {
    m_isRunning = false;
    m_batchTimer->stop();

    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    m_startBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_statusLabel->setText("已停止查询");
}

void BatchIpLookup::processNextIp() {
    if (!m_isRunning || m_currentIndex >= m_ipList.size()) {
        // 批量查询完成
        m_isRunning = false;
        m_startBtn->setEnabled(true);
        m_stopBtn->setEnabled(false);
        m_statusLabel->setText(QString("查询完成，共处理 %1 个地址").arg(m_ipList.size()));
        return;
    }

    QString currentIp = m_ipList[m_currentIndex];

    // 如果不是IP地址，尝试解析域名
    if (!isValidIp(currentIp)) {
        QString resolvedIp = resolveHostname(currentIp);
        if (!resolvedIp.isEmpty()) {
            currentIp = resolvedIp;
        }
    }

    lookupNextIp();
}

void BatchIpLookup::lookupNextIp() {
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
    }

    QString ip = m_ipList[m_currentIndex];
    QString actualIp = ip;

    // 域名解析
    if (!isValidIp(ip)) {
        actualIp = resolveHostname(ip);
        if (actualIp.isEmpty()) {
            IpLookupResult errorResult;
            errorResult.error = "域名解析失败";
            displayResult(ip, errorResult);

            m_currentIndex++;
            updateProgress();
            m_batchTimer->start(100); // 100ms延迟，避免请求过快
            return;
        }
    }

    m_statusLabel->setText(QString("正在查询 %1 (%2/%3)").arg(ip).arg(m_currentIndex + 1).arg(m_ipList.size()));

    // 优先使用 ip2region 离线查询
    auto& db = Ip2RegionLookup::instance();
    if (db.isAvailable()) {
        IpLookupResult result = db.lookup(actualIp);
        if (result.error.isEmpty() && !result.country.isEmpty()) {
            displayResult(ip, result);
            m_currentIndex++;
            updateProgress();
            m_batchTimer->start(10); // 离线查询极快，几乎无延迟
            return;
        }
    }

    // 回退到 API 查询
    QString url = QString("http://ip-api.com/json/%1?fields=status,message,country,regionName,city,isp,org,timezone,lat,lon,as,query").arg(actualIp);
    QNetworkRequest networkRequest { QUrl(url) };
    networkRequest.setRawHeader(QByteArray("User-Agent"), QByteArray("IP Lookup Tool 1.0"));

    m_currentReply = m_networkManager->get(networkRequest);
    connect(m_currentReply, &QNetworkReply::finished, this, &BatchIpLookup::onNetworkReplyFinished);
}

void BatchIpLookup::onNetworkReplyFinished() {
    if (!m_currentReply || !m_isRunning)
        return;

    QString originalIp = m_ipList[m_currentIndex];
    IpLookupResult result;

    if (m_currentReply->error() == QNetworkReply::NoError) {
        QByteArray data = m_currentReply->readAll();
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(data, &error);

        if (error.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject obj = doc.object();

            if (obj.value("status").toString() == "success") {
                result.ip = obj.value("query").toString();
                result.country = obj.value("country").toString();
                result.region = obj.value("regionName").toString();
                result.city = obj.value("city").toString();
                result.isp = obj.value("isp").toString();
                result.organization = obj.value("org").toString();
                result.timezone = obj.value("timezone").toString();
                result.latitude = QString::number(obj.value("lat").toDouble());
                result.longitude = QString::number(obj.value("lon").toDouble());
                result.asn = obj.value("as").toString();
            } else {
                result.error = obj.value("message").toString();
            }
        } else {
            result.error = "解析响应数据失败";
        }
    } else {
        result.error = "网络请求失败: " + m_currentReply->errorString();
    }

    displayResult(originalIp, result);

    m_currentReply->deleteLater();
    m_currentReply = nullptr;

    m_currentIndex++;
    updateProgress();

    // 继续下一个查询，添加延迟避免请求过快
    m_batchTimer->start(500); // 500ms延迟
}

void BatchIpLookup::displayResult(const QString& ip, const IpLookupResult& result) {
    m_results.append({ ip, result });

    int row = m_resultTable->rowCount();
    m_resultTable->insertRow(row);

    m_resultTable->setItem(row, 0, new QTableWidgetItem(ip));
    m_resultTable->setItem(row, 1, new QTableWidgetItem(result.country));
    m_resultTable->setItem(row, 2, new QTableWidgetItem(result.region));
    m_resultTable->setItem(row, 3, new QTableWidgetItem(result.city));
    m_resultTable->setItem(row, 4, new QTableWidgetItem(result.isp));

    QString status = result.isValid() ? "成功" : ("失败: " + result.error);
    auto* statusItem = new QTableWidgetItem(status);

    if (result.isValid()) {
        statusItem->setForeground(QBrush(QColor("#27ae60")));
    } else {
        statusItem->setForeground(QBrush(QColor("#e74c3c")));
    }

    m_resultTable->setItem(row, 5, statusItem);

    // 滚动到最新行
    m_resultTable->scrollToBottom();
}

void BatchIpLookup::updateProgress() {
    m_progressBar->setValue(m_currentIndex);
}

QStringList BatchIpLookup::parseIpList(const QString& text) {
    // 先尝试按行解析（每行一个 IP 或域名）
    QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    QStringList result;
    QSet<QString> seen;

    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;

        // 如果整行是一个合法 IP 或域名，直接加入
        if (isValidIp(line) || !line.contains(' ')) {
            if (!seen.contains(line)) {
                seen.insert(line);
                result.append(line);
            }
            continue;
        }

        // 否则用正则从行中提取 IP
        QRegularExpression ipRe(R"(\b(?:(?:25[0-5]|2[0-4]\d|[01]?\d\d?)\.){3}(?:25[0-5]|2[0-4]\d|[01]?\d\d?)\b)");
        auto it = ipRe.globalMatch(line);
        while (it.hasNext()) {
            QString ip = it.next().captured(0);
            if (!seen.contains(ip)) {
                seen.insert(ip);
                result.append(ip);
            }
        }
    }

    return result;
}

bool BatchIpLookup::isValidIp(const QString& ip) {
    QRegularExpression ipRegex(R"(^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)");
    return ipRegex.match(ip).hasMatch();
}

QString BatchIpLookup::resolveHostname(const QString& hostname) {
    QHostInfo hostInfo = QHostInfo::fromName(hostname);
    if (hostInfo.error() == QHostInfo::NoError && !hostInfo.addresses().isEmpty()) {
        for (const QHostAddress& address : hostInfo.addresses()) {
            if (address.protocol() == QAbstractSocket::IPv4Protocol) {
                return address.toString();
            }
        }
    }
    return QString();
}

// IpLookupTool 主类实现
IpLookupTool::IpLookupTool(QWidget* parent) : QWidget(parent), DynamicObjectBase() {
    setupUI();
}

void IpLookupTool::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);

    // 创建标签页控件
    m_tabWidget = new QTabWidget();

    // 创建单个查询标签页
    m_singleLookup = new SingleIpLookup();
    m_tabWidget->addTab(m_singleLookup, tr("单个查询"));

    // 创建批量查询标签页
    m_batchLookup = new BatchIpLookup();
    m_tabWidget->addTab(m_batchLookup, tr("批量查询"));

    m_mainLayout->addWidget(m_tabWidget);
}

// IpApiClient 实现
IpApiClient::IpApiClient(QObject* parent)
    : QObject(parent)
      , m_currentReply(nullptr)
      , m_currentProvider(IpApiProvider::IpApi) {
    m_networkManager = new QNetworkAccessManager(this);
}

void IpApiClient::lookupIp(const QString& ip, IpApiProvider provider) {
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    m_currentProvider = provider;
    QString url = getApiUrl(ip, provider);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "IpLookupTool/1.0");
    request.setRawHeader("Accept", "application/json");

    m_currentReply = m_networkManager->get(request);
    connect(m_currentReply, &QNetworkReply::finished, this, &IpApiClient::onNetworkReplyFinished);
}

void IpApiClient::getMyPublicIp() {
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    QNetworkRequest request(QUrl("https://api.ipify.org"));
    request.setHeader(QNetworkRequest::UserAgentHeader, "IpLookupTool/1.0");

    m_currentReply = m_networkManager->get(request);
    connect(m_currentReply, &QNetworkReply::finished, [this]() {
        if (m_currentReply->error() == QNetworkReply::NoError) {
            QString ip = QString::fromUtf8(m_currentReply->readAll()).trimmed();
            emit myIpDetected(ip);
        } else {
            emit errorOccurred("无法获取公网IP地址: " + m_currentReply->errorString());
        }
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    });
}

void IpApiClient::onNetworkReplyFinished() {
    if (!m_currentReply)
        return;

    if (m_currentReply->error() == QNetworkReply::NoError) {
        QByteArray data = m_currentReply->readAll();
        IpLookupResult result = parseResponse(data, m_currentProvider);
        emit lookupFinished(result);
    } else {
        IpLookupResult result;
        result.error = QString("网络错误: %1").arg(m_currentReply->errorString());
        emit lookupFinished(result);
    }

    m_currentReply->deleteLater();
    m_currentReply = nullptr;
}

QString IpApiClient::getApiUrl(const QString& ip, IpApiProvider provider) {
    switch (provider) {
    case IpApiProvider::IpApi:
        return QString("http://ip-api.com/json/%1?fields=status,message,country,regionName,city,isp,org,timezone,lat,lon,as,query").arg(ip);
    case IpApiProvider::IpInfo:
        return QString("https://ipinfo.io/%1/json").arg(ip);
    case IpApiProvider::IpStack:
        return QString("http://api.ipstack.com/%1?access_key=YOUR_API_KEY").arg(ip);
    case IpApiProvider::IpGeolocation:
        return QString("https://api.ipgeolocation.io/ipgeo?apiKey=YOUR_API_KEY&ip=%1").arg(ip);
    case IpApiProvider::FreeGeoIp:
        return QString("https://freegeoip.app/json/%1").arg(ip);
    case IpApiProvider::IpData:
        return QString("https://api.ipdata.co/%1?api-key=YOUR_API_KEY").arg(ip);
    default:
        return QString("http://ip-api.com/json/%1").arg(ip);
    }
}

IpLookupResult IpApiClient::parseResponse(const QByteArray& data, IpApiProvider provider) {
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError) {
        IpLookupResult result;
        result.error = QString("JSON解析错误: %1").arg(error.errorString());
        return result;
    }

    QJsonObject json = doc.object();

    switch (provider) {
    case IpApiProvider::IpApi:
        return parseIpApiResponse(json);
    case IpApiProvider::IpInfo:
        return parseIpInfoResponse(json);
    default:
        return parseIpApiResponse(json);
    }
}

IpLookupResult IpApiClient::parseIpApiResponse(const QJsonObject& json) {
    IpLookupResult result;

    QString status = json["status"].toString();
    if (status == "fail") {
        result.error = json["message"].toString();
        return result;
    }

    result.ip = json["query"].toString();
    result.country = json["country"].toString();
    result.region = json["regionName"].toString();
    result.city = json["city"].toString();
    result.isp = json["isp"].toString();
    result.organization = json["org"].toString();
    result.timezone = json["timezone"].toString();
    result.latitude = QString::number(json["lat"].toDouble());
    result.longitude = QString::number(json["lon"].toDouble());
    result.asn = json["as"].toString();

    return result;
}

IpLookupResult IpApiClient::parseIpInfoResponse(const QJsonObject& json) {
    IpLookupResult result;

    if (json.contains("error")) {
        result.error = json["error"].toObject()["message"].toString();
        return result;
    }

    result.ip = json["ip"].toString();
    result.city = json["city"].toString();
    result.region = json["region"].toString();
    result.country = json["country"].toString();
    result.organization = json["org"].toString();
    result.timezone = json["timezone"].toString();

    QString loc = json["loc"].toString();
    if (!loc.isEmpty()) {
        QStringList coords = loc.split(',');
        if (coords.size() == 2) {
            result.latitude = coords[0];
            result.longitude = coords[1];
        }
    }

    return result;
}
