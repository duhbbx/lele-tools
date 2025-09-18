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

#include "../../common/dynamicobjectbase.h"

class WindowsSettings : public QWidget, public DynamicObjectBase {
    Q_OBJECT

public:
    explicit WindowsSettings(QWidget* parent = nullptr);
    ~WindowsSettings();

private slots:
    void onRequestAdminPrivileges();
    void onTestFeature();

private:
    void setupUI();
    void updateAdminStatus();
    bool isRunningAsAdmin();

    // 主布局
    QVBoxLayout* m_mainLayout;

    // 头部
    QLabel* m_titleLabel;
    QLabel* m_adminStatusLabel;
    QPushButton* m_adminRequestBtn;

    // 测试按钮
    QPushButton* m_testBtn;

    // 状态
    bool m_isAdmin;
};

#endif // WINDOWSSETTINGS_H