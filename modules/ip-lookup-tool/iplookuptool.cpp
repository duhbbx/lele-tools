#include "iplookuptool.h"
#include <QHostInfo>
#include <QNetworkInterface>
#include <QNetworkRequest>

// 动态对象创建宏
REGISTER_DYNAMICOBJECT(IpLookupTool);

// SingleIpLookup 实现
SingleIpLookup::SingleIpLookup(QWidget* parent)
    : QWidget(parent)
    , m_currentReply(nullptr) {

    m_networkManager = new QNetworkAccessManager(this);
    setupUI();
}

void SingleIpLookup::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(15);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);

    // 输入区域
    m_inputLayout = new QHBoxLayout();

    auto* inputLabel = new QLabel("请输入IP地址或域名:");
    inputLabel->setStyleSheet("font-weight: bold; color: #2c3e50;");

    m_ipInput = new QLineEdit();
    m_ipInput->setPlaceholderText("例如: 8.8.8.8 或 google.com");
    m_ipInput->setStyleSheet(
        "QLineEdit {"
        "padding: 8px;"
        "border: 2px solid #bdc3c7;"
        "border-radius: 6px;"
        "font-size: 14px;"
        "}"
        "QLineEdit:focus {"
        "border-color: #3498db;"
        "}"
    );

    m_lookupBtn = new QPushButton("🔍 查询");
    m_lookupBtn->setStyleSheet(
        "QPushButton {"
        "background-color: #3498db;"
        "color: white;"
        "border: none;"
        "padding: 8px 16px;"
        "border-radius: 6px;"
        "font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "background-color: #2980b9;"
        "}"
        "QPushButton:pressed {"
        "background-color: #21618c;"
        "}"
    );

    m_clearBtn = new QPushButton("🗑️ 清空");
    m_clearBtn->setStyleSheet(
        "QPushButton {"
        "background-color: #95a5a6;"
        "color: white;"
        "border: none;"
        "padding: 8px 16px;"
        "border-radius: 6px;"
        "font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "background-color: #7f8c8d;"
        "}"
    );

    m_getMyIpBtn = new QPushButton("🌐 获取我的IP");
    m_getMyIpBtn->setStyleSheet(
        "QPushButton {"
        "background-color: #27ae60;"
        "color: white;"
        "border: none;"
        "padding: 8px 16px;"
        "border-radius: 6px;"
        "font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "background-color: #219a52;"
        "}"
    );

    m_inputLayout->addWidget(inputLabel);
    m_inputLayout->addWidget(m_ipInput, 1);
    m_inputLayout->addWidget(m_lookupBtn);
    m_inputLayout->addWidget(m_clearBtn);
    m_inputLayout->addWidget(m_getMyIpBtn);

    // 结果显示区域
    m_resultGroup = new QGroupBox("查询结果");
    m_resultGroup->setStyleSheet(
        "QGroupBox {"
        "font-weight: bold;"
        "border: 2px solid #bdc3c7;"
        "border-radius: 8px;"
        "margin-top: 1ex;"
        "padding-top: 15px;"
        "}"
        "QGroupBox::title {"
        "subcontrol-origin: margin;"
        "left: 10px;"
        "padding: 0 8px 0 8px;"
        "color: #2c3e50;"
        "}"
    );

    m_resultLayout = new QFormLayout(m_resultGroup);
    m_resultLayout->setSpacing(8);
    m_resultLayout->setLabelAlignment(Qt::AlignRight);

    // 创建结果标签
    m_ipLabel = new QLabel("-");
    m_countryLabel = new QLabel("-");
    m_regionLabel = new QLabel("-");
    m_cityLabel = new QLabel("-");
    m_ispLabel = new QLabel("-");
    m_orgLabel = new QLabel("-");
    m_timezoneLabel = new QLabel("-");
    m_coordinatesLabel = new QLabel("-");
    m_asnLabel = new QLabel("-");

    // 设置标签样式
    auto setLabelStyle = [](QLabel* label) {
        label->setStyleSheet(
            "QLabel {"
            "padding: 4px;"
            "background-color: #ecf0f1;"
            "border-radius: 4px;"
            "}"
        );
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    };

    setLabelStyle(m_ipLabel);
    setLabelStyle(m_countryLabel);
    setLabelStyle(m_regionLabel);
    setLabelStyle(m_cityLabel);
    setLabelStyle(m_ispLabel);
    setLabelStyle(m_orgLabel);
    setLabelStyle(m_timezoneLabel);
    setLabelStyle(m_coordinatesLabel);
    setLabelStyle(m_asnLabel);

    // 添加到表单布局
    m_resultLayout->addRow("IP地址:", m_ipLabel);
    m_resultLayout->addRow("国家:", m_countryLabel);
    m_resultLayout->addRow("省/州:", m_regionLabel);
    m_resultLayout->addRow("城市:", m_cityLabel);
    m_resultLayout->addRow("ISP:", m_ispLabel);
    m_resultLayout->addRow("组织:", m_orgLabel);
    m_resultLayout->addRow("时区:", m_timezoneLabel);
    m_resultLayout->addRow("坐标:", m_coordinatesLabel);
    m_resultLayout->addRow("ASN:", m_asnLabel);

    // 操作按钮
    m_buttonLayout = new QHBoxLayout();
    m_copyResultBtn = new QPushButton("📋 复制结果");
    m_copyResultBtn->setStyleSheet(
        "QPushButton {"
        "background-color: #e74c3c;"
        "color: white;"
        "border: none;"
        "padding: 8px 16px;"
        "border-radius: 6px;"
        "font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "background-color: #c0392b;"
        "}"
    );

    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_copyResultBtn);

    // 组装布局
    m_mainLayout->addLayout(m_inputLayout);
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
    }

    QNetworkRequest networkRequest(QUrl("https://api.ipify.org"));
    m_currentReply = m_networkManager->get(networkRequest);
    connect(m_currentReply, &QNetworkReply::finished, [this]() {
        if (m_currentReply->error() == QNetworkReply::NoError) {
            QString myIp = QString::fromUtf8(m_currentReply->readAll()).trimmed();
            m_ipInput->setText(myIp);
            lookupIp(myIp);
        } else {
            displayError("获取外网IP失败: " + m_currentReply->errorString());
        }
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    });

    m_lookupBtn->setEnabled(false);
    m_lookupBtn->setText("获取中...");
}

