#include "base64encodedecode.h"

#include <QDebug>
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QFont>

REGISTER_DYNAMICOBJECT(Base64EncodeDecode);

Base64EncodeDecode::Base64EncodeDecode() : QWidget(nullptr), DynamicObjectBase()
{
    setupUI();
    
    // 连接信号槽
    connect(encodeBtn, &QPushButton::clicked, this, &Base64EncodeDecode::onEncodeClicked);
    connect(decodeBtn, &QPushButton::clicked, this, &Base64EncodeDecode::onDecodeClicked);
    connect(clearBtn, &QPushButton::clicked, this, &Base64EncodeDecode::onClearClicked);
    connect(inputTextEdit, &QTextEdit::textChanged, this, &Base64EncodeDecode::onInputTextChanged);
    
    // 设置默认示例
    QString sampleText = "Hello, 乐乐的工具箱！\n这是一个Base64编码解码工具。";
    inputTextEdit->setPlainText(sampleText);
    onEncodeClicked();
}

Base64EncodeDecode::~Base64EncodeDecode()
{
}

void Base64EncodeDecode::setupUI()
{
    // 主布局
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);
    
    // 设置工具栏
    setupToolbar();
    
    // 设置输入输出区域
    setupInputOutput();
    
    // 添加到主布局
    mainLayout->addWidget(toolbarWidget);
    mainLayout->addWidget(mainSplitter);
    
    // 设置样式
    setStyleSheet(R"(
        QWidget {
            font-family: 'Segoe UI', Arial, sans-serif;
        }
        QPushButton {
            background-color: #f0f0f0;
            border: 1px solid #cccccc;
            border-radius: 4px;
            padding: 4px 8px;
            font-weight: bold;
            font-size: 11pt;
        }
        QPushButton:hover {
            background-color: #e0e0e0;
            border-color: #999999;
        }
        QPushButton:pressed {
            background-color: #d0d0d0;
        }
        QTextEdit, QPlainTextEdit {
            border: 2px solid #dddddd;
            border-radius: 8px;
            padding: 8px;
            font-family: 'Consolas', 'Monaco', monospace;
            font-size: 11pt;
        }
        QTextEdit:focus, QPlainTextEdit:focus {
            border-color: #4CAF50;
        }
        QGroupBox {
            font-weight: bold;
            border: 2px solid #cccccc;
            border-radius: 8px;
            margin-top: 1ex;
            padding-top: 10px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px 0 5px;
        }
    )");
}

void Base64EncodeDecode::setupToolbar()
{
    toolbarWidget = new QWidget();
    toolbarWidget->setFixedHeight(45);
    toolbarLayout = new QHBoxLayout(toolbarWidget);
    toolbarLayout->setContentsMargins(5, 5, 5, 5);
    toolbarLayout->setSpacing(8);
    
    // 创建按钮
    encodeBtn = new QPushButton("🔒 编码");
    encodeBtn->setToolTip("将文本编码为Base64格式");
    encodeBtn->setFixedSize(75, 32);
    
    decodeBtn = new QPushButton("🔓 解码");
    decodeBtn->setToolTip("将Base64解码为原始文本");
    decodeBtn->setFixedSize(75, 32);
    
    clearBtn = new QPushButton("🗑️ 清空");
    clearBtn->setToolTip("清空所有内容");
    clearBtn->setFixedSize(75, 32);
    
    // 状态标签
    statusLabel = new QLabel("就绪");
    statusLabel->setFixedHeight(32);
    statusLabel->setStyleSheet(R"(
        QLabel {
            color: #666; 
            font-weight: bold; 
            padding: 6px 12px; 
            background: #f9f9f9; 
            border-radius: 6px; 
            border: 1px solid #ddd;
        }
    )");
    
    toolbarLayout->addWidget(encodeBtn);
    toolbarLayout->addWidget(decodeBtn);
    toolbarLayout->addWidget(clearBtn);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(statusLabel);
}

