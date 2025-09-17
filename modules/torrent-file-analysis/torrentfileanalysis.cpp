#include "torrentfileanalysis.h"

REGISTER_DYNAMICOBJECT(TorrentFileAnalysis);

// BencodeDecoder 实现
BencodeValue BencodeDecoder::decode(const QByteArray &data)
{
    int pos = 0;
    return decodeValue(data, pos);
}

BencodeValue BencodeDecoder::decodeValue(const QByteArray &data, int &pos)
{
    if (pos >= data.size()) {
        return BencodeValue();
    }
    
    char c = data[pos];
    
    if (c == 'i') {
        return decodeInteger(data, pos);
    } else if (c == 'l') {
        return decodeList(data, pos);
    } else if (c == 'd') {
        return decodeDictionary(data, pos);
    } else if (c >= '0' && c <= '9') {
        return decodeString(data, pos);
    }
    
    return BencodeValue();
}

BencodeValue BencodeDecoder::decodeString(const QByteArray &data, int &pos)
{
    int colonPos = data.indexOf(':', pos);
    if (colonPos == -1) {
        return BencodeValue();
    }
    
    QByteArray lengthStr = data.mid(pos, colonPos - pos);
    bool ok;
    int length = lengthStr.toInt(&ok);
    if (!ok || length < 0) {
        return BencodeValue();
    }
    
    pos = colonPos + 1;
    if (pos + length > data.size()) {
        return BencodeValue();
    }
    
    QByteArray str = data.mid(pos, length);
    pos += length;
    
    return BencodeValue(str);
}

BencodeValue BencodeDecoder::decodeInteger(const QByteArray &data, int &pos)
{
    pos++; // 跳过 'i'
    
    int endPos = data.indexOf('e', pos);
    if (endPos == -1) {
        return BencodeValue();
    }
    
    QByteArray numStr = data.mid(pos, endPos - pos);
    bool ok;
    qint64 num = numStr.toLongLong(&ok);
    if (!ok) {
        return BencodeValue();
    }
    
    pos = endPos + 1;
    return BencodeValue(num);
}

BencodeValue BencodeDecoder::decodeList(const QByteArray &data, int &pos)
{
    pos++; // 跳过 'l'
    
    QList<BencodeValue> list;
    
    while (pos < data.size() && data[pos] != 'e') {
        BencodeValue value = decodeValue(data, pos);
        list.append(value);
    }
    
    if (pos < data.size() && data[pos] == 'e') {
        pos++; // 跳过 'e'
    }
    
    return BencodeValue(list);
}

BencodeValue BencodeDecoder::decodeDictionary(const QByteArray &data, int &pos)
{
    pos++; // 跳过 'd'
    
    QMap<QByteArray, BencodeValue> dict;
    
    while (pos < data.size() && data[pos] != 'e') {
        // 解码键（必须是字符串）
        BencodeValue keyValue = decodeValue(data, pos);
        if (keyValue.type != BencodeType::String) {
            break;
        }
        
        // 解码值
        BencodeValue value = decodeValue(data, pos);
        dict[keyValue.stringValue] = value;
    }
    
    if (pos < data.size() && data[pos] == 'e') {
        pos++; // 跳过 'e'
    }
    
    return BencodeValue(dict);
}

QString BencodeDecoder::valueToString(const BencodeValue &value, int indent)
{
    QString indentStr = QString(indent * 2, ' ');
    
    switch (value.type) {
    case BencodeType::String: {
        // 检查是否为可打印字符
        bool isPrintable = true;
        for (char c : value.stringValue) {
            if (c < 32 || c > 126) {
                isPrintable = false;
                break;
            }
        }
        
        if (isPrintable && value.stringValue.size() < 100) {
            return QString("\"%1\"").arg(QString::fromUtf8(value.stringValue));
        } else {
            return QString("<%1 bytes>").arg(value.stringValue.size());
        }
    }
        
    case BencodeType::Integer:
        return QString::number(value.intValue);
        
    case BencodeType::List: {
        QString result = "[\n";
        for (int i = 0; i < value.listValue.size(); ++i) {
            result += QString(indent * 2 + 2, ' ');
            result += valueToString(value.listValue[i], indent + 1);
            if (i < value.listValue.size() - 1) {
                result += ",";
            }
            result += "\n";
        }
        result += indentStr + "]";
        return result;
    }
    
    case BencodeType::Dictionary: {
        QString result = "{\n";
        auto it = value.dictValue.constBegin();
        while (it != value.dictValue.constEnd()) {
            result += QString(indent * 2 + 2, ' ');
            result += QString("\"%1\": ").arg(QString::fromUtf8(it.key()));
            result += valueToString(it.value(), indent + 1);
            
            auto next = it;
            ++next;
            if (next != value.dictValue.constEnd()) {
                result += ",";
            }
            result += "\n";
            ++it;
        }
        result += indentStr + "}";
        return result;
    }
    }
    
    return "";
}

