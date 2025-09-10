#ifndef SYSTEMINFO_H
#define SYSTEMINFO_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QProgressBar>
#include <QTextEdit>
#include <QPushButton>
#include <QTimer>
#include <QScrollArea>
#include <QFrame>
#include <QSplitter>
#include <QTabWidget>
#include <QHeaderView>
#include <QThread>
#include <QApplication>
#include <QScreen>
#include <QNetworkInterface>
#include <QHostInfo>
#include <QStorageInfo>
#include <QStandardPaths>
#include <QOperatingSystemVersion>
#include <QSysInfo>
#include <QDateTime>
#include <QProcess>
#include <QSettings>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <sysinfoapi.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "kernel32.lib")
#else
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <fstream>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include "../../common/dynamicobjectbase.h"

// 系统信息数据结构
struct SystemInfo {
    // 操作系统信息
    QString osName;
    QString osVersion;
    QString architecture;
    QString computerName;
    QString userName;
    
    // CPU信息
    QString cpuName;
    int physicalCores;
    int logicalCores;
    double cpuFrequency;  // MHz
    QString cpuArchitecture;
    
    // 内存信息
    quint64 totalMemory;     // 字节
    quint64 availableMemory; // 字节
    quint64 usedMemory;      // 字节
    double memoryUsagePercent;
    
    // 交换内存信息
    quint64 totalSwap;
    quint64 usedSwap;
    quint64 availableSwap;
    
    // 网络信息
    QString primaryIP;
    QString defaultGateway;
    QStringList dnsServers;
    QStringList networkInterfaces;
    
    // 显示信息
    QStringList monitors;
    QString resolution;
    
    SystemInfo() : physicalCores(0), logicalCores(0), cpuFrequency(0),
                   totalMemory(0), availableMemory(0), usedMemory(0), memoryUsagePercent(0),
                   totalSwap(0), usedSwap(0), availableSwap(0) {}
};

// 磁盘信息结构
struct DiskInfo {
    QString device;        // 设备名称
    QString mountPoint;    // 挂载点
    QString fileSystem;    // 文件系统类型
    quint64 totalSpace;    // 总容量
    quint64 usedSpace;     // 已用容量
    quint64 availableSpace; // 可用容量
    double usagePercent;   // 使用百分比
    bool isReady;          // 是否就绪
    
    DiskInfo() : totalSpace(0), usedSpace(0), availableSpace(0), 
                 usagePercent(0), isReady(false) {}
};

// 系统信息收集工作线程
class SystemInfoWorker : public QObject
{
    Q_OBJECT

public:
    explicit SystemInfoWorker(QObject *parent = nullptr);

public slots:
    void collectSystemInfo();
    void collectDiskInfo();

signals:
    void systemInfoReady(const SystemInfo &info);
    void diskInfoReady(const QList<DiskInfo> &disks);
    void errorOccurred(const QString &error);

private:
    SystemInfo getSystemInfo();
    QList<DiskInfo> getDiskInfo();
    
#ifdef Q_OS_WIN
    SystemInfo getWindowsSystemInfo();
    QList<DiskInfo> getWindowsDiskInfo();
    QString getWindowsCPUName();
    QString getDefaultGateway();
    QStringList getDNSServers();
#else
    SystemInfo getLinuxSystemInfo();
    QList<DiskInfo> getLinuxDiskInfo();
#endif
};

// 系统信息显示工具
class SystemInfoTool : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit SystemInfoTool(QWidget *parent = nullptr);
    ~SystemInfoTool();

private slots:
    void onRefreshClicked();
    void onExportClicked();
    void onSystemInfoReady(const ::SystemInfo &info);
    void onDiskInfoReady(const QList<DiskInfo> &disks);
    void onErrorOccurred(const QString &error);

private:
    void setupUI();
    void setupSystemInfoArea();
    void setupNetworkInfoArea();
    void setupHardwareInfoArea();
    void setupDiskInfoArea();
    void setupControlArea();
    
    void updateSystemInfoDisplay(const ::SystemInfo &info);
    void updateDiskInfoDisplay(const QList<DiskInfo> &disks);
    void refreshSystemInfo();
    QString formatBytes(quint64 bytes) const;
    QString formatPercentage(double percent) const;
    void saveSettings();
    void loadSettings();
    
    // UI组件
    QVBoxLayout *m_mainLayout;
    QTabWidget *m_tabWidget;
    
    // 系统信息标签页
    QWidget *m_systemTab;
    QScrollArea *m_systemScrollArea;
    QWidget *m_systemContent;
    QVBoxLayout *m_systemLayout;
    
    // 网络信息区域
    QGroupBox *m_networkGroup;
    QGridLayout *m_networkLayout;
    QLineEdit *m_primaryIPEdit;
    QLineEdit *m_gatewayEdit;
    QTextEdit *m_dnsEdit;
    QLineEdit *m_hostnameEdit;
    
    // 硬件信息区域
    QGroupBox *m_hardwareGroup;
    QGridLayout *m_hardwareLayout;
    QLineEdit *m_osEdit;
    QTextEdit *m_cpuEdit;
    QLineEdit *m_coresEdit;
    QLineEdit *m_frequencyEdit;
    QLineEdit *m_memoryEdit;
    QLineEdit *m_swapEdit;
    QProgressBar *m_memoryProgress;
    QProgressBar *m_swapProgress;
    
    // 磁盘信息标签页
    QWidget *m_diskTab;
    QVBoxLayout *m_diskLayout;
    QTableWidget *m_diskTable;
    
    // 控制区域
    QGroupBox *m_controlGroup;
    QPushButton *m_refreshBtn;
    QPushButton *m_exportBtn;
    QLabel *m_lastUpdateLabel;
    
    // 工作线程
    QThread *m_workerThread;
    SystemInfoWorker *m_worker;
    
    // 数据
    ::SystemInfo m_currentSystemInfo;
    QList<DiskInfo> m_currentDiskInfo;
};

#endif // SYSTEMINFO_H
