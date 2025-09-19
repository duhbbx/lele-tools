#ifndef IPLOOKUPTOOL_H
#define IPLOOKUPTOOL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QLabel>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QProgressBar>
#include <QGroupBox>
#include <QSplitter>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QRegularExpression>
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QClipboard>
#include <QApplication>
#include <QHeaderView>
#include <QHostInfo>
#include <QHostAddress>
#include <QAbstractSocket>

#include "../../common/dynamicobjectbase.h"

// IP查询结果结构
struct IpLookupResult {
    QString ip;
    QString country;
    QString region;
    QString city;
    QString isp;
    QString organization;
    QString timezone;
    QString latitude;
    QString longitude;
    QString asn;
    QString error;

    IpLookupResult() = default;

    bool isValid() const {
        return !ip.isEmpty() && error.isEmpty();
    }

    QString toString() const {
        if (!isValid()) return error;

        QStringList parts;
        if (!country.isEmpty()) parts << country;
        if (!region.isEmpty()) parts << region;
        if (!city.isEmpty()) parts << city;
        if (!isp.isEmpty()) parts << QString("(%1)").arg(isp);

        return parts.join(" ");
    }
};

// IP查询服务提供商枚举
enum class IpApiProvider {
    IpApi,          // ip-api.com
    IpInfo,         // ipinfo.io
    IpStack,        // ipstack.com
    IpGeolocation,  // ipgeolocation.io
    FreeGeoIp,      // freegeoip.app
    IpData          // ipdata.co
};

// 单个IP查询组件
class SingleIpLookup : public QWidget {
    Q_OBJECT

public:
    explicit SingleIpLookup(QWidget* parent = nullptr);

private slots:
    void onLookupClicked();
    void onClearClicked();
    void onCopyResultClicked();
    void onGetMyIpClicked();
    void onNetworkReplyFinished();

private:
    void setupUI();
    void lookupIp(const QString& ip);
    void displayResult(const IpLookupResult& result);
    void displayError(const QString& error);
    bool isValidIp(const QString& ip);
    QString resolveHostname(const QString& hostname);

    // UI组件
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_inputLayout;
    QLineEdit* m_ipInput;
    QPushButton* m_lookupBtn;
    QPushButton* m_clearBtn;
    QPushButton* m_getMyIpBtn;

    QGroupBox* m_resultGroup;
    QFormLayout* m_resultLayout;
    QLabel* m_ipLabel;
    QLabel* m_countryLabel;
    QLabel* m_regionLabel;
    QLabel* m_cityLabel;
    QLabel* m_ispLabel;
    QLabel* m_orgLabel;
    QLabel* m_timezoneLabel;
    QLabel* m_coordinatesLabel;
    QLabel* m_asnLabel;

    QHBoxLayout* m_buttonLayout;
    QPushButton* m_copyResultBtn;

    // 网络组件
    QNetworkAccessManager* m_networkManager;
    QNetworkReply* m_currentReply;

    // 当前查询的IP
    QString m_currentIp;
};

// 批量IP查询组件
class BatchIpLookup : public QWidget {
    Q_OBJECT

public:
    explicit BatchIpLookup(QWidget* parent = nullptr);

private slots:
    void onStartBatchClicked();
    void onStopBatchClicked();
    void onClearAllClicked();
    void onImportFromFileClicked();
    void onExportResultsClicked();
    void onCopyAllResultsClicked();
    void onNetworkReplyFinished();
    void processNextIp();

private:
    void setupUI();
    void startBatchLookup();
    void stopBatchLookup();
    void lookupNextIp();
    void displayResult(const QString& ip, const IpLookupResult& result);
    void updateProgress();
    QStringList parseIpList(const QString& text);
    bool isValidIp(const QString& ip);
    QString resolveHostname(const QString& hostname);

    // UI组件
    QVBoxLayout* m_mainLayout;
    QSplitter* m_mainSplitter;

    // 输入区域
    QWidget* m_inputWidget;
    QVBoxLayout* m_inputLayout;
    QLabel* m_inputLabel;
    QPlainTextEdit* m_ipListInput;
    QHBoxLayout* m_inputButtonLayout;
    QPushButton* m_importBtn;
    QPushButton* m_clearAllBtn;

    // 控制区域
    QHBoxLayout* m_controlLayout;
    QPushButton* m_startBtn;
    QPushButton* m_stopBtn;
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;

    // 结果区域
    QWidget* m_resultWidget;
    QVBoxLayout* m_resultLayout;
    QTableWidget* m_resultTable;
    QHBoxLayout* m_resultButtonLayout;
    QPushButton* m_exportBtn;
    QPushButton* m_copyAllBtn;

    // 批量处理状态
    QStringList m_ipList;
    int m_currentIndex;
    bool m_isRunning;
    QTimer* m_batchTimer;

    // 网络组件
    QNetworkAccessManager* m_networkManager;
    QNetworkReply* m_currentReply;

    // 结果存储
    QList<QPair<QString, IpLookupResult>> m_results;
};

// IP查询工具主类
class IpLookupTool final : public QWidget, public DynamicObjectBase {
    Q_OBJECT

public:
    explicit IpLookupTool(QWidget* parent = nullptr);
    ~IpLookupTool() override = default;

private:
    void setupUI();

    // UI组件
    QVBoxLayout* m_mainLayout;
    QTabWidget* m_tabWidget;
    SingleIpLookup* m_singleLookup;
    BatchIpLookup* m_batchLookup;
};

// IP查询API工具类
class IpApiClient : public QObject {
    Q_OBJECT

public:
    explicit IpApiClient(QObject* parent = nullptr);

    void lookupIp(const QString& ip, IpApiProvider provider = IpApiProvider::IpApi);
    void getMyPublicIp();

signals:
    void lookupFinished(const IpLookupResult& result);
    void myIpDetected(const QString& ip);
    void errorOccurred(const QString& error);

private slots:
    void onNetworkReplyFinished();

private:
    QString getApiUrl(const QString& ip, IpApiProvider provider);
    IpLookupResult parseResponse(const QByteArray& data, IpApiProvider provider);
    IpLookupResult parseIpApiResponse(const QJsonObject& json);
    IpLookupResult parseIpInfoResponse(const QJsonObject& json);

    QNetworkAccessManager* m_networkManager;
    QNetworkReply* m_currentReply;
    IpApiProvider m_currentProvider;
};

#endif // IPLOOKUPTOOL_H