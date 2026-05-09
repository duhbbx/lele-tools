#include "imageformatconversion.h"

#include <QApplication>
#include <QButtonGroup>
#include <QBuffer>
#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QStandardPaths>
#include <QSvgRenderer>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>

REGISTER_DYNAMICOBJECT(ImageFormatConversion);

namespace {

// 把图片字节直接嵌进 SVG 文件：raster → svg 的最简方案
QString svgWrapRasterAsBase64(const QImage& img, const QString& mimeType)
{
    QByteArray raw;
    QBuffer buf(&raw);
    buf.open(QIODevice::WriteOnly);
    QString fmt = mimeType.contains("png", Qt::CaseInsensitive) ? "PNG"
                : mimeType.contains("jpeg", Qt::CaseInsensitive) ? "JPG"
                : mimeType.contains("webp", Qt::CaseInsensitive) ? "WEBP"
                : "PNG";
    img.save(&buf, fmt.toUtf8().constData());
    QString b64 = raw.toBase64();
    return QString(R"(<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink"
     width="%1" height="%2" viewBox="0 0 %1 %2">
  <image width="%1" height="%2" xlink:href="data:%3;base64,%4"/>
</svg>
)").arg(img.width()).arg(img.height()).arg(mimeType).arg(b64);
}

QString humanSize(qint64 bytes)
{
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);
}

bool isLossyFormat(const QString& fmt)
{
    QString f = fmt.toLower();
    return f == "jpg" || f == "jpeg" || f == "webp";
}

} // namespace


// ============= ctor =============

ImageFormatConversion::ImageFormatConversion()
    : QWidget(nullptr), DynamicObjectBase()
{
    setAcceptDrops(true);
    setupUI();
}

// ============= UI =============

