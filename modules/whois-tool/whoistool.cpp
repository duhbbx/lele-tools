#include "whoistool.h"
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>

// 动态对象创建宏
REGISTER_DYNAMICOBJECT(WhoisTool);

// WhoisClient 实现
WhoisClient::WhoisClient(QObject* parent)
    : QObject(parent)
    , m_socket(nullptr)
    , m_currentReply(nullptr)
    , m_batchMode(false) {

    m_networkManager = new QNetworkAccessManager(this);
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    m_timeoutTimer->setInterval(30000); // 30秒超时

    initWhoisServers();

    connect(m_timeoutTimer, &QTimer::timeout, [this]() {
        if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
            m_socket->disconnectFromHost();
        }
        emit errorOccurred("查询超时");
    });
}

void WhoisClient::queryDomain(const QString& domain) {
    QString cleanedDomain = cleanDomain(domain);

    if (!isValidDomain(cleanedDomain)) {
        emit errorOccurred("无效的域名格式");
        return;
    }

    m_currentDomain = cleanedDomain;
    m_responseData.clear();

    emit queryProgress(QString("正在查询域名: %1").arg(cleanedDomain));

    QString whoisServer = findWhoisServer(cleanedDomain);
    if (whoisServer.isEmpty()) {
        // 如果找不到WHOIS服务器，尝试使用HTTP API
        queryWithHttp(cleanedDomain);
        return;
    }

    emit queryProgress(QString("连接WHOIS服务器: %1").arg(whoisServer));
    connectToWhoisServer(whoisServer);
}

void WhoisClient::queryIP(const QString& ip) {
    if (!isValidIP(ip)) {
        emit errorOccurred("无效的IP地址格式");
        return;
    }

    m_currentDomain = ip;
    m_responseData.clear();

    emit queryProgress(QString("正在查询IP: %1").arg(ip));

    // IP查询通常使用特定的WHOIS服务器
    QString whoisServer = "whois.arin.net"; // 默认使用ARIN
    if (ip.startsWith("194.") || ip.startsWith("195.") || ip.startsWith("62.")) {
        whoisServer = "whois.ripe.net"; // 欧洲地区
    } else if (ip.startsWith("210.") || ip.startsWith("211.") || ip.startsWith("218.")) {
        whoisServer = "whois.apnic.net"; // 亚太地区
    }

    connectToWhoisServer(whoisServer);
}

void WhoisClient::setBatchMode(bool batch) {
    m_batchMode = batch;
}

void WhoisClient::connectToWhoisServer(const QString& server, int port) {
    if (m_socket) {
        m_socket->deleteLater();
    }

    m_socket = new QTcpSocket(this);

    connect(m_socket, &QTcpSocket::connected, this, &WhoisClient::onSocketConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &WhoisClient::onSocketReadyRead);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &WhoisClient::onSocketError);
    connect(m_socket, &QTcpSocket::disconnected, this, &WhoisClient::onSocketDisconnected);

    m_timeoutTimer->start();
    m_socket->connectToHost(server, port);
}

void WhoisClient::sendQuery(const QString& query) {
    if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
        QString queryString = query + "\r\n";
        m_socket->write(queryString.toUtf8());
        emit queryProgress("发送查询请求...");
    }
}

void WhoisClient::onSocketConnected() {
    m_timeoutTimer->stop();
    emit queryProgress("已连接到WHOIS服务器");
    sendQuery(m_currentDomain);
}

void WhoisClient::onSocketReadyRead() {
    if (m_socket) {
        QByteArray data = m_socket->readAll();
        m_responseData.append(QString::fromUtf8(data));

        if (!m_batchMode) {
            emit queryProgress("接收数据中...");
        }
    }
}

