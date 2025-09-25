#include "httpclient.h"
#include <algorithm>
#include <functional>

REGISTER_DYNAMICOBJECT(HttpClient);

HttpClient::HttpClient() : QWidget(nullptr), DynamicObjectBase() {
    m_networkManager = new QNetworkAccessManager(this);

    // 初始化存储管理器
    m_groupManager = new HttpClientStore::HttpRequestGroupManager(this);
    m_requestManager = new HttpClientStore::HttpRequestManager(this);

    setupUI();
    initializeStorage();
}

HttpClient::~HttpClient() {
    // 清理所有标签页资源
    for (const auto tab : m_tabMap) {
        if (tab->currentReply) {
            tab->currentReply->abort();
            tab->currentReply->deleteLater();
        }
        delete tab;
    }
}

void HttpClient::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    m_mainLayout->setSpacing(10);
    m_nextTabId = 0;

    setupToolbar();

    // 创建水平分割器（左右分隔）
    m_horizontalSplitter = new QSplitter(Qt::Horizontal, this);
    m_horizontalSplitter->setChildrenCollapsible(false);

    // 设置左侧面板
    setupRequestTreeView();

    // 创建右侧主标签页容器
    m_mainTabWidget = new QTabWidget();
    m_mainTabWidget->setTabsClosable(true);
    m_mainTabWidget->setMovable(true);
    m_mainTabWidget->setDocumentMode(true);

    // 添加新标签页按钮
    const auto newTabBtn = new QPushButton("+");
    newTabBtn->setFixedSize(25, 25);
    newTabBtn->setStyleSheet("QPushButton { font-weight: bold; font-size: 14px; }");
    m_mainTabWidget->setCornerWidget(newTabBtn, Qt::TopRightCorner);

    // 连接标签页信号
    connect(m_mainTabWidget, &QTabWidget::tabCloseRequested, this, &HttpClient::onTabCloseRequested);
    connect(m_mainTabWidget, &QTabWidget::currentChanged, this, &HttpClient::onTabChanged);
    connect(newTabBtn, &QPushButton::clicked, this, &HttpClient::onNewTabRequested);

    // 将左侧面板和右侧标签页添加到水平分割器
    m_horizontalSplitter->addWidget(m_leftPanel);
    m_horizontalSplitter->addWidget(m_mainTabWidget);
    m_horizontalSplitter->setSizes({ 250, 600 }); // 左侧250px，右侧600px

    m_mainLayout->addWidget(m_horizontalSplitter);

    // 创建第一个默认标签页
    createNewTab(tr("新请求"));

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
        "    padding: 2px;"
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
        "    border: 1px solid #c0c0c0;"
        "    border-bottom: none;"
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

void HttpClient::setupToolbar() {
    // cURL解析功能已移除，改为分组右键菜单中的导入功能
}


// 槽函数实现

// onParseCurlClicked function removed - functionality moved to import dialog

// onClearAllClicked function removed - clear all functionality no longer needed

void HttpClient::onSendRequestClicked() {
    HttpRequestTab* currentTab = getCurrentTab();
    if (!currentTab)
        return;

    QString host = currentTab->hostEdit->text().trimmed();
    if (host.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请输入主机地址"));
        return;
    }

    buildRequest();
    sendHttpRequest();
}

void HttpClient::onCancelRequestClicked() {
    HttpRequestTab* currentTab = getCurrentTab();
    if (!currentTab)
        return;

    // 停止计时器
    if (currentTab->requestTimer) {
        currentTab->requestTimer->stop();
    }

    if (currentTab->currentReply) {
        currentTab->currentReply->abort();
    }

    currentTab->sendBtn->setEnabled(true);
    currentTab->cancelBtn->setEnabled(false);
    currentTab->statusLabel->setText(tr("状态: 已取消"));
}

// 参数管理
void HttpClient::onAddParameterClicked() {
    HttpRequestTab* currentTab = getCurrentTab();
    if (!currentTab)
        return;

    int row = currentTab->paramsTable->rowCount();
    currentTab->paramsTable->insertRow(row);

    currentTab->paramsTable->setItem(row, 0, new QTableWidgetItem(""));
    currentTab->paramsTable->setItem(row, 1, new QTableWidgetItem(""));

    QCheckBox* checkBox = new QCheckBox();
    checkBox->setChecked(true);
    currentTab->paramsTable->setCellWidget(row, 2, checkBox);
}

void HttpClient::onRemoveParameterClicked() {
    HttpRequestTab* currentTab = getCurrentTab();
    if (!currentTab)
        return;

    removeSelectedTableRows(currentTab->paramsTable);
}

void HttpClient::onParameterChanged() {
    HttpRequestTab* currentTab = getCurrentTab();
    if (!currentTab)
        return;

    // 实时更新请求信息
    currentTab->requestInfo.parameters = getTableData(currentTab->paramsTable);
}

// 头部管理
void HttpClient::onAddHeaderClicked() {
    HttpRequestTab* currentTab = getCurrentTab();
    if (!currentTab)
        return;

    int row = currentTab->headersTable->rowCount();
    currentTab->headersTable->insertRow(row);

    currentTab->headersTable->setItem(row, 0, new QTableWidgetItem(""));
    currentTab->headersTable->setItem(row, 1, new QTableWidgetItem(""));

    QCheckBox* checkBox = new QCheckBox();
    checkBox->setChecked(true);
    currentTab->headersTable->setCellWidget(row, 2, checkBox);
}

void HttpClient::onRemoveHeaderClicked() {
    HttpRequestTab* currentTab = getCurrentTab();
    if (!currentTab)
        return;

    removeSelectedTableRows(currentTab->headersTable);
}

void HttpClient::onHeaderChanged() {
    HttpRequestTab* currentTab = getCurrentTab();
    if (!currentTab)
        return;

    currentTab->requestInfo.headers = getTableData(currentTab->headersTable);
}

// Cookie管理
void HttpClient::onAddCookieClicked() {
    HttpRequestTab* currentTab = getCurrentTab();
    if (!currentTab)
        return;

    int row = currentTab->cookiesTable->rowCount();
    currentTab->cookiesTable->insertRow(row);

    currentTab->cookiesTable->setItem(row, 0, new QTableWidgetItem(""));
    currentTab->cookiesTable->setItem(row, 1, new QTableWidgetItem(""));

    QCheckBox* checkBox = new QCheckBox();
    checkBox->setChecked(true);
    currentTab->cookiesTable->setCellWidget(row, 2, checkBox);
}

void HttpClient::onRemoveCookieClicked() {
    HttpRequestTab* currentTab = getCurrentTab();
    if (!currentTab)
        return;

    removeSelectedTableRows(currentTab->cookiesTable);
}

void HttpClient::onCookieChanged() {
    HttpRequestTab* currentTab = getCurrentTab();
    if (!currentTab)
        return;

    currentTab->requestInfo.cookies = getTableData(currentTab->cookiesTable);
}

// 响应处理
void HttpClient::onRequestFinished() {

    qDebug() << "HttpClient::onRequestFinished";

    HttpRequestTab* currentTab = getCurrentTab();
    if (!currentTab || !currentTab->currentReply)
        return;

    // 停止计时器
    if (currentTab->requestTimer) {
        currentTab->requestTimer->stop();
    }

    if (currentTab->elapsedTimer.isValid()) {
        qint64 elapsed = currentTab->elapsedTimer.elapsed();
        currentTab->timeLabel->setText(tr("时间: %1").arg(formatTime(elapsed)));
    }

    if (currentTab->currentReply->error() == QNetworkReply::NoError) {
        processResponse(currentTab->currentReply);
        displayResponse();
    }

    currentTab->currentReply->deleteLater();
    currentTab->currentReply = nullptr;

    currentTab->sendBtn->setEnabled(true);
    currentTab->cancelBtn->setEnabled(false);
}

void HttpClient::onRequestError(QNetworkReply::NetworkError error) {

    qDebug() << "HttpClient::onRequestError";

    Q_UNUSED(error);

    const HttpRequestTab* currentTab = getCurrentTab();
    if (!currentTab)
        return;

    // 停止计时器
    if (currentTab->requestTimer) {
        currentTab->requestTimer->stop();
    }

    if (currentTab->currentReply) {
        QString errorString = currentTab->currentReply->errorString();
        QVariant statusCodeVar = currentTab->currentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        int statusCode = statusCodeVar.isValid() ? statusCodeVar.toInt() : -1;

        currentTab->statusLabel->setText(
            tr("状态: 错误 - HTTP %1").arg(statusCode)
        );
        currentTab->responseBodyEdit->setText(
            tr("请求错误: HTTP %1, %2").arg(statusCode).arg(errorString)
        );

    }

    currentTab->sendBtn->setEnabled(true);
    currentTab->cancelBtn->setEnabled(false);
}

void HttpClient::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    // 进度条已移除，此方法不再执行任何操作
    Q_UNUSED(bytesReceived);
    Q_UNUSED(bytesTotal);
}

// 实用功能
void HttpClient::onCopyResponseClicked() {
    HttpRequestTab* currentTab = getCurrentTab();
    if (!currentTab)
        return;

    QString response = currentTab->responseBodyEdit->toPlainText();
    if (response.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("没有响应数据可复制"));
        return;
    }

    QApplication::clipboard()->setText(response);
    QMessageBox::information(this, tr("成功"), tr("响应已复制到剪贴板"));
}

void HttpClient::onFormatResponseClicked() {
    HttpRequestTab* tab = getCurrentTab();
    if (!tab)
        return;

    const QString response = tab->responseBodyEdit->toPlainText();
    if (response.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("没有响应数据可格式化"));
        return;
    }

    QString formatted;
    if (tab->responseInfo.contentType.contains("json", Qt::CaseInsensitive)) {
        formatted = formatJsonResponse(response);
    } else if (tab->responseInfo.contentType.contains("xml", Qt::CaseInsensitive)) {
        formatted = formatXmlResponse(response);
    } else {
        formatted = response;
    }

    tab->responseBodyEdit->setText(formatted);
}

void HttpClient::onSaveResponseClicked() {
    // 保存响应到文件
    QMessageBox::information(this, tr("提示"), tr("保存功能开发中..."));
}

