#include "notepad.h"
#include "../markdown-to-pdf/pdfoutlineinjector.h"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QBuffer>
#include <QDebug>
#include <QDesktopServices>
#include <QFontMetricsF>
#include <QPainter>
#include <QPdfWriter>
#include <QPrinter>
#include <QScrollBar>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>
#include <QUrl>
#include <functional>

REGISTER_DYNAMICOBJECT(Notepad);

namespace {
// 各平台默认中文字体
QString defaultEditorFontFamily()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("Microsoft YaHei");      // 微软雅黑
#elif defined(Q_OS_MAC) || defined(Q_OS_IOS)
    return QStringLiteral("PingFang SC");          // 苹方
#else
    return QStringLiteral("Noto Sans CJK SC");     // Linux 常见中文字体
#endif
}
}  // namespace

// RichTextEdit 实现
// 锁定字号常量 — 改这里就能整体改默认/强制字号
static constexpr int LOCKED_PT = 20;

// 把 currentCharFormat 强制设成 20pt — 影响"下一个字符的格式"，
// 包括按键输入、insertPlainText、按 Enter 后的新段落
static inline void forceLockedFormat(QTextEdit* edit) {
    if (!edit) return;
    QTextCharFormat fmt;
    QFont f = edit->font();
    f.setPointSize(LOCKED_PT);
    fmt.setFont(f);
    edit->setCurrentCharFormat(fmt);
}

RichTextEdit::RichTextEdit(QWidget* parent) : QTextEdit(parent),
                                              m_isDragging(false), m_currentHandle(None) {
    setAcceptDrops(true);
    setMouseTracking(true);
}

void RichTextEdit::keyPressEvent(QKeyEvent* event) {
    // 处理 Ctrl+V 粘贴
    if (event->matches(QKeySequence::Paste)) {
        if (const QMimeData* mimeData = QApplication::clipboard()->mimeData()) {
            insertFromMimeData(mimeData);
            return;
        }
    }

    // 关键：每次按键之前，先把 currentCharFormat 锁回 20pt
    // 这样按下的字符、按下 Enter 后的新段落、按方向键移动后的下一次输入，
    // 全部都以 20pt 写入文档
    forceLockedFormat(this);
    QTextEdit::keyPressEvent(event);
    // 按键完成后再锁一次（防御 keyPressEvent 自己改了 cursor format）
    forceLockedFormat(this);
}

void RichTextEdit::insertFromMimeData(const QMimeData* source) {
    // 图片
    if (source->hasImage()) {
        if (const auto pixmap = qvariant_cast<QPixmap>(source->imageData()); !pixmap.isNull()) {
            emit imageDropped(pixmap);
            return;
        }
    }

    // 文件路径
    if (source->hasUrls()) {
        for (QList<QUrl> urls = source->urls(); const QUrl& url : urls) {
            if (url.isLocalFile()) {
                QString filePath = url.toLocalFile();
                if (QFileInfo fileInfo(filePath); fileInfo.exists()) {
                    emit fileDropped(filePath);
                    return;
                }
            }
        }
    }

    // 文本：只取纯文本（剥掉 HTML 里所有 font-size / font-family / 颜色），
    // 然后用 currentCharFormat（已锁 20pt）插入
    forceLockedFormat(this);
    QString plain;
    if (source->hasText()) plain = source->text();
    else if (source->hasHtml()) {
        QTextDocument tmp;
        tmp.setHtml(source->html());
        plain = tmp.toPlainText();
    }
    if (!plain.isEmpty()) {
        QTextCursor cur = textCursor();
        cur.insertText(plain, currentCharFormat());
    }
    forceLockedFormat(this);
}

void RichTextEdit::wheelEvent(QWheelEvent* event) {
    // 检查是否按住Ctrl键并且光标在图片上
    if (event->modifiers() & Qt::ControlModifier) {
        QTextCursor cursor = cursorForPosition(event->position().toPoint());
        QTextImageFormat imageFormat = getImageFormatAtCursor(cursor);

        if (!imageFormat.name().isEmpty()) {
            // 计算缩放因子
            double scaleFactor = 1.0;
            if (event->angleDelta().y() > 0) {
                scaleFactor = 1.1; // 放大
            } else {
                scaleFactor = 0.9; // 缩小
            }

            resizeImageAtCursor(cursor, scaleFactor);
            emit imageResized();
            event->accept();
            return;
        }
    }

    // 默认处理
    QTextEdit::wheelEvent(event);
}

QTextImageFormat RichTextEdit::getImageFormatAtCursor(const QTextCursor& cursor) {
    QTextCharFormat format = cursor.charFormat();
    if (format.isImageFormat()) {
        return format.toImageFormat();
    }
    return QTextImageFormat();
}

void RichTextEdit::resizeImageAtCursor(const QTextCursor& cursor, double scaleFactor) {
    QTextImageFormat imageFormat = getImageFormatAtCursor(cursor);
    if (imageFormat.name().isEmpty()) {
        return;
    }

    // 获取当前尺寸
    int currentWidth = imageFormat.width();
    int currentHeight = imageFormat.height();

    // 如果没有设置尺寸，使用原始图片尺寸
    if (currentWidth <= 0 || currentHeight <= 0) {
        QPixmap pixmap(imageFormat.name());
        if (!pixmap.isNull()) {
            currentWidth = pixmap.width();
            currentHeight = pixmap.height();
        }
    }

    // 计算新尺寸
    int newWidth = qMax(50, static_cast<int>(currentWidth * scaleFactor));
    int newHeight = qMax(50, static_cast<int>(currentHeight * scaleFactor));

    // 限制最大尺寸
    if (newWidth > 800) {
        newHeight = (newHeight * 800) / newWidth;
        newWidth = 800;
    }

    // 更新图片格式
    QTextCursor editCursor = cursor;
    editCursor.select(QTextCursor::WordUnderCursor);

    QTextImageFormat newFormat = imageFormat;
    newFormat.setWidth(newWidth);
    newFormat.setHeight(newHeight);

    editCursor.setCharFormat(newFormat);
}

void RichTextEdit::contextMenuEvent(QContextMenuEvent* event) {
    QTextCursor cursor = cursorForPosition(event->pos());
    QTextImageFormat imageFormat = getImageFormatAtCursor(cursor);

    if (!imageFormat.name().isEmpty()) {
        // 如果光标在图片上，显示图片相关菜单
        showImageContextMenu(event->globalPos(), cursor);
    } else {
        // 否则显示默认菜单
        QTextEdit::contextMenuEvent(event);
    }
}

void RichTextEdit::showImageContextMenu(const QPoint& pos, const QTextCursor& cursor) {
    QMenu* menu = new QMenu(this);

    // 图片缩放选项
    QAction* enlargeAction = menu->addAction(tr("🔍 放大图片 (110%)"));
    QAction* shrinkAction = menu->addAction(tr("🔍 缩小图片 (90%)"));
    menu->addSeparator();

    // 预设尺寸选项
    QAction* smallAction = menu->addAction(tr("📏 小尺寸 (200px)"));
    QAction* mediumAction = menu->addAction(tr("📏 中尺寸 (400px)"));
    QAction* largeAction = menu->addAction(tr("📏 大尺寸 (600px)"));
    menu->addSeparator();

    // 原始尺寸
    QAction* originalAction = menu->addAction(tr("📐 原始尺寸"));

    QAction* selectedAction = menu->exec(pos);

    if (selectedAction == enlargeAction) {
        resizeImageAtCursor(cursor, 1.1);
        emit imageResized();
    } else if (selectedAction == shrinkAction) {
        resizeImageAtCursor(cursor, 0.9);
        emit imageResized();
    } else if (selectedAction == smallAction) {
        resizeImageToWidth(cursor, 200);
        emit imageResized();
    } else if (selectedAction == mediumAction) {
        resizeImageToWidth(cursor, 400);
        emit imageResized();
    } else if (selectedAction == largeAction) {
        resizeImageToWidth(cursor, 600);
        emit imageResized();
    } else if (selectedAction == originalAction) {
        resizeImageToOriginal(cursor);
        emit imageResized();
    }

    menu->deleteLater();
}

void RichTextEdit::resizeImageToWidth(const QTextCursor& cursor, int targetWidth) {
    QTextImageFormat imageFormat = getImageFormatAtCursor(cursor);
    if (imageFormat.name().isEmpty()) {
        return;
    }

    // 获取原始图片尺寸
    QPixmap pixmap(imageFormat.name());
    if (pixmap.isNull()) {
        return;
    }

    // 计算保持宽高比的高度
    int targetHeight = (pixmap.height() * targetWidth) / pixmap.width();

    // 更新图片格式
    QTextCursor editCursor = cursor;
    editCursor.select(QTextCursor::WordUnderCursor);

    QTextImageFormat newFormat = imageFormat;
    newFormat.setWidth(targetWidth);
    newFormat.setHeight(targetHeight);

    editCursor.setCharFormat(newFormat);
}

void RichTextEdit::resizeImageToOriginal(const QTextCursor& cursor) {
    QTextImageFormat imageFormat = getImageFormatAtCursor(cursor);
    if (imageFormat.name().isEmpty()) {
        return;
    }

    // 获取原始图片尺寸
    QPixmap pixmap(imageFormat.name());
    if (pixmap.isNull()) {
        return;
    }

    // 更新为原始尺寸
    QTextCursor editCursor = cursor;
    editCursor.select(QTextCursor::WordUnderCursor);

    QTextImageFormat newFormat = imageFormat;
    newFormat.setWidth(pixmap.width());
    newFormat.setHeight(pixmap.height());

    editCursor.setCharFormat(newFormat);
}

