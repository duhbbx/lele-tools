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
#include <QTreeView>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QMimeData>
#include <QDataStream>

#include "../../common/dynamicobjectbase.h"
#include "httprequeststore.h"

// 前置声明
class HttpRequestTreeModel;

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

// 自定义树模型，支持拖拽操作
class HttpRequestTreeModel : public QStandardItemModel
{
    Q_OBJECT

public:
    explicit HttpRequestTreeModel(QObject* parent = nullptr);

    // 重写拖拽相关方法
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QStringList mimeTypes() const override;
    QMimeData* mimeData(const QModelIndexList& indexes) const override;
    bool canDropMimeData(const QMimeData* data, Qt::DropAction action,
                        int row, int column, const QModelIndex& parent) const override;
    bool dropMimeData(const QMimeData* data, Qt::DropAction action,
                     int row, int column, const QModelIndex& parent) override;

    // 重写编辑相关方法
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

signals:
    void requestMoved(int requestId, int newGroupId);
    void multipleRequestsMoved(const QList<int>& requestIds, int newGroupId);
    void requestRenamed(int requestId, const QString& newName, const QString& oldName);
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

// HTTP请求标签页数据结构
struct HttpRequestTab {
    int requestId;
    int groupId;
    QString tabName;
    QWidget* tabWidget;

    // 请求区域组件
    QGroupBox* requestGroup;
    QComboBox* methodCombo;
    QComboBox* protocolCombo;
    QLineEdit* hostEdit;
    QSpinBox* portSpin;
    QLineEdit* pathEdit;
    QPushButton* sendBtn;
    QPushButton* cancelBtn;
    QPushButton* saveBtn;

    // 请求详情标签页
    QTabWidget* requestTabs;
    QWidget* paramsTab;
    QWidget* headersTab;
    QWidget* cookiesTab;
    QWidget* bodyTab;

    // 参数表格
    QTableWidget* paramsTable;
    QPushButton* addParamBtn;
    QPushButton* removeParamBtn;

    // 头部表格
    QTableWidget* headersTable;
    QPushButton* addHeaderBtn;
    QPushButton* removeHeaderBtn;

    // Cookie表格
    QTableWidget* cookiesTable;
    QPushButton* addCookieBtn;
    QPushButton* removeCookieBtn;

    // 请求体
    QComboBox* bodyTypeCombo;
    QTextEdit* bodyEdit;
    QPushButton* formatBodyBtn;

    // 响应区域
    QWidget* responseWidget;
    QGroupBox* responseGroup;
    QTabWidget* responseTabs;
    QTextEdit* responseBodyEdit;
    QTextEdit* rawResponseEdit;
    QLabel* statusLabel;
    QLabel* timeLabel;
    QPushButton* copyResponseBtn;
    QPushButton* formatResponseBtn;
    QPushButton* saveResponseBtn;

    // 数据
    HttpRequestInfo requestInfo;
    HttpResponseInfo responseInfo;
    QNetworkReply* currentReply;
    QTimer* requestTimer;
    QElapsedTimer elapsedTimer;

    HttpRequestTab() : requestId(0), groupId(0), tabWidget(nullptr), currentReply(nullptr), requestTimer(nullptr) {}
};

class HttpClient : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit HttpClient();
    ~HttpClient();

private slots:
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

    // 请求存储管理
    void onRequestTreeClicked(const QModelIndex& index);
    void onRequestTreeDoubleClicked(const QModelIndex& index);
    void onRequestTreeContextMenu(const QPoint& position);
    void onAddGroupTriggered();
    void onAddRequestTriggered();
    void onRenameTriggered();
    void onDeleteTriggered();
    void onSaveCurrentTriggered();
    void onImportTriggered();

    // 标签页管理
    void onTabCloseRequested(int index);
    void onTabChanged(int index);
    void onNewTabRequested();
    void onSaveTabRequested();
    void onSaveCurrentTabClicked();

