#include "imagecollection.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>
#include <QProcess>
#include <QDebug>
#include <QHeaderView>

REGISTER_DYNAMICOBJECT(ImageCollection);

// ---- Predefined tags ----
const QStringList ImageCollection::predefinedTags = {
    QStringLiteral("证件"),
    QStringLiteral("身份证"),
    QStringLiteral("毕业证"),
    QStringLiteral("驾照"),
    QStringLiteral("户口本"),
    QStringLiteral("护照"),
    QStringLiteral("社保卡"),
    QStringLiteral("其他")
};

// =========================================================================
// ImageCardDelegate
// =========================================================================

ImageCardDelegate::ImageCardDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

QHash<QString, ImageCardDelegate::TagColor> ImageCardDelegate::tagColors()
{
    static QHash<QString, TagColor> colors = {
        {QStringLiteral("证件"),   {{0xe3, 0xf2, 0xfd}, {0x1565, 0xc0 >> 0, 0xc0 & 0xff}}},
        {QStringLiteral("身份证"), {{0xff, 0xf3, 0xe0}, {0xe6, 0x51, 0x00}}},
        {QStringLiteral("毕业证"), {{0xe8, 0xf5, 0xe9}, {0x2e, 0x7d, 0x32}}},
        {QStringLiteral("驾照"),   {{0xfc, 0xe4, 0xec}, {0xc6, 0x28, 0x28}}},
        {QStringLiteral("户口本"), {{0xf3, 0xe5, 0xf5}, {0x6a, 0x1b, 0x9a}}},
        {QStringLiteral("护照"),   {{0xe0, 0xf7, 0xfa}, {0x00, 0x69, 0x7a}}},
        {QStringLiteral("社保卡"), {{0xff, 0xf8, 0xe1}, {0xff, 0x8f, 0x00}}},
        {QStringLiteral("其他"),   {{0xf5, 0xf5, 0xf5}, {0x61, 0x61, 0x61}}},
    };
    return colors;
}

QSize ImageCardDelegate::sizeHint(const QStyleOptionViewItem& /*option*/,
                                  const QModelIndex& /*index*/) const
{
    return QSize(166, 170);
}

void ImageCardDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                              const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    QRect cardRect = option.rect.adjusted(3, 3, -3, -3);

    // Background
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(Qt::white));
    painter->drawRoundedRect(cardRect, 6, 6);

    // Selected state
    if (option.state & QStyle::State_Selected) {
        painter->setPen(QPen(QColor(0x42, 0xa5, 0xf5), 2));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(cardRect.adjusted(1, 1, -1, -1), 6, 6);
    } else if (option.state & QStyle::State_MouseOver) {
        painter->setPen(QPen(QColor(0xbd, 0xbd, 0xbd), 1));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(cardRect.adjusted(1, 1, -1, -1), 6, 6);
    }

    // Thumbnail — 填满卡片宽度，只留 4px 边距
    auto info = index.data(Qt::UserRole).value<ImageInfo>();
    int thumbPad = 4;
    int thumbW = cardRect.width() - thumbPad * 2;
    int thumbH = cardRect.height() - 40; // 底部留给文字和标签

    if (m_cache) {
        QPixmap thumb = m_cache->value(info.storedName);
        if (!thumb.isNull()) {
            QRect thumbArea(cardRect.left() + thumbPad, cardRect.top() + thumbPad, thumbW, thumbH);
            qreal dpr = thumb.devicePixelRatio();
            int logicalW = static_cast<int>(thumb.width() / dpr);
            int logicalH = static_cast<int>(thumb.height() / dpr);
            if (logicalW > thumbArea.width() || logicalH > thumbArea.height()) {
                QSize fit = QSize(logicalW, logicalH).scaled(thumbArea.size(), Qt::KeepAspectRatio);
                logicalW = fit.width();
                logicalH = fit.height();
            }
            int x = thumbArea.left() + (thumbArea.width() - logicalW) / 2;
            int y = thumbArea.top() + (thumbArea.height() - logicalH) / 2;
            painter->drawPixmap(QRect(x, y, logicalW, logicalH), thumb);
        }
    }

    // Filename
    QFont font;
    font.setPointSize(8);
    painter->setFont(font);
    painter->setPen(QColor(0x49, 0x50, 0x57));
    int textY = cardRect.bottom() - 34;
    QRect textRect(cardRect.left() + 4, textY, cardRect.width() - 8, 16);
    QString elidedName = painter->fontMetrics().elidedText(info.fileName, Qt::ElideMiddle, textRect.width());
    painter->drawText(textRect, Qt::AlignHCenter | Qt::AlignVCenter, elidedName);

    // Tag chips
    if (!info.tags.isEmpty()) {
        auto colors = tagColors();
        QFont tagFont;
        tagFont.setPointSize(7);
        painter->setFont(tagFont);
        QFontMetrics tagFm(tagFont);

        int chipX = cardRect.left() + 4;
        int chipY = cardRect.bottom() - 16;
        int maxRight = cardRect.right() - 4;

        for (const QString& tag : info.tags) {
            int chipWidth = tagFm.horizontalAdvance(tag) + 10;
            if (chipX + chipWidth > maxRight) break;

            TagColor tc = colors.value(tag, {{0xf5, 0xf5, 0xf5}, {0x61, 0x61, 0x61}});
            painter->setPen(Qt::NoPen);
            painter->setBrush(tc.background);
            painter->drawRoundedRect(chipX, chipY, chipWidth, 18, 4, 4);

            painter->setPen(tc.foreground);
            painter->drawText(QRect(chipX, chipY, chipWidth, 18), Qt::AlignCenter, tag);

            chipX += chipWidth + 4;
        }
    }

    painter->restore();
}

