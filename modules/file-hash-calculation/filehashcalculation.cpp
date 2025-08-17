#include "filehashcalculation.h"
#include <QMenu>
#include <QTextStream>

REGISTER_DYNAMICOBJECT(FileHashCalculation);

// HashCalculationWorker 实现
HashCalculationWorker::HashCalculationWorker(QObject *parent)
    : QObject(parent), m_stopFlag(false)
{
}

void HashCalculationWorker::setFiles(const QStringList &files)
{
    m_files = files;
}

void HashCalculationWorker::setAlgorithms(const QList<HashAlgorithm> &algorithms)
{
    m_algorithms = algorithms;
}

void HashCalculationWorker::calculateHashes()
{
    for (int i = 0; i < m_files.size(); ++i) {
        if (m_stopFlag) {
            break;
        }
        
        const QString &filePath = m_files[i];
        HashResult result;
        
        QFileInfo fileInfo(filePath);
        if (!fileInfo.exists() || !fileInfo.isFile()) {
            result.fileName = fileInfo.fileName();
            result.filePath = filePath;
            result.isValid = false;
            result.errorMessage = "文件不存在或不是有效文件";
            emit fileProcessed(result);
            emit progressUpdate(i + 1, m_files.size());
            continue;
        }
        
        result.fileName = fileInfo.fileName();
        result.filePath = filePath;
        result.fileSize = fileInfo.size();
        result.modifyTime = fileInfo.lastModified();
        result.isValid = true;
        
        // 计算各种哈希值
        for (HashAlgorithm algorithm : m_algorithms) {
            if (m_stopFlag) break;
            
            QString hash = calculateFileHash(filePath, getQtAlgorithm(algorithm));
            if (hash.isEmpty()) {
                result.isValid = false;
                result.errorMessage = "计算哈希值失败";
                break;
            }
            
            // 根据算法类型设置对应的哈希值
            switch (algorithm) {
                case HashAlgorithm::MD5:
                    result.md5 = hash;
                    break;
                case HashAlgorithm::SHA1:
                    result.sha1 = hash;
                    break;
                case HashAlgorithm::SHA256:
                    result.sha256 = hash;
                    break;
                case HashAlgorithm::SHA512:
                    result.sha512 = hash;
                    break;
                default:
                    // 其他算法可以扩展存储
                    break;
            }
        }
        
        emit fileProcessed(result);
        emit progressUpdate(i + 1, m_files.size());
    }
    
    emit calculationFinished();
}

QString HashCalculationWorker::calculateFileHash(const QString &filePath, QCryptographicHash::Algorithm algorithm)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    
    QCryptographicHash hash(algorithm);
    const qint64 bufferSize = 64 * 1024; // 64KB buffer
    
    while (!file.atEnd() && !m_stopFlag) {
        QByteArray data = file.read(bufferSize);
        hash.addData(data);
    }
    
    if (m_stopFlag) {
        return QString();
    }
    
    return hash.result().toHex().toUpper();
}

QString HashCalculationWorker::getAlgorithmName(HashAlgorithm algorithm)
{
    switch (algorithm) {
        case HashAlgorithm::MD5: return "MD5";
        case HashAlgorithm::SHA1: return "SHA1";
        case HashAlgorithm::SHA224: return "SHA224";
        case HashAlgorithm::SHA256: return "SHA256";
        case HashAlgorithm::SHA384: return "SHA384";
        case HashAlgorithm::SHA512: return "SHA512";
        case HashAlgorithm::SHA3_224: return "SHA3-224";
        case HashAlgorithm::SHA3_256: return "SHA3-256";
        case HashAlgorithm::SHA3_384: return "SHA3-384";
        case HashAlgorithm::SHA3_512: return "SHA3-512";
        default: return "Unknown";
    }
}

QCryptographicHash::Algorithm HashCalculationWorker::getQtAlgorithm(HashAlgorithm algorithm)
{
    switch (algorithm) {
        case HashAlgorithm::MD5: return QCryptographicHash::Md5;
        case HashAlgorithm::SHA1: return QCryptographicHash::Sha1;
        case HashAlgorithm::SHA224: return QCryptographicHash::Sha224;
        case HashAlgorithm::SHA256: return QCryptographicHash::Sha256;
        case HashAlgorithm::SHA384: return QCryptographicHash::Sha384;
        case HashAlgorithm::SHA512: return QCryptographicHash::Sha512;
        case HashAlgorithm::SHA3_224: return QCryptographicHash::Sha3_224;
        case HashAlgorithm::SHA3_256: return QCryptographicHash::Sha3_256;
        case HashAlgorithm::SHA3_384: return QCryptographicHash::Sha3_384;
        case HashAlgorithm::SHA3_512: return QCryptographicHash::Sha3_512;
        default: return QCryptographicHash::Md5;
    }
}

