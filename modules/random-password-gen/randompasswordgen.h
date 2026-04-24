#ifndef RANDOMPASSWORDGEN_H
#define RANDOMPASSWORDGEN_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QCheckBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QSlider>
#include <QProgressBar>
#include <QComboBox>
#include <QListWidget>
#include <QClipboard>
#include <QApplication>
#include <QMessageBox>
#include <QTimer>
#include <QRandomGenerator>

#include "../../common/dynamicobjectbase.h"

class RandomPasswordGen : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit RandomPasswordGen();
    ~RandomPasswordGen();

public slots:
    void onGenerateClicked();
    void onCopyClicked();
    void onClearClicked();
    void onLengthChanged();
    void onCharsetChanged();
    void onPasswordCountChanged();
    void onPasswordItemClicked();
    void updatePasswordStrength();

private:
    void setupUI();
    void setupCharsetOptions();
    void setupPasswordSettings();
    void setupPasswordDisplay();
    void setupToolbar();

    QString generatePassword(int length);
    QString getCharacterSet();
    int calculatePasswordStrength(const QString& password);
    QString getStrengthText(int strength);
    QColor getStrengthColor(int strength);
    void updateStatus(const QString& message, bool isError = false);

    // UI组件
    QVBoxLayout* mainLayout;

    // 工具栏
    QWidget* toolbarWidget;
    QHBoxLayout* toolbarLayout;
    QPushButton* generateBtn;
    QPushButton* copyBtn;
    QPushButton* clearBtn;
    QLabel* statusLabel;

    // 左侧设置面板
    QWidget* settingsWidget;
    QVBoxLayout* settingsLayout;

    // 字符集选项
    QCheckBox* includeDigits;
    QCheckBox* includeLowercase;
    QCheckBox* includeUppercase;
    QCheckBox* includeSpecialChars;
    QCheckBox* includeCustomChars;
    QLineEdit* customCharsEdit;

    // 密码设置
    QLabel* lengthLabel;
    QSlider* lengthSlider;
    QSpinBox* lengthSpinBox;
    QLabel* countLabel;
    QSpinBox* countSpinBox;

    // 高级选项
    QCheckBox* avoidAmbiguous;
    QCheckBox* requireAllTypes;
    QCheckBox* noRepeatingChars;

    // 密码强度显示
    QProgressBar* strengthBar;
    QLabel* strengthLabel;
    QLabel* strengthDescription;

    // 右侧密码显示
    QWidget* displayWidget;
    QVBoxLayout* displayLayout;
    QListWidget* passwordList;

    // 预览区域
    QLabel* previewLabel;
    QPlainTextEdit* previewEdit;

    // 状态
    QStringList generatedPasswords;
    QString currentCharset;
};

#endif // RANDOMPASSWORDGEN_H
