#include "pdfwatermark.h"

#include <QApplication>
#include <QBuffer>
#include <QColorDialog>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMargins>
#include <QMessageBox>
#include <QMimeData>
#include <QPageSize>
#include <QPainter>
#include <QPdfDocument>
#include <QPdfWriter>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSpinBox>
#include <QScrollArea>
#include <QSplitter>
#include <QStandardPaths>
#include <QTimer>
#include <QTransform>
#include <QUrl>
#include <QVBoxLayout>

#include <cmath>

REGISTER_DYNAMICOBJECT(PdfWatermark);

PdfWatermark::PdfWatermark() : QWidget(nullptr), DynamicObjectBase()
{
    setAcceptDrops(true);
    m_pdfDoc = new QPdfDocument(this);
    setupUI();
    onTypeChanged(0);
}

PdfWatermark::~PdfWatermark() = default;

// ─────────────────────────────────────────────────────────────
// UI
// ─────────────────────────────────────────────────────────────

void PdfWatermark::setupUI()
{
    setStyleSheet(R"(
        QLineEdit, QSpinBox, QComboBox {
            border: 1px solid #ced4da; border-radius: 4px;
            padding: 4px 8px; background: #fff; min-height: 22px;
        }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus { border-color: #228be6; }
        QPushButton {
            border: 1px solid #ced4da; border-radius: 4px;
            padding: 5px 12px; background: #fff; min-height: 22px; color: #343a40;
        }
        QPushButton:hover  { background: #f1f3f5; border-color: #adb5bd; }
        QPushButton:pressed{ background: #e9ecef; }
        QPushButton#primaryBtn {
            background: #228be6; border-color: #1c7ed6; color: white; font-weight: bold;
        }
        QPushButton#primaryBtn:hover { background: #1c7ed6; }
        QFrame#card { background: #f8f9fa; border: 1px solid #dee2e6; border-radius: 6px; }
        QLabel { background: transparent; color: #495057; }
        QListWidget { border: 1px solid #dee2e6; border-radius: 4px; background: #fff; }
    )");

    auto* main = new QVBoxLayout(this);
    main->setContentsMargins(10, 8, 10, 8);
    main->setSpacing(8);

    // 顶部：打开 + PDF 信息
    auto* topRow = new QHBoxLayout();
    m_openBtn = new QPushButton(tr("📂 打开 PDF"));
    topRow->addWidget(m_openBtn);
    m_pdfInfoLabel = new QLabel(tr("（未打开 — 或把 PDF 拖到窗口）"));
    m_pdfInfoLabel->setStyleSheet("color:#868e96;");
    topRow->addWidget(m_pdfInfoLabel, 1);
    main->addLayout(topRow);

    auto* split = new QSplitter(Qt::Horizontal);

    // 左列：水印设置面板（放在最前面，需求改了）
    auto* right = new QWidget();
    auto* rv = new QVBoxLayout(right);
    rv->setContentsMargins(0, 0, 0, 0);
    rv->setSpacing(8);

    // 中列：当前页预览（含水印效果，参数变化实时刷新）
    auto* center = new QWidget();
    auto* cv = new QVBoxLayout(center);
    cv->setContentsMargins(0, 0, 0, 0);
    cv->setSpacing(4);
    auto* previewScroll = new QScrollArea();
    previewScroll->setWidgetResizable(true);
    previewScroll->setAlignment(Qt::AlignCenter);
    previewScroll->setStyleSheet("background:#e9ecef; border:1px solid #dee2e6; border-radius:4px;");
    m_previewLabel = new QLabel(tr("（打开 PDF 后这里显示预览）"));
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet("background:transparent; color:#868e96;");
    previewScroll->setWidget(m_previewLabel);
    cv->addWidget(previewScroll, 1);

    // 右列：页面缩略图
    m_pageList = new QListWidget();
    m_pageList->setMinimumWidth(150);
    m_pageList->setMaximumWidth(180);
    m_pageList->setIconSize(QSize(120, 160));
    m_pageList->setViewMode(QListView::ListMode);
    m_pageList->setSpacing(2);

    // 按照 [设置 | 预览 | 缩略图] 顺序加入 splitter

    // 应用范围
    {
        auto* lbl = new QLabel(tr("📌 应用到哪些页面"));
        lbl->setStyleSheet("font-weight:bold;");
        rv->addWidget(lbl);

        auto* rangeRow = new QHBoxLayout();
        m_rangeEdit = new QLineEdit();
        m_rangeEdit->setPlaceholderText(tr("如 1-3,5,7-9（留空 = 全部页）"));
        rangeRow->addWidget(m_rangeEdit, 1);
        rv->addLayout(rangeRow);

        auto* presetRow = new QHBoxLayout();
        m_allBtn       = new QPushButton(tr("全部"));
        m_oddBtn       = new QPushButton(tr("奇数页"));
        m_evenBtn      = new QPushButton(tr("偶数页"));
        m_firstBtn     = new QPushButton(tr("仅首页"));
        m_skipFirstBtn = new QPushButton(tr("跳过首页"));
        for (auto* b : { m_allBtn, m_oddBtn, m_evenBtn, m_firstBtn, m_skipFirstBtn }) {
            presetRow->addWidget(b);
            connect(b, &QPushButton::clicked, this, &PdfWatermark::onRangePreset);
        }
        rv->addLayout(presetRow);
        connect(m_rangeEdit, &QLineEdit::textChanged,
                this, [this](const QString&){ schedulePreview(); });
    }

    // 水印类型
    {
        auto* typeRow = new QHBoxLayout();
        typeRow->addWidget(new QLabel(tr("水印类型:")));
        m_typeCombo = new QComboBox();
        m_typeCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        m_typeCombo->setMinimumWidth(120);
        m_typeCombo->addItem(tr("文字水印"));
        m_typeCombo->addItem(tr("图片水印"));
        typeRow->addWidget(m_typeCombo);
        typeRow->addStretch();
        rv->addLayout(typeRow);
    }

    // 文字水印行
    m_textRow1 = new QWidget();
    {
        auto* row = new QHBoxLayout(m_textRow1);
        row->setContentsMargins(0, 0, 0, 0);
        row->addWidget(new QLabel(tr("文字:")));
        m_textEdit = new QLineEdit(tr("仅供参考 · DRAFT"));
        m_textEdit->setMinimumWidth(180);
        row->addWidget(m_textEdit, 1);

        row->addWidget(new QLabel(tr("字号:")));
        m_fontSizeSpin = new QSpinBox();
        m_fontSizeSpin->setRange(8, 200);
        m_fontSizeSpin->setValue(48);
        m_fontSizeSpin->setSuffix(tr(" px"));
        m_fontSizeSpin->setFixedWidth(80);
        row->addWidget(m_fontSizeSpin);

        m_colorBtn = new QPushButton(tr("颜色"));
        m_colorBtn->setFixedWidth(70);
        row->addWidget(m_colorBtn);

        rv->addWidget(m_textRow1);
    }
    m_textRow2 = new QWidget();
    {
        auto* row = new QHBoxLayout(m_textRow2);
        row->setContentsMargins(0, 0, 0, 0);
        row->addWidget(new QLabel(tr("不透明度:")));
        m_opacitySpin = new QSpinBox();
        m_opacitySpin->setRange(5, 100);
        m_opacitySpin->setValue(25);
        m_opacitySpin->setSuffix(tr(" %"));
        m_opacitySpin->setFixedWidth(80);
        row->addWidget(m_opacitySpin);

        row->addWidget(new QLabel(tr("角度:")));
        m_rotationSpin = new QSpinBox();
        m_rotationSpin->setRange(-90, 90);
        m_rotationSpin->setValue(-30);
        m_rotationSpin->setSuffix(tr("°"));
        m_rotationSpin->setFixedWidth(80);
        row->addWidget(m_rotationSpin);

        row->addWidget(new QLabel(tr("位置:")));
        m_positionCombo = new QComboBox();
        m_positionCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        m_positionCombo->setMinimumWidth(140);
        m_positionCombo->addItem(tr("斜向稀疏铺"),  0);
        m_positionCombo->addItem(tr("斜向中等铺"),  1);
        m_positionCombo->addItem(tr("斜向密集铺"),  2);
        m_positionCombo->addItem(tr("斜向超密铺"),  3);
        m_positionCombo->addItem(tr("居中单个"),    4);
        m_positionCombo->addItem(tr("右下角"),      5);
        m_positionCombo->addItem(tr("底部居中"),    6);
        m_positionCombo->addItem(tr("顶部居中"),    7);
        m_positionCombo->setCurrentIndex(1);   // 默认中等铺
        row->addWidget(m_positionCombo);
        row->addStretch();

        rv->addWidget(m_textRow2);
    }

    // 图片水印行
    m_imgRow1 = new QWidget();
    {
        auto* row = new QHBoxLayout(m_imgRow1);
        row->setContentsMargins(0, 0, 0, 0);
        m_chooseImgBtn = new QPushButton(tr("选择图片…"));
        row->addWidget(m_chooseImgBtn);
        m_imgPathLabel = new QLabel(tr("（未选择）"));
        m_imgPathLabel->setStyleSheet("color:#868e96;");
        row->addWidget(m_imgPathLabel, 1);
        rv->addWidget(m_imgRow1);
    }
    m_imgRow2 = new QWidget();
    {
        auto* row = new QHBoxLayout(m_imgRow2);
        row->setContentsMargins(0, 0, 0, 0);
        row->addWidget(new QLabel(tr("宽度:")));
        m_imgScaleSpin = new QSpinBox();
        m_imgScaleSpin->setRange(5, 100);
        m_imgScaleSpin->setValue(25);
        m_imgScaleSpin->setSuffix(tr(" %页宽"));
        m_imgScaleSpin->setFixedWidth(110);
        row->addWidget(m_imgScaleSpin);

        row->addWidget(new QLabel(tr("不透明度:")));
        m_imgOpacitySpin = new QSpinBox();
        m_imgOpacitySpin->setRange(5, 100);
        m_imgOpacitySpin->setValue(40);
        m_imgOpacitySpin->setSuffix(tr(" %"));
        m_imgOpacitySpin->setFixedWidth(80);
        row->addWidget(m_imgOpacitySpin);

        row->addWidget(new QLabel(tr("位置:")));
        m_imgPositionCombo = new QComboBox();
        m_imgPositionCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        m_imgPositionCombo->setMinimumWidth(140);
        m_imgPositionCombo->addItem(tr("斜向稀疏铺"), 0);
        m_imgPositionCombo->addItem(tr("斜向中等铺"), 1);
        m_imgPositionCombo->addItem(tr("斜向密集铺"), 2);
        m_imgPositionCombo->addItem(tr("斜向超密铺"), 3);
        m_imgPositionCombo->addItem(tr("居中单个"),   4);
        m_imgPositionCombo->addItem(tr("右下角"),     5);
        m_imgPositionCombo->addItem(tr("底部居中"),   6);
        m_imgPositionCombo->setCurrentIndex(1);
        row->addWidget(m_imgPositionCombo);
        row->addStretch();
        rv->addWidget(m_imgRow2);
    }

    // 输出质量
    {
        auto* row = new QHBoxLayout();
        row->addWidget(new QLabel(tr("输出质量:")));
        m_qualityCombo = new QComboBox();
        m_qualityCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        m_qualityCombo->setMinimumWidth(160);
        m_qualityCombo->addItem(tr("高 (300 DPI · q95)"));
        m_qualityCombo->addItem(tr("中 (200 DPI · q85)"));
        m_qualityCombo->addItem(tr("低 (150 DPI · q70)"));
        m_qualityCombo->addItem(tr("极小 (110 DPI · q60)"));
        m_qualityCombo->addItem(tr("极致 (85 DPI · q50)"));
        m_qualityCombo->addItem(tr("原始 (无压缩)"));
        m_qualityCombo->setCurrentIndex(2);    // 默认低，平衡清晰度和体积
        m_qualityCombo->setToolTip(tr(
            "高：清晰度接近原图，体积最大\n"
            "中：肉眼无差别，体积约 1/3\n"
            "低：常规阅读够用\n"
            "极小：屏幕浏览可接受，文字略显粗糙（推荐文本类长 PDF 用）\n"
            "极致：能看清但有马赛克感，仅适合临时分享\n"
            "原始：不重采样不 JPEG，体积最大"));
        row->addWidget(m_qualityCombo);
        row->addStretch();
        m_exportBtn = new QPushButton(tr("💾 导出加水印 PDF"));
        m_exportBtn->setObjectName("primaryBtn");
        m_exportBtn->setEnabled(false);
        row->addWidget(m_exportBtn);
        rv->addLayout(row);
    }

    rv->addStretch();

    m_statusLabel = new QLabel();
    m_statusLabel->setStyleSheet("color:#868e96; font-size:9pt;");
    m_statusLabel->setWordWrap(true);
    rv->addWidget(m_statusLabel);

    split->addWidget(right);    // 0: 设置
    split->addWidget(center);   // 1: 预览
    split->addWidget(m_pageList);   // 2: 缩略图
    split->setStretchFactor(0, 0);   // 设置面板固定
    split->setStretchFactor(1, 1);   // 预览伸缩
    split->setStretchFactor(2, 0);   // 缩略图列固定
    split->setSizes({360, 560, 170});
    right->setMaximumWidth(420);
    main->addWidget(split, 1);

    // 防抖：水印参数变化时延迟刷新预览
    m_previewDebounce = new QTimer(this);
    m_previewDebounce->setSingleShot(true);
    m_previewDebounce->setInterval(250);
    connect(m_previewDebounce, &QTimer::timeout, this, &PdfWatermark::refreshPreview);

    // 信号
    connect(m_openBtn,      &QPushButton::clicked, this, &PdfWatermark::onOpenPdf);
    connect(m_exportBtn,    &QPushButton::clicked, this, &PdfWatermark::onExportPdf);
    connect(m_chooseImgBtn, &QPushButton::clicked, this, &PdfWatermark::onChooseImageWatermark);
    connect(m_colorBtn,     &QPushButton::clicked, this, &PdfWatermark::onChooseColor);
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx){ onTypeChanged(idx); schedulePreview(); });

    // 缩略图选页 → 重渲预览
    connect(m_pageList, &QListWidget::itemSelectionChanged,
            this, &PdfWatermark::onPageSelected);

    // 水印参数任何变化 → 防抖刷新预览
    connect(m_textEdit,        &QLineEdit::textChanged,   this, [this](const QString&){ schedulePreview(); });
    connect(m_fontSizeSpin,    QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int){ schedulePreview(); });
    connect(m_opacitySpin,     QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int){ schedulePreview(); });
    connect(m_rotationSpin,    QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int){ schedulePreview(); });
    connect(m_positionCombo,   QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int){ schedulePreview(); });
    connect(m_imgScaleSpin,    QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int){ schedulePreview(); });
    connect(m_imgOpacitySpin,  QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int){ schedulePreview(); });
    connect(m_imgPositionCombo,QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int){ schedulePreview(); });
}