// FileHashCalculation 主类实现
FileHashCalculation::FileHashCalculation() : QWidget(nullptr), DynamicObjectBase()
{
    m_workerThread = nullptr;
    m_worker = nullptr;
    m_calculating = false;
    
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, [this]() {
        if (m_calculating && !m_startTime.isNull()) {
            qint64 elapsed = m_startTime.msecsTo(QDateTime::currentDateTime());
            timeLabel->setText(QString("耗时: %1").arg(formatDuration(elapsed)));
        }
    });
    
    setupUI();
    setAcceptDrops(true);
    
    // 设置统一的样式
    setStyleSheet(R"(
        QCheckBox {
            spacing: 8px;
            font-size: 11pt;
        }
        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border: 2px solid #cccccc;
            border-radius: 3px;
            background-color: white;
        }
        QCheckBox::indicator:hover {
            border-color: #4CAF50;
            background-color: #f8f8f8;
        }
        QCheckBox::indicator:checked {
            background-color: #4CAF50;
            border-color: #4CAF50;
            image: url(data:image/svg+xml;base64,PHN2ZyB3aWR0aD0iMTIiIGhlaWdodD0iMTIiIHZpZXdCb3g9IjAgMCAxMiAxMiIgZmlsbD0ibm9uZSIgeG1sbnM9Imh0dHA6Ly93d3cudzMub3JnLzIwMDAvc3ZnIj4KPHBhdGggZD0iTTEwIDNMNC41IDguNUwyIDYiIHN0cm9rZT0id2hpdGUiIHN0cm9rZS13aWR0aD0iMiIgc3Ryb2tlLWxpbmVjYXA9InJvdW5kIiBzdHJva2UtbGluZWpvaW49InJvdW5kIi8+Cjwvc3ZnPgo=);
        }
        QCheckBox::indicator:checked:hover {
            background-color: #45a049;
            border-color: #45a049;
        }
        QCheckBox::indicator:disabled {
            background-color: #f5f5f5;
            border-color: #e0e0e0;
        }
        QGroupBox {
            font-weight: bold;
            border: 2px solid #cccccc;
            border-radius: 5px;
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
            border: 1px solid #cccccc;
            border-radius: 4px;
            padding: 6px 12px;
            font-size: 11pt;
        }
        QPushButton:hover {
            background-color: #e0e0e0;
            border-color: #999999;
        }
        QPushButton:pressed {
            background-color: #d0d0d0;
        }
        QPushButton:disabled {
            background-color: #f5f5f5;
            color: #999999;
            border-color: #e0e0e0;
        }
    )");
}

FileHashCalculation::~FileHashCalculation()
{
    if (m_workerThread && m_workerThread->isRunning()) {
        if (m_worker) {
            m_worker->setStopFlag(true);
        }
        m_workerThread->quit();
        m_workerThread->wait(3000);
    }
}

void FileHashCalculation::setupUI()
{
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);
    
    mainSplitter = new QSplitter(Qt::Horizontal, this);
    
    // 左侧面板
    leftPanel = new QWidget;
    leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    
    // 右侧面板
    rightPanel = new QWidget;
    rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    
    setupFileSelectionArea();
    setupAlgorithmArea();
    setupControlArea();
    setupResultArea();
    setupComparisonArea();
    setupStatusArea();
    
    leftLayout->addWidget(fileSelectionGroup);
    leftLayout->addWidget(algorithmGroup);
    leftLayout->addWidget(controlGroup);
    
    rightLayout->addWidget(resultGroup);
    rightLayout->addWidget(comparisonGroup);
    
    mainSplitter->addWidget(leftPanel);
    mainSplitter->addWidget(rightPanel);
    mainSplitter->setSizes({350, 450});
    
    mainLayout->addWidget(mainSplitter, 1); // 添加stretch factor
    mainLayout->addLayout(statusLayout);
}