// 工具函数实现
QString HttpClient::httpMethodToString(HttpMethod method) {
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

HttpMethod HttpClient::stringToHttpMethod(const QString& method) {
    QString upper = method.toUpper();
    if (upper == "POST")
        return HttpMethod::POST;
    if (upper == "PUT")
        return HttpMethod::PUT;
    if (upper == "DELETE")
        return HttpMethod::HTTP_DELETE;
    if (upper == "PATCH")
        return HttpMethod::PATCH;
    if (upper == "HEAD")
        return HttpMethod::HEAD;
    if (upper == "OPTIONS")
        return HttpMethod::OPTIONS;
    return HttpMethod::GET;
}

QString HttpClient::formatFileSize(const qint64 bytes) {
    constexpr qint64 KB = 1024;
    constexpr qint64 MB = KB * 1024;

    if (constexpr qint64 GB = MB * 1024; bytes >= GB) {
        return QString::number(bytes / GB, 'f', 2) + " GB";
    } else if (bytes >= MB) {
        return QString::number(bytes / MB, 'f', 2) + " MB";
    } else if (bytes >= KB) {
        return QString::number(bytes / KB, 'f', 2) + " KB";
    } else {
        return QString::number(bytes) + " B";
    }
}

QString HttpClient::formatTime(const qint64 milliseconds) {
    if (milliseconds < 1000) {
        return QString::number(milliseconds) + " ms";
    } else {
        return QString::number(milliseconds / 1000.0, 'f', 2) + " s";
    }
}

// parseCurlCommand function removed - functionality moved to import dialog

// extractUrl function removed - functionality moved to import dialog

// extractMethod function removed - functionality moved to import dialog

// extractHeaders function removed - functionality moved to import dialog

// extractBody function removed - functionality moved to import dialog

// extractCookies function removed - functionality moved to import dialog

void HttpClient::buildRequest() {
    HttpRequestTab* tab = getCurrentTab();
    if (!tab)
        return;

    tab->requestInfo.method = stringToHttpMethod(tab->methodCombo->currentText());
    tab->requestInfo.body = tab->bodyEdit->toPlainText();
    tab->requestInfo.parameters = getTableData(tab->paramsTable);
    tab->requestInfo.headers = getTableData(tab->headersTable);
    tab->requestInfo.cookies = getTableData(tab->cookiesTable);

    // 从分离的组件获取URL信息
    tab->requestInfo.protocol = tab->protocolCombo->currentText();
    tab->requestInfo.host = tab->hostEdit->text().trimmed();
    tab->requestInfo.port = tab->portSpin->value();
    tab->requestInfo.path = tab->pathEdit->text().trimmed();
}

QUrl HttpClient::buildUrl() const {
    HttpRequestTab* tab = getCurrentTab();
    if (!tab)
        return QUrl();

    QString protocol = tab->protocolCombo->currentText();
    QString host = tab->hostEdit->text().trimmed();
    int port = tab->portSpin->value();
    QString path = tab->pathEdit->text().trimmed();

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
    for (const auto& param : tab->requestInfo.parameters) {
        if (param.enabled && !param.key.isEmpty()) {
            query.addQueryItem(param.key, param.value);
        }
    }

    if (!query.isEmpty()) {
        url.setQuery(query);
    }

    return url;
}

void HttpClient::setRequestHeaders(QNetworkRequest& request) {
    HttpRequestTab* tab = getCurrentTab();
    if (!tab)
        return;

    // 设置请求头
    for (const auto& header : tab->requestInfo.headers) {
        if (header.enabled && !header.key.isEmpty()) {
            request.setRawHeader(header.key.toUtf8(), header.value.toUtf8());
        }
    }

    // 设置 Cookies
    if (!tab->requestInfo.cookies.isEmpty()) {
        QStringList cookieStrings;
        for (const auto& cookie : tab->requestInfo.cookies) {
            if (cookie.enabled && !cookie.key.isEmpty()) {
                cookieStrings.append(QString("%1=%2").arg(cookie.key, cookie.value));
            }
        }
        if (!cookieStrings.isEmpty()) {
            request.setRawHeader("Cookie", cookieStrings.join("; ").toUtf8());
        }
    }
}

QByteArray HttpClient::buildRequestBody() {
    HttpRequestTab* tab = getCurrentTab();
    if (!tab)
        return QByteArray();

    QString bodyType = tab->bodyTypeCombo->currentText();
    QString body = tab->bodyEdit->toPlainText();

    if (bodyType == tr("无请求体") || body.isEmpty()) {
        return QByteArray();
    }

    return body.toUtf8();
}

void HttpClient::sendHttpRequest() {
    HttpRequestTab* tab = getCurrentTab();
    if (!tab) {
        return;
    }

    const QUrl url = buildUrl();
    if (!url.isValid()) {
        QMessageBox::warning(this, tr("错误"), tr("无效的 URL"));
        return;
    }

    QNetworkRequest request(url);
    setRequestHeaders(request);

    // 设置User-Agent
    request.setRawHeader("User-Agent", "HTTP-Client-Tool/1.0");

    // 设置请求超时（10秒）
    request.setTransferTimeout(10000);

    // 获取请求体
    const QByteArray requestBody = buildRequestBody();

    // 发送请求
    const QString method = tab->methodCombo->currentText();

    // 连接信号（注意：这些信号在connectTabSignals中已经设置）

    // 启动计时器
    tab->elapsedTimer.start();
    tab->requestTimer->start();
    tab->timeLabel->setText(tr("时间: 0ms"));

    if (method == "GET") {
        tab->currentReply = m_networkManager->get(request);
    } else if (method == "POST") {
        tab->currentReply = m_networkManager->post(request, requestBody);
    } else if (method == "PUT") {
        tab->currentReply = m_networkManager->put(request, requestBody);
    } else if (method == "DELETE") {
        tab->currentReply = m_networkManager->deleteResource(request);
    } else if (method == "HEAD") {
        tab->currentReply = m_networkManager->head(request);
    } else {
        // 对于其他方法（PATCH, OPTIONS），使用自定义请求
        tab->currentReply = m_networkManager->sendCustomRequest(request, method.toUtf8(), requestBody);
    }

    if (!tab->currentReply) {
        return;
    }

    connect(tab->currentReply, &QNetworkReply::errorOccurred, this, [this, tab](QNetworkReply::NetworkError code) {
        this->onRequestError(code);
    });


    connect(tab->currentReply, &QNetworkReply::finished, this, [this, tab]() {
        this->onRequestFinished();
    });




    // 更新UI状态
    tab->sendBtn->setEnabled(false);
    tab->cancelBtn->setEnabled(true);
    tab->statusLabel->setText(tr("状态: 发送请求中..."));
}

void HttpClient::processResponse(QNetworkReply* reply) {
    HttpRequestTab* tab = getCurrentTab();
    if (!reply || !tab)
        return;

    // 获取响应码
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QString statusText = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();

    tab->responseInfo.statusCode = statusCode;
    tab->responseInfo.statusText = statusText;

    // 获取响应头
    tab->responseInfo.headers.clear();
    QList<QNetworkReply::RawHeaderPair> rawHeaders = reply->rawHeaderPairs();
    for (const auto& headerPair : rawHeaders) {
        QString key = QString::fromUtf8(headerPair.first);
        QString value = QString::fromUtf8(headerPair.second);
        tab->responseInfo.headers.append(KeyValuePair(key, value, true));

        // 获取Content-Type
        if (key.toLower() == "content-type") {
            tab->responseInfo.contentType = value;
        }
    }

    // 获取响应体
    tab->responseInfo.body = reply->readAll();
    tab->responseInfo.contentLength = tab->responseInfo.body.size();

    // 计算响应时间
    if (tab->elapsedTimer.isValid()) {
        tab->responseInfo.responseTime = tab->elapsedTimer.elapsed();
    }
}

void HttpClient::displayResponse() {
    HttpRequestTab* tab = getCurrentTab();
    if (!tab)
        return;

    // 更新状态标签
    QString statusColor = "#28a745"; // 绿色表示成功
    if (tab->responseInfo.statusCode >= 400) {
        statusColor = "#dc3545"; // 红色表示错误
    } else if (tab->responseInfo.statusCode >= 300) {
        statusColor = "#ffc107"; // 黄色表示重定向
    }

    tab->statusLabel->setText(tr("状态: %1 %2").arg(tab->responseInfo.statusCode).arg(tab->responseInfo.statusText));
    tab->statusLabel->setStyleSheet(QString("font-weight: bold; color: %1;").arg(statusColor));

    // 显示响应体
    QString responseBody = QString::fromUtf8(tab->responseInfo.body);
    tab->responseBodyEdit->setText(responseBody);

    // 显示原始响应
    QString rawResponse = QString("状态行: HTTP/1.1 %1 %2\n")
                          .arg(tab->responseInfo.statusCode)
                          .arg(tab->responseInfo.statusText);

    rawResponse += "响应头:\n";
    for (const auto& header : tab->responseInfo.headers) {
        rawResponse += QString("%1: %2\n").arg(header.key, header.value);
    }

    rawResponse += "\n响应体:\n";
    rawResponse += responseBody;

    tab->rawResponseEdit->setText(rawResponse);
}

QString HttpClient::formatJsonResponse(const QString& json) {
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &error);

    if (error.error != QJsonParseError::NoError) {
        return json; // 返回原始 JSON
    }

    return doc.toJson(QJsonDocument::Indented);
}

QString HttpClient::formatXmlResponse(const QString& xml) {
    // 简单的XML格式化（可以后续使用QDomDocument改进）
    QString formatted = xml;
    formatted.replace("><", ">\n<");
    return formatted;
}

void HttpClient::populateParametersTable() {
    // TODO: 填充参数表格
}

void HttpClient::populateHeadersTable() {
    // TODO: 填充头部表格
}

void HttpClient::populateCookiesTable() {
    // TODO: 填充Cookie表格
}

void HttpClient::addTableRow(QTableWidget* table, const KeyValuePair& pair) {
    int row = table->rowCount();
    table->insertRow(row);

    table->setItem(row, 0, new QTableWidgetItem(pair.key));
    table->setItem(row, 1, new QTableWidgetItem(pair.value));

    QCheckBox* checkBox = new QCheckBox();
    checkBox->setChecked(pair.enabled);
    table->setCellWidget(row, 2, checkBox);
}

void HttpClient::removeSelectedTableRows(QTableWidget* table) {
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

QList<KeyValuePair> HttpClient::getTableData(QTableWidget* table) {
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
void HttpClient::onProtocolChanged() {
    HttpRequestTab* tab = getCurrentTab();
    if (!tab)
        return;

    QString protocol = tab->protocolCombo->currentText();
    // 自动设置端口默认值
    if (protocol == "https") {
        tab->portSpin->setValue(443);
    } else if (protocol == "http") {
        tab->portSpin->setValue(80);
    }
    updateFullUrl();
}

void HttpClient::onHostChanged() {
    updateFullUrl();
}

void HttpClient::onPortChanged() {
    updateFullUrl();
}

void HttpClient::onPathChanged() {
    updateFullUrl();
}

void HttpClient::updateFullUrl() {
    HttpRequestTab* tab = getCurrentTab();
    if (!tab)
        return;

    QString protocol = tab->protocolCombo->currentText();
    QString host = tab->hostEdit->text().trimmed();
    int port = tab->portSpin->value();
    QString path = tab->pathEdit->text().trimmed();

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

void HttpClient::onFormatBodyClicked() {
    HttpRequestTab* tab = getCurrentTab();
    if (!tab)
        return;

    QString content = tab->bodyEdit->toPlainText().trimmed();
    if (content.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请先输入内容"));
        return;
    }

    QString bodyType = tab->bodyTypeCombo->currentText();
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
        tab->bodyEdit->setText(formatted);
        // 格式化成功不再弹出提示
    } else {
        QMessageBox::warning(this, tr("错误"), errorMsg);
    }
}

// ============ 请求存储管理功能实现 ============

void HttpClient::setupRequestTreeView() {
    // 创建左侧面板
    m_leftPanel = new QWidget();
    m_leftPanel->setMinimumWidth(200);
    m_leftPanel->setMaximumWidth(400);

    QVBoxLayout* leftLayout = new QVBoxLayout(m_leftPanel);
    leftLayout->setContentsMargins(5, 5, 5, 5);
    leftLayout->setSpacing(5);

    // 添加标题
    QLabel* titleLabel = new QLabel(tr("请求集合"));
    titleLabel->setStyleSheet("font-weight: bold; font-size: 12px; padding: 5px;");
    leftLayout->addWidget(titleLabel);

    // 创建树视图
    m_requestTreeView = new QTreeView();
    m_requestTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_requestTreeView->setHeaderHidden(true);
    m_requestTreeView->setRootIsDecorated(true);
    m_requestTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection); // 支持多选
    m_requestTreeView->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);

    // 启用拖拽功能
    m_requestTreeView->setDragEnabled(true);
    m_requestTreeView->setAcceptDrops(true);
    m_requestTreeView->setDropIndicatorShown(true);
    m_requestTreeView->setDragDropMode(QAbstractItemView::InternalMove);

    // 创建数据模型
    m_requestTreeModel = new HttpRequestTreeModel(this);
    m_requestTreeModel->setHorizontalHeaderLabels({ tr("名称") });
    m_requestTreeView->setModel(m_requestTreeModel);

    // 连接拖拽信号
    connect(m_requestTreeModel, &HttpRequestTreeModel::requestMoved,
            this, &HttpClient::onRequestMoved);
    connect(m_requestTreeModel, &HttpRequestTreeModel::multipleRequestsMoved,
            this, &HttpClient::onMultipleRequestsMoved);

    // 连接重命名信号
    connect(m_requestTreeModel, &HttpRequestTreeModel::requestRenamed,
            this, &HttpClient::onRequestRenamed);

    leftLayout->addWidget(m_requestTreeView);

    // 设置样式
    m_requestTreeView->setStyleSheet(
        "QTreeView {"
        "    border: 1px solid #ccc;"
        "    background-color: #ffffff;"
        "    selection-background-color: #3b82f6;"
        "    outline: none;"
        "}"
        "QTreeView::item {"
        "    height: 24px;"
        "    padding: 2px 4px;"
        "    border: none;"
        "}"
        "QTreeView::item:hover {"
        "    background-color: #f0f8ff;"
        "}"
        "QTreeView::item:selected {"
        "    background-color: #3b82f6;"
        "    color: white;"
        "}"
    );

    setupContextMenus();

    // 连接信号
    connect(m_requestTreeView, &QTreeView::clicked, this, &HttpClient::onRequestTreeClicked);
    connect(m_requestTreeView, &QTreeView::doubleClicked, this, &HttpClient::onRequestTreeDoubleClicked);
    connect(m_requestTreeView, &QTreeView::customContextMenuRequested, this, &HttpClient::onRequestTreeContextMenu);
}

void HttpClient::setupContextMenus() {
    // 分组上下文菜单
    m_groupContextMenu = new QMenu(this);
    m_addRequestAction = new QAction(tr("添加请求"), this);
    m_addRequestAction->setIcon(QIcon(":/icons/add.png"));

    m_renameAction = new QAction(tr("重命名"), this);
    m_renameAction->setIcon(QIcon(":/icons/rename.png"));

    m_deleteAction = new QAction(tr("删除"), this);
    m_deleteAction->setIcon(QIcon(":/icons/delete.png"));

    m_importAction = new QAction(tr("导入请求"), this);
    m_importAction->setIcon(QIcon(":/icons/import.png"));

    m_groupContextMenu->addAction(m_addRequestAction);
    m_groupContextMenu->addAction(m_importAction);
    m_groupContextMenu->addSeparator();
    m_groupContextMenu->addAction(m_renameAction);
    m_groupContextMenu->addAction(m_deleteAction);

    // 请求上下文菜单
    m_requestContextMenu = new QMenu(this);
    m_saveCurrentAction = new QAction(tr("保存当前请求"), this);
    m_saveCurrentAction->setIcon(QIcon(":/icons/save.png"));

    m_requestContextMenu->addAction(m_saveCurrentAction);
    m_requestContextMenu->addSeparator();
    m_requestContextMenu->addAction(m_renameAction);
    m_requestContextMenu->addAction(m_deleteAction);

    // 根菜单（在空白处右键）
    m_addGroupAction = new QAction(tr("添加分组"), this);
    m_addGroupAction->setIcon(QIcon(":/icons/folder.png"));

    // 连接槽函数
    connect(m_addGroupAction, &QAction::triggered, this, &HttpClient::onAddGroupTriggered);
    connect(m_addRequestAction, &QAction::triggered, this, &HttpClient::onAddRequestTriggered);
    connect(m_importAction, &QAction::triggered, this, &HttpClient::onImportTriggered);
    connect(m_renameAction, &QAction::triggered, this, &HttpClient::onRenameTriggered);
    connect(m_deleteAction, &QAction::triggered, this, &HttpClient::onDeleteTriggered);
    connect(m_saveCurrentAction, &QAction::triggered, this, &HttpClient::onSaveCurrentTriggered);
}

