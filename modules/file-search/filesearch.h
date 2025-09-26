#ifndef FILESEARCH_H
#define FILESEARCH_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QTreeView>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QProgressBar>
#include <QSplitter>
#include <QGroupBox>
#include <QSpinBox>
#include <QDateEdit>
#include <QFileInfo>
#include <QDir>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QHeaderView>
#include <QContextMenuEvent>
#include <QMenu>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <QFileIconProvider>
#include <QMimeDatabase>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <QSortFilterProxyModel>
#include <QFormLayout>

#include "../../common/dynamicobjectbase.h"

// 搜索结果数据结构
struct SearchResultItem {
    QString fileName;
    QString filePath;
    QString fileExtension;
    qint64 fileSize;
    QDateTime lastModified;
    bool isDirectory;
    QIcon fileIcon;

    SearchResultItem() : fileSize(0), isDirectory(false) {}
};

// 搜索配置
struct SearchConfig {
    QString searchText;          // 搜索文本
    QStringList searchPaths;     // 搜索路径
    bool useRegex;               // 使用正则表达式
    bool caseSensitive;          // 区分大小写
    bool wholeWordOnly;          // 全词匹配
    bool includeHidden;          // 包含隐藏文件
    bool searchInContent;        // 搜索文件内容
    QStringList fileTypes;       // 文件类型过滤
    qint64 minFileSize;          // 最小文件大小(字节)
    qint64 maxFileSize;          // 最大文件大小(字节)
    QDate dateFrom;              // 修改日期起始
    QDate dateTo;                // 修改日期结束
    int maxResults;              // 最大结果数量

    SearchConfig() : useRegex(false), caseSensitive(false), wholeWordOnly(false),
                    includeHidden(false), searchInContent(false),
                    minFileSize(0), maxFileSize(0), maxResults(10000) {}
};

// 自定义排序代理模型
class FileSearchSortProxyModel : public QSortFilterProxyModel {
    Q_OBJECT

public:
    explicit FileSearchSortProxyModel(QObject *parent = nullptr);

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
};

// 搜索工作线程
class FileSearchWorker : public QObject {
    Q_OBJECT

public:
    explicit FileSearchWorker(const SearchConfig &config);

public slots:
    void startSearch();
    void stopSearch();

signals:
    void searchProgress(int current, int total);
    void resultFound(const SearchResultItem &item);
    void searchFinished(int totalResults, qint64 elapsedMs);
    void searchError(const QString &error);

private:
    SearchConfig m_config;
    QElapsedTimer m_timer;
    bool m_stopped;
    QMutex m_stopMutex;

    void searchInDirectory(const QString &dirPath, int &foundCount);
    bool matchesSearchCriteria(const QFileInfo &fileInfo, const SearchResultItem &item);
    bool matchesFileName(const QString &fileName);
    bool matchesFileSize(qint64 fileSize);
    bool matchesModifiedDate(const QDateTime &modified);
    bool matchesFileType(const QString &extension);
    bool searchInFileContent(const QString &filePath);
    QIcon getFileIcon(const QFileInfo &fileInfo);
};

class FileSearch : public QWidget, public DynamicObjectBase {
    Q_OBJECT

public:
    explicit FileSearch();
    ~FileSearch();

private slots:
    void onSearchTextChanged();
    void onStartSearch();
    void onStopSearch();
    void onClearResults();
    void onAddSearchPath();
    void onRemoveSearchPath();
    void onSearchProgress(int current, int total);
    void onResultFound(const SearchResultItem &item);
    void onSearchFinished(int totalResults, qint64 elapsedMs);
    void onSearchError(const QString &error);
    void onResultDoubleClicked(const QModelIndex &index);
    void onResultContextMenu(const QPoint &pos);
    void onCopyPath();
    void onCopyFileName();
    void onOpenFile();
    void onOpenFileLocation();
    void onDeleteFile();
    void onExportResults();
    void onImportConfig();
    void onExportConfig();

private:
    // UI组件
    QVBoxLayout *m_mainLayout;
    QSplitter *m_mainSplitter;

    // 搜索配置区域
    QGroupBox *m_searchConfigGroup;
    QLineEdit *m_searchLineEdit;
    QComboBox *m_searchPathCombo;
    QPushButton *m_addPathButton;
    QPushButton *m_removePathButton;
    QPushButton *m_startSearchButton;
    QPushButton *m_stopSearchButton;
    QPushButton *m_clearResultsButton;

    // 搜索选项
    QGroupBox *m_searchOptionsGroup;
    QCheckBox *m_useRegexCheckBox;
    QCheckBox *m_caseSensitiveCheckBox;
    QCheckBox *m_wholeWordCheckBox;
    QCheckBox *m_includeHiddenCheckBox;
    QCheckBox *m_searchContentCheckBox;

    // 过滤选项
    QGroupBox *m_filterOptionsGroup;
    QLineEdit *m_fileTypesLineEdit;
    QSpinBox *m_minSizeSpinBox;
    QSpinBox *m_maxSizeSpinBox;
    QDateEdit *m_dateFromEdit;
    QDateEdit *m_dateToEdit;
    QSpinBox *m_maxResultsSpinBox;

    // 结果显示区域
    QGroupBox *m_resultsGroup;
    QTreeView *m_resultsTreeView;
    QStandardItemModel *m_resultsModel;
    FileSearchSortProxyModel *m_proxyModel;
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;

    // 工作线程
    QThread *m_searchThread;
    FileSearchWorker *m_searchWorker;

    // 右键菜单
    QMenu *m_contextMenu;
    QAction *m_copyPathAction;
    QAction *m_copyFileNameAction;
    QAction *m_openFileAction;
    QAction *m_openLocationAction;
    QAction *m_deleteFileAction;

    // 状态
    bool m_isSearching;
    QModelIndex m_contextMenuIndex;
    QFileIconProvider m_iconProvider;
    QMimeDatabase m_mimeDatabase;

    void setupUI();
    void setupSearchConfigArea();
    void setupSearchOptionsArea();
    void setupFilterOptionsArea();
    void setupResultsArea();
    void setupContextMenu();
    void applyStyles();
    void connectSignals();

    SearchConfig getSearchConfig() const;
    QString formatFileSize(qint64 bytes) const;
    QString formatElapsedTime(qint64 milliseconds) const;
    void updateSearchButtonsState();
    void showMessage(const QString &message, bool isError = false);
    void saveSettings();
    void loadSettings();
};

#endif // FILESEARCH_H