// =========================================================================
// ImageCollection
// =========================================================================

ImageCollection::ImageCollection()
{
    qRegisterMetaType<ImageInfo>("ImageInfo");
    setAcceptDrops(true);
    m_db = SqliteWrapper::SqliteManager::getDefaultInstance();
    initDatabase();
    setupUI();
    loadImages();
}

ImageCollection::~ImageCollection() = default;

// ---- Database ----

void ImageCollection::initDatabase()
{
    const QString sql = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS image_collection ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  file_name TEXT NOT NULL,"
        "  stored_name TEXT NOT NULL UNIQUE,"
        "  file_size INTEGER,"
        "  width INTEGER,"
        "  height INTEGER,"
        "  tags TEXT DEFAULT '',"
        "  notes TEXT DEFAULT '',"
        "  extras TEXT DEFAULT '',"
        "  ocr_text TEXT DEFAULT '',"
        "  is_deleted INTEGER DEFAULT 0,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  deleted_at DATETIME"
        ");");
    m_db->execute(sql);

    // 兼容旧表：添加缺失列
    m_db->execute("ALTER TABLE image_collection ADD COLUMN extras TEXT DEFAULT ''");
    m_db->execute("ALTER TABLE image_collection ADD COLUMN ocr_text TEXT DEFAULT ''");
}

// ---- UI ----