// ─────────────────────────────────────────────────────────────
// 打开 / 拖入
// ─────────────────────────────────────────────────────────────

void PdfWatermark::dragEnterEvent(QDragEnterEvent* e)
{
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void PdfWatermark::dropEvent(QDropEvent* e)
{
    for (const QUrl& u : e->mimeData()->urls()) {
        if (u.isLocalFile() && u.toLocalFile().endsWith(".pdf", Qt::CaseInsensitive)) {
            loadPdf(u.toLocalFile());
            return;
        }
    }
}

void PdfWatermark::onOpenPdf()
{
    QString path = QFileDialog::getOpenFileName(
        this, tr("打开 PDF"), QString(), tr("PDF 文件 (*.pdf)"));
    if (path.isEmpty()) return;
    loadPdf(path);
}

void PdfWatermark::loadPdf(const QString& path)
{
    auto err = m_pdfDoc->load(path);
    if (err != QPdfDocument::Error::None) {
        QMessageBox::warning(this, tr("打不开"),
            tr("无法加载 PDF：%1").arg(path));
        return;
    }
    m_pdfPath = path;
    int n = m_pdfDoc->pageCount();
    m_pdfInfoLabel->setText(tr("✅ %1 · 共 %2 页")
                                .arg(QFileInfo(path).fileName()).arg(n));
    buildPageList();
    m_exportBtn->setEnabled(true);
    refreshPreview();
}

void PdfWatermark::buildPageList()
{
    m_pageList->clear();
    int n = m_pdfDoc->pageCount();
    // 缩略图渲染目标尺寸（实际渲染会按页面长宽比保比例）
    const QSize thumbBox(120, 160);
    for (int i = 0; i < n; ++i) {
        QSizeF pts = m_pdfDoc->pagePointSize(i);
        QSize thumbPx = pts.scaled(thumbBox, Qt::KeepAspectRatio).toSize();
        QImage img = m_pdfDoc->render(i, thumbPx);
        auto* item = new QListWidgetItem(tr("第 %1 页").arg(i + 1));
        if (!img.isNull()) item->setIcon(QIcon(QPixmap::fromImage(img)));
        item->setData(Qt::UserRole, i);
        m_pageList->addItem(item);
    }
    if (n > 0) m_pageList->setCurrentRow(0);
}

void PdfWatermark::onPageSelected()
{
    refreshPreview();
}

void PdfWatermark::schedulePreview()
{
    if (m_previewDebounce) m_previewDebounce->start();
}

void PdfWatermark::refreshPreview()
{
    if (!m_pdfDoc || m_pdfDoc->pageCount() <= 0) return;
    QListWidgetItem* it = m_pageList->currentItem();
    if (!it) return;
    int pageIdx = it->data(Qt::UserRole).toInt();

    // 计算预览渲染尺寸：按 label 父级（scroll viewport）可用宽度，保持页面长宽比
    QWidget* viewport = m_previewLabel->parentWidget();
    int availLogical = (viewport ? viewport->width() : 600) - 40;
    if (availLogical < 200) availLogical = 200;

    qreal dpr = devicePixelRatioF();
    if (dpr <= 0) dpr = 1.0;
    int targetPhysW = int(availLogical * dpr + 0.5);

    QSizeF pts = m_pdfDoc->pagePointSize(pageIdx);
    QSize renderPx(targetPhysW, int(targetPhysW * pts.height() / pts.width() + 0.5));
    QImage img = m_pdfDoc->render(pageIdx, renderPx);
    if (img.isNull()) return;

    // 只对"应用范围"内的页面叠加水印预览（让用户立即看出哪些页会被加）
    QSet<int> wmPages = parsePageRange();
    if (wmPages.contains(pageIdx)) {
        // logicalScale = 实际渲染像素 / 假设 300DPI 输出像素。预览这里就当作 1:1
        // （预览本身就是一个分辨率，等比例就好；字号控件值直接用做像素）
        qreal logicalScale = double(renderPx.width()) / double(targetPhysW);   // = 1.0
        drawWatermarkOnImage(img, logicalScale);
    }

    QPixmap pm = QPixmap::fromImage(img);
    pm.setDevicePixelRatio(dpr);
    m_previewLabel->setPixmap(pm);
    m_previewLabel->resize(QSize(int(pm.width() / dpr + 0.5),
                                 int(pm.height() / dpr + 0.5)));
    m_previewLabel->setStyleSheet("background:white; border:1px solid #ced4da;");
}

// ─────────────────────────────────────────────────────────────
// 应用范围
// ─────────────────────────────────────────────────────────────

void PdfWatermark::onRangePreset()
{
    if (m_pdfDoc->pageCount() <= 0) return;
    int n = m_pdfDoc->pageCount();
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    QString r;
    if (btn == m_allBtn)            r = QString("1-%1").arg(n);
    else if (btn == m_oddBtn) {
        QStringList parts;
        for (int i = 1; i <= n; i += 2) parts << QString::number(i);
        r = parts.join(",");
    }
    else if (btn == m_evenBtn) {
        QStringList parts;
        for (int i = 2; i <= n; i += 2) parts << QString::number(i);
        r = parts.join(",");
    }
    else if (btn == m_firstBtn)     r = "1";
    else if (btn == m_skipFirstBtn) r = n > 1 ? QString("2-%1").arg(n) : "";
    m_rangeEdit->setText(r);
    schedulePreview();
}

QSet<int> PdfWatermark::parsePageRange() const
{
    QSet<int> result;
    int n = m_pdfDoc->pageCount();
    if (n <= 0) return result;

    QString s = m_rangeEdit->text().trimmed();
    if (s.isEmpty()) {
        // 留空 = 全部页
        for (int i = 0; i < n; ++i) result.insert(i);
        return result;
    }
    const QStringList parts = s.split(',', Qt::SkipEmptyParts);
    for (const QString& p : parts) {
        QString t = p.trimmed();
        if (t.contains('-')) {
            QStringList ab = t.split('-');
            if (ab.size() == 2) {
                bool ok1 = false, ok2 = false;
                int a = ab[0].trimmed().toInt(&ok1);
                int b = ab[1].trimmed().toInt(&ok2);
                if (ok1 && ok2) {
                    if (a > b) std::swap(a, b);
                    for (int i = a; i <= b; ++i)
                        if (i >= 1 && i <= n) result.insert(i - 1);
                }
            }
        } else {
            bool ok = false;
            int i = t.toInt(&ok);
            if (ok && i >= 1 && i <= n) result.insert(i - 1);
        }
    }
    return result;
}

// ─────────────────────────────────────────────────────────────
// 水印类型切换 / 选色 / 选图
// ─────────────────────────────────────────────────────────────

void PdfWatermark::onTypeChanged(int idx)
{
    bool isText = (idx == 0);
    if (m_textRow1) m_textRow1->setVisible(isText);
    if (m_textRow2) m_textRow2->setVisible(isText);
    if (m_imgRow1)  m_imgRow1->setVisible(!isText);
    if (m_imgRow2)  m_imgRow2->setVisible(!isText);
}

void PdfWatermark::onChooseColor()
{
    QColor c = QColorDialog::getColor(m_textColor, this, tr("选择水印颜色"));
    if (!c.isValid()) return;
    m_textColor = c;
    m_colorBtn->setStyleSheet(QString("background:%1; color:%2;")
        .arg(c.name(),
             (c.lightness() > 128 ? "#000" : "#fff")));
    schedulePreview();
}

void PdfWatermark::onChooseImageWatermark()
{
    QString path = QFileDialog::getOpenFileName(
        this, tr("选择水印图片"), QString(),
        tr("图片 (*.png *.jpg *.jpeg *.bmp *.svg)"));
    if (path.isEmpty()) return;
    QImage img(path);
    if (img.isNull()) {
        QMessageBox::warning(this, tr("无法加载"), tr("无法加载图片：%1").arg(path));
        return;
    }
    m_imageWatermarkPath = path;
    m_imageWatermark = img;
    m_imgPathLabel->setText(QFileInfo(path).fileName());
    schedulePreview();
}

// ─────────────────────────────────────────────────────────────
// 水印绘制：在 QImage 上加水印（坐标 = 图片像素）
// ─────────────────────────────────────────────────────────────

void PdfWatermark::drawWatermarkOnImage(QImage& img, qreal logicalScale)
{
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const bool isText = (m_typeCombo->currentIndex() == 0);
    const QSize size = img.size();

    if (isText) {
        const QString text = m_textEdit->text();
        if (text.isEmpty()) return;
        const int op = m_opacitySpin->value();
        if (op <= 0) return;
        p.setOpacity(op / 100.0);

        QFont font;
        // 字号按"页面像素 × 字号比例"算 — 让水印随页面分辨率缩放
        int fontPx = qMax(8, int(m_fontSizeSpin->value() * logicalScale));
        font.setPixelSize(fontPx);
        p.setFont(font);
        p.setPen(m_textColor);

        const int rot = m_rotationSpin->value();
        const int posIdx = m_positionCombo->currentIndex();
        QFontMetrics fm(font);
        const int textW = fm.horizontalAdvance(text);
        const int textH = fm.height();

        // posIdx: 0=斜稀疏 1=斜中等 2=斜密集 3=斜超密 4=居中 5=右下 6=底中 7=顶中
        auto drawTile = [&](qreal xMult, qreal yMult) {
            p.translate(size.width() / 2.0, size.height() / 2.0);
            p.rotate(rot);
            const int diag = int(std::hypot(size.width(), size.height())) + 200;
            const int tileX = textW + qMax(40, int(fontPx * xMult));
            const int tileY = qMax(60, int(fontPx * yMult));
            for (int y = -diag; y < diag; y += tileY) {
                int offset = ((y / tileY) % 2 == 0) ? 0 : tileX / 2;
                for (int x = -diag + offset; x < diag; x += tileX) {
                    p.drawText(x, y, text);
                }
            }
        };

        if (posIdx == 0)        drawTile(5.5, 6.0);   // 稀疏：行/列间距大
        else if (posIdx == 1)   drawTile(2.5, 3.2);   // 中等（旧默认）
        else if (posIdx == 2)   drawTile(1.2, 2.0);   // 密集
        else if (posIdx == 3)   drawTile(0.5, 1.4);   // 超密（几乎重叠）
        else if (posIdx == 4) {
            // 居中单个
            p.translate(size.width() / 2.0, size.height() / 2.0);
            p.rotate(rot);
            p.drawText(-textW / 2, textH / 2, text);
        } else if (posIdx == 5) {
            int margin = int(20 * logicalScale);
            p.drawText(size.width() - textW - margin,
                       size.height() - margin, text);
        } else if (posIdx == 6) {
            int margin = int(30 * logicalScale);
            p.drawText((size.width() - textW) / 2,
                       size.height() - margin, text);
        } else {
            int margin = int(30 * logicalScale);
            p.drawText((size.width() - textW) / 2,
                       margin + textH, text);
        }
    } else {
        // 图片水印
        if (m_imageWatermark.isNull()) return;
        const int op = m_imgOpacitySpin->value();
        if (op <= 0) return;
        p.setOpacity(op / 100.0);

        const int wmScalePct = m_imgScaleSpin->value();
        const int targetW = qMax(20, size.width() * wmScalePct / 100);
        QImage wm = m_imageWatermark.scaledToWidth(targetW, Qt::SmoothTransformation);
        const int posIdx = m_imgPositionCombo->currentIndex();
        const int margin = int(20 * logicalScale);

        // 同样按密度档位铺
        auto drawTileImg = [&](qreal xMult, qreal yMult) {
            p.translate(size.width() / 2.0, size.height() / 2.0);
            p.rotate(-30);
            const int diag = int(std::hypot(size.width(), size.height())) + 200;
            const int tileX = wm.width() + qMax(20, int(wm.width() * xMult));
            const int tileY = wm.height() + qMax(20, int(wm.height() * yMult));
            for (int y = -diag; y < diag; y += tileY) {
                int offset = ((y / tileY) % 2 == 0) ? 0 : tileX / 2;
                for (int x = -diag + offset; x < diag; x += tileX) {
                    p.drawImage(x, y, wm);
                }
            }
        };

        if (posIdx == 0)        drawTileImg(1.2, 1.5);   // 稀疏
        else if (posIdx == 1)   drawTileImg(0.5, 0.8);   // 中等
        else if (posIdx == 2)   drawTileImg(0.2, 0.4);   // 密集
        else if (posIdx == 3)   drawTileImg(0.05, 0.1);  // 超密
        else if (posIdx == 4)
            p.drawImage((size.width() - wm.width()) / 2,
                        (size.height() - wm.height()) / 2, wm);
        else if (posIdx == 5)
            p.drawImage(size.width() - wm.width() - margin,
                        size.height() - wm.height() - margin, wm);
        else
            p.drawImage((size.width() - wm.width()) / 2,
                        size.height() - wm.height() - margin, wm);
    }
}

// ─────────────────────────────────────────────────────────────
// 导出 PDF
// ─────────────────────────────────────────────────────────────

void PdfWatermark::onExportPdf()
{
    if (m_pdfDoc->pageCount() <= 0) {
        QMessageBox::warning(this, tr("无 PDF"), tr("请先打开一个 PDF。"));
        return;
    }
    const QSet<int> wmPages = parsePageRange();
    if (wmPages.isEmpty()) {
        if (QMessageBox::question(this, tr("无应用页"),
                tr("当前应用范围为空（没有任何页加水印）。继续导出原 PDF 拷贝？")) != QMessageBox::Yes)
            return;
    }

    QString defaultPath = QFileInfo(m_pdfPath).absolutePath() + "/"
                        + QFileInfo(m_pdfPath).completeBaseName() + "_watermarked.pdf";
    QString savePath = QFileDialog::getSaveFileName(
        this, tr("保存 PDF"), defaultPath, tr("PDF 文件 (*.pdf)"));
    if (savePath.isEmpty()) return;
    if (!savePath.endsWith(".pdf", Qt::CaseInsensitive)) savePath += ".pdf";

    // 质量预设
    struct QP { int renderDpi; int jpegQ; };
    static const QP presets[] = {
        {300, 95},   // 高
        {200, 85},   // 中
        {150, 70},   // 低
        {110, 60},   // 极小
        { 85, 50},   // 极致
        {  0,  0},   // 原始（不重采样、不 JPEG）
    };
    int qi = qBound(0, m_qualityCombo->currentIndex(),
                    int(sizeof(presets) / sizeof(presets[0])) - 1);
    const int renderDpi = presets[qi].renderDpi;
    const int jpegQ     = presets[qi].jpegQ;
    const int pdfDpi    = 300;

    QPdfWriter writer(savePath);
    writer.setResolution(pdfDpi);
    writer.setPageMargins(QMarginsF(0, 0, 0, 0));

    QSizeF firstPts = m_pdfDoc->pagePointSize(0);
    writer.setPageSize(QPageSize(firstPts, QPageSize::Point, "Page", QPageSize::ExactMatch));

    QPainter pdfPainter(&writer);
    pdfPainter.setRenderHint(QPainter::Antialiasing, true);
    pdfPainter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    pdfPainter.setRenderHint(QPainter::TextAntialiasing, true);

    int n = m_pdfDoc->pageCount();
    m_statusLabel->setText(tr("⏳ 正在导出…"));
    qApp->processEvents();

    for (int i = 0; i < n; ++i) {
        if (i > 0) {
            QSizeF pts = m_pdfDoc->pagePointSize(i);
            writer.setPageSize(QPageSize(pts, QPageSize::Point, "Page", QPageSize::ExactMatch));
            writer.newPage();
        }

        QRect target = pdfPainter.viewport();
        QSize renderPx;
        if (renderDpi > 0 && renderDpi < pdfDpi) {
            double s = double(renderDpi) / pdfDpi;
            renderPx = QSize(int(target.width()  * s + 0.5),
                             int(target.height() * s + 0.5));
        } else {
            renderPx = QSize(target.width(), target.height());
        }

        QImage page = m_pdfDoc->render(i, renderPx);
        if (page.isNull()) continue;

        // 应用范围内的页面加水印
        if (wmPages.contains(i)) {
            // logicalScale：以 PDF 内部 300 DPI 渲染的输出像素为参考，
            // 实际渲染像素 / 300DPI 输出像素 = renderDpi / pdfDpi
            qreal logicalScale = double(renderPx.width()) / target.width();
            drawWatermarkOnImage(page, logicalScale);
        }

        // JPEG round-trip（同 pdf-stamp 的瘦身机制）
        if (jpegQ > 0 && !page.hasAlphaChannel()) {
            QByteArray bytes;
            QBuffer buf(&bytes);
            buf.open(QIODevice::WriteOnly);
            if (page.save(&buf, "JPEG", jpegQ)) {
                buf.close();
                QImage rt = QImage::fromData(bytes, "JPEG");
                if (!rt.isNull()) page = rt;
            } else {
                buf.close();
            }
        }

        pdfPainter.drawImage(target, page);
        m_statusLabel->setText(tr("⏳ 已处理 %1 / %2 页…").arg(i + 1).arg(n));
        qApp->processEvents();
    }
    pdfPainter.end();

    QFileInfo fi(savePath);
    QString sizeStr;
    qint64 bytes = fi.size();
    if (bytes < 1024 * 1024) sizeStr = QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    else sizeStr = QString("%1 MB").arg(bytes / 1024.0 / 1024.0, 0, 'f', 2);

    m_statusLabel->setText(tr("✅ 完成 · %1").arg(sizeStr));

    QMessageBox box(this);
    box.setWindowTitle(tr("导出完成"));
    box.setText(tr("已保存到:\n%1\n\n共 %2 页 · 体积 %3 · %4")
                    .arg(savePath).arg(n).arg(sizeStr)
                    .arg(wmPages.isEmpty() ? tr("无水印拷贝")
                                           : tr("已对 %1 页加水印").arg(wmPages.size())));
    box.setIcon(QMessageBox::Information);
    auto* openBtn = box.addButton(tr("打开 PDF"), QMessageBox::ActionRole);
    auto* dirBtn  = box.addButton(tr("打开目录"), QMessageBox::ActionRole);
    box.addButton(QMessageBox::Ok);
    box.exec();
    if (box.clickedButton() == openBtn)
        QDesktopServices::openUrl(QUrl::fromLocalFile(savePath));
    else if (box.clickedButton() == dirBtn)
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(savePath).absolutePath()));
}
