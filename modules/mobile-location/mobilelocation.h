
#ifndef MOBILELOCATION_H
#define MOBILELOCATION_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QProgressBar>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QClipboard>
#include <QApplication>
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QStandardPaths>
#include <QDir>
#include <QSettings>

#include "../../common/dynamicobjectbase.h"

// 手机号信息结构
struct MobileInfo {
    QString number;           // 手机号码
    QString country;          // 国家
    QString countryCode;      // 国家代码
    QString province;         // 省份
    QString city;            // 城市
    QString carrier;         // 运营商
    QString numberType;      // 号码类型（移动/固定等）
    bool isValid;            // 是否有效
    QString errorMessage;    // 错误信息
    
    MobileInfo() : isValid(false) {}
};

// 查询历史记录
struct QueryHistory {
    QString number;
    MobileInfo info;
    QDateTime queryTime;
};

class MobileLocation : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit MobileLocation();
    ~MobileLocation();

private slots:
    void onQueryButtonClicked();
    void onBatchQueryClicked();
    void onClearHistoryClicked();
    void onExportHistoryClicked();
    void onImportNumbersClicked();
    void onCopyResultClicked();
    void onNumberChanged();
    void onCountryChanged();
    void onQueryModeChanged();
    void onHistoryItemClicked(int row, int column);
    
    // 网络请求相关
    void onNetworkReplyFinished();
    void onNetworkError(QNetworkReply::NetworkError error);

private:
    void setupUI();
    void setupInputArea();
    void setupResultArea();
    void setupBatchArea();
    void setupHistoryArea();
    void setupStatusArea();
    
    // 查询功能
    MobileInfo queryDomesticNumber(const QString &number);
    MobileInfo queryInternationalNumber(const QString &number);
    void queryNumberOnline(const QString &number);
    
    // 数据处理
    bool isValidChineseNumber(const QString &number);
    bool isValidInternationalNumber(const QString &number);
    QString formatPhoneNumber(const QString &number);
    QString detectCountryCode(const QString &number);
    
    // 历史记录管理
    void addToHistory(const MobileInfo &info);
    void loadHistory();
    void saveHistory();
    void updateHistoryDisplay();
    
    // 批量处理
    void processBatchNumbers();
    void updateBatchProgress(int current, int total);
    
    // 结果显示
    void displayResult(const MobileInfo &info);
    void clearResultDisplay();
    
    // 在线查询结果解析
    MobileInfo parseOnlineResponse(const QString &response);
    
    // 数据初始化
    void initializeDomesticData();
    
    // UI组件
    QVBoxLayout *mainLayout;
    
    // 输入区域
    QGroupBox *inputGroup;
    QGridLayout *inputLayout;
    QLineEdit *numberEdit;
    QComboBox *countryCombo;
    QComboBox *queryModeCombo;
    QPushButton *queryBtn;
    QPushButton *clearBtn;
    
    // 结果显示区域
    QGroupBox *resultGroup;
    QVBoxLayout *resultLayout;
    QLabel *numberLabel;
    QLabel *countryLabel;
    QLabel *provinceLabel;
    QLabel *cityLabel;
    QLabel *carrierLabel;
    QLabel *typeLabel;
    QPushButton *copyResultBtn;
    
    // 批量处理区域
    QGroupBox *batchGroup;
    QVBoxLayout *batchLayout;
    QTextEdit *batchInput;
    QTableWidget *batchResults;
    QHBoxLayout *batchButtonLayout;
    QPushButton *batchQueryBtn;
    QPushButton *importBtn;
    QPushButton *exportBtn;
    QProgressBar *batchProgress;
    QLabel *batchStatusLabel;
    
    // 历史记录区域
    QGroupBox *historyGroup;
    QVBoxLayout *historyLayout;
    QTableWidget *historyTable;
    QHBoxLayout *historyButtonLayout;
    QPushButton *clearHistoryBtn;
    QPushButton *exportHistoryBtn;
    QLabel *historyCountLabel;
    
    // 状态栏
    QHBoxLayout *statusLayout;
    QLabel *statusLabel;
    QProgressBar *progressBar;
    
    // 数据成员
    QNetworkAccessManager *networkManager;
    QNetworkReply *currentReply;
    QString currentQueryNumber;
    QList<QueryHistory> historyList;
    QTimer *statusTimer;
    
    // 国内号段数据
    QHash<QString, QString> domesticCarriers;    // 号段->运营商
    QHash<QString, QString> domesticProvinces;   // 号段->省份
    QHash<QString, QString> domesticCities;      // 号段->城市
    
    // 国际区号数据
    QHash<QString, QString> countryCodes;       // 区号->国家
};

#endif // MOBILELOCATION_H