void HttpClient::initializeStorage() {
    // 检查是否需要创建默认分组
    if (m_groupManager->getGroupCount() == 0) {
        HttpClientStore::HttpRequestGroup defaultGroup(tr("默认分组"));
        m_groupManager->saveGroup(defaultGroup);
    }

    // 加载请求树
    loadRequestTree();
}

void HttpClient::loadRequestTree() {
    m_requestTreeModel->clear();
    m_requestTreeModel->setHorizontalHeaderLabels({ tr("名称") });

    // 加载所有分组
    QList<HttpClientStore::HttpRequestGroup> groups = m_groupManager->getAllGroups();
    for (const auto& group : groups) {
        QStandardItem* groupItem = new QStandardItem(group.name);
        groupItem->setData(group.id, Qt::UserRole); // 存储分组ID
        groupItem->setData("group", Qt::UserRole + 1); // 标记为分组
        groupItem->setIcon(QIcon(":/icons/folder.png"));
        groupItem->setEditable(false);

        m_requestTreeModel->appendRow(groupItem);

        // 加载该分组下的请求
        loadRequestsForGroup(groupItem, group.id);

        // 展开分组
        m_requestTreeView->expand(groupItem->index());
    }
}

void HttpClient::loadRequestsForGroup(QStandardItem* groupItem, int groupId) {
    QList<HttpClientStore::HttpRequestEntity> requests = m_requestManager->getRequestsByGroupId(groupId);
    for (const auto& request : requests) {
        // 格式化显示文本：[方法] 请求名称
        QString method = request.method.toUpper();
        if (method.isEmpty())
            method = "GET"; // 默认为GET
        QString displayText = QString("[%1] %2").arg(method).arg(request.name);

        QStandardItem* requestItem = new QStandardItem(displayText);
        requestItem->setData(request.id, Qt::UserRole); // 存储请求ID
        requestItem->setData("request", Qt::UserRole + 1); // 标记为请求
        requestItem->setData(request.name, Qt::UserRole + 2); // 存储纯请求名称
        requestItem->setIcon(QIcon(":/icons/http.png"));
        requestItem->setEditable(true); // 允许编辑

        // 根据HTTP方法设置不同的图标和颜色
        if (method == "GET") {
            requestItem->setForeground(QBrush(QColor("#2563eb")));
        } else if (method == "POST") {
            requestItem->setForeground(QBrush(QColor("#dc2626")));
        } else if (method == "PUT") {
            requestItem->setForeground(QBrush(QColor("#ea580c")));
        } else if (method == "DELETE") {
            requestItem->setForeground(QBrush(QColor("#dc2626")));
        }

        groupItem->appendRow(requestItem);
    }
}

// ============ 槽函数实现 ============

void HttpClient::onRequestTreeClicked(const QModelIndex& index) {
    if (!index.isValid())
        return;

    QStandardItem* item = m_requestTreeModel->itemFromIndex(index);
    if (!item)
        return;

    QString itemType = item->data(Qt::UserRole + 1).toString();
    if (itemType == "request") {
        int requestId = item->data(Qt::UserRole).toInt();

        // 检查是否已经有对应的标签页打开
        HttpRequestTab* existingTab = getTabByRequestId(requestId);
        if (existingTab) {
            // 如果已经打开，切换到该标签页（阻塞信号避免循环调用）
            m_mainTabWidget->blockSignals(true);
            setCurrentTab(existingTab);
            m_mainTabWidget->blockSignals(false);
        } else {
            // 如果未打开，创建新标签页
            HttpClientStore::HttpRequestEntity request = m_requestManager->getRequestById(requestId);
            if (request.isValid()) {
                createTabFromRequest(request);
            }
        }
    }
}

void HttpClient::onRequestTreeDoubleClicked(const QModelIndex& index) {
    if (!index.isValid())
        return;

    QStandardItem* item = m_requestTreeModel->itemFromIndex(index);
    if (!item)
        return;

    QString itemType = item->data(Qt::UserRole + 1).toString();

    if (itemType == "request") {
        // 请求节点双击进入编辑模式
        m_requestTreeView->edit(index);
    } else {
        // 分组节点双击行为和单击一样，定位到对应的标签页
        onRequestTreeClicked(index);
    }
}

void HttpClient::onRequestTreeContextMenu(const QPoint& position) {
    QModelIndex index = m_requestTreeView->indexAt(position);

    if (index.isValid()) {
        QStandardItem* item = m_requestTreeModel->itemFromIndex(index);
        QString itemType = item->data(Qt::UserRole + 1).toString();

        if (itemType == "group") {
            m_groupContextMenu->exec(m_requestTreeView->mapToGlobal(position));
        } else if (itemType == "request") {
            m_requestContextMenu->exec(m_requestTreeView->mapToGlobal(position));
        }
    } else {
        // 在空白处右键，显示添加分组菜单
        QMenu contextMenu;
        contextMenu.addAction(m_addGroupAction);
        contextMenu.exec(m_requestTreeView->mapToGlobal(position));
    }
}

void HttpClient::onAddGroupTriggered() {
    bool ok;
    QString groupName = QInputDialog::getText(this, tr("添加分组"), tr("分组名称:"), QLineEdit::Normal, "", &ok);

    if (ok && !groupName.isEmpty()) {
        if (m_groupManager->groupExists(groupName)) {
            QMessageBox::warning(this, tr("错误"), tr("分组名称已存在"));
            return;
        }

        HttpClientStore::HttpRequestGroup group(groupName);
        if (m_groupManager->saveGroup(group)) {
            refreshTreeView();
        } else {
            QMessageBox::warning(this, tr("错误"), tr("分组添加失败"));
        }
    }
}

void HttpClient::onAddRequestTriggered() {
    QModelIndex index = m_requestTreeView->currentIndex();
    if (!index.isValid())
        return;

    QStandardItem* item = m_requestTreeModel->itemFromIndex(index);
    if (!item)
        return;

    // 获取分组ID
    int groupId = 0;
    QString itemType = item->data(Qt::UserRole + 1).toString();
    if (itemType == "group") {
        groupId = item->data(Qt::UserRole).toInt();
    } else if (itemType == "request") {
        // 如果点击的是请求，获取其父分组
        QStandardItem* parentItem = item->parent();
        if (parentItem) {
            groupId = parentItem->data(Qt::UserRole).toInt();
        }
    }

    if (groupId == 0)
        return;

    bool ok;
    QString requestName = QInputDialog::getText(this, tr("添加请求"), tr("请求名称:"), QLineEdit::Normal, "", &ok);

    if (ok && !requestName.isEmpty()) {
        if (m_requestManager->requestExists(requestName, groupId)) {
            QMessageBox::warning(this, tr("错误"), tr("请求名称在该分组中已存在"));
            return;
        }

        // 从当前UI创建请求
        HttpClientStore::HttpRequestEntity request = createRequestFromCurrentUI(groupId, requestName);
        if (m_requestManager->saveRequest(request)) {
            refreshTreeView();
        } else {
            QMessageBox::warning(this, tr("错误"), tr("请求添加失败"));
        }
    }
}

void HttpClient::onRenameTriggered() {
    QModelIndex index = m_requestTreeView->currentIndex();
    if (!index.isValid())
        return;

    QStandardItem* item = m_requestTreeModel->itemFromIndex(index);
    if (!item)
        return;

    QString itemType = item->data(Qt::UserRole + 1).toString();
    bool ok;
    QString newName = QInputDialog::getText(this, tr("重命名"), tr("新名称:"), QLineEdit::Normal, item->text(), &ok);

    if (ok && !newName.isEmpty() && newName != item->text()) {
        if (itemType == "group") {
            int groupId = item->data(Qt::UserRole).toInt();
            if (m_groupManager->groupExists(newName)) {
                QMessageBox::warning(this, tr("错误"), tr("分组名称已存在"));
                return;
            }

            if (m_groupManager->renameGroup(groupId, newName)) {
                refreshTreeView();
                QMessageBox::information(this, tr("成功"), tr("重命名成功"));
            } else {
                QMessageBox::warning(this, tr("错误"), tr("重命名失败"));
            }
        } else if (itemType == "request") {
            int requestId = item->data(Qt::UserRole).toInt();
            QStandardItem* parentItem = item->parent();
            if (parentItem) {
                int groupId = parentItem->data(Qt::UserRole).toInt();
                if (m_requestManager->requestExists(newName, groupId)) {
                    QMessageBox::warning(this, tr("错误"), tr("请求名称在该分组中已存在"));
                    return;
                }
            }

            if (m_requestManager->renameRequest(requestId, newName)) {
                // 更新对应的tab标题
                HttpRequestTab* tab = getTabByRequestId(requestId);
                if (tab) {
                    tab->tabName = newName;
                    updateTabTitle(tab);
                }
                refreshTreeView();
            } else {
                QMessageBox::warning(this, tr("错误"), tr("重命名失败"));
            }
        }
    }
}

void HttpClient::onDeleteTriggered() {
    QModelIndex index = m_requestTreeView->currentIndex();
    if (!index.isValid())
        return;

    QStandardItem* item = m_requestTreeModel->itemFromIndex(index);
    if (!item)
        return;

    QString itemType = item->data(Qt::UserRole + 1).toString();

    int ret = QMessageBox::question(this, tr("确认删除"),
                                    tr("确定要删除 \"%1\" 吗？").arg(item->text()),
                                    QMessageBox::Yes | QMessageBox::No,
                                    QMessageBox::No);

    if (ret == QMessageBox::Yes) {
        if (itemType == "group") {
            int groupId = item->data(Qt::UserRole).toInt();
            // 先删除分组下的所有请求
            m_requestManager->deleteRequestsByGroupId(groupId);
            // 再删除分组
            if (m_groupManager->deleteGroup(groupId)) {
                refreshTreeView();
                QMessageBox::information(this, tr("成功"), tr("删除成功"));
            } else {
                QMessageBox::warning(this, tr("错误"), tr("删除失败"));
            }
        } else if (itemType == "request") {
            int requestId = item->data(Qt::UserRole).toInt();
            if (m_requestManager->deleteRequest(requestId)) {
                refreshTreeView();
                QMessageBox::information(this, tr("成功"), tr("删除成功"));
            } else {
                QMessageBox::warning(this, tr("错误"), tr("删除失败"));
            }
        }
    }
}

