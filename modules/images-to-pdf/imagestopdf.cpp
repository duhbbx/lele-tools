#include "imagestopdf.h"

#include <QApplication>
#include <QBuffer>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QImageReader>
#include <QKeyEvent>
#include <QMessageBox>
#include <QMimeData>
#include <QPageSize>
#include <QPainter>
#include <QPalette>
#include <QPdfWriter>
#include <QScrollArea>
#include <QStandardPaths>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QTransform>
#include <QUrl>
#include <QWheelEvent>

#include <functional>

REGISTER_DYNAMICOBJECT(ImagesToPdf);

// 列表控件：既支持内部拖动重排，也接受从 Finder/其他应用拖入文件。
// 之前父 widget 的 dragEnter/drop 接不到事件 — QListWidget 的 InternalMove 模式
// 会把外部 URL 的 dragEnterEvent 默默吞掉，不冒泡到父级。
class ImageListWidget : public QListWidget
{
public:
    using QListWidget::QListWidget;
    void setExternalFileHandler(std::function<void(const QStringList&)> h) {
        m_handler = std::move(h);
    }
protected:
    void dragEnterEvent(QDragEnterEvent* e) override {
        if (e->mimeData()->hasUrls()) { e->acceptProposedAction(); return; }
        QListWidget::dragEnterEvent(e);
    }
    void dragMoveEvent(QDragMoveEvent* e) override {
        if (e->mimeData()->hasUrls()) { e->acceptProposedAction(); return; }
        QListWidget::dragMoveEvent(e);
    }
    void dropEvent(QDropEvent* e) override {
        if (e->mimeData()->hasUrls()) {
            QStringList files;
            for (const QUrl& u : e->mimeData()->urls()) {
                if (u.isLocalFile()) files << u.toLocalFile();
            }
            if (!files.isEmpty() && m_handler) {
                m_handler(files);
                e->acceptProposedAction();
                return;
            }
        }
        QListWidget::dropEvent(e);   // 让内部移动走默认逻辑
    }
private:
    std::function<void(const QStringList&)> m_handler;
};

