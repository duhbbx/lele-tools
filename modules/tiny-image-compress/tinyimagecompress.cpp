#include "tinyimagecompress.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QBuffer>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QPainter>
#include <QApplication>
#include <QHeaderView>
#include <QDesktopServices>

REGISTER_DYNAMICOBJECT(TinyImageCompress);

TinyImageCompress::TinyImageCompress() : QWidget(nullptr), DynamicObjectBase()
{
    setAcceptDrops(true);
    setupUI();
}

void TinyImageCompress::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 6, 8, 6);
    mainLayout->setSpacing(4);

    setStyleSheet(R"(
        QPushButton { padding:4px 10px; border:none; border-radius:4px; font-size:9pt; color:#495057; background:transparent; }
        QPushButton:hover { background-color:#e9ecef; }
        QPushButton:pressed { background-color:#dee2e6; }
        QPushButton:disabled { color:#adb5bd; }
        QLabel { font-size:9pt; color:#495057; }
        QTableWidget { border:1px solid #dee2e6; border-radius:4px; font-size:9pt; gridline-color:#f1f3f5; selection-background-color:#e9ecef; selection-color:#212529; }
        QHeaderView::section { background-color:#f8f9fa; border:none; border-bottom:1px solid #dee2e6; border-right:1px solid #f1f3f5; padding:4px 8px; font-size:9pt; color:#495057; }
        QSlider::groove:horizontal { height:4px; background:#dee2e6; border-radius:2px; }
        QSlider::handle:horizontal { width:14px; height:14px; margin:-5px 0; background:#228be6; border-radius:7px; }
        QSlider::sub-page:horizontal { background:#228be6; border-radius:2px; }
    )");

    // 工具栏 第一行：文件操作
    auto* toolbar1 = new QHBoxLayout();
    toolbar1->setSpacing(2);

    m_addFilesBtn = new QPushButton(tr("添加图片"));
    m_addFolderBtn = new QPushButton(tr("添加目录"));
    m_removeBtn = new QPushButton(tr("移除"));
    m_clearBtn = new QPushButton(tr("清空"));

    toolbar1->addWidget(m_addFilesBtn);
    toolbar1->addWidget(m_addFolderBtn);
    toolbar1->addWidget(m_removeBtn);
    toolbar1->addWidget(m_clearBtn);
    toolbar1->addStretch();
    mainLayout->addLayout(toolbar1);

    // 参数行：质量滑块 + 格式 + 压缩按钮
    auto* paramRow = new QHBoxLayout();
    paramRow->setSpacing(6);

    auto* qualityTitle = new QLabel(tr("压缩质量:"));
    m_qualitySlider = new QSlider(Qt::Horizontal);
    m_qualitySlider->setRange(1, 100);
    m_qualitySlider->setValue(75);
    m_qualitySlider->setFixedWidth(160);

    m_qualitySpin = new QSpinBox();
    m_qualitySpin->setRange(1, 100);
    m_qualitySpin->setValue(75);
    m_qualitySpin->setSuffix("%");
    m_qualitySpin->setFixedWidth(65);

    m_qualityLabel = new QLabel(tr("(越低压缩率越高，推荐 60-80)"));
    m_qualityLabel->setStyleSheet("color:#868e96; font-size:8pt;");

    m_formatCombo = new QComboBox();
    m_formatCombo->addItems({"保持原格式", "JPEG", "PNG", "WebP"});
    m_formatCombo->setFixedWidth(100);

    m_keepMetadataCheck = new QCheckBox(tr("保留EXIF"));
    m_keepMetadataCheck->setChecked(false);

    m_compressBtn = new QPushButton(tr("开始压缩"));
    m_compressBtn->setStyleSheet(
        "QPushButton { color:#228be6; font-weight:bold; }"
        "QPushButton:hover { background-color:#e7f5ff; }"
        "QPushButton:disabled { color:#adb5bd; font-weight:normal; }");
    m_compressBtn->setEnabled(false);

    paramRow->addWidget(qualityTitle);
    paramRow->addWidget(m_qualitySlider);
    paramRow->addWidget(m_qualitySpin);
    paramRow->addWidget(m_qualityLabel);
    paramRow->addWidget(m_formatCombo);
    paramRow->addWidget(m_keepMetadataCheck);
    paramRow->addStretch();
    paramRow->addWidget(m_compressBtn);
    mainLayout->addLayout(paramRow);

    // 结果表格
    m_table = new QTableWidget();
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({
        tr("文件名"), tr("尺寸"), tr("原始大小"), tr("压缩后"), tr("压缩率"), tr("状态"), tr("操作")
    });
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->verticalHeader()->setVisible(false);
    m_table->setShowGrid(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setColumnWidth(0, 200);
    m_table->setColumnWidth(1, 90);
    m_table->setColumnWidth(2, 90);
    m_table->setColumnWidth(3, 90);
    m_table->setColumnWidth(4, 80);
    m_table->setColumnWidth(5, 80);
    mainLayout->addWidget(m_table, 1);

    // 底部：替换按钮 + 进度 + 状态
    auto* bottomRow = new QHBoxLayout();

    m_replaceSelectedBtn = new QPushButton(tr("替换选中原图"));
    m_replaceAllBtn = new QPushButton(tr("一键替换所有原图"));
    m_replaceAllBtn->setStyleSheet(
        "QPushButton { color:#e03131; font-weight:bold; }"
        "QPushButton:hover { background-color:#fff5f5; }");
    m_replaceSelectedBtn->setEnabled(false);
    m_replaceAllBtn->setEnabled(false);

    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    m_progressBar->setMaximumHeight(16);
    m_progressBar->setFixedWidth(200);

    m_statusLabel = new QLabel(tr("拖拽图片或点击\"添加图片\"开始"));
    m_statusLabel->setStyleSheet("color:#868e96; font-size:8pt;");

    bottomRow->addWidget(m_replaceSelectedBtn);
    bottomRow->addWidget(m_replaceAllBtn);
    bottomRow->addStretch();
    bottomRow->addWidget(m_progressBar);
    bottomRow->addWidget(m_statusLabel);
    mainLayout->addLayout(bottomRow);

    // 连接
    connect(m_addFilesBtn, &QPushButton::clicked, this, &TinyImageCompress::onAddFiles);
    connect(m_addFolderBtn, &QPushButton::clicked, this, &TinyImageCompress::onAddFolder);
    connect(m_removeBtn, &QPushButton::clicked, this, &TinyImageCompress::onRemoveSelected);
    connect(m_clearBtn, &QPushButton::clicked, this, &TinyImageCompress::onClearAll);
    connect(m_compressBtn, &QPushButton::clicked, this, &TinyImageCompress::onCompress);
    connect(m_replaceSelectedBtn, &QPushButton::clicked, this, &TinyImageCompress::onReplaceSelected);
    connect(m_replaceAllBtn, &QPushButton::clicked, this, &TinyImageCompress::onReplaceAll);
    connect(m_qualitySlider, &QSlider::valueChanged, m_qualitySpin, &QSpinBox::setValue);
    connect(m_qualitySpin, QOverload<int>::of(&QSpinBox::valueChanged), m_qualitySlider, &QSlider::setValue);
}

void TinyImageCompress::addFiles(const QStringList& files)
{
    static const QStringList exts = {"png", "jpg", "jpeg", "bmp", "webp", "tiff", "gif"};

    for (const QString& file : files) {
        QFileInfo fi(file);
        if (!fi.exists() || !exts.contains(fi.suffix().toLower())) continue;

        // 去重
        bool exists = false;
        for (const auto& item : m_items) {
            if (item.sourcePath == fi.absoluteFilePath()) { exists = true; break; }
        }
        if (exists) continue;

        QImageReader reader(file);
        reader.setAutoTransform(true);
        QSize imgSize = reader.size();

        CompressItem item;
        item.sourcePath = fi.absoluteFilePath();
        item.fileName = fi.fileName();
        item.originalSize = fi.size();
        item.width = imgSize.isValid() ? imgSize.width() : 0;
        item.height = imgSize.isValid() ? imgSize.height() : 0;
        m_items.append(item);
    }

    updateTable();
    updateStatus();
}

void TinyImageCompress::onAddFiles()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this, tr("选择图片"), QString(),
        tr("图片文件 (*.png *.jpg *.jpeg *.bmp *.webp *.tiff *.gif)"));
    if (!files.isEmpty()) addFiles(files);
}

void TinyImageCompress::onAddFolder()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("选择目录"));
    if (dir.isEmpty()) return;

    QStringList files;
    QDir d(dir);
    for (const QFileInfo& fi : d.entryInfoList({"*.png", "*.jpg", "*.jpeg", "*.bmp", "*.webp", "*.tiff", "*.gif"}, QDir::Files))
        files << fi.absoluteFilePath();
    if (!files.isEmpty()) addFiles(files);
}

void TinyImageCompress::onRemoveSelected()
{
    QList<int> rows;
    for (auto* item : m_table->selectedItems()) {
        if (!rows.contains(item->row())) rows.append(item->row());
    }
    std::sort(rows.rbegin(), rows.rend());
    for (int row : rows) {
        if (row < m_items.size()) m_items.removeAt(row);
    }
    updateTable();
    updateStatus();
}

void TinyImageCompress::onClearAll()
{
    m_items.clear();
    updateTable();
    updateStatus();
}

void TinyImageCompress::compressImage(int index)
{
    if (index < 0 || index >= m_items.size()) return;

    CompressItem& item = m_items[index];
    QImage image(item.sourcePath);
    if (image.isNull()) {
        item.done = false;
        return;
    }

    // 确定输出格式
    QString format = m_formatCombo->currentText();
    QFileInfo fi(item.sourcePath);
    QString ext = fi.suffix().toLower();

    if (format == "JPEG") ext = "jpg";
    else if (format == "PNG") ext = "png";
    else if (format == "WebP") ext = "webp";
    // "保持原格式" 则用原扩展名

    // 如果是 JPEG 且不保留 EXIF，转换为 RGB（去除 alpha 和元数据）
    if ((ext == "jpg" || ext == "jpeg") && !m_keepMetadataCheck->isChecked()) {
        if (image.hasAlphaChannel()) {
            QImage rgbImage(image.size(), QImage::Format_RGB32);
            rgbImage.fill(Qt::white);
            QPainter painter(&rgbImage);
            painter.drawImage(0, 0, image);
            image = rgbImage;
        }
    }

    int quality = m_qualitySpin->value();

    // PNG 特殊处理：降低色深实现有损压缩（类似 pngquant）
    if (ext == "png") {
        // 量化为 indexed color（256 色调色板），类似 TinyPNG 的核心算法
        if (quality < 100) {
            image = image.convertToFormat(QImage::Format_Indexed8,
                                          Qt::PreferDither | Qt::DiffuseAlphaDither);
        }
    }

    // 压缩到内存
    QBuffer buffer(&item.compressedData);
    buffer.open(QIODevice::WriteOnly);

    QImageWriter writer(&buffer, ext.toUtf8());
    if (ext == "jpg" || ext == "jpeg") {
        writer.setQuality(quality);
        writer.setCompression(1);
    } else if (ext == "png") {
        writer.setQuality(100 - quality); // PNG: 0=no compression, 100=max
        writer.setCompression(9); // zlib max compression
    } else if (ext == "webp") {
        writer.setQuality(quality);
    } else {
        writer.setQuality(quality);
    }

    if (writer.write(image)) {
        item.compressedSize = item.compressedData.size();
        item.done = true;
    } else {
        item.done = false;
        item.compressedData.clear();
    }
}

void TinyImageCompress::onCompress()
{
    if (m_items.isEmpty()) return;

    m_progressBar->setVisible(true);
    m_progressBar->setMaximum(m_items.size());
    m_progressBar->setValue(0);
    m_compressBtn->setEnabled(false);

    int successCount = 0;
    qint64 totalOriginal = 0, totalCompressed = 0;

    for (int i = 0; i < m_items.size(); ++i) {
        if (!m_items[i].done) {
            compressImage(i);
            if (m_items[i].done) {
                successCount++;
                totalOriginal += m_items[i].originalSize;
                totalCompressed += m_items[i].compressedSize;
            }
        }
        m_progressBar->setValue(i + 1);
        QApplication::processEvents();
    }

    m_progressBar->setVisible(false);
    m_compressBtn->setEnabled(true);
    updateTable();
    updateStatus();

    if (successCount > 0) {
        double savedPercent = totalOriginal > 0 ? (1.0 - (double)totalCompressed / totalOriginal) * 100.0 : 0;
        m_statusLabel->setText(tr("压缩完成: %1 个文件，节省 %2 (%3%)")
            .arg(successCount)
            .arg(formatSize(totalOriginal - totalCompressed))
            .arg(savedPercent, 0, 'f', 1));
        m_statusLabel->setStyleSheet("color:#228be6; font-size:8pt; font-weight:bold;");
    }
}

void TinyImageCompress::onReplaceSelected()
{
    QList<int> rows;
    for (auto* item : m_table->selectedItems()) {
        if (!rows.contains(item->row())) rows.append(item->row());
    }

    int count = 0;
    for (int row : rows) {
        if (row >= m_items.size() || !m_items[row].done || m_items[row].replaced) continue;

        QFile file(m_items[row].sourcePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(m_items[row].compressedData);
            file.close();
            m_items[row].replaced = true;
            count++;
        }
    }
    updateTable();
    m_statusLabel->setText(tr("已替换 %1 个文件").arg(count));
}

void TinyImageCompress::onReplaceAll()
{
    int pending = 0;
    for (const auto& item : m_items)
        if (item.done && !item.replaced) pending++;

    if (pending == 0) return;

    if (QMessageBox::question(this, tr("确认"),
        tr("确定要用压缩后的图片替换 %1 个原图吗？\n此操作不可撤销！").arg(pending))
        != QMessageBox::Yes) return;

    int count = 0;
    for (auto& item : m_items) {
        if (!item.done || item.replaced) continue;
        QFile file(item.sourcePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(item.compressedData);
            file.close();
            item.replaced = true;
            count++;
        }
    }
    updateTable();
    m_statusLabel->setText(tr("已替换 %1 个文件").arg(count));
}

void TinyImageCompress::updateTable()
{
    m_table->setRowCount(m_items.size());

    for (int i = 0; i < m_items.size(); ++i) {
        const auto& item = m_items[i];

        auto* nameItem = new QTableWidgetItem(item.fileName);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        nameItem->setToolTip(item.sourcePath);
        m_table->setItem(i, 0, nameItem);

        auto* sizeItem = new QTableWidgetItem(
            item.width > 0 ? QString("%1x%2").arg(item.width).arg(item.height) : "-");
        sizeItem->setTextAlignment(Qt::AlignCenter);
        sizeItem->setFlags(sizeItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(i, 1, sizeItem);

        auto* origItem = new QTableWidgetItem(formatSize(item.originalSize));
        origItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        origItem->setFlags(origItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(i, 2, origItem);

        QString compStr = item.done ? formatSize(item.compressedSize) : "-";
        auto* compItem = new QTableWidgetItem(compStr);
        compItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        compItem->setFlags(compItem->flags() & ~Qt::ItemIsEditable);
        if (item.done && item.compressedSize < item.originalSize)
            compItem->setForeground(QColor("#2b8a3e"));
        else if (item.done)
            compItem->setForeground(QColor("#e03131"));
        m_table->setItem(i, 3, compItem);

        // 压缩率
        QString ratioStr = "-";
        if (item.done && item.originalSize > 0) {
            double ratio = (1.0 - (double)item.compressedSize / item.originalSize) * 100.0;
            ratioStr = QString("%1%2%").arg(ratio >= 0 ? "-" : "+").arg(qAbs(ratio), 0, 'f', 1);
        }
        auto* ratioItem = new QTableWidgetItem(ratioStr);
        ratioItem->setTextAlignment(Qt::AlignCenter);
        ratioItem->setFlags(ratioItem->flags() & ~Qt::ItemIsEditable);
        if (item.done && item.compressedSize < item.originalSize)
            ratioItem->setForeground(QColor("#2b8a3e"));
        m_table->setItem(i, 4, ratioItem);

        // 状态
        QString status;
        if (item.replaced) status = tr("已替换");
        else if (item.done) status = tr("已压缩");
        else status = tr("待压缩");
        auto* statusItem = new QTableWidgetItem(status);
        statusItem->setTextAlignment(Qt::AlignCenter);
        statusItem->setFlags(statusItem->flags() & ~Qt::ItemIsEditable);
        if (item.replaced) statusItem->setForeground(QColor("#868e96"));
        else if (item.done) statusItem->setForeground(QColor("#228be6"));
        m_table->setItem(i, 5, statusItem);

        // 操作列留空（通过选中行 + 底部按钮操作）
        auto* opItem = new QTableWidgetItem("");
        opItem->setFlags(opItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(i, 6, opItem);

        m_table->setRowHeight(i, 30);
    }
}

void TinyImageCompress::updateStatus()
{
    int total = m_items.size();
    int done = 0, replaced = 0;
    for (const auto& item : m_items) {
        if (item.done) done++;
        if (item.replaced) replaced++;
    }

    m_compressBtn->setEnabled(total > 0);
    m_replaceSelectedBtn->setEnabled(done > replaced);
    m_replaceAllBtn->setEnabled(done > replaced);

    if (total == 0)
        m_statusLabel->setText(tr("拖拽图片或点击\"添加图片\"开始"));
    else
        m_statusLabel->setText(tr("共 %1 个文件，已压缩 %2 个，已替换 %3 个").arg(total).arg(done).arg(replaced));
    m_statusLabel->setStyleSheet("color:#868e96; font-size:8pt;");
}

QString TinyImageCompress::formatSize(qint64 bytes) const
{
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);
}

void TinyImageCompress::onQualityChanged(int value)
{
    Q_UNUSED(value)
}

void TinyImageCompress::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void TinyImageCompress::dropEvent(QDropEvent* event)
{
    QStringList files;
    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile()) {
            QString path = url.toLocalFile();
            QFileInfo fi(path);
            if (fi.isDir()) {
                QDir d(path);
                for (const QFileInfo& f : d.entryInfoList({"*.png","*.jpg","*.jpeg","*.bmp","*.webp","*.tiff","*.gif"}, QDir::Files))
                    files << f.absoluteFilePath();
            } else {
                files << path;
            }
        }
    }
    if (!files.isEmpty()) addFiles(files);
}