void WhoisClient::onSocketError(QAbstractSocket::SocketError error) {
    m_timeoutTimer->stop();
    QString errorString;

    switch (error) {
        case QAbstractSocket::HostNotFoundError:
            errorString = "找不到WHOIS服务器";
            break;
        case QAbstractSocket::ConnectionRefusedError:
            errorString = "连接被拒绝";
            break;
        case QAbstractSocket::NetworkError:
            errorString = "网络错误";
            break;
        default:
            errorString = QString("连接错误: %1").arg(m_socket->errorString());
    }

    emit errorOccurred(errorString);
}

void WhoisClient::onSocketDisconnected() {
    m_timeoutTimer->stop();

    if (!m_responseData.isEmpty()) {
        WhoisResult result = parseWhoisData(m_responseData, m_currentDomain);
        emit queryFinished(result);
    } else {
        emit errorOccurred("未收到WHOIS数据");
    }

    if (m_socket) {
        m_socket->deleteLater();
        m_socket = nullptr;
    }
}

void WhoisClient::queryWithHttp(const QString& domain) {
    // 使用公共的WHOIS API服务
    QString url = QString("https://www.whoisxmlapi.com/whoisserver/WhoisService?domainName=%1&outputFormat=json").arg(domain);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "WhoisTool/1.0");

    if (m_currentReply) {
        m_currentReply->deleteLater();
    }

    m_currentReply = m_networkManager->get(request);
    connect(m_currentReply, &QNetworkReply::finished, this, &WhoisClient::onHttpReplyFinished);
}

void WhoisClient::onHttpReplyFinished() {
    if (!m_currentReply) return;

    WhoisResult result;
    result.domain = m_currentDomain;

    if (m_currentReply->error() == QNetworkReply::NoError) {
        QByteArray data = m_currentReply->readAll();

        // 尝试解析JSON响应
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(data, &error);

        if (error.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("WhoisRecord")) {
                QJsonObject whoisRecord = obj["WhoisRecord"].toObject();

                result.registrar = whoisRecord["registrarName"].toString();
                result.creationDate = whoisRecord["createdDate"].toString();
                result.expirationDate = whoisRecord["expiresDate"].toString();
                result.lastUpdated = whoisRecord["updatedDate"].toString();

                if (whoisRecord.contains("registrant")) {
                    QJsonObject registrant = whoisRecord["registrant"].toObject();
                    result.registrant = registrant["name"].toString();
                }

                if (whoisRecord.contains("nameServers")) {
                    QJsonObject nameServers = whoisRecord["nameServers"].toObject();
                    QStringList nsList;
                    if (nameServers.contains("hostNames")) {
                        QJsonArray hostNames = nameServers["hostNames"].toArray();
                        for (const QJsonValue& value : hostNames) {
                            nsList.append(value.toString());
                        }
                    }
                    result.nameServers = nsList.join(", ");
                }
            }

            result.rawData = QString::fromUtf8(data);
        } else {
            // 如果不是JSON，当作纯文本处理
            result = parseWhoisData(QString::fromUtf8(data), m_currentDomain);
        }
    } else {
        result.error = QString("HTTP查询失败: %1").arg(m_currentReply->errorString());
    }

    m_currentReply->deleteLater();
    m_currentReply = nullptr;

    emit queryFinished(result);
}