// 预览对话框：双击列表项打开，滚轮 / +- / 0 / Esc 控制
class ImagePreviewDialog : public QDialog
{
public:
    ImagePreviewDialog(const QString& path, int rotation, QWidget* parent)
        : QDialog(parent)
    {
        setWindowTitle(QFileInfo(path).fileName());
        resize(960, 720);
        setStyleSheet(R"(
            QPushButton { padding: 4px 12px; border: 1px solid #ced4da;
                          border-radius: 4px; background: #fff; min-width: 56px; }
            QPushButton:hover  { background: #f1f3f5; }
            QPushButton:pressed{ background: #e9ecef; }
            QLabel#zoomLbl { color: #495057; min-width: 60px; font-size: 9pt; }
        )");

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->setSpacing(6);

        m_scroll = new QScrollArea(this);
        m_scroll->setWidgetResizable(false);
        m_scroll->setAlignment(Qt::AlignCenter);
        m_scroll->setBackgroundRole(QPalette::Dark);

        m_label = new QLabel();
        m_label->setAlignment(Qt::AlignCenter);
        m_label->setBackgroundRole(QPalette::Dark);
        m_scroll->setWidget(m_label);
        layout->addWidget(m_scroll, 1);

        m_image = QImage(path);
        if (rotation != 0 && !m_image.isNull()) {
            QTransform t; t.rotate(rotation);
            m_image = m_image.transformed(t, Qt::SmoothTransformation);
        }
        if (m_image.isNull()) m_label->setText(tr("无法加载图片"));

        auto* tbar = new QHBoxLayout();
        auto* zoomOut = new QPushButton("−");
        m_zoomLabel = new QLabel("100%");
        m_zoomLabel->setObjectName("zoomLbl");
        m_zoomLabel->setAlignment(Qt::AlignCenter);
        auto* zoomIn = new QPushButton("+");
        auto* zoom100 = new QPushButton(tr("1:1"));
        auto* zoomFit = new QPushButton(tr("适合窗口"));
        auto* closeBtn = new QPushButton(tr("关闭"));

        tbar->addWidget(zoomOut);
        tbar->addWidget(m_zoomLabel);
        tbar->addWidget(zoomIn);
        tbar->addSpacing(12);
        tbar->addWidget(zoomFit);
        tbar->addWidget(zoom100);
        tbar->addStretch();
        tbar->addWidget(closeBtn);
        layout->addLayout(tbar);

        connect(zoomOut, &QPushButton::clicked, this, [this]{ setZoom(m_zoom * 0.8); });
        connect(zoomIn,  &QPushButton::clicked, this, [this]{ setZoom(m_zoom * 1.25); });
        connect(zoom100, &QPushButton::clicked, this, [this]{ setZoom(1.0); });
        connect(zoomFit, &QPushButton::clicked, this, [this]{ fitToWindow(); });
        connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

        QTimer::singleShot(0, this, [this]{ fitToWindow(); });
    }
protected:
    void wheelEvent(QWheelEvent* e) override {
        double f = (e->angleDelta().y() > 0) ? 1.15 : (1.0 / 1.15);
        setZoom(m_zoom * f);
        e->accept();
    }
    void keyPressEvent(QKeyEvent* e) override {
        switch (e->key()) {
            case Qt::Key_Escape: accept(); return;
            case Qt::Key_Plus: case Qt::Key_Equal:
                setZoom(m_zoom * 1.25); return;
            case Qt::Key_Minus:
                setZoom(m_zoom * 0.8); return;
            case Qt::Key_0:
                setZoom(1.0); return;
        }
        QDialog::keyPressEvent(e);
    }
private:
    void setZoom(double z) {
        m_zoom = qBound(0.05, z, 10.0);
        if (m_image.isNull()) return;
        QSize sz(int(m_image.width()  * m_zoom + 0.5),
                 int(m_image.height() * m_zoom + 0.5));
        m_label->setPixmap(QPixmap::fromImage(m_image).scaled(
            sz, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        m_label->resize(sz);
        m_zoomLabel->setText(QString("%1%").arg(int(m_zoom * 100 + 0.5)));
    }
    void fitToWindow() {
        if (m_image.isNull()) return;
        QSize avail = m_scroll->viewport()->size();
        if (avail.width() <= 0 || avail.height() <= 0) return;
        double zw = double(avail.width())  / m_image.width();
        double zh = double(avail.height()) / m_image.height();
        setZoom(qMin(zw, zh) * 0.98);
    }
    QImage m_image;
    QScrollArea* m_scroll = nullptr;
    QLabel* m_label = nullptr;
    QLabel* m_zoomLabel = nullptr;
    double m_zoom = 1.0;
};

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

    auto* qLabel = new QLabel(tr("质量:"));
    qLabel->setStyleSheet("color:#495057; font-size:9pt; padding:0 4px 0 8px;");
    toolbar->addWidget(qLabel);

    m_qualityCombo = new QComboBox();
    m_qualityCombo->addItem(tr("高 (300 DPI)"));
    m_qualityCombo->addItem(tr("中 (150 DPI)"));
    m_qualityCombo->addItem(tr("低 (100 DPI)"));
    m_qualityCombo->addItem(tr("原始 (无压缩)"));
    m_qualityCombo->setCurrentIndex(1);   // 默认"中"
    m_qualityCombo->setToolTip(tr(
        "高：300 DPI + JPEG q95，接近无损，体积最大\n"
        "中：150 DPI + JPEG q85，肉眼无差别，体积约 1/4（推荐）\n"
        "低：100 DPI + JPEG q70，可读，体积最小\n"
        "原始：保留原图分辨率，不重采样（旧行为）"));
    m_qualityCombo->setStyleSheet(
        "QComboBox { padding: 3px 8px; border: 1px solid #ced4da; "
        "border-radius: 4px; background: #fff; min-width: 120px; font-size: 9pt; }");
    toolbar->addWidget(m_qualityCombo);

    toolbar->addWidget(m_exportBtn);

    mainLayout->addLayout(toolbar);

    // 提示
    auto* hint = new QLabel(tr("拖拽图片到此处，或点击\"添加图片\"选择文件。拖动列表项可调整顺序。"));
    hint->setStyleSheet("color: #868e96; font-size: 8pt; padding: 2px 0;");
    hint->setWordWrap(true);
    mainLayout->addWidget(hint);

    // 图片列表 — 用自定义子类支持外部文件拖入
    auto* imgList = new ImageListWidget();
    imgList->setExternalFileHandler([this](const QStringList& files) {
        addImageFiles(files);
    });
    m_listWidget = imgList;
    m_listWidget->setDragDropMode(QAbstractItemView::DragDrop);
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

    // 双击列表项 → 弹预览
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem* item) {
        if (!item) return;
        QString path = item->data(Qt::UserRole).toString();
        if (path.isEmpty() || !QFileInfo::exists(path)) return;
        int rotation = item->data(Qt::UserRole + 2).toInt();
        ImagePreviewDialog dlg(path, rotation, this);
        dlg.exec();
    });

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

    QString downloadDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    QString defaultPath = downloadDir + QString("/images_%1.pdf")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    QString savePath = QFileDialog::getSaveFileName(
        this, tr("保存 PDF"), defaultPath, tr("PDF 文件 (*.pdf)"));
    if (savePath.isEmpty()) return;

    // 压缩档位：targetDPI = 0 表示保留原图分辨率；jpegQ = 0 表示不做 JPEG round-trip
    struct QualityPreset { int dpi; int jpegQ; };
    const QualityPreset presets[] = {
        {300, 95},   // 高
        {150, 85},   // 中（默认）
        {100, 70},   // 低
        {0,    0},   // 原始
    };
    int qi = qBound(0, m_qualityCombo->currentIndex(),
                    int(sizeof(presets) / sizeof(presets[0])) - 1);
    const int targetDPI = presets[qi].dpi;
    const int jpegQ     = presets[qi].jpegQ;
    const int pdfDPI    = 300;

    QPdfWriter writer(savePath);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageMargins(QMarginsF(0, 0, 0, 0));
    writer.setResolution(pdfDPI);

    QPainter painter(&writer);

    for (int i = 0; i < m_listWidget->count(); ++i) {
        if (i > 0)
            writer.newPage();

        QString filePath = m_listWidget->item(i)->data(Qt::UserRole).toString();
        int rotation = m_listWidget->item(i)->data(Qt::UserRole + 2).toInt();

        QImage image(filePath);
        if (image.isNull()) continue;

        if (rotation != 0) {
            QTransform transform;
            transform.rotate(rotation);
            image = image.transformed(transform, Qt::SmoothTransformation);
        }

        // 页面上的目标显示尺寸（保持比例）
        QRect pageRect = painter.viewport();
        QSize fitToPage = image.size();
        fitToPage.scale(pageRect.size(), Qt::KeepAspectRatio);

        // 重采样 + JPEG 预压缩（核心瘦身环节）
        if (targetDPI > 0) {
            // 嵌入图片所需的实际像素数 = 页面显示像素 × (目标DPI / PDF渲染DPI)
            const double scaleF = double(targetDPI) / pdfDPI;
            const QSize targetPx(int(fitToPage.width()  * scaleF + 0.5),
                                 int(fitToPage.height() * scaleF + 0.5));
            if (image.width() > targetPx.width() && targetPx.width() > 0) {
                image = image.scaled(targetPx, Qt::KeepAspectRatio,
                                     Qt::SmoothTransformation);
            }
            // JPEG round-trip 降低数据熵 → PDF 内部 zlib 再压时体积更小。
            // 跳过有透明通道的图（JPEG 不支持 alpha）
            if (jpegQ > 0 && !image.hasAlphaChannel()) {
                QByteArray jpegBytes;
                QBuffer buf(&jpegBytes);
                buf.open(QIODevice::WriteOnly);
                if (image.save(&buf, "JPEG", jpegQ)) {
                    buf.close();
                    QImage roundTripped = QImage::fromData(jpegBytes, "JPEG");
                    if (!roundTripped.isNull()) image = roundTripped;
                } else {
                    buf.close();
                }
            }
        }

        int x = (pageRect.width()  - fitToPage.width())  / 2;
        int y = (pageRect.height() - fitToPage.height()) / 2;
        painter.drawImage(QRect(x, y, fitToPage.width(), fitToPage.height()), image);
    }

    painter.end();

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("导出完成"));
    msgBox.setText(tr("PDF 已保存到:\n%1\n\n共 %2 页").arg(savePath).arg(m_listWidget->count()));
    msgBox.setIcon(QMessageBox::Information);

    auto* openFileBtn = msgBox.addButton(tr("打开 PDF"), QMessageBox::ActionRole);
    auto* openDirBtn = msgBox.addButton(tr("打开所在目录"), QMessageBox::ActionRole);
    msgBox.addButton(QMessageBox::Ok);

    msgBox.exec();

    if (msgBox.clickedButton() == openFileBtn) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(savePath));
    } else if (msgBox.clickedButton() == openDirBtn) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(savePath).absolutePath()));
    }
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
