#include "imagestopdf.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QImageReader>
#include <QPainter>
#include <QPageSize>
#include <QPdfWriter>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>
#include <QApplication>
#include <QDateTime>
#include <QTransform>
#include <QStyledItemDelegate>

REGISTER_DYNAMICOBJECT(ImagesToPdf);

// 自定义 delegate：显示缩略图 + 文件名 + 尺寸信息
class ImageListDelegate : public QStyledItemDelegate
{
public:
    explicit ImageListDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        // 背景
        if (option.state & QStyle::State_Selected) {
            painter->fillRect(option.rect, QColor(0xe3, 0xf2, 0xfd));
        } else if (option.state & QStyle::State_MouseOver) {
            painter->fillRect(option.rect, QColor(0xf8, 0xf9, 0xfa));
        }

        QRect rect = option.rect.adjusted(6, 4, -6, -4);

        // 缩略图
        QPixmap thumb = index.data(Qt::DecorationRole).value<QPixmap>();
        if (!thumb.isNull()) {
            qreal dpr = thumb.devicePixelRatio();
            int logW = static_cast<int>(thumb.width() / dpr);
            int logH = static_cast<int>(thumb.height() / dpr);
            int thumbSize = rect.height();
            if (logW > thumbSize || logH > thumbSize) {
                QSize fit = QSize(logW, logH).scaled(thumbSize, thumbSize, Qt::KeepAspectRatio);
                logW = fit.width();
                logH = fit.height();
            }
            int x = rect.left() + (thumbSize - logW) / 2;
            int y = rect.top() + (thumbSize - logH) / 2;
            painter->drawPixmap(QRect(x, y, logW, logH), thumb);
        }

        // 文字区域
        int textLeft = rect.left() + rect.height() + 8;
        QRect textRect(textLeft, rect.top(), rect.right() - textLeft, rect.height());

        // 文件名
        painter->setPen(QColor(0x21, 0x25, 0x29));
        QFont nameFont;
        nameFont.setPointSize(9);
        nameFont.setBold(true);
        painter->setFont(nameFont);
        QString name = index.data(Qt::DisplayRole).toString();
        painter->drawText(textRect.adjusted(0, 2, 0, -textRect.height() / 2),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          painter->fontMetrics().elidedText(name, Qt::ElideMiddle, textRect.width()));

        // 尺寸信息
        painter->setPen(QColor(0x86, 0x8e, 0x96));
        QFont infoFont;
        infoFont.setPointSize(8);
        painter->setFont(infoFont);
        QString info = index.data(Qt::UserRole + 1).toString();
        painter->drawText(textRect.adjusted(0, textRect.height() / 2, 0, -2),
                          Qt::AlignLeft | Qt::AlignVCenter, info);

        // 序号
        painter->setPen(QColor(0xad, 0xb5, 0xbd));
        QFont numFont;
        numFont.setPointSize(8);
        painter->setFont(numFont);
        QString num = QString("#%1").arg(index.row() + 1);
        painter->drawText(textRect.adjusted(0, 2, 0, -textRect.height() / 2),
                          Qt::AlignRight | Qt::AlignVCenter, num);

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override
    {
        return QSize(0, 64);
    }
};

ImagesToPdf::ImagesToPdf() : QWidget(nullptr), DynamicObjectBase()
{
    setAcceptDrops(true);
    setupUI();
}