WhoisResult WhoisClient::parseWhoisData(const QString& data, const QString& domain) {
    WhoisResult result;
    result.domain = domain;
    result.rawData = data;

    if (data.isEmpty()) {
        result.error = "未收到WHOIS数据";
        return result;
    }

    // 检查是否包含错误信息
    if (data.contains("No match", Qt::CaseInsensitive) ||
        data.contains("not found", Qt::CaseInsensitive) ||
        data.contains("no data found", Qt::CaseInsensitive)) {
        result.error = "域名未注册或查询无结果";
        return result;
    }

    QStringList lines = data.split('\n');

    for (const QString& line : lines) {
        QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty() || trimmedLine.startsWith('%') || trimmedLine.startsWith('#')) {
            continue;
        }

        QString lowerLine = trimmedLine.toLower();

        // 提取注册商信息
        if (lowerLine.contains("registrar:") || lowerLine.contains("registrar name:")) {
            result.registrar = extractValue(trimmedLine);
        }
        // 提取创建时间
        else if (lowerLine.contains("creation date:") || lowerLine.contains("created:") ||
                 lowerLine.contains("registered:") || lowerLine.contains("created on:")) {
            result.creationDate = extractValue(trimmedLine);
        }
        // 提取到期时间
        else if (lowerLine.contains("expir") && (lowerLine.contains("date:") || lowerLine.contains("on:"))) {
            result.expirationDate = extractValue(trimmedLine);
        }
        // 提取更新时间
        else if (lowerLine.contains("updated") && lowerLine.contains("date:")) {
            result.lastUpdated = extractValue(trimmedLine);
        }
        // 提取状态信息
        else if (lowerLine.contains("status:") || lowerLine.contains("domain status:")) {
            if (result.status.isEmpty()) {
                result.status = extractValue(trimmedLine);
            } else {
                result.status += ", " + extractValue(trimmedLine);
            }
        }
        // 提取名称服务器
        else if (lowerLine.contains("name server:") || lowerLine.contains("nserver:")) {
            if (result.nameServers.isEmpty()) {
                result.nameServers = extractValue(trimmedLine);
            } else {
                result.nameServers += ", " + extractValue(trimmedLine);
            }
        }
        // 提取注册人信息
        else if (lowerLine.contains("registrant name:") || lowerLine.contains("registrant:")) {
            result.registrant = extractValue(trimmedLine);
        }
        // 提取DNSSEC信息
        else if (lowerLine.contains("dnssec:")) {
            result.dnssec = extractValue(trimmedLine);
        }
    }

    return result;
}

QString WhoisClient::extractValue(const QString& line) {
    int colonIndex = line.indexOf(':');
    if (colonIndex != -1 && colonIndex < line.length() - 1) {
        return line.mid(colonIndex + 1).trimmed();
    }
    return "";
}

QString WhoisClient::findWhoisServer(const QString& domain) {
    QString tld = extractTLD(domain);

    for (const WhoisServer& server : m_whoisServers) {
        if (server.tld == tld) {
            return server.server;
        }
    }

    // 如果找不到特定的服务器，尝试通用服务器
    return QString("whois.iana.org");
}

QString WhoisClient::extractTLD(const QString& domain) {
    int lastDotIndex = domain.lastIndexOf('.');
    if (lastDotIndex != -1 && lastDotIndex < domain.length() - 1) {
        return domain.mid(lastDotIndex + 1).toLower();
    }
    return "";
}

void WhoisClient::initWhoisServers() {
    m_whoisServers.clear();

    // 常用顶级域名的WHOIS服务器
    m_whoisServers << WhoisServer("com", "whois.verisign-grs.com");
    m_whoisServers << WhoisServer("net", "whois.verisign-grs.com");
    m_whoisServers << WhoisServer("org", "whois.pir.org");
    m_whoisServers << WhoisServer("info", "whois.afilias.net");
    m_whoisServers << WhoisServer("biz", "whois.neulevel.biz");
    m_whoisServers << WhoisServer("name", "whois.nic.name");
    m_whoisServers << WhoisServer("cn", "whois.cnnic.cn");
    m_whoisServers << WhoisServer("tw", "whois.twnic.net.tw");
    m_whoisServers << WhoisServer("hk", "whois.hkirc.hk");
    m_whoisServers << WhoisServer("jp", "whois.jprs.jp");
    m_whoisServers << WhoisServer("kr", "whois.nida.or.kr");
    m_whoisServers << WhoisServer("uk", "whois.nic.uk");
    m_whoisServers << WhoisServer("de", "whois.denic.de");
    m_whoisServers << WhoisServer("fr", "whois.nic.fr");
    m_whoisServers << WhoisServer("it", "whois.nic.it");
    m_whoisServers << WhoisServer("ru", "whois.ripn.net");
    m_whoisServers << WhoisServer("au", "whois.aunic.net");
    m_whoisServers << WhoisServer("ca", "whois.cira.ca");
    m_whoisServers << WhoisServer("br", "whois.registro.br");
    m_whoisServers << WhoisServer("mx", "whois.mx");
    m_whoisServers << WhoisServer("edu", "whois.educause.edu");
    m_whoisServers << WhoisServer("gov", "whois.dotgov.gov");
    m_whoisServers << WhoisServer("mil", "whois.nic.mil");
}