void RichTextEdit::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        QTextCursor cursor = cursorForPosition(event->pos());
        QTextImageFormat imageFormat = getImageFormatAtCursor(cursor);

        if (!imageFormat.name().isEmpty()) {
            // 检查是否点击在图片的调整句柄上
            QRect imageRect = getImageRect(cursor);
            ResizeHandle handle = getResizeHandle(event->pos(), imageRect);

            if (handle != None) {
                m_isDragging = true;
                m_currentHandle = handle;
                m_dragStartPos = event->pos();
                m_originalImageRect = imageRect;
                m_originalAspectRatio = imageRect.height() > 0
                    ? double(imageRect.width()) / imageRect.height()
                    : 1.0;
                m_selectedImageCursor = cursor;
                m_selectedImageFormat = imageFormat;

                // 设置对应的光标样式
                setCursorForHandle(handle);
                event->accept();
                return;
            } else {
                // 点击图片但不在句柄上，选中图片
                m_selectedImageCursor = cursor;
                m_selectedImageFormat = imageFormat;
                update(); // 重绘以显示句柄
                event->accept();
                return;
            }
        } else {
            // 点击其他地方，取消图片选中
            m_selectedImageFormat = QTextImageFormat();
            update(); // 重绘以隐藏句柄
        }
    }

    QTextEdit::mousePressEvent(event);
}

void RichTextEdit::mouseMoveEvent(QMouseEvent* event) {
    if (m_isDragging && m_currentHandle != None) {
        // 默认按比例缩放，按住 Shift 才允许自由拉伸（拒绝默认乱变形）
        const QPoint delta = event->pos() - m_dragStartPos;
        const QRect orig = m_originalImageRect;
        const double aspect = m_originalAspectRatio > 0 ? m_originalAspectRatio : 1.0;
        const bool freeResize = event->modifiers() & Qt::ShiftModifier;

        double newW = orig.width();
        double newH = orig.height();

        // 1) 按手柄分发：直接修改对应方向的尺寸
        switch (m_currentHandle) {
        case Right:        newW = orig.width()  + delta.x(); break;
        case Left:         newW = orig.width()  - delta.x(); break;
        case Bottom:       newH = orig.height() + delta.y(); break;
        case Top:          newH = orig.height() - delta.y(); break;
        case BottomRight:  newW = orig.width() + delta.x(); newH = orig.height() + delta.y(); break;
        case BottomLeft:   newW = orig.width() - delta.x(); newH = orig.height() + delta.y(); break;
        case TopRight:     newW = orig.width() + delta.x(); newH = orig.height() - delta.y(); break;
        case TopLeft:      newW = orig.width() - delta.x(); newH = orig.height() - delta.y(); break;
        default: break;
        }

        // 2) 锁比例（Shift 才放开）
        if (!freeResize) {
            switch (m_currentHandle) {
            case Right: case Left:
                newH = newW / aspect;
                break;
            case Top: case Bottom:
                newW = newH * aspect;
                break;
            default: {
                // 角点：按"鼠标移动较多的那个轴"为主，另一轴按比例换算 — 跟手感一致
                double dxRel = qAbs(newW - orig.width())  / qMax(1, orig.width());
                double dyRel = qAbs(newH - orig.height()) / qMax(1, orig.height());
                if (dxRel >= dyRel) newH = newW / aspect;
                else                newW = newH * aspect;
                break;
            }
            }
        }

        // 3) 钳制：保持比例的前提下不破坏比例
        const double minDim = 40, maxDim = 2000;
        if (newW < minDim) { newW = minDim; if (!freeResize) newH = newW / aspect; }
        if (newH < minDim) { newH = minDim; if (!freeResize) newW = newH * aspect; }
        if (newW > maxDim) { newW = maxDim; if (!freeResize) newH = newW / aspect; }
        if (newH > maxDim) { newH = maxDim; if (!freeResize) newW = newH * aspect; }

        // 4) 写回 charFormat — 精确选中图片这个字符（长度 1），避免覆盖周围文本
        QTextImageFormat newFormat = m_selectedImageFormat;
        newFormat.setWidth(qRound(newW));
        newFormat.setHeight(qRound(newH));

        QTextCursor c(document());
        int pos = m_selectedImageCursor.position();
        // 图片字符可能位于 pos 或 pos-1（取决于点击落点），探测一下
        c.setPosition(pos);
        c.setPosition(pos + 1, QTextCursor::KeepAnchor);
        if (!c.charFormat().isImageFormat() && pos > 0) {
            c.setPosition(pos - 1);
            c.setPosition(pos, QTextCursor::KeepAnchor);
        }
        if (c.charFormat().isImageFormat()) {
            c.setCharFormat(newFormat);
        } else {
            // 兜底：旧逻辑
            QTextCursor fallback = m_selectedImageCursor;
            fallback.select(QTextCursor::WordUnderCursor);
            fallback.setCharFormat(newFormat);
        }

        emit imageResized();
        event->accept();
        return;
    } else {
        // 检查鼠标是否在图片上，更新光标样式
        QTextCursor cursor = cursorForPosition(event->pos());
        QTextImageFormat imageFormat = getImageFormatAtCursor(cursor);

        if (!imageFormat.name().isEmpty()) {
            QRect imageRect = getImageRect(cursor);
            ResizeHandle handle = getResizeHandle(event->pos(), imageRect);
            setCursorForHandle(handle);
        } else {
            setCursor(Qt::IBeamCursor);
        }
    }

    QTextEdit::mouseMoveEvent(event);
}

void RichTextEdit::mouseReleaseEvent(QMouseEvent* event) {
    if (m_isDragging) {
        m_isDragging = false;
        m_currentHandle = None;
        setCursor(Qt::IBeamCursor);
        event->accept();
        return;
    }

    QTextEdit::mouseReleaseEvent(event);
}

void RichTextEdit::paintEvent(QPaintEvent* event) {
    QTextEdit::paintEvent(event);

    // 如果有选中的图片，绘制调整句柄
    if (!m_selectedImageFormat.name().isEmpty()) {
        QPainter painter(viewport());
        QRect imageRect = getImageRect(m_selectedImageCursor);
        if (!imageRect.isEmpty()) {
            drawResizeHandles(painter, imageRect);
        }
    }
}

QRect RichTextEdit::getImageRect(const QTextCursor& cursor) {
    QTextImageFormat format = getImageFormatAtCursor(cursor);
    if (format.name().isEmpty()) {
        return QRect();
    }

    // QTextCursor::charFormat() 返回的是光标"左侧"那个字符的格式，
    // 所以这里 cursor 实际位于图片**之后**。要得到图片左边缘，需要把
    // 辅助光标退回 1 位（图片字符长度始终是 1）。
    QTextCursor before(document());
    int pos = cursor.position();
    before.setPosition(pos > 0 ? pos - 1 : pos);
    QRect cRect = cursorRect(before);

    int width = format.width();
    int height = format.height();
    if (width <= 0 || height <= 0) {
        QPixmap pixmap(format.name());
        if (!pixmap.isNull()) {
            width = pixmap.width();
            height = pixmap.height();
        } else {
            width = 100;
            height = 100;
        }
    }

    return QRect(cRect.x(), cRect.y(), width, height);
}

RichTextEdit::ResizeHandle RichTextEdit::getResizeHandle(const QPoint& pos, const QRect& imageRect) {
    // 把命中区域加大（hitSize > 视觉手柄 visSize），实际触发更宽容
    const int hitSize = 16;
    const int half = hitSize / 2;

    auto hit = [&](const QPoint& center) {
        return QRect(center.x() - half, center.y() - half, hitSize, hitSize).contains(pos);
    };

    if (hit(imageRect.topLeft()))     return TopLeft;
    if (hit(imageRect.topRight()))    return TopRight;
    if (hit(imageRect.bottomLeft()))  return BottomLeft;
    if (hit(imageRect.bottomRight())) return BottomRight;

    QPoint midL(imageRect.left(),  imageRect.center().y());
    QPoint midR(imageRect.right(), imageRect.center().y());
    QPoint midT(imageRect.center().x(), imageRect.top());
    QPoint midB(imageRect.center().x(), imageRect.bottom());
    if (hit(midL)) return Left;
    if (hit(midR)) return Right;
    if (hit(midT)) return Top;
    if (hit(midB)) return Bottom;

    return None;
}

void RichTextEdit::drawResizeHandles(QPainter& painter, const QRect& imageRect) {
    const int visSize = 10;
    const int half = visSize / 2;
    painter.setRenderHint(QPainter::Antialiasing, true);

    // 蓝色选择框
    painter.setPen(QPen(QColor(34, 139, 230), 2, Qt::SolidLine));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(imageRect.adjusted(-1, -1, 1, 1));

    // 8 个手柄：白底蓝边
    auto drawHandle = [&](const QPoint& c) {
        QRect r(c.x() - half, c.y() - half, visSize, visSize);
        painter.setBrush(Qt::white);
        painter.setPen(QPen(QColor(34, 139, 230), 2));
        painter.drawRoundedRect(r, 2, 2);
    };

    drawHandle(imageRect.topLeft());
    drawHandle(imageRect.topRight());
    drawHandle(imageRect.bottomLeft());
    drawHandle(imageRect.bottomRight());
    drawHandle(QPoint(imageRect.left(),  imageRect.center().y()));
    drawHandle(QPoint(imageRect.right(), imageRect.center().y()));
    drawHandle(QPoint(imageRect.center().x(), imageRect.top()));
    drawHandle(QPoint(imageRect.center().x(), imageRect.bottom()));
}