void ImagesToPdf::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 6, 8, 6);
    mainLayout->setSpacing(4);

    setStyleSheet(R"(
        QPushButton {
            padding: 4px 10px;
            border: none;
            border-radius: 4px;
            font-size: 9pt;
            color: #495057;
            background: transparent;
        }
        QPushButton:hover { background-color: #e9ecef; }
        QPushButton:pressed { background-color: #dee2e6; }
        QPushButton:disabled { color: #adb5bd; }
        QListWidget {
            border: 1px solid #dee2e6;
            border-radius: 4px;
            background: #fff;
            outline: none;
        }
        QListWidget::item { border-bottom: 1px solid #f1f3f5; }
        QListWidget::item:last { border-bottom: none; }
    )");

    // 工具栏
    auto* toolbar = new QHBoxLayout();
    toolbar->setSpacing(2);

    m_addBtn = new QPushButton(tr("添加图片"));
    m_removeBtn = new QPushButton(tr("移除"));
    m_clearBtn = new QPushButton(tr("清空"));
    m_upBtn = new QPushButton(tr("上移"));
    m_downBtn = new QPushButton(tr("下移"));
    m_rotateLeftBtn = new QPushButton(tr("左旋"));
    m_rotateRightBtn = new QPushButton(tr("右旋"));
    m_exportBtn = new QPushButton(tr("导出 PDF"));
    m_exportBtn->setStyleSheet(
        "QPushButton { color: #228be6; font-weight: bold; }"
        "QPushButton:hover { background-color: #e7f5ff; }"
        "QPushButton:disabled { color: #adb5bd; font-weight: normal; }");

    toolbar->addWidget(m_addBtn);
    toolbar->addWidget(m_removeBtn);
    toolbar->addWidget(m_clearBtn);
    toolbar->addWidget(m_upBtn);
    toolbar->addWidget(m_downBtn);
    toolbar->addWidget(m_rotateLeftBtn);
    toolbar->addWidget(m_rotateRightBtn);
    toolbar->addStretch();
    toolbar->addWidget(m_exportBtn);

    mainLayout->addLayout(toolbar);

    // 提示
    auto* hint = new QLabel(tr("拖拽图片到此处，或点击\"添加图片\"选择文件。拖动列表项可调整顺序。"));
    hint->setStyleSheet("color: #868e96; font-size: 8pt; padding: 2px 0;");
    hint->setWordWrap(true);
    mainLayout->addWidget(hint);

    // 图片列表
    m_listWidget = new QListWidget();
    m_listWidget->setDragDropMode(QAbstractItemView::InternalMove);
    m_listWidget->setDefaultDropAction(Qt::MoveAction);
    m_listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_listWidget->setItemDelegate(new ImageListDelegate(this));
    m_listWidget->setMouseTracking(true);
    mainLayout->addWidget(m_listWidget, 1);

    // 状态栏
    m_statusLabel = new QLabel();
    m_statusLabel->setStyleSheet("color: #868e96; font-size: 8pt;");
    mainLayout->addWidget(m_statusLabel);

    // 连接
    connect(m_addBtn, &QPushButton::clicked, this, &ImagesToPdf::onAddImages);
    connect(m_removeBtn, &QPushButton::clicked, this, &ImagesToPdf::onRemoveSelected);
    connect(m_clearBtn, &QPushButton::clicked, this, &ImagesToPdf::onClearAll);
    connect(m_upBtn, &QPushButton::clicked, this, &ImagesToPdf::onMoveUp);
    connect(m_downBtn, &QPushButton::clicked, this, &ImagesToPdf::onMoveDown);
    connect(m_rotateLeftBtn, &QPushButton::clicked, this, &ImagesToPdf::onRotateLeft);
    connect(m_rotateRightBtn, &QPushButton::clicked, this, &ImagesToPdf::onRotateRight);
    connect(m_exportBtn, &QPushButton::clicked, this, &ImagesToPdf::onExportPdf);
    connect(m_listWidget->model(), &QAbstractItemModel::rowsMoved, this, [this]{ updateStatus(); });

    updateStatus();
}

void ImagesToPdf::addImageFiles(const QStringList& files)
{
    qreal dpr = qApp->devicePixelRatio();

    for (const QString& file : files) {
        QFileInfo fi(file);
        if (!fi.exists()) continue;

        QString ext = fi.suffix().toLower();
        if (!QImageReader::supportedImageFormats().contains(ext.toUtf8()))
            continue;

        // 读取尺寸
        QImageReader reader(file);
        reader.setAutoTransform(true);
        QSize imgSize = reader.size();

        // 缩略图（Retina 适配）
        int renderSize = static_cast<int>(56 * dpr);
        if (imgSize.isValid()) {
            reader.setScaledSize(imgSize.scaled(renderSize, renderSize, Qt::KeepAspectRatio));
        }
        QImage img = reader.read();
        QPixmap thumb = QPixmap::fromImage(img);
        thumb.setDevicePixelRatio(dpr);

        auto* item = new QListWidgetItem();
        item->setText(fi.fileName());
        item->setData(Qt::DecorationRole, thumb);
        item->setData(Qt::UserRole, file); // 完整路径

        // 信息文字
        QString info;
        if (imgSize.isValid())
            info = QString("%1x%2  |  %3").arg(imgSize.width()).arg(imgSize.height())
                       .arg(fi.size() < 1024*1024
                            ? QString("%1 KB").arg(fi.size() / 1024.0, 0, 'f', 1)
                            : QString("%1 MB").arg(fi.size() / (1024.0*1024.0), 0, 'f', 2));
        else
            info = QString("%1").arg(fi.size() < 1024*1024
                                    ? QString("%1 KB").arg(fi.size() / 1024.0, 0, 'f', 1)
                                    : QString("%1 MB").arg(fi.size() / (1024.0*1024.0), 0, 'f', 2));
        item->setData(Qt::UserRole + 1, info);

        m_listWidget->addItem(item);
    }
    updateStatus();
}

void ImagesToPdf::onAddImages()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this, tr("选择图片"), QString(),
        tr("图片文件 (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.tiff)"));
    if (!files.isEmpty())
        addImageFiles(files);
}

void ImagesToPdf::onRemoveSelected()
{
    auto items = m_listWidget->selectedItems();
    for (auto* item : items)
        delete item;
    updateStatus();
}

void ImagesToPdf::onClearAll()
{
    if (m_listWidget->count() == 0) return;
    if (QMessageBox::question(this, tr("确认"), tr("确定清空所有图片？"))
        != QMessageBox::Yes) return;
    m_listWidget->clear();
    updateStatus();
}

void ImagesToPdf::onMoveUp()
{
    int row = m_listWidget->currentRow();
    if (row <= 0) return;
    auto* item = m_listWidget->takeItem(row);
    m_listWidget->insertItem(row - 1, item);
    m_listWidget->setCurrentRow(row - 1);
    updateStatus();
}

void ImagesToPdf::onMoveDown()
{
    int row = m_listWidget->currentRow();
    if (row < 0 || row >= m_listWidget->count() - 1) return;
    auto* item = m_listWidget->takeItem(row);
    m_listWidget->insertItem(row + 1, item);
    m_listWidget->setCurrentRow(row + 1);
    updateStatus();
}

void ImagesToPdf::onRotateLeft()
{
    auto* item = m_listWidget->currentItem();
    if (!item) return;
    int rotation = (item->data(Qt::UserRole + 2).toInt() - 90) % 360;
    item->setData(Qt::UserRole + 2, rotation);

    // 更新缩略图
    QString filePath = item->data(Qt::UserRole).toString();
    qreal dpr = qApp->devicePixelRatio();
    int renderSize = static_cast<int>(56 * dpr);
    QImageReader reader(filePath);
    reader.setAutoTransform(true);
    QSize origSize = reader.size();
    if (origSize.isValid())
        reader.setScaledSize(origSize.scaled(renderSize, renderSize, Qt::KeepAspectRatio));
    QImage img = reader.read();
    if (rotation != 0) {
        QTransform t;
        t.rotate(rotation);
        img = img.transformed(t, Qt::SmoothTransformation);
    }
    QPixmap thumb = QPixmap::fromImage(img);
    thumb.setDevicePixelRatio(dpr);
    item->setData(Qt::DecorationRole, thumb);
}

void ImagesToPdf::onRotateRight()
{
    auto* item = m_listWidget->currentItem();
    if (!item) return;
    int rotation = (item->data(Qt::UserRole + 2).toInt() + 90) % 360;
    item->setData(Qt::UserRole + 2, rotation);

    QString filePath = item->data(Qt::UserRole).toString();
    qreal dpr = qApp->devicePixelRatio();
    int renderSize = static_cast<int>(56 * dpr);
    QImageReader reader(filePath);
    reader.setAutoTransform(true);
    QSize origSize = reader.size();
    if (origSize.isValid())
        reader.setScaledSize(origSize.scaled(renderSize, renderSize, Qt::KeepAspectRatio));
    QImage img = reader.read();
    if (rotation != 0) {
        QTransform t;
        t.rotate(rotation);
        img = img.transformed(t, Qt::SmoothTransformation);
    }
    QPixmap thumb = QPixmap::fromImage(img);
    thumb.setDevicePixelRatio(dpr);
    item->setData(Qt::DecorationRole, thumb);
}

void ImagesToPdf::onExportPdf()
{
    if (m_listWidget->count() == 0) {
        QMessageBox::warning(this, tr("提示"), tr("请先添加图片"));
        return;
    }

    QString defaultName = QString("images_%1.pdf")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    QString savePath = QFileDialog::getSaveFileName(
        this, tr("保存 PDF"), defaultName, tr("PDF 文件 (*.pdf)"));
    if (savePath.isEmpty()) return;

    QPdfWriter writer(savePath);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageMargins(QMarginsF(0, 0, 0, 0));
    writer.setResolution(300);

    QPainter painter(&writer);

    for (int i = 0; i < m_listWidget->count(); ++i) {
        if (i > 0)
            writer.newPage();

        QString filePath = m_listWidget->item(i)->data(Qt::UserRole).toString();
        int rotation = m_listWidget->item(i)->data(Qt::UserRole + 2).toInt();

        QImage image(filePath);
        if (image.isNull()) continue;

        // 应用旋转
        if (rotation != 0) {
            QTransform transform;
            transform.rotate(rotation);
            image = image.transformed(transform, Qt::SmoothTransformation);
        }

        // 计算缩放，让图片适应页面（保持比例）
        QRect pageRect = painter.viewport();
        QSize imgSize = image.size();
        imgSize.scale(pageRect.size(), Qt::KeepAspectRatio);

        int x = (pageRect.width() - imgSize.width()) / 2;
        int y = (pageRect.height() - imgSize.height()) / 2;

        painter.drawImage(QRect(x, y, imgSize.width(), imgSize.height()), image);
    }

    painter.end();

    QMessageBox::information(this, tr("完成"),
        tr("PDF 已保存到:\n%1\n\n共 %2 页").arg(savePath).arg(m_listWidget->count()));
}

void ImagesToPdf::updateStatus()
{
    int count = m_listWidget->count();
    m_exportBtn->setEnabled(count > 0);
    m_removeBtn->setEnabled(count > 0);
    m_clearBtn->setEnabled(count > 0);
    m_upBtn->setEnabled(m_listWidget->currentRow() > 0);
    m_downBtn->setEnabled(m_listWidget->currentRow() >= 0 &&
                          m_listWidget->currentRow() < count - 1);
    m_statusLabel->setText(tr("共 %1 张图片").arg(count));
}

void ImagesToPdf::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void ImagesToPdf::dropEvent(QDropEvent* event)
{
    QStringList files;
    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile())
            files << url.toLocalFile();
    }
    if (!files.isEmpty())
        addImageFiles(files);
}
