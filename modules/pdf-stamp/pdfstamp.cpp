#include "pdfstamp.h"

#include <QApplication>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPdfWriter>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPainterPath>
#include <QPen>
#include <QShortcut>
#include <QKeySequence>
#include <QCursor>
#include <QPixmap>
#include <QDrag>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QSlider>
#include <QSpinBox>

REGISTER_DYNAMICOBJECT(PdfStamp);

namespace pdfstamp {

// ========== OverlayItem ==========

OverlayItem::OverlayItem(Kind kind, QWidget* parent)
    : QWidget(parent), m_kind(kind)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setCursor(Qt::SizeAllCursor);
    setMinimumSize(20, 20);
    setToolTip(tr("拖动可移动；滚轮可缩放；右键可删除"));
}

void OverlayItem::setImage(const QImage& img)
{
    m_image = img;
    recomputeSize();
    update();
}

void OverlayItem::setText(const QString& text)
{
    m_text = text;
    recomputeSize();
    update();
}

void OverlayItem::setScale(double s)
{
    if (s < 0.1) s = 0.1;
    if (s > 10.0) s = 10.0;
    m_scale = s;
    recomputeSize();
    update();
}

void OverlayItem::setSelected(bool s)
{
    if (m_selected == s) return;
    m_selected = s;
    update();
}

void OverlayItem::recomputeSize()
{
    QPoint center = geometry().center();
    QSize newSize;
    if (m_kind == DateText) {
        QFont f = font();
        f.setPointSizeF(qMax(8.0, 14.0 * m_scale));
        QFontMetrics fm(f);
        QSize ts = fm.size(0, m_text);
        newSize = QSize(ts.width() + 8, ts.height() + 4);
    } else {
        if (m_image.isNull()) {
            newSize = QSize(80, 80);
        } else {
            int w = qRound(m_image.width() * m_scale);
            int h = qRound(m_image.height() * m_scale);
            // 限制初始大小，方便操作
            newSize = QSize(qMax(20, w), qMax(20, h));
        }
    }
    resize(newSize);
    // 保持中心点不变
    move(center.x() - newSize.width() / 2, center.y() - newSize.height() / 2);
}