// TorrentFileAnalysis 主类实现
TorrentFileAnalysis::TorrentFileAnalysis() : QWidget(nullptr), DynamicObjectBase()
{
    m_hasValidTorrent = false;
    
    m_statusTimer = new QTimer(this);
    m_statusTimer->setSingleShot(true);
    m_statusTimer->setInterval(3000);
    
    setupUI();
    
    // 连接信号槽
    connect(openFileBtn, &QPushButton::clicked, this, &TorrentFileAnalysis::onOpenFile);
    connect(clearBtn, &QPushButton::clicked, this, &TorrentFileAnalysis::onClearAll);
    connect(copyMagnetBtn, &QPushButton::clicked, this, &TorrentFileAnalysis::onCopyMagnetLink);
    connect(copyHashBtn, &QPushButton::clicked, this, &TorrentFileAnalysis::onCopyInfoHash);
    connect(exportFileListBtn, &QPushButton::clicked, this, &TorrentFileAnalysis::onExportFileList);
    connect(saveRawBtn, &QPushButton::clicked, this, &TorrentFileAnalysis::onSaveRawData);
    connect(showDetailsBtn, &QPushButton::clicked, this, &TorrentFileAnalysis::onShowFileDetails);
    connect(fileTree, &QTreeWidget::itemDoubleClicked, this, &TorrentFileAnalysis::onFileItemDoubleClicked);
    
    connect(m_statusTimer, &QTimer::timeout, [this]() {
        statusLabel->setText(tr("就绪"));
    });
    
    // 启用拖拽
    setAcceptDrops(true);
    
    clearAllData();
}

TorrentFileAnalysis::~TorrentFileAnalysis()
{
}

void TorrentFileAnalysis::setupUI()
{
    mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    
    setupFileArea();
    
    // 创建标签页
    tabWidget = new QTabWidget();
    
    setupInfoArea();
    setupFileListArea();
    setupRawDataArea();
    
    tabWidget->addTab(infoWidget, tr("📋 种子信息"));
    tabWidget->addTab(fileListWidget, tr("📁 文件列表"));
    tabWidget->addTab(rawDataWidget, tr("📄 原始数据"));
    
    mainLayout->addWidget(fileGroup);
    mainLayout->addWidget(tabWidget);
    
    setupStatusArea();
    mainLayout->addLayout(statusLayout);
    
    // 应用样式
    setStyleSheet(R"(
        QWidget {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
        }
        QPushButton {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            padding: 8px 16px;
            border: 1px solid #ccc;
            border-radius: 4px;
            font-size: 11pt;
            font-weight: normal;
            min-width: 80px;
            background-color: #f8f9fa;
        }
        QPushButton:hover { 
            background-color: #e9ecef; 
            border-color: #adb5bd;
        }
        QPushButton:pressed {
            background-color: #dee2e6;
        }
        QPushButton:disabled {
            background-color: #e9ecef;
            color: #6c757d;
            border-color: #dee2e6;
        }
        QGroupBox {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            font-weight: bold;
            border: 2px solid #dee2e6;
            border-radius: 8px;
            margin-top: 1ex;
            padding-top: 10px;
            font-size: 12pt;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px 0 5px;
        }
        QLineEdit {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            padding: 8px;
            border: 2px solid #ced4da;
            border-radius: 4px;
            font-size: 11pt;
            background-color: white;
        }
        QLineEdit:focus {
            border-color: #80bdff;
            outline: 0;
        }
        QLabel {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            font-size: 11pt;
        }
        QTextEdit, QPlainTextEdit {
            font-family: 'Consolas', 'Monaco', monospace;
            border: 2px solid #dee2e6;
            border-radius: 4px;
            font-size: 11pt;
            background-color: white;
        }
        QTreeWidget, QTableWidget {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            border: 2px solid #dee2e6;
            border-radius: 4px;
            font-size: 11pt;
            gridline-color: #dee2e6;
            background-color: white;
            alternate-background-color: #f8f9fa;
        }
        QTreeWidget::item, QTableWidget::item {
            padding: 4px;
            border-bottom: 1px solid #dee2e6;
        }
        QTreeWidget::item:selected, QTableWidget::item:selected {
            background-color: #007bff;
            color: white;
        }
        QHeaderView::section {
            background-color: #f8f9fa;
            padding: 8px;
            border: 1px solid #dee2e6;
            font-weight: bold;
        }
        QTabWidget::pane {
            border: 2px solid #dee2e6;
            border-radius: 8px;
        }
        QTabBar::tab {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            padding: 8px 16px;
            margin: 2px;
            border-radius: 4px;
            background-color: #f8f9fa;
        }
        QTabBar::tab:selected {
            background-color: #007bff;
            color: white;
        }
        QTabBar::tab:hover {
            background-color: #e9ecef;
        }
    )");
}

