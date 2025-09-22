#ifndef WINDOWSSETTINGS_H
#define WINDOWSSETTINGS_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>

#include "../../common/dynamicobjectbase.h"

class WindowsSettings : public QWidget, public DynamicObjectBase {
    Q_OBJECT

public:
    explicit WindowsSettings(QWidget* parent = nullptr);
    ~WindowsSettings();

private slots:
    void onRequestAdminPrivileges();
    void onTestFeature();
    void onRunAsAdmin();
    void onRunCustomCommand();
    void onPresetCommandSelected();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void setupUI();
    void updateAdminStatus();
    bool isRunningAsAdmin();
    void executeAsAdmin(const QString& command, const QString& arguments = QString());
    void setupRunAsAdminSection();

    // 主布局
    QVBoxLayout* m_mainLayout;

    // 头部
    QLabel* m_titleLabel;
    QLabel* m_adminStatusLabel;
    QPushButton* m_adminRequestBtn;

    // 测试按钮
    QPushButton* m_testBtn;

    // RunAs 功能区域
    QGroupBox* m_runAsGroup;
    QComboBox* m_presetCombo;
    QLineEdit* m_commandInput;
    QLineEdit* m_argumentsInput;
    QPushButton* m_runAsBtn;
    QPushButton* m_customRunBtn;
    QTextEdit* m_outputText;

    // 状态
    bool m_isAdmin;
    QProcess* m_process;
};

#endif // WINDOWSSETTINGS_H