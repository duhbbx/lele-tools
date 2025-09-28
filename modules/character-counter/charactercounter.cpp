#include "charactercounter.h"
#include <QApplication>
#include <QClipboard>
#include <QRegularExpression>

REGISTER_DYNAMICOBJECT(CharacterCounter);

CharacterCounter::CharacterCounter() {
    setupUI();
}

void CharacterCounter::setupUI() {
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // 创建分割器
    mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainSplitter->setHandleWidth(8);

    // 创建文本编辑区域
    editorWidget = new QWidget;
    editorLayout = new QVBoxLayout(editorWidget);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(8);

    // 编辑器标题
    editorTitle = new QLabel("文本编辑器");
    editorTitle->setStyleSheet("font-weight: bold; font-size: 14px; color: #333; padding: 5px;");
    editorLayout->addWidget(editorTitle);

    // 文本编辑框
    textEdit = new QTextEdit;
    textEdit->setPlaceholderText("请在此输入或粘贴需要统计的文本内容...");
    textEdit->setStyleSheet(
        "QTextEdit {"
        "    border: 2px solid #ddd;"
        "    padding: 2px;"
        "    font-size: 11px;"
        "    font-family: 'Microsoft YaHei', 'Consolas', monospace;"
        "}"
        "QTextEdit:focus {"
        "    border-color: #0078d7;"
        "}"
    );
    editorLayout->addWidget(textEdit);

    // 按钮区域
    buttonLayout = new QHBoxLayout;
    buttonLayout->setSpacing(10);

    clearBtn = new QPushButton("清空");
    clearBtn->setFixedSize(80, 32);
    clearBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #dc3545;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 4px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background-color: #c82333;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #bd2130;"
        "}"
    );

    copyBtn = new QPushButton("复制");
    copyBtn->setFixedSize(80, 32);
    copyBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #007bff;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 4px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background-color: #0056b3;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #004085;"
        "}"
    );

    pasteBtn = new QPushButton("粘贴");
    pasteBtn->setFixedSize(80, 32);
    pasteBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #28a745;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 4px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background-color: #218838;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #1e7e34;"
        "}"
    );

    buttonLayout->addWidget(clearBtn);
    buttonLayout->addWidget(copyBtn);
    buttonLayout->addWidget(pasteBtn);
    buttonLayout->addStretch();

    editorLayout->addLayout(buttonLayout);

    // 创建统计信息区域
    statsWidget = new QWidget;
    statsWidget->setFixedWidth(300);
    statsLayout = new QVBoxLayout(statsWidget);
    statsLayout->setContentsMargins(0, 0, 0, 0);
    statsLayout->setSpacing(0);

    // 统计标题
    statsTitle = new QLabel("统计信息");
    statsTitle->setStyleSheet("font-weight: bold; font-size: 14px; color: #333; padding: 5px;");
    statsLayout->addWidget(statsTitle);

    // 统计信息表格
    QGroupBox* statsGroup = new QGroupBox;
    statsGroup->setStyleSheet(
        "QGroupBox {"
        "    border: 2px solid #ddd;"
        "}"
    );
    statsGridLayout = new QGridLayout(statsGroup);
    statsGridLayout->setContentsMargins(15, 15, 15, 15);
    statsGridLayout->setHorizontalSpacing(20);
    statsGridLayout->setVerticalSpacing(8);

    // 创建统计标签
    QStringList labels = {
        "总字符数:", "中文字符:", "英文字符:", "数字字符:",
        "标点符号:", "空白字符:", "特殊字符:", "行数:",
        "单词数:", "字节数:"
    };

    QList<QLabel**> valuePtrs = {
        &totalCharsValue, &chineseCharsValue, &englishCharsValue, &digitsValue,
        &punctuationValue, &whitespaceValue, &specialCharsValue, &linesValue,
        &wordsValue, &bytesValue
    };

    for (int i = 0; i < labels.size(); ++i) {
        // 标签
        QLabel* label = new QLabel(labels[i]);
        label->setStyleSheet("font-weight: bold; color: #555;");
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        statsGridLayout->addWidget(label, i, 0);

        // 数值
        *valuePtrs[i] = new QLabel("0");
        (*valuePtrs[i])->setStyleSheet(
            "color: #0078d7; font-weight: bold; font-size: 13px; "
            "background-color: #f8f9fa; padding: 4px 8px; border-radius: 4px;"
        );
        (*valuePtrs[i])->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        (*valuePtrs[i])->setMinimumWidth(80);
        statsGridLayout->addWidget(*valuePtrs[i], i, 1);
    }

    statsLayout->addWidget(statsGroup);
    statsLayout->addStretch();

    // 添加到分割器
    mainSplitter->addWidget(editorWidget);
    mainSplitter->addWidget(statsWidget);
    mainSplitter->setSizes({700, 300});

    mainLayout->addWidget(mainSplitter);

    // 连接信号槽
    connect(textEdit, &QTextEdit::textChanged, this, &CharacterCounter::onTextChanged);
    connect(clearBtn, &QPushButton::clicked, this, &CharacterCounter::onClearText);
    connect(copyBtn, &QPushButton::clicked, this, &CharacterCounter::onCopyText);
    connect(pasteBtn, &QPushButton::clicked, this, &CharacterCounter::onPasteText);

    // 初始化统计
    updateStatistics();
}