void SingleIpLookup::lookupIp(const QString& ip) {
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
    }

    m_currentIp = ip;
    m_lookupBtn->setEnabled(false);
    m_lookupBtn->setText("查询中...");

    // 使用ip-api.com进行查询
    QString url = QString("http://ip-api.com/json/%1?fields=status,message,country,regionName,city,isp,org,timezone,lat,lon,as,query").arg(ip);

    // 这在 MSVC 下可能被当成函数声明（所谓 Most Vexing Parse 问题），结果 networkRequest 就不是对象了。
    // QNetworkRequest networkRequest(QUrl(url));
    // chatgpt 牛逼
    QNetworkRequest networkRequest {QUrl(url)};
    networkRequest.setRawHeader(QByteArray("User-Agent"), QByteArray("IP Lookup Tool 1.0"));

    m_currentReply = m_networkManager->get(networkRequest);
    connect(m_currentReply, &QNetworkReply::finished, this, &SingleIpLookup::onNetworkReplyFinished);
}

void SingleIpLookup::onNetworkReplyFinished() {
    m_lookupBtn->setEnabled(true);
    m_lookupBtn->setText("🔍 查询");

    if (!m_currentReply) return;

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
    m_mainLayout->setSpacing(15);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);

    // 主分割器
    m_mainSplitter = new QSplitter(Qt::Horizontal);

    // 输入区域
    m_inputWidget = new QWidget();
    m_inputLayout = new QVBoxLayout(m_inputWidget);

    m_inputLabel = new QLabel("请输入IP地址列表（每行一个）:");
    m_inputLabel->setStyleSheet("font-weight: bold; color: #2c3e50; margin-bottom: 5px;");

    m_ipListInput = new QPlainTextEdit();
    m_ipListInput->setPlaceholderText("8.8.8.8\n1.1.1.1\ngoogle.com\nbaidu.com\n...");
    m_ipListInput->setStyleSheet(
        "QPlainTextEdit {"
        "border: 2px solid #bdc3c7;"
        "border-radius: 6px;"
        "padding: 8px;"
        "font-family: Consolas, Monaco, monospace;"
        "font-size: 14px;"
        "}"
        "QPlainTextEdit:focus {"
        "border-color: #3498db;"
        "}"
    );

    m_inputButtonLayout = new QHBoxLayout();
    m_importBtn = new QPushButton("📁 导入文件");
    m_clearAllBtn = new QPushButton("🗑️ 清空全部");

    m_importBtn->setStyleSheet(
        "QPushButton {"
        "background-color: #f39c12;"
        "color: white;"
        "border: none;"
        "padding: 8px 16px;"
        "border-radius: 6px;"
        "font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "background-color: #d68910;"
        "}"
    );

    m_clearAllBtn->setStyleSheet(
        "QPushButton {"
        "background-color: #e74c3c;"
        "color: white;"
        "border: none;"
        "padding: 8px 16px;"
        "border-radius: 6px;"
        "font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "background-color: #c0392b;"
        "}"
    );

    m_inputButtonLayout->addWidget(m_importBtn);
    m_inputButtonLayout->addWidget(m_clearAllBtn);
    m_inputButtonLayout->addStretch();

    m_inputLayout->addWidget(m_inputLabel);
    m_inputLayout->addWidget(m_ipListInput);
    m_inputLayout->addLayout(m_inputButtonLayout);

    // 控制区域
    m_controlLayout = new QHBoxLayout();

    m_startBtn = new QPushButton("🚀 开始批量查询");
    m_stopBtn = new QPushButton("⏹️ 停止查询");
    m_progressBar = new QProgressBar();
    m_statusLabel = new QLabel("准备就绪");

    m_startBtn->setStyleSheet(
        "QPushButton {"
        "background-color: #27ae60;"
        "color: white;"
        "border: none;"
        "padding: 10px 20px;"
        "border-radius: 6px;"
        "font-weight: bold;"
        "font-size: 14px;"
        "}"
        "QPushButton:hover {"
        "background-color: #219a52;"
        "}"
    );

    m_stopBtn->setStyleSheet(
        "QPushButton {"
        "background-color: #e74c3c;"
        "color: white;"
        "border: none;"
        "padding: 10px 20px;"
        "border-radius: 6px;"
        "font-weight: bold;"
        "font-size: 14px;"
        "}"
        "QPushButton:hover {"
        "background-color: #c0392b;"
        "}"
    );

    m_stopBtn->setEnabled(false);

    m_progressBar->setStyleSheet(
        "QProgressBar {"
        "border: 2px solid #bdc3c7;"
        "border-radius: 5px;"
        "text-align: center;"
        "}"
        "QProgressBar::chunk {"
        "background-color: #3498db;"
        "border-radius: 3px;"
        "}"
    );

    m_statusLabel->setStyleSheet("color: #7f8c8d; font-style: italic;");

    m_controlLayout->addWidget(m_startBtn);
    m_controlLayout->addWidget(m_stopBtn);
    m_controlLayout->addWidget(m_progressBar, 1);
    m_controlLayout->addWidget(m_statusLabel);

    // 结果区域
    m_resultWidget = new QWidget();
    m_resultLayout = new QVBoxLayout(m_resultWidget);

    auto* resultLabel = new QLabel("查询结果:");
    resultLabel->setStyleSheet("font-weight: bold; color: #2c3e50; margin-bottom: 5px;");

    m_resultTable = new QTableWidget();
    m_resultTable->setColumnCount(6);
    QStringList headers = {"IP地址", "国家", "省/州", "城市", "ISP", "状态"};
    m_resultTable->setHorizontalHeaderLabels(headers);

    // 设置表格样式
    m_resultTable->setStyleSheet(
        "QTableWidget {"
        "border: 1px solid #bdc3c7;"
        "border-radius: 6px;"
        "gridline-color: #ecf0f1;"
        "}"
        "QHeaderView::section {"
        "background-color: #34495e;"
        "color: white;"
        "padding: 8px;"
        "border: none;"
        "font-weight: bold;"
        "}"
        "QTableWidget::item {"
        "padding: 8px;"
        "}"
        "QTableWidget::item:selected {"
        "background-color: #3498db;"
        "color: white;"
        "}"
    );

    m_resultTable->horizontalHeader()->setStretchLastSection(false);
    m_resultTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_resultTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_resultTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_resultTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_resultTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_resultTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);

    m_resultButtonLayout = new QHBoxLayout();
    m_exportBtn = new QPushButton("💾 导出结果");
    m_copyAllBtn = new QPushButton("📋 复制全部");

    m_exportBtn->setStyleSheet(
        "QPushButton {"
        "background-color: #9b59b6;"
        "color: white;"
        "border: none;"
        "padding: 8px 16px;"
        "border-radius: 6px;"
        "font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "background-color: #8e44ad;"
        "}"
    );

    m_copyAllBtn->setStyleSheet(
        "QPushButton {"
        "background-color: #e67e22;"
        "color: white;"
        "border: none;"
        "padding: 8px 16px;"
        "border-radius: 6px;"
        "font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "background-color: #d35400;"
        "}"
    );

    m_resultButtonLayout->addWidget(m_exportBtn);
    m_resultButtonLayout->addWidget(m_copyAllBtn);
    m_resultButtonLayout->addStretch();

    m_resultLayout->addWidget(resultLabel);
    m_resultLayout->addWidget(m_resultTable);
    m_resultLayout->addLayout(m_resultButtonLayout);

    // 组装主布局
    m_mainSplitter->addWidget(m_inputWidget);
    m_mainSplitter->addWidget(m_resultWidget);
    m_mainSplitter->setSizes({300, 500});

    m_mainLayout->addLayout(m_controlLayout);
    m_mainLayout->addWidget(m_mainSplitter);

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
    QString fileName = QFileDialog::getOpenFileName(this, "导入IP地址文件", "", "文本文件 (*.txt);;所有文件 (*.*)");
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            QString content = in.readAll();
            m_ipListInput->setPlainText(content);
        } else {
            QMessageBox::warning(this, "错误", "无法打开文件: " + fileName);
        }
    }
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

    // API查询
    QString url = QString("http://ip-api.com/json/%1?fields=status,message,country,regionName,city,isp,org,timezone,lat,lon,as,query").arg(actualIp);
    QNetworkRequest networkRequest {QUrl(url)};
    networkRequest.setRawHeader(QByteArray("User-Agent"), QByteArray("IP Lookup Tool 1.0"));

    m_currentReply = m_networkManager->get(networkRequest);
    connect(m_currentReply, &QNetworkReply::finished, this, &BatchIpLookup::onNetworkReplyFinished);
}

