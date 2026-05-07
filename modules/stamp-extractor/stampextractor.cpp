#include "stampextractor.h"

#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QCursor>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QSlider>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtMath>

REGISTER_DYNAMICOBJECT(StampExtractor);

// ============== ImageView ==============

namespace stampextractor {

ImageView::ImageView(QWidget* parent) : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::WheelFocus);
    setMinimumSize(360, 320);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAutoFillBackground(false);
}

void ImageView::setImage(const QImage& img)
{
    m_image = img;
    m_lassoPath = QPainterPath();
    m_lassoPoly.clear();
    fitToWidget();
}

void ImageView::setView(double zoom, QPointF offset)
{
    m_zoom = qBound(0.05, zoom, 30.0);
    m_offset = offset;
    update();
}

void ImageView::fitToWidget()
{
    if (m_image.isNull() || width() <= 2 || height() <= 2) {
        m_zoom = 1.0;
        m_offset = QPointF(0, 0);
        update();
        return;
    }
    const int margin = 8;
    double zw = double(width() - margin * 2) / m_image.width();
    double zh = double(height() - margin * 2) / m_image.height();
    m_zoom = qMin(zw, zh);
    if (m_zoom <= 0) m_zoom = 1.0;
    QSizeF disp = QSizeF(m_image.size()) * m_zoom;
    m_offset = QPointF((width() - disp.width()) / 2.0, (height() - disp.height()) / 2.0);
    update();
    emit viewChanged(m_zoom, m_offset);
}

void ImageView::setShowCheckerboard(bool b)
{
    m_showChecker = b;
    update();
}

void ImageView::setLassoMode(bool on)
{
    m_lassoMode = on;
    if (!on) m_drawingLasso = false;
    setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
}

void ImageView::clearLasso()
{
    m_lassoPath = QPainterPath();
    m_lassoPoly.clear();
    update();
}

void ImageView::setPlaceholder(const QString& t)
{
    m_placeholder = t;
    update();
}

QPointF ImageView::widgetToImage(const QPointF& w) const
{
    if (m_zoom <= 0) return QPointF();
    return QPointF((w.x() - m_offset.x()) / m_zoom,
                   (w.y() - m_offset.y()) / m_zoom);
}

QPointF ImageView::imageToWidget(const QPointF& i) const
{
    return QPointF(m_offset.x() + i.x() * m_zoom,
                   m_offset.y() + i.y() * m_zoom);
}

