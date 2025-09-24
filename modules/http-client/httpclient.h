#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QGroupBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QProgressBar>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>
#include <QMimeType>
#include <QMimeDatabase>
#include <QTimer>
#include <QElapsedTimer>

#include "../../common/dynamicobjectbase.h"

// HTTP请求方法枚举
enum class HttpMethod {
    GET,
    POST,
    PUT,
    HTTP_DELETE,
    PATCH,
    HEAD,
    OPTIONS
};

// 键值对结构，用于参数、头部、Cookie
struct KeyValuePair {
    QString key;
    QString value;
    bool enabled;

    KeyValuePair() : enabled(true) {}
    KeyValuePair(const QString& k, const QString& v, bool e = true)
        : key(k), value(v), enabled(e) {}
};

// HTTP请求信息结构
struct HttpRequestInfo {
    HttpMethod method;
    QString protocol;
    QString host;
    int port;
    QString path;
    QList<KeyValuePair> parameters;
    QList<KeyValuePair> headers;
    QList<KeyValuePair> cookies;
    QString body;
    QString bodyType; // raw, form-data, x-www-form-urlencoded, etc.

    HttpRequestInfo() : method(HttpMethod::GET), port(80) {}
};

// HTTP响应信息结构
struct HttpResponseInfo {
    int statusCode;
    QString statusText;
    QList<KeyValuePair> headers;
    QByteArray body;
    QString contentType;
    qint64 responseTime; // 响应时间(毫秒)
    qint64 contentLength;

    HttpResponseInfo() : statusCode(0), responseTime(0), contentLength(0) {}
};

class HttpClient : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit HttpClient();
    ~HttpClient();

private slots:
    // cURL解析
    void onCurlInputChanged();
    void onParseCurlClicked();
    void onClearAllClicked();

    // 请求发送
    void onSendRequestClicked();
    void onCancelRequestClicked();

    // 参数管理
    void onAddParameterClicked();
    void onRemoveParameterClicked();
    void onParameterChanged();

    // 头部管理
    void onAddHeaderClicked();
    void onRemoveHeaderClicked();
    void onHeaderChanged();

    // Cookie管理
    void onAddCookieClicked();
    void onRemoveCookieClicked();
    void onCookieChanged();

    // 响应处理
    void onRequestFinished();
    void onRequestError(QNetworkReply::NetworkError error);
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);

    // 实用功能
    void onCopyResponseClicked();
    void onFormatResponseClicked();
    void onSaveResponseClicked();
    void onFormatBodyClicked();

    // URL管理
    void onProtocolChanged();
    void onHostChanged();
    void onPortChanged();
    void onPathChanged();
    void updateFullUrl();

private:
    void setupUI();
    void setupRequestArea();
    void setupParametersTab();
    void setupHeadersTab();
    void setupCookiesTab();
    void setupBodyTab();
    void setupResponseArea();
    void setupToolbar();

    // cURL解析功能
    bool parseCurlCommand(const QString& curlCommand);
    QString extractUrl(const QString& curlCommand);
    HttpMethod extractMethod(const QString& curlCommand);
    QList<KeyValuePair> extractHeaders(const QString& curlCommand);
    QString extractBody(const QString& curlCommand);
    QList<KeyValuePair> extractCookies(const QString& curlCommand);

    // HTTP请求功能
    void buildRequest();
    QUrl buildUrl() const;
    void setRequestHeaders(QNetworkRequest& request);
    QByteArray buildRequestBody();
    void sendHttpRequest();

    // 响应处理
    void processResponse(QNetworkReply* reply);
    void displayResponse();
    QString formatJsonResponse(const QString& json);
    QString formatXmlResponse(const QString& xml);

    // 表格管理
    void populateParametersTable();
    void populateHeadersTable();
    void populateCookiesTable();
    void addTableRow(QTableWidget* table, const KeyValuePair& pair);
    void removeSelectedTableRows(QTableWidget* table);
    QList<KeyValuePair> getTableData(QTableWidget* table);

    // 工具函数
    QString httpMethodToString(HttpMethod method);
    HttpMethod stringToHttpMethod(const QString& method);
    QString formatFileSize(qint64 bytes);
    QString formatTime(qint64 milliseconds);

    // UI组件
    QVBoxLayout* m_mainLayout;
    QSplitter* m_mainSplitter;

    // cURL输入区域
    QGroupBox* m_curlGroup;
    QTextEdit* m_curlInput;
    QPushButton* m_parseCurlBtn;
    QPushButton* m_clearAllBtn;

    // 请求区域
    QWidget* m_requestWidget;
    QGroupBox* m_requestGroup;
    QComboBox* m_methodCombo;
    QComboBox* m_protocolCombo;
    QLineEdit* m_hostEdit;
    QSpinBox* m_portSpin;
    QLineEdit* m_pathEdit;
    QPushButton* m_sendBtn;
    QPushButton* m_cancelBtn;
    QProgressBar* m_progressBar;

    // 请求详情标签页
    QTabWidget* m_requestTabs;
    QWidget* m_paramsTab;
    QWidget* m_headersTab;
    QWidget* m_cookiesTab;
    QWidget* m_bodyTab;

    // 参数表格
    QTableWidget* m_paramsTable;
    QPushButton* m_addParamBtn;
    QPushButton* m_removeParamBtn;

    // 头部表格
    QTableWidget* m_headersTable;
    QPushButton* m_addHeaderBtn;
    QPushButton* m_removeHeaderBtn;

    // Cookie表格
    QTableWidget* m_cookiesTable;
    QPushButton* m_addCookieBtn;
    QPushButton* m_removeCookieBtn;

    // 请求体
    QComboBox* m_bodyTypeCombo;
    QTextEdit* m_bodyEdit;
    QPushButton* m_formatBodyBtn;

    // 响应区域
    QWidget* m_responseWidget;
    QGroupBox* m_responseGroup;
    QTabWidget* m_responseTabs;
    QTextEdit* m_responseBodyEdit;
    QTextEdit* m_rawResponseEdit;
    QLabel* m_statusLabel;
    QLabel* m_timeLabel;
    QLabel* m_sizeLabel;
    QPushButton* m_copyResponseBtn;
    QPushButton* m_formatResponseBtn;
    QPushButton* m_saveResponseBtn;

    // 数据成员
    HttpRequestInfo m_requestInfo;
    HttpResponseInfo m_responseInfo;
    QNetworkAccessManager* m_networkManager;
    QNetworkReply* m_currentReply;
    QTimer* m_requestTimer;
    QElapsedTimer m_elapsedTimer;
};

#endif // HTTPCLIENT_H