void ImageCollection::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(8, 8, 8, 8);
    m_mainLayout->setSpacing(6);

    // Toolbar
    m_toolbarLayout = new QHBoxLayout;
    m_toolbarLayout->setSpacing(6);

    auto* searchLabel = new QLabel(QStringLiteral("Search:"));
    searchLabel->setStyleSheet("font-size:9pt; color:#495057;");
    m_toolbarLayout->addWidget(searchLabel);

    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(QStringLiteral("Filter by filename..."));
    m_searchEdit->setFixedWidth(200);
    m_toolbarLayout->addWidget(m_searchEdit);

    m_tagFilter = new QComboBox;
    m_tagFilter->addItem(QStringLiteral("All Tags"));
    for (const QString& tag : predefinedTags)
        m_tagFilter->addItem(tag);
    m_tagFilter->setFixedWidth(120);
    m_toolbarLayout->addWidget(m_tagFilter);

    m_toolbarLayout->addStretch();

    m_addBtn = new QPushButton(QStringLiteral("+ Add"));
    m_toolbarLayout->addWidget(m_addBtn);

    m_pasteBtn = new QPushButton(QStringLiteral("Paste"));
    m_toolbarLayout->addWidget(m_pasteBtn);

    m_recycleBinBtn = new QPushButton(QStringLiteral("Recycle Bin"));
    m_toolbarLayout->addWidget(m_recycleBinBtn);

    m_viewToggleBtn = new QPushButton(QStringLiteral("Table View"));
    m_toolbarLayout->addWidget(m_viewToggleBtn);

    m_mainLayout->addLayout(m_toolbarLayout);

    // Stacked widget for card/table views
    m_viewStack = new QStackedWidget;

    // List widget (card view) - index 0
    m_listWidget = new QListWidget;
    m_listWidget->setViewMode(QListView::IconMode);
    m_listWidget->setResizeMode(QListView::Adjust);
    m_listWidget->setMovement(QListView::Static);
    m_listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    m_listWidget->setIconSize(QSize(160, 130));
    m_listWidget->setGridSize(QSize(172, 176));
    m_listWidget->setSpacing(4);

    m_delegate = new ImageCardDelegate(this);
    m_delegate->setThumbnailCache(&m_thumbnailCache);
    m_listWidget->setItemDelegate(m_delegate);

    m_viewStack->addWidget(m_listWidget); // index 0

    // Table widget (table view) - index 1
    m_tableWidget = new QTableWidget;
    m_tableWidget->setColumnCount(6);
    m_tableWidget->setHorizontalHeaderLabels({
        QStringLiteral(""),
        QStringLiteral("Filename"),
        QStringLiteral("Size"),
        QStringLiteral("Dimensions"),
        QStringLiteral("Tags"),
        QStringLiteral("Date")
    });
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tableWidget->setAlternatingRowColors(true);
    m_tableWidget->setShowGrid(false);
    m_tableWidget->verticalHeader()->setVisible(false);
    m_tableWidget->horizontalHeader()->setStretchLastSection(true);
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableWidget->setIconSize(QSize(48, 48));
    m_tableWidget->setColumnWidth(0, 60);
    m_tableWidget->setColumnWidth(1, 200);
    m_tableWidget->setColumnWidth(2, 80);
    m_tableWidget->setColumnWidth(3, 100);
    m_tableWidget->setColumnWidth(4, 150);

    m_viewStack->addWidget(m_tableWidget); // index 1

    // 主分割器（左：图片列表，右：详情面板）
    m_mainSplitter = new QSplitter(Qt::Horizontal);
    m_mainSplitter->addWidget(m_viewStack);

    // 主分割器右侧 — 详情面板
    m_detailPanel = new QWidget;
    auto* detailLayout = new QVBoxLayout(m_detailPanel);
    detailLayout->setContentsMargins(0, 0, 0, 0);
    detailLayout->setSpacing(0);

    // 预览图
    m_detailPreview = new QLabel;
    m_detailPreview->setFixedHeight(160);
    m_detailPreview->setAlignment(Qt::AlignCenter);
    m_detailPreview->setStyleSheet("background: #f1f3f5; border-bottom: 1px solid #e9ecef;");
    detailLayout->addWidget(m_detailPreview);

    // 文件信息区域（带内边距）
    auto* infoArea = new QWidget;
    auto* infoLayout = new QVBoxLayout(infoArea);
    infoLayout->setContentsMargins(10, 8, 10, 0);
    infoLayout->setSpacing(4);

    m_detailFileName = new QLabel;
    m_detailFileName->setStyleSheet("font-weight: 600; font-size: 12px; color: #212529;");
    m_detailFileName->setWordWrap(true);
    infoLayout->addWidget(m_detailFileName);

    m_detailFileInfo = new QLabel;
    m_detailFileInfo->setStyleSheet("color: #868e96; font-size: 11px;");
    m_detailFileInfo->setWordWrap(true);
    infoLayout->addWidget(m_detailFileInfo);
    detailLayout->addWidget(infoArea);

    // 分隔线
    auto* sep1 = new QFrame; sep1->setFrameShape(QFrame::HLine); sep1->setStyleSheet("color: #e9ecef;"); sep1->setFixedHeight(1);
    detailLayout->addWidget(sep1);

    // 扩展信息区域
    auto* extraArea = new QWidget;
    auto* extraLayout = new QVBoxLayout(extraArea);
    extraLayout->setContentsMargins(10, 6, 10, 0);
    extraLayout->setSpacing(4);

    // 标题行 + 操作按钮
    auto* extraHeader = new QHBoxLayout;
    auto* extraLabel = new QLabel(tr("扩展信息"));
    extraLabel->setStyleSheet("color: #495057; font-size: 11px; font-weight: 600;");
    extraHeader->addWidget(extraLabel);
    extraHeader->addStretch();

    m_addExtraBtn = new QPushButton(tr("添加"));
    m_addExtraBtn->setFixedHeight(22);
    m_addExtraBtn->setStyleSheet(
        "QPushButton { color: #228be6; font-size: 11px; padding: 0 8px; border: 1px solid #dee2e6; border-radius: 4px; background: white; }"
        "QPushButton:hover { background: #e7f5ff; border-color: #228be6; }");

    m_removeExtraBtn = new QPushButton(tr("删除"));
    m_removeExtraBtn->setFixedHeight(22);
    m_removeExtraBtn->setStyleSheet(
        "QPushButton { color: #868e96; font-size: 11px; padding: 0 8px; border: 1px solid #dee2e6; border-radius: 4px; background: white; }"
        "QPushButton:hover { background: #fff5f5; border-color: #e03131; color: #e03131; }");

    m_saveExtraBtn = new QPushButton(tr("保存"));
    m_saveExtraBtn->setFixedHeight(22);
    m_saveExtraBtn->setStyleSheet(
        "QPushButton { color: white; font-size: 11px; padding: 0 10px; border: none; border-radius: 4px; background: #228be6; }"
        "QPushButton:hover { background: #1c7ed6; }");

    extraHeader->addWidget(m_addExtraBtn);
    extraHeader->addWidget(m_removeExtraBtn);
    extraHeader->addWidget(m_saveExtraBtn);
    extraLayout->addLayout(extraHeader);

    m_extrasTable = new QTableWidget(0, 2);
    m_extrasTable->setHorizontalHeaderLabels({tr("键"), tr("值")});
    m_extrasTable->horizontalHeader()->setStretchLastSection(true);
    m_extrasTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_extrasTable->setColumnWidth(0, 80);
    m_extrasTable->verticalHeader()->setVisible(false);
    m_extrasTable->verticalHeader()->setDefaultSectionSize(26);
    m_extrasTable->setMaximumHeight(150);
    m_extrasTable->setAlternatingRowColors(true);
    m_extrasTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_extrasTable->setStyleSheet(
        "QTableWidget { border: 1px solid #dee2e6; border-radius: 4px; font-size: 11px; background: white; gridline-color: #f1f3f5; }"
        "QTableWidget::item { padding: 2px 6px; }"
        "QHeaderView::section { background: #f8f9fa; border: none; border-bottom: 1px solid #dee2e6; border-right: 1px solid #f1f3f5;"
        "  padding: 4px 6px; font-size: 10px; color: #868e96; font-weight: 500; }");
    extraLayout->addWidget(m_extrasTable);
    detailLayout->addWidget(extraArea);

    // 分隔线
    auto* sep2 = new QFrame; sep2->setFrameShape(QFrame::HLine); sep2->setStyleSheet("color: #e9ecef;"); sep2->setFixedHeight(1);
    detailLayout->addWidget(sep2);

    // OCR 区域
    auto* ocrArea = new QWidget;
    auto* ocrLayout = new QVBoxLayout(ocrArea);
    ocrLayout->setContentsMargins(10, 6, 10, 8);
    ocrLayout->setSpacing(4);

    auto* ocrHeader = new QHBoxLayout;
    auto* ocrLabel = new QLabel(tr("OCR 识别结果"));
    ocrLabel->setStyleSheet("color: #495057; font-size: 11px; font-weight: 600;");
    ocrHeader->addWidget(ocrLabel);
    ocrHeader->addStretch();

    auto* reOcrBtn = new QPushButton(tr("重新识别"));
    reOcrBtn->setFixedHeight(22);
    reOcrBtn->setStyleSheet(
        "QPushButton { color: #495057; font-size: 11px; padding: 0 8px; border: 1px solid #dee2e6; border-radius: 4px; background: white; }"
        "QPushButton:hover { background: #f1f3f5; }");
    connect(reOcrBtn, &QPushButton::clicked, this, [this]() {
        if (m_currentDetailId <= 0) return;
        // 找到当前图片的 storedName
        for (int i = 0; i < m_listWidget->count(); ++i) {
            ImageInfo info = m_listWidget->item(i)->data(Qt::UserRole).value<ImageInfo>();
            if (info.id == m_currentDetailId) {
                m_ocrResultEdit->setPlainText(tr("正在识别..."));
                autoOcrImage(info.id, info.storedName);
                break;
            }
        }
    });
    ocrHeader->addWidget(reOcrBtn);
    ocrLayout->addLayout(ocrHeader);

    m_ocrResultEdit = new QTextEdit;
    m_ocrResultEdit->setReadOnly(true);
    m_ocrResultEdit->setPlaceholderText(tr("点击「重新识别」或添加新图片自动识别"));
    m_ocrResultEdit->setStyleSheet(
        "QTextEdit { border: 1px solid #dee2e6; border-radius: 4px; font-size: 11px; padding: 6px; background: #fafafa; }");
    ocrLayout->addWidget(m_ocrResultEdit, 1);
    detailLayout->addWidget(ocrArea, 1);

    m_detailPanel->setMinimumWidth(250);
    m_detailPanel->setMaximumWidth(420);
    m_detailPanel->setStyleSheet("QWidget { background: white; }");
    m_mainSplitter->addWidget(m_detailPanel);
    m_mainSplitter->setSizes({600, 300});
    m_mainSplitter->setChildrenCollapsible(false);

    m_mainLayout->addWidget(m_mainSplitter, 1);

    // Status bar
    m_statusLabel = new QLabel;
    m_statusLabel->setStyleSheet("font-size:9pt; color:#6c757d; padding:2px 0;");
    m_mainLayout->addWidget(m_statusLabel);

    // 初始状态
    m_detailFileName->setText(tr("点击图片查看详情"));
    m_detailFileInfo->clear();

    // Styles
    setStyleSheet(QStringLiteral(
        "QPushButton { padding:4px 10px; border:none; border-radius:4px; font-size:9pt; color:#495057; background:transparent; }"
        "QPushButton:hover { background-color:#e9ecef; }"
        "QLineEdit { border:1px solid #dee2e6; border-radius:4px; padding:3px 8px; font-size:9pt; }"
        "QComboBox { border:1px solid #dee2e6; border-radius:4px; padding:3px 8px; font-size:9pt; }"
        "QListWidget { border:1px solid #dee2e6; border-radius:4px; background:#fafafa; }"
        "QTableWidget { border:1px solid #dee2e6; border-radius:4px; background:#fafafa; alternate-background-color:#f8f9fa; }"
        "QTableWidget::item { padding:4px; }"
        "QHeaderView::section { background:#f1f3f5; border:none; padding:6px 8px; font-size:9pt; color:#495057; }"
    ));

    // Connections
    connect(m_addBtn, &QPushButton::clicked, this, &ImageCollection::onAddImages);
    connect(m_pasteBtn, &QPushButton::clicked, this, &ImageCollection::onPasteFromClipboard);
    connect(m_recycleBinBtn, &QPushButton::clicked, this, &ImageCollection::onToggleRecycleBin);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ImageCollection::onSearchTextChanged);
    connect(m_tagFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ImageCollection::onTagFilterChanged);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &ImageCollection::onItemDoubleClicked);
    connect(m_listWidget, &QListWidget::customContextMenuRequested, this, &ImageCollection::onItemContextMenu);
    connect(m_viewToggleBtn, &QPushButton::clicked, this, &ImageCollection::onToggleView);

    // 详情面板信号
    connect(m_listWidget, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* current) {
        if (current) {
            ImageInfo info = current->data(Qt::UserRole).value<ImageInfo>();
            showDetail(info);
        }
    });
    connect(m_addExtraBtn, &QPushButton::clicked, this, &ImageCollection::onAddExtraRow);
    connect(m_removeExtraBtn, &QPushButton::clicked, this, &ImageCollection::onRemoveExtraRow);
    connect(m_saveExtraBtn, &QPushButton::clicked, this, [this]() { saveExtras(m_currentDetailId); });
}

