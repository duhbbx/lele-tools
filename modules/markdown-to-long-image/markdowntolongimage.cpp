#include "markdowntolongimage.h"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QScrollBar>
#include <QDateTime>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QTextBlock>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <cmath>

REGISTER_DYNAMICOBJECT(MarkdownToLongImage);

namespace {

struct Theme {
    QString name;
    QColor bg;         // 整图背景
    QColor text;       // 正文文字
    QColor heading;    // 标题文字
    QColor accent;     // 链接 / 强调
    QColor codeBg;     // 代码背景
    QColor codeText;   // 代码文字
    QColor quoteBg;    // 引用块背景
    QColor quoteBar;   // 引用块左侧条
    QColor border;     // 分隔线 / 边框
    QString family;    // 字体家族（含 CJK 后备）
    bool isDark;
};

const Theme& themeAt(int idx)
{
    static const Theme themes[] = {
        // 0 — 简洁白
        {
            "简洁白",
            QColor(0xff, 0xff, 0xff),
            QColor(0x21, 0x29, 0x33),
            QColor(0x12, 0x18, 0x21),
            QColor(0x1c, 0x7e, 0xd6),
            QColor(0xf5, 0xf7, 0xfa),
            QColor(0x37, 0x42, 0x51),
            QColor(0xf1, 0xf5, 0xfb),
            QColor(0x1c, 0x7e, 0xd6),
            QColor(0xe2, 0xe8, 0xf0),
            "PingFang SC, Hiragino Sans GB, Microsoft YaHei, Noto Sans CJK SC, sans-serif",
            false
        },
        // 1 — 暖纸张
        {
            "暖纸张",
            QColor(0xfa, 0xf6, 0xee),
            QColor(0x3a, 0x33, 0x29),
            QColor(0x2a, 0x22, 0x18),
            QColor(0xb1, 0x6a, 0x1a),
            QColor(0xf2, 0xea, 0xd8),
            QColor(0x4a, 0x3f, 0x2b),
            QColor(0xf4, 0xeb, 0xd6),
            QColor(0xb1, 0x6a, 0x1a),
            QColor(0xe6, 0xd9, 0xb8),
            "Songti SC, Source Han Serif SC, Noto Serif CJK SC, serif",
            false
        },
        // 2 — 深色夜读
        {
            "深色夜读",
            QColor(0x1a, 0x1d, 0x21),
            QColor(0xe6, 0xeb, 0xf2),
            QColor(0xff, 0xff, 0xff),
            QColor(0x4d, 0xab, 0xf7),
            QColor(0x24, 0x29, 0x33),
            QColor(0xc0, 0xcb, 0xd8),
            QColor(0x22, 0x29, 0x35),
            QColor(0x4d, 0xab, 0xf7),
            QColor(0x37, 0x3e, 0x4a),
            "PingFang SC, Hiragino Sans GB, Microsoft YaHei, Noto Sans CJK SC, sans-serif",
            true
        },
    };
    constexpr int n = sizeof(themes) / sizeof(themes[0]);
    if (idx < 0 || idx >= n) idx = 0;
    return themes[idx];
}

// 把图压到 maxBytes 以下并返回字节流和实际格式名
// 策略：PNG 全色 → PNG-8（调色板量化）→ JPEG 渐进降质，第一个达标的就用
// 返回值：是否成功压到 ≤ maxBytes（false 时仍把可得到的最小变体写回 outBytes）
bool compressImageUnder(const QImage& src, qint64 maxBytes,
                        QByteArray& outBytes, QString& outExt)
{
    auto encodePng = [&](const QImage& img) -> QByteArray {
        QByteArray b;
        QBuffer buf(&b);
        buf.open(QIODevice::WriteOnly);
        img.save(&buf, "PNG", 100);
        return b;
    };
    auto encodeJpeg = [&](const QImage& img, int q) -> QByteArray {
        QImage rgb = img.hasAlphaChannel() ? img.convertToFormat(QImage::Format_RGB32) : img;
        QByteArray b;
        QBuffer buf(&b);
        buf.open(QIODevice::WriteOnly);
        rgb.save(&buf, "JPEG", q);
        return b;
    };

    QByteArray pngFull = encodePng(src);
    if (pngFull.size() <= maxBytes) {
        outBytes = pngFull; outExt = "png"; return true;
    }

    // PNG-8：256 色调色板，对文本/纯色背景有效
    QImage indexed = src.convertToFormat(QImage::Format_Indexed8,
                                         Qt::PreferDither | Qt::DiffuseAlphaDither);
    QByteArray png8 = encodePng(indexed);
    if (png8.size() <= maxBytes) {
        outBytes = png8; outExt = "png"; return true;
    }

    // JPEG 渐进降质
    QByteArray bestJpeg;
    for (int q : {92, 85, 78, 70, 62, 55}) {
        QByteArray jpg = encodeJpeg(src, q);
        if (jpg.size() <= maxBytes) {
            outBytes = jpg; outExt = "jpg"; return true;
        }
        bestJpeg = jpg;   // 记录最后一档（最小的 JPEG）
    }

    // 全部超限，回最小那个
    QByteArray smallest = png8;
    QString smallestExt = "png";
    if (bestJpeg.size() < smallest.size()) {
        smallest = bestJpeg;
        smallestExt = "jpg";
    }
    if (pngFull.size() < smallest.size()) {
        smallest = pngFull;
        smallestExt = "png";
    }
    outBytes = smallest;
    outExt = smallestExt;
    return false;
}

QString formatBytes(qint64 b)
{
    if (b < 1024) return QString("%1 B").arg(b);
    if (b < 1024 * 1024) return QString("%1 KB").arg(b / 1024.0, 0, 'f', 1);
    return QString("%1 MB").arg(b / 1024.0 / 1024.0, 0, 'f', 2);
}

// 按指定层级（H1 / H2 等）切片：每个切片从匹配的标题行开始，到下一个同级标题前结束。
// level=1 → "# "；level=2 → "## "。代码块（``` 内）的 # 行被忽略。
// 第一个匹配标题之前的内容（若有）作为独立切片 "(前言)"。
struct MdSlice { QString title; QString content; };

QList<MdSlice> sliceByLevel(const QString& md, int level)
{
    if (level < 1) level = 1;
    if (level > 6) level = 6;
    const QString prefix = QString(level, QChar('#')) + " ";          // "# " / "## " ...
    const QString deeper = QString(level + 1, QChar('#'));            // "##" / "###" ... 用于排除更深层级

    QList<MdSlice> result;
    QStringList lines = md.split('\n');
    bool inFence = false;
    MdSlice cur;
    auto pushIfNonEmpty = [&]() {
        if (!cur.content.trimmed().isEmpty()) result.append(cur);
        cur = MdSlice();
    };
    for (const QString& line : lines) {
        QString t = line.trimmed();
        if (t.startsWith("```") || t.startsWith("~~~")) inFence = !inFence;
        bool isTarget = !inFence
                        && t.startsWith(prefix)
                        && !t.startsWith(deeper);
        if (isTarget) {
            pushIfNonEmpty();
            cur.title = t.mid(prefix.size()).trimmed();
        }
        if (!cur.content.isEmpty()) cur.content += '\n';
        cur.content += line;
    }
    pushIfNonEmpty();
    return result;
}

// 文件名安全化：去掉路径分隔符 / 控制字符等
QString sanitizeFileName(const QString& s)
{
    static const QString invalid = QStringLiteral("/\\:*?\"<>|");
    QString out = s.trimmed();
    for (QChar c : invalid) out.replace(c, '_');
    out.replace('\n', '_');
    out.replace('\t', '_');
    out = out.trimmed();
    if (out.length() > 60) out = out.left(60);
    if (out.isEmpty()) out = QStringLiteral("untitled");
    return out;
}

// CommonMark 里单换行被视为软换行 → 跟下一行合并成同一段（"多行变一行"）。
// 大多数人写 MD 时是按 GitHub/Discourse 的"软换行=硬换行"心智模型，所以这里
// 在渲染前把非特殊行尾巴塞两个空格（CommonMark 硬换行语法），让单换行真的换行。
// 跳过代码块（``` 内）和表格行（| 起头），它们有自己的语法不能被破坏。
QString softBreaksToHard(const QString& src)
{
    QStringList lines = src.split('\n');
    bool inFence = false;
    for (int i = 0; i < lines.size(); ++i) {
        QString trimmed = lines[i].trimmed();
        if (trimmed.startsWith("```") || trimmed.startsWith("~~~")) {
            inFence = !inFence;
            continue;
        }
        if (inFence) continue;
        if (trimmed.isEmpty()) continue;
        if (trimmed.startsWith('|')) continue;          // 表格行
        if (trimmed.startsWith("---") || trimmed.startsWith("===")) continue;

        if (i + 1 >= lines.size()) continue;
        QString next = lines[i + 1].trimmed();
        if (next.isEmpty()) continue;                   // 下行为空 → 已经是段落分隔
        if (next.startsWith('|')) continue;
        if (next.startsWith("---") || next.startsWith("===")) continue;

        // 添加硬换行：去掉尾部已有空格，再加两个空格
        while (lines[i].endsWith(' ')) lines[i].chop(1);
        lines[i] += "  ";
    }
    return lines.join('\n');
}

const char* kDefaultMd = R"(# Markdown 长图渲染示例

这是一段说明文字，展示 **加粗**、*斜体*、`行内代码`、[链接](https://example.com) 等基本样式。

## 二级标题

支持的元素：

- 无序列表项
- 列表项中也支持 **加粗**
  - 嵌套子项

1. 有序列表项 1
2. 有序列表项 2

> 引用块：用于强调一段话。
> 多行引用也可以连写。

### 代码块

```python
def hello(name: str) -> None:
    print(f"Hello, {name}!")
```

| 列 1 | 列 2 | 列 3 |
|------|------|------|
| a    | b    | c    |
| d    | e    | f    |

---

切换主题、调整宽度、添加水印，全部右侧实时预览。
)";

}  // namespace