QString WhoisClient::cleanDomain(const QString& domain) {
    QString cleaned = domain.trimmed().toLower();

    // 移除协议前缀
    if (cleaned.startsWith("http://")) {
        cleaned = cleaned.mid(7);
    } else if (cleaned.startsWith("https://")) {
        cleaned = cleaned.mid(8);
    }

    // 移除www前缀
    if (cleaned.startsWith("www.")) {
        cleaned = cleaned.mid(4);
    }

    // 移除路径
    int slashIndex = cleaned.indexOf('/');
    if (slashIndex != -1) {
        cleaned = cleaned.left(slashIndex);
    }

    return cleaned;
}

bool WhoisClient::isValidDomain(const QString& domain) {
    if (domain.isEmpty() || domain.length() > 253) {
        return false;
    }

    QRegularExpression domainRegex(R"(^[a-zA-Z0-9]([a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?(\.[a-zA-Z0-9]([a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?)*$)");
    return domainRegex.match(domain).hasMatch();
}

bool WhoisClient::isValidIP(const QString& ip) {
    QRegularExpression ipRegex(R"(^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)");
    return ipRegex.match(ip).hasMatch();
}

// SingleWhoisQuery 实现
SingleWhoisQuery::SingleWhoisQuery(QWidget* parent)
    : QWidget(parent) {

    m_whoisClient = new WhoisClient(this);
    setupUI();

    connect(m_whoisClient, &WhoisClient::queryFinished, this, &SingleWhoisQuery::onQueryFinished);
    connect(m_whoisClient, &WhoisClient::queryProgress, this, &SingleWhoisQuery::onQueryProgress);
    connect(m_whoisClient, &WhoisClient::errorOccurred, this, &SingleWhoisQuery::onQueryError);
}