void RichTextEdit::setCursorForHandle(ResizeHandle handle) {
    switch (handle) {
    case TopLeft:
    case BottomRight:
        setCursor(Qt::SizeFDiagCursor);
        break;
    case TopRight:
    case BottomLeft:
        setCursor(Qt::SizeBDiagCursor);
        break;
    case Top:
    case Bottom:
        setCursor(Qt::SizeVerCursor);
        break;
    case Left:
    case Right:
        setCursor(Qt::SizeHorCursor);
        break;
    default:
        setCursor(Qt::IBeamCursor);
        break;
    }
}

QString RichTextEdit::savePixmapToFile(const QPixmap& pixmap) {
    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString fileName = QString("clipboard_image_%1.png").arg(QDateTime::currentMSecsSinceEpoch());

    if (QString filePath = QDir(tempDir).filePath(fileName); pixmap.save(filePath, "PNG")) {
        return filePath;
    }

    return { };
}

Notepad::Notepad() : QWidget(nullptr), DynamicObjectBase(),
                     currentNoteId(-1), hasUnsavedChanges(false) {
    setupUI();
    setupShortcuts();
    initializeDatabase();

    // 设置媒体存储路径
    mediaStorageBasePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/notepad_media";
    createMediaDirectory(mediaStorageBasePath);

    // 设置自动保存定时器（缩短为 3 秒，丢失更新窗口更小）
    autoSaveTimer = new QTimer(this);
    autoSaveTimer->setSingleShot(false);
    connect(autoSaveTimer, &QTimer::timeout, this, &Notepad::onAutoSaveTimer);
    autoSaveTimer->start(3 * 1000);

    // 🔒 字号 20pt 锁定：每次文本变化（输入、删除、粘贴、HR、HTML 注入）立即同步
    // 把全文 mergeCharFormat 到 20pt。同步执行不防抖 — 慢一拍都不行。
    connect(contentEdit, &QTextEdit::textChanged, this, [this]() {
        if (!suppressNormalize) normalizeMinFontSizes();
    });

    // 失焦立即保存：编辑器失去焦点（点别处、切窗口、切笔记前一刻）就 flush 一次
    contentEdit->installEventFilter(this);

    loadNotes();
    updateButtonStates();

    // 确保编辑器在启动时可用
    contentEdit->setEnabled(true);

    // updateStatus(tr("就绪")); // 移除状态栏
}

Notepad::~Notepad() {
    if (hasUnsavedChanges) {
        saveCurrentNote();
    }
    // 不要删除共用数据库实例
}

void Notepad::setupUI() {
    mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    mainSplitter = new QSplitter(Qt::Horizontal);

    setupNoteListArea();
    setupEditorArea();
    // setupStatusBar(); // 移除状态栏

    mainSplitter->addWidget(noteListWidget);
    mainSplitter->addWidget(editorWidget);
    mainSplitter->setStretchFactor(0, 0);
    mainSplitter->setStretchFactor(1, 1);
    mainSplitter->setSizes({ 300, 700 });

    mainLayout->addWidget(mainSplitter);
    // mainLayout->addLayout(statusLayout); // 移除状态栏
}

void Notepad::setupNoteListArea() {
    noteListWidget = new QWidget();
    noteListWidget->setFixedWidth(320);
    noteListLayout = new QVBoxLayout(noteListWidget);
    noteListLayout->setContentsMargins(0, 0, 0, 0);

    // 搜索框
    searchEdit = new QLineEdit();
    searchEdit->setPlaceholderText(tr("搜索笔记..."));
    connect(searchEdit, &QLineEdit::textChanged, this, &Notepad::onSearchTextChanged);

    // 笔记列表
    notesList = new QListWidget();
    notesList->setAlternatingRowColors(false); // 关闭交替行颜色，使用自定义样式
    notesList->setSelectionMode(QAbstractItemView::ExtendedSelection); // 支持多选
    notesList->setFocusPolicy(Qt::StrongFocus); // 确保可以接收键盘事件
    notesList->setContextMenuPolicy(Qt::CustomContextMenu); // 启用右键菜单


    notesList->setItemDelegate(new NoteItemDelegate(notesList));


    connect(notesList, &QListWidget::currentRowChanged, this, &Notepad::onNoteSelectionChanged);
    connect(notesList, &QListWidget::itemSelectionChanged, this, &Notepad::onItemSelectionChanged);
    connect(notesList, &QListWidget::customContextMenuRequested, this, &Notepad::onShowContextMenu);

    // 按钮区域
    auto* buttonLayout = new QHBoxLayout();
    newNoteBtn = new QPushButton(tr("➕ 新建"));
    newNoteBtn->setToolTip(tr("新建笔记 (Ctrl+N)"));
    newNoteBtn->setStyleSheet("QPushButton { background-color: #28a745; color: white; } QPushButton:hover { background-color: #218838; }");

    deleteNoteBtn = new QPushButton(tr("🗑️ 删除"));
    deleteNoteBtn->setStyleSheet("QPushButton { background-color: #dc3545; color: white; } QPushButton:hover { background-color: #c82333; }");
    deleteNoteBtn->setEnabled(false);

    connect(newNoteBtn, &QPushButton::clicked, this, &Notepad::onNewNote);
    connect(deleteNoteBtn, &QPushButton::clicked, this, &Notepad::onDeleteSelectedNotes);

    buttonLayout->addWidget(newNoteBtn);
    buttonLayout->addWidget(deleteNoteBtn);

    noteListLayout->addWidget(searchEdit);
    noteListLayout->addWidget(notesList);
    noteListLayout->addLayout(buttonLayout);
}

void Notepad::setupEditorArea() {
    editorWidget = new QWidget();
    editorLayout = new QVBoxLayout(editorWidget);
    editorLayout->setContentsMargins(0, 0, 0, 0);

    setupToolbar();

    // 创建编辑器容器（包含行号和编辑器）
    editorContainer = new QWidget();
    editorContainerLayout = new QHBoxLayout(editorContainer);
    editorContainerLayout->setContentsMargins(0, 0, 0, 0);
    editorContainerLayout->setSpacing(0);

    // 内容编辑器
    contentEdit = new RichTextEdit();
    contentEdit->setPlaceholderText(
        tr("开始编写你的笔记...\n\n"
           "提示：插入图片后，点击图片可显示蓝色拖拽手柄；按住 Ctrl + 滚轮可整体缩放；"
           "右键图片有 200/400/600px 与原始尺寸快捷菜单。"));
    contentEdit->setAcceptRichText(true);
    {
        QFont editorFont;
        // 把当前平台的默认中文字体放到 fallback 链最前面
        editorFont.setFamilies({defaultEditorFontFamily(),
                                QStringLiteral("PingFang SC"),
                                QStringLiteral("Microsoft YaHei"),
                                QStringLiteral("Helvetica Neue"),
                                QStringLiteral("sans-serif")});
        editorFont.setPointSize(20);   // 默认 20pt（与全局最小字号一致）
        contentEdit->setFont(editorFont);
    }
    // 富文本模式（允许粘贴格式、B/I/U 等），但 widget 默认字体 20pt 兜底
    contentEdit->setAcceptRichText(true);
    contentEdit->document()->setDefaultFont(contentEdit->font());
    contentEdit->setStyleSheet(
        "QTextEdit {"
        "    border: 1px solid #d0d7de;"
        "    border-radius: 6px;"
        "    padding: 14px 18px;"
        "    background: #ffffff;"
        "    selection-background-color: #b8d4ff;"
        "}"
        "QTextEdit:focus { border-color: #228be6; }"
    );

    // 连接信号
    connect(contentEdit, &RichTextEdit::imageDropped, this, &Notepad::onImagePasted);
    connect(contentEdit, &RichTextEdit::fileDropped, this, &Notepad::onFileDropped);
    connect(contentEdit, &RichTextEdit::imageResized, this, [this]() {
        if (currentNoteId != -1) {
            hasUnsavedChanges = true;
            updateButtonStates();
        }
    });

    // 连接内容变化，只做基本标记
    // 光标移动 / 进入不同格式区域时，把工具栏字号同步成 cursor 当前 char format 的字号
    connect(contentEdit, &QTextEdit::currentCharFormatChanged,
            this, &Notepad::onCursorFormatChanged);

    connect(contentEdit, &QTextEdit::textChanged, this, [this]() {
        if (currentNoteId != -1) {
            hasUnsavedChanges = true;
            updateButtonStates();
        }


    });

    // 添加到容器
    editorContainerLayout->addWidget(contentEdit);

    editorLayout->addWidget(toolbar);
    editorLayout->addWidget(toolbar2);
    editorLayout->addWidget(editorContainer);
}

void Notepad::setupShortcuts() {
    // 创建 Ctrl+N 快捷键
    newNoteShortcut = new QShortcut(QKeySequence::New, this);
    connect(newNoteShortcut, &QShortcut::activated, this, &Notepad::onNewNote);
    newNoteShortcut->setEnabled(false); // 初始时禁用，等tab显示时启用
}

void Notepad::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    // 当tab显示时启用快捷键
    if (newNoteShortcut) {
        newNoteShortcut->setEnabled(true);
    }
}

void Notepad::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    // 当tab隐藏时禁用快捷键
    if (newNoteShortcut) {
        newNoteShortcut->setEnabled(false);
    }
}

void Notepad::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Delete) {
        // 如果笔记列表有焦点且有选中项，删除选中的笔记
        if (notesList->hasFocus() && !notesList->selectedItems().isEmpty()) {
            onDeleteSelectedNotes();
            return;
        }
    }
    QWidget::keyPressEvent(event);
}

