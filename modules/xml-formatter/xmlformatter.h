#ifndef XMLFORMATTER_H
#define XMLFORMATTER_H

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
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "../../common/dynamicobjectbase.h"

// XML语法高亮器
class XmlHighlighter final : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit XmlHighlighter(QTextDocument* parent = nullptr);

protected:
    void highlightBlock(const QString& text) override;

private:
    void setupFormats();

    QTextCharFormat xmlElementFormat;
    QTextCharFormat xmlAttributeFormat;
    QTextCharFormat xmlValueFormat;
    QTextCharFormat xmlCommentFormat;
    QTextCharFormat xmlKeywordFormat;
};

class XmlFormatter final : public QWidget, public DynamicObjectBase {
    Q_OBJECT

public:
    explicit XmlFormatter();
    ~XmlFormatter() override;

public slots:
    void onInputTextChanged();
    void onFormatXml() const;
    void onMinifyXml() const;
    void onValidateXml();
    void onClearAll();
    void onCopyFormatted();

private:
    void setupUI();
    void setupToolbar();
    void setupInputOutput();
    static QString formatXmlString(const QString& xml);
    static QString minifyXmlString(const QString& xml);
    static bool validateXmlString(const QString& xml, QString& errorMessage);
    void updateStatus(const QString& message, bool isError = false) const;

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

    // 输入输出区域
    QWidget* inputWidget;
    QWidget* outputWidget;
    QVBoxLayout* inputLayout;
    QVBoxLayout* outputLayout;

    QLabel* inputLabel;
    QLabel* outputLabel;
    QTextEdit* inputTextEdit;
    QPlainTextEdit* outputTextEdit;
    XmlHighlighter* highlighter;

    // 状态
    bool isValidXml;
};

#endif // XMLFORMATTER_H
