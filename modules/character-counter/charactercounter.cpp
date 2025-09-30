#include "charactercounter.h"
#include <QApplication>
#include <QClipboard>
#include <QRegularExpression>
#include <QtConcurrent>

REGISTER_DYNAMICOBJECT(CharacterCounter);

CharacterCounter::CharacterCounter() : isCalculating(false) {
    // 初始化定时器和监视器
    updateTimer = new QTimer(this);
    updateTimer->setSingleShot(true);
    updateTimer->setInterval(300); // 300ms防抖延迟

    statsWatcher = new QFutureWatcher<CharacterStats>(this);

    setupUI();

    // 连接信号
    connect(updateTimer, &QTimer::timeout, this, &CharacterCounter::performUpdate);
    connect(statsWatcher, &QFutureWatcher<CharacterStats>::finished, this, &CharacterCounter::onStatsCalculated);
}

CharacterCounter::~CharacterCounter() {
    // 等待异步任务完成
    if (statsWatcher->isRunning()) {
        statsWatcher->waitForFinished();
    }
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

    // 使用QPlainTextEdit替代QTextEdit以获得更好的大文本性能
    textEdit = new QPlainTextEdit;
    textEdit->setPlaceholderText("请在此输入或粘贴需要统计的文本内容...");
    textEdit->setStyleSheet(
        "QPlainTextEdit {"
        "    border: 2px solid #ddd;"
        "    padding: 2px;"
        "    font-size: 11px;"
        "    font-family: 'Microsoft YaHei', 'Consolas', monospace;"
        "}"
        "QPlainTextEdit:focus {"
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

    // 连接信号槽 - 使用scheduleUpdate而不是直接更新
    connect(textEdit, &QPlainTextEdit::textChanged, this, &CharacterCounter::scheduleUpdate);
    connect(clearBtn, &QPushButton::clicked, this, &CharacterCounter::onClearText);
    connect(copyBtn, &QPushButton::clicked, this, &CharacterCounter::onCopyText);
    connect(pasteBtn, &QPushButton::clicked, this, &CharacterCounter::onPasteText);

    // 初始化统计
    CharacterStats initialStats;
    updateStatisticsDisplay(initialStats);
}

void CharacterCounter::onTextChanged() {
    // 此函数已不再使用，由scheduleUpdate替代
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

void CharacterCounter::scheduleUpdate() {
    // 重启定时器，实现防抖效果
    updateTimer->start();
}

void CharacterCounter::performUpdate() {
    // 如果正在计算，则跳过
    if (isCalculating.load()) {
        // 重新调度
        updateTimer->start();
        return;
    }

    // 获取文本
    QString text = textEdit->toPlainText();

    // 标记正在计算
    isCalculating.store(true);

    // 在后台线程异步计算统计信息
    QFuture<CharacterStats> future = QtConcurrent::run([text]() {
        return CharacterCounter::calculateStatistics(text);
    });

    statsWatcher->setFuture(future);
}

void CharacterCounter::onStatsCalculated() {
    // 标记计算完成
    isCalculating.store(false);

    // 获取结果并更新显示
    CharacterStats stats = statsWatcher->result();
    updateStatisticsDisplay(stats);
}

void CharacterCounter::updateStatisticsDisplay(const CharacterStats& stats) {
    totalCharsValue->setText(QString::number(stats.totalChars));
    chineseCharsValue->setText(QString::number(stats.chineseChars));
    englishCharsValue->setText(QString::number(stats.englishChars));
    digitsValue->setText(QString::number(stats.digits));
    punctuationValue->setText(QString::number(stats.punctuation));
    whitespaceValue->setText(QString::number(stats.whitespace));
    specialCharsValue->setText(QString::number(stats.specialChars));
    linesValue->setText(QString::number(stats.lines));
    wordsValue->setText(QString::number(stats.words));
    bytesValue->setText(QString::number(stats.bytes));
}

// 优化后的统计函数：一次遍历完成所有统计
CharacterStats CharacterCounter::calculateStatistics(const QString& text) {
    CharacterStats stats;

    stats.totalChars = text.length();
    stats.bytes = text.toUtf8().size();

    // 行数统计
    if (text.isEmpty()) {
        stats.lines = 0;
    } else {
        stats.lines = text.count('\n') + 1;
    }

    // 单次遍历完成所有字符分类统计
    for (const QChar& ch : text) {
        ushort unicode = ch.unicode();

        // 中文字符
        if ((unicode >= 0x4E00 && unicode <= 0x9FFF) ||  // CJK统一汉字
            (unicode >= 0x3400 && unicode <= 0x4DBF) ||  // CJK扩展A
            (unicode >= 0x20000 && unicode <= 0x2A6DF) || // CJK扩展B
            (unicode >= 0x2A700 && unicode <= 0x2B73F) || // CJK扩展C
            (unicode >= 0x2B740 && unicode <= 0x2B81F) || // CJK扩展D
            (unicode >= 0x2B820 && unicode <= 0x2CEAF) || // CJK扩展E
            (unicode >= 0x3000 && unicode <= 0x303F) ||  // CJK符号和标点
            (unicode >= 0xFF00 && unicode <= 0xFFEF)) {  // 全角字符
            stats.chineseChars++;
            stats.words++; // 每个中文字符算一个词
        }
        // 英文字符
        else if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) {
            stats.englishChars++;
        }
        // 数字
        else if (ch.isDigit()) {
            stats.digits++;
        }
        // 空白字符
        else if (ch.isSpace()) {
            stats.whitespace++;
        }
        // 标点符号
        else if (ch.isPunct()) {
            stats.punctuation++;
        }
        // 特殊字符
        else if (!ch.isLetterOrNumber() && !ch.isSpace() && !ch.isPunct()) {
            stats.specialChars++;
        }
    }

    // 英文单词统计（使用正则表达式）
    QRegularExpression englishWordRegex(R"([a-zA-Z]+(?:'[a-zA-Z]+)?)");
    QRegularExpressionMatchIterator matches = englishWordRegex.globalMatch(text);

    while (matches.hasNext()) {
        matches.next();
        stats.words++;
    }

    return stats;
}