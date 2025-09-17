#include "mediamanager.h"
#include <QDebug>
#include <QMimeDatabase>
#include <QImageReader>
#include <QMovie>

REGISTER_DYNAMICOBJECT(MediaManager);

// MediaListWidget 实现
MediaListWidget::MediaListWidget(QWidget* parent) : QListWidget(parent), contextMenuItem(nullptr)
{
    // 创建右键菜单
    contextMenu = new QMenu(this);
    
    openAction = contextMenu->addAction(tr("🔍 打开文件"));
    copyPathAction = contextMenu->addAction(tr("📋 复制路径"));
    showInExplorerAction = contextMenu->addAction(tr("📁 在资源管理器中显示"));
    contextMenu->addSeparator();
    deleteAction = contextMenu->addAction(tr("🗑️ 删除文件"));
    
    connect(openAction, &QAction::triggered, this, &MediaListWidget::onOpenFile);
    connect(copyPathAction, &QAction::triggered, this, &MediaListWidget::onCopyPath);
    connect(showInExplorerAction, &QAction::triggered, this, &MediaListWidget::onShowInExplorer);
    connect(deleteAction, &QAction::triggered, this, &MediaListWidget::onDeleteFile);
}

void MediaListWidget::contextMenuEvent(QContextMenuEvent* event)
{
    contextMenuItem = itemAt(event->pos());
    if (contextMenuItem) {
        contextMenu->exec(event->globalPos());
    }
}

void MediaListWidget::onOpenFile()
{
    if (contextMenuItem) {
        QString filePath = contextMenuItem->data(Qt::UserRole + 1).toString();
        emit openFileRequested(filePath);
    }
}

void MediaListWidget::onDeleteFile()
{
    if (contextMenuItem) {
        int mediaId = contextMenuItem->data(Qt::UserRole).toInt();
        emit deleteFileRequested(mediaId);
    }
}

void MediaListWidget::onCopyPath()
{
    if (contextMenuItem) {
        QString filePath = contextMenuItem->data(Qt::UserRole + 1).toString();
        emit copyPathRequested(filePath);
    }
}

void MediaListWidget::onShowInExplorer()
{
    if (contextMenuItem) {
        QString filePath = contextMenuItem->data(Qt::UserRole + 1).toString();
        emit showInExplorerRequested(filePath);
    }
}

// MediaManager 实现
MediaManager::MediaManager() : QWidget(nullptr), DynamicObjectBase()
{
    setupUI();
    initializeDatabase();
    
    // 设置媒体存储路径
    mediaStorageBasePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/notepad_media";
    createMediaDirectory(mediaStorageBasePath);
    
    loadMediaFiles();
    updateButtonStates();
    updateStatus(tr("就绪"));
}

MediaManager::~MediaManager()
{
    // 不要删除共用数据库实例
}

void MediaManager::setupUI()
{
    mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    
    setupToolbar();
    
    mainSplitter = new QSplitter(Qt::Horizontal);
    
    setupMediaListArea();
    setupPreviewArea();
    setupStatusArea();
    
    mainSplitter->addWidget(mediaListWidget);
    mainSplitter->addWidget(previewWidget);
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 1);
    mainSplitter->setSizes({400, 400});
    
    // 设置工具栏固定高度
    toolbarGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    
    // 设置状态栏固定高度
    QWidget* statusWidget = new QWidget();
    statusWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    statusWidget->setFixedHeight(35);
    QHBoxLayout* statusWidgetLayout = new QHBoxLayout(statusWidget);
    statusWidgetLayout->setContentsMargins(8, 4, 8, 4);
    
    // 将原来的状态栏组件移到新容器中
    statusWidgetLayout->addWidget(statusLabel);
    statusWidgetLayout->addWidget(fileCountLabel);
    statusWidgetLayout->addWidget(totalSizeLabel);
    statusWidgetLayout->addStretch();
    statusWidgetLayout->addWidget(progressBar);
    
    // 布局顺序：工具栏(固定) -> 主内容(可伸缩) -> 状态栏(固定)
    mainLayout->addWidget(toolbarGroup, 0);        // 0 = 不伸缩
    mainLayout->addWidget(mainSplitter, 1);        // 1 = 可伸缩
    mainLayout->addWidget(statusWidget, 0);        // 0 = 不伸缩
}