void ImageFormatConversion::setupUI()
{
    setStyleSheet(R"(
        QLineEdit, QSpinBox, QComboBox {
            border: 1px solid #ced4da; border-radius: 4px;
            padding: 5px 8px; background: #fff; min-height: 22px;
            selection-background-color: #b8d4ff;
        }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus { border-color: #228be6; }
        QPushButton {
            border: 1px solid #ced4da; border-radius: 4px;
            padding: 5px 14px; background: #ffffff; min-height: 22px; color: #343a40;
        }
        QPushButton:hover  { background: #f1f3f5; border-color: #adb5bd; }
        QPushButton:pressed{ background: #e9ecef; }
        QPushButton:disabled { color: #adb5bd; background: #f8f9fa; }
        QPushButton#primaryBtn {
            background: #228be6; border: 1px solid #1c7ed6; color: white; font-weight: bold;
        }
        QPushButton#primaryBtn:hover  { background: #1c7ed6; }
        QPushButton#primaryBtn:disabled { background: #adb5bd; border-color: #adb5bd; color: #f1f3f5; }
        QFrame#card {
            background: #f8f9fa; border: 1px solid #dee2e6; border-radius: 6px;
        }
        QListWidget {
            border: 1px solid #dee2e6; border-radius: 4px; background: #ffffff;
            outline: none;
        }
        QListWidget::item { padding: 4px 6px; border-bottom: 1px solid #f1f3f5; }
        QListWidget::item:hover { background: #f1f3f5; }
        QListWidget::item:selected { background: #e7f5ff; color: #1864ab; }
        QRadioButton { background: transparent; }
        QCheckBox { background: transparent; }
        QLabel { background: transparent; }
    )");

    auto* main = new QVBoxLayout(this);
    main->setContentsMargins(12, 10, 12, 10);
    main->setSpacing(10);

    // ── 文件列表 ──
    {
        auto* card = new QFrame();
        card->setObjectName("card");
        auto* col = new QVBoxLayout(card);
        col->setContentsMargins(12, 10, 12, 10);
        col->setSpacing(6);

        auto* hdrRow = new QHBoxLayout();
        auto* lbl = new QLabel(tr("📁 待转换图片"));
        lbl->setStyleSheet("font-weight: bold; color: #495057;");
        hdrRow->addWidget(lbl);
        hdrRow->addStretch();
        m_addBtn = new QPushButton(tr("+ 添加图片"));
        m_removeBtn = new QPushButton(tr("- 移除"));
        m_clearBtn = new QPushButton(tr("清空"));
        hdrRow->addWidget(m_addBtn);
        hdrRow->addWidget(m_removeBtn);
        hdrRow->addWidget(m_clearBtn);
        col->addLayout(hdrRow);

        m_listWidget = new QListWidget();
        m_listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
        m_listWidget->setIconSize(QSize(56, 56));   // 缩略图尺寸
        m_listWidget->setUniformItemSizes(true);
        col->addWidget(m_listWidget, 1);

        m_listStatus = new QLabel(tr("拖拽图片到此处，或点上方「添加图片」。支持 PNG / JPEG / WEBP / BMP / GIF / TIFF / ICO / SVG 等。"));
        m_listStatus->setStyleSheet("color:#868e96; font-size:8pt;");
        m_listStatus->setWordWrap(true);
        col->addWidget(m_listStatus);

        main->addWidget(card, 1);
    }

    // ── 输出选项 ──
    {
        auto* card = new QFrame();
        card->setObjectName("card");
        auto* grid = new QVBoxLayout(card);
        grid->setContentsMargins(12, 10, 12, 10);
        grid->setSpacing(6);

        auto* hdr = new QLabel(tr("⚙️ 输出选项"));
        hdr->setStyleSheet("font-weight: bold; color: #495057;");
        grid->addWidget(hdr);

        // 目标格式 + 质量
        auto* row1 = new QHBoxLayout();
        row1->setSpacing(8);
        row1->addWidget(new QLabel(tr("目标格式:")));
        m_formatCombo = new QComboBox();
        m_formatCombo->addItems({"PNG", "JPEG", "WEBP", "BMP", "TIFF", "ICO", "SVG"});
        m_formatCombo->setCurrentText("PNG");
        m_formatCombo->setMinimumWidth(120);
        row1->addWidget(m_formatCombo);

        row1->addSpacing(16);
        m_qualityLabel = new QLabel(tr("质量:"));
        row1->addWidget(m_qualityLabel);
        m_qualitySlider = new QSlider(Qt::Horizontal);
        m_qualitySlider->setRange(1, 100);
        m_qualitySlider->setValue(90);
        m_qualitySlider->setMinimumWidth(140);
        row1->addWidget(m_qualitySlider);
        auto* qualityVal = new QLabel("90");
        qualityVal->setMinimumWidth(28);
        row1->addWidget(qualityVal);
        connect(m_qualitySlider, &QSlider::valueChanged, qualityVal,
                [qualityVal](int v){ qualityVal->setText(QString::number(v)); });

        row1->addStretch();
        grid->addLayout(row1);

        // 输出目录
        auto* row2 = new QHBoxLayout();
        row2->setSpacing(8);
        row2->addWidget(new QLabel(tr("输出目录:")));
        m_outputDirEdit = new QLineEdit();
        m_outputDirEdit->setPlaceholderText(tr("默认与源文件同目录"));
        row2->addWidget(m_outputDirEdit, 1);
        m_outputDirBtn = new QPushButton(tr("浏览..."));
        row2->addWidget(m_outputDirBtn);
        grid->addLayout(row2);

        // 调整尺寸
        auto* row3 = new QHBoxLayout();
        row3->setSpacing(8);
        row3->addWidget(new QLabel(tr("调整尺寸:")));
        m_resizeKeepRadio = new QRadioButton(tr("保持原尺寸"));
        m_resizeMaxRadio = new QRadioButton(tr("适应最大边"));
        m_resizeExactRadio = new QRadioButton(tr("精确尺寸"));
        m_resizeKeepRadio->setChecked(true);
        auto* grp = new QButtonGroup(this);
        grp->addButton(m_resizeKeepRadio);
        grp->addButton(m_resizeMaxRadio);
        grp->addButton(m_resizeExactRadio);
        row3->addWidget(m_resizeKeepRadio);
        row3->addWidget(m_resizeMaxRadio);
        m_maxDimSpin = new QSpinBox();
        m_maxDimSpin->setRange(16, 16384);
        m_maxDimSpin->setValue(1920);
        m_maxDimSpin->setSuffix(" px");
        row3->addWidget(m_maxDimSpin);
        row3->addSpacing(8);
        row3->addWidget(m_resizeExactRadio);
        m_exactWSpin = new QSpinBox();
        m_exactWSpin->setRange(1, 16384);
        m_exactWSpin->setValue(800);
        m_exactWSpin->setSuffix(" w");
        row3->addWidget(m_exactWSpin);
        row3->addWidget(new QLabel(tr("×")));
        m_exactHSpin = new QSpinBox();
        m_exactHSpin->setRange(1, 16384);
        m_exactHSpin->setValue(600);
        m_exactHSpin->setSuffix(" h");
        row3->addWidget(m_exactHSpin);
        m_keepAspectCheck = new QCheckBox(tr("锁比例"));
        m_keepAspectCheck->setChecked(true);
        row3->addWidget(m_keepAspectCheck);
        row3->addStretch();
        grid->addLayout(row3);

        // 限制最大输出大小
        auto* row4 = new QHBoxLayout();
        m_sizeLimitCheck = new QCheckBox(tr("限制最大输出大小"));
        m_sizeLimitCheck->setChecked(false);
        m_sizeLimitCheck->setToolTip(tr(
            "勾选后会自动降质量并按比例缩小，直到输出文件 ≤ 指定大小"));
        row4->addWidget(m_sizeLimitCheck);
        m_sizeLimitSpin = new QSpinBox();
        m_sizeLimitSpin->setRange(1, 100 * 1024);  // 1 KB ~ 100 MB
        m_sizeLimitSpin->setValue(500);
        m_sizeLimitSpin->setSuffix(tr(" KB"));
        m_sizeLimitSpin->setMinimumWidth(110);
        m_sizeLimitSpin->setEnabled(false);
        row4->addWidget(m_sizeLimitSpin);
        connect(m_sizeLimitCheck, &QCheckBox::toggled,
                m_sizeLimitSpin, &QSpinBox::setEnabled);
        row4->addStretch();
        grid->addLayout(row4);

        // 覆盖
        auto* row5 = new QHBoxLayout();
        m_overwriteCheck = new QCheckBox(tr("已存在同名文件时覆盖"));
        m_overwriteCheck->setChecked(false);
        row5->addWidget(m_overwriteCheck);
        row5->addStretch();
        grid->addLayout(row5);

        main->addWidget(card);
    }

    // ── 操作 + 进度 ──
    {
        auto* row = new QHBoxLayout();
        m_convertBtn = new QPushButton(tr("🚀 开始转换"));
        m_convertBtn->setObjectName("primaryBtn");
        m_convertBtn->setMinimumWidth(120);
        row->addWidget(m_convertBtn);
        m_progress = new QProgressBar();
        m_progress->setRange(0, 1);
        m_progress->setValue(0);
        m_progress->setFormat(tr("%v / %m"));
        row->addWidget(m_progress, 1);
        main->addLayout(row);

        m_statusLabel = new QLabel(tr("就绪"));
        m_statusLabel->setStyleSheet(
            "color: #2e7d32; padding: 6px 10px; background: #e8f5e8;"
            " border-radius: 4px; font-weight: bold;");
        main->addWidget(m_statusLabel);
    }

    // 信号
    connect(m_addBtn, &QPushButton::clicked, this, &ImageFormatConversion::onAddFiles);
    connect(m_removeBtn, &QPushButton::clicked, this, &ImageFormatConversion::onRemoveSelected);
    connect(m_clearBtn, &QPushButton::clicked, this, &ImageFormatConversion::onClearAll);
    connect(m_outputDirBtn, &QPushButton::clicked, this, &ImageFormatConversion::onPickOutputDir);
    connect(m_convertBtn, &QPushButton::clicked, this, &ImageFormatConversion::onConvert);
    connect(m_formatCombo, &QComboBox::currentTextChanged, this,
            [this](const QString&){ onTargetFormatChanged(); });
    connect(m_resizeKeepRadio, &QRadioButton::toggled, this, &ImageFormatConversion::onResizeModeChanged);
    connect(m_resizeMaxRadio, &QRadioButton::toggled, this, &ImageFormatConversion::onResizeModeChanged);
    connect(m_resizeExactRadio, &QRadioButton::toggled, this, &ImageFormatConversion::onResizeModeChanged);

    onTargetFormatChanged();
    onResizeModeChanged();
}

// ============= 拖拽 =============

void ImageFormatConversion::dragEnterEvent(QDragEnterEvent* e)
{
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void ImageFormatConversion::dropEvent(QDropEvent* e)
{
    QStringList paths;
    for (const QUrl& u : e->mimeData()->urls()) {
        if (u.isLocalFile()) paths << u.toLocalFile();
    }
    if (!paths.isEmpty()) {
        addPaths(paths);
        e->acceptProposedAction();
    }
}

// ============= 列表 =============

void ImageFormatConversion::onAddFiles()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this, tr("选择图片"), QString(),
        tr("图片 (*.png *.jpg *.jpeg *.webp *.bmp *.gif *.tif *.tiff *.ico *.svg);;所有文件 (*)"));
    if (!files.isEmpty()) addPaths(files);
}

void ImageFormatConversion::addPaths(const QStringList& paths)
{
    static const QStringList kExts = {
        "png","jpg","jpeg","webp","bmp","gif","tif","tiff","ico","svg"
    };
    const int thumbSize = 56;
    qreal dpr = qApp->devicePixelRatio();
    int renderPx = int(thumbSize * dpr);

    for (const QString& path : paths) {
        QFileInfo fi(path);
        if (!fi.exists() || !fi.isFile()) continue;
        if (!kExts.contains(fi.suffix().toLower())) continue;

        QString suffix = fi.suffix().toLower();
        QString dim;
        QPixmap thumb;

        if (suffix == "svg") {
            QSvgRenderer r(fi.absoluteFilePath());
            if (r.isValid()) {
                QSize s = r.defaultSize();
                dim = QString("%1×%2 (vector)").arg(s.width()).arg(s.height());

                // 渲染缩略图
                QSize fit = s.isEmpty() ? QSize(renderPx, renderPx)
                                        : s.scaled(renderPx, renderPx, Qt::KeepAspectRatio);
                QImage img(fit, QImage::Format_ARGB32);
                img.fill(Qt::transparent);
                QPainter p(&img);
                p.setRenderHint(QPainter::Antialiasing, true);
                p.setRenderHint(QPainter::SmoothPixmapTransform, true);
                r.render(&p);
                p.end();
                thumb = QPixmap::fromImage(img);
            } else {
                dim = "(SVG)";
            }
        } else {
            QImageReader r(fi.absoluteFilePath());
            r.setAutoTransform(true);
            QSize origSize = r.size();
            dim = origSize.isValid()
                      ? QString("%1×%2").arg(origSize.width()).arg(origSize.height())
                      : "?";

            // 让 QImageReader 直接读缩放尺寸（节省内存）
            if (origSize.isValid()) {
                QSize fit = origSize.scaled(renderPx, renderPx, Qt::KeepAspectRatio);
                r.setScaledSize(fit);
            }
            QImage img = r.read();
            if (!img.isNull()) thumb = QPixmap::fromImage(img);
        }

        if (!thumb.isNull()) thumb.setDevicePixelRatio(dpr);

        QString text = QString("%1\n%2  ·  %3")
                           .arg(fi.fileName())
                           .arg(dim)
                           .arg(humanSize(fi.size()));
        auto* it = new QListWidgetItem(text);
        if (!thumb.isNull()) it->setIcon(QIcon(thumb));
        it->setData(Qt::UserRole, fi.absoluteFilePath());
        it->setToolTip(fi.absoluteFilePath());
        m_listWidget->addItem(it);
    }
    m_listStatus->setText(tr("共 %1 张图片").arg(m_listWidget->count()));
}

void ImageFormatConversion::onRemoveSelected()
{
    for (auto* it : m_listWidget->selectedItems()) {
        delete it;
    }
    m_listStatus->setText(tr("共 %1 张图片").arg(m_listWidget->count()));
}

void ImageFormatConversion::onClearAll()
{
    m_listWidget->clear();
    m_listStatus->setText(tr("共 0 张图片"));
}

void ImageFormatConversion::onPickOutputDir()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("选择输出目录"),
        m_outputDirEdit->text().isEmpty()
            ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
            : m_outputDirEdit->text());
    if (!dir.isEmpty()) m_outputDirEdit->setText(dir);
}

void ImageFormatConversion::onTargetFormatChanged()
{
    bool lossy = isLossyFormat(m_formatCombo->currentText());
    m_qualityLabel->setEnabled(lossy);
    m_qualitySlider->setEnabled(lossy);
}

void ImageFormatConversion::onResizeModeChanged()
{
    bool max = m_resizeMaxRadio->isChecked();
    bool exact = m_resizeExactRadio->isChecked();
    m_maxDimSpin->setEnabled(max);
    m_exactWSpin->setEnabled(exact);
    m_exactHSpin->setEnabled(exact);
    m_keepAspectCheck->setEnabled(exact);
}

// ============= 加载 / 缩放 / 保存 =============

QImage ImageFormatConversion::loadImage(const QString& path) const
{
    QFileInfo fi(path);
    if (fi.suffix().toLower() == "svg") {
        QSvgRenderer renderer(path);
        if (!renderer.isValid()) return QImage();
        // 渲染尺寸：根据 resize 模式决定
        QSize defSize = renderer.defaultSize();
        if (defSize.isEmpty()) defSize = QSize(1024, 1024);
        QSize target = defSize;
        if (m_resizeMaxRadio->isChecked()) {
            int max = m_maxDimSpin->value();
            target = defSize.scaled(max, max, Qt::KeepAspectRatio);
        } else if (m_resizeExactRadio->isChecked()) {
            target = QSize(m_exactWSpin->value(), m_exactHSpin->value());
            if (m_keepAspectCheck->isChecked())
                target = defSize.scaled(target, Qt::KeepAspectRatio);
        }
        QImage out(target, QImage::Format_ARGB32);
        out.fill(Qt::transparent);
        QPainter p(&out);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        renderer.render(&p);
        p.end();
        return out;
    }
    QImageReader reader(path);
    reader.setAutoTransform(true);  // 尊重 EXIF 方向
    return reader.read();
}

QImage ImageFormatConversion::applyResize(const QImage& src) const
{
    if (src.isNull()) return src;
    if (m_resizeKeepRadio->isChecked()) return src;
    if (m_resizeMaxRadio->isChecked()) {
        int max = m_maxDimSpin->value();
        if (src.width() <= max && src.height() <= max) return src;
        return src.scaled(max, max, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    // 精确尺寸
    QSize target(m_exactWSpin->value(), m_exactHSpin->value());
    Qt::AspectRatioMode mode = m_keepAspectCheck->isChecked()
                                   ? Qt::KeepAspectRatio
                                   : Qt::IgnoreAspectRatio;
    return src.scaled(target, mode, Qt::SmoothTransformation);
}

bool ImageFormatConversion::saveAsSvgWrappedRaster(const QImage& img, const QString& outPath, QString* err) const
{
    QString svg = svgWrapRasterAsBase64(img, "image/png");
    QFile f(outPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (err) *err = f.errorString();
        return false;
    }
    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    ts << svg;
    f.close();
    return true;
}

bool ImageFormatConversion::saveWithSizeLimit(const QImage& img, const QString& outPath,
                                              const QString& fmt, qint64 limitBytes,
                                              QString* err) const
{
    QString upper = fmt.toUpper();
    bool lossy = isLossyFormat(fmt);
    bool isSvg = upper == "SVG";

    // 直接落盘并返回文件大小（出错返回 -1）
    auto writeAttempt = [&](const QImage& im, int quality) -> qint64 {
        if (isSvg) {
            QString s = svgWrapRasterAsBase64(im, "image/png");
            QFile f(outPath);
            if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                if (err) *err = f.errorString();
                return -1;
            }
            QTextStream ts(&f);
            ts.setEncoding(QStringConverter::Utf8);
            ts << s;
            f.close();
            return QFileInfo(outPath).size();
        }
        QImageWriter w(outPath, upper.toUtf8());
        if (lossy) w.setQuality(quality);
        else if (upper == "PNG") {
            int compression = qBound(0, m_qualitySlider->value() / 10, 9);
            w.setCompression(compression);
        }
        if (!w.write(im)) {
            if (err) *err = w.errorString();
            return -1;
        }
        return QFileInfo(outPath).size();
    };

    // 1) 用当前质量先存一次
    int curQ = m_qualitySlider->value();
    qint64 size = writeAttempt(img, curQ);
    if (size < 0) return false;
    if (size <= limitBytes) return true;

    // 2) lossy: 二分降质量（5..curQ-1）
    if (lossy) {
        int lo = 5, hi = curQ - 1, bestQ = -1;
        qint64 bestSize = 0;
        while (lo <= hi) {
            int mid = (lo + hi) / 2;
            size = writeAttempt(img, mid);
            if (size < 0) return false;
            if (size <= limitBytes) {
                bestQ = mid;
                bestSize = size;
                lo = mid + 1;
            } else {
                hi = mid - 1;
            }
        }
        if (bestQ > 0) {
            // 写入最终最佳质量版本
            if (writeAttempt(img, bestQ) < 0) return false;
            (void)bestSize;
            return true;
        }
        // 即便 q=5 仍超限 → 落入降采样
    }

    // 3) 按比例缩小（保留比例），每轮 0.85x，质量降到中档
    int scaleQuality = lossy ? 60 : 0;
    QImage current = img;
    for (int step = 0; step < 8; ++step) {
        QSize newSize(int(current.width() * 0.85), int(current.height() * 0.85));
        if (newSize.width() < 32 || newSize.height() < 32) break;
        current = current.scaled(newSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        size = writeAttempt(current, scaleQuality);
        if (size < 0) return false;
        if (size <= limitBytes) {
            // 在缩小后的尺寸上再二分一次质量，挤出更高质量
            if (lossy) {
                int lo = scaleQuality, hi = curQ, bestQ = scaleQuality;
                while (lo <= hi) {
                    int mid = (lo + hi) / 2;
                    size = writeAttempt(current, mid);
                    if (size < 0) return false;
                    if (size <= limitBytes) {
                        bestQ = mid;
                        lo = mid + 1;
                    } else {
                        hi = mid - 1;
                    }
                }
                writeAttempt(current, bestQ);
            }
            return true;
        }
    }

    if (err) *err = QString(QObject::tr("无法压缩到 %1 KB 以下"))
                        .arg(limitBytes / 1024);
    return false;
}

bool ImageFormatConversion::saveImage(const QImage& img, const QString& outPath,
                                      const QString& fmt, QString* err) const
{
    QString upper = fmt.toUpper();
    if (upper == "SVG") return saveAsSvgWrappedRaster(img, outPath, err);

    // 写之前保证输出目录存在
    QDir().mkpath(QFileInfo(outPath).absolutePath());

    QImageWriter writer(outPath, upper.toUtf8());
    if (isLossyFormat(fmt)) {
        writer.setQuality(m_qualitySlider->value());
    } else if (upper == "PNG") {
        // PNG 用质量映射成压缩等级（0=快/差,9=慢/好；slider 越高 → 压缩越高）
        int compression = qBound(0, m_qualitySlider->value() / 10, 9);
        writer.setCompression(compression);
    }
    if (!writer.write(img)) {
        if (err) *err = writer.errorString();
        return false;
    }
    return true;
}

// ============= 转换主流程 =============

void ImageFormatConversion::onConvert()
{
    if (m_listWidget->count() == 0) {
        QMessageBox::information(this, tr("提示"), tr("请先添加待转换的图片。"));
        return;
    }
    QString fmt = m_formatCombo->currentText();
    QString customOutDir = m_outputDirEdit->text().trimmed();

    int total = m_listWidget->count();
    int ok = 0, fail = 0, skipped = 0;
    QStringList errors;

    m_progress->setRange(0, total);
    m_progress->setValue(0);
    m_convertBtn->setEnabled(false);

    for (int i = 0; i < total; ++i) {
        QString src = m_listWidget->item(i)->data(Qt::UserRole).toString();
        QFileInfo fi(src);
        QString outDir = customOutDir.isEmpty() ? fi.absolutePath() : customOutDir;
        QString outName = fi.completeBaseName() + "." + fmt.toLower();
        QString outPath = outDir + "/" + outName;

        if (QFile::exists(outPath) && !m_overwriteCheck->isChecked()) {
            // 加时间戳避免冲突
            outPath = outDir + "/" + fi.completeBaseName() + "_"
                     + QDateTime::currentDateTime().toString("HHmmss")
                     + "." + fmt.toLower();
        }

        QImage img = loadImage(src);
        if (img.isNull()) {
            ++fail;
            errors << QString("%1: 无法读取").arg(fi.fileName());
            m_progress->setValue(i + 1);
            QApplication::processEvents();
            continue;
        }
        QImage out = applyResize(img);
        QString err;
        bool savedOk = false;
        if (m_sizeLimitCheck->isChecked()) {
            qint64 limit = qint64(m_sizeLimitSpin->value()) * 1024;  // KB → bytes
            // 输出 dir 必须存在
            QDir().mkpath(QFileInfo(outPath).absolutePath());
            savedOk = saveWithSizeLimit(out, outPath, fmt, limit, &err);
        } else {
            savedOk = saveImage(out, outPath, fmt, &err);
        }
        if (savedOk) {
            ++ok;
        } else {
            ++fail;
            errors << QString("%1: %2").arg(fi.fileName(), err);
        }
        m_progress->setValue(i + 1);
        m_statusLabel->setText(tr("正在转换... %1 / %2").arg(i + 1).arg(total));
        QApplication::processEvents();
    }

    m_convertBtn->setEnabled(true);
    QString summary = tr("完成：成功 %1，失败 %2").arg(ok).arg(fail);
    if (skipped > 0) summary += tr("，跳过 %1").arg(skipped);
    m_statusLabel->setText(summary);

    QMessageBox box(this);
    box.setWindowTitle(tr("转换完成"));
    box.setIcon(fail == 0 ? QMessageBox::Information : QMessageBox::Warning);
    QString msg = summary;
    if (!errors.isEmpty()) {
        msg += "\n\n" + tr("失败明细：") + "\n" + errors.mid(0, 10).join("\n");
        if (errors.size() > 10) msg += tr("\n... 共 %1 项错误").arg(errors.size());
    }
    box.setText(msg);
    QPushButton* openDirBtn = nullptr;
    if (ok > 0) {
        QString outDir = customOutDir.isEmpty()
                            ? QFileInfo(m_listWidget->item(0)->data(Qt::UserRole).toString()).absolutePath()
                            : customOutDir;
        openDirBtn = box.addButton(tr("打开输出目录"), QMessageBox::ActionRole);
        openDirBtn->setProperty("dir", outDir);
    }
    box.addButton(QMessageBox::Ok);
    box.exec();
    if (openDirBtn && box.clickedButton() == openDirBtn) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(openDirBtn->property("dir").toString()));
    }
}