void OverlayItem::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    if (m_kind == DateText) {
        QFont f = font();
        f.setPointSizeF(qMax(8.0, 14.0 * m_scale));
        p.setFont(f);
        p.setPen(QColor(20, 20, 20));
        p.drawText(rect(), Qt::AlignCenter, m_text);
    } else if (!m_image.isNull()) {
        p.drawImage(rect(), m_image);
    } else {
        // 占位：显示明显边框 + 图标提示
        p.fillRect(rect(), QColor(255, 0, 0, 40));
        p.setPen(QPen(QColor(0xff, 0x33, 0x33), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(rect().adjusted(0, 0, -1, -1));
        p.drawText(rect(), Qt::AlignCenter, tr("(图片加载失败)"));
    }

    // 选中：粗实线蓝框 + 角标
    if (m_selected) {
        p.setPen(QPen(QColor(0x19, 0x71, 0xc2), 2, Qt::SolidLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(rect().adjusted(0, 0, -1, -1));
        // 四角小方块
        const int s = 4;
        const QColor c(0x19, 0x71, 0xc2);
        p.fillRect(QRect(0, 0, s, s), c);
        p.fillRect(QRect(width()-s, 0, s, s), c);
        p.fillRect(QRect(0, height()-s, s, s), c);
        p.fillRect(QRect(width()-s, height()-s, s, s), c);
    } else if (underMouse()) {
        // 悬停时虚线
        p.setPen(QPen(QColor(0x33, 0x99, 0xff, 180), 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(rect().adjusted(0, 0, -1, -1));
    }
}

void OverlayItem::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragStart = e->pos();
        raise();
        setFocus();
        emit clicked(this);
        e->accept();
    }
}

void OverlayItem::mouseMoveEvent(QMouseEvent* e)
{
    if (m_dragging && (e->buttons() & Qt::LeftButton)) {
        QPoint delta = e->pos() - m_dragStart;
        QPoint np = pos() + delta;
        // 限制在父控件范围内（中心点必须在父控件内）
        if (parentWidget()) {
            QSize ps = parentWidget()->size();
            np.setX(qBound(-width()/2, np.x(), ps.width() - width()/2));
            np.setY(qBound(-height()/2, np.y(), ps.height() - height()/2));
        }
        move(np);
        e->accept();
    }
}

void OverlayItem::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        m_dragging = false;
        e->accept();
    }
}

void OverlayItem::wheelEvent(QWheelEvent* e)
{
    int delta = e->angleDelta().y();
    if (delta == 0) { e->ignore(); return; }
    double factor = (delta > 0) ? 1.1 : (1.0 / 1.1);
    setScale(m_scale * factor);
    e->accept();
}

void OverlayItem::contextMenuEvent(QContextMenuEvent* e)
{
    QMenu menu(this);
    QAction* aBigger = menu.addAction(tr("放大 +10%"));
    QAction* aSmaller = menu.addAction(tr("缩小 -10%"));
    menu.addSeparator();
    QAction* aRemove = menu.addAction(tr("删除"));
    QAction* picked = menu.exec(e->globalPos());
    if (!picked) return;
    if (picked == aBigger) setScale(m_scale * 1.1);
    else if (picked == aSmaller) setScale(m_scale / 1.1);
    else if (picked == aRemove) deleteLater();
}

// ========== PdfPageView ==========

PdfPageView::PdfPageView(int pageIndex, const QImage& rendered, QWidget* parent)
    : QWidget(parent), m_pageIndex(pageIndex), m_rendered(rendered)
{
    setFixedSize(rendered.size());
    setStyleSheet("background-color: white;");
    setAcceptDrops(true);
}

void PdfPageView::dragEnterEvent(QDragEnterEvent* e)
{
    if (e->mimeData()->hasFormat(kAssetMime))
        e->acceptProposedAction();
}

void PdfPageView::dragMoveEvent(QDragMoveEvent* e)
{
    if (e->mimeData()->hasFormat(kAssetMime))
        e->acceptProposedAction();
}

void PdfPageView::dropEvent(QDropEvent* e)
{
    if (!e->mimeData()->hasFormat(kAssetMime)) return;
    QString s = QString::fromUtf8(e->mimeData()->data(kAssetMime));
    int pipe = s.indexOf('|');
    if (pipe <= 0) return;
    QString kindStr = s.left(pipe);
    QString path = s.mid(pipe + 1);
    emit assetDropped(this, kindStr, path, e->position().toPoint());
    e->acceptProposedAction();
}

OverlayItem* PdfPageView::addOverlay(OverlayItem::Kind kind, const QImage& img, const QPoint& centerPos)
{
    auto* it = new OverlayItem(kind, this);
    // 先放到目标位置（避免初始大尺寸跳到屏外）
    it->move(centerPos.x(), centerPos.y());
    it->resize(20, 20);
    it->setImage(img);
    // 默认缩放：让图章/签名长边约为页宽的 1/6
    if (!img.isNull()) {
        double maxLong = qMax(img.width(), img.height());
        double targetLong = qMax(60.0, width() / 6.0);
        if (maxLong > 0) it->setScale(targetLong / maxLong);
    }
    // 重新居中到目标点
    it->move(centerPos.x() - it->width() / 2, centerPos.y() - it->height() / 2);
    it->show();
    it->raise();
    it->update();
    connect(it, &OverlayItem::clicked, this,
            [this](OverlayItem* o){ emit overlayClicked(this, o); });
    return it;
}

OverlayItem* PdfPageView::addDateOverlay(const QString& text, const QPoint& centerPos)
{
    auto* it = new OverlayItem(OverlayItem::DateText, this);
    it->move(centerPos.x(), centerPos.y());
    it->setText(text);
    it->move(centerPos.x() - it->width() / 2, centerPos.y() - it->height() / 2);
    it->show();
    it->raise();
    it->update();
    connect(it, &OverlayItem::clicked, this,
            [this](OverlayItem* o){ emit overlayClicked(this, o); });
    return it;
}

void PdfPageView::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        emit emptyClicked(this, e->pos());
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

QList<OverlayItem*> PdfPageView::overlays() const
{
    QList<OverlayItem*> list;
    for (auto* c : children()) {
        if (auto* it = qobject_cast<OverlayItem*>(c))
            list << it;
    }
    return list;
}

void PdfPageView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawImage(rect(), m_rendered);
    // 边框
    p.setPen(QColor(0xdd, 0xdd, 0xdd));
    p.setBrush(Qt::NoBrush);
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}

} // namespace pdfstamp


// ========== 签名绘制对话框 ==========

namespace {

class SignaturePadDialog : public QDialog
{
public:
    explicit SignaturePadDialog(QWidget* parent) : QDialog(parent)
    {
        setWindowTitle(QObject::tr("绘制签名"));
        resize(640, 400);
        m_canvas = QImage(QSize(620, 260), QImage::Format_ARGB32);
        m_canvas.fill(Qt::white);

        auto* layout = new QVBoxLayout(this);
        m_canvasLabel = new QLabel(this);
        m_canvasLabel->setFixedSize(m_canvas.size());
        m_canvasLabel->setStyleSheet("background:#fff; border:1px solid #ccc;");
        m_canvasLabel->installEventFilter(this);
        m_canvasLabel->setPixmap(QPixmap::fromImage(m_canvas));

        layout->addWidget(m_canvasLabel);

        // 粗细行
        auto* widthRow = new QHBoxLayout();
        widthRow->addWidget(new QLabel(QObject::tr("线条粗细:"), this));
        m_widthSlider = new QSlider(Qt::Horizontal, this);
        m_widthSlider->setRange(1, 20);
        m_widthSlider->setValue(int(m_penWidth));
        widthRow->addWidget(m_widthSlider, 1);
        m_widthSpin = new QSpinBox(this);
        m_widthSpin->setRange(1, 20);
        m_widthSpin->setSuffix(QObject::tr(" px"));
        m_widthSpin->setValue(int(m_penWidth));
        widthRow->addWidget(m_widthSpin);
        m_widthPreview = new QLabel(this);
        m_widthPreview->setFixedSize(120, 24);
        m_widthPreview->setStyleSheet("background:#fafafa; border:1px solid #e0e0e0;");
        widthRow->addWidget(m_widthPreview);
        layout->addLayout(widthRow);

        auto* btnRow = new QHBoxLayout();
        auto* clearBtn = new QPushButton(QObject::tr("清空"), this);
        btnRow->addWidget(clearBtn);
        btnRow->addStretch();
        auto* bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        btnRow->addWidget(bbox);
        layout->addLayout(btnRow);

        connect(clearBtn, &QPushButton::clicked, this, [this]{
            m_canvas.fill(Qt::white);
            m_canvasLabel->setPixmap(QPixmap::fromImage(m_canvas));
        });
        connect(bbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);

        connect(m_widthSlider, &QSlider::valueChanged, this, [this](int v){
            m_penWidth = v;
            QSignalBlocker b(m_widthSpin);
            m_widthSpin->setValue(v);
            updateWidthPreview();
        });
        connect(m_widthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v){
            m_penWidth = v;
            QSignalBlocker b(m_widthSlider);
            m_widthSlider->setValue(v);
            updateWidthPreview();
        });

