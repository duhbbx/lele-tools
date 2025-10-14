#ifndef PORTSCANNER_H
#define PORTSCANNER_H

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QMessageBox>
#include <QProgressBar>
#include <QComboBox>
#include <QGroupBox>
#include <QProcess>
#include <QTimer>
#include <QRegularExpression>
#include <QApplication>
#include <QFileInfo>
#include <QMenu>
#include <QDir>
#include "../../common/dynamicobjectbase.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <iphlpapi.h>
#include <tcpmib.h>
#include <udpmib.h>
#include <psapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "psapi.lib")
#endif

struct PortInfo {
    QString protocol;       // TCP/UDP
    QString localAddress;   // 本地地址
    QString localPort;      // 本地端口
    QString remoteAddress;  // 远程地址
    QString remotePort;     // 远程端口
    QString state;          // 连接状态
    QString processName;    // 进程名
    QString processId;      // 进程ID
    QString processPath;    // 进程路径
};

class PortScanner : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit PortScanner(QWidget *parent = nullptr);
    ~PortScanner();

private slots:
    void refreshPortList();
    void cancelScan();
    void searchPorts();
    void clearSearch();
    void onTableItemDoubleClicked(int row, int column);
    void copySelectedToClipboard();
    void exportToFile();
    void processBatchLines();
    void updateTableBatch();
    void showContextMenu(const QPoint& pos);
    void onKillProcessRequested();
    void onShowProcessPathRequested();

private:
    // UI 组件
    QVBoxLayout* mainLayout;
    QHBoxLayout* controlLayout;
    QHBoxLayout* searchLayout;

    // 控制区域
    QGroupBox* controlGroup;
    QPushButton* refreshButton;
    QPushButton* cancelButton;
    QPushButton* exportButton;
    QLabel* countLabel;
    QProgressBar* progressBar;

    // 搜索区域
    QGroupBox* searchGroup;
    QLineEdit* searchEdit;
    QComboBox* searchTypeCombo;
    QPushButton* searchButton;
    QPushButton* clearButton;

    // 表格
    QTableWidget* portTable;

    // 数据
    QList<PortInfo> portInfoList;
    QList<PortInfo> filteredPortInfoList;
    QProcess* currentProcess;

    // 分批处理
    QStringList pendingLines;
    int currentLineIndex;
    QTimer* parseTimer;

    // 表格异步更新
    int currentTableIndex;
    QTimer* tableTimer;

    // 功能方法
    void setupUI();
    void setupTable();
    void setupConnections();
    void populateTable(const QList<PortInfo>& portList);
    void populateTableAsync();
    void parseNetstatOutput(const QString& output);
    void parseNetstatLineWindows(const QString& line);
    void parseNetstatLineLinux(const QString& line);
    QString getProcessName(const QString& pid);
    void showStatusMessage(const QString& message, int timeout = 3000);
    void resizeTableColumns();

#ifdef Q_OS_WIN
    // Windows API 方法
    void scanPortsWithWindowsAPI();
    void scanTcpConnections();
    void scanUdpConnections();
    QString getProcessNameByPid(DWORD pid);
    QString getProcessPathByPid(DWORD pid);
    QString getStateString(DWORD state);
    QString formatAddress(DWORD addr, WORD port);
    void killProcessByPid(DWORD pid);
#endif

    // 样式
    void applyStyles();
};

#endif // PORTSCANNER_H