void MediaManager::setupToolbar()
{
    toolbarGroup = new QGroupBox(tr("🎬 媒体文件管理器"));
    toolbarLayout = new QHBoxLayout(toolbarGroup);
    
    importBtn = new QPushButton(tr("📁 导入文件"));
    importBtn->setStyleSheet("QPushButton { background-color: #28a745; color: white; } QPushButton:hover { background-color: #218838; }");
    
    deleteBtn = new QPushButton(tr("🗑️ 删除选中"));
    deleteBtn->setStyleSheet("QPushButton { background-color: #dc3545; color: white; } QPushButton:hover { background-color: #c82333; }");
    deleteBtn->setEnabled(false);
    
    clearBtn = new QPushButton(tr("🧹 清空全部"));
    clearBtn->setStyleSheet("QPushButton { background-color: #fd7e14; color: white; } QPushButton:hover { background-color: #e66100; }");
    
    refreshBtn = new QPushButton(tr("🔄 刷新"));
    refreshBtn->setStyleSheet("QPushButton { background-color: #007bff; color: white; } QPushButton:hover { background-color: #0056b3; }");
    
    cleanupBtn = new QPushButton(tr("🧽 清理孤立文件"));
    cleanupBtn->setStyleSheet("QPushButton { background-color: #6f42c1; color: white; } QPushButton:hover { background-color: #5a32a3; }");
    
    // 搜索框
    searchEdit = new QLineEdit();
    searchEdit->setPlaceholderText(tr("搜索媒体文件..."));
    searchEdit->setMaximumWidth(200);
    
    // 筛选下拉框
    filterCombo = new QComboBox();
    filterCombo->addItems({tr("全部文件"), tr("图片"), tr("视频"), tr("音频"), tr("其他")});
    filterCombo->setMaximumWidth(120);
    
    connect(importBtn, &QPushButton::clicked, this, &MediaManager::onImportFiles);
    connect(deleteBtn, &QPushButton::clicked, this, &MediaManager::onDeleteSelected);
    connect(clearBtn, &QPushButton::clicked, this, &MediaManager::onClearAll);
    connect(refreshBtn, &QPushButton::clicked, this, &MediaManager::onRefreshList);
    connect(cleanupBtn, &QPushButton::clicked, this, &MediaManager::onCleanupOrphans);
    connect(searchEdit, &QLineEdit::textChanged, this, &MediaManager::onSearchTextChanged);
    connect(filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MediaManager::onFilterChanged);
    
    toolbarLayout->addWidget(importBtn);
    toolbarLayout->addWidget(deleteBtn);
    toolbarLayout->addWidget(clearBtn);
    toolbarLayout->addWidget(refreshBtn);
    toolbarLayout->addWidget(cleanupBtn);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(new QLabel(tr("搜索:")));
    toolbarLayout->addWidget(searchEdit);
    toolbarLayout->addWidget(new QLabel(tr("筛选:")));
    toolbarLayout->addWidget(filterCombo);
}

void MediaManager::setupMediaListArea()
{
    mediaListWidget = new QWidget();
    mediaListLayout = new QVBoxLayout(mediaListWidget);
    
    mediaListGroup = new QGroupBox(tr("📋 媒体文件列表"));
    QVBoxLayout* groupLayout = new QVBoxLayout(mediaListGroup);
    
    mediaList = new MediaListWidget();
    mediaList->setAlternatingRowColors(true);
    
    connect(mediaList, &QListWidget::currentRowChanged, this, &MediaManager::onMediaSelectionChanged);
    connect(mediaList, &MediaListWidget::openFileRequested, this, &MediaManager::onOpenFile);
    connect(mediaList, &MediaListWidget::deleteFileRequested, this, &MediaManager::onDeleteFile);
    connect(mediaList, &MediaListWidget::copyPathRequested, this, &MediaManager::onCopyPath);
    connect(mediaList, &MediaListWidget::showInExplorerRequested, this, &MediaManager::onShowInExplorer);
    
    groupLayout->addWidget(mediaList);
    mediaListLayout->addWidget(mediaListGroup);
}