void ImageView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0xf1, 0xf3, 0xf5));

    if (m_image.isNull()) {
        p.setPen(QColor(0x86, 0x8e, 0x96));
        p.drawText(rect(), Qt::AlignCenter, m_placeholder);
        p.setPen(QColor(0xce, 0xd4, 0xda));
        p.setBrush(Qt::NoBrush);
        p.drawRect(rect().adjusted(0, 0, -1, -1));
        return;
    }

    QSizeF dispSize = QSizeF(m_image.size()) * m_zoom;
    QRectF dst(m_offset, dispSize);

    if (m_showChecker) {
        // 棋盘格作为透明背景
        const int tile = 10;
        const QColor c1(255, 255, 255);
        const QColor c2(220, 220, 220);
        QRect dstRect = dst.toAlignedRect().intersected(rect());
        for (int y = dstRect.top(); y < dstRect.bottom() + 1; y += tile) {
            for (int x = dstRect.left(); x < dstRect.right() + 1; x += tile) {
                bool dark = ((x / tile) + (y / tile)) % 2;
                p.fillRect(x, y, tile, tile, dark ? c2 : c1);
            }
        }
    }

    p.setRenderHint(QPainter::SmoothPixmapTransform, m_zoom < 4.0);
    p.drawImage(dst, m_image);

    // 套索：finalized
    if (!m_lassoPath.isEmpty()) {
        QPainterPath wpath;
        QPolygonF poly = m_lassoPath.toFillPolygon();
        QPolygonF wpoly;
        for (const QPointF& pt : poly) wpoly << imageToWidget(pt);
        wpath.addPolygon(wpoly);
        p.setPen(QPen(QColor(40, 120, 220), 2, Qt::SolidLine));
        p.setBrush(QColor(40, 120, 220, 35));
        p.drawPath(wpath);
    }
    // 套索：drawing in progress
    if (m_drawingLasso && m_lassoPoly.size() > 1) {
        QPolygonF wpoly;
        for (const QPointF& pt : m_lassoPoly) wpoly << imageToWidget(pt);
        p.setPen(QPen(QColor(220, 50, 50), 2, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawPolyline(wpoly);
    }

    // 边框
    p.setPen(QColor(0xce, 0xd4, 0xda));
    p.setBrush(Qt::NoBrush);
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}

void ImageView::wheelEvent(QWheelEvent* e)
{
    if (m_image.isNull()) { e->ignore(); return; }
    double delta = e->angleDelta().y();
    if (delta == 0) { e->ignore(); return; }
    double factor = (delta > 0) ? 1.15 : 1.0 / 1.15;
    double newZoom = qBound(0.05, m_zoom * factor, 30.0);
    if (qFuzzyCompare(newZoom, m_zoom)) { e->accept(); return; }

    QPointF mouse = e->position();
    QPointF imgPt = widgetToImage(mouse);
    m_zoom = newZoom;
    QPointF newWidgetPt = imageToWidget(imgPt);
    m_offset += (mouse - newWidgetPt);

    update();
    emit viewChanged(m_zoom, m_offset);
    e->accept();
}

void ImageView::mousePressEvent(QMouseEvent* e)
{
    if (m_image.isNull()) { e->ignore(); return; }
    if (e->button() == Qt::LeftButton) {
        if (m_lassoMode) {
            m_drawingLasso = true;
            m_lassoPoly.clear();
            m_lassoPoly << widgetToImage(e->position());
            update();
        } else {
            m_panning = true;
            m_lastMousePos = e->pos();
            setCursor(Qt::ClosedHandCursor);
        }
        e->accept();
        return;
    }
    if (e->button() == Qt::MiddleButton) {
        m_panning = true;
        m_lastMousePos = e->pos();
        setCursor(Qt::ClosedHandCursor);
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void ImageView::mouseMoveEvent(QMouseEvent* e)
{
    if (m_drawingLasso) {
        QPointF p = widgetToImage(e->position());
        // 简单稀疏化：与上一点距离>1像素才追加，避免点过多
        if (m_lassoPoly.isEmpty() ||
            QLineF(m_lassoPoly.last(), p).length() > 1.0) {
            m_lassoPoly << p;
            update();
        }
        e->accept();
        return;
    }
    if (m_panning) {
        QPoint delta = e->pos() - m_lastMousePos;
        m_offset += QPointF(delta);
        m_lastMousePos = e->pos();
        update();
        emit viewChanged(m_zoom, m_offset);
        e->accept();
        return;
    }
    QWidget::mouseMoveEvent(e);
}

void ImageView::mouseReleaseEvent(QMouseEvent* e)
{
    if (m_drawingLasso && (e->button() == Qt::LeftButton)) {
        m_drawingLasso = false;
        if (m_lassoPoly.size() >= 3) {
            m_lassoPath = QPainterPath();
            m_lassoPath.addPolygon(m_lassoPoly);
            m_lassoPath.closeSubpath();
            emit lassoCompleted(m_lassoPath);
        }
        m_lassoPoly.clear();
        update();
        e->accept();
        return;
    }
    if (m_panning && (e->button() == Qt::LeftButton || e->button() == Qt::MiddleButton)) {
        m_panning = false;
        setCursor(m_lassoMode ? Qt::CrossCursor : Qt::ArrowCursor);
        e->accept();
        return;
    }
    QWidget::mouseReleaseEvent(e);
}

void ImageView::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    // 如果当前是默认（fit）状态，跟随 widget 尺寸重新 fit；否则保持
    // 简化：每次 resize 都 fit（不会闪 — 重绘流畅）
    if (!m_image.isNull()) fitToWidget();
}

} // namespace stampextractor


// ============== StampExtractor ==============

StampExtractor::StampExtractor() : QWidget(nullptr), DynamicObjectBase()
{
    m_color = QColor(220, 30, 30);
    setAcceptDrops(true);
    setupUi();
}

void StampExtractor::setupUi()
{
    auto* main = new QVBoxLayout(this);
    main->setContentsMargins(8, 6, 8, 6);
    main->setSpacing(6);

    setStyleSheet(R"(
        QPushButton {
            padding: 5px 12px;
            border: none;
            border-radius: 4px;
            font-size: 9pt;
            color: #495057;
            background: transparent;
        }
        QPushButton:hover { background-color: #e9ecef; }
        QPushButton:pressed { background-color: #dee2e6; }
        QPushButton:checked { background-color: #d0ebff; color: #1864ab; }
        QPushButton:disabled { color: #adb5bd; }
    )");

    // 工具栏
    auto* topRow = new QHBoxLayout();
    m_openBtn = new QPushButton(tr("选择图片..."));
    topRow->addWidget(m_openBtn);

    topRow->addSpacing(8);
    m_lassoBtn = new QPushButton(tr("套索"));
    m_lassoBtn->setCheckable(true);
    m_lassoBtn->setToolTip(tr("启用后在原图上按住左键拖动，圈出要保留的区域"));
    topRow->addWidget(m_lassoBtn);
    m_clearLassoBtn = new QPushButton(tr("清除套索"));
    topRow->addWidget(m_clearLassoBtn);
    m_fitBtn = new QPushButton(tr("适应窗口"));
    topRow->addWidget(m_fitBtn);

    topRow->addSpacing(12);
    topRow->addWidget(new QLabel(tr("输出颜色:")));
    m_colorBtn = new QPushButton();
    m_colorBtn->setFixedSize(48, 26);
    topRow->addWidget(m_colorBtn);
    topRow->addSpacing(8);
    m_resetBtn = new QPushButton(tr("还原默认阈值"));
    topRow->addWidget(m_resetBtn);

    topRow->addStretch();
    m_saveBtn = new QPushButton(tr("保存为图章"));
    m_saveBtn->setEnabled(false);
    m_saveBtn->setStyleSheet(
        "QPushButton { color: #228be6; font-weight: bold; }"
        "QPushButton:hover { background-color: #e7f5ff; }"
        "QPushButton:disabled { color: #adb5bd; font-weight: normal; }");
    topRow->addWidget(m_saveBtn);
    main->addLayout(topRow);

    // 提示
    m_hint = new QLabel(tr(
        "拖入图片或点击\"选择图片...\"。滚轮缩放原图，左键拖动平移；\"套索\"启用后画自由多边形圈出图章。"));
    m_hint->setWordWrap(true);
    m_hint->setStyleSheet("color:#868e96; font-size:9pt; padding:2px 0;");
    main->addWidget(m_hint);

    // 双视图
    auto* previewRow = new QHBoxLayout();
    previewRow->setSpacing(8);
    m_srcView = new stampextractor::ImageView();
    m_srcView->setPlaceholder(tr("原图（拖入图片到此处）"));
    m_dstView = new stampextractor::ImageView();
    m_dstView->setShowCheckerboard(true);
    m_dstView->setPlaceholder(tr("提取结果"));
    previewRow->addWidget(m_srcView, 1);
    previewRow->addWidget(m_dstView, 1);
    main->addLayout(previewRow, 1);

    // 滑块
    auto* grid = new QGridLayout();
    grid->setColumnStretch(1, 1);
    grid->setHorizontalSpacing(10);

    grid->addWidget(new QLabel(tr("最小红色强度:")), 0, 0);
    m_redSlider = new QSlider(Qt::Horizontal);
    m_redSlider->setRange(0, 255);
    m_redSlider->setValue(80);
    grid->addWidget(m_redSlider, 0, 1);
    m_redValueLabel = new QLabel("80");
    m_redValueLabel->setMinimumWidth(40);
    grid->addWidget(m_redValueLabel, 0, 2);

    grid->addWidget(new QLabel(tr("红色优势度:")), 1, 0);
    m_sensSlider = new QSlider(Qt::Horizontal);
    m_sensSlider->setRange(0, 200);
    m_sensSlider->setValue(30);
    grid->addWidget(m_sensSlider, 1, 1);
    m_sensValueLabel = new QLabel("30");
    m_sensValueLabel->setMinimumWidth(40);
    grid->addWidget(m_sensValueLabel, 1, 2);

    grid->addWidget(new QLabel(tr("色相偏移:")), 2, 0);
    m_hueSlider = new QSlider(Qt::Horizontal);
    m_hueSlider->setRange(-180, 180);
    m_hueSlider->setValue(0);
    m_hueSlider->setTickPosition(QSlider::TicksBelow);
    m_hueSlider->setTickInterval(60);
    grid->addWidget(m_hueSlider, 2, 1);
    m_hueValueLabel = new QLabel("0°");
    m_hueValueLabel->setMinimumWidth(40);
    grid->addWidget(m_hueValueLabel, 2, 2);

    grid->addWidget(new QLabel(tr("饱和度:")), 3, 0);
    m_satSlider = new QSlider(Qt::Horizontal);
    m_satSlider->setRange(0, 200);
    m_satSlider->setValue(100);
    m_satSlider->setTickPosition(QSlider::TicksBelow);
    m_satSlider->setTickInterval(50);
    grid->addWidget(m_satSlider, 3, 1);
    m_satValueLabel = new QLabel("100%");
    m_satValueLabel->setMinimumWidth(40);
    grid->addWidget(m_satValueLabel, 3, 2);

    // 当前生效颜色的小色块预览
    auto* effRow = new QHBoxLayout();
    effRow->addWidget(new QLabel(tr("生效颜色:")));
    m_effectiveSwatch = new QLabel();
    m_effectiveSwatch->setFixedSize(48, 22);
    m_effectiveSwatch->setStyleSheet("background:#dc1e1e; border:1px solid #888; border-radius:3px;");
    effRow->addWidget(m_effectiveSwatch);
    effRow->addStretch();
    grid->addLayout(effRow, 4, 1, 1, 2);

    m_autoTrim = new QCheckBox(tr("保存时自动裁剪透明边距"));
    m_autoTrim->setChecked(true);
    grid->addWidget(m_autoTrim, 5, 1, 1, 2);

    main->addLayout(grid);

    applyColorButtonStyle();

    connect(m_openBtn, &QPushButton::clicked, this, &StampExtractor::onOpenImage);
    connect(m_colorBtn, &QPushButton::clicked, this, &StampExtractor::onPickColor);
    connect(m_saveBtn, &QPushButton::clicked, this, &StampExtractor::onSave);
    connect(m_resetBtn, &QPushButton::clicked, this, &StampExtractor::onResetThresholds);
    connect(m_lassoBtn, &QPushButton::toggled, this, &StampExtractor::onToggleLasso);
    connect(m_clearLassoBtn, &QPushButton::clicked, this, &StampExtractor::onClearLasso);
    connect(m_fitBtn, &QPushButton::clicked, this, &StampExtractor::onFitView);

    connect(m_redSlider, &QSlider::valueChanged, this, [this](int v){
        m_redValueLabel->setText(QString::number(v));
        recomputeExtraction();
    });
    connect(m_sensSlider, &QSlider::valueChanged, this, [this](int v){
        m_sensValueLabel->setText(QString::number(v));
        recomputeExtraction();
    });
    connect(m_hueSlider, &QSlider::valueChanged, this, [this](int v){
        m_hueValueLabel->setText(QString("%1°").arg(v));
        applyColorButtonStyle();
        recomputeExtraction();
    });
    connect(m_satSlider, &QSlider::valueChanged, this, [this](int v){
        m_satValueLabel->setText(QString("%1%").arg(v));
        applyColorButtonStyle();
        recomputeExtraction();
    });

    // 同步两个视图的缩放与平移（套索激活时不同步——此时两边显示不同区域）
    connect(m_srcView, &stampextractor::ImageView::viewChanged, this,
            [this](double z, QPointF o){
                if (m_srcView->hasLasso()) return;
                QSignalBlocker b(m_dstView);
                m_dstView->setView(z, o);
            });
    connect(m_dstView, &stampextractor::ImageView::viewChanged, this,
            [this](double z, QPointF o){
                if (m_srcView->hasLasso()) return;
                QSignalBlocker b(m_srcView);
                m_srcView->setView(z, o);
            });

    // 套索完成 → 重算提取
    connect(m_srcView, &stampextractor::ImageView::lassoCompleted, this,
            [this](const QPainterPath&){
                recomputeExtraction();
            });
}

void StampExtractor::applyColorButtonStyle()
{
    m_colorBtn->setStyleSheet(
        QString("QPushButton { background-color: %1; border: 1px solid #888; border-radius: 3px; }"
                "QPushButton:hover { border: 1px solid #228be6; }")
            .arg(m_color.name()));
    if (m_effectiveSwatch) {
        m_effectiveSwatch->setStyleSheet(
            QString("background:%1; border:1px solid #888; border-radius:3px;")
                .arg(effectiveColor().name()));
    }
}

QColor StampExtractor::effectiveColor() const
{
    int h, s, l, a;
    m_color.getHsl(&h, &s, &l, &a);
    if (h < 0) h = 0;  // 当 base 是灰度时 H 为 -1，钳到 0 避免越界
    int hueShift = m_hueSlider ? m_hueSlider->value() : 0;
    int satScale = m_satSlider ? m_satSlider->value() : 100;
    h = ((h + hueShift) % 360 + 360) % 360;
    s = qBound(0, int(s * satScale / 100.0), 255);
    QColor out;
    out.setHsl(h, s, l, a);
    return out;
}

void StampExtractor::onOpenImage()
{
    QString f = QFileDialog::getOpenFileName(
        this, tr("选择带图章的图片"), QString(),
        tr("图片 (*.png *.jpg *.jpeg *.bmp *.tiff *.webp)"));
    if (f.isEmpty()) return;
    loadImageFromFile(f);
}

void StampExtractor::loadImageFromFile(const QString& path)
{
    QImage img(path);
    if (img.isNull()) {
        QMessageBox::warning(this, tr("打开失败"), tr("无法读取图片：%1").arg(path));
        return;
    }
    const int maxDim = 1600;
    if (img.width() > maxDim || img.height() > maxDim) {
        img = img.scaled(maxDim, maxDim, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    m_source = img.convertToFormat(QImage::Format_ARGB32);
    m_srcView->setImage(m_source);
    m_dstView->setImage(QImage(m_source.size(), QImage::Format_ARGB32));  // 占位空白
    // 同步视图状态（dstView 的 fit 用同样尺寸）
    m_dstView->setView(m_srcView->zoom(), m_srcView->offset());

    m_hint->setText(tr("已加载：%1（%2x%3）。滚轮缩放，拖动平移；"
                       "需要更精确选取时点\"套索\"圈出图章。")
                        .arg(QFileInfo(path).fileName())
                        .arg(m_source.width()).arg(m_source.height()));
    m_saveBtn->setEnabled(true);
    recomputeExtraction();
}

void StampExtractor::onPickColor()
{
    QColor c = QColorDialog::getColor(m_color, this, tr("选择输出颜色"));
    if (!c.isValid()) return;
    m_color = c;
    applyColorButtonStyle();
    recomputeExtraction();
}

void StampExtractor::onResetThresholds()
{
    m_redSlider->setValue(80);
    m_sensSlider->setValue(30);
    m_hueSlider->setValue(0);
    m_satSlider->setValue(100);
}

void StampExtractor::onToggleLasso(bool on)
{
    m_srcView->setLassoMode(on);
    if (on) {
        m_hint->setText(tr("套索已启用：在原图上按住左键拖动，圈出图章区域；松开即生效。"));
    } else {
        m_hint->setText(tr("套索已关闭。可拖动平移，滚轮缩放；需要选区时再次启用套索。"));
    }
}

void StampExtractor::onClearLasso()
{
    m_srcView->clearLasso();
    recomputeExtraction();
}

void StampExtractor::onFitView()
{
    m_srcView->fitToWidget();
    // viewChanged 会同步到 dstView
}

void StampExtractor::recomputeExtraction()
{
    if (m_source.isNull()) return;

    bool useLasso = m_srcView->hasLasso();
    QPainterPath lasso = useLasso ? m_srcView->lassoPath() : QPainterPath();
    QImage mask = useLasso ? rasterizeMask(lasso, m_source.size()) : QImage();

    QImage full = extractRedStamp(m_source, m_redSlider->value(),
                                   m_sensSlider->value(), effectiveColor(), mask);

    if (useLasso) {
        // 裁剪到套索包围盒 + 自适应边距，结果视图只显示这一小块
        QRectF bb = lasso.boundingRect();
        const int pad = qMax(24, int(qMin(bb.width(), bb.height()) * 0.08));
        QRect crop(qFloor(bb.x()), qFloor(bb.y()),
                   qCeil(bb.width()), qCeil(bb.height()));
        crop = crop.adjusted(-pad, -pad, pad, pad)
                   .intersected(QRect(QPoint(0, 0), m_source.size()));
        if (crop.isEmpty()) {
            m_extracted = full;
        } else {
            m_extracted = full.copy(crop);
        }
        // 套索结果用 fitToWidget，独立于源视图
        QSignalBlocker b(m_dstView);
        m_dstView->setImage(m_extracted);   // setImage 内部会 fitToWidget
    } else {
        // 全图模式：与源视图保持同步
        m_extracted = full;
        double z = m_srcView->zoom();
        QPointF o = m_srcView->offset();
        QSignalBlocker b(m_dstView);
        m_dstView->setImage(m_extracted);
        m_dstView->setView(z, o);
    }
    m_dstView->update();
}

void StampExtractor::onSave()
{
    if (m_extracted.isNull() || isExtractEmpty(m_extracted)) {
        QMessageBox::warning(this, tr("无内容"),
                             tr("提取结果为空，请检查源图片或调整阈值。"));
        return;
    }
    QImage out = m_autoTrim->isChecked() ? trimTransparent(m_extracted) : m_extracted;

    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) base = QDir::homePath() + "/.lele-tools";
    QString dir = base + "/pdf-stamp";
    QDir().mkpath(dir);

    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    QString fullPath = dir + QString("/stamp_%1.png").arg(id);

    if (!out.save(fullPath, "PNG")) {
        QMessageBox::warning(this, tr("保存失败"), tr("无法写入：%1").arg(fullPath));
        return;
    }

    QMessageBox box(this);
    box.setWindowTitle(tr("已保存"));
    box.setText(tr("图章已加入 PDF 盖章工具的资源库:\n%1").arg(fullPath));
    box.setIcon(QMessageBox::Information);
    auto* dirBtn = box.addButton(tr("打开所在目录"), QMessageBox::ActionRole);
    auto* anotherBtn = box.addButton(tr("再抠一张"), QMessageBox::ActionRole);
    box.addButton(QMessageBox::Ok);
    box.exec();

    if (box.clickedButton() == dirBtn) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    } else if (box.clickedButton() == anotherBtn) {
        onOpenImage();
    }
}

void StampExtractor::dragEnterEvent(QDragEnterEvent* e)
{
    if (e->mimeData()->hasUrls()) {
        for (const QUrl& u : e->mimeData()->urls()) {
            if (u.isLocalFile()) {
                e->acceptProposedAction();
                return;
            }
        }
    }
}

void StampExtractor::dropEvent(QDropEvent* e)
{
    for (const QUrl& u : e->mimeData()->urls()) {
        if (!u.isLocalFile()) continue;
        QString path = u.toLocalFile();
        QImageReader reader(path);
        if (!reader.canRead()) continue;
        loadImageFromFile(path);
        e->acceptProposedAction();
        return;
    }
}

// ============== 算法 ==============

QImage StampExtractor::rasterizeMask(const QPainterPath& path, const QSize& size)
{
    QImage mask(size, QImage::Format_Grayscale8);
    mask.fill(0);
    QPainter p(&mask);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setBrush(Qt::white);
    p.setPen(Qt::NoPen);
    p.drawPath(path);
    p.end();
    return mask;
}

QImage StampExtractor::extractRedStamp(const QImage& src, int redMin, int sensMin,
                                       const QColor& outColor, const QImage& mask)
{
    QImage src32 = src.convertToFormat(QImage::Format_ARGB32);
    QImage out(src32.size(), QImage::Format_ARGB32);
    out.fill(Qt::transparent);

    int outR = outColor.red();
    int outG = outColor.green();
    int outB = outColor.blue();
    int rangeDenom = qMax(1, 255 - sensMin);
    bool useMask = !mask.isNull();

    for (int y = 0; y < src32.height(); ++y) {
        const QRgb* sl = reinterpret_cast<const QRgb*>(src32.constScanLine(y));
        QRgb* dl = reinterpret_cast<QRgb*>(out.scanLine(y));
        const uchar* ml = useMask ? mask.constScanLine(y) : nullptr;
        for (int x = 0; x < src32.width(); ++x) {
            if (useMask && ml[x] == 0) continue;
            QRgb p = sl[x];
            int r = qRed(p), g = qGreen(p), b = qBlue(p);
            if (r < redMin) continue;
            int redness = r - qMax(g, b);
            if (redness < sensMin) continue;
            int alpha = qBound(0, (redness - sensMin) * 255 / rangeDenom, 255);
            if (alpha == 0) continue;
            dl[x] = qRgba(outR, outG, outB, alpha);
        }
    }
    return out;
}

bool StampExtractor::isExtractEmpty(const QImage& img)
{
    for (int y = 0; y < img.height(); y += 4) {
        const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < img.width(); x += 4) {
            if (qAlpha(line[x]) > 5) return false;
        }
    }
    return true;
}

QImage StampExtractor::trimTransparent(const QImage& src)
{
    int minX = src.width(), minY = src.height(), maxX = -1, maxY = -1;
    for (int y = 0; y < src.height(); ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(src.constScanLine(y));
        for (int x = 0; x < src.width(); ++x) {
            if (qAlpha(line[x]) > 5) {
                if (x < minX) minX = x;
                if (y < minY) minY = y;
                if (x > maxX) maxX = x;
                if (y > maxY) maxY = y;
            }
        }
    }
    if (maxX < 0) return src;
    const int pad = 6;
    minX = qMax(0, minX - pad);
    minY = qMax(0, minY - pad);
    maxX = qMin(src.width() - 1, maxX + pad);
    maxY = qMin(src.height() - 1, maxY + pad);
    return src.copy(minX, minY, maxX - minX + 1, maxY - minY + 1);
}