// ---- Storage helpers ----

QString ImageCollection::storagePath() const
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                   + QStringLiteral("/image-collection/");
    QDir().mkpath(path);
    return path;
}

QString ImageCollection::formatFileSize(qint64 bytes) const
{
    if (bytes < 1024) return QString::number(bytes) + " B";
    if (bytes < 1024 * 1024) return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    if (bytes < 1024LL * 1024 * 1024) return QString::number(bytes / (1024.0 * 1024.0), 'f', 2) + " MB";
    return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
}

ImageInfo ImageCollection::imageInfoFromRecord(const QVariantMap& record) const
{
    ImageInfo info;
    info.id = record.value("id").toInt();
    info.fileName = record.value("file_name").toString();
    info.storedName = record.value("stored_name").toString();
    info.fileSize = record.value("file_size").toLongLong();
    info.width = record.value("width").toInt();
    info.height = record.value("height").toInt();
    QString tagsStr = record.value("tags").toString();
    if (!tagsStr.isEmpty())
        info.tags = tagsStr.split(',', Qt::SkipEmptyParts);
    info.notes = record.value("notes").toString();
    info.ocrText = record.value("ocr_text").toString();
    info.extras = extrasFromJson(record.value("extras").toString());
    info.isDeleted = record.value("is_deleted").toInt() != 0;
    info.createdAt = QDateTime::fromString(record.value("created_at").toString(), Qt::ISODate);
    if (!record.value("deleted_at").isNull())
        info.deletedAt = QDateTime::fromString(record.value("deleted_at").toString(), Qt::ISODate);
    return info;
}

// ---- Thumbnail cache ----

