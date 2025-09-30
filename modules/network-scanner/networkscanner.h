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
#include <QAtomicInt>
#include <QHostInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStringConverter>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include "../../common/dynamicobjectbase.h"

// 移除复杂的系统包含以避免编译问题

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

// 扫描方法枚举
enum ScanMethod {
    SCAN_ARP,          // ARP二层发现（最快、最可靠，同网段）
    SCAN_PING,         // 使用ping命令
    SCAN_TCP,          // TCP套接字扫描
    SCAN_UDP,          // UDP套接字扫描
    SCAN_ICMP,         // ICMP套接字扫描
    SCAN_FAST_TCP,     // 快速TCP端口扫描
    SCAN_NATIVE_ICMP,  // 原生ICMP扫描（无进程）
    SCAN_MULTI_TCP     // 并行多端口TCP扫描
};

// 单个主机扫描任务
class HostScanTask : public QObject {
    Q_OBJECT

public:
    explicit HostScanTask(const QString& ip, int timeout, QAtomicInt* stopFlag,
                         ScanMethod method = SCAN_TCP, QObject* parent = nullptr);

public slots:
    void scan();
    static void stop();

signals:
    void finished(const HostInfo& hostInfo);

private:
    // ARP二层发现（最快、最可靠）
    bool scanWithArp();

    // 原有ping扫描方法
    bool scanWithPing();

    // 新增高效扫描方法
    bool scanWithTcp();
    bool scanWithUdp();
    bool scanWithIcmp();
    bool scanWithFastTcp();

    // 原生ICMP实现
    bool scanWithNativeIcmp();
    bool scanWithMultiPortTcp();

    QString getHostName(const QString& ip) const;
    QString getMacAddress(const QString& ip) const;
    static QString getMacVendor(const QString& mac);
    static bool isValidIP(const QString& ip);

    // 并行查询DNS和ARP
    void performParallelQueries(HostInfo& info) const;

    QString m_ip;
    int m_timeout;
    QAtomicInt* m_stopFlag;
    ScanMethod m_scanMethod;
    QElapsedTimer m_timer;
};

// 扫描工作线程
class ScanWorker : public QObject {
    Q_OBJECT

public:
    explicit ScanWorker(QObject* parent = nullptr);
    void setScanParams(const QString& startIP, const QString& endIP, int timeout,
                      int threadCount, ScanMethod method = SCAN_TCP);
    void stop();

public slots:
    void startScan();

signals:
    void hostFound(const HostInfo& hostInfo);
    void scanProgress(int current, int total);
    void scanFinished();

private slots:
    void onTaskFinished(const HostInfo& hostInfo);

private:
    void scanNextBatch();
    QStringList generateIPRange(const QString& startIP, const QString& endIP);
    static bool isValidIP(const QString& ip);

    QString m_startIP;
    QString m_endIP;
    int m_timeout;
    int m_threadCount;
    ScanMethod m_scanMethod;
    QAtomicInt m_stopRequested;
    QStringList m_ipList;
    QAtomicInt m_currentIndex;
    QAtomicInt m_activeTasks;
    QMutex m_mutex;
    QList<QThread*> m_threads;
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
    QComboBox* m_scanMethodCombo;
    
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