MarkdownToLongImage::MarkdownToLongImage() : QWidget(nullptr), DynamicObjectBase()
{
    setupUI();
    m_editor->setPlainText(QString::fromUtf8(kDefaultMd));
    onRender();
}

MarkdownToLongImage::~MarkdownToLongImage() = default;

void MarkdownToLongImage::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    if (!m_currentImage.isNull()) refreshPreview();
}

void MarkdownToLongImage::setupUI()
{
    setStyleSheet(R"(
        QPlainTextEdit, QLineEdit, QSpinBox, QComboBox {
            border: 1px solid #ced4da; border-radius: 4px;
            padding: 5px 8px; background: #fff; min-height: 22px;
            selection-background-color: #b8d4ff;
        }
        QPlainTextEdit { font-family: 'SF Mono','Consolas',monospace; font-size: 10pt; }
        QSpinBox:focus, QComboBox:focus, QLineEdit:focus,
        QPlainTextEdit:focus { border-color: #228be6; }
        QPushButton {
            border: 1px solid #ced4da; border-radius: 4px;
            padding: 5px 14px; background: #ffffff; min-height: 22px; color: #343a40;
        }
        QPushButton:hover  { background: #f1f3f5; border-color: #adb5bd; }
        QPushButton:pressed{ background: #e9ecef; }
        QPushButton#primaryBtn {
            background: #228be6; border: 1px solid #1c7ed6; color: white; font-weight: bold;
        }
        QPushButton#primaryBtn:hover { background: #1c7ed6; }
        QScrollArea { border: 1px solid #dee2e6; border-radius: 4px; background: #e9ecef; }
        QLabel { background: transparent; color: #495057; }
    )");

    auto* main = new QVBoxLayout(this);
    main->setContentsMargins(10, 8, 10, 8);
    main->setSpacing(8);

    // ── 顶部工具栏 1：宽度 / 主题 / 操作
    auto* row1 = new QHBoxLayout();
    row1->setSpacing(6);
    row1->addWidget(new QLabel(tr("宽度:")));
    m_widthCombo = new QComboBox();
    m_widthCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_widthCombo->setMinimumWidth(96);
    m_widthCombo->addItem("640 px",  640);
    m_widthCombo->addItem("750 px",  750);
    m_widthCombo->addItem("900 px",  900);
    m_widthCombo->addItem("1080 px", 1080);
    m_widthCombo->setCurrentIndex(1);
    row1->addWidget(m_widthCombo);

    row1->addSpacing(12);
    row1->addWidget(new QLabel(tr("倍数:")));
    m_scaleCombo = new QComboBox();
    m_scaleCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_scaleCombo->setMinimumWidth(80);
    m_scaleCombo->addItem("@1x", 1);
    m_scaleCombo->addItem("@2x", 2);
    m_scaleCombo->addItem("@3x", 3);
    m_scaleCombo->setCurrentIndex(1);   // 默认 @2x，Retina / 微信不再糊
    m_scaleCombo->setToolTip(tr(
        "渲染倍数。文字按宽度排版，但实际输出像素 × 倍数：\n"
        "@1x  紧凑但 Retina 显示器看会糊\n"
        "@2x  推荐，Retina / 微信发图清晰\n"
        "@3x  超高分辨率，文件最大"));
    row1->addWidget(m_scaleCombo);

    row1->addSpacing(12);
    row1->addWidget(new QLabel(tr("主题:")));
    m_themeCombo = new QComboBox();
    m_themeCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_themeCombo->setMinimumWidth(120);
    m_themeCombo->addItem(themeAt(0).name);
    m_themeCombo->addItem(themeAt(1).name);
    m_themeCombo->addItem(themeAt(2).name);
    m_themeCombo->setCurrentIndex(0);
    row1->addWidget(m_themeCombo);

    row1->addStretch();
    m_sizeLabel = new QLabel();
    m_sizeLabel->setStyleSheet("color:#868e96; font-size:9pt;");
    row1->addWidget(m_sizeLabel);
    m_copyBtn   = new QPushButton(tr("📋 复制图片"));

    m_sliceLevelCombo = new QComboBox();
    m_sliceLevelCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_sliceLevelCombo->setMinimumWidth(90);
    m_sliceLevelCombo->addItem(tr("按 H1 切"), 1);
    m_sliceLevelCombo->addItem(tr("按 H2 切"), 2);
    m_sliceLevelCombo->setCurrentIndex(0);
    m_sliceLevelCombo->setToolTip(tr("选择切片的标题层级 — H1 即一级 (# )，H2 即二级 (## )"));

    m_sliceBtn  = new QPushButton(tr("✂ 切片导出"));
    m_sliceBtn->setToolTip(tr("按所选层级把整篇切成多张图分别导出，每张独立压到 ≤ 2MB"));

    m_exportBtn = new QPushButton(tr("💾 导出 PNG"));
    m_exportBtn->setObjectName("primaryBtn");

    row1->addWidget(m_copyBtn);
    row1->addWidget(m_sliceLevelCombo);
    row1->addWidget(m_sliceBtn);
    row1->addWidget(m_exportBtn);
    main->addLayout(row1);

    // ── 顶部工具栏 2：水印
    auto* row2 = new QHBoxLayout();
    row2->setSpacing(6);
    row2->addWidget(new QLabel(tr("水印:")));
    m_watermarkEdit = new QLineEdit();
    m_watermarkEdit->setPlaceholderText(tr("留空则无水印"));
    m_watermarkEdit->setMinimumWidth(180);
    row2->addWidget(m_watermarkEdit, 1);

    row2->addWidget(new QLabel(tr("不透明度:")));
    m_watermarkOpacity = new QSpinBox();
    m_watermarkOpacity->setRange(0, 100);
    m_watermarkOpacity->setValue(30);
    m_watermarkOpacity->setSuffix(" %");
    m_watermarkOpacity->setFixedWidth(80);
    row2->addWidget(m_watermarkOpacity);

    row2->addWidget(new QLabel(tr("位置:")));
    m_watermarkPosCombo = new QComboBox();
    m_watermarkPosCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_watermarkPosCombo->setMinimumWidth(110);
    m_watermarkPosCombo->addItem(tr("斜向铺满"));
    m_watermarkPosCombo->addItem(tr("右下角"));
    m_watermarkPosCombo->addItem(tr("底部居中"));
    m_watermarkPosCombo->setCurrentIndex(0);
    row2->addWidget(m_watermarkPosCombo);

    row2->addSpacing(16);
    m_syncScrollCheck = new QCheckBox(tr("滚动联动"));
    m_syncScrollCheck->setChecked(true);
    m_syncScrollCheck->setToolTip(tr("勾选后左右两边滚动按比例同步"));
    row2->addWidget(m_syncScrollCheck);
    main->addLayout(row2);

    // ── 主区域：左编辑，右预览
    auto* split = new QSplitter(Qt::Horizontal);

    m_editor = new QPlainTextEdit();
    m_editor->setPlaceholderText(tr("在这里输入 Markdown…"));
    split->addWidget(m_editor);

    m_previewScroll = new QScrollArea();
    m_previewScroll->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    m_previewScroll->setWidgetResizable(false);
    m_previewLabel = new QLabel();
    m_previewLabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    m_previewLabel->setStyleSheet("background: transparent;");
    m_previewScroll->setWidget(m_previewLabel);
    split->addWidget(m_previewScroll);

    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 1);
    split->setSizes({500, 600});
    main->addWidget(split, 1);

    // 拖动 splitter 时即时刷新预览，让图像跟着面板宽度铺满
    connect(split, &QSplitter::splitterMoved, this,
            [this](int, int){ refreshPreview(); });

    // ── 防抖
    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(300);
    connect(m_debounce, &QTimer::timeout, this, &MarkdownToLongImage::onRender);

    // ── 信号
    connect(m_editor, &QPlainTextEdit::textChanged, this, &MarkdownToLongImage::scheduleRender);
    connect(m_widthCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int){ scheduleRender(); });
    connect(m_scaleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int){ scheduleRender(); });
    connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int){ scheduleRender(); });
    connect(m_watermarkEdit, &QLineEdit::textChanged,
            this, [this](const QString&){ scheduleRender(); });
    connect(m_watermarkOpacity, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int){ scheduleRender(); });
    connect(m_watermarkPosCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int){ scheduleRender(); });
    connect(m_copyBtn,   &QPushButton::clicked, this, &MarkdownToLongImage::onCopyImage);
    connect(m_exportBtn, &QPushButton::clicked, this, &MarkdownToLongImage::onExportPng);
    connect(m_sliceBtn,  &QPushButton::clicked, this, &MarkdownToLongImage::onSliceExport);

    // ── 左右滚动联动（按比例 from->maximum() → to->maximum()）
    QScrollBar* editorBar  = m_editor->verticalScrollBar();
    QScrollBar* previewBar = m_previewScroll->verticalScrollBar();
    auto syncFromTo = [this](QScrollBar* from, QScrollBar* to) {
        if (!m_syncScrollCheck->isChecked()) return;
        if (m_isSyncing) return;
        if (from->maximum() <= 0) return;
        const double ratio = double(from->value()) / from->maximum();
        m_isSyncing = true;
        to->setValue(int(ratio * to->maximum() + 0.5));
        m_isSyncing = false;
    };
    connect(editorBar,  &QScrollBar::valueChanged, this,
            [editorBar, previewBar, syncFromTo](int){ syncFromTo(editorBar, previewBar); });
    connect(previewBar, &QScrollBar::valueChanged, this,
            [editorBar, previewBar, syncFromTo](int){ syncFromTo(previewBar, editorBar); });
}