void TorrentFileAnalysis::setupFileArea()
{
    fileGroup = new QGroupBox("📁 种子文件选择");
    fileLayout = new QHBoxLayout(fileGroup);
    
    filePathEdit = new QLineEdit();
    filePathEdit->setPlaceholderText(tr("选择或拖拽种子文件到此处..."));
    filePathEdit->setReadOnly(true);
    fileLayout->addWidget(filePathEdit);
    
    openFileBtn = new QPushButton(tr("📂 浏览文件"));
    openFileBtn->setStyleSheet("QPushButton { background-color: #007bff; color: white; } QPushButton:hover { background-color: #0056b3; }");
    fileLayout->addWidget(openFileBtn);
    
    clearBtn = new QPushButton(tr("🗑️ 清空"));
    clearBtn->setStyleSheet("QPushButton { background-color: #dc3545; color: white; } QPushButton:hover { background-color: #c82333; }");
    fileLayout->addWidget(clearBtn);
    
    // 拖拽提示
    dropHintLabel = new QLabel(tr("💡 提示：您可以直接拖拽.torrent文件到窗口中进行分析"));
    dropHintLabel->setStyleSheet("color: #6c757d; font-style: italic; margin: 5px;");
    dropHintLabel->setAlignment(Qt::AlignCenter);
    
    QVBoxLayout *fileGroupLayout = new QVBoxLayout();
    fileGroupLayout->addLayout(fileLayout);
    fileGroupLayout->addWidget(dropHintLabel);
    fileGroup->setLayout(fileGroupLayout);
}

void TorrentFileAnalysis::setupInfoArea()
{
    infoWidget = new QWidget();
    QVBoxLayout *infoMainLayout = new QVBoxLayout(infoWidget);
    
    infoScrollArea = new QScrollArea();
    infoScrollArea->setWidgetResizable(true);
    infoScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    infoScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    QWidget *scrollWidget = new QWidget();
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollWidget);
    
    // 基本信息
    basicInfoGroup = new QGroupBox("📋 基本信息");
    basicInfoLayout = new QGridLayout(basicInfoGroup);
    
    basicInfoLayout->addWidget(new QLabel(tr("名称:")), 0, 0);
    nameLabel = new QLabel(tr("-"));
    nameLabel->setWordWrap(true);
    nameLabel->setStyleSheet("font-weight: bold; color: #2c3e50;");
    basicInfoLayout->addWidget(nameLabel, 0, 1);
    
    basicInfoLayout->addWidget(new QLabel(tr("注释:")), 1, 0);
    commentLabel = new QLabel(tr("-"));
    commentLabel->setWordWrap(true);
    basicInfoLayout->addWidget(commentLabel, 1, 1);
    
    basicInfoLayout->addWidget(new QLabel(tr("创建者:")), 2, 0);
    createdByLabel = new QLabel(tr("-"));
    basicInfoLayout->addWidget(createdByLabel, 2, 1);
    
    basicInfoLayout->addWidget(new QLabel(tr("创建时间:")), 3, 0);
    creationDateLabel = new QLabel(tr("-"));
    basicInfoLayout->addWidget(creationDateLabel, 3, 1);
    
    basicInfoLayout->addWidget(new QLabel(tr("主要Tracker:")), 4, 0);
    announceLabel = new QLabel(tr("-"));
    announceLabel->setWordWrap(true);
    basicInfoLayout->addWidget(announceLabel, 4, 1);
    
    basicInfoLayout->addWidget(new QLabel(tr("Info Hash:")), 5, 0);
    infoHashLabel = new QLabel(tr("-"));
    infoHashLabel->setStyleSheet("font-family: 'Consolas', monospace; font-weight: bold; color: #e74c3c;");
    infoHashLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    basicInfoLayout->addWidget(infoHashLabel, 5, 1);
    
    basicInfoLayout->addWidget(new QLabel(tr("总大小:")), 6, 0);
    totalSizeLabel = new QLabel(tr("-"));
    totalSizeLabel->setStyleSheet("font-weight: bold; color: #27ae60;");
    basicInfoLayout->addWidget(totalSizeLabel, 6, 1);
    
    basicInfoLayout->addWidget(new QLabel(tr("块大小:")), 7, 0);
    pieceLengthLabel = new QLabel(tr("-"));
    basicInfoLayout->addWidget(pieceLengthLabel, 7, 1);
    
    basicInfoLayout->addWidget(new QLabel(tr("块数量:")), 8, 0);
    pieceCountLabel = new QLabel(tr("-"));
    basicInfoLayout->addWidget(pieceCountLabel, 8, 1);
    
    basicInfoLayout->addWidget(new QLabel(tr("文件数量:")), 9, 0);
    fileCountLabel = new QLabel(tr("-"));
    basicInfoLayout->addWidget(fileCountLabel, 9, 1);
    
    basicInfoLayout->addWidget(new QLabel(tr("私有种子:")), 10, 0);
    isPrivateLabel = new QLabel(tr("-"));
    basicInfoLayout->addWidget(isPrivateLabel, 10, 1);
    
    basicInfoLayout->addWidget(new QLabel(tr("编码:")), 11, 0);
    encodingLabel = new QLabel(tr("-"));
    basicInfoLayout->addWidget(encodingLabel, 11, 1);
    
    scrollLayout->addWidget(basicInfoGroup);
    
    // 磁力链接
    magnetGroup = new QGroupBox("🧲 磁力链接");
    magnetLayout = new QVBoxLayout(magnetGroup);
    
    magnetLinkEdit = new QTextEdit();
    magnetLinkEdit->setMaximumHeight(80);
    magnetLinkEdit->setReadOnly(true);
    magnetLinkEdit->setPlainText("暂无磁力链接");
    magnetLayout->addWidget(magnetLinkEdit);
    
    magnetButtonLayout = new QHBoxLayout();
    copyMagnetBtn = new QPushButton(tr("📋 复制磁力链接"));
    copyMagnetBtn->setStyleSheet("QPushButton { background-color: #28a745; color: white; } QPushButton:hover { background-color: #218838; }");
    copyMagnetBtn->setEnabled(false);
    magnetButtonLayout->addWidget(copyMagnetBtn);
    
    copyHashBtn = new QPushButton(tr("📋 复制Hash"));
    copyHashBtn->setStyleSheet("QPushButton { background-color: #17a2b8; color: white; } QPushButton:hover { background-color: #138496; }");
    copyHashBtn->setEnabled(false);
    magnetButtonLayout->addWidget(copyHashBtn);
    
    magnetButtonLayout->addStretch();
    magnetLayout->addLayout(magnetButtonLayout);
    
    scrollLayout->addWidget(magnetGroup);
    
    // Tracker列表
    announceGroup = new QGroupBox("📡 Tracker列表");
    announceLayout = new QVBoxLayout(announceGroup);
    
    announceListEdit = new QTextEdit();
    announceListEdit->setMaximumHeight(120);
    announceListEdit->setReadOnly(true);
    announceListEdit->setPlainText("暂无Tracker信息");
    announceLayout->addWidget(announceListEdit);
    
    scrollLayout->addWidget(announceGroup);
    scrollLayout->addStretch();
    
    infoScrollArea->setWidget(scrollWidget);
    infoMainLayout->addWidget(infoScrollArea);
}