void Notepad::setupToolbar() {
    // 共用样式（两个工具栏一致）
    static const QString kToolbarCss = QStringLiteral(
        "QToolBar { spacing: 3px; padding: 4px 6px; border: none;"
        "           background: #f8f9fa; }"
        "QToolBar QPushButton {"
        "  padding: 4px 9px; min-height: 24px; border: 1px solid transparent;"
        "  border-radius: 4px; color: #343a40; background: transparent; }"
        "QToolBar QPushButton:hover {"
        "  background: #e7f5ff; border-color: #d0ebff; }"
        "QToolBar QPushButton:pressed { background: #d0ebff; }"
        "QToolBar QPushButton:checked {"
        "  background: #d0ebff; border-color: #74c0fc; color: #1864ab; }"
        "QToolBar QPushButton:disabled { color: #adb5bd; }"
        "QToolBar QSpinBox, QToolBar QFontComboBox {"
        "  padding: 2px 4px; border: 1px solid #ced4da; border-radius: 4px; background:#fff; }"
        "QToolBar QLabel { color: #495057; padding: 0 4px; background: transparent; }"
        "QToolBar::separator { background: #dee2e6; width: 1px; margin: 4px 4px; }"
    );

    toolbar = new QToolBar();
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar->setIconSize(QSize(16, 16));
    toolbar->setMovable(false);
    toolbar->setStyleSheet(kToolbarCss);

    toolbar2 = new QToolBar();
    toolbar2->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar2->setIconSize(QSize(16, 16));
    toolbar2->setMovable(false);
    toolbar2->setStyleSheet(kToolbarCss + "QToolBar { border-bottom: 1px solid #dee2e6; }");

    // 字体设置
    fontCombo = new QFontComboBox();
    fontCombo->setCurrentFont(QFont(defaultEditorFontFamily()));
    connect(fontCombo, &QFontComboBox::currentFontChanged, this, &Notepad::onFontChanged);

    fontSizeSpinBox = new QSpinBox();
    fontSizeSpinBox->setRange(20, 72);    // 最小 20pt，最大 72，可改
    fontSizeSpinBox->setValue(20);
    fontSizeSpinBox->setSuffix(tr("pt"));
    connect(fontSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &Notepad::onFontSizeChanged);

    // 文本样式按钮（用 setProperty + 字体属性，避免覆盖工具栏 CSS 的 hover/checked 状态）
    auto styleBtn = [](QPushButton* btn, const char* css) {
        btn->setStyleSheet(QString::fromLatin1(
            "QPushButton { %1 padding: 4px 9px; min-width: 22px;"
            "              border: 1px solid transparent; border-radius: 4px; }"
            "QPushButton:hover { background: #e7f5ff; border-color:#d0ebff; }"
            "QPushButton:checked { background: #d0ebff; border-color:#74c0fc; color:#1864ab; }"
        ).arg(css));
    };

    boldBtn = new QPushButton(tr("B"));
    boldBtn->setCheckable(true);
    boldBtn->setToolTip(tr("加粗 (Ctrl+B)"));
    styleBtn(boldBtn, "font-weight: bold;");
    connect(boldBtn, &QPushButton::clicked, this, &Notepad::onBoldClicked);

    italicBtn = new QPushButton(tr("I"));
    italicBtn->setCheckable(true);
    italicBtn->setToolTip(tr("斜体 (Ctrl+I)"));
    styleBtn(italicBtn, "font-style: italic;");
    connect(italicBtn, &QPushButton::clicked, this, &Notepad::onItalicClicked);

    underlineBtn = new QPushButton(tr("U"));
    underlineBtn->setCheckable(true);
    underlineBtn->setToolTip(tr("下划线 (Ctrl+U)"));
    styleBtn(underlineBtn, "text-decoration: underline;");
    connect(underlineBtn, &QPushButton::clicked, this, &Notepad::onUnderlineClicked);

    // 颜色按钮（纯 emoji + tooltip，节省工具栏宽度）
    textColorBtn = new QPushButton(tr("🎨"));
    textColorBtn->setToolTip(tr("字体颜色"));
    connect(textColorBtn, &QPushButton::clicked, this, &Notepad::onTextColorClicked);

    bgColorBtn = new QPushButton(tr("🖍️"));
    bgColorBtn->setToolTip(tr("背景颜色"));
    connect(bgColorBtn, &QPushButton::clicked, this, &Notepad::onBackgroundColorClicked);

    // 对齐按钮
    alignLeftBtn = new QPushButton(tr("⬅"));
    alignLeftBtn->setCheckable(true);
    alignLeftBtn->setChecked(true);
    alignLeftBtn->setToolTip(tr("左对齐"));
    connect(alignLeftBtn, &QPushButton::clicked, this, &Notepad::onAlignLeftClicked);

    alignCenterBtn = new QPushButton(tr("⇔"));
    alignCenterBtn->setCheckable(true);
    alignCenterBtn->setToolTip(tr("居中对齐"));
    connect(alignCenterBtn, &QPushButton::clicked, this, &Notepad::onAlignCenterClicked);

    alignRightBtn = new QPushButton(tr("➡"));
    alignRightBtn->setCheckable(true);
    alignRightBtn->setToolTip(tr("右对齐"));
    connect(alignRightBtn, &QPushButton::clicked, this, &Notepad::onAlignRightClicked);

    alignJustifyBtn = new QPushButton(tr("≡"));
    alignJustifyBtn->setCheckable(true);
    alignJustifyBtn->setToolTip(tr("两端对齐"));
    connect(alignJustifyBtn, &QPushButton::clicked, this, &Notepad::onAlignJustifyClicked);

    // 媒体插入按钮（纯 emoji + tooltip）
    insertImageBtn = new QPushButton(tr("🖼️"));
    insertImageBtn->setToolTip(tr("插入图片"));
    connect(insertImageBtn, &QPushButton::clicked, this, &Notepad::onInsertImage);

    insertMediaBtn = new QPushButton(tr("🎬"));
    insertMediaBtn->setToolTip(tr("插入媒体"));
    connect(insertMediaBtn, &QPushButton::clicked, this, &Notepad::onInsertMedia);

    insertHrBtn = new QPushButton(tr("─"));
    insertHrBtn->setToolTip(tr("插入水平分割线（Ctrl+Shift+H）"));
    insertHrBtn->setShortcut(QKeySequence("Ctrl+Shift+H"));
    connect(insertHrBtn, &QPushButton::clicked, this, &Notepad::onInsertHr);

    cleanFormatBtn = new QPushButton(tr("🧹 清理格式"));
    cleanFormatBtn->setToolTip(tr(
        "一键清理：把整篇内容强制统一成 20pt 纯字，\n"
        "去掉加粗 / 斜体 / 下划线 / 颜色 / 字体名等所有格式。\n"
        "粘贴外部内容字号混乱时用。"));
    connect(cleanFormatBtn, &QPushButton::clicked, this, &Notepad::onCleanFormat);

    // 保存 + 导出
    saveBtn = new QPushButton(tr("💾 保存"));
    saveBtn->setStyleSheet(
        "QPushButton { background:#228be6; color:#fff; border:none;"
        "              padding: 5px 12px; border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background:#1c7ed6; }"
        "QPushButton:pressed { background:#1971c2; }"
    );
    saveBtn->setToolTip(tr("保存当前笔记 (Ctrl+S)"));
    connect(saveBtn, &QPushButton::clicked, this, &Notepad::onSaveNote);

    exportPdfBtn = new QPushButton(tr("📄 导出 PDF"));
    exportPdfBtn->setToolTip(tr("把当前笔记另存为 PDF 文件"));
    connect(exportPdfBtn, &QPushButton::clicked, this, &Notepad::onExportPdf);

    // 第一行：字体 / 字号 / B I U / 颜色
    toolbar->addWidget(new QLabel(tr("字体:")));
    toolbar->addWidget(fontCombo);
    toolbar->addWidget(new QLabel(tr("大小:")));
    toolbar->addWidget(fontSizeSpinBox);
    toolbar->addSeparator();
    toolbar->addWidget(boldBtn);
    toolbar->addWidget(italicBtn);
    toolbar->addWidget(underlineBtn);
    toolbar->addSeparator();
    toolbar->addWidget(textColorBtn);
    toolbar->addWidget(bgColorBtn);


    // 第二行：对齐 / 插入 / 保存 / 导出
    toolbar2->addWidget(alignLeftBtn);
    toolbar2->addWidget(alignCenterBtn);
    toolbar2->addWidget(alignRightBtn);
    toolbar2->addWidget(alignJustifyBtn);
    toolbar2->addSeparator();
    toolbar2->addWidget(insertImageBtn);
    toolbar2->addWidget(insertMediaBtn);
    toolbar2->addWidget(insertHrBtn);
    toolbar2->addSeparator();
    toolbar2->addWidget(cleanFormatBtn);
    // 用 stretch widget 把保存/导出推到右边
    {
        auto* spacer = new QWidget();
        spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        toolbar2->addWidget(spacer);
    }
    toolbar2->addWidget(saveBtn);
    toolbar2->addWidget(exportPdfBtn);
}

void Notepad::setupStatusBar() {
    // 状态栏已移除，此函数保留为空以避免编译错误
    // 如果需要状态栏功能，可以重新实现
}