QPixmap ImageCollection::getThumbnail(const QString& storedName, int maxWidth, int maxHeight)
{
    if (m_thumbnailCache.contains(storedName))
        return m_thumbnailCache.value(storedName);

    // Retina 适配：按 DPR 倍数渲染，显示时缩放回逻辑像素
    qreal dpr = qApp->devicePixelRatio();
    int renderWidth = static_cast<int>(maxWidth * dpr);
    int renderHeight = static_cast<int>(maxHeight * dpr);

    QString filePath = storagePath() + storedName;
    QImageReader reader(filePath);
    reader.setAutoTransform(true);
    QSize origSize = reader.size();
    if (origSize.isValid()) {
        QSize scaled = origSize.scaled(renderWidth, renderHeight, Qt::KeepAspectRatio);
        reader.setScaledSize(scaled);
    }
    QImage image = reader.read();
    QPixmap pix = QPixmap::fromImage(image);
    if (!pix.isNull()) {
        pix.setDevicePixelRatio(dpr);
        m_thumbnailCache.insert(storedName, pix);
    }
    return pix;
}

// ---- Load / Reload ----

void ImageCollection::loadImages()
{
    m_listWidget->clear();

    QString sql = QStringLiteral("SELECT * FROM image_collection WHERE is_deleted = :deleted");
    QVariantMap params;
    params["deleted"] = m_showingRecycleBin ? 1 : 0;

    // Tag filter
    if (m_tagFilter && m_tagFilter->currentIndex() > 0) {
        QString tag = m_tagFilter->currentText();
        sql += QStringLiteral(" AND (',' || tags || ',') LIKE :tag");
        params["tag"] = QStringLiteral("%,") + tag + QStringLiteral(",%");
    }

    sql += QStringLiteral(" ORDER BY created_at DESC");

    auto result = m_db->select(sql, params);
    if (!result.success) {
        qDebug() << "[ImageCollection] DB select failed";
        return;
    }

    qDebug() << "[ImageCollection] DB select returned" << result.data.size() << "rows";

    QString searchText = m_searchEdit ? m_searchEdit->text().trimmed() : QString();

    int addedCount = 0;
    for (const QVariant& row : result.data) {
        QVariantMap record = row.toMap();
        ImageInfo info = imageInfoFromRecord(record);

        // Client-side search filter
        if (!searchText.isEmpty() && !info.fileName.contains(searchText, Qt::CaseInsensitive))
            continue;

        // Ensure thumbnail is cached
        getThumbnail(info.storedName, 160, 130);

        auto* item = new QListWidgetItem;
        item->setData(Qt::UserRole, QVariant::fromValue(info));
        item->setSizeHint(QSize(166, 170));
        m_listWidget->addItem(item);
        ++addedCount;
    }

    qDebug() << "[ImageCollection] Added" << addedCount << "items to list widget";

    loadTableView();
    updateStatusBar();
}

void ImageCollection::updateStatusBar()
{
    int count = m_listWidget->count();

    // Calculate storage usage
    QDir dir(storagePath());
    qint64 totalSize = 0;
    const auto entries = dir.entryInfoList(QDir::Files);
    for (const QFileInfo& fi : entries)
        totalSize += fi.size();

    QString label = m_showingRecycleBin ? QStringLiteral("Recycle Bin") : QStringLiteral("Images");
    m_statusLabel->setText(QStringLiteral("%1: %2 | Storage: %3")
                               .arg(label)
                               .arg(count)
                               .arg(formatFileSize(totalSize)));
}

// ---- Import ----

void ImageCollection::importImage(const QString& filePath)
{
    QFileInfo fi(filePath);
    if (!fi.exists()) return;

    QString ext = fi.suffix().toLower();
    QString storedName = QUuid::createUuid().toString(QUuid::WithoutBraces) + "." + ext;

    QString destPath = storagePath() + storedName;
    bool copyOk = QFile::copy(filePath, destPath);
    qDebug() << "[ImageCollection] Copy" << filePath << "->" << destPath << ":" << (copyOk ? "OK" : "FAILED");
    if (!copyOk) return;

    QImageReader reader(destPath);
    reader.setAutoTransform(true);
    QSize imgSize = reader.size();

    QVariantMap data;
    data["file_name"] = fi.fileName();
    data["stored_name"] = storedName;
    data["file_size"] = fi.size();
    data["width"] = imgSize.isValid() ? imgSize.width() : 0;
    data["height"] = imgSize.isValid() ? imgSize.height() : 0;
    data["tags"] = QString();
    data["notes"] = QString();

    m_db->insert("image_collection", data);
    qDebug() << "[ImageCollection] Inserted into DB:" << fi.fileName()
             << "stored as" << storedName
             << "size:" << fi.size()
             << "dimensions:" << imgSize;

    // 获取插入后的 ID 并自动 OCR
    auto res = m_db->select("SELECT id FROM image_collection WHERE stored_name = :sn",
                            {{"sn", storedName}});
    if (res.success && !res.data.isEmpty()) {
        int newId = res.data.first().toMap().value("id").toInt();
        autoOcrImage(newId, storedName);
    }
}

// ---- View toggle ----

void ImageCollection::onToggleView()
{
    m_isTableView = !m_isTableView;
    m_viewStack->setCurrentIndex(m_isTableView ? 1 : 0);
    m_viewToggleBtn->setText(m_isTableView ? QStringLiteral("Card View") : QStringLiteral("Table View"));
    loadImages();
}