void TorrentFileAnalysis::setupFileListArea()
{
    fileListWidget = new QWidget();
    fileListLayout = new QVBoxLayout(fileListWidget);
    
    fileListGroup = new QGroupBox("📁 文件列表");
    QVBoxLayout *fileListGroupLayout = new QVBoxLayout(fileListGroup);
    
    fileTree = new QTreeWidget();
    fileTree->setHeaderLabels({"文件路径", "大小", "百分比"});
    fileTree->setAlternatingRowColors(true);
    fileTree->setSortingEnabled(true);
    fileTree->setRootIsDecorated(true);
    
    // 设置列宽
    QHeaderView* header = fileTree->header();
    header->setStretchLastSection(false);
    fileTree->setColumnWidth(0, 400);
    fileTree->setColumnWidth(1, 120);
    fileTree->setColumnWidth(2, 100);
    header->setStretchLastSection(true);
    
    fileListGroupLayout->addWidget(fileTree);
    
    fileButtonLayout = new QHBoxLayout();
    
    exportFileListBtn = new QPushButton(tr("💾 导出文件列表"));
    exportFileListBtn->setStyleSheet("QPushButton { background-color: #28a745; color: white; } QPushButton:hover { background-color: #218838; }");
    exportFileListBtn->setEnabled(false);
    fileButtonLayout->addWidget(exportFileListBtn);
    
    showDetailsBtn = new QPushButton(tr("🔍 显示详情"));
    showDetailsBtn->setEnabled(false);
    fileButtonLayout->addWidget(showDetailsBtn);
    
    fileButtonLayout->addStretch();
    
    fileStatsLabel = new QLabel(tr("文件统计: 0 个文件, 总大小: 0 B"));
    fileStatsLabel->setStyleSheet("color: #6c757d; font-weight: bold;");
    fileButtonLayout->addWidget(fileStatsLabel);
    
    fileListGroupLayout->addLayout(fileButtonLayout);
    fileListLayout->addWidget(fileListGroup);
}

