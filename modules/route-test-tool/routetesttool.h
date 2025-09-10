#ifndef ROUTETESTTOOL_H
#define ROUTETESTTOOL_H

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
#include <QComboBox>
#include <QCheckBox>
#include <QTimer>
#include <QThread>
#include <QProcess>
#include <QSettings>
#include <QTabWidget>
#include <QSplitter>
#include <QDateTime>
#include <QElapsedTimer>

#include "../../common/dynamicobjectbase.h"

// 预定义的测试节点
struct TestNode {
    QString name;        // 节点名称
    QString ip;          // IP地址
    QString isp;         // 运营商
    QString location;    // 地理位置
    
    TestNode() = default;
    TestNode(const QString &n, const QString &i, const QString &isp_name, const QString &loc)
        : name(n), ip(i), isp(isp_name), location(loc) {}
};

Q_DECLARE_METATYPE(TestNode)

// 路由测试结果
struct RouteTestResult {
    QString targetName;     // 目标名称
    QString targetIP;       // 目标IP
    QStringList routeHops;  // 路由跳点列表
    bool success;           // 是否成功
    int totalHops;          // 总跳数
    QString errorMessage;   // 错误信息
    QDateTime testTime;     // 测试时间
    
    RouteTestResult() : success(false), totalHops(0) {}
};

// 路由测试工作线程
class RouteTestWorker : public QObject
{
    Q_OBJECT

public:
    explicit RouteTestWorker(QObject *parent = nullptr);

public slots:
    void testSingleRoute(const QString &ip, const QString &name);
    void testMultipleRoutes(const QList<TestNode> &nodes);
    void stopTest();

signals:
    void testStarted(const QString &target);
    void testCompleted(const RouteTestResult &result);
    void allTestsCompleted();
    void progressUpdated(int current, int total);
    void logMessage(const QString &message);

private:
    RouteTestResult performTraceroute(const QString &ip, const QString &name);
    QStringList parseTracerouteOutput(const QString &output);
    
    bool m_running;
    QProcess *m_process;
};

// 主路由测试工具类
class RouteTestTool : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit RouteTestTool(QWidget *parent = nullptr);
    ~RouteTestTool();

private slots:
    void onQuickTestClicked();
    void onCustomTestClicked();
    void onNodeTestClicked();
    void onStopTestClicked();
    void onClearResultsClicked();
    void onExportResultsClicked();
    void onCustomIPChanged();
    void onNodeSelectionChanged();
    
    // 工作线程信号处理
    void onTestStarted(const QString &target);
    void onTestCompleted(const RouteTestResult &result);
    void onAllTestsCompleted();
    void onProgressUpdated(int current, int total);
    void onLogMessage(const QString &message);

private:
    void setupUI();
    void setupQuickTestArea();
    void setupCustomTestArea();
    void setupNodeTestArea();
    void setupResultsArea();
    void setupControlArea();
    
    void initializeTestNodes();
    void updateUI();
    void addResultToTable(const RouteTestResult &result);
    void logMessage(const QString &message);
    void saveSettings();
    void loadSettings();
    
    QList<TestNode> getQuickTestNodes() const;
    QList<TestNode> getSelectedNodes() const;
    
    // UI组件
    QVBoxLayout *m_mainLayout;
    QSplitter *m_mainSplitter;
    
    // 左侧控制面板
    QWidget *m_controlPanel;
    QVBoxLayout *m_controlLayout;
    
    // 快速测试区域
    QGroupBox *m_quickTestGroup;
    QPushButton *m_quickTestBtn;
    QLabel *m_quickTestDesc;
    
    // 自定义测试区域
    QGroupBox *m_customTestGroup;
    QLineEdit *m_customIPEdit;
    QLineEdit *m_customNameEdit;
    QPushButton *m_customTestBtn;
    
    // 节点测试区域
    QGroupBox *m_nodeTestGroup;
    QComboBox *m_ispCombo;
    QComboBox *m_nodeCombo;
    QPushButton *m_nodeTestBtn;
    
    // 控制按钮区域
    QGroupBox *m_controlGroup;
    QPushButton *m_stopBtn;
    QPushButton *m_clearBtn;
    QPushButton *m_exportBtn;
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;
    
    // 右侧结果面板
    QWidget *m_resultsPanel;
    QVBoxLayout *m_resultsLayout;
    
    // 结果显示
    QTabWidget *m_resultTabs;
    QTableWidget *m_resultsTable;
    QTextEdit *m_logText;
    QTextEdit *m_currentTestText;
    
    // 数据存储
    QMap<QString, QList<TestNode>> m_testNodes;  // 按ISP分类的测试节点
    QList<RouteTestResult> m_testResults;        // 测试结果
    
    // 工作线程
    QThread *m_workerThread;
    RouteTestWorker *m_worker;
    
    // 状态管理
    bool m_testing;
    int m_currentTestIndex;
    int m_totalTests;
};

#endif // ROUTETESTTOOL_H