void FileHashCalculation::setupFileSelectionArea()
{
    fileSelectionGroup = new QGroupBox("文件选择", this);
    fileSelectionLayout = new QVBoxLayout(fileSelectionGroup);
    
    fileListEdit = new QTextEdit;
    fileListEdit->setMaximumHeight(120);
    fileListEdit->setPlaceholderText("拖拽文件到此处，或点击按钮选择文件...");
    fileListEdit->setReadOnly(true);
    fileSelectionLayout->addWidget(fileListEdit);
    
    fileButtonLayout = new QHBoxLayout;
    selectFilesBtn = new QPushButton("选择文件");
    selectFolderBtn = new QPushButton("选择文件夹");
    clearFilesBtn = new QPushButton("清空列表");
    
    fileButtonLayout->addWidget(selectFilesBtn);
    fileButtonLayout->addWidget(selectFolderBtn);
    fileButtonLayout->addWidget(clearFilesBtn);
    fileSelectionLayout->addLayout(fileButtonLayout);
    
    fileCountLabel = new QLabel("已选择 0 个文件");
    fileSelectionLayout->addWidget(fileCountLabel);
    
    // 连接信号
    connect(selectFilesBtn, &QPushButton::clicked, this, &FileHashCalculation::onSelectFilesClicked);
    connect(selectFolderBtn, &QPushButton::clicked, this, &FileHashCalculation::onSelectFolderClicked);
    connect(clearFilesBtn, &QPushButton::clicked, [this]() {
        m_selectedFiles.clear();
        updateFileList();
    });
}

void FileHashCalculation::setupAlgorithmArea()
{
    algorithmGroup = new QGroupBox("哈希算法", this);
    algorithmLayout = new QGridLayout(algorithmGroup);
    
    md5CheckBox = new QCheckBox("MD5");
    sha1CheckBox = new QCheckBox("SHA1");
    sha224CheckBox = new QCheckBox("SHA224");
    sha256CheckBox = new QCheckBox("SHA256");
    sha384CheckBox = new QCheckBox("SHA384");
    sha512CheckBox = new QCheckBox("SHA512");
    sha3_224CheckBox = new QCheckBox("SHA3-224");
    sha3_256CheckBox = new QCheckBox("SHA3-256");
    sha3_384CheckBox = new QCheckBox("SHA3-384");
    sha3_512CheckBox = new QCheckBox("SHA3-512");
    
    // 默认选择常用算法
    md5CheckBox->setChecked(true);
    sha1CheckBox->setChecked(true);
    sha256CheckBox->setChecked(true);
    
    algorithmLayout->addWidget(md5CheckBox, 0, 0);
    algorithmLayout->addWidget(sha1CheckBox, 0, 1);
    algorithmLayout->addWidget(sha224CheckBox, 1, 0);
    algorithmLayout->addWidget(sha256CheckBox, 1, 1);
    algorithmLayout->addWidget(sha384CheckBox, 2, 0);
    algorithmLayout->addWidget(sha512CheckBox, 2, 1);
    algorithmLayout->addWidget(sha3_224CheckBox, 3, 0);
    algorithmLayout->addWidget(sha3_256CheckBox, 3, 1);
    algorithmLayout->addWidget(sha3_384CheckBox, 4, 0);
    algorithmLayout->addWidget(sha3_512CheckBox, 4, 1);
    
    QHBoxLayout *algorithmButtonLayout = new QHBoxLayout;
    selectAllBtn = new QPushButton("全选");
    selectNoneBtn = new QPushButton("全不选");
    
    algorithmButtonLayout->addWidget(selectAllBtn);
    algorithmButtonLayout->addWidget(selectNoneBtn);
    algorithmButtonLayout->addStretch();
    
    algorithmLayout->addLayout(algorithmButtonLayout, 5, 0, 1, 2);
    
    // 连接信号
    connect(selectAllBtn, &QPushButton::clicked, [this]() {
        md5CheckBox->setChecked(true);
        sha1CheckBox->setChecked(true);
        sha224CheckBox->setChecked(true);
        sha256CheckBox->setChecked(true);
        sha384CheckBox->setChecked(true);
        sha512CheckBox->setChecked(true);
        sha3_224CheckBox->setChecked(true);
        sha3_256CheckBox->setChecked(true);
        sha3_384CheckBox->setChecked(true);
        sha3_512CheckBox->setChecked(true);
    });
    
    connect(selectNoneBtn, &QPushButton::clicked, [this]() {
        md5CheckBox->setChecked(false);
        sha1CheckBox->setChecked(false);
        sha224CheckBox->setChecked(false);
        sha256CheckBox->setChecked(false);
        sha384CheckBox->setChecked(false);
        sha512CheckBox->setChecked(false);
        sha3_224CheckBox->setChecked(false);
        sha3_256CheckBox->setChecked(false);
        sha3_384CheckBox->setChecked(false);
        sha3_512CheckBox->setChecked(false);
    });
}