void Notepad::initializeDatabase() {
    // 使用共用的默认数据库实例
    dbManager = SqliteWrapper::SqliteManager::getDefaultInstance();

    if (!dbManager) {
        // updateStatus(tr("无法获取数据库实例"), true); // 移除状态栏
        return;
    }

    // 如果数据库未打开，尝试打开默认数据库
    if (!dbManager->isOpen()) {
        QString defaultDbPath = SqliteWrapper::SqliteManager::getDefaultDatabasePath();
        if (!dbManager->openDatabase(defaultDbPath)) {
            // updateStatus(tr("无法打开共用数据库: %1").arg(dbManager->getLastError()), true); // 移除状态栏
            return;
        }
    }

    // 创建笔记表
    const QString createNotesTable = R"(
        CREATE TABLE IF NOT EXISTS notes (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            title TEXT NOT NULL,
            content TEXT,
            created DATETIME DEFAULT CURRENT_TIMESTAMP,
            modified DATETIME DEFAULT CURRENT_TIMESTAMP,
            is_deleted INTEGER DEFAULT 0
        )
    )";

    // 创建媒体文件表
    const QString createMediaTable = R"(
        CREATE TABLE IF NOT EXISTS media_files (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_name TEXT NOT NULL,
            relative_path TEXT NOT NULL,
            mime_type TEXT,
            file_size INTEGER,
            created DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";

    if (const SqliteWrapper::QueryResult notesResult = dbManager->execute(createNotesTable); !notesResult.success) {
        // updateStatus(tr("创建笔记表失败: %1").arg(notesResult.errorMessage), true); // 移除状态栏
        return;
    }

    if (const SqliteWrapper::QueryResult mediaResult = dbManager->execute(createMediaTable); !mediaResult.success) {
        // updateStatus(tr("创建媒体表失败: %1").arg(mediaResult.errorMessage), true); // 移除状态栏
        return;
    }

    // updateStatus(tr("数据库初始化完成")); // 移除状态栏
}

void Notepad::loadNotes() {
    notes.clear();

    const QString query = "SELECT id, title, content, created, modified, is_deleted FROM notes WHERE is_deleted = 0 ORDER BY modified DESC";

    if (SqliteWrapper::QueryResult result = dbManager->select(query); result.success) {
        for (const QVariant& row : result.data) {
            QVariantMap rowData = row.toMap();
            NoteItem note;
            note.id = rowData["id"].toInt();
            note.title = rowData["title"].toString();
            note.content = rowData["content"].toString();
            note.created = rowData["created"].toDateTime();
            note.modified = rowData["modified"].toDateTime();
            note.isDeleted = rowData["is_deleted"].toBool();

            notes.append(note);
        }
    }

    updateNotesList();
}

void Notepad::updateNotesList() {
    notesList->clear();

    const QString searchText = searchEdit->text().toLower();

    for (const NoteItem& note : notes) {
        if (searchText.isEmpty() ||
            note.title.toLower().contains(searchText) ||
            note.content.toLower().contains(searchText)) {
            addNoteToList(note, false); // 不插入到顶部，保持原有顺序
        }
    }
}

void Notepad::addNoteToList(const NoteItem& note, bool insertAtTop) {
    QListWidgetItem* item = new QListWidgetItem;

    // 存储数据
    item->setData(Qt::UserRole, note.id);
    item->setData(Qt::DisplayRole, note.title.isEmpty() ? tr("无标题") : note.title);
    item->setData(Qt::UserRole + 1, note.modified.toString("yyyy-MM-dd hh:mm"));

    // 高度由 delegate 控制
    item->setSizeHint(QSize(200, 50));

    if (insertAtTop) {
        notesList->insertItem(0, item);
    } else {
        notesList->addItem(item);
    }
}



void Notepad::updateNoteInList(const NoteItem& note) const {
    for (int i = 0; i < notesList->count(); ++i) {
        QListWidgetItem* item = notesList->item(i);
        if (item && item->data(Qt::UserRole).toInt() == note.id) {
            QString displayTitle = note.title.isEmpty() ? tr("无标题") : note.title;
            if (displayTitle.length() > 50) {
                displayTitle = displayTitle.left(50) + "...";
            }

            // 更新数据
            item->setData(Qt::DisplayRole, displayTitle);
            item->setData(Qt::UserRole + 1, note.modified.toString("yyyy-MM-dd hh:mm"));
            break;
        }
    }
}


void Notepad::removeNoteFromList(int noteId) {
    // 查找并删除对应的列表项
    for (int i = 0; i < notesList->count(); ++i) {
        QListWidgetItem* item = notesList->item(i);
        if (item && item->data(Qt::UserRole).toInt() == noteId) {
            delete notesList->takeItem(i);
            break;
        }
    }
}

void Notepad::onSearchTextChanged() {
    updateNotesList();
}

void Notepad::onNoteSelectionChanged() {
    if (hasUnsavedChanges && currentNoteId != -1) {
        saveCurrentNote();
    }

    QListWidgetItem* currentItem = notesList->currentItem();
    if (currentItem) {
        int noteId = currentItem->data(Qt::UserRole).toInt();
        loadNoteContent(noteId);
    } else {
        // 没有选中笔记时，不清空编辑器，允许用户创建新笔记
        currentNoteId = -1;
        hasUnsavedChanges = false;
    }

    updateButtonStates();
}

void Notepad::onItemSelectionChanged() {
    // 更新删除按钮状态
    QList<QListWidgetItem*> selectedItems = notesList->selectedItems();
    deleteNoteBtn->setEnabled(!selectedItems.isEmpty());

    // 如果只有一个选中项，更新当前笔记
    if (selectedItems.size() == 1) {
        onNoteSelectionChanged();
    } else if (selectedItems.size() > 1) {
        // 多选时清空编辑器前，先把当前笔记的脏数据持久化，避免静默丢失
        if (hasUnsavedChanges && currentNoteId != -1) {
            saveCurrentNote();
        }
        currentNoteId = -1;
        contentEdit->clear();
        hasUnsavedChanges = false;
        updateButtonStates();
    }
}

void Notepad::loadNoteContent(int noteId) {
    for (const NoteItem& note : notes) {
        if (note.id == noteId) {
            currentNoteId = noteId;
            contentEdit->setHtml(note.content);
            hasUnsavedChanges = false;
            normalizeMinFontSizes();   // 自动把 <20pt 的字号升到 20pt

            // 延迟设置焦点，确保内容加载完成
            QTimer::singleShot(50, [this]() {
                contentEdit->setFocus();
                QTextCursor cursor = contentEdit->textCursor();
                cursor.movePosition(QTextCursor::Start);
                contentEdit->setTextCursor(cursor);
            });

            // updateStatus(tr("已加载笔记")); // 移除状态栏
            break;
        }
    }
}

void Notepad::onNewNote() {
    // 如果有未保存的更改，先保存当前笔记
    if (hasUnsavedChanges && currentNoteId != -1) {
        saveCurrentNote();
    }

    // 使用原生SQL插入新笔记
    QString sql = "INSERT INTO notes (title, content) VALUES (:title, :content)";
    QVariantMap params;
    params["title"] = tr("新笔记");
    params["content"] = "";

    SqliteWrapper::QueryResult result = dbManager->execute(sql, params);

    if (result.success && result.lastInsertId > 0) {
        currentNoteId = result.lastInsertId;
        hasUnsavedChanges = false;

        // 创建新的笔记项并添加到内存列表顶部
        NoteItem newNote;
        newNote.id = result.lastInsertId;
        newNote.title = tr("新笔记");
        newNote.content = "";
        newNote.created = QDateTime::currentDateTime();
        newNote.modified = QDateTime::currentDateTime();
        newNote.isDeleted = false;

        // 添加到内存列表顶部
        notes.prepend(newNote);

        // 在UI列表顶部插入新项
        addNoteToList(newNote, true);

        // 选中新创建的笔记（第一个）
        notesList->setCurrentRow(0);

        // 清空编辑器并设置焦点
        contentEdit->clear();

        // 延迟设置焦点，确保界面更新完成
        QTimer::singleShot(100, [this]() {
            contentEdit->setFocus();
            QTextCursor cursor = contentEdit->textCursor();
            cursor.movePosition(QTextCursor::Start);
            contentEdit->setTextCursor(cursor);
        });

        // updateStatus(tr("已创建新笔记")); // 移除状态栏
        updateButtonStates();
    } else {
        // updateStatus(tr("创建笔记失败: %1").arg(result.errorMessage), true); // 移除状态栏
    }
}

void Notepad::onDeleteNote() {
    if (currentNoteId == -1)
        return;

    int ret = QMessageBox::question(this, tr("确认删除"),
                                    tr("确定要删除这篇笔记吗？此操作不可撤销。"),
                                    QMessageBox::Yes | QMessageBox::No);

    if (ret == QMessageBox::Yes) {
        QString sql = "UPDATE notes SET is_deleted = 1 WHERE id = :id";
        QVariantMap params;
        params["id"] = currentNoteId;

        SqliteWrapper::QueryResult result = dbManager->execute(sql, params);

        if (result.success && result.affectedRows > 0) {
            int deletedNoteId = currentNoteId;
            currentNoteId = -1;
            contentEdit->clear();
            hasUnsavedChanges = false;

            // 从内存列表中移除
            for (int i = 0; i < notes.size(); ++i) {
                if (notes[i].id == deletedNoteId) {
                    notes.removeAt(i);
                    break;
                }
            }

            // 从UI列表中移除
            removeNoteFromList(deletedNoteId);

            // updateStatus(tr("笔记已删除")); // 移除状态栏
            updateButtonStates();
        } else {
            // updateStatus(tr("删除笔记失败"), true); // 移除状态栏
        }
    }
}

