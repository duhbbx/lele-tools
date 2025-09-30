#ifndef CHARACTERCOUNTER_H
#define CHARACTERCOUNTER_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPlainTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QSplitter>
#include <QFrame>
#include <QTimer>
#include <QFuture>
#include <QFutureWatcher>
#include <atomic>
#include "../../common/dynamicobjectbase.h"

// 统计结果结构体
struct CharacterStats {
    int totalChars = 0;
    int chineseChars = 0;
    int englishChars = 0;
    int digits = 0;
    int punctuation = 0;
    int whitespace = 0;
    int specialChars = 0;
    int lines = 0;
    int words = 0;
    int bytes = 0;
};

class CharacterCounter final : public QWidget, public DynamicObjectBase {
    Q_OBJECT

public:
    explicit CharacterCounter();
    ~CharacterCounter() override;

private slots:
    void onTextChanged();
    void onClearText();
    void onCopyText();
    void onPasteText();
    void scheduleUpdate();
    void performUpdate();
    void onStatsCalculated();

private:
    void setupUI();
    void updateStatisticsDisplay(const CharacterStats& stats);

    // 优化后的统计函数：一次遍历完成所有统计
    static CharacterStats calculateStatistics(const QString& text);

    // UI 组件
    QVBoxLayout* mainLayout;
    QSplitter* mainSplitter;

    // 文本编辑区域
    QWidget* editorWidget;
    QVBoxLayout* editorLayout;
    QLabel* editorTitle;
    QPlainTextEdit* textEdit;
    QHBoxLayout* buttonLayout;
    QPushButton* clearBtn;
    QPushButton* copyBtn;
    QPushButton* pasteBtn;

    // 统计信息区域
    QWidget* statsWidget;
    QVBoxLayout* statsLayout;
    QLabel* statsTitle;
    QGridLayout* statsGridLayout;

    // 统计标签
    QLabel* totalCharsLabel;
    QLabel* totalCharsValue;
    QLabel* chineseCharsLabel;
    QLabel* chineseCharsValue;
    QLabel* englishCharsLabel;
    QLabel* englishCharsValue;
    QLabel* digitsLabel;
    QLabel* digitsValue;
    QLabel* punctuationLabel;
    QLabel* punctuationValue;
    QLabel* whitespaceLabel;
    QLabel* whitespaceValue;
    QLabel* specialCharsLabel;
    QLabel* specialCharsValue;
    QLabel* linesLabel;
    QLabel* linesValue;
    QLabel* wordsLabel;
    QLabel* wordsValue;
    QLabel* bytesLabel;
    QLabel* bytesValue;

    // 性能优化相关
    QTimer* updateTimer;  // 防抖定时器
    QFutureWatcher<CharacterStats>* statsWatcher;  // 异步统计监视器
    std::atomic<bool> isCalculating;  // 是否正在计算
};

#endif // CHARACTERCOUNTER_H