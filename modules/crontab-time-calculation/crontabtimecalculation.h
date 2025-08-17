
#ifndef CRONTABTIMECALCULATION_H
#define CRONTABTIMECALCULATION_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QPlainTextEdit>
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
#include <QCalendarWidget>
#include <QDateTimeEdit>
#include <QTimer>
#include <QDateTime>
#include <QTimeZone>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QStringConverter>

#include "../../common/dynamicobjectbase.h"

// Crontab字段枚举
enum CrontabField {
    MINUTE = 0,
    HOUR = 1,
    DAY = 2,
    MONTH = 3,
    WEEKDAY = 4
};

// Crontab表达式结构
struct CrontabExpression {
    QString minute;     // 分钟 (0-59)
    QString hour;       // 小时 (0-23)
    QString day;        // 日 (1-31)
    QString month;      // 月 (1-12)
    QString weekday;    // 星期 (0-7, 0和7都表示星期日)
    QString command;    // 命令
    
    CrontabExpression() : minute("*"), hour("*"), day("*"), month("*"), weekday("*") {}
    
    QString toString() const {
        return QString("%1 %2 %3 %4 %5").arg(minute, hour, day, month, weekday);
    }
};

// 语法高亮器
class CrontabSyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit CrontabSyntaxHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<HighlightingRule> highlightingRules;

    QTextCharFormat fieldFormat;
    QTextCharFormat numberFormat;
    QTextCharFormat specialFormat;
    QTextCharFormat errorFormat;
};

class CrontabTimeCalculation : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit CrontabTimeCalculation();
    ~CrontabTimeCalculation();

private slots:
    // 解析相关
    void onCrontabExpressionChanged();
    void onParseButtonClicked();
    void onValidateButtonClicked();
    
    // 生成相关
    void onGenerateButtonClicked();
    void onQuickTemplateClicked();
    void onFieldChanged();
    
    // 时间计算
    void onCalculateNextRunsClicked();
    void onTimezoneChanged();
    void updateCurrentTime();
    
    // 工具功能
    void onCopyExpressionClicked();
    void onCopyNextRunsClicked();
    void onExportClicked();
    void onImportClicked();
    void onClearAllClicked();
    
    // 模板和示例
    void onTemplateSelected(int index);
    void onExampleClicked();

private:
    void setupUI();
    void setupParseTab();
    void setupGenerateTab();
    void setupCalculateTab();
    void setupTemplateTab();
    
    // 核心功能
    bool parseCrontabExpression(const QString &expression, CrontabExpression &result);
    QString generateCrontabExpression() const;
    QStringList validateCrontabExpression(const QString &expression);
    QList<QDateTime> calculateNextRuns(const QString &expression, int count = 10);
    QString explainCrontabExpression(const QString &expression);
    
    // 辅助功能
    bool isValidField(const QString &field, CrontabField fieldType);
    QStringList expandField(const QString &field, int min, int max);
    QString getFieldName(CrontabField field);
    QString getFieldDescription(const QString &field, CrontabField fieldType);
    void updateExpressionPreview();
    void updateExplanation();
    void populateTemplates();
    void loadCommonExamples();
    
    // 时间匹配功能
    bool matchesCronExpression(const QDateTime &dateTime, const CrontabExpression &expr);
    bool matchesField(int value, const QString &field, int min, int max);
    
    // UI设置
    void setupToolbar();
    
    // UI组件
    QTabWidget *mainTabWidget;
    
    // 解析标签页
    QWidget *parseTab;
    QGroupBox *inputGroup;
    QLineEdit *expressionEdit;
    QPushButton *parseButton;
    QPushButton *validateButton;
    QPushButton *copyExpressionButton;
    
    QGroupBox *resultGroup;
    QTextEdit *explanationEdit;
    QTextEdit *validationEdit;
    QLabel *statusLabel;
    
    // 生成标签页
    QWidget *generateTab;
    QGroupBox *fieldsGroup;
    QComboBox *minuteCombo;
    QComboBox *hourCombo;
    QComboBox *dayCombo;
    QComboBox *monthCombo;
    QComboBox *weekdayCombo;
    QPushButton *generateButton;
    
    QGroupBox *previewGroup;
    QLineEdit *generatedExpressionEdit;
    QTextEdit *generatedExplanationEdit;
    
    QGroupBox *quickTemplatesGroup;
    QPushButton *everyMinuteBtn;
    QPushButton *everyHourBtn;
    QPushButton *everyDayBtn;
    QPushButton *everyWeekBtn;
    QPushButton *everyMonthBtn;
    
    // 时间计算标签页
    QWidget *calculateTab;
    QGroupBox *timeSettingsGroup;
    QLineEdit *calcExpressionEdit;
    QComboBox *timezoneCombo;
    QSpinBox *runCountSpin;
    QPushButton *calculateButton;
    QPushButton *copyNextRunsButton;
    
    QGroupBox *currentTimeGroup;
    QLabel *currentTimeLabel;
    QTimer *timeUpdateTimer;
    
    QGroupBox *nextRunsGroup;
    QTableWidget *nextRunsTable;
    
    // 模板和示例标签页
    QWidget *templateTab;
    QGroupBox *commonTemplatesGroup;
    QListWidget *templatesList;
    
    QGroupBox *examplesGroup;
    QTableWidget *examplesTable;
    
    QGroupBox *customTemplatesGroup;
    QPushButton *saveTemplateButton;
    QPushButton *loadTemplateButton;
    
    // 工具栏
    QGroupBox *toolbarGroup;
    QPushButton *exportButton;
    QPushButton *importButton;
    QPushButton *clearAllButton;
    
    // 数据成员
    CrontabExpression currentExpression;
    CrontabSyntaxHighlighter *syntaxHighlighter;
    QStringList commonTemplates;
    QStringList templateDescriptions;
    
    // 常量
    static const QStringList MINUTE_OPTIONS;
    static const QStringList HOUR_OPTIONS;
    static const QStringList DAY_OPTIONS;
    static const QStringList MONTH_OPTIONS;
    static const QStringList WEEKDAY_OPTIONS;
    static const QStringList MONTH_NAMES;
    static const QStringList WEEKDAY_NAMES;
};

#endif // CRONTABTIMECALCULATION_H
