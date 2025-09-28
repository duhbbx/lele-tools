#ifndef CHARACTERCOUNTER_H
#define CHARACTERCOUNTER_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QSplitter>
#include <QFrame>
#include "../../common/dynamicobjectbase.h"

class CharacterCounter final : public QWidget, public DynamicObjectBase {
    Q_OBJECT

public:
    explicit CharacterCounter();
    ~CharacterCounter() override = default;

private slots:
    void onTextChanged();
    void onClearText();
    void onCopyText();
    void onPasteText();

private:
    void setupUI();
    void updateStatistics();

    // 字符统计相关函数
    int countTotalCharacters(const QString& text);
    int countChineseCharacters(const QString& text);
    int countEnglishCharacters(const QString& text);
    int countDigits(const QString& text);
    int countPunctuation(const QString& text);
    int countWhitespace(const QString& text);
    int countSpecialCharacters(const QString& text);
    int countLines(const QString& text);
    int countWords(const QString& text);
    int countBytes(const QString& text);

    // UI 组件
    QVBoxLayout* mainLayout;
    QSplitter* mainSplitter;

    // 文本编辑区域
    QWidget* editorWidget;
    QVBoxLayout* editorLayout;
    QLabel* editorTitle;
    QTextEdit* textEdit;
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
};

#endif // CHARACTERCOUNTER_H