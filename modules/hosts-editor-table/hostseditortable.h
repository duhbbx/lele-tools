#ifndef HOSTSEDITOR_H
#define HOSTSEDITOR_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QGroupBox>
#include <QCheckBox>
#include <QComboBox>
#include <QProgressBar>
#include <QTimer>
#include <QProcess>
#include <QFileSystemWatcher>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>
#include <QRegularExpression>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#include <wininet.h>
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shell32.lib")
#endif

#include "../../common/dynamicobjectbase.h"

struct HostEntry {
    QString ip;
    QString hostname;
    QString comment;
    bool enabled;
    
    HostEntry() : enabled(true) {}
    HostEntry(const QString& ip, const QString& hostname, const QString& comment = "", bool enabled = true)
        : ip(ip), hostname(hostname), comment(comment), enabled(enabled) {}
};

class HostsEditor : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit HostsEditor();
    ~HostsEditor();

public slots:
    void onLoadHosts();
    void onSaveHosts();
    void onAddEntry();
    void onRemoveEntry();
    void onEditEntry();
    void onToggleEntry();
    void onClearAll();
    void onBackupHosts();
    void onRestoreHosts();
    void onFlushDns();
    void onImportHosts();
    void onExportHosts();
    void onSearchHosts();
    void onQuickAdd();

private slots:
    void onTableItemChanged(QTableWidgetItem* item);
    void onTableSelectionChanged();
    void onFileChanged(const QString& path);
    void onDnsFlushFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void setupUI();
    void setupToolbar();
    void setupTableArea();
    void setupQuickAddArea();
    void setupStatusArea();
    void loadHostsFile();
    void saveHostsFile();
    void parseHostsContent(const QString& content);
    void parseHostsLine(const QString& line);
    QString generateHostsContent();
    void updateTable();
    void updateStatus(const QString& message, bool isError = false);
    void updateButtonStates();
    bool checkAdminRights();
    bool requestAdminRights();
    bool saveWithElevation();
    void flushDnsWithAPI();
    QString getHostsFilePath();
    void addHostEntry(const QString& ip, const QString& hostname, const QString& comment = "", bool enabled = true);
    void removeSelectedEntries();
    void toggleSelectedEntries();
    bool isValidIP(const QString& ip);
    bool isValidHostname(const QString& hostname);
    void showAdminWarning();
    
    // UI组件
    QVBoxLayout* mainLayout;
    QSplitter* mainSplitter;
    
    // 工具栏
    QGroupBox* toolbarGroup;
    QHBoxLayout* toolbarLayout;
    QPushButton* loadBtn;
    QPushButton* saveBtn;
    QPushButton* addBtn;
    QPushButton* removeBtn;
    QPushButton* editBtn;
    QPushButton* toggleBtn;
    QPushButton* clearBtn;
    QPushButton* backupBtn;
    QPushButton* restoreBtn;
    QPushButton* flushDnsBtn;
    QPushButton* importBtn;
    QPushButton* exportBtn;
    
    // 搜索区域
    QHBoxLayout* searchLayout;
    QLineEdit* searchEdit;
    QPushButton* searchBtn;
    QLabel* searchResultLabel;
    
    // 表格区域
    QGroupBox* tableGroup;
    QVBoxLayout* tableLayout;
    QTableWidget* hostsTable;
    
    // 快速添加区域
    QGroupBox* quickAddGroup;
    QGridLayout* quickAddLayout;
    QLineEdit* ipEdit;
    QLineEdit* hostnameEdit;
    QLineEdit* commentEdit;
    QCheckBox* enabledCheck;
    QPushButton* quickAddBtn;
    QComboBox* commonHostsCombo;
    
    // 状态区域
    QHBoxLayout* statusLayout;
    QLabel* statusLabel;
    QLabel* entriesCountLabel;
    QLabel* adminStatusLabel;
    QProgressBar* progressBar;
    
    // 数据和状态
    QList<HostEntry> hostEntries;
    QFileSystemWatcher* fileWatcher;
    QProcess* dnsFlushProcess;
    QString hostsFilePath;
    bool hasAdminRights;
    bool isLoading;
    bool hasUnsavedChanges;
};

#endif // HOSTSEDITOR_H