#ifndef BASE64ENCODEDECODE_H
#define BASE64ENCODEDECODE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QSplitter>
#include <QGroupBox>

#include "../../common/dynamicobjectbase.h"

class Base64EncodeDecode : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit Base64EncodeDecode();
    ~Base64EncodeDecode();

public slots:
    void onEncodeClicked();
    void onDecodeClicked();
    void onClearClicked();
    void onInputTextChanged();

private:
    void setupUI();
    void setupToolbar();
    void setupInputOutput();
    void updateStatus(const QString& message, bool isError = false);
    
    // UI组件
    QVBoxLayout* mainLayout;
    QHBoxLayout* toolbarLayout;
    QSplitter* mainSplitter;
    
    // 工具栏
    QWidget* toolbarWidget;
    QPushButton* encodeBtn;
    QPushButton* decodeBtn;
    QPushButton* clearBtn;
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
};

#endif // BASE64ENCODEDECODE_H