void Notepad::onDeleteSelectedNotes() {
    QList<QListWidgetItem*> selectedItems = notesList->selectedItems();
    if (selectedItems.isEmpty())
        return;

    // 确认删除
    QString message = tr("确定要删除选中的 %1 个笔记吗？").arg(selectedItems.size());
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("确认删除"),
        message,
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        // 收集要删除的笔记ID
        QList<int> noteIds;
        for (QListWidgetItem* item : selectedItems) {
            noteIds.append(item->data(Qt::UserRole).toInt());
        }

        // 从数据库删除
        for (int noteId : noteIds) {
            QVariantMap params;
            params["id"] = noteId;
            dbManager->execute("UPDATE notes SET is_deleted = 1 WHERE id = :id", params);
        }

        // 从内存中移除
        notes.removeIf([noteIds](const NoteItem& note) {
            return noteIds.contains(note.id);
        });

        // 从列表中移除所有选中项
        qDeleteAll(selectedItems);

        // 清空编辑器
        currentNoteId = -1;
        contentEdit->clear();
        hasUnsavedChanges = false;

        updateButtonStates();
        // updateStatus(tr("已删除 %1 个笔记").arg(noteIds.size())); // 移除状态栏
    }
}

void Notepad::onShowContextMenu(const QPoint& pos) {
    QListWidgetItem* item = notesList->itemAt(pos);
    QList<QListWidgetItem*> selectedItems = notesList->selectedItems();

    QMenu contextMenu(this);

    if (!selectedItems.isEmpty()) {
        // 有选中项时显示删除选项
        QString deleteText = selectedItems.size() == 1 ? tr("🗑️ 删除笔记") : tr("🗑️ 删除选中的 %1 个笔记").arg(selectedItems.size());

        QAction* deleteAction = contextMenu.addAction(deleteText);
        connect(deleteAction, &QAction::triggered, this, &Notepad::onDeleteSelectedNotes);

        contextMenu.addSeparator();
    }

    // 总是显示新建选项
    QAction* newAction = contextMenu.addAction(tr("➕ 新建笔记"));
    connect(newAction, &QAction::triggered, this, &Notepad::onNewNote);

    // 显示菜单
    contextMenu.exec(notesList->mapToGlobal(pos));
}

void Notepad::onSaveNote() {
    saveCurrentNote();
}


void Notepad::saveCurrentNote() {
    if (currentNoteId == -1 || !hasUnsavedChanges)
        return;

    QString content = contentEdit->toHtml();
    QString plainText = contentEdit->toPlainText();
    QString title = plainText.left(50).replace('\n', ' ').trimmed();

    if (title.isEmpty()) {
        title = tr("无标题");
    }

    QString sql = "UPDATE notes SET title = :title, content = :content, modified = :modified WHERE id = :id";
    QVariantMap params;
    params["title"] = title;
    params["content"] = content;
    params["modified"] = QDateTime::currentDateTime();
    params["id"] = currentNoteId;

    SqliteWrapper::QueryResult result = dbManager->execute(sql, params);

    if (result.success && result.affectedRows > 0) {
        hasUnsavedChanges = false;

        // 更新内存中的笔记数据
        for (NoteItem& note : notes) {
            if (note.id == currentNoteId) {
                note.title = title;
                note.content = content;
                note.modified = QDateTime::currentDateTime();

                // 更新列表中的对应项
                updateNoteInList(note);
                break;
            }
        }

        // updateStatus(tr("笔记已保存")); // 移除状态栏
    } else {
        // updateStatus(tr("保存笔记失败"), true); // 移除状态栏
    }
}

void Notepad::createNewNoteFromContent() {
    // 获取当前编辑器内容
    QString content = contentEdit->toHtml();
    QString plainText = contentEdit->toPlainText();
    QString title = plainText.left(50).replace('\n', ' ').trimmed();

    if (title.isEmpty()) {
        title = tr("新笔记");
    }

    // 创建新笔记
    QString sql = "INSERT INTO notes (title, content) VALUES (:title, :content)";
    QVariantMap params;
    params["title"] = title;
    params["content"] = content;

    SqliteWrapper::QueryResult result = dbManager->execute(sql, params);

    if (result.success && result.lastInsertId > 0) {
        currentNoteId = result.lastInsertId;
        hasUnsavedChanges = false;

        // 创建新的笔记项并添加到内存列表顶部
        NoteItem newNote;
        newNote.id = result.lastInsertId;
        newNote.title = title;
        newNote.content = content;
        newNote.created = QDateTime::currentDateTime();
        newNote.modified = QDateTime::currentDateTime();
        newNote.isDeleted = false;

        // 添加到内存列表顶部
        notes.prepend(newNote);

        // 在UI列表顶部插入新项
        addNoteToList(newNote, true);

        // 选中新创建的笔记（第一个），但不触发内容加载
        disconnect(notesList, &QListWidget::currentRowChanged, this, &Notepad::onNoteSelectionChanged);
        notesList->setCurrentRow(0);
        connect(notesList, &QListWidget::currentRowChanged, this, &Notepad::onNoteSelectionChanged);

        // 延迟设置焦点，确保界面更新完成
        QTimer::singleShot(100, [this]() {
            contentEdit->setFocus();
            QTextCursor cursor = contentEdit->textCursor();
            cursor.movePosition(QTextCursor::Start);
            contentEdit->setTextCursor(cursor);
        });

        // updateStatus(tr("已自动创建新笔记")); // 移除状态栏
        updateButtonStates();
    } else {
        // updateStatus(tr("自动创建笔记失败: %1").arg(result.errorMessage), true); // 移除状态栏
    }
}

void Notepad::onAutoSaveTimer() {
    if (currentNoteId != -1 && hasUnsavedChanges) {
        // 如果有当前笔记且有未保存更改，保存现有笔记
        saveCurrentNote();
    } else if (currentNoteId == -1 && !contentEdit->toPlainText().isEmpty()) {
        // 如果没有当前笔记但编辑器有内容，创建新笔记
        createNewNoteFromContent();
    }
}

void Notepad::onImagePasted(const QPixmap& pixmap) {
    // 确保有当前笔记
    if (currentNoteId == -1) {
        // 如果没有当前笔记，先创建一个空笔记
        QString sql = "INSERT INTO notes (title, content) VALUES (:title, :content)";
        QVariantMap params;
        params["title"] = tr("新笔记");
        params["content"] = contentEdit->toHtml(); // 保存当前内容

        SqliteWrapper::QueryResult result = dbManager->execute(sql, params);

        if (result.success && result.lastInsertId > 0) {
            currentNoteId = result.lastInsertId;
            hasUnsavedChanges = false;
            loadNotes();
            updateButtonStates();
        } else {
            // updateStatus(tr("无法创建笔记以保存图片"), true); // 移除状态栏
            return;
        }
    }

    // 保存粘贴的图片到临时文件
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString fileName = QString("pasted_image_%1.png").arg(QDateTime::currentMSecsSinceEpoch());
    QString tempFilePath = QDir(tempDir).filePath(fileName);

    if (pixmap.save(tempFilePath, "PNG")) {
        // 保存到媒体存储
        QString savedPath = saveMediaFile(tempFilePath);
        if (!savedPath.isEmpty()) {
            insertImageToEditor(savedPath);
            // updateStatus(tr("已粘贴图片")); // 移除状态栏
        }
        // 清理临时文件
        QFile::remove(tempFilePath);
    } else {
        // updateStatus(tr("保存粘贴图片失败"), true); // 移除状态栏
    }
}

bool Notepad::eventFilter(QObject* obj, QEvent* event)
{
    // 当编辑器失去焦点时立刻持久化（防止 3s 自动保存窗口内的丢失）
    if (event->type() == QEvent::FocusOut && obj == contentEdit) {
        if (hasUnsavedChanges && currentNoteId != -1) {
            saveCurrentNote();
        }
    }
    return QWidget::eventFilter(obj, event);
}

