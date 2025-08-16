#ifndef PING_H
#define PING_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QSpinBox>
#include <QProgressBar>
#include <QTimer>
#include <QProcess>
#include <QGroupBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QSplitter>
#include <QCheckBox>
#include <QComboBox>
#include <QtNetwork/QHostInfo>
#include <QtNetwork/QNetworkInterface>

#include "../../common/dynamicobjectbase.h"

struct PingResult {
    QString host;
    QString ip;
    int sequenceNumber;
    double responseTime;
    int ttl;
    bool success;
    QString errorMessage;
};

struct PingStatistics {
    int packetsSent;
    int packetsReceived;
    int packetsLost;
    double lossPercentage;
    double minTime;
    double maxTime;
    double avgTime;
    QString resolvedIP;
};

class Ping : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit Ping();
    ~Ping();

public slots:
    void onStartPing();
    void onStopPing();
    void onClearResults();
    void onCopyResults();
    void onHostChanged();
    void onPingFinished();
    void onPingTimeout();
    void onResolveHost();

private slots:
    void processPingOutput();
    void processPingError();
    void pingProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void setupUI();
    void setupInputArea();
    void setupControlArea();
    void setupResultsArea();
    void setupStatisticsArea();
    void startPingProcess();
    void stopPingProcess();
    void updateStatistics();
    void addPingResult(const PingResult& result);
    void updateStatus(const QString& message, bool isError = false);
    void resolveHostname();
    PingResult parsePingOutput(const QString& output);
    
    // UI组件
    QVBoxLayout* mainLayout;
    QSplitter* mainSplitter;
    
    // 输入区域
    QGroupBox* inputGroup;
    QGridLayout* inputLayout;
    QLabel* hostLabel;
    QLineEdit* hostEdit;
    QPushButton* resolveBtn;
    QLabel* ipLabel;
    QLabel* ipValueLabel;
    
    // 控制区域
    QGroupBox* controlGroup;
    QHBoxLayout* controlLayout;
    QLabel* countLabel;
    QSpinBox* countSpinBox;
    QLabel* intervalLabel;
    QSpinBox* intervalSpinBox;
    QLabel* timeoutLabel;
    QSpinBox* timeoutSpinBox;
    QCheckBox* continuousCheck;
    
    // 按钮区域
    QHBoxLayout* buttonLayout;
    QPushButton* startBtn;
    QPushButton* stopBtn;
    QPushButton* clearBtn;
    QPushButton* copyBtn;
    QLabel* statusLabel;
    
    // 结果区域
    QWidget* resultsWidget;
    QVBoxLayout* resultsLayout;
    QLabel* resultsLabel;
    QTableWidget* resultsTable;
    
    // 统计区域
    QGroupBox* statsGroup;
    QGridLayout* statsLayout;
    QLabel* sentLabel;
    QLabel* sentValueLabel;
    QLabel* receivedLabel;
    QLabel* receivedValueLabel;
    QLabel* lossLabel;
    QLabel* lossValueLabel;
    QLabel* minLabel;
    QLabel* minValueLabel;
    QLabel* maxLabel;
    QLabel* maxValueLabel;
    QLabel* avgLabel;
    QLabel* avgValueLabel;
    
    // 进度显示
    QProgressBar* progressBar;
    
    // 状态和数据
    QProcess* pingProcess;
    QTimer* pingTimer;
    QTimer* timeoutTimer;
    PingStatistics statistics;
    QList<PingResult> pingResults;
    bool isPinging;
    int currentSequence;
    QString currentHost;
    QString resolvedIP;
};

#endif // PING_H