void MarkdownToLongImage::scheduleRender()
{
    m_debounce->start();
}

QString MarkdownToLongImage::buildCss(int themeIndex) const
{
    const Theme& t = themeAt(themeIndex);
    auto col = [](const QColor& c) { return c.name(QColor::HexRgb); };

    // 注：QTextDocument 的 default stylesheet 只支持有限 CSS 子集，
    // 不要写 border-radius、box-shadow 等高级属性，会被忽略；
    // 但 background、color、margin、padding、border、font-* 都 OK。
    QString css;
    css += QString("body { color: %1; font-family: %2; font-size: 14pt; line-height: 220%; }\n")
               .arg(col(t.text), t.family);
    // 用 padding-bottom 控制"标题→下一段"的距离（margin 在 Qt CSS 里可能被折叠）；
    // padding 不参与折叠，所以是最稳的"留白"手段
    // h1 有 border-bottom，border 阻止 margin 折叠，所以 margin-bottom 直接生效
    // 不会跟下一段的 margin-top 合并。同时保留 padding-bottom 控制文字到 border 的距离
    css += QString("h1 { color: %1; font-size: 26pt; margin-top: 40px; margin-bottom: 36px; "
                   "padding-bottom: 16px; padding-top: 4px; "
                   "border-bottom: 1px solid %2; }\n")
               .arg(col(t.heading), col(t.border));
    css += QString("h2 { color: %1; font-size: 21pt; margin-top: 34px; margin-bottom: 0; "
                   "padding-bottom: 22px; padding-top: 4px; }\n").arg(col(t.heading));
    css += QString("h3 { color: %1; font-size: 17pt; margin-top: 30px; margin-bottom: 0; "
                   "padding-bottom: 18px; padding-top: 2px; }\n").arg(col(t.heading));
    css += QString("h4 { color: %1; font-size: 15pt; margin-top: 26px; margin-bottom: 0; "
                   "padding-bottom: 14px; }\n").arg(col(t.heading));
    // 段落自身也要点呼吸：上下 16px
    css += QString("p { margin: 16px 0; }\n");
    css += QString("a { color: %1; text-decoration: underline; }\n").arg(col(t.accent));
    css += QString("strong, b { color: %1; }\n").arg(col(t.heading));
    css += QString("em, i { color: %1; }\n").arg(col(t.text));

    // 行内代码
    css += QString("code { background: %1; color: %2; padding: 1px 6px; border-radius: 3px; "
                   "font-family: 'SF Mono','Consolas',monospace; font-size: 13pt; }\n")
               .arg(col(t.codeBg), col(t.codeText));
    // 代码块
    css += QString("pre { background: %1; color: %2; padding: 12px 14px; "
                   "font-family: 'SF Mono','Consolas',monospace; font-size: 13pt; "
                   "border: 1px solid %3; }\n")
               .arg(col(t.codeBg), col(t.codeText), col(t.border));

    css += QString("blockquote { background-color: %1; border-left: 6px solid %2; "
                   "padding: 14px 22px; margin: 20px 0; color: %3; }\n")
               .arg(col(t.quoteBg), col(t.quoteBar), col(t.text));
    // 引用块内部段落收紧，避免多行 blockquote 看着"散"
    css += QString("blockquote p { margin: 4px 0; }\n");

    css += QString("hr { border: none; border-top: 1px solid %1; margin: 20px 0; }\n")
               .arg(col(t.border));

    // 表格
    css += QString("table { border-collapse: collapse; margin: 12px 0; }\n");
    css += QString("th { background: %1; color: %2; border: 1px solid %3; padding: 8px 12px; }\n")
               .arg(col(t.codeBg), col(t.heading), col(t.border));
    css += QString("td { border: 1px solid %1; padding: 8px 12px; }\n").arg(col(t.border));

    // 列表
    css += QString("ul, ol { margin: 8px 0; padding-left: 24px; }\n");
    css += QString("li { margin: 4px 0; }\n");

    return css;
}

