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
#include <QFileInfo>
#include <QStyledItemDelegate>

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

// 自定义表格委托，用于控制编辑器样式和大小
class HostsTableDelegate : public QStyledItemDelegate {
    Q_OBJECT
    
public:
    explicit HostsTableDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}
    
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        Q_UNUSED(option)
        Q_UNUSED(index)
        
        QLineEdit* editor = new QLineEdit(parent);
        editor->setStyleSheet(
            "QLineEdit {"
            "    padding: 2px;"
            "    font-size: 10pt;"
            "    border: 2px solid #2196f3;"
            "    border-radius: 3px;"
            "    background-color: white;"
            "    min-height: 20px;"
            "}"
        );
        return editor;
    }
    
    void setEditorData(QWidget* editor, const QModelIndex& index) const override {
        QString value = index.model()->data(index, Qt::EditRole).toString();
        QLineEdit* lineEdit = static_cast<QLineEdit*>(editor);
        lineEdit->setText(value);
        lineEdit->selectAll(); // 选中所有文本，方便编辑
    }
    
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override {
        QLineEdit* lineEdit = static_cast<QLineEdit*>(editor);
        QString value = lineEdit->text();
        model->setData(index, value, Qt::EditRole);
    }
    
    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        Q_UNUSED(index)
        QRect rect = option.rect;
        rect.adjust(2, 2, -2, -2); // 留出一些边距
        editor->setGeometry(rect);
    }
    
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.setHeight(qMax(35, size.height())); // 确保最小高度35px
        return size;
    }
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
    bool tryPowerShellElevation(const QString& tempFile);
    bool tryCmdElevation(const QString& tempFile);
    bool tryRobocopyElevation(const QString& tempFile);
    void showManualSaveDialog(const QString& tempFile);
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
    QTableWidget* hostsTable;

    // 快速添加区域
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