void Base64EncodeDecode::setupInputOutput()
{
    // 创建分割器
    mainSplitter = new QSplitter(Qt::Horizontal);
    mainSplitter->setChildrenCollapsible(false);
    
    // 输入区域
    inputWidget = new QWidget();
    inputLayout = new QVBoxLayout(inputWidget);
    inputLayout->setContentsMargins(0, 0, 0, 0);
    inputLayout->setSpacing(8);
    
    inputLabel = new QLabel("📝 输入文本");
    inputLabel->setStyleSheet("font-weight: bold; font-size: 12pt; color: #333;");
    
    inputTextEdit = new QTextEdit();
    inputTextEdit->setPlaceholderText("请输入要编码或解码的文本...");
    
    inputLayout->addWidget(inputLabel);
    inputLayout->addWidget(inputTextEdit);
    
    // 输出区域
    outputWidget = new QWidget();
    outputLayout = new QVBoxLayout(outputWidget);
    outputLayout->setContentsMargins(0, 0, 0, 0);
    outputLayout->setSpacing(8);
    
    outputLabel = new QLabel("📄 输出结果");
    outputLabel->setStyleSheet("font-weight: bold; font-size: 12pt; color: #333;");
    
    outputTextEdit = new QPlainTextEdit();
    outputTextEdit->setReadOnly(true);
    outputTextEdit->setPlaceholderText("编码或解码结果将显示在这里...");
    
    // 设置等宽字体
    QFont monoFont("Consolas", 11);
    if (!monoFont.exactMatch()) {
        monoFont.setFamily("Monaco");
        if (!monoFont.exactMatch()) {
            monoFont.setFamily("Courier New");
        }
    }
    outputTextEdit->setFont(monoFont);
    
    outputLayout->addWidget(outputLabel);
    outputLayout->addWidget(outputTextEdit);
    
    // 添加到分割器
    mainSplitter->addWidget(inputWidget);
    mainSplitter->addWidget(outputWidget);
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 1);
}

void Base64EncodeDecode::onEncodeClicked()
{
    QString inputText = inputTextEdit->toPlainText();
    if (inputText.isEmpty()) {
        updateStatus("输入为空，请输入要编码的文本", true);
        return;
    }
    
    try {
        // 将文本转换为UTF-8字节数组，然后编码为Base64
        QByteArray utf8Data = inputText.toUtf8();
        QString base64Result = utf8Data.toBase64();
        
        outputTextEdit->setPlainText(base64Result);
        outputLabel->setText("📄 Base64编码结果");
        updateStatus(QString("编码完成，输入%1字符，输出%2字符").arg(inputText.length()).arg(base64Result.length()), false);
        
    } catch (const std::exception& e) {
        updateStatus(QString("编码失败: %1").arg(e.what()), true);
    }
}

void Base64EncodeDecode::onDecodeClicked()
{
    QString inputText = inputTextEdit->toPlainText().trimmed();
    if (inputText.isEmpty()) {
        updateStatus("输入为空，请输入要解码的Base64文本", true);
        return;
    }
    
    try {
        // 将Base64字符串解码为字节数组，然后转换为UTF-8文本
        QByteArray base64Data = inputText.toUtf8();
        QByteArray decodedData = QByteArray::fromBase64(base64Data);
        
        if (decodedData.isEmpty() && !inputText.isEmpty()) {
            updateStatus("解码失败，输入的Base64格式可能不正确", true);
            return;
        }
        
        QString result = QString::fromUtf8(decodedData);
        outputTextEdit->setPlainText(result);
        outputLabel->setText("📄 Base64解码结果");
        updateStatus(QString("解码完成，输入%1字符，输出%2字符").arg(inputText.length()).arg(result.length()), false);
        
    } catch (const std::exception& e) {
        updateStatus(QString("解码失败: %1").arg(e.what()), true);
    }
}

void Base64EncodeDecode::onClearClicked()
{
    inputTextEdit->clear();
    outputTextEdit->clear();
    outputLabel->setText("📄 输出结果");
    updateStatus("已清空所有内容", false);
}

void Base64EncodeDecode::onInputTextChanged()
{
    QString text = inputTextEdit->toPlainText();
    if (text.isEmpty()) {
        updateStatus("输入为空", false);
    } else {
        updateStatus(QString("输入文本长度: %1 字符").arg(text.length()), false);
    }
}

void Base64EncodeDecode::updateStatus(const QString& message, bool isError)
{
    statusLabel->setText(message);
    if (isError) {
        statusLabel->setStyleSheet(R"(
            QLabel {
                color: #d32f2f; 
                font-weight: bold; 
                padding: 6px 12px; 
                background: #ffebee; 
                border-radius: 6px; 
                border: 1px solid #f8bbd9;
            }
        )");
    } else {
        statusLabel->setStyleSheet(R"(
            QLabel {
                color: #2e7d32; 
                font-weight: bold; 
                padding: 6px 12px; 
                background: #e8f5e8; 
                border-radius: 6px; 
                border: 1px solid #c8e6c9;
            }
        )");
    }
}
