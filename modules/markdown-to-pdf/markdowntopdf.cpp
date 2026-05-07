#include "markdowntopdf.h"

#include "pdfoutlineinjector.h"

#include <QAbstractItemView>
#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QFontMetricsF>
#include <QFrame>
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
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPrinter>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

REGISTER_DYNAMICOBJECT(MarkdownToPdf);

namespace {

// 几个内置主题，每个返回一段 CSS。base 为正文字号（pt）。
QString cssGitHubLight(int base)
{
    return QString(R"(
        body { font-family: -apple-system, "PingFang SC", "Microsoft YaHei",
                            "Segoe UI", sans-serif;
               font-size: %1pt; line-height: 1.6; color: #212529; }
        h1 { font-size: %2pt; border-bottom: 2px solid #dee2e6;
             padding-bottom: 6px; margin-top: 18px; color: #1a1e22; }
        h2 { font-size: %3pt; border-bottom: 1px solid #e9ecef;
             padding-bottom: 4px; margin-top: 14px; color: #1a1e22; }
        h3 { font-size: %4pt; margin-top: 12px; }
        h4, h5, h6 { font-size: %5pt; }
        p { margin: 8px 0; }
        a { color: #1971c2; text-decoration: none; }
        code { font-family: "Menlo", "Consolas", monospace; font-size: %6pt;
               background: #f1f3f5; padding: 2px 4px; border-radius: 3px; }
        pre { background: #f8f9fa; border: 1px solid #e9ecef;
              border-radius: 4px; padding: 8px; }
        pre code { background: transparent; padding: 0; }
        blockquote { border-left: 4px solid #ced4da; color: #495057;
                     margin: 8px 0; padding: 0 12px; }
        table { border-collapse: collapse; margin: 8px 0; }
        th, td { border: 1px solid #ced4da; padding: 6px 10px; }
        th { background: #f1f3f5; font-weight: bold; }
        ul, ol { margin: 6px 0 6px 20px; }
        li { margin: 2px 0; }
        hr { border: none; border-top: 1px solid #dee2e6; margin: 14px 0; }
    )")
    .arg(base)
    .arg(int(base * 1.8))
    .arg(int(base * 1.5))
    .arg(int(base * 1.25))
    .arg(int(base * 1.1))
    .arg(int(base * 0.95));
}

QString cssGitHubDark(int base)
{
    return QString(R"(
        body { font-family: -apple-system, "PingFang SC", "Microsoft YaHei",
                            "Segoe UI", sans-serif;
               font-size: %1pt; line-height: 1.6;
               background: #0d1117; color: #c9d1d9; }
        h1 { font-size: %2pt; border-bottom: 2px solid #21262d;
             padding-bottom: 6px; margin-top: 18px; color: #f0f6fc; }
        h2 { font-size: %3pt; border-bottom: 1px solid #21262d;
             padding-bottom: 4px; margin-top: 14px; color: #f0f6fc; }
        h3 { font-size: %4pt; margin-top: 12px; color: #f0f6fc; }
        h4, h5, h6 { font-size: %5pt; color: #f0f6fc; }
        p { margin: 8px 0; }
        a { color: #58a6ff; text-decoration: none; }
        code { font-family: "Menlo", "Consolas", monospace; font-size: %6pt;
               background: #161b22; padding: 2px 4px; border-radius: 3px;
               color: #f0883e; }
        pre { background: #161b22; border: 1px solid #30363d;
              border-radius: 4px; padding: 8px; color: #c9d1d9; }
        pre code { background: transparent; padding: 0; color: #c9d1d9; }
        blockquote { border-left: 4px solid #30363d; color: #8b949e;
                     margin: 8px 0; padding: 0 12px; }
        table { border-collapse: collapse; margin: 8px 0; }
        th, td { border: 1px solid #30363d; padding: 6px 10px; }
        th { background: #161b22; color: #f0f6fc; font-weight: bold; }
        ul, ol { margin: 6px 0 6px 20px; }
        li { margin: 2px 0; }
        hr { border: none; border-top: 1px solid #30363d; margin: 14px 0; }
    )")
    .arg(base).arg(int(base * 1.8)).arg(int(base * 1.5))
    .arg(int(base * 1.25)).arg(int(base * 1.1)).arg(int(base * 0.95));
}

QString cssAcademic(int base)
{
    return QString(R"(
        body { font-family: "Times New Roman", "Songti SC", "SimSun", serif;
               font-size: %1pt; line-height: 1.7; color: #1a1a1a; text-align: justify; }
        h1 { font-size: %2pt; text-align: center; margin-top: 18px;
             font-weight: normal; }
        h2 { font-size: %3pt; margin-top: 14px; font-weight: bold;
             font-variant: small-caps; }
        h3 { font-size: %4pt; margin-top: 12px; font-style: italic; }
        h4, h5, h6 { font-size: %5pt; font-style: italic; }
        p { margin: 6px 0; text-indent: 2em; }
        a { color: #1a1a1a; text-decoration: underline; }
        code { font-family: "Courier New", monospace; font-size: %6pt;
               background: #f5f5f5; padding: 1px 3px; }
        pre { background: #f5f5f5; border: 1px solid #ddd; padding: 8px; }
        pre code { background: transparent; padding: 0; }
        blockquote { border-left: 3px solid #888; color: #444;
                     margin: 8px 20px; padding: 0 12px; font-style: italic; }
        table { border-collapse: collapse; margin: 12px auto; }
        th, td { border: 1px solid #999; padding: 6px 12px; }
        th { background: #eee; font-weight: bold; }
        hr { border: none; border-top: 1px solid #888; margin: 18px 0; }
    )")
    .arg(base).arg(int(base * 1.6)).arg(int(base * 1.3))
    .arg(int(base * 1.15)).arg(int(base * 1.05)).arg(int(base * 0.95));
}

QString cssMinimal(int base)
{
    return QString(R"(
        body { font-family: -apple-system, "Helvetica Neue", "PingFang SC",
                            "Microsoft YaHei", sans-serif;
               font-size: %1pt; line-height: 1.65; color: #2c2c2c; }
        h1, h2, h3, h4, h5, h6 { font-weight: 600; margin-top: 16px; color: #111; }
        h1 { font-size: %2pt; }
        h2 { font-size: %3pt; }
        h3 { font-size: %4pt; }
        h4, h5, h6 { font-size: %5pt; }
        p { margin: 6px 0; }
        a { color: #444; text-decoration: underline; }
        code { font-family: "Menlo", monospace; font-size: %6pt;
               background: #f5f5f5; padding: 1px 4px; }
        pre { background: #fafafa; border-left: 3px solid #ccc; padding: 8px; }
        pre code { background: transparent; padding: 0; }
        blockquote { border-left: 2px solid #bbb; color: #555;
                     margin: 8px 0; padding: 0 12px; }
        table { border-collapse: collapse; margin: 8px 0; }
        th, td { border-bottom: 1px solid #ccc; padding: 6px 10px; }
        th { font-weight: 600; }
        hr { border: none; border-top: 1px solid #ddd; margin: 12px 0; }
    )")
    .arg(base).arg(int(base * 1.7)).arg(int(base * 1.4))
    .arg(int(base * 1.2)).arg(int(base * 1.05)).arg(int(base * 0.95));
}

QString cssSolarized(int base)
{
    return QString(R"(
        body { font-family: -apple-system, "PingFang SC", sans-serif;
               font-size: %1pt; line-height: 1.6;
               background: #fdf6e3; color: #586e75; }
        h1 { font-size: %2pt; color: #b58900;
             border-bottom: 2px solid #eee8d5; padding-bottom: 6px; margin-top: 18px; }
        h2 { font-size: %3pt; color: #cb4b16; margin-top: 14px; }
        h3 { font-size: %4pt; color: #2aa198; margin-top: 12px; }
        h4, h5, h6 { font-size: %5pt; color: #859900; }
        p { margin: 8px 0; }
        a { color: #268bd2; text-decoration: none; }
        code { font-family: "Menlo", monospace; font-size: %6pt;
               background: #eee8d5; padding: 2px 4px; border-radius: 3px;
               color: #d33682; }
        pre { background: #eee8d5; border: 1px solid #93a1a1;
              border-radius: 4px; padding: 8px; }
        pre code { background: transparent; padding: 0; color: #586e75; }
        blockquote { border-left: 4px solid #93a1a1; color: #657b83;
                     margin: 8px 0; padding: 0 12px; }
        table { border-collapse: collapse; margin: 8px 0; }
        th, td { border: 1px solid #93a1a1; padding: 6px 10px; }
        th { background: #eee8d5; color: #b58900; font-weight: bold; }
        ul, ol { margin: 6px 0 6px 20px; }
        hr { border: none; border-top: 1px solid #93a1a1; margin: 14px 0; }
    )")
    .arg(base).arg(int(base * 1.8)).arg(int(base * 1.5))
    .arg(int(base * 1.25)).arg(int(base * 1.1)).arg(int(base * 0.95));
}

QString themeStyleSheet(const QString& key, int base)
{
    if (key == "github-dark")  return cssGitHubDark(base);
    if (key == "academic")     return cssAcademic(base);
    if (key == "minimal")      return cssMinimal(base);
    if (key == "solarized")    return cssSolarized(base);
    return cssGitHubLight(base);  // default
}

struct HeadingEntry {
    int level;       // 1..6
    QString text;
    int bodyPage;    // 0-indexed page in body
};

QList<HeadingEntry> collectHeadings(QTextDocument& doc, qreal pageHeightPx, int maxDepth)
{
    QList<HeadingEntry> result;
    auto* layout = doc.documentLayout();
    for (auto block = doc.begin(); block != doc.end(); block = block.next()) {
        int level = block.blockFormat().headingLevel();
        if (level <= 0 || level > maxDepth) continue;
        QString text = block.text();
        if (text.isEmpty()) continue;
        QRectF r = layout->blockBoundingRect(block);
        int page = pageHeightPx > 0 ? int(r.y() / pageHeightPx) : 0;
        result.append({level, text, page});
    }
    return result;
}

QString tocCss(int base)
{
    return QString(R"(
        body { font-family: -apple-system, "PingFang SC", "Microsoft YaHei", sans-serif;
               font-size: %1pt; color: #212529; line-height: 1.6; }
        h1.toc-title { font-size: %2pt; text-align: center;
                       border-bottom: 2px solid #dee2e6;
                       padding-bottom: 8px; margin-bottom: 16px; }
        table.toc { width: 100%%; border-collapse: collapse; }
        table.toc td { border: none; padding: 4px 6px; vertical-align: top; }
        table.toc td.title { }
        table.toc td.page { text-align: right; color: #495057;
                            white-space: nowrap; padding-left: 12px; }
        .lvl1 { font-weight: bold; font-size: %3pt; }
        .lvl2 { padding-left: 16px !important; }
        .lvl3 { padding-left: 32px !important; color: #495057; }
        .lvl4 { padding-left: 48px !important; color: #6c757d; font-size: %4pt; }
        .lvl5, .lvl6 { padding-left: 64px !important; color: #868e96; font-size: %4pt; }
    )")
    .arg(base).arg(int(base * 1.8)).arg(int(base * 1.05)).arg(int(base * 0.9));
}

QString buildTocHtml(const QList<HeadingEntry>& headings, int bodyPageOffset)
{
    QString rows;
    for (const auto& h : headings) {
        int displayPage = h.bodyPage + 1 + bodyPageOffset;
        QString text = h.text.toHtmlEscaped();
        rows += QString("<tr class='lvl%1'>"
                       "<td class='title lvl%1'>%2</td>"
                       "<td class='page'>%3</td></tr>\n")
                .arg(h.level).arg(text).arg(displayPage);
    }
    return QString("<h1 class='toc-title'>目录</h1>\n"
                   "<table class='toc'>%1</table>")
        .arg(rows);
}

void drawDocPage(QPainter* painter, QTextDocument& doc, int pageIdx,
                 const QSizeF& pageSizePx)
{
    painter->save();
    painter->translate(0, -pageIdx * pageSizePx.height());
    QRectF clip(0, pageIdx * pageSizePx.height(),
                pageSizePx.width(), pageSizePx.height());
    painter->setClipRect(clip);
    doc.drawContents(painter, clip);
    painter->restore();
}

} // namespace


MarkdownToPdf::MarkdownToPdf() : QWidget(nullptr), DynamicObjectBase()
{
    setAcceptDrops(true);
    setupUi();
}

void MarkdownToPdf::setupUi()
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
        QComboBox, QSpinBox {
            border: 1px solid #dee2e6;
            border-radius: 4px;
            padding: 2px 6px;
            background: #fff;
        }
    )");

    // ── 视觉竖线分隔，在工具栏不同分组之间用作间隔
    auto makeSeparator = [this]() -> QWidget* {
        auto* sep = new QFrame(this);
        sep->setFrameShape(QFrame::VLine);
        sep->setFrameShadow(QFrame::Plain);
        sep->setStyleSheet("color:#dee2e6;");
        sep->setFixedWidth(1);
        return sep;
    };

    // ===== 第一行：文件操作（左）+ 导出（右）=====
    auto* row1 = new QHBoxLayout();
    row1->setSpacing(4);
    m_openBtn = new QPushButton(tr("打开 .md"));
    m_saveBtn = new QPushButton(tr("保存 .md"));
    row1->addWidget(m_openBtn);
    row1->addWidget(m_saveBtn);
    row1->addStretch();

    m_exportBtn = new QPushButton(tr("导出 PDF"));
    m_exportBtn->setStyleSheet(
        "QPushButton { color: #228be6; font-weight: bold; padding: 5px 16px; }"
        "QPushButton:hover { background-color: #e7f5ff; }");
    row1->addWidget(m_exportBtn);
    main->addLayout(row1);

    // ===== 第二行：所有渲染参数，按"主题/排版/页面/输出"分组并加竖线分隔 =====
    auto* row2 = new QHBoxLayout();
    row2->setSpacing(6);

    // 组 1：主题 + 字号
    row2->addWidget(new QLabel(tr("主题")));
    m_themeCombo = new QComboBox();
    m_themeCombo->addItem(tr("GitHub 浅色"), "github-light");
    m_themeCombo->addItem(tr("GitHub 深色"), "github-dark");
    m_themeCombo->addItem(tr("学术（衬线）"), "academic");
    m_themeCombo->addItem(tr("极简"), "minimal");
    m_themeCombo->addItem(tr("Solarized"), "solarized");
    m_themeCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_themeCombo->setMinimumContentsLength(10);
    m_themeCombo->view()->setMinimumWidth(160);
    row2->addWidget(m_themeCombo);

    row2->addSpacing(8);
    row2->addWidget(new QLabel(tr("字号")));
    m_fontSizeSpin = new QSpinBox();
    m_fontSizeSpin->setRange(8, 24);
    m_fontSizeSpin->setValue(11);
    m_fontSizeSpin->setSuffix(" pt");
    row2->addWidget(m_fontSizeSpin);

    row2->addSpacing(10);
    row2->addWidget(makeSeparator());
    row2->addSpacing(10);

    // 组 2：页面尺寸 / 方向 / 页边距
    row2->addWidget(new QLabel(tr("页面")));
    m_pageSizeCombo = new QComboBox();
    m_pageSizeCombo->addItem("A4", int(QPageSize::A4));
    m_pageSizeCombo->addItem("Letter", int(QPageSize::Letter));
    m_pageSizeCombo->addItem("A3", int(QPageSize::A3));
    m_pageSizeCombo->addItem("A5", int(QPageSize::A5));
    m_pageSizeCombo->addItem("B5", int(QPageSize::B5));
    row2->addWidget(m_pageSizeCombo);

    m_orientationCombo = new QComboBox();
    m_orientationCombo->addItem(tr("纵向"), int(QPageLayout::Portrait));
    m_orientationCombo->addItem(tr("横向"), int(QPageLayout::Landscape));
    row2->addWidget(m_orientationCombo);

    row2->addSpacing(8);
    row2->addWidget(new QLabel(tr("页边距")));
    m_marginSpin = new QSpinBox();
    m_marginSpin->setRange(5, 50);
    m_marginSpin->setValue(20);
    m_marginSpin->setSuffix(" mm");
    row2->addWidget(m_marginSpin);

    row2->addSpacing(10);
    row2->addWidget(makeSeparator());
    row2->addSpacing(10);

    // 组 3：页码 + 目录
    m_pageNumbersCheck = new QCheckBox(tr("页码"));
    m_pageNumbersCheck->setChecked(true);
    row2->addWidget(m_pageNumbersCheck);

    m_tocCheck = new QCheckBox(tr("目录"));
    m_tocCheck->setChecked(false);
    m_tocCheck->setToolTip(tr("生成目录页与 PDF 侧边栏书签"));
    row2->addWidget(m_tocCheck);

    row2->addWidget(new QLabel(tr("深度")));
    m_tocDepthSpin = new QSpinBox();
    m_tocDepthSpin->setRange(1, 6);
    m_tocDepthSpin->setValue(3);
    m_tocDepthSpin->setToolTip(tr("目录最多包含到第几级标题（H1~H6）"));
    row2->addWidget(m_tocDepthSpin);

    row2->addStretch();
    main->addLayout(row2);

    // 分屏：左编辑右预览
    m_splitter = new QSplitter(Qt::Horizontal);
    m_editor = new QPlainTextEdit();
    QFont editorFont("Menlo, Consolas, monospace");
    editorFont.setStyleHint(QFont::Monospace);
    editorFont.setPointSize(11);
    m_editor->setFont(editorFont);
    m_editor->setPlaceholderText(tr("在此输入 Markdown，或拖入 .md 文件..."));
    m_editor->setTabStopDistance(40);
    m_splitter->addWidget(m_editor);

    m_preview = new QTextBrowser();
    m_preview->setOpenExternalLinks(true);
    m_splitter->addWidget(m_preview);
    m_splitter->setSizes({500, 500});
    main->addWidget(m_splitter, 1);

    m_statusLabel = new QLabel(tr("就绪。提示：图片用相对路径时请先\"打开 .md\"以正确解析。"));
    m_statusLabel->setStyleSheet("color:#868e96; font-size:8pt;");
    m_statusLabel->setWordWrap(true);
    main->addWidget(m_statusLabel);

    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(200);

    m_editor->setPlainText(QStringLiteral(
        "# Markdown 转 PDF\n\n"
        "**支持**：标题、列表、*斜体*、`代码`、链接、图片、表格、引用、围栏代码块。\n\n"
        "## 列表\n\n- 第一项\n- 第二项\n- 第三项\n\n"
        "## 表格\n\n| 列 1 | 列 2 |\n|------|------|\n| A    | B    |\n| C    | D    |\n\n"
        "## 代码\n\n```python\ndef hello():\n    print('hello, world')\n```\n\n"
        "> 引用：左侧实时编辑，右侧实时预览，点右上角\"导出 PDF\"。\n"));

    connect(m_openBtn, &QPushButton::clicked, this, &MarkdownToPdf::onOpenMd);
    connect(m_saveBtn, &QPushButton::clicked, this, &MarkdownToPdf::onSaveMd);
    connect(m_exportBtn, &QPushButton::clicked, this, &MarkdownToPdf::onExportPdf);
    connect(m_editor, &QPlainTextEdit::textChanged, this, &MarkdownToPdf::onMarkdownChanged);
    connect(m_debounce, &QTimer::timeout, this, &MarkdownToPdf::renderPreview);
    connect(m_fontSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MarkdownToPdf::renderPreview);
    connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MarkdownToPdf::renderPreview);

    renderPreview();
}

void MarkdownToPdf::onMarkdownChanged()
{
    m_debounce->start();
}

QString MarkdownToPdf::currentStyleSheet() const
{
    return themeStyleSheet(m_themeCombo->currentData().toString(),
                           m_fontSizeSpin->value());
}

void MarkdownToPdf::renderPreview()
{
    QString md = m_editor->toPlainText();
    QTextDocument* doc = m_preview->document();
    doc->setDefaultStyleSheet(currentStyleSheet());
    if (!m_currentMdPath.isEmpty()) {
        doc->setBaseUrl(QUrl::fromLocalFile(QFileInfo(m_currentMdPath).absolutePath() + "/"));
    }
    doc->setMarkdown(md, QTextDocument::MarkdownDialectGitHub);
    // 让深色主题不会被预览的白底盖掉
    QString themeKey = m_themeCombo ? m_themeCombo->currentData().toString() : QString();
    if (themeKey == "github-dark") {
        m_preview->setStyleSheet("QTextBrowser { background:#0d1117; color:#c9d1d9; }");
    } else if (themeKey == "solarized") {
        m_preview->setStyleSheet("QTextBrowser { background:#fdf6e3; color:#586e75; }");
    } else {
        m_preview->setStyleSheet("");
    }
}

void MarkdownToPdf::onOpenMd()
{
    QString f = QFileDialog::getOpenFileName(this, tr("打开 Markdown 文件"), QString(),
        tr("Markdown (*.md *.markdown *.txt);;所有文件 (*)"));
    if (f.isEmpty()) return;
    loadMarkdownFile(f);
}

void MarkdownToPdf::loadMarkdownFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("打开失败"), tr("无法读取：%1").arg(path));
        return;
    }
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    QString md = in.readAll();
    file.close();

    m_currentMdPath = path;
    m_editor->setPlainText(md);
    m_statusLabel->setText(tr("已加载：%1").arg(QFileInfo(path).fileName()));
    renderPreview();
}

void MarkdownToPdf::onSaveMd()
{
    QString defaultPath = m_currentMdPath.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
              "/" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".md"
        : m_currentMdPath;
    QString f = QFileDialog::getSaveFileName(this, tr("保存 Markdown"), defaultPath,
        tr("Markdown (*.md);;所有文件 (*)"));
    if (f.isEmpty()) return;

    QFile file(f);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("保存失败"), tr("无法写入：%1").arg(f));
        return;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << m_editor->toPlainText();
    file.close();

    m_currentMdPath = f;
    m_statusLabel->setText(tr("已保存：%1").arg(f));
}

void MarkdownToPdf::onExportPdf()
{
    QString defaultBase = m_currentMdPath.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
              "/markdown_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")
        : QFileInfo(m_currentMdPath).absolutePath() + "/" +
              QFileInfo(m_currentMdPath).completeBaseName();
    QString savePath = QFileDialog::getSaveFileName(this, tr("导出 PDF"),
        defaultBase + ".pdf", tr("PDF 文件 (*.pdf)"));
    if (savePath.isEmpty()) return;

    // === 配置打印机 ===
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(savePath);

    auto pageSize = static_cast<QPageSize::PageSizeId>(m_pageSizeCombo->currentData().toInt());
    auto orientation = static_cast<QPageLayout::Orientation>(m_orientationCombo->currentData().toInt());
    qreal mm = m_marginSpin->value();
    printer.setPageSize(QPageSize(pageSize));
    printer.setPageOrientation(orientation);
    printer.setPageMargins(QMarginsF(mm, mm, mm, mm), QPageLayout::Millimeter);

    QSizeF pageSizePx = printer.pageRect(QPrinter::DevicePixel).size();

    // === footer 字体 + 高度（用 QFontMetricsF 在 printer 设备上得到真实像素高度）===
    QFont footerFont("-apple-system, PingFang SC, sans-serif");
    footerFont.setPointSize(qMax(8, m_fontSizeSpin->value() - 2));
    QFontMetricsF footerFm(footerFont, &printer);
    qreal footerLineH = footerFm.height();
    qreal footerBandH = m_pageNumbersCheck->isChecked() ? footerLineH * 2.2 : 0.0;

    // 正文/TOC 各自的可绘制 page 高度（保留 footer 区）
    QSizeF bodyPageSize(pageSizePx.width(), pageSizePx.height() - footerBandH);

    // === 正文 doc ===
    QTextDocument bodyDoc;
    // 关键：让 doc 的 layout 引用 printer 的 DPI，否则 pt 字号会按 96 DPI 计算导致字过小
    bodyDoc.documentLayout()->setPaintDevice(&printer);
    bodyDoc.setDefaultStyleSheet(currentStyleSheet());
    if (!m_currentMdPath.isEmpty()) {
        bodyDoc.setBaseUrl(QUrl::fromLocalFile(QFileInfo(m_currentMdPath).absolutePath() + "/"));
    }
    bodyDoc.setMarkdown(m_editor->toPlainText(), QTextDocument::MarkdownDialectGitHub);
    bodyDoc.setPageSize(bodyPageSize);

    int bodyPages = bodyDoc.pageCount();

    // === 收集标题（用于 TOC + 书签）===
    QList<HeadingEntry> headings = collectHeadings(
        bodyDoc, bodyPageSize.height(), m_tocDepthSpin->value());

    bool wantToc = m_tocCheck->isChecked() && !headings.isEmpty();

    // === 构建 TOC doc，并迭代收敛页码偏移 ===
    QTextDocument tocDoc;
    int tocPages = 0;
    if (wantToc) {
        tocDoc.documentLayout()->setPaintDevice(&printer);
        tocDoc.setDefaultStyleSheet(tocCss(m_fontSizeSpin->value()));
        tocDoc.setPageSize(bodyPageSize);

        int offset = 1;
        for (int iter = 0; iter < 4; ++iter) {
            tocDoc.setHtml(buildTocHtml(headings, offset));
            int actual = tocDoc.pageCount();
            if (actual == offset) { tocPages = actual; break; }
            offset = actual;
            tocPages = actual;
        }
    }

    // === 渲染输出 ===
    QPainter painter(&printer);
    bool first = true;
    int totalPages = tocPages + bodyPages;

    auto drawFooter = [&](int displayPageIdx /* 1-based */) {
        if (footerBandH <= 0) return;
        painter.save();
        painter.setFont(footerFont);
        painter.setPen(QColor(120, 120, 120));
        QRectF footerRect(0, pageSizePx.height() - footerBandH,
                          pageSizePx.width(), footerBandH);
        painter.drawText(footerRect, Qt::AlignCenter,
            tr("第 %1 / %2 页").arg(displayPageIdx).arg(totalPages));
        painter.restore();
    };

    // TOC 页
    for (int i = 0; i < tocPages; ++i) {
        if (!first) printer.newPage();
        first = false;
        drawDocPage(&painter, tocDoc, i, bodyPageSize);
        drawFooter(i + 1);
    }

    // 正文页
    for (int i = 0; i < bodyPages; ++i) {
        if (!first) printer.newPage();
        first = false;
        drawDocPage(&painter, bodyDoc, i, bodyPageSize);
        drawFooter(i + 1 + tocPages);
    }

    painter.end();

    // === PDF outline 注入（侧边栏书签）===
    QString outlineErr;
    if (!headings.isEmpty()) {
        // 把扁平 heading 列表按 level 转成嵌套树
        std::function<void(int, QList<HeadingEntry>::const_iterator&,
                          QList<HeadingEntry>::const_iterator,
                          QList<pdfoutline::Item>&)> buildTree;
        buildTree = [&](int parentLevel, QList<HeadingEntry>::const_iterator& it,
                        QList<HeadingEntry>::const_iterator end,
                        QList<pdfoutline::Item>& out) {
            while (it != end && it->level > parentLevel) {
                pdfoutline::Item node;
                node.title = it->text;
                // PDF 中的页码 = TOC 页数 + body 中的页码
                node.page0 = tocPages + it->bodyPage;
                int myLevel = it->level;
                ++it;
                buildTree(myLevel, it, end, node.children);
                out.append(node);
            }
        };
        QList<pdfoutline::Item> tree;
        auto it = headings.cbegin();
        buildTree(0, it, headings.cend(), tree);
        pdfoutline::inject(savePath, tree, &outlineErr);
    }

    QMessageBox box(this);
    box.setWindowTitle(tr("导出完成"));
    QString summary = tr("PDF 已保存：\n%1\n\n").arg(savePath);
    if (tocPages > 0) {
        summary += tr("目录 %1 页 + 正文 %2 页（共 %3 页）")
                       .arg(tocPages).arg(bodyPages).arg(tocPages + bodyPages);
    } else {
        summary += tr("共 %1 页").arg(bodyPages);
    }
    box.setText(summary);
    box.setIcon(QMessageBox::Information);
    auto* openBtn = box.addButton(tr("打开 PDF"), QMessageBox::ActionRole);
    auto* dirBtn = box.addButton(tr("打开所在目录"), QMessageBox::ActionRole);
    box.addButton(QMessageBox::Ok);
    box.exec();

    if (box.clickedButton() == openBtn)
        QDesktopServices::openUrl(QUrl::fromLocalFile(savePath));
    else if (box.clickedButton() == dirBtn)
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(savePath).absolutePath()));
}

void MarkdownToPdf::dragEnterEvent(QDragEnterEvent* e)
{
    if (e->mimeData()->hasUrls()) {
        for (const QUrl& u : e->mimeData()->urls()) {
            if (u.isLocalFile()) {
                QString suf = QFileInfo(u.toLocalFile()).suffix().toLower();
                if (suf == "md" || suf == "markdown" || suf == "txt") {
                    e->acceptProposedAction();
                    return;
                }
            }
        }
    }
}

void MarkdownToPdf::dropEvent(QDropEvent* e)
{
    for (const QUrl& u : e->mimeData()->urls()) {
        if (!u.isLocalFile()) continue;
        QString p = u.toLocalFile();
        QString suf = QFileInfo(p).suffix().toLower();
        if (suf == "md" || suf == "markdown" || suf == "txt") {
            loadMarkdownFile(p);
            e->acceptProposedAction();
            return;
        }
    }
}