void MediaManager::setupPreviewArea()
{
    previewWidget = new QWidget();
    previewLayout = new QVBoxLayout(previewWidget);
    
    previewGroup = new QGroupBox(tr("👁️ 预览"));
    QVBoxLayout* groupLayout = new QVBoxLayout(previewGroup);
    
    previewScrollArea = new QScrollArea();
    previewScrollArea->setAlignment(Qt::AlignCenter);
    previewScrollArea->setWidgetResizable(true);
    
    previewLabel = new QLabel();
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setMinimumSize(300, 200);
    previewLabel->setStyleSheet("border: 1px solid #ccc; background-color: #f8f9fa;");
    previewLabel->setText(tr("选择文件查看预览"));
    
    previewScrollArea->setWidget(previewLabel);
    
    infoLabel = new QLabel();
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("padding: 8px; background-color: #f8f9fa; border: 1px solid #ccc; border-radius: 4px;");
    infoLabel->setText(tr("文件信息将显示在这里"));
    
    groupLayout->addWidget(previewScrollArea, 1);
    groupLayout->addWidget(infoLabel, 0);
    
    previewLayout->addWidget(previewGroup);
}

void MediaManager::setupStatusArea()
{
    // 只创建状态栏组件，布局在 setupUI 中处理
    statusLabel = new QLabel(tr("就绪"));
    statusLabel->setStyleSheet("color: #28a745; font-weight: bold;");
    
    fileCountLabel = new QLabel(tr("文件: 0"));
    fileCountLabel->setStyleSheet("color: #6c757d;");
    
    totalSizeLabel = new QLabel(tr("总大小: 0 B"));
    totalSizeLabel->setStyleSheet("color: #6c757d;");
    
    progressBar = new QProgressBar();
    progressBar->setVisible(false);
    progressBar->setMaximumHeight(20);
}

