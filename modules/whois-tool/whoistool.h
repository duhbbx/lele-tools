#ifndef WHOISTOOL_H
#define WHOISTOOL_H

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
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QRegularExpression>
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QClipboard>
#include <QApplication>
#include <QHeaderView>
#include <QScrollArea>
#include <QProcess>
#include <QTcpSocket>
#include <QHostInfo>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "../../common/dynamicobjectbase.h"

// WHOIS查询结果结构
struct WhoisResult {
    QString domain;
    QString registrar;
    QString registrant;
    QString adminContact;
    QString techContact;
    QString nameServers;
    QString creationDate;
    QString expirationDate;
    QString lastUpdated;
    QString status;
    QString dnssec;
    QString rawData;
    QString error;
    QDateTime queryTime;

    WhoisResult() {
        queryTime = QDateTime::currentDateTime();
    }

    bool isValid() const {
        return error.isEmpty() && !domain.isEmpty();
    }

    QString toSummary() const {
        if (!isValid()) return error;

        QStringList summary;
        if (!registrar.isEmpty()) summary << QString("注册商: %1").arg(registrar);
        if (!creationDate.isEmpty()) summary << QString("注册时间: %1").arg(creationDate);
        if (!expirationDate.isEmpty()) summary << QString("到期时间: %1").arg(expirationDate);
        if (!status.isEmpty()) summary << QString("状态: %1").arg(status);

        return summary.join(" | ");
    }
};

// WHOIS服务器映射
struct WhoisServer {
    QString tld;        // 顶级域名
    QString server;     // WHOIS服务器
    int port;          // 端口号
    QString prefix;    // 查询前缀

    WhoisServer(const QString& t = "", const QString& s = "", int p = 43, const QString& pr = "")
        : tld(t), server(s), port(p), prefix(pr) {}
};

// WHOIS客户端
class WhoisClient : public QObject {
    Q_OBJECT

public:
    explicit WhoisClient(QObject* parent = nullptr);

    void queryDomain(const QString& domain);
    void queryIP(const QString& ip);
    void setBatchMode(bool batch);

signals:
    void queryFinished(const WhoisResult& result);
    void queryProgress(const QString& message);
    void errorOccurred(const QString& error);

private slots:
    void onSocketConnected();
    void onSocketReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);
    void onSocketDisconnected();
    void onHttpReplyFinished();

private:
    QString findWhoisServer(const QString& domain);
    QString extractTLD(const QString& domain);
    void connectToWhoisServer(const QString& server, int port = 43);
    void sendQuery(const QString& query);
    WhoisResult parseWhoisData(const QString& data, const QString& domain);
    void initWhoisServers();
    QString cleanDomain(const QString& domain);
    bool isValidDomain(const QString& domain);
    bool isValidIP(const QString& ip);
    void queryWithHttp(const QString& domain);
    QString extractValue(const QString& line);

    QTcpSocket* m_socket;
    QNetworkAccessManager* m_networkManager;
    QNetworkReply* m_currentReply;
    QString m_currentDomain;
    QString m_responseData;
    QList<WhoisServer> m_whoisServers;
    bool m_batchMode;
    QTimer* m_timeoutTimer;
};

// 单个WHOIS查询组件
class SingleWhoisQuery : public QWidget {
    Q_OBJECT

public:
    explicit SingleWhoisQuery(QWidget* parent = nullptr);

private slots:
    void onQueryClicked();
    void onClearClicked();
    void onCopyResultClicked();
    void onSaveResultClicked();

public slots:
    void onQueryFinished(const WhoisResult& result);
    void onQueryProgress(const QString& message);
    void onQueryError(const QString& error);

private:
    void setupUI();
    void displayResult(const WhoisResult& result);
    void updateUI();

    // UI组件
    QVBoxLayout* m_mainLayout;
    QSplitter* m_mainSplitter;

    // 查询区域
    QWidget* m_queryWidget;
    QVBoxLayout* m_queryLayout;
    QGroupBox* m_inputGroup;
    QFormLayout* m_inputLayout;
    QLineEdit* m_domainInput;
    QHBoxLayout* m_buttonLayout;
    QPushButton* m_queryBtn;
    QPushButton* m_clearBtn;

    // 结果概览区域
    QGroupBox* m_summaryGroup;
    QFormLayout* m_summaryLayout;
    QLabel* m_domainLabel;
    QLabel* m_registrarLabel;
    QLabel* m_creationLabel;
    QLabel* m_expirationLabel;
    QLabel* m_statusLabel;