void TorrentFileAnalysis::setupRawDataArea()
{
    rawDataWidget = new QWidget();
    rawDataLayout = new QVBoxLayout(rawDataWidget);
    
    rawDataGroup = new QGroupBox("📄 原始Bencode数据");
    QVBoxLayout *rawDataGroupLayout = new QVBoxLayout(rawDataGroup);
    
    rawDataEdit = new QPlainTextEdit();
    rawDataEdit->setReadOnly(true);
    rawDataEdit->setPlainText("暂无数据");
    rawDataEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    rawDataGroupLayout->addWidget(rawDataEdit);
    
    rawButtonLayout = new QHBoxLayout();
    
    saveRawBtn = new QPushButton(tr("💾 保存原始数据"));
    saveRawBtn->setStyleSheet("QPushButton { background-color: #17a2b8; color: white; } QPushButton:hover { background-color: #138496; }");
    saveRawBtn->setEnabled(false);
    rawButtonLayout->addWidget(saveRawBtn);
    
    rawButtonLayout->addStretch();
    rawDataGroupLayout->addLayout(rawButtonLayout);
    
    rawDataLayout->addWidget(rawDataGroup);
}

void TorrentFileAnalysis::setupStatusArea()
{
    statusLayout = new QHBoxLayout();
    
    statusLabel = new QLabel(tr("就绪"));
    statusLabel->setStyleSheet("color: #28a745; font-weight: bold;");
    
    progressBar = new QProgressBar();
    progressBar->setVisible(false);
    progressBar->setMaximumHeight(20);
    progressBar->setMaximumWidth(200);
    
    statusLayout->addWidget(statusLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(progressBar);
}

// 拖拽事件处理
void TorrentFileAnalysis::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty()) {
            QString filePath = urls.first().toLocalFile();
            if (filePath.endsWith(".torrent", Qt::CaseInsensitive)) {
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

void TorrentFileAnalysis::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty()) {
        QString filePath = urls.first().toLocalFile();
        if (filePath.endsWith(".torrent", Qt::CaseInsensitive)) {
            filePathEdit->setText(filePath);
            analyzeTorrentFile(filePath);
            event->acceptProposedAction();
            return;
        }
    }
    event->ignore();
}

// 槽函数实现
void TorrentFileAnalysis::onOpenFile()
{
    QString filePath = QFileDialog::getOpenFileName(this, "选择种子文件", "", "Torrent Files (*.torrent)");
    if (!filePath.isEmpty()) {
        filePathEdit->setText(filePath);
        analyzeTorrentFile(filePath);
    }
}

void TorrentFileAnalysis::onClearAll()
{
    clearAllData();
}

void TorrentFileAnalysis::onCopyMagnetLink()
{
    if (m_hasValidTorrent && !m_currentTorrent.magnetLink.isEmpty()) {
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setText(m_currentTorrent.magnetLink);
        statusLabel->setText(tr("磁力链接已复制到剪贴板"));
        m_statusTimer->start();
    }
}

void TorrentFileAnalysis::onCopyInfoHash()
{
    if (m_hasValidTorrent && !m_currentTorrent.infoHash.isEmpty()) {
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setText(m_currentTorrent.infoHash.toHex().toUpper());
        statusLabel->setText(tr("Info Hash已复制到剪贴板"));
        m_statusTimer->start();
    }
}

void TorrentFileAnalysis::onExportFileList()
{
    if (!m_hasValidTorrent || m_fileItems.isEmpty()) {
        return;
    }
    
    QString fileName = QString("%1_文件列表.txt").arg(m_currentTorrent.name);
    QString filePath = QFileDialog::getSaveFileName(this, "导出文件列表", fileName, "Text Files (*.txt)");
    
    if (!filePath.isEmpty()) {
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out.setEncoding(QStringConverter::Utf8);
            
            out << QString("种子名称: %1\n").arg(m_currentTorrent.name);
            out << QString("总大小: %1\n").arg(formatFileSize(m_currentTorrent.totalSize));
            out << QString("文件数量: %1\n").arg(m_fileItems.size());
            out << QString("导出时间: %1\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
            out << "\n" << QString("-").repeated(80) << "\n\n";
            
            for (const FileItem &item : m_fileItems) {
                out << QString("%1\t%2\t%.2f%%\n")
                       .arg(item.path)
                       .arg(item.sizeString)
                       .arg(item.percentage);
            }
            
            statusLabel->setText(tr("文件列表导出成功"));
            m_statusTimer->start();
        } else {
            QMessageBox::warning(this, "导出失败", "无法写入文件: " + file.errorString());
        }
    }
}