void ImageCollection::loadTableView()
{
    m_tableWidget->setRowCount(0);

    QString sql = QStringLiteral("SELECT * FROM image_collection WHERE is_deleted = :deleted");
    QVariantMap params;
    params["deleted"] = m_showingRecycleBin ? 1 : 0;

    if (m_tagFilter && m_tagFilter->currentIndex() > 0) {
        QString tag = m_tagFilter->currentText();
        sql += QStringLiteral(" AND (',' || tags || ',') LIKE :tag");
        params["tag"] = QStringLiteral("%,") + tag + QStringLiteral(",%");
    }

    sql += QStringLiteral(" ORDER BY created_at DESC");

    auto result = m_db->select(sql, params);
    if (!result.success) return;

    QString searchText = m_searchEdit ? m_searchEdit->text().trimmed() : QString();

    for (const QVariant& row : result.data) {
        QVariantMap record = row.toMap();
        ImageInfo info = imageInfoFromRecord(record);

        if (!searchText.isEmpty() && !info.fileName.contains(searchText, Qt::CaseInsensitive))
            continue;

        int r = m_tableWidget->rowCount();
        m_tableWidget->insertRow(r);
        m_tableWidget->setRowHeight(r, 52);

        // Column 0: Thumbnail
        QPixmap thumb = getThumbnail(info.storedName, 48, 48);
        auto* thumbItem = new QTableWidgetItem;
        if (!thumb.isNull())
            thumbItem->setIcon(QIcon(thumb));
        thumbItem->setData(Qt::UserRole, QVariant::fromValue(info));
        m_tableWidget->setItem(r, 0, thumbItem);

        // Column 1: Filename
        m_tableWidget->setItem(r, 1, new QTableWidgetItem(info.fileName));

        // Column 2: File size
        m_tableWidget->setItem(r, 2, new QTableWidgetItem(formatFileSize(info.fileSize)));

        // Column 3: Dimensions
        QString dims = (info.width > 0 && info.height > 0)
                           ? QStringLiteral("%1x%2").arg(info.width).arg(info.height)
                           : QStringLiteral("-");
        m_tableWidget->setItem(r, 3, new QTableWidgetItem(dims));

        // Column 4: Tags
        m_tableWidget->setItem(r, 4, new QTableWidgetItem(info.tags.join(QStringLiteral(", "))));

        // Column 5: Date
        QString dateStr = info.createdAt.isValid()
                              ? info.createdAt.toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                              : QStringLiteral("-");
        m_tableWidget->setItem(r, 5, new QTableWidgetItem(dateStr));
    }
}

// ---- Slots ----

void ImageCollection::onAddImages()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this, QStringLiteral("Add Images"), QString(),
        QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.tiff *.svg)"));

    if (files.isEmpty()) return;

    for (const QString& f : files)
        importImage(f);

    loadImages();
}

void ImageCollection::onPasteFromClipboard()
{
    const QClipboard* clipboard = QApplication::clipboard();
    const QMimeData* mimeData = clipboard->mimeData();

    if (mimeData->hasImage()) {
        QImage image = qvariant_cast<QImage>(mimeData->imageData());
        if (image.isNull()) return;

        QString storedName = QUuid::createUuid().toString(QUuid::WithoutBraces) + ".png";
        QString destPath = storagePath() + storedName;
        image.save(destPath, "PNG");

        QFileInfo fi(destPath);
        QVariantMap data;
        data["file_name"] = QStringLiteral("clipboard_") +
                            QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".png";
        data["stored_name"] = storedName;
        data["file_size"] = fi.size();
        data["width"] = image.width();
        data["height"] = image.height();
        data["tags"] = QString();
        data["notes"] = QString();

        m_db->insert("image_collection", data);
        loadImages();
    } else if (mimeData->hasUrls()) {
        for (const QUrl& url : mimeData->urls()) {
            if (url.isLocalFile())
                importImage(url.toLocalFile());
        }
        loadImages();
    }
}

void ImageCollection::onToggleRecycleBin()
{
    m_showingRecycleBin = !m_showingRecycleBin;
    m_recycleBinBtn->setText(m_showingRecycleBin ? QStringLiteral("Back") : QStringLiteral("Recycle Bin"));
    loadImages();
}

void ImageCollection::onSearchTextChanged(const QString& /*text*/)
{
    loadImages();
}

void ImageCollection::onTagFilterChanged(int /*index*/)
{
    loadImages();
}

void ImageCollection::onItemDoubleClicked(QListWidgetItem* item)
{
    if (!item) return;
    auto info = item->data(Qt::UserRole).value<ImageInfo>();
    QString filePath = storagePath() + info.storedName;
    QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
}

void ImageCollection::onItemContextMenu(const QPoint& pos)
{
    QListWidgetItem* item = m_listWidget->itemAt(pos);
    if (!item) return;

    auto info = item->data(Qt::UserRole).value<ImageInfo>();
    QMenu menu(this);

    // Edit tags submenu
    QMenu* tagsMenu = menu.addMenu(QStringLiteral("Edit Tags"));
    for (const QString& tag : predefinedTags) {
        QAction* action = tagsMenu->addAction(tag);
        action->setCheckable(true);
        action->setChecked(info.tags.contains(tag));
    }
    connect(tagsMenu, &QMenu::triggered, this, [this, item, info](QAction* action) mutable {
        QString tag = action->text();
        if (action->isChecked()) {
            if (!info.tags.contains(tag))
                info.tags.append(tag);
        } else {
            info.tags.removeAll(tag);
        }
        QString tagsStr = info.tags.join(',');
        QVariantMap data;
        data["tags"] = tagsStr;
        QVariantMap whereParams;
        whereParams["id"] = info.id;
        m_db->update("image_collection", data, "id = :id", whereParams);
        loadImages();
    });

    menu.addSeparator();

    if (m_showingRecycleBin) {
        menu.addAction(QStringLiteral("Restore"), this, &ImageCollection::onRestore);
        menu.addAction(QStringLiteral("Permanent Delete"), this, &ImageCollection::onPermanentDelete);
        menu.addSeparator();
        menu.addAction(QStringLiteral("Empty Recycle Bin"), this, &ImageCollection::onEmptyRecycleBin);
    } else {
        menu.addAction(QStringLiteral("Delete"), this, &ImageCollection::onSoftDelete);
    }

    menu.addSeparator();
#ifdef Q_OS_MAC
    menu.addAction(QStringLiteral("Open in Finder"), this, &ImageCollection::onOpenInExplorer);
#else
    menu.addAction(QStringLiteral("Open in Explorer"), this, &ImageCollection::onOpenInExplorer);
#endif

    menu.exec(m_listWidget->viewport()->mapToGlobal(pos));
}