void Notepad::onExportPdf()
{
    // 必要时先保存
    if (hasUnsavedChanges && currentNoteId != -1) {
        saveCurrentNote();
    }
    if (contentEdit->document()->isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("当前笔记为空，无内容可导出。"));
        return;
    }

    // 默认文件名：用当前笔记标题（如果能从 notes 表里取到）
    QString suggestedName = tr("笔记");
    for (const NoteItem& n : notes) {
        if (n.id == currentNoteId) {
            QString t = n.title.trimmed();
            if (!t.isEmpty()) suggestedName = t;
            break;
        }
    }
    QString defaultPath =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        + "/" + suggestedName + ".pdf";
    QString savePath = QFileDialog::getSaveFileName(
        this, tr("导出为 PDF"), defaultPath, tr("PDF 文件 (*.pdf)"));
    if (savePath.isEmpty()) return;

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(savePath);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageMargins(QMarginsF(20, 20, 20, 20), QPageLayout::Millimeter);

    QSizeF pageSizePx = printer.pageRect(QPrinter::DevicePixel).size();

    // 页脚：用 QFontMetricsF 在 printer 设备上拿真实像素高度，避免 12pt 文字被裁
    QFont footerFont;
    footerFont.setFamilies({defaultEditorFontFamily(),
                            QStringLiteral("PingFang SC"),
                            QStringLiteral("Microsoft YaHei"),
                            QStringLiteral("sans-serif")});
    footerFont.setPointSize(9);
    QFontMetricsF footerFm(footerFont, &printer);
    qreal footerBandH = footerFm.height() * 2.2;

    // 正文实际可用区域要预留页脚带
    QSizeF bodyPageSize(pageSizePx.width(), pageSizePx.height() - footerBandH);

    // 用编辑器文档的克隆，避免改 layout/paintDevice 影响 UI
    QTextDocument* doc = contentEdit->document()->clone();
    doc->documentLayout()->setPaintDevice(&printer);  // 关键：让 pt → 像素映射用 printer 真实 DPI

    // A4 上 12pt 文字偏小、不舒适，按比例放大每段文字与默认字号
    constexpr qreal kPdfScale = 1.4;
    {
        QFont df = doc->defaultFont();
        int pt = df.pointSize();
        if (pt <= 0) pt = 12;
        df.setPointSizeF(pt * kPdfScale);
        doc->setDefaultFont(df);

        for (auto block = doc->begin(); block.isValid(); block = block.next()) {
            for (auto it = block.begin(); !it.atEnd(); ++it) {
                QTextFragment frag = it.fragment();
                if (!frag.isValid()) continue;
                QTextCharFormat fmt = frag.charFormat();
                qreal cur = fmt.fontPointSize();
                if (cur <= 0) {
                    QFont f = fmt.font();
                    cur = f.pointSizeF();
                    if (cur <= 0) cur = 12;
                }
                fmt.setFontPointSize(cur * kPdfScale);
                QTextCursor c(doc);
                c.setPosition(frag.position());
                c.setPosition(frag.position() + frag.length(), QTextCursor::KeepAnchor);
                c.mergeCharFormat(fmt);
            }
        }
    }

    doc->setPageSize(bodyPageSize);

    // ── 收集书签条目 ──
    // 1) 优先用文档里的 heading block（headingLevel > 0）
    struct HItem { int level; QString text; int page; };
    QList<HItem> headings;
    for (auto block = doc->begin(); block.isValid(); block = block.next()) {
        int level = block.blockFormat().headingLevel();
        if (level <= 0 || level > 6) continue;
        QString text = block.text().trimmed();
        if (text.isEmpty()) continue;
        QRectF r = doc->documentLayout()->blockBoundingRect(block);
        int page = bodyPageSize.height() > 0
                       ? int(r.y() / bodyPageSize.height()) : 0;
        headings.append({level, text, page});
    }
    // 2) 没找到 heading 的话，至少给整个笔记加一条根书签（用标题）
    if (headings.isEmpty()) {
        headings.append({1, suggestedName, 0});
    }

    // ── 按页渲染：正文 + 底部页码 ──
    int totalPages = doc->pageCount();
    QPainter painter(&printer);
    for (int i = 0; i < totalPages; ++i) {
        if (i > 0) printer.newPage();

        painter.save();
        painter.translate(0, -i * bodyPageSize.height());
        QRectF clip(0, i * bodyPageSize.height(),
                    bodyPageSize.width(), bodyPageSize.height());
        painter.setClipRect(clip);
        doc->drawContents(&painter, clip);
        painter.restore();

        painter.save();
        painter.setFont(footerFont);
        painter.setPen(QColor(120, 120, 120));
        QRectF footerRect(0, pageSizePx.height() - footerBandH,
                          pageSizePx.width(), footerBandH);
        painter.drawText(footerRect, Qt::AlignCenter,
            tr("第 %1 / %2 页").arg(i + 1).arg(totalPages));
        painter.restore();
    }
    painter.end();
    delete doc;

    // ── 注入 PDF 侧边栏书签（incremental update）──
    {
        // 把扁平 heading 列表按 level 嵌套成树
        std::function<void(int,
                           QList<HItem>::const_iterator&,
                           QList<HItem>::const_iterator,
                           QList<pdfoutline::Item>&)> build;
        build = [&](int parentLevel,
                    QList<HItem>::const_iterator& it,
                    QList<HItem>::const_iterator end,
                    QList<pdfoutline::Item>& out) {
            while (it != end && it->level > parentLevel) {
                pdfoutline::Item node;
                node.title = it->text;
                node.page0 = it->page;
                int myLevel = it->level;
                ++it;
                build(myLevel, it, end, node.children);
                out.append(node);
            }
        };
        QList<pdfoutline::Item> tree;
        auto it = headings.cbegin();
        build(0, it, headings.cend(), tree);
        QString outlineErr;
        pdfoutline::inject(savePath, tree, &outlineErr);
    }

    QMessageBox box(this);
    box.setWindowTitle(tr("导出完成"));
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
}

void Notepad::onFileDropped(const QString& filePath) {
    // 确保有当前笔记
    if (currentNoteId == -1) {
        // 如果没有当前笔记，先创建一个空笔记
        QString sql = "INSERT INTO notes (title, content) VALUES (:title, :content)";
        QVariantMap params;
        params["title"] = tr("新笔记");
        params["content"] = contentEdit->toHtml(); // 保存当前内容

        SqliteWrapper::QueryResult result = dbManager->execute(sql, params);

        if (result.success && result.lastInsertId > 0) {
            currentNoteId = result.lastInsertId;
            hasUnsavedChanges = false;
            loadNotes();
            updateButtonStates();
        } else {
            // updateStatus(tr("无法创建笔记以保存文件"), true); // 移除状态栏
            return;
        }
    }

    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        // updateStatus(tr("文件不存在"), true); // 移除状态栏
        return;
    }

    // 保存文件到媒体存储
    QString savedPath = saveMediaFile(filePath);
    if (!savedPath.isEmpty()) {
        insertMediaToEditor(savedPath);
        // updateStatus(tr("已添加文件: %1").arg(fileInfo.fileName())); // 移除状态栏
    }
}

void Notepad::onInsertImage() {
    QString fileName = QFileDialog::getOpenFileName(this,
                                                    tr("选择图片"), "",
                                                    tr("图片文件 (*.png *.jpg *.jpeg *.gif *.bmp *.svg)"));

    if (!fileName.isEmpty()) {
        QString savedPath = saveMediaFile(fileName);
        if (!savedPath.isEmpty()) {
            insertImageToEditor(savedPath);
        }
    }
}

void Notepad::onInsertMedia() {
    QString fileName = QFileDialog::getOpenFileName(this,
                                                    tr("选择媒体文件"), "",
                                                    tr("媒体文件 (*.mp4 *.avi *.mov *.mp3 *.wav *.gif)"));

    if (!fileName.isEmpty()) {
        QString savedPath = saveMediaFile(fileName);
        if (!savedPath.isEmpty()) {
            insertMediaToEditor(savedPath);
        }
    }
}

QString Notepad::saveMediaFile(const QString& filePath) {
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        // updateStatus(tr("文件不存在"), true); // 移除状态栏
        return QString();
    }

    // 生成存储路径
    QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString fileName = uuid + "-" + fileInfo.fileName();

    QDateTime now = QDateTime::currentDateTime();
    QString yearMonth = now.toString("yyyy/MM");
    QString relativePath = QString("media/%1/%2").arg(yearMonth, fileName);
    QString fullPath = QString("%1/%2").arg(mediaStorageBasePath, relativePath);

    // 确保目录存在
    QFileInfo fullPathInfo(fullPath);
    createMediaDirectory(fullPathInfo.absolutePath());

    // 复制文件
    if (QFile::copy(filePath, fullPath)) {
        // 保存到数据库
        QString sql = "INSERT INTO media_files (file_name, relative_path, mime_type, file_size) VALUES (:file_name, :relative_path, :mime_type, :file_size)";
        QVariantMap params;
        params["file_name"] = fileInfo.fileName();
        params["relative_path"] = relativePath;
        params["mime_type"] = ""; // TODO: 检测MIME类型
        params["file_size"] = fileInfo.size();

        SqliteWrapper::QueryResult result = dbManager->execute(sql, params);

        if (result.success && result.lastInsertId > 0) {
            // updateStatus(tr("媒体文件已保存")); // 移除状态栏
            return fullPath;
        } else {
            QFile::remove(fullPath);
            // updateStatus(tr("保存媒体文件信息失败"), true); // 移除状态栏
        }
    } else {
        // updateStatus(tr("复制媒体文件失败"), true); // 移除状态栏
    }

    return QString();
}

void Notepad::insertImageToEditor(const QString& imagePath) {
    QTextCursor cursor = contentEdit->textCursor();

    // 获取图片的原始尺寸
    QPixmap pixmap(imagePath);
    if (pixmap.isNull()) {
        // updateStatus(tr("无法加载图片"), true); // 移除状态栏
        return;
    }

    // 计算适合的显示尺寸（最大宽度400px，保持宽高比）
    int maxWidth = 400;
    int displayWidth = pixmap.width();
    int displayHeight = pixmap.height();

    if (displayWidth > maxWidth) {
        displayHeight = (displayHeight * maxWidth) / displayWidth;
        displayWidth = maxWidth;
    }

    // 创建图片格式
    QTextImageFormat imageFormat;
    imageFormat.setName(imagePath);
    imageFormat.setWidth(displayWidth);
    imageFormat.setHeight(displayHeight);

    // 设置图片可以调整大小的属性
    imageFormat.setProperty(QTextFormat::ImageAltText, tr("图片 - 点击选中后拖拽句柄调整大小，或右键菜单，或Ctrl+滚轮缩放"));

    cursor.insertImage(imageFormat);

    // 在图片后添加一个换行
    cursor.insertText("\n");

    contentEdit->setTextCursor(cursor);

    // 标记为有未保存更改
    if (currentNoteId != -1) {
        hasUnsavedChanges = true;
        updateButtonStates();
    }
}

void Notepad::insertMediaToEditor(const QString& mediaPath) {
    QTextCursor cursor = contentEdit->textCursor();

    if (isImageFile(mediaPath)) {
        insertImageToEditor(mediaPath);
    } else {
        // 对于其他媒体文件，插入链接
        QFileInfo fileInfo(mediaPath);
        QString html = QString("<p><a href='file:///%1'>📎 %2</a> (%3)</p>")
            .arg(mediaPath, fileInfo.fileName(), formatFileSize(fileInfo.size()));
        cursor.insertHtml(html);
    }

    contentEdit->setTextCursor(cursor);
}