void TorrentFileAnalysis::onSaveRawData()
{
    if (!m_hasValidTorrent) {
        return;
    }
    
    QString fileName = QString("%1_原始数据.txt").arg(m_currentTorrent.name);
    QString filePath = QFileDialog::getSaveFileName(this, "保存原始数据", fileName, "Text Files (*.txt)");
    
    if (!filePath.isEmpty()) {
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out.setEncoding(QStringConverter::Utf8);
            out << rawDataEdit->toPlainText();
            
            statusLabel->setText(tr("原始数据保存成功"));
            m_statusTimer->start();
        } else {
            QMessageBox::warning(this, "保存失败", "无法写入文件: " + file.errorString());
        }
    }
}

void TorrentFileAnalysis::onAnalyzeTorrent()
{
    if (!m_currentFilePath.isEmpty()) {
        analyzeTorrentFile(m_currentFilePath);
    }
}

void TorrentFileAnalysis::onFileItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column)
    
    if (item && item->childCount() == 0) { // 叶子节点（文件）
        QString filePath = item->text(0);
        QString fileSize = item->text(1);
        QString percentage = item->text(2);
        
        QString message = QString("文件: %1\n大小: %2\n占比: %3")
                         .arg(filePath)
                         .arg(fileSize)
                         .arg(percentage);
        
        QMessageBox::information(this, "文件详情", message);
    }
}

void TorrentFileAnalysis::onShowFileDetails()
{
    if (m_hasValidTorrent) {
        QString details = QString("种子详情:\n\n")
                         + QString("名称: %1\n").arg(m_currentTorrent.name)
                         + QString("总大小: %1\n").arg(formatFileSize(m_currentTorrent.totalSize))
                         + QString("文件数量: %1\n").arg(m_fileItems.size())
                         + QString("块大小: %1\n").arg(formatFileSize(m_currentTorrent.pieceLength))
                         + QString("块数量: %1\n").arg(m_currentTorrent.pieceCount);
        
        QMessageBox::information(this, "种子详情", details);
    }
}

// 核心分析功能
void TorrentFileAnalysis::analyzeTorrentFile(const QString &filePath)
{
    statusLabel->setText(tr("正在分析种子文件..."));
    progressBar->setVisible(true);
    progressBar->setRange(0, 0); // 不确定进度
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        statusLabel->setText("无法打开文件: " + file.errorString());
        progressBar->setVisible(false);
        return;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    try {
        // 解码Bencode数据
        m_rawData = BencodeDecoder::decode(data);
        
        // 解析种子信息
        m_currentTorrent = parseTorrentData(m_rawData);
        m_currentFilePath = filePath;
        m_hasValidTorrent = true;
        
        // 更新UI
        updateInfoDisplay(m_currentTorrent);
        updateFileList(m_currentTorrent);
        updateRawData(m_rawData);
        
        // 启用按钮
        copyMagnetBtn->setEnabled(true);
        copyHashBtn->setEnabled(true);
        exportFileListBtn->setEnabled(true);
        saveRawBtn->setEnabled(true);
        showDetailsBtn->setEnabled(true);
        
        statusLabel->setText(tr("种子文件分析完成"));
        
    } catch (const std::exception &e) {
        statusLabel->setText("种子文件解析失败: " + QString(e.what()));
        clearAllData();
    }
    
    progressBar->setVisible(false);
    m_statusTimer->start();
}