QImage MarkdownToLongImage::renderToImage() const
{
    return renderMarkdownToImage(m_editor->toPlainText());
}

QImage MarkdownToLongImage::renderMarkdownToImage(const QString& markdownText) const
{
    const int logicalWidth = m_widthCombo->currentData().toInt();
    const int scale = qMax(1, m_scaleCombo->currentData().toInt());
    const int themeIdx = m_themeCombo->currentIndex();
    const Theme& t = themeAt(themeIdx);
    const int padding = 56;   // 逻辑像素

    QTextDocument doc;
    doc.setDocumentMargin(0);     // 自己管 padding，避免 doc 自带 margin 让高度算偏小
    QFont docFont;
    QStringList families = t.family.split(',', Qt::SkipEmptyParts);
    for (QString& f : families) f = f.trimmed().remove('\'').remove('"');
    docFont.setFamilies(families);
    docFont.setPointSize(14);
    doc.setDefaultFont(docFont);
    doc.setDefaultStyleSheet(buildCss(themeIdx));
    doc.setMarkdown(softBreaksToHard(markdownText));
    doc.setTextWidth(logicalWidth - padding * 2);

    // 关键：documentLayout()->documentSize() 在某些版本/内容下会算小一两行；
    // 用最后一个 block 的 boundingRect.bottom() 取实际渲染到的最低 Y，再加 buffer
    qreal contentBottom = doc.documentLayout()->documentSize().height();
    {
        QTextBlock last = doc.lastBlock();
        if (last.isValid()) {
            QRectF br = doc.documentLayout()->blockBoundingRect(last);
            contentBottom = qMax(contentBottom, br.bottom());
        }
    }
    int logicalHeight = int(std::ceil(contentBottom)) + padding * 2 + 16;  // +16 安全 buffer
    if (logicalHeight < 200) logicalHeight = 200;

    // 物理像素数 = 逻辑 × scale。painter.scale 把逻辑坐标映射到物理像素。
    // 不 setDevicePixelRatio：避免 painter 的 device 矩形按逻辑算导致剪裁。
    qint64 physW64 = qint64(logicalWidth) * scale;
    qint64 physH64 = qint64(logicalHeight) * scale;
    // QImage 极端高度时分配会失败，限到 32760（QPainter 内部 16-bit 坐标安全线）
    int actualScale = scale;
    while (actualScale > 1 && (physW64 > 32000 || physH64 > 32000)) {
        actualScale--;
        physW64 = qint64(logicalWidth) * actualScale;
        physH64 = qint64(logicalHeight) * actualScale;
    }
    const int physW = int(physW64);
    const int physH = int(physH64);

    QImage img(QSize(physW, physH), QImage::Format_ARGB32_Premultiplied);
    if (img.isNull()) {
        // 分配失败时回退到 @1x
        img = QImage(QSize(logicalWidth, logicalHeight), QImage::Format_ARGB32_Premultiplied);
        if (img.isNull()) return QImage();
    }
    img.fill(t.bg);

    QPainter painter(&img);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    if (actualScale > 1) painter.scale(actualScale, actualScale);

    painter.save();
    painter.translate(padding, padding);
    QAbstractTextDocumentLayout::PaintContext ctx;
    ctx.palette.setColor(QPalette::Text, t.text);
    ctx.palette.setColor(QPalette::Link, t.accent);
    // 显式给 clip：覆盖整个内容区域，确保最后一行不被默认 clip 切掉
    ctx.clip = QRectF(0, 0,
                      logicalWidth - padding * 2,
                      logicalHeight);   // 留足够空间到底部
    doc.documentLayout()->draw(&painter, ctx);
    painter.restore();

    // 水印按逻辑像素绘制；painter.scale 仍然生效
    applyWatermark(painter, QSize(logicalWidth, logicalHeight));

    return img;
}