void HttpClient::onSaveCurrentTriggered() {
    // 获取当前标签页
    HttpRequestTab* currentTab = getCurrentTab();
    if (!currentTab) {
        QMessageBox::warning(this, tr("提示"), tr("没有打开的请求标签页"));
        return;
    }

    QModelIndex index = m_requestTreeView->currentIndex();
    if (!index.isValid()) {
        QMessageBox::warning(this, tr("提示"), tr("请先在左侧树中选择一个分组或请求"));
        return;
    }

    QStandardItem* item = m_requestTreeModel->itemFromIndex(index);
    if (!item)
        return;

    QString itemType = item->data(Qt::UserRole + 1).toString();
    int groupId = 0;

    if (itemType == "request") {
        // 选中的是请求节点，更新现有请求
        int requestId = item->data(Qt::UserRole).toInt();

        // 获取分组ID
        QStandardItem* parentItem = item->parent();
        if (!parentItem)
            return;
        groupId = parentItem->data(Qt::UserRole).toInt();

        // 从当前UI创建请求数据，使用存储的纯请求名称
        QString requestName = item->data(Qt::UserRole + 2).toString(); // 获取纯请求名称
        if (requestName.isEmpty()) {
            // 如果没有存储纯名称，从显示文本中提取
            QString displayText = item->text();
            QRegularExpression regex(R"(\[.*?\]\s*(.+))");
            QRegularExpressionMatch match = regex.match(displayText);
            requestName = match.hasMatch() ? match.captured(1) : displayText;
        }

        HttpClientStore::HttpRequestEntity request = createRequestFromCurrentUI(groupId, requestName);
        request.updatedAt = QDateTime::currentDateTime();

        if (m_requestManager->updateRequest(requestId, request)) {
            // 更新对应的标签页信息
            currentTab->requestId = requestId;
            currentTab->groupId = groupId;
            currentTab->tabName = requestName;
            updateTabTitle(currentTab);

            // 刷新树视图以显示最新的方法前缀
            refreshTreeView();
        } else {
            QMessageBox::warning(this, tr("错误"), tr("更新请求失败"));
        }
    } else if (itemType == "group") {
        // 选中的是分组节点，创建新请求
        groupId = item->data(Qt::UserRole).toInt();

        // 弹出对话框让用户输入请求名称
        bool ok;
        QString requestName = QInputDialog::getText(this, tr("新建请求"),
                                                    tr("请输入请求名称:"),
                                                    QLineEdit::Normal,
                                                    tr("新请求"), &ok);

        if (!ok || requestName.trimmed().isEmpty()) {
            return;
        }

        requestName = requestName.trimmed();

        // 检查同名请求是否存在
        if (m_requestManager->requestExists(requestName, groupId)) {
            QMessageBox::warning(this, tr("错误"), tr("该分组下已存在同名请求"));
            return;
        }

        // 从当前UI创建请求数据
        HttpClientStore::HttpRequestEntity request = createRequestFromCurrentUI(groupId, requestName);

        // 确保新请求有基本的默认值
        if (request.method.isEmpty())
            request.method = "GET";
        if (request.protocol.isEmpty())
            request.protocol = "https";
        if (request.host.isEmpty())
            request.host = "example.com";
        if (request.path.isEmpty())
            request.path = "/";
        if (request.port <= 0)
            request.port = 443;
        request.createdAt = QDateTime::currentDateTime();
        request.updatedAt = QDateTime::currentDateTime();

        if (m_requestManager->saveRequest(request)) {
            // 刷新树视图
            refreshTreeView();

            // 获取新创建请求的ID并更新标签页
            // 由于我们刚刚保存了请求，最新的请求应该在列表末尾
            QList<HttpClientStore::HttpRequestEntity> groupRequests = m_requestManager->getRequestsByGroupId(groupId);
            if (!groupRequests.isEmpty()) {
                HttpClientStore::HttpRequestEntity savedRequest = groupRequests.last();
                currentTab->requestId = savedRequest.id;
                currentTab->groupId = groupId;
                currentTab->tabName = requestName;
                updateTabTitle(currentTab);

                // 将标签页与请求ID关联
                m_requestTabMap[savedRequest.id] = m_mainTabWidget->currentIndex();
            }

            // 自动选择新创建的请求
            QTimer::singleShot(100, [this, requestName, groupId]() {
                selectRequestInTree(requestName, groupId);
            });
        } else {
            QMessageBox::warning(this, tr("错误"), tr("保存请求失败"));
        }
    } else {
        QMessageBox::warning(this, tr("提示"), tr("请选择一个分组来保存新请求，或选择现有请求进行更新"));
    }
}

// ============ 辅助函数实现 ============

void HttpClient::populateRequestFromEntity(const HttpClientStore::HttpRequestEntity& request) {
    HttpRequestTab* tab = getCurrentTab();
    if (!tab)
        return;

    // 设置请求方法
    tab->methodCombo->setCurrentText(request.method);

    // 设置URL组件
    tab->protocolCombo->setCurrentText(request.protocol);
    tab->hostEdit->setText(request.host);
    tab->portSpin->setValue(request.port);
    tab->pathEdit->setText(request.path);

    // cURL区域已移除，跳过设置

    // 清空表格并填充参数
    tab->paramsTable->setRowCount(0);
    QList<QVariantMap> params = HttpClientStore::HttpRequestEntity::parametersFromJson(request.parameters);
    for (const auto& param : params) {
        int row = tab->paramsTable->rowCount();
        tab->paramsTable->insertRow(row);
        tab->paramsTable->setItem(row, 0, new QTableWidgetItem(param["key"].toString()));
        tab->paramsTable->setItem(row, 1, new QTableWidgetItem(param["value"].toString()));

        QCheckBox* checkBox = new QCheckBox();
        checkBox->setChecked(param["enabled"].toBool());
        tab->paramsTable->setCellWidget(row, 2, checkBox);
    }

    // 清空表格并填充请求头
    tab->headersTable->setRowCount(0);
    QList<QVariantMap> headers = HttpClientStore::HttpRequestEntity::headersFromJson(request.headers);
    for (const auto& header : headers) {
        int row = tab->headersTable->rowCount();
        tab->headersTable->insertRow(row);
        tab->headersTable->setItem(row, 0, new QTableWidgetItem(header["key"].toString()));
        tab->headersTable->setItem(row, 1, new QTableWidgetItem(header["value"].toString()));

        QCheckBox* checkBox = new QCheckBox();
        checkBox->setChecked(header["enabled"].toBool());
        tab->headersTable->setCellWidget(row, 2, checkBox);
    }

    // 清空表格并填充Cookies
    tab->cookiesTable->setRowCount(0);
    QList<QVariantMap> cookies = HttpClientStore::HttpRequestEntity::cookiesFromJson(request.cookies);
    for (const auto& cookie : cookies) {
        int row = tab->cookiesTable->rowCount();
        tab->cookiesTable->insertRow(row);
        tab->cookiesTable->setItem(row, 0, new QTableWidgetItem(cookie["key"].toString()));
        tab->cookiesTable->setItem(row, 1, new QTableWidgetItem(cookie["value"].toString()));

        QCheckBox* checkBox = new QCheckBox();
        checkBox->setChecked(cookie["enabled"].toBool());
        tab->cookiesTable->setCellWidget(row, 2, checkBox);
    }

    // 设置请求体
    tab->bodyTypeCombo->setCurrentText(request.bodyType);
    tab->bodyEdit->setPlainText(request.body);
}

HttpClientStore::HttpRequestEntity HttpClient::createRequestFromCurrentUI(int groupId, const QString& name) {
    HttpClientStore::HttpRequestEntity request(groupId, name);

    HttpRequestTab* tab = getCurrentTab();
    if (!tab) {
        // 如果没有当前标签页，创建一个新的默认标签页
        tab = createNewTab(name, 0, groupId);
        if (!tab)
            return request;
    }

    // 设置基本信息
    request.method = tab->methodCombo->currentText();
    request.protocol = tab->protocolCombo->currentText();
    request.host = tab->hostEdit->text().trimmed();
    request.port = tab->portSpin->value();
    request.path = tab->pathEdit->text().trimmed();
    request.curlCommand = ""; // cURL区域已移除
    request.bodyType = tab->bodyTypeCombo->currentText();
    request.body = tab->bodyEdit->toPlainText();

    // 收集参数
    QList<QVariantMap> params;
    for (int i = 0; i < tab->paramsTable->rowCount(); ++i) {
        QVariantMap param;
        QTableWidgetItem* keyItem = tab->paramsTable->item(i, 0);
        QTableWidgetItem* valueItem = tab->paramsTable->item(i, 1);
        QCheckBox* checkBox = qobject_cast<QCheckBox*>(tab->paramsTable->cellWidget(i, 2));

        if (keyItem && valueItem && checkBox) {
            param["key"] = keyItem->text();
            param["value"] = valueItem->text();
            param["enabled"] = checkBox->isChecked();
            params.append(param);
        }
    }
    request.parameters = HttpClientStore::HttpRequestEntity::parametersToJson(params);

    // 收集请求头
    QList<QVariantMap> headers;
    for (int i = 0; i < tab->headersTable->rowCount(); ++i) {
        QVariantMap header;
        QTableWidgetItem* keyItem = tab->headersTable->item(i, 0);
        QTableWidgetItem* valueItem = tab->headersTable->item(i, 1);
        QCheckBox* checkBox = qobject_cast<QCheckBox*>(tab->headersTable->cellWidget(i, 2));

        if (keyItem && valueItem && checkBox) {
            header["key"] = keyItem->text();
            header["value"] = valueItem->text();
            header["enabled"] = checkBox->isChecked();
            headers.append(header);
        }
    }
    request.headers = HttpClientStore::HttpRequestEntity::headersToJson(headers);

    // 收集Cookies
    QList<QVariantMap> cookies;
    for (int i = 0; i < tab->cookiesTable->rowCount(); ++i) {
        QVariantMap cookie;
        QTableWidgetItem* keyItem = tab->cookiesTable->item(i, 0);
        QTableWidgetItem* valueItem = tab->cookiesTable->item(i, 1);
        QCheckBox* checkBox = qobject_cast<QCheckBox*>(tab->cookiesTable->cellWidget(i, 2));

        if (keyItem && valueItem && checkBox) {
            cookie["key"] = keyItem->text();
            cookie["value"] = valueItem->text();
            cookie["enabled"] = checkBox->isChecked();
            cookies.append(cookie);
        }
    }
    request.cookies = HttpClientStore::HttpRequestEntity::cookiesToJson(cookies);

    return request;
}

QStandardItem* HttpClient::findGroupItem(int groupId) {
    for (int i = 0; i < m_requestTreeModel->rowCount(); ++i) {
        QStandardItem* item = m_requestTreeModel->item(i);
        if (item && item->data(Qt::UserRole).toInt() == groupId) {
            return item;
        }
    }
    return nullptr;
}

QStandardItem* HttpClient::findRequestItem(int requestId) {
    for (int i = 0; i < m_requestTreeModel->rowCount(); ++i) {
        QStandardItem* groupItem = m_requestTreeModel->item(i);
        if (groupItem) {
            for (int j = 0; j < groupItem->rowCount(); ++j) {
                QStandardItem* requestItem = groupItem->child(j);
                if (requestItem && requestItem->data(Qt::UserRole).toInt() == requestId) {
                    return requestItem;
                }
            }
        }
    }
    return nullptr;
}

void HttpClient::refreshTreeView() {
    loadRequestTree();
}

void HttpClient::selectRequestInTree(const QString& requestName, int groupId) {
    // 查找指定分组下的请求节点并选中
    QStandardItem* groupItem = findGroupItem(groupId);
    if (!groupItem)
        return;

    for (int i = 0; i < groupItem->rowCount(); ++i) {
        QStandardItem* requestItem = groupItem->child(i);
        if (requestItem && requestItem->text() == requestName) {
            // 展开分组
            QModelIndex groupIndex = m_requestTreeModel->indexFromItem(groupItem);
            m_requestTreeView->expand(groupIndex);

            // 选中请求节点
            QModelIndex requestIndex = m_requestTreeModel->indexFromItem(requestItem);
            m_requestTreeView->setCurrentIndex(requestIndex);

            // 确保节点可见
            m_requestTreeView->scrollTo(requestIndex);
            break;
        }
    }
}

// ============ 标签页管理功能实现 ============

HttpRequestTab* HttpClient::createNewTab(const QString& tabName, int requestId, int groupId) {
    HttpRequestTab* tab = new HttpRequestTab();
    tab->requestId = requestId;
    tab->groupId = groupId;
    tab->tabName = tabName;

    // 创建标签页的主容器
    tab->tabWidget = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(tab->tabWidget);

    // 创建垂直分割器（上下分隔）
    QSplitter* splitter = new QSplitter(Qt::Vertical);

    // 设置标签页UI
    setupTabUI(tab);

    // 添加到分割器
    QWidget* requestWidget = new QWidget();
    QVBoxLayout* requestLayout = new QVBoxLayout(requestWidget);
    requestLayout->addWidget(tab->requestGroup);

    QWidget* responseWidget = new QWidget();
    QVBoxLayout* responseLayout = new QVBoxLayout(responseWidget);
    responseLayout->addWidget(tab->responseGroup);

    splitter->addWidget(requestWidget);
    splitter->addWidget(responseWidget);
    splitter->setSizes({ 400, 300 });

    mainLayout->addWidget(splitter);

    // 连接信号
    connectTabSignals(tab);

    // 添加到主标签页容器
    int index = m_mainTabWidget->addTab(tab->tabWidget, tabName);
    m_tabMap[index] = tab;

    if (requestId > 0) {
        m_requestTabMap[requestId] = index;
    }

    // 切换到新标签页
    m_mainTabWidget->setCurrentIndex(index);

    return tab;
}

HttpRequestTab* HttpClient::createTabFromRequest(const HttpClientStore::HttpRequestEntity& request) {
    QString tabName = QString("[%1] %2").arg(request.method).arg(request.name);
    HttpRequestTab* tab = createNewTab(tabName, request.id, request.groupId);

    // 填充请求数据
    populateTabFromEntity(tab, request);

    return tab;
}