void SingleWhoisQuery::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(15);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);

    // 创建主分割器
    m_mainSplitter = new QSplitter(Qt::Horizontal);

    // 左侧查询区域
    m_queryWidget = new QWidget();
    m_queryWidget->setFixedWidth(400);
    m_queryLayout = new QVBoxLayout(m_queryWidget);

    // 输入组
    m_inputGroup = new QGroupBox("域名/IP查询");
    m_inputGroup->setStyleSheet(
        "QGroupBox {"
        "font-weight: bold;"
        "border: 2px solid #bdc3c7;"
        "border-radius: 8px;"
        "margin-top: 10px;"
        "padding-top: 10px;"
        "}"
        "QGroupBox::title {"
        "subcontrol-origin: margin;"
        "left: 10px;"
        "padding: 0 5px 0 5px;"
        "}"
    );

    m_inputLayout = new QFormLayout(m_inputGroup);

    m_domainInput = new QLineEdit();
    m_domainInput->setPlaceholderText("输入域名或IP地址，如: google.com 或 8.8.8.8");
    m_domainInput->setStyleSheet("padding: 8px; border: 1px solid #bdc3c7; border-radius: 4px;");

    m_buttonLayout = new QHBoxLayout();
    m_queryBtn = new QPushButton("🔍 查询");
    m_queryBtn->setStyleSheet(
        "QPushButton {"
        "background-color: #3498db;"
        "color: white;"
        "border: none;"
        "padding: 10px 20px;"
        "border-radius: 6px;"
        "font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "background-color: #2980b9;"
        "}"
        "QPushButton:disabled {"
        "background-color: #bdc3c7;"
        "}"
    );

    m_clearBtn = new QPushButton("🗑️ 清空");
    m_clearBtn->setStyleSheet(
        "QPushButton {"
        "background-color: #95a5a6;"
        "color: white;"
        "border: none;"
        "padding: 10px 20px;"
        "border-radius: 6px;"
        "font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "background-color: #7f8c8d;"
        "}"
    );

    m_buttonLayout->addWidget(m_queryBtn);
    m_buttonLayout->addWidget(m_clearBtn);

    m_inputLayout->addRow("查询目标:", m_domainInput);
    m_inputLayout->addRow("", m_buttonLayout);

    // 结果概览组
    m_summaryGroup = new QGroupBox("查询结果概览");
    m_summaryGroup->setStyleSheet(m_inputGroup->styleSheet());
    m_summaryLayout = new QFormLayout(m_summaryGroup);

    m_domainLabel = new QLabel("-");
    m_registrarLabel = new QLabel("-");
    m_creationLabel = new QLabel("-");
    m_expirationLabel = new QLabel("-");
    m_statusLabel = new QLabel("-");

    m_summaryLayout->addRow("域名:", m_domainLabel);
    m_summaryLayout->addRow("注册商:", m_registrarLabel);
    m_summaryLayout->addRow("注册时间:", m_creationLabel);
    m_summaryLayout->addRow("到期时间:", m_expirationLabel);
    m_summaryLayout->addRow("状态:", m_statusLabel);

    // 状态组
    m_statusGroup = new QGroupBox("查询状态");
    m_statusGroup->setStyleSheet(m_inputGroup->styleSheet());
    m_statusLayout = new QVBoxLayout(m_statusGroup);

    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 0);
    m_progressBar->setVisible(false);

    m_queryStatusLabel = new QLabel("准备就绪");
    m_queryStatusLabel->setStyleSheet("color: #7f8c8d; font-style: italic;");

    m_statusLayout->addWidget(m_progressBar);
    m_statusLayout->addWidget(m_queryStatusLabel);

    // 组装左侧布局
    m_queryLayout->addWidget(m_inputGroup);
    m_queryLayout->addWidget(m_summaryGroup);
    m_queryLayout->addWidget(m_statusGroup);
    m_queryLayout->addStretch();

    // 右侧结果区域
    m_resultWidget = new QWidget();
    m_resultLayout = new QVBoxLayout(m_resultWidget);

    // 结果标签页
    m_resultTabs = new QTabWidget();
    m_resultTabs->setStyleSheet(
        "QTabWidget::pane {"
        "border: 1px solid #bdc3c7;"
        "background-color: white;"
        "}"
        "QTabBar::tab {"
        "background-color: #ecf0f1;"
        "color: #2c3e50;"
        "padding: 8px 16px;"
        "margin-right: 2px;"
        "border-top-left-radius: 4px;"
        "border-top-right-radius: 4px;"
        "}"
        "QTabBar::tab:selected {"
        "background-color: white;"
        "border-bottom: 2px solid #3498db;"
        "}"
    );

    // 详细信息表格
    m_detailsTable = new QTableWidget();
    m_detailsTable->setColumnCount(2);
    m_detailsTable->setHorizontalHeaderLabels({"字段", "值"});
    m_detailsTable->horizontalHeader()->setStretchLastSection(true);
    m_detailsTable->setAlternatingRowColors(true);
    m_detailsTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    // 原始数据文本
    m_rawDataText = new QTextEdit();
    m_rawDataText->setReadOnly(true);
    m_rawDataText->setFont(QFont("Consolas", 9));
    m_rawDataText->setStyleSheet(
        "QTextEdit {"
        "border: 1px solid #bdc3c7;"
        "background-color: #f8f9fa;"
        "}"
    );

    m_resultTabs->addTab(m_detailsTable, "📊 详细信息");
    m_resultTabs->addTab(m_rawDataText, "📄 原始数据");

    // 结果操作按钮
    m_resultButtonLayout = new QHBoxLayout();
    m_copyBtn = new QPushButton("📋 复制结果");
    m_saveBtn = new QPushButton("💾 保存结果");

    m_copyBtn->setStyleSheet(
        "QPushButton {"
        "background-color: #27ae60;"
        "color: white;"
        "border: none;"
        "padding: 8px 16px;"
        "border-radius: 4px;"
        "font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "background-color: #219a52;"
        "}"
        "QPushButton:disabled {"
        "background-color: #bdc3c7;"
        "}"
    );

    m_saveBtn->setStyleSheet(m_copyBtn->styleSheet().replace("#27ae60", "#e74c3c").replace("#219a52", "#c0392b"));

    m_copyBtn->setEnabled(false);
    m_saveBtn->setEnabled(false);

    m_resultButtonLayout->addWidget(m_copyBtn);
    m_resultButtonLayout->addWidget(m_saveBtn);
    m_resultButtonLayout->addStretch();

    m_resultLayout->addWidget(m_resultTabs);
    m_resultLayout->addLayout(m_resultButtonLayout);

    // 组装主布局
    m_mainSplitter->addWidget(m_queryWidget);
    m_mainSplitter->addWidget(m_resultWidget);
    m_mainSplitter->setSizes({400, 600});

    m_mainLayout->addWidget(m_mainSplitter);

    // 连接信号
    connect(m_queryBtn, &QPushButton::clicked, this, &SingleWhoisQuery::onQueryClicked);
    connect(m_clearBtn, &QPushButton::clicked, this, &SingleWhoisQuery::onClearClicked);
    connect(m_copyBtn, &QPushButton::clicked, this, &SingleWhoisQuery::onCopyResultClicked);
    connect(m_saveBtn, &QPushButton::clicked, this, &SingleWhoisQuery::onSaveResultClicked);

    // 回车键查询
    connect(m_domainInput, &QLineEdit::returnPressed, this, &SingleWhoisQuery::onQueryClicked);
}