void MarkdownToLongImage::applyWatermark(QPainter& painter, const QSize& imgSize) const
{
    const QString text = m_watermarkEdit->text().trimmed();
    if (text.isEmpty()) return;
    int opacityPct = m_watermarkOpacity->value();
    if (opacityPct <= 0) return;

    const int themeIdx = m_themeCombo->currentIndex();
    const Theme& t = themeAt(themeIdx);

    painter.save();
    painter.setOpacity(opacityPct / 100.0);

    QFont wmFont = painter.font();
    wmFont.setPointSize(16);
    wmFont.setBold(false);
    painter.setFont(wmFont);
    painter.setPen(t.isDark ? QColor(255, 255, 255) : QColor(80, 80, 80));

    const int posMode = m_watermarkPosCombo->currentIndex();
    QFontMetrics fm(wmFont);
    const int textW = fm.horizontalAdvance(text);
    const int textH = fm.height();

    if (posMode == 0) {
        // 斜向铺满：旋转 -30° 后用 tile 网格绘制；网格在旋转坐标系里要覆盖
        // 整图对角线长度的矩形。
        painter.translate(imgSize.width() / 2.0, imgSize.height() / 2.0);
        painter.rotate(-30);
        const int diag = int(std::hypot(imgSize.width(), imgSize.height())) + 200;
        const int tileX = textW + 100;
        const int tileY = 140;
        for (int y = -diag; y < diag; y += tileY) {
            // 错位
            int offset = ((y / tileY) % 2 == 0) ? 0 : tileX / 2;
            for (int x = -diag + offset; x < diag; x += tileX) {
                painter.drawText(x, y, text);
            }
        }
    } else if (posMode == 1) {
        // 右下角
        int x = imgSize.width() - textW - 24;
        int y = imgSize.height() - 24;
        painter.drawText(x, y, text);
    } else {
        // 底部居中
        int x = (imgSize.width() - textW) / 2;
        int y = imgSize.height() - 24;
        painter.drawText(x, y, text);
    }

    painter.restore();
}