void HttpClient::setupTabUI(HttpRequestTab* tab) {
    // 请求区域
    tab->requestGroup = new QGroupBox(tr("HTTP 请求"));
    QVBoxLayout* groupLayout = new QVBoxLayout(tab->requestGroup);

    // 方法、协议、端口、主机在一行
    QHBoxLayout* topLayout = new QHBoxLayout();

    // 方法
    topLayout->addWidget(new QLabel(tr("方法:")));
    tab->methodCombo = new QComboBox();
    tab->methodCombo->addItems({ "GET", "POST", "PUT", "DELETE", "PATCH", "HEAD", "OPTIONS" });
    tab->methodCombo->setMinimumWidth(80);
    tab->methodCombo->setMaximumWidth(80);
    topLayout->addWidget(tab->methodCombo);

    topLayout->addSpacing(10);

    // 协议
    topLayout->addWidget(new QLabel(tr("协议:")));
    tab->protocolCombo = new QComboBox();
    tab->protocolCombo->addItems({ "https", "http" });
    tab->protocolCombo->setMinimumWidth(70);
    tab->protocolCombo->setMaximumWidth(70);
    topLayout->addWidget(tab->protocolCombo);

    topLayout->addSpacing(10);

    // 端口
    topLayout->addWidget(new QLabel(tr("端口:")));
    tab->portSpin = new QSpinBox();
    tab->portSpin->setRange(1, 65535);
    tab->portSpin->setValue(443);
    tab->portSpin->setMinimumWidth(70);
    tab->portSpin->setMaximumWidth(70);
    topLayout->addWidget(tab->portSpin);

    topLayout->addSpacing(10);

    // 主机
    topLayout->addWidget(new QLabel(tr("主机:")));
    tab->hostEdit = new QLineEdit();
    tab->hostEdit->setPlaceholderText(tr("例如: api.example.com"));
    tab->hostEdit->setMinimumWidth(200);
    topLayout->addWidget(tab->hostEdit, 1);

    groupLayout->addLayout(topLayout);

    // 路径单独一行
    QHBoxLayout* pathLayout = new QHBoxLayout();
    pathLayout->addWidget(new QLabel(tr("路径:")));
    tab->pathEdit = new QLineEdit();
    tab->pathEdit->setPlaceholderText(tr("例如: /api/v1/users"));
    tab->pathEdit->setText("/");
    pathLayout->addWidget(tab->pathEdit);
    groupLayout->addLayout(pathLayout);

    // 按钮行
    auto* buttonLayout = new QHBoxLayout();
    tab->sendBtn = new QPushButton(tr("发送请求"));
    tab->sendBtn->setStyleSheet("QPushButton { background-color: #28a745; color: white; font-weight: bold; }");
    tab->sendBtn->setMinimumWidth(100);
    buttonLayout->addWidget(tab->sendBtn);

    tab->cancelBtn = new QPushButton(tr("取消"));
    tab->cancelBtn->setStyleSheet("QPushButton { background-color: #6c757d; color: white; }");
    tab->cancelBtn->setMinimumWidth(80);
    tab->cancelBtn->setEnabled(false);
    buttonLayout->addWidget(tab->cancelBtn);

    tab->saveBtn = new QPushButton(tr("保存"));
    tab->saveBtn->setStyleSheet("QPushButton { background-color: #007bff; color: white; }");
    tab->saveBtn->setMinimumWidth(80);
    buttonLayout->addWidget(tab->saveBtn);
    buttonLayout->addStretch();

    groupLayout->addLayout(buttonLayout);

    // 请求详情标签页
    tab->requestTabs = new QTabWidget();

    // 参数标签页
    setupTabParametersTab(tab);
    // 请求头标签页
    setupTabHeadersTab(tab);
    // Cookie标签页
    setupTabCookiesTab(tab);
    // 请求体标签页
    setupTabBodyTab(tab);

    groupLayout->addWidget(tab->requestTabs);

    // 响应区域
    setupTabResponseArea(tab);
}

void HttpClient::setupTabParametersTab(HttpRequestTab* tab) {
    tab->paramsTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab->paramsTab);

    // 参数表格
    tab->paramsTable = new QTableWidget(0, 3);
    tab->paramsTable->setHorizontalHeaderLabels({ tr("键"), tr("值"), tr("启用") });
    tab->paramsTable->horizontalHeader()->setStretchLastSection(false);
    tab->paramsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    tab->paramsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    tab->paramsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    tab->paramsTable->setColumnWidth(2, 60);

    layout->addWidget(tab->paramsTable);

    // 按钮行
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    tab->addParamBtn = new QPushButton(tr("添加"));
    tab->removeParamBtn = new QPushButton(tr("删除"));
    buttonLayout->addWidget(tab->addParamBtn);
    buttonLayout->addWidget(tab->removeParamBtn);
    buttonLayout->addStretch();
    layout->addLayout(buttonLayout);

    tab->requestTabs->addTab(tab->paramsTab, tr("参数"));
}

void HttpClient::setupTabHeadersTab(HttpRequestTab* tab) {
    tab->headersTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab->headersTab);

    // 请求头表格
    tab->headersTable = new QTableWidget(0, 3);
    tab->headersTable->setHorizontalHeaderLabels({ tr("键"), tr("值"), tr("启用") });
    tab->headersTable->horizontalHeader()->setStretchLastSection(false);
    tab->headersTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    tab->headersTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    tab->headersTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    tab->headersTable->setColumnWidth(2, 60);

    layout->addWidget(tab->headersTable);

    // 按钮行
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    tab->addHeaderBtn = new QPushButton(tr("添加"));
    tab->removeHeaderBtn = new QPushButton(tr("删除"));
    buttonLayout->addWidget(tab->addHeaderBtn);
    buttonLayout->addWidget(tab->removeHeaderBtn);
    buttonLayout->addStretch();
    layout->addLayout(buttonLayout);

    tab->requestTabs->addTab(tab->headersTab, tr("请求头"));
}

void HttpClient::setupTabCookiesTab(HttpRequestTab* tab) {
    tab->cookiesTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab->cookiesTab);

    // Cookie表格
    tab->cookiesTable = new QTableWidget(0, 3);
    tab->cookiesTable->setHorizontalHeaderLabels({ tr("键"), tr("值"), tr("启用") });
    tab->cookiesTable->horizontalHeader()->setStretchLastSection(false);
    tab->cookiesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    tab->cookiesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    tab->cookiesTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    tab->cookiesTable->setColumnWidth(2, 60);

    layout->addWidget(tab->cookiesTable);

    // 按钮行
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    tab->addCookieBtn = new QPushButton(tr("添加"));
    tab->removeCookieBtn = new QPushButton(tr("删除"));
    buttonLayout->addWidget(tab->addCookieBtn);
    buttonLayout->addWidget(tab->removeCookieBtn);
    buttonLayout->addStretch();
    layout->addLayout(buttonLayout);

    tab->requestTabs->addTab(tab->cookiesTab, tr("Cookies"));
}

void HttpClient::setupTabBodyTab(HttpRequestTab* tab) {
    tab->bodyTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab->bodyTab);

    // 请求体类型选择和格式化按钮在一行
    QHBoxLayout* topLayout = new QHBoxLayout();
    topLayout->addWidget(new QLabel(tr("类型:")));

    tab->bodyTypeCombo = new QComboBox();
    tab->bodyTypeCombo->addItems({ tr("无请求体"), "raw", "JSON", "XML", "form-data", "x-www-form-urlencoded" });
    tab->bodyTypeCombo->setMinimumWidth(150);
    topLayout->addWidget(tab->bodyTypeCombo);

    // 格式化按钮
    tab->formatBodyBtn = new QPushButton(tr("格式化"));
    tab->formatBodyBtn->setVisible(false);
    topLayout->addWidget(tab->formatBodyBtn);

    topLayout->addStretch();
    layout->addLayout(topLayout);

    // 请求体内容
    tab->bodyEdit = new QTextEdit();
    tab->bodyEdit->setPlaceholderText(tr("在这里输入请求体内容..."));
    tab->bodyEdit->setMaximumHeight(200);
    layout->addWidget(tab->bodyEdit);

    tab->requestTabs->addTab(tab->bodyTab, tr("请求体"));
}

void HttpClient::setupTabResponseArea(HttpRequestTab* tab) {
    tab->responseWidget = new QWidget();
    tab->responseGroup = new QGroupBox(tr("HTTP 响应"));
    QVBoxLayout* responseLayout = new QVBoxLayout(tab->responseWidget);
    responseLayout->addWidget(tab->responseGroup);

    QVBoxLayout* groupLayout = new QVBoxLayout(tab->responseGroup);

    // 状态信息行，把按钮添加到右侧
    QHBoxLayout* statusLayout = new QHBoxLayout();
    tab->statusLabel = new QLabel(tr("状态: 就绪"));
    tab->statusLabel->setStyleSheet("font-weight: bold; color: #666;");
    statusLayout->addWidget(tab->statusLabel);

    // 时间标签
    tab->timeLabel = new QLabel(tr("时间: --"));
    tab->timeLabel->setStyleSheet("font-weight: bold; color: #666;");
    statusLayout->addWidget(tab->timeLabel);

    statusLayout->addStretch();

    // 响应操作按钮
    tab->copyResponseBtn = new QPushButton(tr("复制"));
    tab->copyResponseBtn->setMinimumWidth(60);
    statusLayout->addWidget(tab->copyResponseBtn);

    tab->formatResponseBtn = new QPushButton(tr("格式化"));
    tab->formatResponseBtn->setMinimumWidth(60);
    statusLayout->addWidget(tab->formatResponseBtn);

    tab->saveResponseBtn = new QPushButton(tr("保存"));
    tab->saveResponseBtn->setMinimumWidth(60);
    statusLayout->addWidget(tab->saveResponseBtn);

    groupLayout->addLayout(statusLayout);

    // 响应标签页
    tab->responseTabs = new QTabWidget();

    // 响应体标签页
    tab->responseBodyEdit = new QTextEdit();
    tab->responseBodyEdit->setReadOnly(true);
    tab->responseTabs->addTab(tab->responseBodyEdit, tr("响应体"));

    // 原始响应标签页
    tab->rawResponseEdit = new QTextEdit();
    tab->rawResponseEdit->setReadOnly(true);
    tab->rawResponseEdit->setFont(QFont("Consolas", 10));
    tab->responseTabs->addTab(tab->rawResponseEdit, tr("原始响应"));

    groupLayout->addWidget(tab->responseTabs);
}

void HttpClient::connectTabSignals(HttpRequestTab* tab) {
    // 请求按钮信号
    connect(tab->sendBtn, &QPushButton::clicked, [this, tab]() {
        // 设置当前标签页并发送请求
        setCurrentTab(tab);
        onSendRequestClicked();
    });

    connect(tab->cancelBtn, &QPushButton::clicked, [this, tab]() {
        setCurrentTab(tab);
        onCancelRequestClicked();
    });

    connect(tab->saveBtn, &QPushButton::clicked, [this, tab]() {
        setCurrentTab(tab);
        onSaveCurrentTabClicked();
    });

    // 参数管理信号
    connect(tab->addParamBtn, &QPushButton::clicked, [this, tab]() {
        addTableRow(tab->paramsTable, KeyValuePair("", "", true));
    });
    connect(tab->removeParamBtn, &QPushButton::clicked, [this, tab]() {
        removeSelectedTableRows(tab->paramsTable);
    });

    // 请求头管理信号
    connect(tab->addHeaderBtn, &QPushButton::clicked, [this, tab]() {
        addTableRow(tab->headersTable, KeyValuePair("", "", true));
    });
    connect(tab->removeHeaderBtn, &QPushButton::clicked, [this, tab]() {
        removeSelectedTableRows(tab->headersTable);
    });

    // Cookie管理信号
    connect(tab->addCookieBtn, &QPushButton::clicked, [this, tab]() {
        addTableRow(tab->cookiesTable, KeyValuePair("", "", true));
    });
    connect(tab->removeCookieBtn, &QPushButton::clicked, [this, tab]() {
        removeSelectedTableRows(tab->cookiesTable);
    });

    // 响应按钮信号
    connect(tab->copyResponseBtn, &QPushButton::clicked, [this, tab]() {
        setCurrentTab(tab);
        onCopyResponseClicked();
    });
    connect(tab->formatResponseBtn, &QPushButton::clicked, [this, tab]() {
        setCurrentTab(tab);
        onFormatResponseClicked();
    });
    connect(tab->saveResponseBtn, &QPushButton::clicked, [this, tab]() {
        setCurrentTab(tab);
        onSaveResponseClicked();
    });

    // URL组件信号
    connect(tab->protocolCombo, &QComboBox::currentTextChanged, [this, tab]() {
        setCurrentTab(tab);
        onProtocolChanged();
    });
    connect(tab->hostEdit, &QLineEdit::textChanged, [this, tab]() {
        setCurrentTab(tab);
        onHostChanged();
    });
    connect(tab->portSpin, QOverload<int>::of(&QSpinBox::valueChanged), [this, tab]() {
        setCurrentTab(tab);
        onPortChanged();
    });
    connect(tab->pathEdit, &QLineEdit::textChanged, [this, tab]() {
        setCurrentTab(tab);
        onPathChanged();
    });

    // 请求体格式化按钮
    connect(tab->formatBodyBtn, &QPushButton::clicked, [this, tab]() {
        setCurrentTab(tab);
        onFormatBodyClicked();
    });

    // 请求体类型变化
    connect(tab->bodyTypeCombo, &QComboBox::currentTextChanged, [tab]() {
        QString bodyType = tab->bodyTypeCombo->currentText();
        bool shouldShow = bodyType.contains("JSON", Qt::CaseInsensitive) || bodyType.contains("XML", Qt::CaseInsensitive);
        tab->formatBodyBtn->setVisible(shouldShow);
    });

    // 初始化计时器
    tab->requestTimer = new QTimer(tab->tabWidget);
    tab->requestTimer->setInterval(100); // 每100毫秒更新一次时间显示
    connect(tab->requestTimer, &QTimer::timeout, [this, tab]() {
        if (tab->elapsedTimer.isValid()) {
            qint64 elapsed = tab->elapsedTimer.elapsed();
            tab->timeLabel->setText(tr("时间: %1").arg(formatTime(elapsed)));
        }
    });
}