void SingleWhoisQuery::onQueryClicked() {
    QString input = m_domainInput->text().trimmed();

    if (input.isEmpty()) {
        QMessageBox::warning(this, "警告", "请输入域名或IP地址");
        return;
    }

    m_queryBtn->setEnabled(false);
    m_progressBar->setVisible(true);
    m_queryStatusLabel->setText("正在查询...");

    // 清空之前的结果
    m_detailsTable->setRowCount(0);
    m_rawDataText->clear();
    m_copyBtn->setEnabled(false);
    m_saveBtn->setEnabled(false);

    // 判断是域名还是IP
    QRegularExpression ipRegex(R"(^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)");
    if (ipRegex.match(input).hasMatch()) {
        m_whoisClient->queryIP(input);
    } else {
        m_whoisClient->queryDomain(input);
    }
}

void SingleWhoisQuery::onClearClicked() {
    m_domainInput->clear();
    m_detailsTable->setRowCount(0);
    m_rawDataText->clear();

    // 重置概览标签
    m_domainLabel->setText("-");
    m_registrarLabel->setText("-");
    m_creationLabel->setText("-");
    m_expirationLabel->setText("-");
    m_statusLabel->setText("-");

    m_copyBtn->setEnabled(false);
    m_saveBtn->setEnabled(false);
    m_queryStatusLabel->setText("准备就绪");
}

