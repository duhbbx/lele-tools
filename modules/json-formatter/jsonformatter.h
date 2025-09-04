#ifndef JSONFORMATTER_H
#define JSONFORMATTER_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QHeaderView>
#include <QScrollArea>

#include "../../common/dynamicobjectbase.h"

// JSON语法高亮器
class JsonHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit JsonHighlighter(QTextDocument* parent = nullptr);

protected:
    void highlightBlock(const QString& text) override;

private:
    void setupFormats();

    QTextCharFormat keyFormat;
    QTextCharFormat stringFormat;
    QTextCharFormat numberFormat;
    QTextCharFormat booleanFormat;
    QTextCharFormat nullFormat;
    QTextCharFormat punctuationFormat;
    QTextCharFormat errorFormat;
};

class JsonFormatter : public QWidget, public DynamicObjectBase {
    Q_OBJECT

public:
    explicit JsonFormatter();
    ~JsonFormatter();

public slots:
    void onInputTextChanged();
    void onFormatJson();
    void onMinifyJson();
    void onValidateJson();
    void onClearAll();
    void onCopyFormatted();
    void onExpandAll();
    void onCollapseAll();
    void onSearchInTree();

private:
    void setupUI();
    void setupToolbar();
    void setupInputArea();
    void setupOutputArea();
    void setupTreeView();
    void updateJsonTree(const QJsonDocument& doc);
    void addJsonValueToTree(QTreeWidgetItem* parent, const QString& key, const QJsonValue& value);
    void updateStatusBar(const QString& message, bool isError = false);
    QString getJsonValueTypeString(const QJsonValue& value);
    QIcon getJsonValueIcon(const QJsonValue& value);

    // UI组件
    QVBoxLayout* mainLayout;
    QHBoxLayout* toolbarLayout;
    QSplitter* mainSplitter;
    QSplitter* rightSplitter;

    // 工具栏
    QWidget* toolbarWidget;
    QPushButton* formatBtn;
    QPushButton* minifyBtn;
    QPushButton* validateBtn;
    QPushButton* clearBtn;
    QPushButton* copyBtn;
    QLabel* statusLabel;

    // 输入区域
    QWidget* inputWidget;
    QVBoxLayout* inputLayout;
    QLabel* inputLabel;
    QTextEdit* inputTextEdit;

    // 输出区域
    QTabWidget* outputTabWidget;
    QWidget* formattedTab;
    QWidget* treeTab;

    // 格式化输出
    QVBoxLayout* formattedLayout;
    QPlainTextEdit* outputTextEdit;
    JsonHighlighter* highlighter;

    // 树形视图
    QVBoxLayout* treeLayout;
    QHBoxLayout* treeToolbarLayout;
    QLineEdit* searchLineEdit;
    QPushButton* expandAllBtn;
    QPushButton* collapseAllBtn;
    QTreeWidget* jsonTreeWidget;

    // 状态
    bool isValidJson;
    QJsonDocument currentJsonDoc;
};

#endif // JSONFORMATTER_H