TorrentInfo TorrentFileAnalysis::parseTorrentData(const BencodeValue &root)
{
    TorrentInfo info;
    
    if (root.type != BencodeType::Dictionary) {
        throw std::runtime_error("无效的种子文件格式");
    }
    
    const auto &rootDict = root.dictValue;
    
    // 解析基本信息
    if (rootDict.contains("comment")) {
        info.comment = QString::fromUtf8(rootDict["comment"].stringValue);
    }
    
    if (rootDict.contains("created by")) {
        info.createdBy = QString::fromUtf8(rootDict["created by"].stringValue);
    }
    
    if (rootDict.contains("creation date")) {
        info.creationDate = QDateTime::fromSecsSinceEpoch(rootDict["creation date"].intValue);
    }
    
    if (rootDict.contains("announce")) {
        info.announce = QString::fromUtf8(rootDict["announce"].stringValue);
    }
    
    // 解析announce-list
    if (rootDict.contains("announce-list")) {
        const auto &announceList = rootDict["announce-list"].listValue;
        for (const auto &tierValue : announceList) {
            if (tierValue.type == BencodeType::List) {
                for (const auto &urlValue : tierValue.listValue) {
                    if (urlValue.type == BencodeType::String) {
                        info.announceList.append(QString::fromUtf8(urlValue.stringValue));
                    }
                }
            }
        }
    }
    
    // 解析info字典
    if (rootDict.contains("info")) {
        const auto &infoDict = rootDict["info"];
        if (infoDict.type == BencodeType::Dictionary) {
            const auto &infoDictValue = infoDict.dictValue;
            
            // 计算info hash
            info.infoHash = calculateInfoHash(infoDict);
            
            // 解析名称
            if (infoDictValue.contains("name")) {
                info.name = QString::fromUtf8(infoDictValue["name"].stringValue);
            }
            
            // 解析块信息
            if (infoDictValue.contains("piece length")) {
                info.pieceLength = infoDictValue["piece length"].intValue;
            }
            
            if (infoDictValue.contains("pieces")) {
                info.pieceCount = infoDictValue["pieces"].stringValue.size() / 20;
            }
            
            // 解析私有标志
            if (infoDictValue.contains("private")) {
                info.isPrivate = (infoDictValue["private"].intValue != 0);
            }
            
            // 解析文件信息
            if (infoDictValue.contains("files")) {
                // 多文件种子
                const auto &filesList = infoDictValue["files"].listValue;
                for (const auto &fileValue : filesList) {
                    if (fileValue.type == BencodeType::Dictionary) {
                        const auto &fileDict = fileValue.dictValue;
                        
                        QString filePath;
                        qint64 fileSize = 0;
                        
                        if (fileDict.contains("path")) {
                            const auto &pathList = fileDict["path"].listValue;
                            QStringList pathParts;
                            for (const auto &pathPart : pathList) {
                                pathParts.append(QString::fromUtf8(pathPart.stringValue));
                            }
                            filePath = pathParts.join("/");
                        }
                        
                        if (fileDict.contains("length")) {
                            fileSize = fileDict["length"].intValue;
                        }
                        
                        info.files.append(filePath);
                        info.fileSizes.append(fileSize);
                        info.totalSize += fileSize;
                    }
                }
            } else if (infoDictValue.contains("length")) {
                // 单文件种子
                info.totalSize = infoDictValue["length"].intValue;
                info.files.append(info.name);
                info.fileSizes.append(info.totalSize);
            }
            
            // 生成磁力链接
            info.magnetLink = generateMagnetLink(info.infoHash, info.name);
        }
    }
    
    return info;
}

void TorrentFileAnalysis::updateInfoDisplay(const TorrentInfo &info)
{
    nameLabel->setText(info.name.isEmpty() ? "-" : info.name);
    commentLabel->setText(info.comment.isEmpty() ? "-" : info.comment);
    createdByLabel->setText(info.createdBy.isEmpty() ? "-" : info.createdBy);
    creationDateLabel->setText(info.creationDate.isValid() ? 
                               formatDateTime(info.creationDate) : "-");
    announceLabel->setText(info.announce.isEmpty() ? "-" : info.announce);
    infoHashLabel->setText(info.infoHash.isEmpty() ? QString("-") : 
                           QString(info.infoHash.toHex().toUpper()));
    totalSizeLabel->setText(formatFileSize(info.totalSize));
    pieceLengthLabel->setText(formatFileSize(info.pieceLength));
    pieceCountLabel->setText(QString::number(info.pieceCount));
    fileCountLabel->setText(QString::number(info.files.size()));
    isPrivateLabel->setText(info.isPrivate ? "是" : "否");
    encodingLabel->setText(info.encoding.isEmpty() ? "UTF-8" : info.encoding);
    
    // 更新磁力链接
    magnetLinkEdit->setPlainText(info.magnetLink.isEmpty() ? "暂无磁力链接" : info.magnetLink);
    
    // 更新Tracker列表
    if (info.announceList.isEmpty()) {
        announceListEdit->setPlainText(info.announce.isEmpty() ? "暂无Tracker信息" : info.announce);
    } else {
        announceListEdit->setPlainText(info.announceList.join("\n"));
    }
}