void FileHashCalculation::setupControlArea()
{
    controlGroup = new QGroupBox("操作控制", this);
    controlLayout = new QHBoxLayout(controlGroup);
    
    calculateBtn = new QPushButton("开始计算");
    stopBtn = new QPushButton("停止计算");
    clearBtn = new QPushButton("清空结果");
    
    stopBtn->setEnabled(false);
    
    controlLayout->addWidget(calculateBtn);
    controlLayout->addWidget(stopBtn);
    controlLayout->addWidget(clearBtn);
    
    // 连接信号
    connect(calculateBtn, &QPushButton::clicked, this, &FileHashCalculation::onCalculateClicked);
    connect(stopBtn, &QPushButton::clicked, this, &FileHashCalculation::onStopClicked);
    connect(clearBtn, &QPushButton::clicked, this, &FileHashCalculation::onClearClicked);
}

void FileHashCalculation::setupResultArea()
{
    resultGroup = new QGroupBox("计算结果", this);
    resultLayout = new QVBoxLayout(resultGroup);
    
    resultTable = new QTableWidget;
    resultTable->setColumnCount(7);
    resultTable->setHorizontalHeaderLabels({"文件名", "大小", "MD5", "SHA1", "SHA256", "SHA512", "状态"});
    resultTable->horizontalHeader()->setStretchLastSection(true);
    resultTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    resultTable->setContextMenuPolicy(Qt::CustomContextMenu);
    resultLayout->addWidget(resultTable);
    
    resultButtonLayout = new QHBoxLayout;
    copyResultBtn = new QPushButton("复制结果");
    saveResultBtn = new QPushButton("保存结果");
    exportBtn = new QPushButton("导出报告");
    
    resultButtonLayout->addWidget(copyResultBtn);
    resultButtonLayout->addWidget(saveResultBtn);
    resultButtonLayout->addWidget(exportBtn);
    resultButtonLayout->addStretch();
    resultLayout->addLayout(resultButtonLayout);
    
    // 连接信号
    connect(copyResultBtn, &QPushButton::clicked, this, &FileHashCalculation::onCopyResultClicked);
    connect(saveResultBtn, &QPushButton::clicked, this, &FileHashCalculation::onSaveResultClicked);
    connect(exportBtn, &QPushButton::clicked, [this]() { saveResultsToFile(); });
    connect(resultTable, &QTableWidget::customContextMenuRequested, 
            this, &FileHashCalculation::onResultTableContextMenu);
    connect(resultTable, &QTableWidget::itemClicked, 
            this, &FileHashCalculation::onResultTableItemClicked);
}

void FileHashCalculation::setupComparisonArea()
{
    comparisonGroup = new QGroupBox("哈希值比较验证", this);
    comparisonLayout = new QVBoxLayout(comparisonGroup);
    
    QHBoxLayout *inputLayout = new QHBoxLayout;
    inputLayout->addWidget(new QLabel("期望哈希值:"));
    expectedHashEdit = new QLineEdit;
    expectedHashEdit->setPlaceholderText("输入要比较的哈希值...");
    inputLayout->addWidget(expectedHashEdit);
    comparisonLayout->addLayout(inputLayout);
    
    QHBoxLayout *typeLayout = new QHBoxLayout;
    typeLayout->addWidget(new QLabel("哈希类型:"));
    hashTypeCombo = new QComboBox;
    hashTypeCombo->addItems({"MD5", "SHA1", "SHA256", "SHA512"});
    hashTypeCombo->setCurrentText("MD5");
    typeLayout->addWidget(hashTypeCombo);
    
    compareBtn = new QPushButton("比较");
    verifyBtn = new QPushButton("验证");
    typeLayout->addWidget(compareBtn);
    typeLayout->addWidget(verifyBtn);
    typeLayout->addStretch();
    comparisonLayout->addLayout(typeLayout);
    
    comparisonResultLabel = new QLabel("等待比较...");
    comparisonResultLabel->setWordWrap(true);
    comparisonResultLabel->setStyleSheet("QLabel { padding: 8px; border: 1px solid #ccc; border-radius: 4px; }");
    comparisonLayout->addWidget(comparisonResultLabel);
    
    // 连接信号
    connect(compareBtn, &QPushButton::clicked, this, &FileHashCalculation::onCompareClicked);
    connect(verifyBtn, &QPushButton::clicked, this, &FileHashCalculation::onVerifyClicked);
}