void ImageCollection::onSoftDelete()
{
    auto items = m_listWidget->selectedItems();
    for (auto* item : items) {
        auto info = item->data(Qt::UserRole).value<ImageInfo>();
        QVariantMap data;
        data["is_deleted"] = 1;
        data["deleted_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        QVariantMap whereParams;
        whereParams["id"] = info.id;
        m_db->update("image_collection", data, "id = :id", whereParams);
    }
    loadImages();
}

void ImageCollection::onRestore()
{
    auto items = m_listWidget->selectedItems();
    for (auto* item : items) {
        auto info = item->data(Qt::UserRole).value<ImageInfo>();
        QVariantMap data;
        data["is_deleted"] = 0;
        data["deleted_at"] = QVariant();
        QVariantMap whereParams;
        whereParams["id"] = info.id;
        m_db->update("image_collection", data, "id = :id", whereParams);
    }
    loadImages();
}

void ImageCollection::onPermanentDelete()
{
    auto items = m_listWidget->selectedItems();
    if (items.isEmpty()) return;

    auto ret = QMessageBox::question(this, QStringLiteral("Confirm"),
                                     QStringLiteral("Permanently delete %1 image(s)?").arg(items.size()));
    if (ret != QMessageBox::Yes) return;

    for (auto* item : items) {
        auto info = item->data(Qt::UserRole).value<ImageInfo>();
        QFile::remove(storagePath() + info.storedName);
        m_thumbnailCache.remove(info.storedName);
        QVariantMap whereParams;
        whereParams["id"] = info.id;
        m_db->remove("image_collection", "id = :id", whereParams);
    }
    loadImages();
}

void ImageCollection::onEmptyRecycleBin()
{
    auto ret = QMessageBox::question(this, QStringLiteral("Confirm"),
                                     QStringLiteral("Permanently delete all items in recycle bin?"));
    if (ret != QMessageBox::Yes) return;

    QString sql = QStringLiteral("SELECT * FROM image_collection WHERE is_deleted = 1");
    auto result = m_db->select(sql);
    if (result.success) {
        for (const QVariant& row : result.data) {
            QVariantMap record = row.toMap();
            QString storedName = record.value("stored_name").toString();
            QFile::remove(storagePath() + storedName);
            m_thumbnailCache.remove(storedName);
        }
    }

    QVariantMap whereParams;
    whereParams["del"] = 1;
    m_db->remove("image_collection", "is_deleted = :del", whereParams);
    loadImages();
}

void ImageCollection::onEditTags()
{
    // Handled inline in context menu via tagsMenu
}

void ImageCollection::onOpenInExplorer()
{
    auto* item = m_listWidget->currentItem();
    if (!item) return;
    auto info = item->data(Qt::UserRole).value<ImageInfo>();
    QString filePath = storagePath() + info.storedName;

#ifdef Q_OS_MAC
    QProcess::startDetached("open", {"-R", filePath});
#elif defined(Q_OS_WIN)
    QProcess::startDetached("explorer", {"/select,", QDir::toNativeSeparators(filePath)});
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(storagePath()));
#endif
}

// ---- Drag & Drop ----

void ImageCollection::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        for (const QUrl& url : event->mimeData()->urls()) {
            if (url.isLocalFile()) {
                event->acceptProposedAction();
                return;
            }
        }
    }
}

void ImageCollection::dropEvent(QDropEvent* event)
{
    if (!event->mimeData()->hasUrls()) return;

    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile())
            importImage(url.toLocalFile());
    }
    loadImages();
    event->acceptProposedAction();
}

// ── 详情面板 ──

