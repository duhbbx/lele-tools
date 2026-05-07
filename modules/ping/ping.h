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

// 实时 RTT 折线图（轻量自绘控件，避免引入 QtCharts）
class RttChart : public QWidget
{
    Q_OBJECT
public:
    explicit RttChart(QWidget* parent = nullptr);
    void addPoint(double rttMs, bool success);
    void clear();
    void setMaxPoints(int n) { m_maxPoints = n; }

protected:
    void paintEvent(QPaintEvent* e) override;
    QSize sizeHint() const override { return QSize(400, 140); }

private:
    struct P { double rtt; bool ok; };
    QList<P> m_points;
    int m_maxPoints = 200;
};

struct PingStatistics {
    int packetsSent;
    int packetsReceived;
    int packetsLost;
    double lossPercentage;
    double minTime;
    double maxTime;
    double avgTime;
    double jitter;       // RTT 抖动（标准差）
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
    QWidget* inputGroup;
    QGridLayout* inputLayout;
    QLabel* hostLabel;
    QLineEdit* hostEdit;
    QPushButton* resolveBtn;
    QLabel* ipLabel;
    QLabel* ipValueLabel;

    // 控制区域
    QWidget* controlGroup;
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
    QWidget* statsGroup;
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
    QLabel* jitterLabel;
    QLabel* jitterValueLabel;
    RttChart* rttChart;
    
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
    int attemptsMade;        // 实际发起的 ping 次数（用于正确计数）
    bool useIPv6;
};

#endif // PING_H