void FileHashCalculation::setupStatusArea()
{
    statusLayout = new QHBoxLayout;
    statusLabel = new QLabel("就绪");
    progressBar = new QProgressBar;
    timeLabel = new QLabel("耗时: 00:00");
    
    progressBar->setVisible(false);
    
    statusLayout->addWidget(statusLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(timeLabel);
    statusLayout->addWidget(progressBar);
}

// 槽函数实现
void FileHashCalculation::onSelectFilesClicked()
{
    QStringList files = QFileDialog::getOpenFileNames(this, 
        "选择要计算哈希的文件", 
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        "所有文件 (*.*)");
    
    if (!files.isEmpty()) {
        addFilesToList(files);
    }
}

void FileHashCalculation::onSelectFolderClicked()
{
    QString folder = QFileDialog::getExistingDirectory(this, 
        "选择文件夹", 
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    
    if (!folder.isEmpty()) {
        QDir dir(folder);
        QStringList files;
        
        // 递归获取文件夹中的所有文件
        QDirIterator it(folder, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            files.append(it.next());
        }
        
        if (!files.isEmpty()) {
            addFilesToList(files);
        } else {
            QMessageBox::information(this, "提示", "选择的文件夹中没有文件");
        }
    }
}

void FileHashCalculation::onCalculateClicked()
{
    if (m_selectedFiles.isEmpty()) {
        QMessageBox::information(this, "提示", "请先选择要计算的文件");
        return;
    }
    
    QStringList selectedAlgorithms = getSelectedAlgorithms();
    if (selectedAlgorithms.isEmpty()) {
        QMessageBox::information(this, "提示", "请至少选择一种哈希算法");
        return;
    }
    
    startCalculation();
}

void FileHashCalculation::onStopClicked()
{
    stopCalculation();
}

void FileHashCalculation::onClearClicked()
{
    clearResults();
}

void FileHashCalculation::onCopyResultClicked()
{
    copyResultsToClipboard();
}

void FileHashCalculation::onSaveResultClicked()
{
    saveResultsToFile();
}

void FileHashCalculation::onCompareClicked()
{
    compareHashValues();
}

void FileHashCalculation::onVerifyClicked()
{
    verifyHashValue();
}

void FileHashCalculation::onAlgorithmChanged()
{
    // 算法选择变化时的处理
}

void FileHashCalculation::onProgressUpdate(int current, int total)
{
    updateProgress(current, total);
}

void FileHashCalculation::onFileProcessed(const HashResult &result)
{
    addResultToTable(result);
    m_results.append(result);
}

void FileHashCalculation::onCalculationFinished()
{
    m_calculating = false;
    calculateBtn->setEnabled(true);
    stopBtn->setEnabled(false);
    progressBar->setVisible(false);
    m_timer->stop();
    
    statusLabel->setText(QString("计算完成，共处理 %1 个文件").arg(m_results.size()));
    
    // 清理工作线程
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait();
        m_workerThread->deleteLater();
        m_workerThread = nullptr;
    }
    if (m_worker) {
        m_worker->deleteLater();
        m_worker = nullptr;
    }
}

void FileHashCalculation::onErrorOccurred(const QString &error)
{
    statusLabel->setText(QString("错误: %1").arg(error));
}

void FileHashCalculation::onResultTableContextMenu(const QPoint &pos)
{
    QTableWidgetItem *item = resultTable->itemAt(pos);
    if (!item) return;
    
    QMenu menu(this);
    menu.addAction("复制哈希值", [this, item]() {
        QApplication::clipboard()->setText(item->text());
    });
    menu.addAction("复制文件路径", [this, item]() {
        int row = item->row();
        if (row < m_results.size()) {
            QApplication::clipboard()->setText(m_results[row].filePath);
        }
    });
    menu.addAction("打开文件位置", [this, item]() {
        int row = item->row();
        if (row < m_results.size()) {
            QString filePath = m_results[row].filePath;
            QFileInfo fileInfo(filePath);
            QString dir = fileInfo.absoluteDir().absolutePath();
            QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
        }
    });
    
    menu.exec(resultTable->mapToGlobal(pos));
}

void FileHashCalculation::onResultTableItemClicked(QTableWidgetItem *item)
{
    if (!item) return;
    
    int row = item->row();
    if (row >= 0 && row < m_results.size()) {
        const HashResult &result = m_results[row];
        
        // 根据点击的列自动填充比较框
        QString hashValue;
        QString hashType;
        
        int column = item->column();
        switch (column) {
            case 2: // MD5列
                hashValue = result.md5;
                hashType = "MD5";
                break;
            case 3: // SHA1列
                hashValue = result.sha1;
                hashType = "SHA1";
                break;
            case 4: // SHA256列
                hashValue = result.sha256;
                hashType = "SHA256";
                break;
            case 5: // SHA512列
                hashValue = result.sha512;
                hashType = "SHA512";
                break;
            default:
                return;
        }
        
        if (!hashValue.isEmpty()) {
            expectedHashEdit->setText(hashValue);
            hashTypeCombo->setCurrentText(hashType);
        }
    }
}

// 拖拽支持
void FileHashCalculation::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void FileHashCalculation::dropEvent(QDropEvent *event)
{
    QStringList files;
    
    for (const QUrl &url : event->mimeData()->urls()) {
        if (url.isLocalFile()) {
            QString filePath = url.toLocalFile();
            QFileInfo fileInfo(filePath);
            
            if (fileInfo.isFile()) {
                files.append(filePath);
            } else if (fileInfo.isDir()) {
                // 如果是文件夹，递归添加其中的文件
                QDirIterator it(filePath, QDir::Files, QDirIterator::Subdirectories);
                while (it.hasNext()) {
                    files.append(it.next());
                }
            }
        }
    }
    
    if (!files.isEmpty()) {
        addFilesToList(files);
    }
    
    event->acceptProposedAction();
}

// 辅助功能实现
void FileHashCalculation::addFilesToList(const QStringList &files)
{
    for (const QString &file : files) {
        if (!m_selectedFiles.contains(file)) {
            m_selectedFiles.append(file);
        }
    }
    
    // 限制文件数量
    if (m_selectedFiles.size() > MAX_FILES) {
        m_selectedFiles = m_selectedFiles.mid(0, MAX_FILES);
        QMessageBox::warning(this, "警告", QString("文件数量过多，已限制为 %1 个文件").arg(MAX_FILES));
    }
    
    updateFileList();
}

void FileHashCalculation::updateFileList()
{
    QStringList displayList;
    for (const QString &file : m_selectedFiles) {
        QFileInfo fileInfo(file);
        displayList.append(QString("%1 (%2)").arg(fileInfo.fileName(), formatFileSize(fileInfo.size())));
    }
    
    fileListEdit->setPlainText(displayList.join('\n'));
    fileCountLabel->setText(QString("已选择 %1 个文件").arg(m_selectedFiles.size()));
}

void FileHashCalculation::clearResults()
{
    m_results.clear();
    resultTable->setRowCount(0);
    comparisonResultLabel->setText("等待比较...");
    comparisonResultLabel->setStyleSheet("QLabel { padding: 8px; border: 1px solid #ccc; border-radius: 4px; }");
}

void FileHashCalculation::startCalculation()
{
    if (m_calculating) return;
    
    m_calculating = true;
    calculateBtn->setEnabled(false);
    stopBtn->setEnabled(true);
    progressBar->setVisible(true);
    progressBar->setValue(0);
    progressBar->setMaximum(m_selectedFiles.size());
    
    clearResults();
    
    // 创建工作线程
    m_workerThread = new QThread(this);
    m_worker = new HashCalculationWorker;
    m_worker->moveToThread(m_workerThread);
    
    // 设置参数
    m_worker->setFiles(m_selectedFiles);
    
    QList<HashAlgorithm> algorithms;
    if (md5CheckBox->isChecked()) algorithms.append(HashAlgorithm::MD5);
    if (sha1CheckBox->isChecked()) algorithms.append(HashAlgorithm::SHA1);
    if (sha224CheckBox->isChecked()) algorithms.append(HashAlgorithm::SHA224);
    if (sha256CheckBox->isChecked()) algorithms.append(HashAlgorithm::SHA256);
    if (sha384CheckBox->isChecked()) algorithms.append(HashAlgorithm::SHA384);
    if (sha512CheckBox->isChecked()) algorithms.append(HashAlgorithm::SHA512);
    if (sha3_224CheckBox->isChecked()) algorithms.append(HashAlgorithm::SHA3_224);
    if (sha3_256CheckBox->isChecked()) algorithms.append(HashAlgorithm::SHA3_256);
    if (sha3_384CheckBox->isChecked()) algorithms.append(HashAlgorithm::SHA3_384);
    if (sha3_512CheckBox->isChecked()) algorithms.append(HashAlgorithm::SHA3_512);
    
    m_worker->setAlgorithms(algorithms);
    
    // 连接信号
    connect(m_workerThread, &QThread::started, m_worker, &HashCalculationWorker::calculateHashes);
    connect(m_worker, &HashCalculationWorker::progressUpdate, this, &FileHashCalculation::onProgressUpdate);
    connect(m_worker, &HashCalculationWorker::fileProcessed, this, &FileHashCalculation::onFileProcessed);
    connect(m_worker, &HashCalculationWorker::calculationFinished, this, &FileHashCalculation::onCalculationFinished);
    connect(m_worker, &HashCalculationWorker::errorOccurred, this, &FileHashCalculation::onErrorOccurred);
    
    // 开始计算
    m_startTime = QDateTime::currentDateTime();
    m_timer->start(100); // 每100ms更新一次时间显示
    m_workerThread->start();
    
    statusLabel->setText("正在计算哈希值...");
}

void FileHashCalculation::stopCalculation()
{
    if (!m_calculating) return;
    
    if (m_worker) {
        m_worker->setStopFlag(true);
    }
    
    statusLabel->setText("正在停止计算...");
}

void FileHashCalculation::updateProgress(int current, int total)
{
    progressBar->setValue(current);
    statusLabel->setText(QString("正在处理: %1/%2").arg(current).arg(total));
}

void FileHashCalculation::addResultToTable(const HashResult &result)
{
    int row = resultTable->rowCount();
    resultTable->insertRow(row);
    
    resultTable->setItem(row, 0, new QTableWidgetItem(result.fileName));
    resultTable->setItem(row, 1, new QTableWidgetItem(formatFileSize(result.fileSize)));
    resultTable->setItem(row, 2, new QTableWidgetItem(result.md5));
    resultTable->setItem(row, 3, new QTableWidgetItem(result.sha1));
    resultTable->setItem(row, 4, new QTableWidgetItem(result.sha256));
    resultTable->setItem(row, 5, new QTableWidgetItem(result.sha512));
    resultTable->setItem(row, 6, new QTableWidgetItem(result.isValid ? "成功" : result.errorMessage));
    
    // 设置行颜色
    if (!result.isValid) {
        for (int col = 0; col < resultTable->columnCount(); ++col) {
            QTableWidgetItem *item = resultTable->item(row, col);
            if (item) {
                item->setBackground(QColor(255, 200, 200)); // 浅红色背景
            }
        }
    }
}

QStringList FileHashCalculation::getSelectedAlgorithms()
{
    QStringList algorithms;
    if (md5CheckBox->isChecked()) algorithms.append("MD5");
    if (sha1CheckBox->isChecked()) algorithms.append("SHA1");
    if (sha224CheckBox->isChecked()) algorithms.append("SHA224");
    if (sha256CheckBox->isChecked()) algorithms.append("SHA256");
    if (sha384CheckBox->isChecked()) algorithms.append("SHA384");
    if (sha512CheckBox->isChecked()) algorithms.append("SHA512");
    if (sha3_224CheckBox->isChecked()) algorithms.append("SHA3-224");
    if (sha3_256CheckBox->isChecked()) algorithms.append("SHA3-256");
    if (sha3_384CheckBox->isChecked()) algorithms.append("SHA3-384");
    if (sha3_512CheckBox->isChecked()) algorithms.append("SHA3-512");
    return algorithms;
}

QString FileHashCalculation::formatFileSize(qint64 size)
{
    const qint64 KB = 1024;
    const qint64 MB = KB * 1024;
    const qint64 GB = MB * 1024;
    
    if (size >= GB) {
        return QString::number(size / (double)GB, 'f', 2) + " GB";
    } else if (size >= MB) {
        return QString::number(size / (double)MB, 'f', 2) + " MB";
    } else if (size >= KB) {
        return QString::number(size / (double)KB, 'f', 2) + " KB";
    } else {
        return QString::number(size) + " B";
    }
}

QString FileHashCalculation::formatDuration(qint64 ms)
{
    qint64 seconds = ms / 1000;
    qint64 minutes = seconds / 60;
    seconds %= 60;
    
    return QString("%1:%2").arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
}

void FileHashCalculation::saveResultsToFile()
{
    if (m_results.isEmpty()) {
        QMessageBox::information(this, "提示", "没有结果可保存");
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(this,
        "保存哈希计算结果",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/hash_results.csv",
        "CSV文件 (*.csv);;文本文件 (*.txt)");
    
    if (fileName.isEmpty()) return;
    
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "错误", "无法创建文件");
        return;
    }
    
    QTextStream out(&file);
    
    // 写入标题行
    if (fileName.endsWith(".csv")) {
        out << "文件名,文件路径,文件大小,MD5,SHA1,SHA256,SHA512,修改时间,状态\n";
        
        // 写入数据
        for (const HashResult &result : m_results) {
            out << QString("\"%1\",\"%2\",\"%3\",\"%4\",\"%5\",\"%6\",\"%7\",\"%8\",\"%9\"\n")
                   .arg(result.fileName)
                   .arg(result.filePath)
                   .arg(formatFileSize(result.fileSize))
                   .arg(result.md5)
                   .arg(result.sha1)
                   .arg(result.sha256)
                   .arg(result.sha512)
                   .arg(result.modifyTime.toString("yyyy-MM-dd hh:mm:ss"))
                   .arg(result.isValid ? "成功" : result.errorMessage);
        }
    } else {
        // 文本格式
        out << "文件哈希计算结果报告\n";
        out << "生成时间: " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "\n";
        out << "文件总数: " << m_results.size() << "\n\n";
        
        for (int i = 0; i < m_results.size(); ++i) {
            const HashResult &result = m_results[i];
            out << QString("文件 %1:\n").arg(i + 1);
            out << "  文件名: " << result.fileName << "\n";
            out << "  路径: " << result.filePath << "\n";
            out << "  大小: " << formatFileSize(result.fileSize) << "\n";
            if (!result.md5.isEmpty()) out << "  MD5: " << result.md5 << "\n";
            if (!result.sha1.isEmpty()) out << "  SHA1: " << result.sha1 << "\n";
            if (!result.sha256.isEmpty()) out << "  SHA256: " << result.sha256 << "\n";
            if (!result.sha512.isEmpty()) out << "  SHA512: " << result.sha512 << "\n";
            out << "  修改时间: " << result.modifyTime.toString("yyyy-MM-dd hh:mm:ss") << "\n";
            out << "  状态: " << (result.isValid ? "成功" : result.errorMessage) << "\n\n";
        }
    }
    
    statusLabel->setText(QString("结果已保存到: %1").arg(fileName));
}