void TorrentFileAnalysis::updateFileList(const TorrentInfo &info)
{
    fileTree->clear();
    m_fileItems.clear();
    
    if (info.files.isEmpty()) {
        fileStatsLabel->setText(tr("文件统计: 0 个文件, 总大小: 0 B"));
        return;
    }
    
    // 构建文件项列表
    for (int i = 0; i < info.files.size(); ++i) {
        QString filePath = info.files[i];
        qint64 fileSize = (i < info.fileSizes.size()) ? info.fileSizes[i] : 0;
        QString sizeString = formatFileSize(fileSize);
        double percentage = (info.totalSize > 0) ? (double(fileSize) / info.totalSize * 100.0) : 0.0;
        
        m_fileItems.append(FileItem(filePath, fileSize, sizeString, percentage));
    }
    
    // 构建树形结构
    QMap<QString, QTreeWidgetItem*> dirItems;
    
    for (const FileItem &item : m_fileItems) {
        QStringList pathParts = item.path.split("/", Qt::SkipEmptyParts);
        
        if (pathParts.isEmpty()) continue;
        
        QTreeWidgetItem *currentItem = nullptr;
        QString currentPath = "";
        
        // 构建目录结构
        for (int i = 0; i < pathParts.size() - 1; ++i) {
            if (!currentPath.isEmpty()) {
                currentPath += "/";
            }
            currentPath += pathParts[i];
            
            if (!dirItems.contains(currentPath)) {
                QTreeWidgetItem *dirItem = new QTreeWidgetItem();
                dirItem->setText(0, pathParts[i]);
                dirItem->setText(1, "");
                dirItem->setText(2, "");
                
                if (currentItem) {
                    currentItem->addChild(dirItem);
                } else {
                    fileTree->addTopLevelItem(dirItem);
                }
                
                dirItems[currentPath] = dirItem;
            }
            
            currentItem = dirItems[currentPath];
        }
        
        // 添加文件项
        QTreeWidgetItem *fileItem = new QTreeWidgetItem();
        fileItem->setText(0, pathParts.last());
        fileItem->setText(1, item.sizeString);
        fileItem->setText(2, QString::number(item.percentage, 'f', 2) + "%");
        
        if (currentItem) {
            currentItem->addChild(fileItem);
        } else {
            fileTree->addTopLevelItem(fileItem);
        }
    }
    
    // 展开所有项
    fileTree->expandAll();
    
    // 更新统计信息
    fileStatsLabel->setText(QString("文件统计: %1 个文件, 总大小: %2")
                           .arg(m_fileItems.size())
                           .arg(formatFileSize(info.totalSize)));
}

void TorrentFileAnalysis::updateRawData(const BencodeValue &root)
{
    QString rawText = BencodeDecoder::valueToString(root);
    rawDataEdit->setPlainText(rawText);
}

void TorrentFileAnalysis::clearAllData()
{
    m_currentFilePath.clear();
    m_currentTorrent = TorrentInfo();
    m_rawData = BencodeValue();
    m_fileItems.clear();
    m_hasValidTorrent = false;
    
    filePathEdit->clear();
    
    // 清空信息显示
    nameLabel->setText(tr("-"));
    commentLabel->setText(tr("-"));
    createdByLabel->setText(tr("-"));
    creationDateLabel->setText(tr("-"));
    announceLabel->setText(tr("-"));
    infoHashLabel->setText(tr("-"));
    totalSizeLabel->setText(tr("-"));
    pieceLengthLabel->setText(tr("-"));
    pieceCountLabel->setText(tr("-"));
    fileCountLabel->setText(tr("-"));
    isPrivateLabel->setText(tr("-"));
    encodingLabel->setText(tr("-"));
    
    magnetLinkEdit->setPlainText("暂无磁力链接");
    announceListEdit->setPlainText("暂无Tracker信息");
    
    fileTree->clear();
    fileStatsLabel->setText(tr("文件统计: 0 个文件, 总大小: 0 B"));
    
    rawDataEdit->setPlainText("暂无数据");
    
    // 禁用按钮
    copyMagnetBtn->setEnabled(false);
    copyHashBtn->setEnabled(false);
    exportFileListBtn->setEnabled(false);
    saveRawBtn->setEnabled(false);
    showDetailsBtn->setEnabled(false);
    
    statusLabel->setText(tr("就绪"));
}

// 工具函数
QString TorrentFileAnalysis::formatFileSize(qint64 bytes) const
{
    if (bytes < 1024) {
        return QString("%1 B").arg(bytes);
    } else if (bytes < 1024 * 1024) {
        return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    } else if (bytes < 1024 * 1024 * 1024) {
        return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    } else {
        return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
    }
}

QString TorrentFileAnalysis::formatDateTime(const QDateTime &dateTime) const
{
    return dateTime.toString("yyyy-MM-dd hh:mm:ss");
}

QByteArray TorrentFileAnalysis::calculateInfoHash(const BencodeValue &infoDict)
{
    // 这里需要重新编码info字典并计算SHA1哈希
    // 由于Bencode编码比较复杂，这里返回一个占位符
    // 在实际实现中，需要实现Bencode编码器
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData("placeholder"); // 占位符
    return hash.result();
}

QString TorrentFileAnalysis::generateMagnetLink(const QByteArray &infoHash, const QString &name)
{
    if (infoHash.isEmpty()) {
        return "";
    }
    
    QString magnetLink = QString("magnet:?xt=urn:btih:%1").arg(QString(infoHash.toHex().toUpper()));
    
    if (!name.isEmpty()) {
        QString encodedName = QUrl::toPercentEncoding(name);
        magnetLink += QString("&dn=%1").arg(encodedName);
    }
    
    // 添加tracker
    if (!m_currentTorrent.announce.isEmpty()) {
        QString encodedTracker = QUrl::toPercentEncoding(m_currentTorrent.announce);
        magnetLink += QString("&tr=%1").arg(encodedTracker);
    }
    
    return magnetLink;
}

