
#ifndef UUIDGEN_H
#define UUIDGEN_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QListWidget>
#include <QTabWidget>
#include <QProgressBar>
#include <QTimer>
#include <QDateTime>
#include <QUuid>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QStandardPaths>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStringConverter>
#include <QCryptographicHash>
#include <QRandomGenerator>

#include "../../common/dynamicobjectbase.h"

// UUID版本枚举
enum UuidVersion {
    UUID_V1 = 1,    // 基于时间戳和MAC地址
    UUID_V2 = 2,    // DCE安全UUID（很少使用）
    UUID_V3 = 3,    // 基于名称的MD5哈希
    UUID_V4 = 4,    // 随机或伪随机UUID
    UUID_V5 = 5     // 基于名称的SHA-1哈希
};

// UUID格式枚举
enum UuidFormat {
    FORMAT_STANDARD,        // 8-4-4-4-12 (带连字符)
    FORMAT_SIMPLE,          // 32位十六进制字符串
    FORMAT_BRACES,          // {8-4-4-4-12}
    FORMAT_PARENTHESES,     // (8-4-4-4-12)
    FORMAT_URN,             // urn:uuid:8-4-4-4-12
    FORMAT_UPPERCASE,       // 大写格式
    FORMAT_LOWERCASE        // 小写格式
};

// UUID信息结构
struct UuidInfo {
    QString uuid;
    UuidVersion version;
    QString timestamp;
    QString description;
    QDateTime generated;
    
    UuidInfo() : version(UUID_V4) {}
    UuidInfo(const QString &u, UuidVersion v, const QString &desc = "")
        : uuid(u), version(v), description(desc), generated(QDateTime::currentDateTime()) {}
};

class UuidGen : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit UuidGen();
    ~UuidGen();

private slots:
    // 生成相关
    void onGenerateSingleClicked();
    void onGenerateBatchClicked();
    void onVersionChanged();
    void onFormatChanged();
    void onCustomParametersChanged();
    
    // 验证和解析
    void onValidateClicked();
    void onParseClicked();
    void onInputUuidChanged();
    
    // 操作功能
    void onCopyClicked();
    void onCopyAllClicked();
    void onClearHistoryClicked();
    void onExportClicked();
    void onImportClicked();
    
    // 工具功能
    void onConvertFormatClicked();
    void onGenerateFromNameClicked();
    void onCompareUuidsClicked();
    
    // 历史记录
    void onHistoryItemClicked(QTableWidgetItem *item);
    void onDeleteHistoryItemClicked();

private:
    void setupUI();
    void setupGeneratorTab();
    void setupValidatorTab();
    void setupConverterTab();
    void setupHistoryTab();
    void setupToolbar();
    
    // 核心功能
    QString generateUuid(UuidVersion version, const QString &customData = "");
    QString generateUuidV1();
    QString generateUuidV3(const QString &namespace_uuid, const QString &name);
    QString generateUuidV4();
    QString generateUuidV5(const QString &namespace_uuid, const QString &name);
    
    // 格式化功能
    QString formatUuid(const QString &uuid, UuidFormat format);
    QString convertCase(const QString &uuid, bool uppercase);
    
    // 验证和解析功能
    bool isValidUuid(const QString &uuid);
    UuidInfo parseUuid(const QString &uuid);
    QString getVersionDescription(UuidVersion version);
    UuidVersion detectVersion(const QString &uuid);
    
    // 辅助功能
    void addToHistory(const UuidInfo &info);
    void updateHistoryTable();
    void updateStatistics();
    void saveSettings();
    void loadSettings();
    QString generateMD5Hash(const QString &data);
    QString generateSHA1Hash(const QString &data);
    
    // UI组件
    QTabWidget *mainTabWidget;
    
    // 生成器标签页
    QWidget *generatorTab;
    QGroupBox *settingsGroup;
    QComboBox *versionCombo;
    QComboBox *formatCombo;
    QSpinBox *countSpin;
    QLineEdit *namespaceEdit;
    QLineEdit *nameEdit;
    QPushButton *generateButton;
    QPushButton *batchGenerateButton;
    
    QGroupBox *resultGroup;
    QTextEdit *resultEdit;
    QPushButton *copyButton;
    QPushButton *copyAllButton;
    QLabel *statisticsLabel;
    
    // 验证器标签页
    QWidget *validatorTab;
    QGroupBox *inputGroup;
    QLineEdit *inputUuidEdit;
    QPushButton *validateButton;
    QPushButton *parseButton;
    
    QGroupBox *validationResultGroup;
    QLabel *validationStatusLabel;
    QTextEdit *parseResultEdit;
    
    // 转换器标签页
    QWidget *converterTab;
    QGroupBox *convertGroup;
    QLineEdit *convertInputEdit;
    QComboBox *convertFromCombo;
    QComboBox *convertToCombo;
    QPushButton *convertButton;
    
    QGroupBox *convertResultGroup;
    QTextEdit *convertResultEdit;
    QPushButton *copyConvertedButton;
    
    QGroupBox *toolsGroup;
    QPushButton *generateFromNameButton;
    QPushButton *compareButton;
    
    // 历史记录标签页
    QWidget *historyTab;
    QGroupBox *historyGroup;
    QTableWidget *historyTable;
    QPushButton *deleteItemButton;
    QPushButton *clearHistoryButton;
    QLabel *historyStatsLabel;
    
    // 工具栏
    QGroupBox *toolbarGroup;
    QPushButton *exportButton;
    QPushButton *importButton;
    QPushButton *settingsButton;
    
    // 数据成员
    QList<UuidInfo> uuidHistory;
    int totalGenerated;
    QTimer *updateTimer;
    
    // 常量
    static const QStringList VERSION_NAMES;
    static const QStringList FORMAT_NAMES;
    static const QStringList NAMESPACE_UUIDS;
};

#endif // UUIDGEN_H