void CharacterCounter::onTextChanged() {
    updateStatistics();
}

void CharacterCounter::onClearText() {
    textEdit->clear();
}

void CharacterCounter::onCopyText() {
    QApplication::clipboard()->setText(textEdit->toPlainText());
}

void CharacterCounter::onPasteText() {
    textEdit->insertPlainText(QApplication::clipboard()->text());
}

void CharacterCounter::updateStatistics() {
    QString text = textEdit->toPlainText();

    totalCharsValue->setText(QString::number(countTotalCharacters(text)));
    chineseCharsValue->setText(QString::number(countChineseCharacters(text)));
    englishCharsValue->setText(QString::number(countEnglishCharacters(text)));
    digitsValue->setText(QString::number(countDigits(text)));
    punctuationValue->setText(QString::number(countPunctuation(text)));
    whitespaceValue->setText(QString::number(countWhitespace(text)));
    specialCharsValue->setText(QString::number(countSpecialCharacters(text)));
    linesValue->setText(QString::number(countLines(text)));
    wordsValue->setText(QString::number(countWords(text)));
    bytesValue->setText(QString::number(countBytes(text)));
}

int CharacterCounter::countTotalCharacters(const QString& text) {
    return text.length();
}

int CharacterCounter::countChineseCharacters(const QString& text) {
    int count = 0;
    for (const QChar& ch : text) {
        ushort unicode = ch.unicode();
        // Unicode中文字符范围
        if ((unicode >= 0x4E00 && unicode <= 0x9FFF) ||  // CJK统一汉字
            (unicode >= 0x3400 && unicode <= 0x4DBF) ||  // CJK扩展A
            (unicode >= 0x20000 && unicode <= 0x2A6DF) || // CJK扩展B
            (unicode >= 0x2A700 && unicode <= 0x2B73F) || // CJK扩展C
            (unicode >= 0x2B740 && unicode <= 0x2B81F) || // CJK扩展D
            (unicode >= 0x2B820 && unicode <= 0x2CEAF) || // CJK扩展E
            (unicode >= 0x3000 && unicode <= 0x303F) ||  // CJK符号和标点
            (unicode >= 0xFF00 && unicode <= 0xFFEF)) {  // 全角字符
            count++;
        }
    }
    return count;
}

int CharacterCounter::countEnglishCharacters(const QString& text) {
    int count = 0;
    for (const QChar& ch : text) {
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) {
            count++;
        }
    }
    return count;
}

int CharacterCounter::countDigits(const QString& text) {
    int count = 0;
    for (const QChar& ch : text) {
        if (ch.isDigit()) {
            count++;
        }
    }
    return count;
}

int CharacterCounter::countPunctuation(const QString& text) {
    int count = 0;
    for (const QChar& ch : text) {
        if (ch.isPunct()) {
            count++;
        }
    }
    return count;
}

int CharacterCounter::countWhitespace(const QString& text) {
    int count = 0;
    for (const QChar& ch : text) {
        if (ch.isSpace()) {
            count++;
        }
    }
    return count;
}

int CharacterCounter::countSpecialCharacters(const QString& text) {
    int count = 0;
    for (const QChar& ch : text) {
        // 特殊字符：非字母、非数字、非空白、非标点的字符
        if (!ch.isLetterOrNumber() && !ch.isSpace() && !ch.isPunct()) {
            count++;
        }
    }
    return count;
}

int CharacterCounter::countLines(const QString& text) {
    if (text.isEmpty()) {
        return 0;
    }
    return text.count('\n') + 1;
}

int CharacterCounter::countWords(const QString& text) {
    if (text.trimmed().isEmpty()) {
        return 0;
    }

    int wordCount = 0;

    // 先统计中文字符数（每个中文字符算一个词）
    for (const QChar& ch : text) {
        ushort unicode = ch.unicode();
        if ((unicode >= 0x4E00 && unicode <= 0x9FFF) ||  // CJK统一汉字
            (unicode >= 0x3400 && unicode <= 0x4DBF) ||  // CJK扩展A
            (unicode >= 0x20000 && unicode <= 0x2A6DF) || // CJK扩展B
            (unicode >= 0x2A700 && unicode <= 0x2B73F) || // CJK扩展C
            (unicode >= 0x2B740 && unicode <= 0x2B81F) || // CJK扩展D
            (unicode >= 0x2B820 && unicode <= 0x2CEAF)) { // CJK扩展E
            wordCount++;
        }
    }

    // 再统计英文单词（按空白字符分隔）
    QRegularExpression englishWordRegex(R"([a-zA-Z]+(?:'[a-zA-Z]+)?)");
    QRegularExpressionMatchIterator matches = englishWordRegex.globalMatch(text);

    while (matches.hasNext()) {
        matches.next();
        wordCount++;
    }

    return wordCount;
}

int CharacterCounter::countBytes(const QString& text) {
    return text.toUtf8().size();
}