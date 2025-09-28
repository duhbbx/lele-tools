#include "filesearch.h"

// 注册到动态对象工厂
REGISTER_DYNAMICOBJECT(FileSearch);
#include <QApplication>
#include <QFileDialog>
#include <QStandardPaths>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>
#include <QProcess>
#include <QFormLayout>

// FileSearchSortProxyModel 实现
FileSearchSortProxyModel::FileSearchSortProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent) {
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

bool FileSearchSortProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const {
    QStandardItemModel *model = qobject_cast<QStandardItemModel*>(sourceModel());
    if (!model) return QSortFilterProxyModel::lessThan(left, right);

    int column = left.column();

    // 文件大小列按数值排序
    if (column == 2) {
        QVariant leftData = model->data(left, Qt::UserRole);
        QVariant rightData = model->data(right, Qt::UserRole);
        if (leftData.isValid() && rightData.isValid()) {
            return leftData.toLongLong() < rightData.toLongLong();
        }
    }

    // 修改时间列按日期排序
    if (column == 3) {
        QVariant leftData = model->data(left, Qt::UserRole);
        QVariant rightData = model->data(right, Qt::UserRole);
        if (leftData.isValid() && rightData.isValid()) {
            return leftData.toDateTime() < rightData.toDateTime();
        }
    }

    return QSortFilterProxyModel::lessThan(left, right);
}

// FileSearchWorker 实现
FileSearchWorker::FileSearchWorker(const SearchConfig &config)
    : m_config(config), m_stopped(false) {}

void FileSearchWorker::startSearch() {
    m_timer.start();
    m_stopped = false;

    try {
        int foundCount = 0;

        for (const QString &searchPath : m_config.searchPaths) {
            if (m_stopped) break;

            QDir dir(searchPath);
            if (!dir.exists()) {
                emit searchError(tr("搜索路径不存在: %1").arg(searchPath));
                continue;
            }

            searchInDirectory(searchPath, foundCount);

            if (foundCount >= m_config.maxResults) {
                break;
            }
        }

        qint64 elapsed = m_timer.elapsed();
        emit searchFinished(foundCount, elapsed);

    } catch (const std::exception &e) {
        emit searchError(QString::fromLocal8Bit(e.what()));
    }
}

void FileSearchWorker::stopSearch() {
    QMutexLocker locker(&m_stopMutex);
    m_stopped = true;
}

void FileSearchWorker::searchInDirectory(const QString &dirPath, int &foundCount) {
    if (foundCount >= m_config.maxResults) return;

    QMutexLocker locker(&m_stopMutex);
    if (m_stopped) return;
    locker.unlock();

    QDir dir(dirPath);
    QDir::Filters filters = QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot;

    if (m_config.includeHidden) {
        filters |= QDir::Hidden;
    }

    QFileInfoList entries = dir.entryInfoList(filters, QDir::Name);

    for (const QFileInfo &fileInfo : entries) {
        locker.relock();
        if (m_stopped) return;
        locker.unlock();

        if (foundCount >= m_config.maxResults) break;

        SearchResultItem item;
        item.fileName = fileInfo.fileName();
        item.filePath = fileInfo.absoluteFilePath();
        item.fileExtension = fileInfo.suffix().toLower();
        item.fileSize = fileInfo.size();
        item.lastModified = fileInfo.lastModified();
        item.isDirectory = fileInfo.isDir();
        item.fileIcon = getFileIcon(fileInfo);

        if (matchesSearchCriteria(fileInfo, item)) {
            emit resultFound(item);
            foundCount++;
        }

        // 递归搜索子目录
        if (fileInfo.isDir()) {
            searchInDirectory(fileInfo.absoluteFilePath(), foundCount);
        }

        // 更新进度（每处理100个文件更新一次）
        if (foundCount % 100 == 0) {
            emit searchProgress(foundCount, m_config.maxResults);
        }
    }
}

bool FileSearchWorker::matchesSearchCriteria(const QFileInfo &fileInfo, const SearchResultItem &item) {
    // 文件名匹配
    if (!matchesFileName(item.fileName)) {
        return false;
    }

    // 文件大小过滤
    if (!item.isDirectory && !matchesFileSize(item.fileSize)) {
        return false;
    }

    // 修改时间过滤
    if (!matchesModifiedDate(item.lastModified)) {
        return false;
    }

    // 文件类型过滤
    if (!item.isDirectory && !matchesFileType(item.fileExtension)) {
        return false;
    }

    // 文件内容搜索
    if (m_config.searchInContent && !item.isDirectory) {
        if (!searchInFileContent(item.filePath)) {
            return false;
        }
    }

    return true;
}

bool FileSearchWorker::matchesFileName(const QString &fileName) {
    if (m_config.searchText.isEmpty()) return true;

    QString searchText = m_config.searchText;
    QString targetText = fileName;

    if (!m_config.caseSensitive) {
        searchText = searchText.toLower();
        targetText = targetText.toLower();
    }

    if (m_config.useRegex) {
        QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
        if (!m_config.caseSensitive) {
            options |= QRegularExpression::CaseInsensitiveOption;
        }

        QRegularExpression regex(searchText, options);
        return regex.match(targetText).hasMatch();
    } else {
        if (m_config.wholeWordOnly) {
            QStringList words = targetText.split(QRegularExpression("\\W+"), Qt::SkipEmptyParts);
            return words.contains(searchText, m_config.caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
        } else {
            return targetText.contains(searchText, m_config.caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
        }
    }
}

bool FileSearchWorker::matchesFileSize(qint64 fileSize) {
    if (m_config.minFileSize > 0 && fileSize < m_config.minFileSize) {
        return false;
    }

    if (m_config.maxFileSize > 0 && fileSize > m_config.maxFileSize) {
        return false;
    }

    return true;
}

bool FileSearchWorker::matchesModifiedDate(const QDateTime &modified) {
    if (m_config.dateFrom.isValid()) {
        if (modified.date() < m_config.dateFrom) {
            return false;
        }
    }

    if (m_config.dateTo.isValid()) {
        if (modified.date() > m_config.dateTo) {
            return false;
        }
    }

    return true;
}

bool FileSearchWorker::matchesFileType(const QString &extension) {
    if (m_config.fileTypes.isEmpty()) return true;

    return m_config.fileTypes.contains(extension, Qt::CaseInsensitive);
}

bool FileSearchWorker::searchInFileContent(const QString &filePath) {
    // 简单的文本文件内容搜索实现
    // 你可以在这里实现更复杂的文件内容搜索逻辑
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    // 限制文件大小，避免读取过大的文件
    if (file.size() > 10 * 1024 * 1024) { // 10MB
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    QString content = stream.readAll();
    file.close();

    return matchesFileName(content); // 复用文件名匹配逻辑
}

QIcon FileSearchWorker::getFileIcon(const QFileInfo &fileInfo) {
    static QFileIconProvider iconProvider;
    return iconProvider.icon(fileInfo);
}

// FileSearch 主类实现
FileSearch::FileSearch()
    : m_searchThread(nullptr), m_searchWorker(nullptr), m_isSearching(false) {
    setupUI();
    connectSignals();
    applyStyles();
    loadSettings();
    updateSearchButtonsState();
}

FileSearch::~FileSearch() {
    saveSettings();

    if (m_searchThread && m_searchThread->isRunning()) {
        if (m_searchWorker) {
            m_searchWorker->stopSearch();
        }
        m_searchThread->quit();
        m_searchThread->wait(3000);
    }
}

void FileSearch::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainSplitter = new QSplitter(Qt::Vertical, this);

    setupSearchConfigArea();
    setupSearchOptionsArea();
    setupFilterOptionsArea();
    setupResultsArea();
    setupContextMenu();

    // 布局配置区域
    QWidget *configWidget = new QWidget;
    QVBoxLayout *configLayout = new QVBoxLayout(configWidget);
    configLayout->addWidget(m_searchConfigGroup);

    QHBoxLayout *optionsLayout = new QHBoxLayout;
    optionsLayout->addWidget(m_searchOptionsGroup);
    optionsLayout->addWidget(m_filterOptionsGroup);
    configLayout->addLayout(optionsLayout);

    m_mainSplitter->addWidget(configWidget);
    m_mainSplitter->addWidget(m_resultsGroup);
    m_mainSplitter->setSizes({300, 500});

    m_mainLayout->addWidget(m_mainSplitter);
}

void FileSearch::setupSearchConfigArea() {
    m_searchConfigGroup = new QGroupBox(tr("搜索配置"));
    QVBoxLayout *layout = new QVBoxLayout(m_searchConfigGroup);

    // 搜索文本输入
    QHBoxLayout *searchLayout = new QHBoxLayout;
    QLabel *searchLabel = new QLabel(tr("搜索内容:"));
    m_searchLineEdit = new QLineEdit;
    m_searchLineEdit->setPlaceholderText(tr("输入要搜索的文件名或内容..."));
    searchLayout->addWidget(searchLabel);
    searchLayout->addWidget(m_searchLineEdit);
    layout->addLayout(searchLayout);

    // 搜索路径
    QHBoxLayout *pathLayout = new QHBoxLayout;
    QLabel *pathLabel = new QLabel(tr("搜索路径:"));
    m_searchPathCombo = new QComboBox;
    m_searchPathCombo->setEditable(true);
    m_searchPathCombo->setCurrentText(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
    m_addPathButton = new QPushButton(tr("添加路径"));
    m_removePathButton = new QPushButton(tr("移除路径"));

    pathLayout->addWidget(pathLabel);
    pathLayout->addWidget(m_searchPathCombo, 1);
    pathLayout->addWidget(m_addPathButton);
    pathLayout->addWidget(m_removePathButton);
    layout->addLayout(pathLayout);

    // 搜索控制按钮
    QHBoxLayout *controlLayout = new QHBoxLayout;
    m_startSearchButton = new QPushButton(tr("开始搜索"));
    m_stopSearchButton = new QPushButton(tr("停止搜索"));
    m_clearResultsButton = new QPushButton(tr("清空结果"));

    controlLayout->addWidget(m_startSearchButton);
    controlLayout->addWidget(m_stopSearchButton);
    controlLayout->addWidget(m_clearResultsButton);
    controlLayout->addStretch();
    layout->addLayout(controlLayout);
}

void FileSearch::setupSearchOptionsArea() {
    m_searchOptionsGroup = new QGroupBox(tr("搜索选项"));
    QVBoxLayout *layout = new QVBoxLayout(m_searchOptionsGroup);

    m_useRegexCheckBox = new QCheckBox(tr("使用正则表达式"));
    m_caseSensitiveCheckBox = new QCheckBox(tr("区分大小写"));
    m_wholeWordCheckBox = new QCheckBox(tr("全词匹配"));
    m_includeHiddenCheckBox = new QCheckBox(tr("包含隐藏文件"));
    m_searchContentCheckBox = new QCheckBox(tr("搜索文件内容"));

    layout->addWidget(m_useRegexCheckBox);
    layout->addWidget(m_caseSensitiveCheckBox);
    layout->addWidget(m_wholeWordCheckBox);
    layout->addWidget(m_includeHiddenCheckBox);
    layout->addWidget(m_searchContentCheckBox);
}

void FileSearch::setupFilterOptionsArea() {
    m_filterOptionsGroup = new QGroupBox(tr("过滤选项"));
    QFormLayout *layout = new QFormLayout(m_filterOptionsGroup);

    // 文件类型过滤
    m_fileTypesLineEdit = new QLineEdit;
    m_fileTypesLineEdit->setPlaceholderText(tr("例如: txt,doc,pdf"));
    layout->addRow(tr("文件类型:"), m_fileTypesLineEdit);

    // 文件大小过滤
    m_minSizeSpinBox = new QSpinBox;
    m_minSizeSpinBox->setRange(0, 999999);
    m_minSizeSpinBox->setSuffix(tr(" KB"));
    layout->addRow(tr("最小大小:"), m_minSizeSpinBox);

    m_maxSizeSpinBox = new QSpinBox;
    m_maxSizeSpinBox->setRange(0, 999999);
    m_maxSizeSpinBox->setSuffix(tr(" KB"));
    layout->addRow(tr("最大大小:"), m_maxSizeSpinBox);

    // 修改日期过滤
    m_dateFromEdit = new QDateEdit;
    m_dateFromEdit->setCalendarPopup(true);
    m_dateFromEdit->setDate(QDate::currentDate().addYears(-1));
    layout->addRow(tr("修改日期从:"), m_dateFromEdit);

    m_dateToEdit = new QDateEdit;
    m_dateToEdit->setCalendarPopup(true);
    m_dateToEdit->setDate(QDate::currentDate());
    layout->addRow(tr("修改日期到:"), m_dateToEdit);

    // 最大结果数
    m_maxResultsSpinBox = new QSpinBox;
    m_maxResultsSpinBox->setRange(100, 100000);
    m_maxResultsSpinBox->setValue(10000);
    layout->addRow(tr("最大结果数:"), m_maxResultsSpinBox);
}

void FileSearch::setupResultsArea() {
    m_resultsGroup = new QGroupBox(tr("搜索结果"));
    QVBoxLayout *layout = new QVBoxLayout(m_resultsGroup);

    // 结果树视图
    m_resultsTreeView = new QTreeView;
    m_resultsModel = new QStandardItemModel(this);
    m_resultsModel->setHorizontalHeaderLabels({
        tr("文件名"), tr("路径"), tr("大小"), tr("修改时间"), tr("类型")
    });

    m_proxyModel = new FileSearchSortProxyModel(this);
    m_proxyModel->setSourceModel(m_resultsModel);
    m_resultsTreeView->setModel(m_proxyModel);

    m_resultsTreeView->setSortingEnabled(true);
    m_resultsTreeView->setAlternatingRowColors(true);
    m_resultsTreeView->setContextMenuPolicy(Qt::CustomContextMenu);

    // 设置列宽
    QHeaderView *header = m_resultsTreeView->header();
    header->resizeSection(0, 200); // 文件名
    header->resizeSection(1, 300); // 路径
    header->resizeSection(2, 80);  // 大小
    header->resizeSection(3, 120); // 修改时间
    header->resizeSection(4, 60);  // 类型
    header->setStretchLastSection(false);

    layout->addWidget(m_resultsTreeView);

    // 进度条和状态
    m_progressBar = new QProgressBar;
    m_statusLabel = new QLabel(tr("就绪"));

    QHBoxLayout *statusLayout = new QHBoxLayout;
    statusLayout->addWidget(m_progressBar);
    statusLayout->addWidget(m_statusLabel);
    layout->addLayout(statusLayout);
}

void FileSearch::setupContextMenu() {
    m_contextMenu = new QMenu(this);

    m_copyPathAction = m_contextMenu->addAction(tr("复制完整路径"));
    m_copyFileNameAction = m_contextMenu->addAction(tr("复制文件名"));
    m_contextMenu->addSeparator();
    m_openFileAction = m_contextMenu->addAction(tr("打开文件"));
    m_openLocationAction = m_contextMenu->addAction(tr("打开文件位置"));
    m_contextMenu->addSeparator();
    m_deleteFileAction = m_contextMenu->addAction(tr("删除文件"));
}

void FileSearch::connectSignals() {
    connect(m_searchLineEdit, &QLineEdit::textChanged, this, &FileSearch::onSearchTextChanged);
    connect(m_startSearchButton, &QPushButton::clicked, this, &FileSearch::onStartSearch);
    connect(m_stopSearchButton, &QPushButton::clicked, this, &FileSearch::onStopSearch);
    connect(m_clearResultsButton, &QPushButton::clicked, this, &FileSearch::onClearResults);
    connect(m_addPathButton, &QPushButton::clicked, this, &FileSearch::onAddSearchPath);
    connect(m_removePathButton, &QPushButton::clicked, this, &FileSearch::onRemoveSearchPath);

    connect(m_resultsTreeView, &QTreeView::doubleClicked, this, &FileSearch::onResultDoubleClicked);
    connect(m_resultsTreeView, &QTreeView::customContextMenuRequested, this, &FileSearch::onResultContextMenu);

    // 上下文菜单操作
    connect(m_copyPathAction, &QAction::triggered, this, &FileSearch::onCopyPath);
    connect(m_copyFileNameAction, &QAction::triggered, this, &FileSearch::onCopyFileName);
    connect(m_openFileAction, &QAction::triggered, this, &FileSearch::onOpenFile);
    connect(m_openLocationAction, &QAction::triggered, this, &FileSearch::onOpenFileLocation);
    connect(m_deleteFileAction, &QAction::triggered, this, &FileSearch::onDeleteFile);
}

void FileSearch::applyStyles() {
    // 应用现有项目的样式风格
    setStyleSheet(R"(
        QGroupBox {
            font-weight: bold;
            border: 2px solid #cccccc;
            border-radius: 8px;
            margin-top: 1ex;
            padding-top: 10px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px 0 5px;
        }
        QPushButton {
            background-color: #f0f0f0;
            border: 1px solid #ccc;
            border-radius: 4px;
            padding: 5px 10px;
            min-width: 80px;
        }
        QPushButton:hover {
            background-color: #e0e0e0;
        }
        QPushButton:pressed {
            background-color: #d0d0d0;
        }
        QPushButton:disabled {
            background-color: #f5f5f5;
            color: #999;
        }
        QTreeView {
            border: 1px solid #ccc;
            border-radius: 4px;
            alternate-background-color: #f9f9f9;
        }
        QTreeView::item {
            padding: 2px;
        }
        QTreeView::item:selected {
            background-color: #3875d7;
            color: white;
        }
    )");
}

SearchConfig FileSearch::getSearchConfig() const {
    SearchConfig config;

    config.searchText = m_searchLineEdit->text();

    // 获取所有搜索路径
    for (int i = 0; i < m_searchPathCombo->count(); ++i) {
        QString path = m_searchPathCombo->itemText(i);
        if (!path.isEmpty() && QDir(path).exists()) {
            config.searchPaths.append(path);
        }
    }

    // 如果下拉框中的当前文本不在列表中，也添加进去
    QString currentPath = m_searchPathCombo->currentText();
    if (!currentPath.isEmpty() && QDir(currentPath).exists() && !config.searchPaths.contains(currentPath)) {
        config.searchPaths.append(currentPath);
    }

    config.useRegex = m_useRegexCheckBox->isChecked();
    config.caseSensitive = m_caseSensitiveCheckBox->isChecked();
    config.wholeWordOnly = m_wholeWordCheckBox->isChecked();
    config.includeHidden = m_includeHiddenCheckBox->isChecked();
    config.searchInContent = m_searchContentCheckBox->isChecked();

    // 解析文件类型
    QString fileTypesText = m_fileTypesLineEdit->text();
    if (!fileTypesText.isEmpty()) {
        config.fileTypes = fileTypesText.split(',', Qt::SkipEmptyParts);
        for (QString &type : config.fileTypes) {
            type = type.trimmed().toLower();
        }
    }

    config.minFileSize = m_minSizeSpinBox->value() * 1024; // 转换为字节
    config.maxFileSize = m_maxSizeSpinBox->value() * 1024; // 转换为字节
    config.dateFrom = m_dateFromEdit->date();
    config.dateTo = m_dateToEdit->date();
    config.maxResults = m_maxResultsSpinBox->value();

    return config;
}

void FileSearch::onSearchTextChanged() {
    updateSearchButtonsState();
}

void FileSearch::onStartSearch() {
    if (m_isSearching) return;

    SearchConfig config = getSearchConfig();

    if (config.searchText.isEmpty()) {
        showMessage(tr("请输入搜索内容"), true);
        return;
    }

    if (config.searchPaths.isEmpty()) {
        showMessage(tr("请添加至少一个搜索路径"), true);
        return;
    }

    // 清空之前的结果
    onClearResults();

    // 创建工作线程
    m_searchThread = new QThread(this);
    m_searchWorker = new FileSearchWorker(config);
    m_searchWorker->moveToThread(m_searchThread);

    // 连接信号
    connect(m_searchThread, &QThread::started, m_searchWorker, &FileSearchWorker::startSearch);
    connect(m_searchWorker, &FileSearchWorker::searchProgress, this, &FileSearch::onSearchProgress);
    connect(m_searchWorker, &FileSearchWorker::resultFound, this, &FileSearch::onResultFound);
    connect(m_searchWorker, &FileSearchWorker::searchFinished, this, &FileSearch::onSearchFinished);
    connect(m_searchWorker, &FileSearchWorker::searchError, this, &FileSearch::onSearchError);

    // 启动搜索
    m_isSearching = true;
    updateSearchButtonsState();
    m_progressBar->setRange(0, config.maxResults);
    m_progressBar->setValue(0);
    m_statusLabel->setText(tr("搜索中..."));

    m_searchThread->start();
}

void FileSearch::onStopSearch() {
    if (!m_isSearching || !m_searchWorker) return;

    m_searchWorker->stopSearch();
    m_statusLabel->setText(tr("正在停止搜索..."));
}

void FileSearch::onClearResults() {
    m_resultsModel->clear();
    m_resultsModel->setHorizontalHeaderLabels({
        tr("文件名"), tr("路径"), tr("大小"), tr("修改时间"), tr("类型")
    });
    m_progressBar->setValue(0);
    m_statusLabel->setText(tr("就绪"));
}

void FileSearch::onAddSearchPath() {
    QString path = QFileDialog::getExistingDirectory(this, tr("选择搜索路径"));
    if (!path.isEmpty()) {
        // 检查是否已存在
        bool exists = false;
        for (int i = 0; i < m_searchPathCombo->count(); ++i) {
            if (m_searchPathCombo->itemText(i) == path) {
                exists = true;
                break;
            }
        }

        if (!exists) {
            m_searchPathCombo->addItem(path);
        }
        m_searchPathCombo->setCurrentText(path);
    }
}

void FileSearch::onRemoveSearchPath() {
    int currentIndex = m_searchPathCombo->currentIndex();
    if (currentIndex >= 0) {
        m_searchPathCombo->removeItem(currentIndex);
    }
}

void FileSearch::onSearchProgress(int current, int total) {
    m_progressBar->setValue(current);
    m_statusLabel->setText(tr("已找到 %1 个文件...").arg(current));
}

void FileSearch::onResultFound(const SearchResultItem &item) {
    QList<QStandardItem*> rowItems;

    // 文件名
    QStandardItem *nameItem = new QStandardItem(item.fileIcon, item.fileName);
    rowItems.append(nameItem);

    // 路径
    QStandardItem *pathItem = new QStandardItem(item.filePath);
    rowItems.append(pathItem);

    // 文件大小
    QString sizeText = item.isDirectory ? tr("<目录>") : formatFileSize(item.fileSize);
    QStandardItem *sizeItem = new QStandardItem(sizeText);
    sizeItem->setData(item.fileSize, Qt::UserRole); // 用于排序
    rowItems.append(sizeItem);

    // 修改时间
    QString timeText = item.lastModified.toString("yyyy-MM-dd hh:mm:ss");
    QStandardItem *timeItem = new QStandardItem(timeText);
    timeItem->setData(item.lastModified, Qt::UserRole); // 用于排序
    rowItems.append(timeItem);

    // 文件类型
    QString typeText = item.isDirectory ? tr("文件夹") :
                      item.fileExtension.isEmpty() ? tr("文件") :
                      item.fileExtension.toUpper() + tr(" 文件");
    QStandardItem *typeItem = new QStandardItem(typeText);
    rowItems.append(typeItem);

    m_resultsModel->appendRow(rowItems);
}

void FileSearch::onSearchFinished(int totalResults, qint64 elapsedMs) {
    m_isSearching = false;
    updateSearchButtonsState();

    m_progressBar->setValue(totalResults);
    m_statusLabel->setText(tr("搜索完成: 找到 %1 个文件，用时 %2")
                           .arg(totalResults)
                           .arg(formatElapsedTime(elapsedMs)));

    // 清理线程
    if (m_searchThread) {
        m_searchThread->quit();
        m_searchThread->wait();
        m_searchThread->deleteLater();
        m_searchThread = nullptr;
    }

    if (m_searchWorker) {
        m_searchWorker->deleteLater();
        m_searchWorker = nullptr;
    }
}

void FileSearch::onSearchError(const QString &error) {
    m_isSearching = false;
    updateSearchButtonsState();
    showMessage(error, true);

    // 清理线程
    if (m_searchThread) {
        m_searchThread->quit();
        m_searchThread->wait();
        m_searchThread->deleteLater();
        m_searchThread = nullptr;
    }

    if (m_searchWorker) {
        m_searchWorker->deleteLater();
        m_searchWorker = nullptr;
    }
}

void FileSearch::onResultDoubleClicked(const QModelIndex &index) {
    if (!index.isValid()) return;

    QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
    QStandardItem *pathItem = m_resultsModel->item(sourceIndex.row(), 1);
    if (pathItem) {
        QString filePath = pathItem->text();
        QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
    }
}

void FileSearch::onResultContextMenu(const QPoint &pos) {
    QModelIndex index = m_resultsTreeView->indexAt(pos);
    if (!index.isValid()) return;

    m_contextMenuIndex = m_proxyModel->mapToSource(index);
    m_contextMenu->exec(m_resultsTreeView->mapToGlobal(pos));
}

void FileSearch::onCopyPath() {
    if (!m_contextMenuIndex.isValid()) return;

    QStandardItem *pathItem = m_resultsModel->item(m_contextMenuIndex.row(), 1);
    if (pathItem) {
        QApplication::clipboard()->setText(pathItem->text());
        showMessage(tr("路径已复制到剪贴板"));
    }
}

void FileSearch::onCopyFileName() {
    if (!m_contextMenuIndex.isValid()) return;

    QStandardItem *nameItem = m_resultsModel->item(m_contextMenuIndex.row(), 0);
    if (nameItem) {
        QApplication::clipboard()->setText(nameItem->text());
        showMessage(tr("文件名已复制到剪贴板"));
    }
}

void FileSearch::onOpenFile() {
    if (!m_contextMenuIndex.isValid()) return;

    QStandardItem *pathItem = m_resultsModel->item(m_contextMenuIndex.row(), 1);
    if (pathItem) {
        QString filePath = pathItem->text();
        QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
    }
}

void FileSearch::onOpenFileLocation() {
    if (!m_contextMenuIndex.isValid()) return;

    QStandardItem *pathItem = m_resultsModel->item(m_contextMenuIndex.row(), 1);
    if (pathItem) {
        QString filePath = pathItem->text();

#ifdef Q_OS_WIN
        // Windows: 使用 explorer.exe 选中文件
        QString cmd = QString("explorer.exe /select,\"%1\"").arg(QDir::toNativeSeparators(filePath));
        QProcess::startDetached(cmd);
#elif defined(Q_OS_MAC)
        // macOS: 使用 Finder 显示文件
        QProcess::startDetached("open", {"-R", filePath});
#else
        // Linux: 打开包含文件的目录
        QFileInfo fileInfo(filePath);
        QDesktopServices::openUrl(QUrl::fromLocalFile(fileInfo.absolutePath()));
#endif
    }
}

void FileSearch::onDeleteFile() {
    if (!m_contextMenuIndex.isValid()) return;

    QStandardItem *pathItem = m_resultsModel->item(m_contextMenuIndex.row(), 1);
    QStandardItem *nameItem = m_resultsModel->item(m_contextMenuIndex.row(), 0);
    if (pathItem && nameItem) {
        QString filePath = pathItem->text();
        QString fileName = nameItem->text();

        int ret = QMessageBox::warning(this, tr("确认删除"),
                                     tr("确定要删除文件 '%1' 吗？\n\n此操作不可恢复。").arg(fileName),
                                     QMessageBox::Yes | QMessageBox::No,
                                     QMessageBox::No);

        if (ret == QMessageBox::Yes) {
            QFile file(filePath);
            if (file.remove()) {
                // 从结果中移除该行
                m_resultsModel->removeRow(m_contextMenuIndex.row());
                showMessage(tr("文件删除成功"));
            } else {
                showMessage(tr("文件删除失败: %1").arg(file.errorString()), true);
            }
        }
    }
}

void FileSearch::onExportResults() {
    // 导出搜索结果的实现（可选功能）
}

void FileSearch::onImportConfig() {
    // 导入搜索配置的实现（可选功能）
}

void FileSearch::onExportConfig() {
    // 导出搜索配置的实现（可选功能）
}

QString FileSearch::formatFileSize(qint64 bytes) const {
    const qint64 KB = 1024;
    const qint64 MB = KB * 1024;
    const qint64 GB = MB * 1024;

    if (bytes >= GB) {
        return QString("%1 GB").arg(QString::number(bytes / double(GB), 'f', 2));
    } else if (bytes >= MB) {
        return QString("%1 MB").arg(QString::number(bytes / double(MB), 'f', 2));
    } else if (bytes >= KB) {
        return QString("%1 KB").arg(QString::number(bytes / double(KB), 'f', 1));
    } else {
        return QString("%1 B").arg(bytes);
    }
}

QString FileSearch::formatElapsedTime(qint64 milliseconds) const {
    if (milliseconds < 1000) {
        return QString("%1 ms").arg(milliseconds);
    } else if (milliseconds < 60000) {
        return QString("%1.%2 s").arg(milliseconds / 1000).arg((milliseconds % 1000) / 100);
    } else {
        int seconds = milliseconds / 1000;
        int minutes = seconds / 60;
        seconds %= 60;
        return QString("%1:%2").arg(minutes).arg(seconds, 2, 10, QChar('0'));
    }
}

void FileSearch::updateSearchButtonsState() {
    bool canSearch = !m_isSearching && !m_searchLineEdit->text().isEmpty();
    bool canStop = m_isSearching;

    m_startSearchButton->setEnabled(canSearch);
    m_stopSearchButton->setEnabled(canStop);
    m_searchLineEdit->setEnabled(!m_isSearching);
}

void FileSearch::showMessage(const QString &message, bool isError) {
    m_statusLabel->setText(message);
    if (isError) {
        m_statusLabel->setStyleSheet("color: red;");
    } else {
        m_statusLabel->setStyleSheet("color: green;");
    }

    // 3秒后恢复正常状态
    QTimer::singleShot(3000, [this]() {
        m_statusLabel->setStyleSheet("");
        if (!m_isSearching) {
            m_statusLabel->setText(tr("就绪"));
        }
    });
}

void FileSearch::saveSettings() {
    QSettings settings("LeleTools", "FileSearch");

    // 保存搜索路径
    settings.beginWriteArray("searchPaths");
    for (int i = 0; i < m_searchPathCombo->count(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("path", m_searchPathCombo->itemText(i));
    }
    settings.endArray();

    // 保存搜索选项
    settings.setValue("useRegex", m_useRegexCheckBox->isChecked());
    settings.setValue("caseSensitive", m_caseSensitiveCheckBox->isChecked());
    settings.setValue("wholeWord", m_wholeWordCheckBox->isChecked());
    settings.setValue("includeHidden", m_includeHiddenCheckBox->isChecked());
    settings.setValue("searchContent", m_searchContentCheckBox->isChecked());

    // 保存过滤选项
    settings.setValue("fileTypes", m_fileTypesLineEdit->text());
    settings.setValue("minSize", m_minSizeSpinBox->value());
    settings.setValue("maxSize", m_maxSizeSpinBox->value());
    settings.setValue("maxResults", m_maxResultsSpinBox->value());
}

void FileSearch::loadSettings() {
    QSettings settings("LeleTools", "FileSearch");

    // 加载搜索路径
    int size = settings.beginReadArray("searchPaths");
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        QString path = settings.value("path").toString();
        if (!path.isEmpty() && QDir(path).exists()) {
            m_searchPathCombo->addItem(path);
        }
    }
    settings.endArray();

    // 如果没有保存的路径，添加默认路径
    if (m_searchPathCombo->count() == 0) {
        m_searchPathCombo->addItem(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
        m_searchPathCombo->addItem(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
        m_searchPathCombo->addItem(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));
    }

    // 加载搜索选项
    m_useRegexCheckBox->setChecked(settings.value("useRegex", false).toBool());
    m_caseSensitiveCheckBox->setChecked(settings.value("caseSensitive", false).toBool());
    m_wholeWordCheckBox->setChecked(settings.value("wholeWord", false).toBool());
    m_includeHiddenCheckBox->setChecked(settings.value("includeHidden", false).toBool());
    m_searchContentCheckBox->setChecked(settings.value("searchContent", false).toBool());

    // 加载过滤选项
    m_fileTypesLineEdit->setText(settings.value("fileTypes", "").toString());
    m_minSizeSpinBox->setValue(settings.value("minSize", 0).toInt());
    m_maxSizeSpinBox->setValue(settings.value("maxSize", 0).toInt());
    m_maxResultsSpinBox->setValue(settings.value("maxResults", 10000).toInt());
}

