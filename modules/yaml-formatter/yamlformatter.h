#ifndef YAMLFORMATTER_H
#define YAMLFORMATTER_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QSpinBox>
#include <QCheckBox>
#include <QGroupBox>

#include "../../common/dynamicobjectbase.h"

// YAML语法高亮器
class YamlHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit YamlHighlighter(QTextDocument* parent = nullptr);

protected:
    void highlightBlock(const QString& text) override;

private:
    void setupFormats();
    
    QTextCharFormat keyFormat;
    QTextCharFormat valueFormat;
    QTextCharFormat commentFormat;
    QTextCharFormat stringFormat;
    QTextCharFormat numberFormat;
    QTextCharFormat booleanFormat;
    QTextCharFormat nullFormat;
    QTextCharFormat documentFormat;
    QTextCharFormat listFormat;
};

class YamlFormatter : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit YamlFormatter();
    ~YamlFormatter();

public slots:
    void onInputTextChanged();
    void onFormatYaml();
    void onMinifyYaml();
    void onValidateYaml();
    void onClearAll();
    void onCopyFormatted();
    void onIndentChanged();

private:
    void setupUI();
    void setupToolbar();
    void setupInputOutput();
    void setupOptions();
    QString formatYamlString(const QString& yaml, int indent);
    QString minifyYamlString(const QString& yaml);
    bool validateYamlString(const QString& yaml, QString& errorMessage);
    void updateStatus(const QString& message, bool isError = false);
    
    // UI组件
    QVBoxLayout* mainLayout;
    QSplitter* mainSplitter;
    
    // 工具栏
    QWidget* toolbarWidget;
    QHBoxLayout* toolbarLayout;
    QPushButton* formatBtn;
    QPushButton* minifyBtn;
    QPushButton* validateBtn;
    QPushButton* clearBtn;
    QPushButton* copyBtn;
    QLabel* statusLabel;
    
    // 选项区域
    QGroupBox* optionsGroup;
    QHBoxLayout* optionsLayout;
    QLabel* indentLabel;
    QSpinBox* indentSpinBox;
    QCheckBox* sortKeysCheck;
    QCheckBox* flowStyleCheck;
    
    // 输入输出区域
    QWidget* inputWidget;
    QWidget* outputWidget;
    QVBoxLayout* inputLayout;
    QVBoxLayout* outputLayout;
    
    QLabel* inputLabel;
    QLabel* outputLabel;
    QTextEdit* inputTextEdit;
    QPlainTextEdit* outputTextEdit;
    YamlHighlighter* highlighter;
    
    // 状态
    bool isValidYaml;
};

#endif // YAMLFORMATTER_H