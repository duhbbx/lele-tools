#ifndef TRACEROUTETOOL_H
#define TRACEROUTETOOL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QProgressBar>
#include <QTextEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QHostInfo>
#include <QNetworkInterface>
#include <QSettings>

#ifdef Q_OS_WIN
// Windows平台网络API
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

#ifdef WITH_PCAP
// 如果找到Npcap SDK，使用pcap API
#include <pcap.h>
#pragma comment(lib, "wpcap.lib")
#pragma comment(lib, "packet.lib")
#endif

#ifdef WITH_WINDOWS_ICMP
// 备用方案：使用Windows ICMP API
#include <icmpapi.h>
#pragma comment(lib, "icmp.lib")
#endif

#else
// Linux/macOS平台
#ifdef WITH_PCAP
#include <pcap/pcap.h>
#endif
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "../../common/dynamicobjectbase.h"

// 路由跳跃信息结构
struct RouteHop {
    int hopNumber;          // 跳数
    QString ipAddress;      // IP地址
    QString hostname;       // 主机名（如果可解析）
    double rtt1;           // 第一次往返时间(ms)
    double rtt2;           // 第二次往返时间(ms) 
    double rtt3;           // 第三次往返时间(ms)
    bool timeout;          // 是否超时
    QString status;        // 状态信息
    
    RouteHop() : hopNumber(0), rtt1(-1), rtt2(-1), rtt3(-1), timeout(false) {}
};

// Traceroute工作线程
class TracerouteWorker : public QObject
{
    Q_OBJECT

public:
    explicit TracerouteWorker(QObject *parent = nullptr);
    ~TracerouteWorker();

public slots:
    void startTraceroute(const QString &target, int maxHops, int timeout);
    void stopTraceroute();

signals:
    void hopDiscovered(const RouteHop &hop);
    void tracerouteCompleted();
    void errorOccurred(const QString &error);
    void progressUpdated(int currentHop, int maxHops);

private:
    void performTraceroute(const QString &target, int maxHops, int timeoutMs);
    bool sendICMPPacket(const QString &target, int ttl, int timeoutMs, RouteHop &hop);
    bool initializeNetwork();
    void cleanup();
    
#ifdef Q_OS_WIN
    // Windows特定实现
    SOCKET m_socket;
    
#ifdef WITH_PCAP
    pcap_t *m_pcapHandle;
    bool initializeNpcap();
#endif

#ifdef WITH_WINDOWS_ICMP
    HANDLE m_icmpHandle;
    bool initializeIcmp();
#endif
    
    bool initializeWinsock();
    void cleanupWindows();
#else
    // Linux/macOS特定实现
    int m_socket;
#ifdef WITH_PCAP
    pcap_t *m_pcapHandle;
#endif
    void cleanupUnix();
#endif
    
    bool m_running;
    QMutex m_mutex;
};

// 主Traceroute工具类
class TracerouteTool : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit TracerouteTool(QWidget *parent = nullptr);
    ~TracerouteTool();

private slots:
    void onStartTraceroute();
    void onStopTraceroute();
    void onClearResults();
    void onExportResults();
    void onTargetChanged();
    void onHopDiscovered(const RouteHop &hop);
    void onTracerouteCompleted();
    void onErrorOccurred(const QString &error);
    void onProgressUpdated(int currentHop, int maxHops);

private:
    void setupUI();
    void setupControlArea();
    void setupResultsArea();
    void setupStatusArea();
    void updateUI();
    void addHopToTable(const RouteHop &hop);
    void logMessage(const QString &message);
    void saveSettings();
    void loadSettings();
    QString formatRTT(double rtt) const;
    
    // UI组件
    QVBoxLayout *m_mainLayout;
    
    // 控制区域
    QGroupBox *m_controlGroup;
    QLineEdit *m_targetEdit;
    QSpinBox *m_maxHopsSpin;
    QSpinBox *m_timeoutSpin;
    QCheckBox *m_resolveNamesCheck;
    QPushButton *m_startBtn;
    QPushButton *m_stopBtn;
    QPushButton *m_clearBtn;
    QPushButton *m_exportBtn;
    
    // 结果区域
    QGroupBox *m_resultsGroup;
    QTableWidget *m_resultsTable;
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;
    
    // 日志区域
    QGroupBox *m_logGroup;
    QTextEdit *m_logText;
    
    // 工作线程
    QThread *m_workerThread;
    TracerouteWorker *m_worker;
    
    // 状态
    bool m_running;
    int m_currentHop;
    int m_maxHops;
    QString m_currentTarget;
};

#endif // TRACEROUTETOOL_H