void MediaManager::initializeDatabase()
{
    // 使用共用的默认数据库实例
    dbManager = SqliteWrapper::SqliteManager::getDefaultInstance();
    
    if (!dbManager) {
        updateStatus(tr("无法获取数据库实例"), true);
        return;
    }
    
    // 如果数据库未打开，尝试打开默认数据库
    if (!dbManager->isOpen()) {
        QString defaultDbPath = SqliteWrapper::SqliteManager::getDefaultDatabasePath();
        if (!dbManager->openDatabase(defaultDbPath)) {
            updateStatus(tr("无法打开共用数据库: %1").arg(dbManager->getLastError()), true);
            return;
        }
    }
    
    // 创建媒体文件表（如果不存在）
    QString createMediaTable = R"(
        CREATE TABLE IF NOT EXISTS media_files (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_name TEXT NOT NULL,
            relative_path TEXT NOT NULL,
            mime_type TEXT,
            file_size INTEGER,
            created DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
    
    SqliteWrapper::QueryResult result = dbManager->execute(createMediaTable);
    if (!result.success) {
        updateStatus(tr("创建媒体表失败: %1").arg(result.errorMessage), true);
        return;
    }
    
    updateStatus(tr("数据库初始化完成"));
}

void MediaManager::loadMediaFiles()
{
    mediaFiles.clear();
    
    QString query = "SELECT id, file_name, relative_path, mime_type, file_size, created FROM media_files ORDER BY created DESC";
    SqliteWrapper::QueryResult result = dbManager->select(query);
    
    if (result.success) {
        for (const QVariant& row : result.data) {
            QVariantMap rowData = row.toMap();
            MediaFileInfo mediaInfo;
            mediaInfo.id = rowData["id"].toInt();
            mediaInfo.fileName = rowData["file_name"].toString();
            mediaInfo.relativePath = rowData["relative_path"].toString();
            mediaInfo.fullPath = QString("%1/%2").arg(mediaStorageBasePath, mediaInfo.relativePath);
            mediaInfo.mimeType = rowData["mime_type"].toString();
            mediaInfo.fileSize = rowData["file_size"].toLongLong();
            mediaInfo.created = rowData["created"].toDateTime();
            mediaInfo.fileType = detectFileType(mediaInfo.fullPath);
            
            mediaFiles.append(mediaInfo);
        }
    }
    
    updateMediaList();
    updateStatistics();
}

void MediaManager::updateMediaList()
{
    mediaList->clear();
    
    QString searchText = searchEdit->text().toLower();
    QString filterType = filterCombo->currentText();
    
    for (const MediaFileInfo& mediaInfo : mediaFiles) {
        // 搜索过滤
        if (!searchText.isEmpty() && !mediaInfo.fileName.toLower().contains(searchText)) {
            continue;
        }
        
        // 类型过滤
        if (filterType != tr("全部文件")) {
            QString expectedType;
            if (filterType == tr("图片")) expectedType = "image";
            else if (filterType == tr("视频")) expectedType = "video";
            else if (filterType == tr("音频")) expectedType = "audio";
            else if (filterType == tr("其他")) expectedType = "document";
            
            if (mediaInfo.fileType != expectedType) {
                continue;
            }
        }
        
        QListWidgetItem* item = new QListWidgetItem();
        
        QString typeIcon;
        if (mediaInfo.fileType == "image") typeIcon = "🖼️";
        else if (mediaInfo.fileType == "video") typeIcon = "🎬";
        else if (mediaInfo.fileType == "audio") typeIcon = "🎵";
        else typeIcon = "📄";
        
        QString displayText = QString("%1 %2\n%3 | %4").arg(
            typeIcon,
            mediaInfo.fileName,
            formatFileSize(mediaInfo.fileSize),
            mediaInfo.created.toString("yyyy-MM-dd hh:mm")
        );
        
        item->setText(displayText);
        item->setData(Qt::UserRole, mediaInfo.id);
        item->setData(Qt::UserRole + 1, mediaInfo.fullPath);
        item->setData(Qt::UserRole + 2, mediaInfo.fileType);
        
        mediaList->addItem(item);
    }
}

void MediaManager::updateStatistics()
{
    int totalFiles = mediaFiles.size();
    qint64 totalSize = 0;
    
    for (const MediaFileInfo& mediaInfo : mediaFiles) {
        totalSize += mediaInfo.fileSize;
    }
    
    fileCountLabel->setText(tr("文件: %1").arg(totalFiles));
    totalSizeLabel->setText(tr("总大小: %1").arg(formatFileSize(totalSize)));
}

void MediaManager::onImportFiles()
{
    QStringList fileNames = QFileDialog::getOpenFileNames(this,
        tr("选择要导入的媒体文件"), "",
        tr("媒体文件 (*.png *.jpg *.jpeg *.gif *.bmp *.svg *.mp4 *.avi *.mov *.mp3 *.wav *.aac)"));
    
    if (!fileNames.isEmpty()) {
        progressBar->setVisible(true);
        progressBar->setRange(0, fileNames.size());
        
        int imported = 0;
        for (int i = 0; i < fileNames.size(); ++i) {
            QString savedPath = importMediaFile(fileNames[i]);
            if (!savedPath.isEmpty()) {
                imported++;
            }
            progressBar->setValue(i + 1);
            QApplication::processEvents();
        }
        
        progressBar->setVisible(false);
        loadMediaFiles(); // 重新加载列表
        updateStatus(tr("成功导入 %1 个文件").arg(imported));
    }
}

QString MediaManager::importMediaFile(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        return QString();
    }
    
    // 生成存储路径
    QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString fileName = uuid + "-" + fileInfo.fileName();
    
    QDateTime now = QDateTime::currentDateTime();
    QString yearMonth = now.toString("yyyy/MM");
    QString relativePath = QString("media/%1/%2").arg(yearMonth, fileName);
    QString fullPath = QString("%1/%2").arg(mediaStorageBasePath, relativePath);
    
    // 确保目录存在
    QFileInfo fullPathInfo(fullPath);
    createMediaDirectory(fullPathInfo.absolutePath());
    
    // 复制文件
    if (QFile::copy(filePath, fullPath)) {
        // 保存到数据库
        QString sql = "INSERT INTO media_files (file_name, relative_path, mime_type, file_size) VALUES (:file_name, :relative_path, :mime_type, :file_size)";
        QVariantMap params;
        params["file_name"] = fileInfo.fileName();
        params["relative_path"] = relativePath;
        params["mime_type"] = detectMimeType(filePath);
        params["file_size"] = fileInfo.size();
        
        SqliteWrapper::QueryResult result = dbManager->execute(sql, params);
        
        if (result.success && result.lastInsertId > 0) {
            return fullPath;
        } else {
            QFile::remove(fullPath);
        }
    }
    
    return QString();
}

void MediaManager::onDeleteSelected()
{
    QListWidgetItem* currentItem = mediaList->currentItem();
    if (!currentItem) {
        updateStatus(tr("请选择要删除的文件"), true);
        return;
    }
    
    int mediaId = currentItem->data(Qt::UserRole).toInt();
    onDeleteFile(mediaId);
}