QString Notepad::getMediaStoragePath() {
    return mediaStorageBasePath;
}

void Notepad::createMediaDirectory(const QString& path) {
    QDir dir;
    if (!dir.exists(path)) {
        dir.mkpath(path);
    }
}

bool Notepad::isImageFile(const QString& filePath) {
    QStringList imageExtensions = { "png", "jpg", "jpeg", "gif", "bmp", "svg" };
    QFileInfo fileInfo(filePath);
    return imageExtensions.contains(fileInfo.suffix().toLower());
}

bool Notepad::isVideoFile(const QString& filePath) {
    QStringList videoExtensions = { "mp4", "avi", "mov", "mkv", "wmv" };
    QFileInfo fileInfo(filePath);
    return videoExtensions.contains(fileInfo.suffix().toLower());
}

bool Notepad::isAudioFile(const QString& filePath) {
    QStringList audioExtensions = { "mp3", "wav", "aac", "ogg", "flac" };
    QFileInfo fileInfo(filePath);
    return audioExtensions.contains(fileInfo.suffix().toLower());
}

QString Notepad::formatFileSize(qint64 bytes) {
    if (bytes < 1024)
        return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QString("%1 KB").arg(bytes / 1024);
    if (bytes < 1024 * 1024 * 1024)
        return QString("%1 MB").arg(bytes / (1024 * 1024));
    return QString("%1 GB").arg(bytes / (1024 * 1024 * 1024));
}

void Notepad::onFontChanged() {
    QTextCursor cursor = contentEdit->textCursor();
    QTextCharFormat format = cursor.charFormat();
    format.setFontFamily(fontCombo->currentFont().family());
    cursor.setCharFormat(format);
}

void Notepad::onFontSizeChanged() {
    // 用 mergeCurrentCharFormat 一步到位：
    // - 有选区时合并到选区的 char format
    // - 无选区时合并到 cursor 的 "current char format"（即下一个字符的格式）
    // 这样按回车后新段落继承的就是工具栏上的字号，不会再"时大时小"
    QTextCharFormat fmt;
    fmt.setFontPointSize(fontSizeSpinBox->value());
    contentEdit->mergeCurrentCharFormat(fmt);
}

// 光标位置变化时把工具栏字号同步成 cursor 当前 char format 的字号 —
// 让"工具栏显示的字号" 始终等于"下一个字符的字号"，避免用户误判
void Notepad::onCursorFormatChanged(const QTextCharFormat& fmt) {
    qreal pt = fmt.fontPointSize();
    if (pt <= 0) return;
    int v = int(pt + 0.5);
    if (v < 20) v = 20;     // 工具栏也强制 ≥20
    QSignalBlocker block(fontSizeSpinBox);
    fontSizeSpinBox->setValue(v);
}

// 全局最小字号规范化：遍历文档每个 block 的每个 fragment，凡是 fontPointSize < 20
// 都强制 mergeCharFormat 升到 20pt。处理三种来源：
//   1. 粘贴进来的小字体（如从 13pt 的 Word/网页粘进来）
//   2. 旧笔记加载时的历史字号
//   3. 任何意外的小字号设置
// 走 textChanged 触发 + 200ms 防抖，输入流畅不卡。
// 用 suppressNormalize 标志防止自己引起的 textChanged 反复触发死循环。
void Notepad::normalizeMinFontSizes() {
    if (!contentEdit || suppressNormalize) return;
    // 🔒 终极锁定：把整篇文档每个字符的字号统统设为 20pt，
    // 同时清掉 fontPixelSize / fontFamily 覆盖等可能干扰显示尺寸的属性。
    // selectAll + mergeCharFormat(只动 font) — 保留 B / I / 颜色 / 链接等其它属性。
    suppressNormalize = true;
    QTextCursor cur(contentEdit->document());
    cur.beginEditBlock();
    cur.select(QTextCursor::Document);
    QTextCharFormat fmt;
    QFont f = contentEdit->font();
    f.setPointSize(20);
    fmt.setFont(f);
    cur.mergeCharFormat(fmt);
    cur.endEditBlock();
    suppressNormalize = false;
}

// 应急按钮：把整篇内容统一成 20pt 纯字，去掉所有格式（B/I/U/颜色/字体名）
void Notepad::onCleanFormat() {
    if (!contentEdit) return;
    suppressNormalize = true;
    QTextCursor cur = contentEdit->textCursor();
    int oldPos = cur.position();
    cur.select(QTextCursor::Document);
    QTextCharFormat clean;
    clean.setFont(contentEdit->font());     // widget 字体（20pt）
    cur.setCharFormat(clean);                // 整覆盖，所有 char-level 格式清光
    cur.clearSelection();
    cur.setPosition(qMin(oldPos, contentEdit->document()->characterCount() - 1));
    contentEdit->setTextCursor(cur);
    suppressNormalize = false;
}

// 在光标处插入一条水平分割线（独立成段）
void Notepad::onInsertHr() {
    QTextCursor cursor = contentEdit->textCursor();
    cursor.beginEditBlock();
    // 如果当前不在段首，先 break 一段
    if (!cursor.atBlockStart()) cursor.insertBlock();
    cursor.insertHtml("<hr>");
    // 让分割线后另起一段，并继承工具栏字号
    cursor.insertBlock();
    QTextCharFormat fmt;
    fmt.setFontPointSize(fontSizeSpinBox->value());
    cursor.mergeCharFormat(fmt);
    contentEdit->mergeCurrentCharFormat(fmt);
    cursor.endEditBlock();
    contentEdit->setFocus();
}

void Notepad::onBoldClicked() {
    QTextCursor cursor = contentEdit->textCursor();
    QTextCharFormat format = cursor.charFormat();
    format.setFontWeight(boldBtn->isChecked() ? QFont::Bold : QFont::Normal);
    cursor.setCharFormat(format);
}

void Notepad::onItalicClicked() {
    QTextCursor cursor = contentEdit->textCursor();
    QTextCharFormat format = cursor.charFormat();
    format.setFontItalic(italicBtn->isChecked());
    cursor.setCharFormat(format);
}

void Notepad::onUnderlineClicked() {
    QTextCursor cursor = contentEdit->textCursor();
    QTextCharFormat format = cursor.charFormat();
    format.setFontUnderline(underlineBtn->isChecked());
    cursor.setCharFormat(format);
}

void Notepad::onTextColorClicked() {
    QColor color = QColorDialog::getColor(Qt::black, this, tr("选择字体颜色"));
    if (color.isValid()) {
        QTextCursor cursor = contentEdit->textCursor();
        QTextCharFormat format = cursor.charFormat();
        format.setForeground(color);
        cursor.setCharFormat(format);
    }
}

void Notepad::onBackgroundColorClicked() {
    QColor color = QColorDialog::getColor(Qt::white, this, tr("选择背景颜色"));
    if (color.isValid()) {
        QTextCursor cursor = contentEdit->textCursor();
        QTextCharFormat format = cursor.charFormat();
        format.setBackground(color);
        cursor.setCharFormat(format);
    }
}

void Notepad::onAlignLeftClicked() {
    contentEdit->setAlignment(Qt::AlignLeft);
    alignLeftBtn->setChecked(true);
    alignCenterBtn->setChecked(false);
    alignRightBtn->setChecked(false);
    alignJustifyBtn->setChecked(false);
}

void Notepad::onAlignCenterClicked() {
    contentEdit->setAlignment(Qt::AlignCenter);
    alignLeftBtn->setChecked(false);
    alignCenterBtn->setChecked(true);
    alignRightBtn->setChecked(false);
    alignJustifyBtn->setChecked(false);
}

void Notepad::onAlignRightClicked() {
    contentEdit->setAlignment(Qt::AlignRight);
    alignLeftBtn->setChecked(false);
    alignCenterBtn->setChecked(false);
    alignRightBtn->setChecked(true);
    alignJustifyBtn->setChecked(false);
}

void Notepad::onAlignJustifyClicked() {
    contentEdit->setAlignment(Qt::AlignJustify);
    alignLeftBtn->setChecked(false);
    alignCenterBtn->setChecked(false);
    alignRightBtn->setChecked(false);
    alignJustifyBtn->setChecked(true);
}

void Notepad::updateButtonStates() {
    bool hasNote = (currentNoteId != -1);

    // 编辑相关按钮 - 始终启用以支持创建新笔记
    fontCombo->setEnabled(true);
    fontSizeSpinBox->setEnabled(true);
    boldBtn->setEnabled(true);
    italicBtn->setEnabled(true);
    underlineBtn->setEnabled(true);
    textColorBtn->setEnabled(true);
    bgColorBtn->setEnabled(true);
    alignLeftBtn->setEnabled(true);
    alignCenterBtn->setEnabled(true);
    alignRightBtn->setEnabled(true);
    alignJustifyBtn->setEnabled(true);
    insertImageBtn->setEnabled(hasNote); // 只有选中笔记时才能插入媒体
    insertMediaBtn->setEnabled(hasNote);
    saveBtn->setEnabled(hasNote && hasUnsavedChanges);

    // 编辑器始终启用
    contentEdit->setEnabled(true);

    // 删除按钮只有选中笔记时才启用
    deleteNoteBtn->setEnabled(hasNote);
}

// void Notepad::updateStatus(const QString& message, bool isError) {
//     statusLabel->setText(message);
//     statusLabel->setStyleSheet(isError ? "color: #dc3545; font-weight: bold;" : "color: #28a745; font-weight: bold;");
// } // 移除状态栏