void MarkdownToLongImage::onRender()
{
    m_currentImage = renderToImage();
    refreshPreview();
    if (!m_currentImage.isNull()) {
        m_sizeLabel->setText(tr("尺寸 %1 × %2")
                                 .arg(m_currentImage.width())
                                 .arg(m_currentImage.height()));
    }
}

void MarkdownToLongImage::refreshPreview()
{
    if (m_currentImage.isNull()) return;

    // HiDPI 清晰：pixmap 按 *物理像素* 缩放，再 setDevicePixelRatio
    qreal dpr = devicePixelRatioF();
    if (dpr <= 0) dpr = 1.0;

    int availLogical = m_previewScroll->viewport()->width() - 24;
    if (availLogical < 200) availLogical = 200;
    const int targetPhys = int(availLogical * dpr + 0.5);

    // 始终缩到面板宽度（上/下采样都做），让预览铺满；
    // 导出的 PNG 仍按用户选定宽度，不受预览影响
    QImage scaled = m_currentImage.scaledToWidth(targetPhys, Qt::SmoothTransformation);
    QPixmap pm = QPixmap::fromImage(scaled);
    pm.setDevicePixelRatio(dpr);

    m_previewLabel->setPixmap(pm);
    m_previewLabel->resize(QSize(int(pm.width()  / dpr + 0.5),
                                 int(pm.height() / dpr + 0.5)));
}