void MediaManager::onDeleteFile(int mediaId)
{
    int ret = QMessageBox::question(this, tr("确认删除"),
                                   tr("确定要删除这个媒体文件吗？文件将从磁盘中彻底删除。"),
                                   QMessageBox::Yes | QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        // 找到文件信息
        MediaFileInfo* mediaInfo = nullptr;
        for (MediaFileInfo& info : mediaFiles) {
            if (info.id == mediaId) {
                mediaInfo = &info;
                break;
            }
        }
        
        if (mediaInfo) {
            // 删除物理文件
            QFile::remove(mediaInfo->fullPath);
            
            // 从数据库删除记录
            QString sql = "DELETE FROM media_files WHERE id = :id";
            QVariantMap params;
            params["id"] = mediaId;
            
            SqliteWrapper::QueryResult result = dbManager->execute(sql, params);
            
            if (result.success && result.affectedRows > 0) {
                loadMediaFiles();
                updateStatus(tr("文件已删除"));
            } else {
                updateStatus(tr("删除文件记录失败"), true);
            }
        }
    }
}

void MediaManager::onClearAll()
{
    if (mediaFiles.isEmpty()) {
        updateStatus(tr("没有文件可清空"), true);
        return;
    }
    
    int ret = QMessageBox::question(this, tr("确认清空"),
                                   tr("确定要清空所有媒体文件吗？所有文件将从磁盘中彻底删除。"),
                                   QMessageBox::Yes | QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        progressBar->setVisible(true);
        progressBar->setRange(0, mediaFiles.size());
        
        int deleted = 0;
        for (int i = 0; i < mediaFiles.size(); ++i) {
            const MediaFileInfo& mediaInfo = mediaFiles[i];
            QFile::remove(mediaInfo.fullPath);
            progressBar->setValue(i + 1);
            QApplication::processEvents();
            deleted++;
        }
        
        // 清空数据库
        SqliteWrapper::QueryResult result = dbManager->execute("DELETE FROM media_files");
        Q_UNUSED(result);
        
        progressBar->setVisible(false);
        loadMediaFiles();
        updateStatus(tr("已清空 %1 个文件").arg(deleted));
    }
}

void MediaManager::onRefreshList()
{
    loadMediaFiles();
    updateStatus(tr("列表已刷新"));
}

void MediaManager::onCleanupOrphans()
{
    cleanupOrphanedFiles();
}

void MediaManager::cleanupOrphanedFiles()
{
    updateStatus(tr("正在清理孤立文件..."));
    
    // 扫描数据库中的文件，检查物理文件是否存在
    int cleanedCount = 0;
    QList<int> orphanIds;
    
    for (const MediaFileInfo& mediaInfo : mediaFiles) {
        if (!QFile::exists(mediaInfo.fullPath)) {
            orphanIds.append(mediaInfo.id);
        }
    }
    
    // 删除孤立记录
    for (int id : orphanIds) {
        QString sql = "DELETE FROM media_files WHERE id = :id";
        QVariantMap params;
        params["id"] = id;
        
        SqliteWrapper::QueryResult result = dbManager->execute(sql, params);
        
        if (result.success && result.affectedRows > 0) {
            cleanedCount++;
        }
    }
    
    if (cleanedCount > 0) {
        loadMediaFiles();
        updateStatus(tr("已清理 %1 个孤立记录").arg(cleanedCount));
    } else {
        updateStatus(tr("没有发现孤立文件"));
    }
}

void MediaManager::onSearchTextChanged()
{
    updateMediaList();
}

void MediaManager::onFilterChanged()
{
    updateMediaList();
}

void MediaManager::onMediaSelectionChanged()
{
    QListWidgetItem* currentItem = mediaList->currentItem();
    if (currentItem) {
        int mediaId = currentItem->data(Qt::UserRole).toInt();
        
        // 找到对应的媒体信息
        for (const MediaFileInfo& mediaInfo : mediaFiles) {
            if (mediaInfo.id == mediaId) {
                updatePreview(mediaInfo);
                break;
            }
        }
        
        deleteBtn->setEnabled(true);
    } else {
        previewLabel->clear();
        previewLabel->setText(tr("选择文件查看预览"));
        infoLabel->setText(tr("文件信息将显示在这里"));
        deleteBtn->setEnabled(false);
    }
    
    updateButtonStates();
}

