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
    return QSize(180, 200);
}

void ImageCardDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                              const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    QRect cardRect = option.rect.adjusted(4, 4, -4, -4);

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

    // Thumbnail
    auto info = index.data(Qt::UserRole).value<ImageInfo>();

    if (m_cache) {
        QPixmap thumb = m_cache->value(info.storedName);
        if (!thumb.isNull()) {
            QRect thumbArea(cardRect.left() + 10, cardRect.top() + 6, 160, 130);
            QPixmap scaled = thumb.scaled(thumbArea.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            int x = thumbArea.left() + (thumbArea.width() - scaled.width()) / 2;
            int y = thumbArea.top() + (thumbArea.height() - scaled.height()) / 2;
            painter->drawPixmap(x, y, scaled);
        }
    }

    // Filename
    QFont font;
    font.setPointSize(9);
    painter->setFont(font);
    painter->setPen(QColor(0x49, 0x50, 0x57));
    QRect textRect(cardRect.left() + 6, cardRect.top() + 140, cardRect.width() - 12, 20);
    QString elidedName = painter->fontMetrics().elidedText(info.fileName, Qt::ElideMiddle, textRect.width());
    painter->drawText(textRect, Qt::AlignHCenter | Qt::AlignVCenter, elidedName);

    // Tag chips
    if (!info.tags.isEmpty()) {
        auto colors = tagColors();
        QFont tagFont;
        tagFont.setPointSize(8);
        painter->setFont(tagFont);
        QFontMetrics tagFm(tagFont);

        int chipX = cardRect.left() + 6;
        int chipY = cardRect.top() + 164;
        int maxRight = cardRect.right() - 6;

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
        "  is_deleted INTEGER DEFAULT 0,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  deleted_at DATETIME"
        ");");
    m_db->execute(sql);
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
    m_listWidget->setGridSize(QSize(188, 208));
    m_listWidget->setSpacing(4);

    m_delegate = new ImageCardDelegate(this);
    m_delegate->setThumbnailCache(&m_thumbnailCache);
    m_listWidget->setItemDelegate(m_delegate);

    m_viewStack->addWidget(m_listWidget); // index 0

    // Table widget (table view) - index 1
    m_tableWidget = new QTableWidget;
    m_tableWidget->setColumnCount(6);
    m_tableWidget->setHorizontalHeaderLabels({
        QStringLiteral("Thumbnail"),
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

    m_mainLayout->addWidget(m_viewStack, 1);

    // Status bar
    m_statusLabel = new QLabel;
    m_statusLabel->setStyleSheet("font-size:9pt; color:#6c757d; padding:2px 0;");
    m_mainLayout->addWidget(m_statusLabel);

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

    QString filePath = storagePath() + storedName;
    QImageReader reader(filePath);
    reader.setAutoTransform(true);
    QSize origSize = reader.size();
    if (origSize.isValid()) {
        QSize scaled = origSize.scaled(maxWidth, maxHeight, Qt::KeepAspectRatio);
        reader.setScaledSize(scaled);
    }
    QImage image = reader.read();
    QPixmap pix = QPixmap::fromImage(image);
    if (!pix.isNull())
        m_thumbnailCache.insert(storedName, pix);
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
        item->setSizeHint(QSize(180, 200));
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
            thumbItem->setIcon(QIcon(thumb.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
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