void MarkdownToLongImage::onCopyImage()
{
    if (m_currentImage.isNull()) onRender();
    if (m_currentImage.isNull()) return;
    QApplication::clipboard()->setImage(m_currentImage);
    m_sizeLabel->setText(tr("✅ 已复制到剪贴板 · 尺寸 %1 × %2")
                             .arg(m_currentImage.width())
                             .arg(m_currentImage.height()));
}

void MarkdownToLongImage::onExportPng()
{
    if (m_currentImage.isNull()) onRender();
    if (m_currentImage.isNull()) {
        QMessageBox::warning(this, tr("无内容"), tr("当前没有可导出的图。"));
        return;
    }
    QString defName = QString("markdown_%1.png")
                          .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    QString defDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    QString path = QFileDialog::getSaveFileName(
        this, tr("导出长图"),
        defDir + "/" + defName,
        tr("图片 (*.png *.jpg)"));
    if (path.isEmpty()) return;

    // 自动压到 ≤ 2MB；超过则尝试 PNG-8 / JPEG 降质
    const qint64 LIMIT = 2 * 1024 * 1024;
    QByteArray bytes;
    QString ext;
    bool underLimit = compressImageUnder(m_currentImage, LIMIT, bytes, ext);

    // 校正路径扩展名
    QFileInfo fi(path);
    if (fi.suffix().toLower() != ext) {
        path = fi.absolutePath() + "/" + fi.completeBaseName() + "." + ext;
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("保存失败"), tr("无法写入：%1").arg(path));
        return;
    }
    f.write(bytes);
    f.close();

    QString sizeNote = underLimit
        ? tr("体积 %1（%2）").arg(formatBytes(bytes.size()), ext.toUpper())
        : tr("体积 %1（%2）— 已尽力压缩但仍超 2MB")
              .arg(formatBytes(bytes.size()), ext.toUpper());

    QMessageBox box(this);
    box.setWindowTitle(tr("导出完成"));
    box.setText(tr("长图已保存：\n%1\n\n尺寸 %2 × %3\n%4")
                    .arg(path)
                    .arg(m_currentImage.width())
                    .arg(m_currentImage.height())
                    .arg(sizeNote));
    box.setIcon(QMessageBox::Information);
    auto* openBtn = box.addButton(tr("打开图片"), QMessageBox::ActionRole);
    auto* dirBtn  = box.addButton(tr("打开所在目录"), QMessageBox::ActionRole);
    box.addButton(QMessageBox::Ok);
    box.exec();
    if (box.clickedButton() == openBtn)
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    else if (box.clickedButton() == dirBtn)
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
}