void SingleWhoisQuery::onCopyResultClicked() {
    if (m_lastResult.isValid()) {
        QString result = QString("WHOIS查询结果 - %1\n").arg(m_lastResult.domain);
        result += QString("查询时间: %1\n\n").arg(m_lastResult.queryTime.toString());

        if (!m_lastResult.registrar.isEmpty()) {
            result += QString("注册商: %1\n").arg(m_lastResult.registrar);
        }
        if (!m_lastResult.creationDate.isEmpty()) {
            result += QString("注册时间: %1\n").arg(m_lastResult.creationDate);
        }
        if (!m_lastResult.expirationDate.isEmpty()) {
            result += QString("到期时间: %1\n").arg(m_lastResult.expirationDate);
        }
        if (!m_lastResult.status.isEmpty()) {
            result += QString("状态: %1\n").arg(m_lastResult.status);
        }

        result += "\n原始数据:\n" + m_lastResult.rawData;

        QApplication::clipboard()->setText(result);
        QMessageBox::information(this, "成功", "查询结果已复制到剪贴板");
    }
}

void SingleWhoisQuery::onSaveResultClicked() {
    if (m_lastResult.isValid()) {
        QString fileName = QFileDialog::getSaveFileName(
            this,
            "保存WHOIS查询结果",
            QString("whois_%1_%2.txt")
                .arg(m_lastResult.domain)
                .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")),
            "文本文件 (*.txt);;所有文件 (*.*)"
        );

        if (!fileName.isEmpty()) {
            QFile file(fileName);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream stream(&file);
                stream.setEncoding(QStringConverter::Utf8);

                stream << QString("WHOIS查询结果 - %1\n").arg(m_lastResult.domain);
                stream << QString("查询时间: %1\n\n").arg(m_lastResult.queryTime.toString());

                if (!m_lastResult.registrar.isEmpty()) {
                    stream << QString("注册商: %1\n").arg(m_lastResult.registrar);
                }
                if (!m_lastResult.creationDate.isEmpty()) {
                    stream << QString("注册时间: %1\n").arg(m_lastResult.creationDate);
                }
                if (!m_lastResult.expirationDate.isEmpty()) {
                    stream << QString("到期时间: %1\n").arg(m_lastResult.expirationDate);
                }
                if (!m_lastResult.status.isEmpty()) {
                    stream << QString("状态: %1\n").arg(m_lastResult.status);
                }

                stream << "\n原始数据:\n" << m_lastResult.rawData;

                QMessageBox::information(this, "成功", "查询结果已保存");
            } else {
                QMessageBox::warning(this, "错误", "无法保存文件");
            }
        }
    }
}

void SingleWhoisQuery::onQueryFinished(const WhoisResult& result) {
    m_lastResult = result;
    m_queryBtn->setEnabled(true);
    m_progressBar->setVisible(false);

    if (result.isValid()) {
        displayResult(result);
        m_queryStatusLabel->setText("查询完成");
        m_copyBtn->setEnabled(true);
        m_saveBtn->setEnabled(true);
    } else {
        m_queryStatusLabel->setText(QString("查询失败: %1").arg(result.error));
        QMessageBox::warning(this, "查询失败", result.error);
    }
}

void SingleWhoisQuery::onQueryProgress(const QString& message) {
    m_queryStatusLabel->setText(message);
}

void SingleWhoisQuery::onQueryError(const QString& error) {
    m_queryBtn->setEnabled(true);
    m_progressBar->setVisible(false);
    m_queryStatusLabel->setText(QString("错误: %1").arg(error));
    QMessageBox::warning(this, "查询错误", error);
}

