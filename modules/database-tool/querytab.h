#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QLabel>
#include <QToolBar>
#include <QAction>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QRegularExpression>

#include "../../common/connx/Connection.h"

// SQL语法高亮器
class SqlSyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit SqlSyntaxHighlighter(QTextDocument* parent = nullptr);

protected:
    void highlightBlock(const QString& text) override;

private:
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<HighlightingRule> highlightingRules;

    QTextCharFormat keywordFormat;
    QTextCharFormat functionFormat;
    QTextCharFormat stringFormat;
    QTextCharFormat commentFormat;
    QTextCharFormat numberFormat;
    QTextCharFormat operatorFormat;
};

// 查询标签页
class QueryTab : public QWidget {
    Q_OBJECT

public:
    explicit QueryTab(Connx::Connection* connection, QWidget* parent = nullptr);

    void setQuery(const QString& query);
    QString getQuery() const;
    void executeQuery();

signals:
    void titleChanged(const QString& title);
    void queryExecuted(const Connx::QueryResult& result);

private slots:
    void onExecuteClicked();
    void onClearClicked() const;
    void onFormatClicked() const;

private:
    void setupUI();
    void updateResults(const Connx::QueryResult& result) const;

    Connx::Connection* m_connection;
    QVBoxLayout* m_layout;

    // 工具栏
    QToolBar* m_toolbar;
    QAction* m_executeAction;
    QAction* m_clearAction;
    QAction* m_formatAction;

    // 查询区域
    QTextEdit* m_queryEdit;
    SqlSyntaxHighlighter* m_syntaxHighlighter;

    // 结果区域
    QTabWidget* m_resultTabs;
    QTableWidget* m_resultTable;
    QTextEdit* m_messagesText;

    // 状态栏
    QLabel* m_statusLabel;
    QLabel* m_timeLabel;
    QLabel* m_rowsLabel;
};
