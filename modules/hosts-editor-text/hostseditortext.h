#ifndef SIMPLEHOSTSEDITOR_H
#define SIMPLEHOSTSEDITOR_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QProgressBar>
#include <QTimer>
#include <QMessageBox>
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QFileInfo>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#include <wininet.h>
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shell32.lib")
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif

#include "../../common/dynamicobjectbase.h"

class SimpleHostsEditor : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit SimpleHostsEditor();
    ~SimpleHostsEditor();

public slots:
    void onLoadHosts();
    void onSaveHosts();
    void onFlushDns();
    void onResetHosts();
    void onBackupHosts();

private slots:
    void onTextChanged();
    void updateStatus();

private:
    void setupUI();
    void setupToolbar();
    void setupEditorArea();
    void setupStatusArea();
    void loadHostsFile();
    bool saveHostsFile();
    bool saveWithElevation();
    void flushDnsWithAPI();
    void updateStatusMessage(const QString& message, bool isError = false);
    bool checkAdminRights();
    QString getHostsFilePath();
    void createBackup();
    
    // UI组件
    QVBoxLayout* mainLayout;
    
    // 工具栏
    QGroupBox* toolbarGroup;
    QHBoxLayout* toolbarLayout;
    QPushButton* loadBtn;
    QPushButton* saveBtn;
    QPushButton* flushDnsBtn;
    QPushButton* resetBtn;
    QPushButton* backupBtn;
    
    // 编辑区域
    QGroupBox* editorGroup;
    QVBoxLayout* editorLayout;
    QTextEdit* hostsTextEdit;
    QLabel* infoLabel;
    
    // 状态区域
    QHBoxLayout* statusLayout;
    QLabel* statusLabel;
    QLabel* adminStatusLabel;
    QLabel* lineCountLabel;
    QProgressBar* progressBar;
    
    // 数据和状态
    QString hostsFilePath;
    bool hasAdminRights;
    bool hasUnsavedChanges;
    QTimer* statusTimer;
};

#endif // SIMPLEHOSTSEDITOR_H
