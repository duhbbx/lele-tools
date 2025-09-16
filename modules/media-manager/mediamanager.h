#ifndef MEDIAMANAGER_H
#define MEDIAMANAGER_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QLineEdit>
#include <QComboBox>
#include <QProgressBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QTimer>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QUrl>
#include <QUuid>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QPixmap>
#include <QMovie>
#include <QScrollArea>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QProcess>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <QListWidgetItem>
#include <QSqlQuery>
#include <QSqlError>

#include "../../common/dynamicobjectbase.h"
#include "../../common/sqlite/SqliteManager.h"

struct MediaFileInfo {
    int id;
    QString fileName;
    QString relativePath;
    QString fullPath;
    QString mimeType;
    qint64 fileSize;
    QDateTime created;
    QString fileType; // image, video, audio, document
    
    MediaFileInfo() : id(-1), fileSize(0) {}
    MediaFileInfo(int _id, const QString& _fileName, const QString& _relativePath,
                  const QString& _fullPath, const QString& _mimeType, qint64 _fileSize,
                  const QDateTime& _created, const QString& _fileType)
        : id(_id), fileName(_fileName), relativePath(_relativePath), fullPath(_fullPath),
          mimeType(_mimeType), fileSize(_fileSize), created(_created), fileType(_fileType) {}
};

class MediaListWidget : public QListWidget
{
    Q_OBJECT

public:
    explicit MediaListWidget(QWidget* parent = nullptr);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

signals:
    void openFileRequested(const QString& filePath);
    void deleteFileRequested(int mediaId);
    void copyPathRequested(const QString& filePath);
    void showInExplorerRequested(const QString& filePath);

private slots:
    void onOpenFile();
    void onDeleteFile();
    void onCopyPath();
    void onShowInExplorer();

private:
    QMenu* contextMenu;
    QAction* openAction;
    QAction* deleteAction;
    QAction* copyPathAction;
    QAction* showInExplorerAction;
    QListWidgetItem* contextMenuItem;
};

class MediaManager : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit MediaManager();
    ~MediaManager();

private slots:
    void onImportFiles();
    void onDeleteSelected();
    void onClearAll();
    void onRefreshList();
    void onSearchTextChanged();
    void onFilterChanged();
    void onMediaSelectionChanged();
    void onOpenFile(const QString& filePath);
    void onDeleteFile(int mediaId);
    void onCopyPath(const QString& filePath);
    void onShowInExplorer(const QString& filePath);
    void onCleanupOrphans();

private:
    void setupUI();
    void setupToolbar();
    void setupMediaListArea();
    void setupPreviewArea();
    void setupStatusArea();
    void initializeDatabase();
    void loadMediaFiles();
    void updateMediaList();
    void updatePreview(const MediaFileInfo& mediaInfo);
    void updateButtonStates();
    void updateStatus(const QString& message, bool isError = false);
    void updateStatistics();
    QString importMediaFile(const QString& filePath);
    QString getMediaStoragePath();
    void createMediaDirectory(const QString& path);
    QString detectFileType(const QString& filePath);
    QString detectMimeType(const QString& filePath);
    bool isImageFile(const QString& filePath);
    bool isVideoFile(const QString& filePath);
    bool isAudioFile(const QString& filePath);
    QString formatFileSize(qint64 bytes);
    void cleanupOrphanedFiles();
    QPixmap createVideoThumbnail(const QString& videoPath);
    
    // UI 组件
    QVBoxLayout* mainLayout;
    QSplitter* mainSplitter;
    
    // 工具栏
    QGroupBox* toolbarGroup;
    QHBoxLayout* toolbarLayout;
    QPushButton* importBtn;
    QPushButton* deleteBtn;
    QPushButton* clearBtn;
    QPushButton* refreshBtn;
    QPushButton* cleanupBtn;
    
    // 搜索和筛选
    QLineEdit* searchEdit;
    QComboBox* filterCombo;
    
    // 媒体列表区域
    QWidget* mediaListWidget;
    QVBoxLayout* mediaListLayout;
    QGroupBox* mediaListGroup;
    MediaListWidget* mediaList;
    
    // 预览区域
    QWidget* previewWidget;
    QVBoxLayout* previewLayout;
    QGroupBox* previewGroup;
    QScrollArea* previewScrollArea;
    QLabel* previewLabel;
    QLabel* infoLabel;
    
    // 状态栏
    QLabel* statusLabel;
    QLabel* fileCountLabel;
    QLabel* totalSizeLabel;
    QProgressBar* progressBar;
    
    // 数据相关
    SqliteWrapper::SqliteManager* dbManager;
    QList<MediaFileInfo> mediaFiles;
    QString mediaStorageBasePath;
};

#endif // MEDIAMANAGER_H