// 标签页管理辅助方法

void HttpClient::populateTabFromEntity(HttpRequestTab* tab, const HttpClientStore::HttpRequestEntity& request) {
    // 设置基本信息
    tab->methodCombo->setCurrentText(request.method);
    tab->protocolCombo->setCurrentText(request.protocol);
    tab->hostEdit->setText(request.host);
    tab->portSpin->setValue(request.port);
    tab->pathEdit->setText(request.path);

    // 清空并填充参数表格
    tab->paramsTable->setRowCount(0);
    QList<QVariantMap> params = HttpClientStore::HttpRequestEntity::parametersFromJson(request.parameters);
    for (const auto& param : params) {
        int row = tab->paramsTable->rowCount();
        tab->paramsTable->insertRow(row);
        tab->paramsTable->setItem(row, 0, new QTableWidgetItem(param["key"].toString()));
        tab->paramsTable->setItem(row, 1, new QTableWidgetItem(param["value"].toString()));

        QCheckBox* checkBox = new QCheckBox();
        checkBox->setChecked(param["enabled"].toBool());
        tab->paramsTable->setCellWidget(row, 2, checkBox);
    }

    // 清空并填充请求头表格
    tab->headersTable->setRowCount(0);
    QList<QVariantMap> headers = HttpClientStore::HttpRequestEntity::headersFromJson(request.headers);
    for (const auto& header : headers) {
        int row = tab->headersTable->rowCount();
        tab->headersTable->insertRow(row);
        tab->headersTable->setItem(row, 0, new QTableWidgetItem(header["key"].toString()));
        tab->headersTable->setItem(row, 1, new QTableWidgetItem(header["value"].toString()));

        QCheckBox* checkBox = new QCheckBox();
        checkBox->setChecked(header["enabled"].toBool());
        tab->headersTable->setCellWidget(row, 2, checkBox);
    }

    // 清空并填充Cookies表格
    tab->cookiesTable->setRowCount(0);
    QList<QVariantMap> cookies = HttpClientStore::HttpRequestEntity::cookiesFromJson(request.cookies);
    for (const auto& cookie : cookies) {
        int row = tab->cookiesTable->rowCount();
        tab->cookiesTable->insertRow(row);
        tab->cookiesTable->setItem(row, 0, new QTableWidgetItem(cookie["key"].toString()));
        tab->cookiesTable->setItem(row, 1, new QTableWidgetItem(cookie["value"].toString()));

        QCheckBox* checkBox = new QCheckBox();
        checkBox->setChecked(cookie["enabled"].toBool());
        tab->cookiesTable->setCellWidget(row, 2, checkBox);
    }

    // 设置请求体
    tab->bodyTypeCombo->setCurrentText(request.bodyType);
    tab->bodyEdit->setPlainText(request.body);

    // 更新标签页标题
    updateTabTitle(tab);
}

HttpClientStore::HttpRequestEntity HttpClient::createRequestFromTab(HttpRequestTab* tab, int groupId, const QString& name) {
    HttpClientStore::HttpRequestEntity request(groupId, name);

    // 设置基本信息
    request.method = tab->methodCombo->currentText();
    request.protocol = tab->protocolCombo->currentText();
    request.host = tab->hostEdit->text().trimmed();
    request.port = tab->portSpin->value();
    request.path = tab->pathEdit->text().trimmed();
    request.body = tab->bodyEdit->toPlainText();
    request.bodyType = tab->bodyTypeCombo->currentText();

    // 获取参数
    QList<QVariantMap> params;
    for (int row = 0; row < tab->paramsTable->rowCount(); ++row) {
        QVariantMap param;
        if (tab->paramsTable->item(row, 0) && tab->paramsTable->item(row, 1)) {
            param["key"] = tab->paramsTable->item(row, 0)->text();
            param["value"] = tab->paramsTable->item(row, 1)->text();

            QCheckBox* checkBox = qobject_cast<QCheckBox*>(tab->paramsTable->cellWidget(row, 2));
            if (checkBox) {
                param["enabled"] = checkBox->isChecked();
            } else {
                param["enabled"] = true;
            }
            params.append(param);
        }
    }
    request.parameters = HttpClientStore::HttpRequestEntity::parametersToJson(params);

    // 获取请求头
    QList<QVariantMap> headers;
    for (int row = 0; row < tab->headersTable->rowCount(); ++row) {
        QVariantMap header;
        if (tab->headersTable->item(row, 0) && tab->headersTable->item(row, 1)) {
            header["key"] = tab->headersTable->item(row, 0)->text();
            header["value"] = tab->headersTable->item(row, 1)->text();

            QCheckBox* checkBox = qobject_cast<QCheckBox*>(tab->headersTable->cellWidget(row, 2));
            if (checkBox) {
                header["enabled"] = checkBox->isChecked();
            } else {
                header["enabled"] = true;
            }
            headers.append(header);
        }
    }
    request.headers = HttpClientStore::HttpRequestEntity::headersToJson(headers);

    // 获取Cookies
    QList<QVariantMap> cookies;
    for (int row = 0; row < tab->cookiesTable->rowCount(); ++row) {
        QVariantMap cookie;
        if (tab->cookiesTable->item(row, 0) && tab->cookiesTable->item(row, 1)) {
            cookie["key"] = tab->cookiesTable->item(row, 0)->text();
            cookie["value"] = tab->cookiesTable->item(row, 1)->text();

            QCheckBox* checkBox = qobject_cast<QCheckBox*>(tab->cookiesTable->cellWidget(row, 2));
            if (checkBox) {
                cookie["enabled"] = checkBox->isChecked();
            } else {
                cookie["enabled"] = true;
            }
            cookies.append(cookie);
        }
    }
    request.cookies = HttpClientStore::HttpRequestEntity::cookiesToJson(cookies);

    return request;
}

HttpRequestTab* HttpClient::getCurrentTab() const {
    int currentIndex = m_mainTabWidget->currentIndex();
    if (currentIndex >= 0 && m_tabMap.contains(currentIndex)) {
        return m_tabMap[currentIndex];
    }
    return nullptr;
}

HttpRequestTab* HttpClient::getTabByRequestId(int requestId) {
    if (m_requestTabMap.contains(requestId)) {
        int tabIndex = m_requestTabMap[requestId];
        if (m_tabMap.contains(tabIndex)) {
            return m_tabMap[tabIndex];
        }
    }
    return nullptr;
}

void HttpClient::setCurrentTab(HttpRequestTab* tab) {
    // 找到标签页索引并切换
    for (auto it = m_tabMap.begin(); it != m_tabMap.end(); ++it) {
        if (it.value() == tab) {
            m_mainTabWidget->setCurrentIndex(it.key());
            break;
        }
    }
}

void HttpClient::updateTabTitle(HttpRequestTab* tab) {
    if (!tab)
        return;

    QString method = tab->methodCombo->currentText();
    QString title;

    if (tab->requestId > 0) {
        // 已保存的请求显示 [方法] 名称
        title = QString("[%1] %2").arg(method).arg(tab->tabName.split("] ").last());
    } else {
        // 新请求显示方法名
        title = tr("新请求 - %1").arg(method);
    }

    // 更新标签页标题
    for (auto it = m_tabMap.begin(); it != m_tabMap.end(); ++it) {
        if (it.value() == tab) {
            m_mainTabWidget->setTabText(it.key(), title);
            break;
        }
    }
}

void HttpClient::closeTab(int index) {
    if (index < 0 || !m_tabMap.contains(index))
        return;

    HttpRequestTab* tab = m_tabMap[index];

    // 移除映射关系
    if (tab->requestId > 0) {
        m_requestTabMap.remove(tab->requestId);
    }

    // 清理标签页资源
    if (tab->currentReply) {
        tab->currentReply->abort();
        tab->currentReply->deleteLater();
    }

    if (tab->requestTimer) {
        tab->requestTimer->stop();
        tab->requestTimer->deleteLater();
    }

    delete tab;

    // 从映射中移除
    m_tabMap.remove(index);

    // 移除标签页
    m_mainTabWidget->removeTab(index);

    // 重新调整映射索引（因为移除标签页会影响后续标签页的索引）
    QMap<int, HttpRequestTab*> newTabMap;
    QMap<int, int> newRequestTabMap;

    for (int i = 0; i < m_mainTabWidget->count(); ++i) {
        // 找到旧索引对应的标签页
        HttpRequestTab* oldTab = nullptr;
        for (auto it = m_tabMap.begin(); it != m_tabMap.end(); ++it) {
            if (it.key() > index && m_mainTabWidget->widget(i) == it.value()->tabWidget) {
                oldTab = it.value();
                break;
            } else if (it.key() < index && m_mainTabWidget->widget(i) == it.value()->tabWidget) {
                oldTab = it.value();
                break;
            }
        }

        if (oldTab) {
            newTabMap[i] = oldTab;
            if (oldTab->requestId > 0) {
                newRequestTabMap[oldTab->requestId] = i;
            }
        }
    }

    m_tabMap = newTabMap;
    m_requestTabMap = newRequestTabMap;

    // 如果没有标签页了，创建一个新的默认标签页
    if (m_mainTabWidget->count() == 0) {
        createNewTab(tr("新请求"));
    }
}

void HttpClient::updateTreeViewDisplay() {
    // 更新树视图显示，添加请求方法和图标
    for (int i = 0; i < m_requestTreeModel->rowCount(); ++i) {
        QStandardItem* groupItem = m_requestTreeModel->item(i);
        if (groupItem) {
            // 为分组添加文件夹图标
            groupItem->setIcon(QIcon(":/icons/folder.png")); // 需要添加图标资源

            for (int j = 0; j < groupItem->rowCount(); ++j) {
                QStandardItem* requestItem = groupItem->child(j);
                if (requestItem) {
                    int requestId = requestItem->data(Qt::UserRole).toInt();
                    HttpClientStore::HttpRequestEntity request = m_requestManager->getRequestById(requestId);
                    if (request.isValid()) {
                        // 更新显示文本包含请求方法
                        QString displayText = QString("[%1] %2").arg(request.method).arg(request.name);
                        requestItem->setText(displayText);

                        // 根据HTTP方法设置不同颜色
                        QColor methodColor;
                        if (request.method == "GET")
                            methodColor = QColor("#28a745");
                        else if (request.method == "POST")
                            methodColor = QColor("#007bff");
                        else if (request.method == "PUT")
                            methodColor = QColor("#ffc107");
                        else if (request.method == "DELETE")
                            methodColor = QColor("#dc3545");
                        else if (request.method == "PATCH")
                            methodColor = QColor("#17a2b8");
                        else
                            methodColor = QColor("#666666");

                        requestItem->setForeground(QBrush(methodColor));
                    }
                }
            }
        }
    }
}

// 标签页槽函数实现

void HttpClient::onTabCloseRequested(int index) {
    closeTab(index);
}

void HttpClient::onTabChanged(int index) {
    Q_UNUSED(index);

    // 获取当前标签页
    HttpRequestTab* currentTab = getCurrentTab();
    if (!currentTab || currentTab->requestId == 0) {
        // 如果是新标签页或无效标签页，清除左侧树的选择
        m_requestTreeView->blockSignals(true);
        m_requestTreeView->clearSelection();
        m_requestTreeView->blockSignals(false);
        return;
    }

    // 在左侧树中找到对应的请求节点并选中（阻塞信号避免循环调用）
    QStandardItem* requestItem = findRequestItem(currentTab->requestId);
    if (requestItem) {
        QModelIndex itemIndex = m_requestTreeModel->indexFromItem(requestItem);
        m_requestTreeView->blockSignals(true);
        m_requestTreeView->setCurrentIndex(itemIndex);
        m_requestTreeView->expand(itemIndex.parent()); // 展开父节点（分组节点）
        m_requestTreeView->blockSignals(false);
    }
}

void HttpClient::onNewTabRequested() {
    createNewTab(tr("新请求"));
}

void HttpClient::onSaveTabRequested() {
    HttpRequestTab* tab = getCurrentTab();
    if (!tab)
        return;

    // 如果是新标签页，提示输入名称
    if (tab->requestId == 0) {
        bool ok;
        QString name = QInputDialog::getText(this, tr("保存请求"), tr("请输入请求名称:"), QLineEdit::Normal, "", &ok);
        if (ok && !name.isEmpty()) {
            // 选择分组（简化处理，使用第一个分组）
            QList<HttpClientStore::HttpRequestGroup> groups = m_groupManager->getAllGroups();
            if (!groups.isEmpty()) {
                HttpClientStore::HttpRequestEntity request = createRequestFromTab(tab, groups.first().id, name);
                if (m_requestManager->saveRequest(request)) {
                    // 更新标签页信息
                    tab->requestId = request.id; // 注意：这里需要从数据库获取实际的ID
                    tab->groupId = request.groupId;
                    tab->tabName = name;

                    // 更新映射
                    for (auto it = m_tabMap.begin(); it != m_tabMap.end(); ++it) {
                        if (it.value() == tab) {
                            m_requestTabMap[tab->requestId] = it.key();
                            break;
                        }
                    }

                    updateTabTitle(tab);
                    refreshTreeView();
                } else {
                    QMessageBox::warning(this, tr("错误"), tr("请求保存失败"));
                }
            }
        }
    } else {
        // 更新已存在的请求
        QString name = tab->tabName.split("] ").last();
        HttpClientStore::HttpRequestEntity request = createRequestFromTab(tab, tab->groupId, name);
        request.id = tab->requestId;

        if (m_requestManager->updateRequest(tab->requestId, request)) {
            updateTabTitle(tab);
            refreshTreeView();
            QMessageBox::information(this, tr("成功"), tr("请求已更新"));
        } else {
            QMessageBox::warning(this, tr("错误"), tr("请求更新失败"));
        }
    }
}