void MediaManager::updatePreview(const MediaFileInfo& mediaInfo)
{
    // 更新文件信息
    QString infoText = QString(
        tr("文件名: %1\n"
           "大小: %2\n"
           "类型: %3\n"
           "创建时间: %4\n"
           "路径: %5")
    ).arg(
        mediaInfo.fileName,
        formatFileSize(mediaInfo.fileSize),
        mediaInfo.fileType,
        mediaInfo.created.toString("yyyy-MM-dd hh:mm:ss"),
        mediaInfo.relativePath
    );
    
    infoLabel->setText(infoText);
    
    // 更新预览
    if (mediaInfo.fileType == "image") {
        QPixmap pixmap(mediaInfo.fullPath);
        if (!pixmap.isNull()) {
            // 缩放图片以适应预览区域
            QPixmap scaledPixmap = pixmap.scaled(400, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            previewLabel->setPixmap(scaledPixmap);
        } else {
            previewLabel->setText(tr("无法加载图片"));
        }
    } else if (mediaInfo.fileType == "video") {
        // 对于视频，显示一个占位符或缩略图
        previewLabel->setText(tr("🎬 视频文件\n点击右键选择打开"));
    } else if (mediaInfo.fileType == "audio") {
        previewLabel->setText(tr("🎵 音频文件\n点击右键选择打开"));
    } else {
        previewLabel->setText(tr("📄 文档文件\n点击右键选择打开"));
    }
}

void MediaManager::onOpenFile(const QString& filePath)
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
}

void MediaManager::onCopyPath(const QString& filePath)
{
    QApplication::clipboard()->setText(filePath);
    updateStatus(tr("文件路径已复制到剪贴板"));
}

void MediaManager::onShowInExplorer(const QString& filePath)
{
#ifdef Q_OS_WIN
    QProcess::startDetached("explorer.exe", QStringList() << "/select," << QDir::toNativeSeparators(filePath));
#elif defined(Q_OS_MAC)
    QProcess::startDetached("open", QStringList() << "-R" << filePath);
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(filePath).absolutePath()));
#endif
}

QString MediaManager::getMediaStoragePath()
{
    return mediaStorageBasePath;
}

void MediaManager::createMediaDirectory(const QString& path)
{
    QDir dir;
    if (!dir.exists(path)) {
        dir.mkpath(path);
    }
}

QString MediaManager::detectFileType(const QString& filePath)
{
    if (isImageFile(filePath)) return "image";
    if (isVideoFile(filePath)) return "video";
    if (isAudioFile(filePath)) return "audio";
    return "document";
}

QString MediaManager::detectMimeType(const QString& filePath)
{
    QMimeDatabase mimeDb;
    QMimeType mimeType = mimeDb.mimeTypeForFile(filePath);
    return mimeType.name();
}

bool MediaManager::isImageFile(const QString& filePath)
{
    QStringList imageExtensions = {"png", "jpg", "jpeg", "gif", "bmp", "svg", "webp"};
    QFileInfo fileInfo(filePath);
    return imageExtensions.contains(fileInfo.suffix().toLower());
}

bool MediaManager::isVideoFile(const QString& filePath)
{
    QStringList videoExtensions = {"mp4", "avi", "mov", "mkv", "wmv", "flv", "webm"};
    QFileInfo fileInfo(filePath);
    return videoExtensions.contains(fileInfo.suffix().toLower());
}

bool MediaManager::isAudioFile(const QString& filePath)
{
    QStringList audioExtensions = {"mp3", "wav", "aac", "ogg", "flac", "m4a"};
    QFileInfo fileInfo(filePath);
    return audioExtensions.contains(fileInfo.suffix().toLower());
}

QString MediaManager::formatFileSize(qint64 bytes)
{
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024);
    if (bytes < 1024 * 1024 * 1024) return QString("%1 MB").arg(bytes / (1024 * 1024));
    return QString("%1 GB").arg(bytes / (1024 * 1024 * 1024));
}

void MediaManager::updateButtonStates()
{
    bool hasFiles = !mediaFiles.isEmpty();
    bool hasSelection = mediaList->currentItem() != nullptr;
    
    deleteBtn->setEnabled(hasSelection);
    clearBtn->setEnabled(hasFiles);
    cleanupBtn->setEnabled(hasFiles);
}

void MediaManager::updateStatus(const QString& message, bool isError)
{
    statusLabel->setText(message);
    statusLabel->setStyleSheet(isError ? 
        "color: #dc3545; font-weight: bold;" : 
        "color: #28a745; font-weight: bold;");
}

