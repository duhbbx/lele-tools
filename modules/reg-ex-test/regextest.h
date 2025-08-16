#ifndef REGEXTEST_H
#define REGEXTEST_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTabWidget>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QListWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QTimer>

#include "../../common/dynamicobjectbase.h"

// 正则表达式语法高亮器
class RegexHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit RegexHighlighter(QTextDocument* parent = nullptr);

protected:
    void highlightBlock(const QString& text) override;

private:
    void setupFormats();
    
    QTextCharFormat characterClassFormat;
    QTextCharFormat quantifierFormat;
    QTextCharFormat anchorFormat;
    QTextCharFormat groupFormat;
    QTextCharFormat escapeFormat;
    QTextCharFormat literalFormat;
};

class RegExTest : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit RegExTest();
    ~RegExTest();

public slots:
    void onTestRegex();
    void onRegexChanged();
    void onTextChanged();
    void onClearResults();
    void onCopyMatches();
    void onLoadPreset();

private:
    void setupUI();
    void setupToolbar();
    void setupInputArea();
    void setupResultsArea();
    void setupPresetsArea();
    void performRegexTest();
    void updateStatus(const QString& message, bool isError = false);
    void loadCommonPresets();
    
    // UI组件
    QVBoxLayout* mainLayout;
    QSplitter* mainSplitter;
    
    // 工具栏
    QWidget* toolbarWidget;
    QHBoxLayout* toolbarLayout;
    QPushButton* testBtn;
    QPushButton* clearBtn;
    QPushButton* copyBtn;
    QLabel* statusLabel;
    
    // 输入区域
    QWidget* inputWidget;
    QVBoxLayout* inputLayout;
    
    // 正则表达式输入
    QGroupBox* regexGroup;
    QLineEdit* regexEdit;
    RegexHighlighter* regexHighlighter;
    QHBoxLayout* flagsLayout;
    QCheckBox* caseSensitiveCheck;
    QCheckBox* multilineCheck;
    QCheckBox* dotAllCheck;
    QCheckBox* extendedCheck;
    
    // 测试文本输入
    QGroupBox* textGroup;
    QTextEdit* testTextEdit;
    
    // 结果区域
    QTabWidget* resultsTabWidget;
    
    // 匹配结果标签页
    QWidget* matchesTab;
    QVBoxLayout* matchesLayout;
    QTableWidget* matchesTable;
    
    // 替换标签页
    QWidget* replaceTab;
    QVBoxLayout* replaceLayout;
    QLineEdit* replaceEdit;
    QPushButton* replaceBtn;
    QPlainTextEdit* replaceResultEdit;
    
    // 预设模式
    QGroupBox* presetsGroup;
    QComboBox* presetsCombo;
    QPushButton* loadPresetBtn;
    
    // 状态
    QRegularExpression currentRegex;
    QTimer* testTimer;
};

#endif // REGEXTEST_H