        updateWidthPreview();
    }

    QImage signature() const
    {
        // 把白色背景转为透明
        QImage out = m_canvas.convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < out.height(); ++y) {
            QRgb* line = reinterpret_cast<QRgb*>(out.scanLine(y));
            for (int x = 0; x < out.width(); ++x) {
                QRgb c = line[x];
                int r = qRed(c), g = qGreen(c), b = qBlue(c);
                if (r > 240 && g > 240 && b > 240) {
                    line[x] = qRgba(0, 0, 0, 0);
                }
            }
        }
        return out;
    }

protected:
    bool eventFilter(QObject* o, QEvent* e) override
    {
        if (o != m_canvasLabel) return QDialog::eventFilter(o, e);
        if (e->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(e);
            m_lastPos = me->pos();
            m_drawing = true;
            return true;
        } else if (e->type() == QEvent::MouseMove && m_drawing) {
            auto* me = static_cast<QMouseEvent*>(e);
            QPainter p(&m_canvas);
            p.setRenderHint(QPainter::Antialiasing, true);
            QPen pen(Qt::black, m_penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            p.setPen(pen);
            p.drawLine(m_lastPos, me->pos());
            m_lastPos = me->pos();
            m_canvasLabel->setPixmap(QPixmap::fromImage(m_canvas));
            return true;
        } else if (e->type() == QEvent::MouseButtonRelease) {
            m_drawing = false;
            return true;
        }
        return QDialog::eventFilter(o, e);
    }

private:
    void updateWidthPreview()
    {
        QPixmap pm(m_widthPreview->size());
        pm.fill(Qt::white);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        QPen pen(Qt::black, m_penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.drawLine(8, pm.height() / 2, pm.width() - 8, pm.height() / 2);
        m_widthPreview->setPixmap(pm);
    }

    QImage m_canvas;
    QLabel* m_canvasLabel = nullptr;
    QPoint m_lastPos;
    bool m_drawing = false;
    qreal m_penWidth = 3.0;
    QSlider* m_widthSlider = nullptr;
    QSpinBox* m_widthSpin = nullptr;
    QLabel* m_widthPreview = nullptr;
};

// 支持自定义 mime 拖拽的资源列表
class AssetListWidget : public QListWidget
{
public:
    using QListWidget::QListWidget;

protected:
    QStringList mimeTypes() const override
    {
        return QStringList() << pdfstamp::PdfPageView::kAssetMime;
    }

    QMimeData* mimeData(const QList<QListWidgetItem*>& items) const override
    {
        if (items.isEmpty()) return nullptr;
        auto* it = items.first();
        QString path = it->data(Qt::UserRole).toString();
        QString kind = it->data(Qt::UserRole + 1).toString();
        auto* m = new QMimeData;
        m->setData(pdfstamp::PdfPageView::kAssetMime,
                   (kind + "|" + path).toUtf8());
        return m;
    }

    void startDrag(Qt::DropActions /*actions*/) override
    {
        auto* it = currentItem();
        if (!it) return;
        QString path = it->data(Qt::UserRole).toString();
        QImage img(path);
        if (img.isNull()) return;

        QList<QListWidgetItem*> items{it};
        QMimeData* mime = mimeData(items);
        if (!mime) return;

        auto* drag = new QDrag(this);
        drag->setMimeData(mime);

        // 拖拽时的预览图（缩略到 100px 长边）
        int target = 100;
        QImage scaled = img.scaled(target, target,
                                   Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QPixmap pm = QPixmap::fromImage(scaled);
        drag->setPixmap(pm);
        drag->setHotSpot(QPoint(pm.width() / 2, pm.height() / 2));

        drag->exec(Qt::CopyAction);
    }
};

} // anon namespace


// ========== PdfStamp ==========

PdfStamp::PdfStamp() : QWidget(nullptr), DynamicObjectBase()
{
    setupUi();

#ifdef WITH_QT_PDF
    m_pdfDoc = new QPdfDocument(this);
#endif

    reloadAssets();
}

PdfStamp::~PdfStamp() = default;

void PdfStamp::setupUi()
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
        QPushButton:disabled { color: #adb5bd; }
        QListWidget {
            border: 1px solid #dee2e6;
            border-radius: 4px;
            background: #fff;
            outline: none;
        }
        QLineEdit, QComboBox {
            border: 1px solid #dee2e6;
            border-radius: 4px;
            padding: 3px 6px;
            background: #fff;
        }
    )");

    // 工具栏
    auto* toolbar = new QHBoxLayout();
    toolbar->setSpacing(2);

    m_openBtn = new QPushButton(tr("打开 PDF"));
    m_addStampBtn = new QPushButton(tr("导入图章"));
    m_addSignBtn = new QPushButton(tr("导入签名"));
    m_drawSignBtn = new QPushButton(tr("手写签名"));
    m_addDateBtn = new QPushButton(tr("加日期"));
    m_delOverlayBtn = new QPushButton(tr("删除选中"));
    m_delOverlayBtn->setEnabled(false);
    m_saveAsBtn = new QPushButton(tr("另存为 PDF"));
    m_saveAsBtn->setStyleSheet(
        "QPushButton { color: #228be6; font-weight: bold; }"
        "QPushButton:hover { background-color: #e7f5ff; }"
        "QPushButton:disabled { color: #adb5bd; font-weight: normal; }");

    toolbar->addWidget(m_openBtn);
    toolbar->addSpacing(8);
    toolbar->addWidget(m_addStampBtn);
    toolbar->addWidget(m_addSignBtn);
    toolbar->addWidget(m_drawSignBtn);
    toolbar->addSpacing(8);
    toolbar->addWidget(m_addDateBtn);
    toolbar->addSpacing(8);
    toolbar->addWidget(m_delOverlayBtn);
    toolbar->addStretch();
    toolbar->addWidget(m_saveAsBtn);

    main->addLayout(toolbar);

    // 日期格式行
    auto* dateRow = new QHBoxLayout();
    dateRow->addWidget(new QLabel(tr("日期格式:")));
    m_dateFormatCombo = new QComboBox();
    m_dateFormatCombo->addItem(tr("yyyy 年 M 月 d 日"));
    m_dateFormatCombo->addItem(tr("yyyy-MM-dd"));
    m_dateFormatCombo->addItem(tr("yyyy/MM/dd"));
    m_dateFormatCombo->addItem(tr("yyyy.MM.dd"));
    m_dateFormatCombo->addItem(tr("自定义文字"));
    dateRow->addWidget(m_dateFormatCombo);
    m_customDateEdit = new QLineEdit();
    m_customDateEdit->setPlaceholderText(tr("自定义文字（选\"自定义文字\"时使用）"));
    dateRow->addWidget(m_customDateEdit, 1);
    main->addLayout(dateRow);

    // 主体：左侧资源列表 + 右侧页面预览
    auto* body = new QHBoxLayout();
    body->setSpacing(8);

    // 左侧
    auto* leftCol = new QVBoxLayout();
    leftCol->setSpacing(4);
    auto* lbl = new QLabel(tr("我的图章 / 签名"));
    lbl->setStyleSheet("color:#495057; font-weight:bold;");
    leftCol->addWidget(lbl);
    m_assetList = new AssetListWidget();
    m_assetList->setIconSize(QSize(64, 64));
    m_assetList->setMinimumWidth(180);
    m_assetList->setMaximumWidth(220);
    m_assetList->setDragEnabled(true);
    m_assetList->setDragDropMode(QAbstractItemView::DragOnly);
    leftCol->addWidget(m_assetList, 1);
    auto* assetBtnRow = new QHBoxLayout();
    m_useAssetBtn = new QPushButton(tr("使用此项"));
    m_delAssetBtn = new QPushButton(tr("从我的资源中删除"));
    assetBtnRow->addWidget(m_useAssetBtn);
    assetBtnRow->addWidget(m_delAssetBtn);
    leftCol->addLayout(assetBtnRow);

    auto* leftWidget = new QWidget();
    leftWidget->setLayout(leftCol);
    body->addWidget(leftWidget, 0);

    // 右侧
    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setStyleSheet("QScrollArea { border:1px solid #dee2e6; background:#f1f3f5; }");
    m_pagesContainer = new QWidget();
    m_pagesContainer->setStyleSheet("background:#f1f3f5;");
    m_pagesLayout = new QVBoxLayout(m_pagesContainer);
    m_pagesLayout->setContentsMargins(20, 20, 20, 20);
    m_pagesLayout->setSpacing(16);
    m_pagesLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    m_scrollArea->setWidget(m_pagesContainer);
    body->addWidget(m_scrollArea, 1);

    main->addLayout(body, 1);

    // 状态栏
    m_statusLabel = new QLabel();
    m_statusLabel->setStyleSheet("color:#868e96; font-size:8pt;");
    main->addWidget(m_statusLabel);

#ifndef WITH_QT_PDF
    m_statusLabel->setText(tr("当前构建未启用 Qt Pdf 支持，无法打开 PDF。"));
    m_openBtn->setEnabled(false);
    m_saveAsBtn->setEnabled(false);
    m_addDateBtn->setEnabled(false);
    m_useAssetBtn->setEnabled(false);
#else
    m_statusLabel->setText(tr("点击\"打开 PDF\"开始。可拖拽覆盖物，滚轮缩放，右键删除。"));
#endif

    connect(m_openBtn, &QPushButton::clicked, this, &PdfStamp::onOpenPdf);
    connect(m_saveAsBtn, &QPushButton::clicked, this, &PdfStamp::onSaveAs);
    connect(m_addStampBtn, &QPushButton::clicked, this, &PdfStamp::onAddStampFromFile);
    connect(m_addSignBtn, &QPushButton::clicked, this, &PdfStamp::onAddSignatureFromFile);
    connect(m_drawSignBtn, &QPushButton::clicked, this, &PdfStamp::onDrawSignature);
    connect(m_addDateBtn, &QPushButton::clicked, this, &PdfStamp::onAddDate);
    connect(m_useAssetBtn, &QPushButton::clicked, this, &PdfStamp::onUseSelectedAsset);
    connect(m_delAssetBtn, &QPushButton::clicked, this, &PdfStamp::onRemoveSelectedAsset);
    connect(m_delOverlayBtn, &QPushButton::clicked, this, &PdfStamp::onRemoveSelectedOverlay);
    connect(m_assetList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*){ onUseSelectedAsset(); });

    // 快捷键
    auto* esc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    esc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(esc, &QShortcut::activated, this, &PdfStamp::cancelPlaceMode);

    auto* del = new QShortcut(QKeySequence(Qt::Key_Delete), this);
    del->setContext(Qt::WidgetWithChildrenShortcut);
    connect(del, &QShortcut::activated, this, &PdfStamp::onRemoveSelectedOverlay);

    auto* bs = new QShortcut(QKeySequence(Qt::Key_Backspace), this);
    bs->setContext(Qt::WidgetWithChildrenShortcut);
    connect(bs, &QShortcut::activated, this, &PdfStamp::onRemoveSelectedOverlay);
}