void MarkdownToLongImage::onSliceExport()
{
    int level = m_sliceLevelCombo->currentData().toInt();
    if (level < 1) level = 1;
    const QString levelTag = QString("H%1").arg(level);

    QList<MdSlice> slices = sliceByLevel(m_editor->toPlainText(), level);
    if (slices.isEmpty()) {
        QMessageBox::warning(this, tr("无内容"), tr("当前没有可切片的内容。"));
        return;
    }
    if (slices.size() == 1) {
        if (QMessageBox::question(this, tr("仅一个切片"),
                tr("整篇文档按 %1 切片只会得到一张图。\n继续？").arg(levelTag),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
    }

    QString defDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    QString dir = QFileDialog::getExistingDirectory(this, tr("选择切片导出目录"), defDir);
    if (dir.isEmpty()) return;

    QString baseName = QString("markdown_%1")
                           .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    const qint64 LIMIT = 2 * 1024 * 1024;

    int totalSaved = 0;
    int totalOverLimit = 0;
    qint64 totalBytes = 0;
    QStringList savedPaths;

    for (int i = 0; i < slices.size(); ++i) {
        const MdSlice& s = slices[i];
        QImage img = renderMarkdownToImage(s.content);
        if (img.isNull()) continue;

        QByteArray bytes;
        QString ext;
        bool under = compressImageUnder(img, LIMIT, bytes, ext);
        if (!under) totalOverLimit++;

        QString safeTitle = sanitizeFileName(s.title);
        QString fname = QString("%1_%2_%3.%4")
                            .arg(baseName)
                            .arg(i + 1, 2, 10, QChar('0'))
                            .arg(safeTitle)
                            .arg(ext);
        QString fullPath = dir + "/" + fname;
        QFile f(fullPath);
        if (!f.open(QIODevice::WriteOnly)) continue;
        f.write(bytes);
        f.close();
        totalSaved++;
        totalBytes += bytes.size();
        savedPaths << fullPath;
    }

    if (totalSaved == 0) {
        QMessageBox::warning(this, tr("失败"), tr("一张都没写成功。"));
        return;
    }

    QString summary = tr("已保存 %1 张切片到：\n%2\n\n总体积 %3")
                          .arg(totalSaved).arg(dir).arg(formatBytes(totalBytes));
    if (totalOverLimit > 0) {
        summary += "\n" + tr("（其中 %1 张已尽力压缩但仍超 2MB）").arg(totalOverLimit);
    }

    QMessageBox box(this);
    box.setWindowTitle(tr("切片导出完成"));
    box.setText(summary);
    box.setIcon(QMessageBox::Information);
    auto* dirBtn = box.addButton(tr("打开所在目录"), QMessageBox::ActionRole);
    box.addButton(QMessageBox::Ok);
    box.exec();
    if (box.clickedButton() == dirBtn)
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}
