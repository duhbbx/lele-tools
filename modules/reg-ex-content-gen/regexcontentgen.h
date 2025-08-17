#ifndef REGEXCONTENTGEN_H
#define REGEXCONTENTGEN_H

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
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QListWidget>
#include <QTabWidget>
#include <QProgressBar>
#include <QTimer>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QClipboard>
#include <QApplication>
#include <QMessageBox>
#include <QDateTime>
#include <QDebug>

#include "../../common/dynamicobjectbase.h"

// 正则表达式语法高亮器
class RegexSyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit RegexSyntaxHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightingRule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<HighlightingRule> highlightingRules;
};

// 正则表达式内容生成器主类
class RegExContentGen : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit RegExContentGen();
    ~RegExContentGen();

private slots:
    void onGenerateContent();
    void onClearAll();
    void onCopyResults();
    void onPresetSelected();
    void onRegexChanged();
    void onGenerateCountChanged();
    void onExportResults();
    void onImportRegex();

private:
    void setupUI();
    void setupInputArea();
    void setupOptionsArea();
    void setupResultsArea();
    void setupPresetArea();
    void setupStatusArea();
    
    // 核心生成逻辑
    QString generateFromRegex(const QString &regex);
    QStringList generateMultipleFromRegex(const QString &regex, int count);
    
    // 正则解析和生成辅助方法
    QString processRegexPattern(const QString &pattern);
    QString processCharacterClass(const QString &charClass);
    QString processQuantifier(const QString &base, const QString &quantifier);
    QString processGroup(const QString &group);
    QString processAlternation(const QString &alternation);
    QString generateRandomString(const QString &charset, int minLen, int maxLen);
    
    // 预设正则模式
    void initializePresets();
    void addPreset(const QString &name, const QString &regex, const QString &description);
    
    // UI组件
    QVBoxLayout *mainLayout;
    QSplitter *mainSplitter;
    
    // 输入区域
    QGroupBox *inputGroup;
    QVBoxLayout *inputLayout;
    QLineEdit *regexInput;
    QLabel *regexLabel;
    QLabel *statusLabel;
    RegexSyntaxHighlighter *syntaxHighlighter;
    
    // 选项区域
    QGroupBox *optionsGroup;
    QGridLayout *optionsLayout;
    QSpinBox *countSpinBox;
    QSpinBox *maxLengthSpinBox;
    QCheckBox *uniqueCheckBox;
    QCheckBox *caseSensitiveCheckBox;
    QPushButton *generateBtn;
    QPushButton *clearBtn;
    
    // 结果区域
    QGroupBox *resultsGroup;
    QVBoxLayout *resultsLayout;
    QTextEdit *resultsText;
    QHBoxLayout *resultsButtonLayout;
    QPushButton *copyBtn;
    QPushButton *exportBtn;
    
    // 预设区域
    QGroupBox *presetGroup;
    QVBoxLayout *presetLayout;
    QComboBox *presetCombo;
    QListWidget *presetList;
    QPushButton *importBtn;
    
    // 状态区域
    QHBoxLayout *statusLayout;
    QLabel *generatedCountLabel;
    QProgressBar *progressBar;
    
    // 数据成员
    QStringList generatedResults;
    QTimer *generateTimer;
    
    // 预设数据结构
    struct RegexPreset {
        QString name;
        QString regex;
        QString description;
    };
    QList<RegexPreset> presets;
};

#endif // REGEXCONTENTGEN_H