    // 详细结果区域
    QWidget* m_resultWidget;
    QVBoxLayout* m_resultLayout;
    QTabWidget* m_resultTabs;
    QTableWidget* m_detailsTable;
    QTextEdit* m_rawDataText;

    QHBoxLayout* m_resultButtonLayout;
    QPushButton* m_copyBtn;
    QPushButton* m_saveBtn;

    // 状态区域
    QGroupBox* m_statusGroup;
    QVBoxLayout* m_statusLayout;
    QProgressBar* m_progressBar;
    QLabel* m_queryStatusLabel;

    WhoisClient* m_whoisClient;
    WhoisResult m_lastResult;
};

// 批量WHOIS查询组件
class BatchWhoisQuery : public QWidget {
    Q_OBJECT

public:
    explicit BatchWhoisQuery(QWidget* parent = nullptr);

private slots:
    void onStartBatchClicked();
    void onStopBatchClicked();
    void onClearAllClicked();
    void onImportFromFileClicked();
    void onExportResultsClicked();
    void onCopyAllResultsClicked();
    void onQueryFinished(const WhoisResult& result);
    void onQueryProgress(const QString& message);
    void onQueryError(const QString& error);
    void processNextDomain();

private:
    void setupUI();
    void startBatchQuery();
    void stopBatchQuery();
    void updateProgress();
    void addResultToTable(const WhoisResult& result);
    QStringList parseDomainList(const QString& text);

    // UI组件
    QVBoxLayout* m_mainLayout;
    QSplitter* m_mainSplitter;

    // 输入区域
    QWidget* m_inputWidget;
    QVBoxLayout* m_inputLayout;
    QGroupBox* m_inputGroup;
    QVBoxLayout* m_inputGroupLayout;
    QLabel* m_inputLabel;
    QPlainTextEdit* m_domainListInput;
    QHBoxLayout* m_inputButtonLayout;
    QPushButton* m_importBtn;
    QPushButton* m_clearAllBtn;

    // 控制区域
    QGroupBox* m_controlGroup;
    QHBoxLayout* m_controlLayout;
    QPushButton* m_startBtn;
    QPushButton* m_stopBtn;
    QSpinBox* m_delaySpinBox;
    QLabel* m_delayLabel;

    // 进度区域
    QGroupBox* m_progressGroup;
    QVBoxLayout* m_progressLayout;
    QProgressBar* m_progressBar;
    QLabel* m_batchStatusLabel;
    QLabel* m_currentDomainLabel;

    // 结果区域
    QWidget* m_resultWidget;
    QVBoxLayout* m_resultLayout;
    QLabel* m_resultLabel;
    QTableWidget* m_resultTable;
    QHBoxLayout* m_resultButtonLayout;
    QPushButton* m_exportBtn;
    QPushButton* m_copyAllBtn;

    // 批量处理状态
    QStringList m_domainList;
    int m_currentIndex;
    bool m_isRunning;
    QTimer* m_batchTimer;
    WhoisClient* m_whoisClient;
    QList<WhoisResult> m_results;
};

// WHOIS历史记录组件
class WhoisHistory : public QWidget {
    Q_OBJECT

public:
    explicit WhoisHistory(QWidget* parent = nullptr);

    void addRecord(const WhoisResult& result);

private slots:
    void onClearHistoryClicked();
    void onExportHistoryClicked();
    void onViewDetailsClicked();
    void onDeleteSelectedClicked();

private:
    void setupUI();
    void updateHistoryTable();

    // UI组件
    QVBoxLayout* m_mainLayout;
    QGroupBox* m_historyGroup;
    QVBoxLayout* m_historyLayout;
    QTableWidget* m_historyTable;
    QHBoxLayout* m_buttonLayout;
    QPushButton* m_viewDetailsBtn;
    QPushButton* m_deleteSelectedBtn;
    QPushButton* m_clearHistoryBtn;
    QPushButton* m_exportHistoryBtn;

    QList<WhoisResult> m_history;
};

// WHOIS工具主类
class WhoisTool final : public QWidget, public DynamicObjectBase {
    Q_OBJECT

public:
    explicit WhoisTool(QWidget* parent = nullptr);
    ~WhoisTool() override = default;

private slots:
    void onWhoisQueryFinished(const WhoisResult& result);

private:
    void setupUI();

    // UI组件
    QVBoxLayout* m_mainLayout;
    QTabWidget* m_tabWidget;
    SingleWhoisQuery* m_singleQuery;
    BatchWhoisQuery* m_batchQuery;
    WhoisHistory* m_history;
};

#endif // WHOISTOOL_H