void HttpClient::onSaveCurrentTabClicked() {
    HttpRequestTab* tab = getCurrentTab();
    if (!tab)
        return;

    if (tab->requestId == 0) {
        // 新请求，需要提示输入名称和选择分组
        QDialog dialog(this);
        dialog.setWindowTitle(tr("保存新请求"));
        dialog.setFixedSize(400, 300);

        QVBoxLayout* layout = new QVBoxLayout(&dialog);

        // 请求名称
        QLabel* nameLabel = new QLabel(tr("请求名称:"));
        QLineEdit* nameEdit = new QLineEdit();
        // 根据当前URL生成默认名称
        QString defaultName = QString("%1 %2").arg(tab->methodCombo->currentText()).arg(tab->hostEdit->text());
        if (!tab->pathEdit->text().isEmpty() && tab->pathEdit->text() != "/") {
            defaultName += tab->pathEdit->text();
        }
        nameEdit->setText(defaultName);
        layout->addWidget(nameLabel);
        layout->addWidget(nameEdit);

        // 分组选择
        QLabel* groupLabel = new QLabel(tr("选择分组:"));
        QComboBox* groupCombo = new QComboBox();

        QList<HttpClientStore::HttpRequestGroup> groups = m_groupManager->getAllGroups();
        if (groups.isEmpty()) {
            // 如果没有分组，创建默认分组
            HttpClientStore::HttpRequestGroup defaultGroup(tr("默认分组"));
            if (m_groupManager->saveGroup(defaultGroup)) {
                groups = m_groupManager->getAllGroups();
            }
        }

        for (const auto& group : groups) {
            groupCombo->addItem(group.name, group.id);
        }
        layout->addWidget(groupLabel);
        layout->addWidget(groupCombo);

        // 按钮
        QHBoxLayout* buttonLayout = new QHBoxLayout();
        QPushButton* okButton = new QPushButton(tr("保存"));
        QPushButton* cancelButton = new QPushButton(tr("取消"));

        okButton->setStyleSheet("QPushButton { background-color: #28a745; color: white; font-weight: bold; }");
        cancelButton->setStyleSheet("QPushButton { background-color: #6c757d; color: white; }");

        buttonLayout->addStretch();
        buttonLayout->addWidget(okButton);
        buttonLayout->addWidget(cancelButton);
        layout->addLayout(buttonLayout);

        connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);
        connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);

        if (dialog.exec() == QDialog::Accepted) {
            QString name = nameEdit->text().trimmed();
            if (name.isEmpty()) {
                QMessageBox::warning(this, tr("错误"), tr("请输入请求名称"));
                return;
            }

            int groupId = groupCombo->currentData().toInt();

            // 检查同名请求
            if (m_requestManager->requestExists(name, groupId)) {
                QMessageBox::warning(this, tr("错误"), tr("该分组下已存在同名请求"));
                return;
            }

            HttpClientStore::HttpRequestEntity request = createRequestFromTab(tab, groupId, name);
            if (m_requestManager->saveRequest(request)) {
                // 获取实际的ID (SQLite会自动分配)
                HttpClientStore::HttpRequestEntity savedRequest = m_requestManager->getRequestsByGroupId(groupId).last();

                // 更新标签页信息
                tab->requestId = savedRequest.id;
                tab->groupId = groupId;
                tab->tabName = name;

                // 更新映射
                for (auto it = m_tabMap.begin(); it != m_tabMap.end(); ++it) {
                    if (it.value() == tab) {
                        m_requestTabMap[tab->requestId] = it.key();
                        break;
                    }
                }

                updateTabTitle(tab);
                refreshTreeView();

                // 自动选择新创建的请求
                QTimer::singleShot(100, [this, name, groupId]() {
                    selectRequestInTree(name, groupId);
                });

                QMessageBox::information(this, tr("成功"), tr("请求已保存"));
            } else {
                QMessageBox::warning(this, tr("错误"), tr("请求保存失败"));
            }
        }
    } else {
        // 更新已存在的请求
        QString name = tab->tabName;

        // 如果标签名包含方法前缀，提取实际名称
        if (name.contains("] ")) {
            name = name.split("] ").last();
        }

        HttpClientStore::HttpRequestEntity request = createRequestFromTab(tab, tab->groupId, name);
        request.id = tab->requestId;

        if (m_requestManager->updateRequest(tab->requestId, request)) {
            // 更新标签页标题以反映可能的方法变化
            updateTabTitle(tab);
            refreshTreeView();
            updateTreeViewDisplay(); // 确保左侧树视图显示一致

            QMessageBox::information(this, tr("成功"), tr("请求已更新"));
        } else {
            QMessageBox::warning(this, tr("错误"), tr("请求更新失败"));
        }
    }
}

// ============ HttpRequestTreeModel 实现 ============

HttpRequestTreeModel::HttpRequestTreeModel(QObject* parent)
    : QStandardItemModel(parent) {
}

Qt::ItemFlags HttpRequestTreeModel::flags(const QModelIndex& index) const {
    Qt::ItemFlags defaultFlags = QStandardItemModel::flags(index);

    if (index.isValid()) {
        QStandardItem* item = itemFromIndex(index);
        if (item) {
            QString itemType = item->data(Qt::UserRole + 1).toString();
            if (itemType == "request") {
                // 请求节点可以被拖拽
                return defaultFlags | Qt::ItemIsDragEnabled;
            } else if (itemType == "group") {
                // 分组节点可以接受拖拽
                return defaultFlags | Qt::ItemIsDropEnabled;
            }
        }
    }
    return defaultFlags;
}

QStringList HttpRequestTreeModel::mimeTypes() const {
    return { "application/x-http-request-items" };
}

QMimeData* HttpRequestTreeModel::mimeData(const QModelIndexList& indexes) const {
    QMimeData* mimeData = new QMimeData();
    QByteArray encodedData;
    QDataStream stream(&encodedData, QIODevice::WriteOnly);

    // 收集所有被拖拽的请求ID
    QList<int> requestIds;
    for (const QModelIndex& index : indexes) {
        if (index.isValid()) {
            QStandardItem* item = itemFromIndex(index);
            if (item && item->data(Qt::UserRole + 1).toString() == "request") {
                int requestId = item->data(Qt::UserRole).toInt();
                requestIds.append(requestId);
            }
        }
    }

    stream << requestIds;
    mimeData->setData("application/x-http-request-items", encodedData);
    return mimeData;
}

bool HttpRequestTreeModel::canDropMimeData(const QMimeData* data, Qt::DropAction action,
                                           int row, int column, const QModelIndex& parent) const {
    if (!data->hasFormat("application/x-http-request-items")) {
        return false;
    }

    if (parent.isValid()) {
        QStandardItem* parentItem = itemFromIndex(parent);
        if (parentItem && parentItem->data(Qt::UserRole + 1).toString() == "group") {
            return true; // 可以拖拽到分组
        }
    }

    return false;
}

bool HttpRequestTreeModel::dropMimeData(const QMimeData* data, Qt::DropAction action,
                                        int row, int column, const QModelIndex& parent) {
    if (!canDropMimeData(data, action, row, column, parent)) {
        return false;
    }

    if (parent.isValid()) {
        QStandardItem* parentItem = itemFromIndex(parent);
        if (parentItem && parentItem->data(Qt::UserRole + 1).toString() == "group") {
            int newGroupId = parentItem->data(Qt::UserRole).toInt();

            // 解析拖拽的数据
            QByteArray encodedData = data->data("application/x-http-request-items");
            QDataStream stream(&encodedData, QIODevice::ReadOnly);
            QList<int> requestIds;
            stream >> requestIds;

            if (requestIds.size() == 1) {
                emit requestMoved(requestIds.first(), newGroupId);
            } else if (requestIds.size() > 1) {
                emit multipleRequestsMoved(requestIds, newGroupId);
            }

            return true;
        }
    }

    return false;
}

bool HttpRequestTreeModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || role != Qt::EditRole) {
        return QStandardItemModel::setData(index, value, role);
    }

    QStandardItem* item = itemFromIndex(index);
    if (!item) {
        return false;
    }

    QString itemType = item->data(Qt::UserRole + 1).toString();
    if (itemType != "request") {
        return QStandardItemModel::setData(index, value, role);
    }

    QString newName = value.toString().trimmed();
    QString oldName = item->data(Qt::UserRole + 2).toString(); // 获取纯请求名称

    if (newName.isEmpty() || newName == oldName) {
        return false;
    }

    int requestId = item->data(Qt::UserRole).toInt();
    emit requestRenamed(requestId, newName, oldName);

    // 暂时不更新显示，让信号处理函数来决定是否更新
    return false;
}

QVariant HttpRequestTreeModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) {
        return QStandardItemModel::data(index, role);
    }

    QStandardItem* item = itemFromIndex(index);
    if (!item) {
        return QStandardItemModel::data(index, role);
    }

    // 如果是请求节点且是编辑角色，返回纯请求名称
    if (role == Qt::EditRole) {
        QString itemType = item->data(Qt::UserRole + 1).toString();
        if (itemType == "request") {
            QString pureName = item->data(Qt::UserRole + 2).toString();
            if (!pureName.isEmpty()) {
                return pureName;
            }
        }
    }

    // 其他情况使用默认行为
    return QStandardItemModel::data(index, role);
}

// ============ HttpClient 拖拽处理槽函数 ============

void HttpClient::onRequestMoved(int requestId, int newGroupId) {
    // 获取请求详情
    HttpClientStore::HttpRequestEntity request = m_requestManager->getRequestById(requestId);
    if (request.id == 0)
        return;

    // 检查目标分组中是否已存在同名请求
    if (m_requestManager->requestExists(request.name, newGroupId)) {
        QMessageBox::warning(this, tr("错误"),
                             tr("目标分组中已存在同名请求：%1").arg(request.name));
        refreshTreeView(); // 刷新视图恢复原状
        return;
    }

    // 更新请求的分组ID
    request.groupId = newGroupId;
    if (m_requestManager->updateRequest(requestId, request)) {
        refreshTreeView(); // 刷新树视图
        QMessageBox::information(this, tr("成功"), tr("请求已移动到新分组"));
    } else {
        QMessageBox::warning(this, tr("错误"), tr("请求移动失败"));
        refreshTreeView(); // 刷新视图恢复原状
    }
}

void HttpClient::onMultipleRequestsMoved(const QList<int>& requestIds, int newGroupId) {
    // 检查所有请求名称是否会冲突
    QStringList conflictNames;
    QList<HttpClientStore::HttpRequestEntity> requests;

    for (int requestId : requestIds) {
        HttpClientStore::HttpRequestEntity request = m_requestManager->getRequestById(requestId);
        if (request.id != 0) {
            if (m_requestManager->requestExists(request.name, newGroupId)) {
                conflictNames.append(request.name);
            } else {
                requests.append(request);
            }
        }
    }

    if (!conflictNames.isEmpty()) {
        QString message = tr("以下请求在目标分组中已存在同名项，无法移动：\n%1")
            .arg(conflictNames.join(", "));
        QMessageBox::warning(this, tr("错误"), message);
        refreshTreeView(); // 刷新视图恢复原状
        return;
    }

    // 移动所有请求
    int successCount = 0;
    for (auto& request : requests) {
        request.groupId = newGroupId;
        if (m_requestManager->updateRequest(request.id, request)) {
            successCount++;
        }
    }

    refreshTreeView(); // 刷新树视图

    if (successCount == requests.size()) {
        QMessageBox::information(this, tr("成功"),
                                 tr("成功移动 %1 个请求到新分组").arg(successCount));
    } else {
        QMessageBox::warning(this, tr("部分成功"),
                             tr("成功移动 %1 个请求，失败 %2 个")
                             .arg(successCount).arg(requests.size() - successCount));
    }
}

void HttpClient::onImportTriggered() {
    // 获取当前选中的分组
    QModelIndex index = m_requestTreeView->currentIndex();
    if (!index.isValid()) {
        QMessageBox::information(this, tr("提示"), tr("请先选择一个分组"));
        return;
    }

    QStandardItem* item = m_requestTreeModel->itemFromIndex(index);
    if (!item)
        return;

    QString itemType = item->data(Qt::UserRole + 1).toString();
    int groupId = 0;
    QString groupName;

    if (itemType == "group") {
        groupId = item->data(Qt::UserRole).toInt();
        groupName = item->text();
    } else if (itemType == "request") {
        // 如果选中的是请求，获取其父分组
        QStandardItem* parentItem = item->parent();
        if (!parentItem)
            return;
        groupId = parentItem->data(Qt::UserRole).toInt();
        groupName = parentItem->text();
    }

    // 创建导入对话框
    showImportDialog(groupId, groupName);
}