void ImageCollection::showDetail(const ImageInfo& info)
{
    m_currentDetailId = info.id;

    // 预览图
    QPixmap pix = getThumbnail(info.storedName, 280, 170);
    m_detailPreview->setPixmap(pix.scaled(m_detailPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

    // 基本信息
    m_detailFileName->setText(info.fileName);
    m_detailFileInfo->setText(
        tr("%1 x %2 | %3 | %4")
            .arg(info.width).arg(info.height)
            .arg(formatFileSize(info.fileSize))
            .arg(info.createdAt.toString("yyyy-MM-dd HH:mm")));

    // 填充 extras 表格
    m_extrasTable->setRowCount(0);
    for (const auto& kv : info.extras) {
        int row = m_extrasTable->rowCount();
        m_extrasTable->insertRow(row);
        m_extrasTable->setItem(row, 0, new QTableWidgetItem(kv.first));
        m_extrasTable->setItem(row, 1, new QTableWidgetItem(kv.second));
    }

    // OCR 结果
    m_ocrResultEdit->setPlainText(info.ocrText.isEmpty() ? tr("(未识别)") : info.ocrText);
}

void ImageCollection::onAddExtraRow()
{
    int row = m_extrasTable->rowCount();
    m_extrasTable->insertRow(row);
    m_extrasTable->setItem(row, 0, new QTableWidgetItem(""));
    m_extrasTable->setItem(row, 1, new QTableWidgetItem(""));
    m_extrasTable->editItem(m_extrasTable->item(row, 0));
}

void ImageCollection::onRemoveExtraRow()
{
    auto selected = m_extrasTable->selectedItems();
    if (!selected.isEmpty()) {
        m_extrasTable->removeRow(selected.first()->row());
    }
}

void ImageCollection::saveExtras(int imageId)
{
    if (imageId <= 0) return;

    QList<QPair<QString,QString>> extras;
    for (int i = 0; i < m_extrasTable->rowCount(); ++i) {
        QString key = m_extrasTable->item(i, 0) ? m_extrasTable->item(i, 0)->text().trimmed() : "";
        QString val = m_extrasTable->item(i, 1) ? m_extrasTable->item(i, 1)->text().trimmed() : "";
        if (!key.isEmpty())
            extras.append({key, val});
    }

    m_db->execute("UPDATE image_collection SET extras = :extras WHERE id = :id",
                  {{"extras", extrasToJson(extras)}, {"id", imageId}});

    // 更新列表中的数据
    for (int i = 0; i < m_listWidget->count(); ++i) {
        ImageInfo info = m_listWidget->item(i)->data(Qt::UserRole).value<ImageInfo>();
        if (info.id == imageId) {
            info.extras = extras;
            m_listWidget->item(i)->setData(Qt::UserRole, QVariant::fromValue(info));
            break;
        }
    }
}

QString ImageCollection::extrasToJson(const QList<QPair<QString,QString>>& extras) const
{
    QJsonArray arr;
    for (const auto& kv : extras) {
        QJsonObject obj;
        obj["k"] = kv.first;
        obj["v"] = kv.second;
        arr.append(obj);
    }
    return QJsonDocument(arr).toJson(QJsonDocument::Compact);
}

QList<QPair<QString,QString>> ImageCollection::extrasFromJson(const QString& json) const
{
    QList<QPair<QString,QString>> result;
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isArray()) return result;
    for (const auto& val : doc.array()) {
        QJsonObject obj = val.toObject();
        result.append({obj["k"].toString(), obj["v"].toString()});
    }
    return result;
}

void ImageCollection::onItemSelectionChanged()
{
    auto* item = m_listWidget->currentItem();
    if (item) {
        ImageInfo info = item->data(Qt::UserRole).value<ImageInfo>();
        showDetail(info);
    }
}

void ImageCollection::autoOcrImage(int imageId, const QString& storedName)
{
    QString imgPath = storagePath() + storedName;
    if (!QFile::exists(imgPath)) return;

    // 使用 PaddleOCR CLI 异步识别（轻量模式）
    auto* proc = new QProcess(this);

    // 补充 PATH（macOS .app 环境）
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString path = env.value("PATH");
    QStringList extraPaths = {
        "/opt/homebrew/bin", "/opt/homebrew/Caskroom/miniconda/base/bin",
        "/usr/local/bin", QDir::homePath() + "/miniconda3/bin",
        QDir::homePath() + "/anaconda3/bin", QDir::homePath() + "/.local/bin"
    };
    for (const auto& p : extraPaths)
        if (QDir(p).exists() && !path.contains(p)) path = p + ":" + path;
    env.insert("PATH", path);
    proc->setProcessEnvironment(env);

    // 输出到临时目录
    QString outDir = QDir::tempPath() + "/lele_ic_ocr_" + QUuid::createUuid().toString(QUuid::Id128);
    QDir().mkpath(outDir);

    proc->setProgram("paddleocr");
    proc->setArguments({"ocr", "-i", imgPath, "--save_path", outDir,
        "--use_doc_orientation_classify", "False",
        "--use_doc_unwarping", "False",
        "--use_textline_orientation", "False",
        "--text_detection_model_name", "PP-OCRv5_mobile_det",
        "--text_recognition_model_name", "PP-OCRv5_mobile_rec"});

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this, [this, proc, imageId, outDir](int, QProcess::ExitStatus) {

        // 解析 JSON 结果
        QDir dir(outDir);
        QStringList jsonFiles = dir.entryList({"*_res.json"}, QDir::Files);
        QString ocrText;
        if (!jsonFiles.isEmpty()) {
            QFile f(dir.filePath(jsonFiles.first()));
            if (f.open(QIODevice::ReadOnly)) {
                QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
                f.close();
                if (doc.isObject()) {
                    QJsonArray texts = doc.object()["rec_texts"].toArray();
                    QStringList lines;
                    for (const auto& t : texts) {
                        QString s = t.toString().trimmed();
                        if (!s.isEmpty()) lines.append(s);
                    }
                    ocrText = lines.join('\n');
                }
            }
        }
        QDir(outDir).removeRecursively();

        if (!ocrText.isEmpty()) {
            m_db->execute("UPDATE image_collection SET ocr_text = :text WHERE id = :id",
                          {{"text", ocrText}, {"id", imageId}});

            // 更新列表中的数据
            for (int i = 0; i < m_listWidget->count(); ++i) {
                ImageInfo info = m_listWidget->item(i)->data(Qt::UserRole).value<ImageInfo>();
                if (info.id == imageId) {
                    info.ocrText = ocrText;
                    m_listWidget->item(i)->setData(Qt::UserRole, QVariant::fromValue(info));
                    // 如果当前正在显示这张的详情，更新 OCR 区域
                    if (m_currentDetailId == imageId)
                        m_ocrResultEdit->setPlainText(ocrText);
                    break;
                }
            }
        }

        proc->deleteLater();
    });

    proc->start();
}
