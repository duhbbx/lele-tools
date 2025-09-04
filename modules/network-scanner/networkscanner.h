#ifndef NETWORKSCANNER_H
#define NETWORKSCANNER_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QProgressBar>
#include <QGroupBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QTextEdit>
#include <QThread>
#include <QMutex>
#include <QTimer>
#include <QNetworkInterface>
#include <QHostInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStringConverter>
#include "../../common/dynamicobjectbase.h"

// 主机信息结构
struct HostInfo {
    QString ipAddress;
    QString hostName;
    QString macAddress;
    QString vendor;
    int responseTime;
    bool isOnline;
    
    HostInfo() : responseTime(-1), isOnline(false) {}
};

// 扫描工作线程
class ScanWorker : public QObject {
    Q_OBJECT

public:
    explicit ScanWorker(QObject* parent = nullptr);
    void setScanParams(const QString& startIP, const QString& endIP, int timeout, int threadCount);
    void stop();

public slots:
    void startScan();

signals:
    void hostFound(const HostInfo& hostInfo);
    void scanProgress(int current, int total);
    void scanFinished();

private slots:
    void onPingFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void scanRange();
    void pingHost(const QString& ip);
    QString getHostName(const QString& ip);
    QString getMacAddress(const QString& ip);
    QString getMacVendor(const QString& mac);
    QStringList generateIPRange(const QString& startIP, const QString& endIP);
    bool isValidIP(const QString& ip);

    QString m_startIP;
    QString m_endIP;
    int m_timeout;
    int m_threadCount;
    bool m_stopRequested;
    QStringList m_ipList;
    int m_currentIndex;
    int m_activeProcesses;
    QMutex m_mutex;
    QMap<QProcess*, QString> m_processMap;
};

class NetworkScanner : public QWidget, public DynamicObjectBase {
    Q_OBJECT

public:
    explicit NetworkScanner(QWidget* parent = nullptr);
    ~NetworkScanner();

private slots:
    void onStartScan();
    void onStopScan();
    void onClearResults();
    void onExportResults();
    void onRefreshInterfaces();
    void onHostFound(const HostInfo& hostInfo);
    void onScanProgress(int current, int total);
    void onScanFinished();
    void onInterfaceChanged();
    void onCustomRangeToggled(bool enabled);

private:
    void setupUI();
    void setupScanSettings();
    void setupResultsTable();
    void setupStatusArea();
    void loadNetworkInterfaces();
    void updateIPRange();
    bool validateIPRange();
    void resetScan();
    void updateScanButton(bool isScanning);
    
    // UI组件
    QVBoxLayout* m_mainLayout;
    
    // 扫描设置区域
    QGroupBox* m_settingsGroup;
    QComboBox* m_interfaceCombo;
    QPushButton* m_refreshBtn;
    QCheckBox* m_customRangeCheck;
    QLineEdit* m_startIPEdit;
    QLineEdit* m_endIPEdit;
    QLabel* m_ipRangeLabel;
    QSpinBox* m_timeoutSpin;
    QSpinBox* m_threadSpin;
    
    // 控制按钮
    QHBoxLayout* m_buttonLayout;
    QPushButton* m_scanBtn;
    QPushButton* m_stopBtn;
    QPushButton* m_clearBtn;
    QPushButton* m_exportBtn;
    
    // 结果显示区域
    QGroupBox* m_resultsGroup;
    QTableWidget* m_resultsTable;
    
    // 状态区域
    QGroupBox* m_statusGroup;
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
    QLabel* m_foundLabel;
    QTextEdit* m_logText;
    
    // 扫描相关
    QThread* m_scanThread;
    ScanWorker* m_scanWorker;
    bool m_isScanning;
    int m_foundHosts;
    QTimer* m_statusTimer;
    
    // 设置
    QMap<QString, QString> m_interfaceMap; // 接口名 -> IP地址
};

#endif // NETWORKSCANNER_H