void HttpClient::showImportDialog(int groupId, const QString& groupName) {
    // 创建导入对话框
    QDialog dialog(this);
    dialog.setWindowTitle(tr("导入请求到分组: %1").arg(groupName));
    dialog.setFixedSize(500, 450);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);

    // 请求名称
    QLabel* nameLabel = new QLabel(tr("请求名称:"));
    QLineEdit* nameEdit = new QLineEdit(tr("导入的请求"));
    layout->addWidget(nameLabel);
    layout->addWidget(nameEdit);

    // 导入类型
    QLabel* typeLabel = new QLabel(tr("导入类型:"));
    QComboBox* typeCombo = new QComboBox();
    typeCombo->addItem("cURL");
    typeCombo->addItem("wget");
    typeCombo->addItem("HTTPie");
    typeCombo->addItem("Postman (JSON)");
    layout->addWidget(typeLabel);
    layout->addWidget(typeCombo);

    // 输入内容
    QLabel* contentLabel = new QLabel(tr("输入内容:"));
    QTextEdit* contentEdit = new QTextEdit();
    contentEdit->setPlaceholderText(tr("在此粘贴 cURL 命令或其他格式的请求内容"));
    layout->addWidget(contentLabel);
    layout->addWidget(contentEdit);

    // 错误信息标签
    QLabel* errorLabel = new QLabel();
    errorLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    errorLabel->setWordWrap(true);
    errorLabel->setVisible(false);
    layout->addWidget(errorLabel);

    // 按钮
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* okButton = new QPushButton(tr("确定"));
    QPushButton* cancelButton = new QPushButton(tr("取消"));

    okButton->setStyleSheet("QPushButton { background-color: #007bff; color: white; font-weight: bold; }");
    cancelButton->setStyleSheet("QPushButton { background-color: #6c757d; color: white; font-weight: bold; }");

    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    layout->addLayout(buttonLayout);

    // 连接取消按钮
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);

    // 连接确定按钮（不直接关闭对话框）
    connect(okButton, &QPushButton::clicked, [&]() {
        // 隐藏之前的错误信息
        errorLabel->setVisible(false);

        QString requestName = nameEdit->text().trimmed();
        QString importType = typeCombo->currentText();
        QString content = contentEdit->toPlainText().trimmed();

        if (requestName.isEmpty()) {
            errorLabel->setText(tr("错误: 请输入请求名称"));
            errorLabel->setVisible(true);
            return;
        }

        if (content.isEmpty()) {
            errorLabel->setText(tr("错误: 请输入要导入的内容"));
            errorLabel->setVisible(true);
            return;
        }

        // 检查同名请求是否存在
        if (m_requestManager->requestExists(requestName, groupId)) {
            errorLabel->setText(tr("错误: 该分组下已存在同名请求"));
            errorLabel->setVisible(true);
            return;
        }

        // 尝试处理导入
        QString errorMessage;
        if (processImport(groupId, requestName, importType, content, errorMessage)) {
            // 导入成功，关闭对话框
            dialog.accept();
        } else {
            // 导入失败，显示错误信息，不关闭对话框
            errorLabel->setText(tr("导入失败: %1").arg(errorMessage));
            errorLabel->setVisible(true);
        }
    });

    // 显示对话框
    dialog.exec();
}

bool HttpClient::processImport(int groupId, const QString& requestName, const QString& importType, const QString& content, QString& errorMessage) {
    HttpClientStore::HttpRequestEntity request;

    try {
        if (importType == "curl" || importType == "cURL") {
            // 检查 curl 命令格式
            if (!content.trimmed().startsWith("curl")) {
                errorMessage = tr("无效的 cURL 命令格式");
                return false;
            }

            // 解析 curl 命令
            request = parseCurlCommand(content, groupId, requestName);

            // 验证解析结果
            if (request.host == "example.com") {
                errorMessage = tr("cURL 命令中未找到有效的 URL");
                return false;
            }
        } else if (importType == "wget") {
            // 暂时创建基本请求，后续可以实现 wget 解析
            request = HttpClientStore::HttpRequestEntity(groupId, requestName);
            request.method = "GET";
            request.protocol = "https";
            request.host = "example.com";
            request.port = 443;
            request.path = "/";
            errorMessage = tr("wget 格式暂时不支持，已创建默认请求");
        } else {
            // 默认创建基本的GET请求
            request = HttpClientStore::HttpRequestEntity(groupId, requestName);
            request.method = "GET";
            request.protocol = "https";
            request.host = "example.com";
            request.port = 443;
            request.path = "/";
            errorMessage = tr("未知的导入格式，已创建默认请求");
        }

        // 设置时间戳
        request.createdAt = QDateTime::currentDateTime();
        request.updatedAt = QDateTime::currentDateTime();

        // 保存原始命令
        if (importType == "curl" || importType == "cURL") {
            request.curlCommand = content;
        }

        // 尝试保存请求
        if (m_requestManager->saveRequest(request)) {
            refreshTreeView();

            // 自动选择新创建的请求
            QTimer::singleShot(100, [this, requestName, groupId]() {
                selectRequestInTree(requestName, groupId);
            });

            return true;
        } else {
            errorMessage = tr("数据库保存失败，请检查数据库连接");
            return false;
        }
    } catch (const std::exception& e) {
        errorMessage = tr("解析过程中发生异常: %1").arg(e.what());
        return false;
    } catch (...) {
        errorMessage = tr("解析过程中发生未知错误");
        return false;
    }
}

void HttpClient::onRequestRenamed(int requestId, const QString& newName, const QString& oldName) {
    // 获取请求详情以确定分组
    HttpClientStore::HttpRequestEntity request = m_requestManager->getRequestById(requestId);
    if (request.id == 0) {
        refreshTreeView(); // 刷新视图恢复原状
        return;
    }

    // 检查同分组下是否已存在同名请求
    if (m_requestManager->requestExists(newName, request.groupId)) {
        QMessageBox::warning(this, tr("错误"), tr("该分组下已存在同名请求：%1").arg(newName));
        refreshTreeView(); // 刷新视图恢复原状
        return;
    }

    // 更新数据库中的请求名称
    if (m_requestManager->renameRequest(requestId, newName)) {
        // 更新对应的tab标题
        HttpRequestTab* tab = getTabByRequestId(requestId);
        if (tab) {
            tab->tabName = newName;
            updateTabTitle(tab);
        }

        // 获取更新后的请求信息以获取方法
        HttpClientStore::HttpRequestEntity updatedRequest = m_requestManager->getRequestById(requestId);

        // 更新树视图节点的显示文本
        QStandardItem* requestItem = findRequestItem(requestId);
        if (requestItem) {
            QString method = updatedRequest.method.toUpper();
            if (method.isEmpty())
                method = "GET";
            QString displayText = QString("[%1] %2").arg(method).arg(newName);
            requestItem->setText(displayText);
            requestItem->setData(newName, Qt::UserRole + 2); // 更新纯请求名称
        } else {
            // 如果找不到节点，刷新整个树视图
            refreshTreeView();
        }
    } else {
        QMessageBox::warning(this, tr("错误"), tr("重命名失败"));
        refreshTreeView(); // 刷新视图恢复原状
    }
}

HttpClientStore::HttpRequestEntity HttpClient::parseCurlCommand(const QString& curlCommand, int groupId, const QString& requestName) {
    HttpClientStore::HttpRequestEntity request(groupId, requestName);

    // 设置默认值
    request.method = "GET";
    request.protocol = "https";
    request.host = "example.com";
    request.port = 443;
    request.path = "/";
    request.bodyType = "raw";

    if (curlCommand.trimmed().isEmpty()) {
        return request;
    }

    QString cmd = curlCommand.trimmed();

    // 移除开头的 curl 命令
    if (cmd.startsWith("curl ")) {
        cmd = cmd.mid(5);
    }

    // 用于存储解析的数据
    QList<QVariantMap> headers;
    QList<QVariantMap> cookies;
    QString body;
    QString contentType;

    // 解析参数
    QRegularExpression paramRegex;
    QRegularExpressionMatchIterator iterator;

    // 解析 URL (通常在最后或者在 -X 之前)
    QRegularExpression urlRegex(R"(['""]?(https?://[^\s'"]+)['""]?)");
    QRegularExpressionMatch urlMatch = urlRegex.match(cmd);
    if (urlMatch.hasMatch()) {
        QString url = urlMatch.captured(1);
        QUrl parsedUrl(url);

        if (parsedUrl.isValid()) {
            request.protocol = parsedUrl.scheme().toLower();
            request.host = parsedUrl.host();
            request.port = parsedUrl.port(parsedUrl.scheme() == "https" ? 443 : 80);
            request.path = parsedUrl.path();
            if (request.path.isEmpty()) request.path = "/";

            // 处理查询参数
            if (parsedUrl.hasQuery()) {
                QUrlQuery query(parsedUrl.query());
                QList<QVariantMap> params;
                for (const auto& item : query.queryItems()) {
                    QVariantMap param;
                    param["key"] = item.first;
                    param["value"] = item.second;
                    param["enabled"] = true;
                    params.append(param);
                }
                request.parameters = HttpClientStore::HttpRequestEntity::parametersToJson(params);
            }
        }
    }

    // 解析请求方法 -X 或 --request (支持大小写和引号)
    paramRegex.setPattern(R"(-X\s+['""]?([A-Za-z]+)['""]?|--request\s+['""]?([A-Za-z]+)['""]?)");
    iterator = paramRegex.globalMatch(cmd);
    while (iterator.hasNext()) {
        QRegularExpressionMatch match = iterator.next();
        QString method = match.captured(1).isEmpty() ? match.captured(2) : match.captured(1);
        if (!method.isEmpty()) {
            request.method = method.toUpper();
        }
    }

    // 解析请求头 -H 或 --header
    paramRegex.setPattern(R"(-H\s+['""]([^'""]+)['""]|--header\s+['""]([^'""]+)['""])");
    iterator = paramRegex.globalMatch(cmd);
    while (iterator.hasNext()) {
        QRegularExpressionMatch match = iterator.next();
        QString headerLine = match.captured(1).isEmpty() ? match.captured(2) : match.captured(1);

        if (headerLine.contains(":")) {
            QStringList parts = headerLine.split(":", Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                QString key = parts[0].trimmed();
                QString value = parts.mid(1).join(":").trimmed();

                // 特殊处理 Content-Type
                if (key.toLower() == "content-type") {
                    contentType = value;
                    if (value.contains("application/json")) {
                        request.bodyType = "JSON";
                    } else if (value.contains("application/xml") || value.contains("text/xml")) {
                        request.bodyType = "XML";
                    } else if (value.contains("application/x-www-form-urlencoded")) {
                        request.bodyType = "x-www-form-urlencoded";
                    }
                }

                QVariantMap header;
                header["key"] = key;
                header["value"] = value;
                header["enabled"] = true;
                headers.append(header);
            }
        }
    }

    // 解析 Cookie -b 或 --cookie
    paramRegex.setPattern(R"(-b\s+['""]([^'""]+)['""]|--cookie\s+['""]([^'""]+)['""])");
    iterator = paramRegex.globalMatch(cmd);
    while (iterator.hasNext()) {
        QRegularExpressionMatch match = iterator.next();
        QString cookieLine = match.captured(1).isEmpty() ? match.captured(2) : match.captured(1);

        QStringList cookieParts = cookieLine.split(";", Qt::SkipEmptyParts);
        for (const QString& cookiePart : cookieParts) {
            if (cookiePart.contains("=")) {
                QStringList parts = cookiePart.split("=", Qt::SkipEmptyParts);
                if (parts.size() >= 2) {
                    QVariantMap cookie;
                    cookie["key"] = parts[0].trimmed();
                    cookie["value"] = parts.mid(1).join("=").trimmed();
                    cookie["enabled"] = true;
                    cookies.append(cookie);
                }
            }
        }
    }

    // 解析请求体 -d 或 --data
    paramRegex.setPattern(R"(-d\s+['""]([^'""]*)['""]|--data\s+['""]([^'""]*)['""]|-d\s+([^\s]+)|--data\s+([^\s]+))");
    iterator = paramRegex.globalMatch(cmd);
    while (iterator.hasNext()) {
        QRegularExpressionMatch match = iterator.next();
        QString data;
        for (int i = 1; i <= 4; ++i) {
            if (!match.captured(i).isEmpty()) {
                data = match.captured(i);
                break;
            }
        }
        if (!data.isEmpty()) {
            if (body.isEmpty()) {
                body = data;
            } else {
                body += "&" + data;
            }
        }
    }

    // 设置解析结果
    request.headers = HttpClientStore::HttpRequestEntity::headersToJson(headers);
    request.cookies = HttpClientStore::HttpRequestEntity::cookiesToJson(cookies);
    request.body = body;

    return request;
}
