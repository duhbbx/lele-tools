#ifndef ROUTETESTTOOL_H
#define ROUTETESTTOOL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QProgressBar>
#include <QListWidget>
#include <QTabWidget>
#include <QProcess>
#include <QHostInfo>

#include "../../common/dynamicobjectbase.h"

// 一个测试节点
struct RouteTarget {
    QString name;        // 显示名（"上海电信"）
    QString ip;          // 目标 IP
    QString isp;         // 运营商分组
};
Q_DECLARE_METATYPE(RouteTarget)

class RouteTestTool : public QWidget, public DynamicObjectBase
{
    Q_OBJECT
public:
    explicit RouteTestTool(QWidget* parent = nullptr);
    ~RouteTestTool() override;

private slots:
    void onStartSelected();
    void onAddCustom();
    void onStop();
    void onClearTabs();
    void onCopyAll();

    void onProcOutput();
    void onProcFinished(int code, QProcess::ExitStatus status);

private:
    // UI
    void setupUI();
    QWidget* buildPresetCard();
    QWidget* buildCustomCard();
    QWidget* buildResultsArea();

    // 队列
    void runNextTarget();
    void parseLine(const QString& line);
    void appendHopToCurrent(int hop, const QString& ip, const QString& host,
                            double r1, double r2, double r3,
                            bool timeout, const QString& region);
    QTableWidget* makeHopTable();

    void setBusy(bool busy);
    void setStatus(const QString& s, bool err = false);
    void populatePresets();

    // Widgets
    QListWidget* m_presetList = nullptr;     // 内置节点（带复选）
    QLineEdit* m_customName = nullptr;
    QLineEdit* m_customIp = nullptr;
    QPushButton* m_addCustomBtn = nullptr;
    QPushButton* m_startBtn = nullptr;
    QPushButton* m_stopBtn = nullptr;
    QPushButton* m_clearBtn = nullptr;
    QPushButton* m_copyBtn = nullptr;
    QProgressBar* m_progress = nullptr;
    QLabel* m_status = nullptr;
    QTabWidget* m_tabs = nullptr;

    // 进程 / 队列
    QProcess* m_proc = nullptr;
    QByteArray m_pendingBuffer;
    QList<RouteTarget> m_queue;
    int m_currentIdx = -1;
    QTableWidget* m_currentTable = nullptr;
    int m_currentLastHop = 0;
};

#endif // ROUTETESTTOOL_H