void SingleWhoisQuery::displayResult(const WhoisResult& result) {
    // 更新概览信息
    m_domainLabel->setText(result.domain);
    m_registrarLabel->setText(result.registrar.isEmpty() ? "-" : result.registrar);
    m_creationLabel->setText(result.creationDate.isEmpty() ? "-" : result.creationDate);
    m_expirationLabel->setText(result.expirationDate.isEmpty() ? "-" : result.expirationDate);
    m_statusLabel->setText(result.status.isEmpty() ? "-" : result.status);

    // 更新详细信息表格
    m_detailsTable->setRowCount(0);

    auto addRow = [this](const QString& field, const QString& value) {
        if (!value.isEmpty()) {
            int row = m_detailsTable->rowCount();
            m_detailsTable->insertRow(row);
            m_detailsTable->setItem(row, 0, new QTableWidgetItem(field));
            m_detailsTable->setItem(row, 1, new QTableWidgetItem(value));
        }
    };

    addRow("域名/IP", result.domain);
    addRow("注册商", result.registrar);
    addRow("注册人", result.registrant);
    addRow("注册时间", result.creationDate);
    addRow("到期时间", result.expirationDate);
    addRow("最后更新", result.lastUpdated);
    addRow("状态", result.status);
    addRow("名称服务器", result.nameServers);
    addRow("DNSSEC", result.dnssec);
    addRow("查询时间", result.queryTime.toString());

    m_detailsTable->resizeColumnsToContents();

    // 更新原始数据
    m_rawDataText->setPlainText(result.rawData);
}

// WhoisTool 主类实现
WhoisTool::WhoisTool(QWidget* parent) : QWidget(parent), DynamicObjectBase() {
    setupUI();

    // 连接信号，将查询结果添加到历史记录
    connect(m_singleQuery, &SingleWhoisQuery::onQueryFinished, this, &WhoisTool::onWhoisQueryFinished);
}

void WhoisTool::setupUI() {
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

    // 创建各个标签页
    m_singleQuery = new SingleWhoisQuery();
    m_tabWidget->addTab(m_singleQuery, "🔍 单个查询");

    // 暂时注释掉其他标签页，先实现基本功能
    // m_batchQuery = new BatchWhoisQuery();
    // m_tabWidget->addTab(m_batchQuery, "📊 批量查询");

    // m_history = new WhoisHistory();
    // m_tabWidget->addTab(m_history, "📚 查询历史");

    m_mainLayout->addWidget(m_tabWidget);
}

void WhoisTool::onWhoisQueryFinished(const WhoisResult& result) {
    // 这里可以添加到历史记录
    // if (m_history && result.isValid()) {
    //     m_history->addRecord(result);
    // }
}

// BatchWhoisQuery 实现
BatchWhoisQuery::BatchWhoisQuery(QWidget* parent) : QWidget(parent) {
    setupUI();
}

void BatchWhoisQuery::setupUI() {
    auto* layout = new QVBoxLayout(this);
    auto* label = new QLabel("批量WHOIS查询功能正在完善中...");
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet("font-size: 16px; color: #7f8c8d; font-style: italic;");
    layout->addWidget(label);
}

void BatchWhoisQuery::onStartBatchClicked() {}
void BatchWhoisQuery::onStopBatchClicked() {}
void BatchWhoisQuery::onClearAllClicked() {}
void BatchWhoisQuery::onImportFromFileClicked() {}
void BatchWhoisQuery::onExportResultsClicked() {}
void BatchWhoisQuery::onCopyAllResultsClicked() {}
void BatchWhoisQuery::onQueryFinished(const WhoisResult& result) { Q_UNUSED(result); }
void BatchWhoisQuery::onQueryProgress(const QString& message) { Q_UNUSED(message); }
void BatchWhoisQuery::onQueryError(const QString& error) { Q_UNUSED(error); }
void BatchWhoisQuery::processNextDomain() {}

// WhoisHistory 实现
WhoisHistory::WhoisHistory(QWidget* parent) : QWidget(parent) {
    setupUI();
}

void WhoisHistory::setupUI() {
    auto* layout = new QVBoxLayout(this);
    auto* label = new QLabel("WHOIS历史记录功能正在完善中...");
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet("font-size: 16px; color: #7f8c8d; font-style: italic;");
    layout->addWidget(label);
}

void WhoisHistory::onClearHistoryClicked() {}
void WhoisHistory::onExportHistoryClicked() {}
void WhoisHistory::onViewDetailsClicked() {}
void WhoisHistory::onDeleteSelectedClicked() {}