QString PdfStamp::assetsDir() const
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) base = QDir::homePath() + "/.lele-tools";
    QString d = base + "/pdf-stamp";
    QDir().mkpath(d);
    return d;
}

void PdfStamp::reloadAssets()
{
    m_assetList->clear();
    QDir dir(assetsDir());
    QStringList files = dir.entryList(QStringList() << "*.png" << "*.jpg" << "*.jpeg",
                                      QDir::Files, QDir::Time);
    for (const QString& f : files) {
        QString full = dir.filePath(f);
        QImage img(full);
        if (img.isNull()) continue;

        // 文件名格式: stamp_xxx.png  /  signature_xxx.png
        QString kind = f.startsWith("signature_") ? tr("签名") : tr("图章");

        auto* it = new QListWidgetItem();
        it->setIcon(QPixmap::fromImage(img.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        it->setText(QString("[%1] %2").arg(kind, f));
        it->setData(Qt::UserRole, full);  // 路径
        it->setData(Qt::UserRole + 1, f.startsWith("signature_") ? "signature" : "stamp");
        m_assetList->addItem(it);
    }
}

void PdfStamp::saveAssetCopy(const QString& srcPath, const QString& kind)
{
    QImage img(srcPath);
    if (img.isNull()) {
        QMessageBox::warning(this, tr("错误"), tr("无法读取图片：%1").arg(srcPath));
        return;
    }
    saveAssetImage(img, kind);
}

void PdfStamp::saveAssetImage(const QImage& img, const QString& kind)
{
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    QString name = QString("%1_%2.png").arg(kind, id);
    QString full = assetsDir() + "/" + name;
    if (!img.save(full, "PNG")) {
        QMessageBox::warning(this, tr("错误"), tr("保存失败：%1").arg(full));
        return;
    }
    reloadAssets();
}

void PdfStamp::onAddStampFromFile()
{
    QString f = QFileDialog::getOpenFileName(
        this, tr("选择图章图片（建议 PNG 透明背景）"), QString(),
        tr("图片 (*.png *.jpg *.jpeg)"));
    if (f.isEmpty()) return;
    saveAssetCopy(f, "stamp");
}

void PdfStamp::onAddSignatureFromFile()
{
    QString f = QFileDialog::getOpenFileName(
        this, tr("选择签名图片（建议 PNG 透明背景）"), QString(),
        tr("图片 (*.png *.jpg *.jpeg)"));
    if (f.isEmpty()) return;
    saveAssetCopy(f, "signature");
}

void PdfStamp::onDrawSignature()
{
    SignaturePadDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    QImage sig = dlg.signature();
    if (sig.isNull()) return;
    saveAssetImage(sig, "signature");
}

void PdfStamp::onRemoveSelectedAsset()
{
    auto* it = m_assetList->currentItem();
    if (!it) {
        QMessageBox::information(this, tr("提示"), tr("请先选择左侧列表中的一个项。"));
        return;
    }
    QString path = it->data(Qt::UserRole).toString();
    if (QMessageBox::question(this, tr("确认删除"),
                              tr("确定从我的资源中删除该项吗？\n%1").arg(path))
        != QMessageBox::Yes) return;
    QFile::remove(path);
    reloadAssets();
}

pdfstamp::PdfPageView* PdfStamp::currentPageView() const
{
    // 视野中心点（容器坐标系）所在的那一页视图
    if (m_pageViews.isEmpty()) return nullptr;

    // 容器相对于 viewport 的位置：滚动后 y() 为负值
    int vpH = m_scrollArea->viewport()->height();
    int containerY = m_pagesContainer->y(); // 滚动时为负
    int centerInContainerY = -containerY + vpH / 2;

    pdfstamp::PdfPageView* best = nullptr;
    int bestDist = INT_MAX;
    for (const auto& p : m_pageViews) {
        if (!p) continue;
        QRect g = p->geometry();
        if (g.top() <= centerInContainerY && centerInContainerY <= g.bottom())
            return p.data();
        int dy = qAbs(g.center().y() - centerInContainerY);
        if (dy < bestDist) { bestDist = dy; best = p.data(); }
    }
    return best;
}

void PdfStamp::onUseSelectedAsset()
{
    auto* it = m_assetList->currentItem();
    if (!it) {
        QMessageBox::information(this, tr("提示"), tr("请先在左侧选择一个图章或签名。"));
        return;
    }
    if (m_pageViews.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请先打开 PDF。"));
        return;
    }
    QString path = it->data(Qt::UserRole).toString();
    QString kindStr = it->data(Qt::UserRole + 1).toString();
    QImage img(path);
    if (img.isNull()) {
        QMessageBox::warning(this, tr("图片加载失败"),
                             tr("无法读取图片文件：\n%1").arg(path));
        return;
    }

    auto kind = (kindStr == "signature") ? pdfstamp::OverlayItem::Signature
                                         : pdfstamp::OverlayItem::Stamp;
    enterPlaceImageMode(kind, img);
}

void PdfStamp::onAddDate()
{
    if (m_pageViews.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请先打开 PDF。"));
        return;
    }
    QString fmt = m_dateFormatCombo->currentText();
    QString text;
    if (fmt == tr("自定义文字")) {
        text = m_customDateEdit->text().trimmed();
        if (text.isEmpty()) text = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    } else {
        QString actual = fmt;
        if (fmt.contains(tr("yyyy 年 M 月 d 日"))) actual = "yyyy 年 M 月 d 日";
        text = QDateTime::currentDateTime().toString(actual);
    }
    enterPlaceDateMode(text);
}

void PdfStamp::enterPlaceImageMode(pdfstamp::OverlayItem::Kind kind, const QImage& img)
{
    m_placeMode = PlaceImage;
    m_pendingKind = kind;
    m_pendingImage = img;
    m_pendingText.clear();

    // 用图片缩略图作为光标，达到"实时预览"效果
    int cursorLong = 64;
    QImage scaled = img.scaled(cursorLong, cursorLong, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPixmap pm = QPixmap::fromImage(scaled);
    QCursor cur(pm, pm.width() / 2, pm.height() / 2);
    for (auto& p : m_pageViews) if (p) p->setCursor(cur);

    m_statusLabel->setText(tr("移动鼠标到 PDF 任意位置，点击放下 %1（按 Esc 取消）")
                               .arg(kind == pdfstamp::OverlayItem::Signature ? tr("签名") : tr("图章")));
}

void PdfStamp::enterPlaceDateMode(const QString& text)
{
    m_placeMode = PlaceDate;
    m_pendingText = text;
    m_pendingImage = QImage();

    // 文字预览光标
    QFont f;
    f.setPointSize(14);
    QFontMetrics fm(f);
    QSize ts = fm.size(0, text);
    QPixmap pm(ts + QSize(8, 4));
    pm.fill(Qt::transparent);
    QPainter pp(&pm);
    pp.setRenderHint(QPainter::Antialiasing, true);
    pp.setFont(f);
    pp.setPen(QColor(0x19, 0x71, 0xc2));
    pp.drawText(pm.rect(), Qt::AlignCenter, text);
    pp.end();
    QCursor cur(pm, pm.width() / 2, pm.height() / 2);
    for (auto& p : m_pageViews) if (p) p->setCursor(cur);

    m_statusLabel->setText(tr("移动鼠标到 PDF 任意位置，点击放下日期：%1（按 Esc 取消）").arg(text));
}

void PdfStamp::cancelPlaceMode()
{
    if (m_placeMode == PlaceNone) return;
    m_placeMode = PlaceNone;
    m_pendingImage = QImage();
    m_pendingText.clear();
    for (auto& p : m_pageViews) if (p) p->unsetCursor();
    m_statusLabel->setText(tr("已取消放置。"));
}

void PdfStamp::onPageEmptyClicked(pdfstamp::PdfPageView* page, const QPoint& posOnPage)
{
    if (m_placeMode == PlaceImage && !m_pendingImage.isNull()) {
        auto* item = page->addOverlay(m_pendingKind, m_pendingImage, posOnPage);
        cancelPlaceMode();
        selectOverlay(item);
        m_statusLabel->setText(tr("已在第 %1 页放下 %2。可拖动 / 滚轮缩放 / Delete 删除")
                                   .arg(page->pageIndex() + 1)
                                   .arg(m_pendingKind == pdfstamp::OverlayItem::Signature ? tr("签名") : tr("图章")));
    } else if (m_placeMode == PlaceDate && !m_pendingText.isEmpty()) {
        auto* item = page->addDateOverlay(m_pendingText, posOnPage);
        QString placedText = m_pendingText;
        cancelPlaceMode();
        selectOverlay(item);
        m_statusLabel->setText(tr("已在第 %1 页放下日期：%2").arg(page->pageIndex() + 1).arg(placedText));
    } else {
        // 取消选中
        selectOverlay(nullptr);
    }
}

void PdfStamp::onOverlayClicked(pdfstamp::PdfPageView* page, pdfstamp::OverlayItem* item)
{
    Q_UNUSED(page);
    if (m_placeMode != PlaceNone) {
        // 放置模式下，点到已有覆盖物上也视作放置
        QPoint posOnPage = item->geometry().center();
        // 找到所属页面
        if (auto* p = qobject_cast<pdfstamp::PdfPageView*>(item->parentWidget())) {
            onPageEmptyClicked(p, posOnPage);
        }
        return;
    }
    selectOverlay(item);
}

void PdfStamp::selectOverlay(pdfstamp::OverlayItem* it)
{
    if (m_selectedOverlay && m_selectedOverlay != it) {
        m_selectedOverlay->setSelected(false);
    }
    m_selectedOverlay = it;
    if (it) it->setSelected(true);
    m_delOverlayBtn->setEnabled(it != nullptr);
}

void PdfStamp::onRemoveSelectedOverlay()
{
    if (!m_selectedOverlay) return;
    m_selectedOverlay->deleteLater();
    m_selectedOverlay = nullptr;
    m_delOverlayBtn->setEnabled(false);
    m_statusLabel->setText(tr("已删除选中的覆盖物。"));
}

void PdfStamp::onAssetDropped(pdfstamp::PdfPageView* page, const QString& kindStr,
                              const QString& path, const QPoint& posOnPage)
{
    QImage img(path);
    if (img.isNull()) {
        QMessageBox::warning(this, tr("图片加载失败"),
                             tr("无法读取图片文件：\n%1").arg(path));
        return;
    }
    auto kind = (kindStr == "signature") ? pdfstamp::OverlayItem::Signature
                                         : pdfstamp::OverlayItem::Stamp;
    auto* item = page->addOverlay(kind, img, posOnPage);
    selectOverlay(item);
    m_statusLabel->setText(tr("已在第 %1 页放下 %2（拖拽放置）。可拖动 / 滚轮缩放 / Delete 删除")
                               .arg(page->pageIndex() + 1)
                               .arg(kind == pdfstamp::OverlayItem::Signature ? tr("签名") : tr("图章")));
}

void PdfStamp::onOpenPdf()
{
#ifndef WITH_QT_PDF
    QMessageBox::warning(this, tr("不支持"), tr("当前构建未启用 Qt Pdf 模块。"));
#else
    QString f = QFileDialog::getOpenFileName(this, tr("打开 PDF"), QString(),
                                             tr("PDF 文件 (*.pdf)"));
    if (f.isEmpty()) return;

    auto status = m_pdfDoc->load(f);
    if (status != QPdfDocument::Error::None) {
        QMessageBox::warning(this, tr("打开失败"),
                             tr("无法打开 PDF：%1").arg(f));
        return;
    }
    m_currentPdfPath = f;
    renderAllPages();
#endif
}

void PdfStamp::renderAllPages()
{
#ifdef WITH_QT_PDF
    // 清空旧页面
    QLayoutItem* child;
    while ((child = m_pagesLayout->takeAt(0)) != nullptr) {
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }
    m_pageViews.clear();

    int n = m_pdfDoc->pageCount();
    if (n <= 0) {
        m_statusLabel->setText(tr("PDF 无可渲染页面。"));
        return;
    }

    // 每页渲染目标宽度（按 DPR 适配）
    qreal dpr = qApp->devicePixelRatio();
    int targetWidth = qMin(900, qMax(600, m_scrollArea->viewport()->width() - 80));

    for (int i = 0; i < n; ++i) {
        QSizeF pts = m_pdfDoc->pagePointSize(i);  // points (1/72 inch)
        if (pts.width() <= 0) continue;
        double scale = targetWidth / pts.width();
        QSize px(int(pts.width() * scale * dpr), int(pts.height() * scale * dpr));
        QImage img = m_pdfDoc->render(i, px);
        if (img.isNull()) continue;
        img.setDevicePixelRatio(dpr);

        // 显示尺寸（逻辑像素）
        QImage display = img;
        // PdfPageView 需要按显示像素显示
        QImage logical(QSize(int(pts.width() * scale), int(pts.height() * scale)),
                       QImage::Format_ARGB32);
        logical.fill(Qt::white);
        QPainter pp(&logical);
        pp.setRenderHint(QPainter::SmoothPixmapTransform, true);
        pp.drawImage(QRect(QPoint(0,0), logical.size()), img);
        pp.end();

        auto* view = new pdfstamp::PdfPageView(i, logical);
        connect(view, &pdfstamp::PdfPageView::emptyClicked, this, &PdfStamp::onPageEmptyClicked);
        connect(view, &pdfstamp::PdfPageView::overlayClicked, this, &PdfStamp::onOverlayClicked);
        connect(view, &pdfstamp::PdfPageView::assetDropped, this, &PdfStamp::onAssetDropped);
        m_pagesLayout->addWidget(view, 0, Qt::AlignHCenter);
        m_pageViews.append(QPointer<pdfstamp::PdfPageView>(view));
    }

    m_statusLabel->setText(tr("已加载：%1（共 %2 页）").arg(QFileInfo(m_currentPdfPath).fileName()).arg(n));
#endif
}

void PdfStamp::onSaveAs()
{
#ifndef WITH_QT_PDF
    QMessageBox::warning(this, tr("不支持"), tr("当前构建未启用 Qt Pdf 模块。"));
#else
    if (m_pageViews.isEmpty()) {
        QMessageBox::warning(this, tr("提示"), tr("请先打开 PDF。"));
        return;
    }

    QString defaultPath = m_currentPdfPath.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/stamped.pdf"
        : QFileInfo(m_currentPdfPath).absolutePath() + "/" +
              QFileInfo(m_currentPdfPath).completeBaseName() + "_stamped.pdf";

    QString savePath = QFileDialog::getSaveFileName(
        this, tr("另存为 PDF"), defaultPath, tr("PDF 文件 (*.pdf)"));
    if (savePath.isEmpty()) return;

    QPdfWriter writer(savePath);
    writer.setResolution(300);
    writer.setPageMargins(QMarginsF(0, 0, 0, 0));

    // 用第一页的页面尺寸近似（QPdfDocument 给出 points = 1/72 inch）
    QSizeF firstPts = m_pdfDoc->pagePointSize(0);
    QPageSize ps(firstPts, QPageSize::Point, "Page", QPageSize::ExactMatch);
    writer.setPageSize(ps);

    QPainter painter(&writer);

    for (int i = 0; i < m_pageViews.size(); ++i) {
        if (i > 0) {
            // 为该页设置正确尺寸
            QSizeF pts = m_pdfDoc->pagePointSize(m_pageViews[i]->pageIndex());
            writer.setPageSize(QPageSize(pts, QPageSize::Point, "Page", QPageSize::ExactMatch));
            writer.newPage();
        }

        auto* view = m_pageViews[i].data();
        if (!view) continue;

        // 1) 绘制原页面位图（高分辨率重新渲染，避免视图缩放损耗）
        int pageIdx = view->pageIndex();
        QSizeF pts = m_pdfDoc->pagePointSize(pageIdx);
        // 输出尺寸（device pixels）
        QRect target = painter.viewport();
        QSize outPx(target.width(), target.height());

        QImage hi = m_pdfDoc->render(pageIdx, outPx);
        if (!hi.isNull())
            painter.drawImage(target, hi);

        // 2) 把覆盖物按视图坐标映射到输出 painter 坐标
        QSize viewSize = view->size();
        double sx = double(target.width()) / viewSize.width();
        double sy = double(target.height()) / viewSize.height();

        for (auto* it : view->overlays()) {
            QRect g = it->geometry();
            QRectF outRect(g.x() * sx, g.y() * sy, g.width() * sx, g.height() * sy);

            if (it->kind() == pdfstamp::OverlayItem::DateText) {
                QFont f = painter.font();
                // overlay 字体大小（逻辑）：以高度近似估计
                double pixSize = g.height() * 0.7;
                f.setPixelSize(qMax(8, int(pixSize * sy)));
                painter.save();
                painter.setFont(f);
                painter.setPen(QColor(20, 20, 20));
                painter.drawText(outRect, Qt::AlignCenter, it->text());
                painter.restore();
            } else {
                QImage img = it->image();
                if (img.isNull()) continue;
                painter.drawImage(outRect, img);
            }
        }
    }

    painter.end();

    QMessageBox box(this);
    box.setWindowTitle(tr("保存完成"));
    box.setText(tr("PDF 已保存:\n%1").arg(savePath));
    box.setIcon(QMessageBox::Information);
    auto* openBtn = box.addButton(tr("打开 PDF"), QMessageBox::ActionRole);
    auto* dirBtn = box.addButton(tr("打开所在目录"), QMessageBox::ActionRole);
    box.addButton(QMessageBox::Ok);
    box.exec();

    if (box.clickedButton() == openBtn)
        QDesktopServices::openUrl(QUrl::fromLocalFile(savePath));
    else if (box.clickedButton() == dirBtn)
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(savePath).absolutePath()));
#endif
}