    // 拖拽处理
    void onRequestMoved(int requestId, int newGroupId);
    void onMultipleRequestsMoved(const QList<int>& requestIds, int newGroupId);

    // 重命名处理
    void onRequestRenamed(int requestId, const QString& newName, const QString& oldName);

private:
    void setupUI();
    static void setupToolbar();
    void setupRequestTreeView();
    void setupContextMenus();

    // 请求存储管理
    void initializeStorage();
    void loadRequestTree();
    void loadRequestsForGroup(QStandardItem* groupItem, int groupId);
    void populateRequestFromEntity(const HttpClientStore::HttpRequestEntity& request);
    HttpClientStore::HttpRequestEntity createRequestFromCurrentUI(int groupId, const QString& name);
    QStandardItem* findGroupItem(int groupId);
    QStandardItem* findRequestItem(int requestId);
    void refreshTreeView();
    void selectRequestInTree(const QString& requestName, int groupId);
    void showImportDialog(int groupId, const QString& groupName);
    bool processImport(int groupId, const QString& requestName, const QString& importType, const QString& content, QString& errorMessage);
    HttpClientStore::HttpRequestEntity parseCurlCommand(const QString& curlCommand, int groupId, const QString& requestName);

    // 标签页管理
    HttpRequestTab* createNewTab(const QString& tabName, int requestId = 0, int groupId = 0);
    HttpRequestTab* createTabFromRequest(const HttpClientStore::HttpRequestEntity& request);
    void closeTab(int index);
    void populateTabFromEntity(HttpRequestTab* tab, const HttpClientStore::HttpRequestEntity& request);
    HttpClientStore::HttpRequestEntity createRequestFromTab(HttpRequestTab* tab, int groupId, const QString& name);
    HttpRequestTab* getTabByRequestId(int requestId);
    HttpRequestTab* getCurrentTab() const;
    void setCurrentTab(HttpRequestTab* tab);
    void setupTabUI(HttpRequestTab* tab);
    void setupTabParametersTab(HttpRequestTab* tab);
    void setupTabHeadersTab(HttpRequestTab* tab);
    void setupTabCookiesTab(HttpRequestTab* tab);
    void setupTabBodyTab(HttpRequestTab* tab);
    void setupTabResponseArea(HttpRequestTab* tab);
    void connectTabSignals(HttpRequestTab* tab);
    void updateTabTitle(HttpRequestTab* tab);
    void updateTreeViewDisplay();


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
    static QString httpMethodToString(HttpMethod method);
    static HttpMethod stringToHttpMethod(const QString& method);
    static QString formatFileSize(qint64 bytes);
    static QString formatTime(qint64 milliseconds);

    // UI组件
    QVBoxLayout* m_mainLayout;
    QSplitter* m_mainSplitter;

    // 主标签页容器（右侧）
    QTabWidget* m_mainTabWidget;

    // 左侧请求树视图
    QTreeView* m_requestTreeView;
    HttpRequestTreeModel* m_requestTreeModel;
    QWidget* m_leftPanel;
    QSplitter* m_horizontalSplitter;

    // 存储管理器
    HttpClientStore::HttpRequestGroupManager* m_groupManager;
    HttpClientStore::HttpRequestManager* m_requestManager;

    // 上下文菜单
    QMenu* m_groupContextMenu;
    QMenu* m_requestContextMenu;
    QAction* m_addGroupAction;
    QAction* m_addRequestAction;
    QAction* m_renameAction;
    QAction* m_deleteAction;
    QAction* m_saveCurrentAction;
    QAction* m_importAction;

    // 标签页管理
    QMap<int, HttpRequestTab*> m_tabMap; // 标签页索引到标签页对象的映射
    QMap<int, int> m_requestTabMap; // 请求ID到标签页索引的映射
    int m_nextTabId;

    // 全局组件
    QNetworkAccessManager* m_networkManager;
};

#endif // HTTPCLIENT_H