void FileHashCalculation::copyResultsToClipboard()
{
    if (m_results.isEmpty()) {
        QMessageBox::information(this, "提示", "没有结果可复制");
        return;
    }
    
    QString text;
    for (const HashResult &result : m_results) {
        text += QString("%1\t%2\t%3\t%4\t%5\t%6\n")
                .arg(result.fileName)
                .arg(formatFileSize(result.fileSize))
                .arg(result.md5)
                .arg(result.sha1)
                .arg(result.sha256)
                .arg(result.sha512);
    }
    
    QApplication::clipboard()->setText(text);
    statusLabel->setText("结果已复制到剪贴板");
}

void FileHashCalculation::compareHashValues()
{
    QString expectedHash = expectedHashEdit->text().trimmed().toUpper();
    if (expectedHash.isEmpty()) {
        comparisonResultLabel->setText("请输入要比较的哈希值");
        comparisonResultLabel->setStyleSheet("QLabel { padding: 8px; border: 1px solid #orange; border-radius: 4px; color: orange; }");
        return;
    }
    
    QString hashType = hashTypeCombo->currentText();
    bool found = false;
    
    for (const HashResult &result : m_results) {
        QString actualHash;
        if (hashType == "MD5") actualHash = result.md5;
        else if (hashType == "SHA1") actualHash = result.sha1;
        else if (hashType == "SHA256") actualHash = result.sha256;
        else if (hashType == "SHA512") actualHash = result.sha512;
        
        if (actualHash.toUpper() == expectedHash) {
            comparisonResultLabel->setText(QString("匹配成功！\n文件: %1\n%2: %3")
                                           .arg(result.fileName)
                                           .arg(hashType)
                                           .arg(actualHash));
            comparisonResultLabel->setStyleSheet("QLabel { padding: 8px; border: 1px solid #green; border-radius: 4px; color: green; }");
            found = true;
            break;
        }
    }
    
    if (!found) {
        comparisonResultLabel->setText(QString("未找到匹配的%1哈希值").arg(hashType));
        comparisonResultLabel->setStyleSheet("QLabel { padding: 8px; border: 1px solid #red; border-radius: 4px; color: red; }");
    }
}

void FileHashCalculation::verifyHashValue()
{
    // 验证功能与比较类似，但会显示更详细的信息
    compareHashValues();
}

#include "filehashcalculation.moc"