void BatchIpLookup::onNetworkReplyFinished() {
    if (!m_currentReply || !m_isRunning) return;

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
    m_results.append({ip, result});

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
    QStringList ips;
    QStringList lines = text.split('\n', Qt::SkipEmptyParts);

    for (QString line : lines) {
        line = line.trimmed();
        if (!line.isEmpty() && !line.startsWith('#')) {
            ips.append(line);
        }
    }

    return ips;
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
    m_tabWidget->setStyleSheet(
        "QTabWidget::pane {"
        "border: 1px solid #bdc3c7;"
        "background-color: white;"
        "}"
        "QTabBar::tab {"
        "background-color: #ecf0f1;"
        "color: #2c3e50;"
        "padding: 12px 24px;"
        "margin-right: 2px;"
        "border-top-left-radius: 6px;"
        "border-top-right-radius: 6px;"
        "font-weight: bold;"
        "}"
        "QTabBar::tab:selected {"
        "background-color: white;"
        "border-bottom: 2px solid #3498db;"
        "}"
        "QTabBar::tab:hover {"
        "background-color: #d5dbdb;"
        "}"
    );

    // 创建单个查询标签页
    m_singleLookup = new SingleIpLookup();
    m_tabWidget->addTab(m_singleLookup, "🔍 单个查询");

    // 创建批量查询标签页
    m_batchLookup = new BatchIpLookup();
    m_tabWidget->addTab(m_batchLookup, "📊 批量查询");

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
    if (!m_currentReply) return;

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

