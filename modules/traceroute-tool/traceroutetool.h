#ifndef TRACEROUTETOOL_H
#define TRACEROUTETOOL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QProgressBar>
#include <QSpinBox>
#include <QCheckBox>
#include <QProcess>
#include <QHostInfo>

#include "../../common/dynamicobjectbase.h"

// 路由跳跃信息结构
struct RouteHop {
    int hopNumber = 0;
    QString ipAddress;
    QString hostname;
    QString region;             // 地理位置（来自 ip2region）
    double rtt1 = -1, rtt2 = -1, rtt3 = -1;   // 三次探测的 RTT (ms)
    bool timeout = false;       // 完全超时（三次都没回包）
};

// 主工具类 — 直接 shell out 系统 traceroute / tracert
class TracerouteTool : public QWidget, public DynamicObjectBase
{
    Q_OBJECT
public:
    explicit TracerouteTool(QWidget* parent = nullptr);
    ~TracerouteTool() override;

private slots:
    void onStartTraceroute();
    void onStopTraceroute();
    void onClearResults();
    void onExportResults();
    void onTargetChanged();

    void onProcessOutput();
    void onProcessError();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    void setupUI();
    QWidget* buildTargetCard();
    QWidget* buildOptionsCard();
    QWidget* buildResultsArea();

    void appendHop(const RouteHop& hop);
    void parseLine(const QString& line);
    void setBusyState(bool busy);
    void updateStatus(const QString& msg, bool err = false);

    // UI
    QLineEdit* m_targetEdit = nullptr;
    QSpinBox* m_maxHopsSpin = nullptr;
    QSpinBox* m_timeoutSpin = nullptr;
    QCheckBox* m_resolveNamesCheck = nullptr;
    QPushButton* m_startBtn = nullptr;
    QPushButton* m_stopBtn = nullptr;
    QPushButton* m_clearBtn = nullptr;
    QPushButton* m_exportBtn = nullptr;
    QTableWidget* m_table = nullptr;
    QProgressBar* m_progress = nullptr;
    QLabel* m_status = nullptr;

    // 进程
    QProcess* m_proc = nullptr;
    QByteArray m_pendingBuffer;   // 流式按行解析的剩余字节

    // 状态
    QString m_target;
    int m_lastHopSeen = 0;
};

#endif // TRACEROUTETOOL_H
