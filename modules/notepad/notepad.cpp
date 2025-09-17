#include "notepad.h"
#include <QDebug>
#include <QStandardPaths>
#include <QSqlQuery>
#include <QSqlError>
#include <QTextCharFormat>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QBuffer>
#include <QScrollBar>

REGISTER_DYNAMICOBJECT(Notepad);

// RichTextEdit 实现
RichTextEdit::RichTextEdit(QWidget* parent) : QTextEdit(parent),
                                              m_isDragging(false), m_currentHandle(None) {
    setAcceptDrops(true);
    setMouseTracking(true); // 启用鼠标跟踪
}

void RichTextEdit::keyPressEvent(QKeyEvent* event) {
    // 处理 Ctrl+V 粘贴
    if (event->matches(QKeySequence::Paste)) {
        if (const QMimeData* mimeData = QApplication::clipboard()->mimeData()) {
            insertFromMimeData(mimeData);
            return;
        }
    }

    QTextEdit::keyPressEvent(event);
}

void RichTextEdit::insertFromMimeData(const QMimeData* source) {
    // 检查是否包含图片
    if (source->hasImage()) {
        if (const auto pixmap = qvariant_cast<QPixmap>(source->imageData()); !pixmap.isNull()) {
            emit imageDropped(pixmap);
            return;
        }
    }

    // 检查是否包含文件路径
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

    // 默认处理文本
    QTextEdit::insertFromMimeData(source);
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
        // 计算新的图片尺寸
        QPoint delta = event->pos() - m_dragStartPos;
        QRect newRect = m_originalImageRect;

        switch (m_currentHandle) {
        case TopLeft:
            newRect.setTopLeft(newRect.topLeft() + delta);
            break;
        case TopRight:
            newRect.setTopRight(newRect.topRight() + delta);
            break;
        case BottomLeft:
            newRect.setBottomLeft(newRect.bottomLeft() + delta);
            break;
        case BottomRight:
            newRect.setBottomRight(newRect.bottomRight() + delta);
            break;
        case Top:
            newRect.setTop(newRect.top() + delta.y());
            break;
        case Bottom:
            newRect.setBottom(newRect.bottom() + delta.y());
            break;
        case Left:
            newRect.setLeft(newRect.left() + delta.x());
            break;
        case Right:
            newRect.setRight(newRect.right() + delta.x());
            break;
        default:
            break;
        }

        // 限制最小尺寸
        if (newRect.width() < 50)
            newRect.setWidth(50);
        if (newRect.height() < 50)
            newRect.setHeight(50);

        // 限制最大尺寸
        if (newRect.width() > 800)
            newRect.setWidth(800);
        if (newRect.height() > 600)
            newRect.setHeight(600);

        // 更新图片格式
        QTextCursor editCursor = m_selectedImageCursor;
        editCursor.select(QTextCursor::WordUnderCursor);

        QTextImageFormat newFormat = m_selectedImageFormat;
        newFormat.setWidth(newRect.width());
        newFormat.setHeight(newRect.height());

        editCursor.setCharFormat(newFormat);

        // 发出图片调整信号
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

    // 获取光标在视口中的位置
    QRect cRect = cursorRect(cursor);

    // 使用格式中的尺寸，如果没有则使用原始图片尺寸
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
    const int handleSize = 8;
    const int margin = 4;

    // 检查角落
    if (QRect(imageRect.topLeft() - QPoint(margin, margin), QSize(handleSize, handleSize)).contains(pos))
        return TopLeft;
    if (QRect(imageRect.topRight() - QPoint(handleSize - margin, margin), QSize(handleSize, handleSize)).contains(pos))
        return TopRight;
    if (QRect(imageRect.bottomLeft() - QPoint(margin, handleSize - margin), QSize(handleSize, handleSize)).contains(pos))
        return BottomLeft;
    if (QRect(imageRect.bottomRight() - QPoint(handleSize - margin, handleSize - margin), QSize(handleSize, handleSize)).contains(pos))
        return BottomRight;

    // 检查边缘
    if (QRect(imageRect.left() - margin, imageRect.top() + handleSize, handleSize, imageRect.height() - 2 * handleSize).contains(pos))
        return Left;
    if (QRect(imageRect.right() - margin, imageRect.top() + handleSize, handleSize, imageRect.height() - 2 * handleSize).contains(pos))
        return Right;
    if (QRect(imageRect.left() + handleSize, imageRect.top() - margin, imageRect.width() - 2 * handleSize, handleSize).contains(pos))
        return Top;
    if (QRect(imageRect.left() + handleSize, imageRect.bottom() - margin, imageRect.width() - 2 * handleSize, handleSize).contains(pos))
        return Bottom;

    return None;
}

void RichTextEdit::drawResizeHandles(QPainter& painter, const QRect& imageRect) {
    const int handleSize = 8;
    const int margin = 4;

    painter.setPen(QPen(Qt::blue, 2));
    painter.setBrush(Qt::white);

    // 绘制角落句柄
    painter.drawRect(QRect(imageRect.topLeft() - QPoint(margin, margin), QSize(handleSize, handleSize)));
    painter.drawRect(QRect(imageRect.topRight() - QPoint(handleSize - margin, margin), QSize(handleSize, handleSize)));
    painter.drawRect(QRect(imageRect.bottomLeft() - QPoint(margin, handleSize - margin), QSize(handleSize, handleSize)));
    painter.drawRect(QRect(imageRect.bottomRight() - QPoint(handleSize - margin, handleSize - margin), QSize(handleSize, handleSize)));

    // 绘制边缘句柄
    painter.drawRect(QRect(imageRect.left() - margin, imageRect.center().y() - handleSize / 2, handleSize, handleSize));
    painter.drawRect(QRect(imageRect.right() - margin, imageRect.center().y() - handleSize / 2, handleSize, handleSize));
    painter.drawRect(QRect(imageRect.center().x() - handleSize / 2, imageRect.top() - margin, handleSize, handleSize));
    painter.drawRect(QRect(imageRect.center().x() - handleSize / 2, imageRect.bottom() - margin, handleSize, handleSize));
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

    // 设置自动保存定时器
    autoSaveTimer = new QTimer(this);
    autoSaveTimer->setSingleShot(false); // 设置为重复定时器
    connect(autoSaveTimer, &QTimer::timeout, this, &Notepad::onAutoSaveTimer);
    autoSaveTimer->start(10 * 1000); // 每3秒检查一次

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
    contentEdit->setPlaceholderText(tr("开始编写你的笔记..."));
    contentEdit->setAcceptRichText(true);
    contentEdit->setStyleSheet(
        "QTextEdit {"
        "    border: 1px solid #e0e0e0;"
        "    border-left: none;"
        "    padding: 8px;"
        "    font-family: 'Consolas', 'Monaco', monospace;"
        "    font-size: 12px;"
        "    line-height: 1.4;"
        "}"
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
    connect(contentEdit, &QTextEdit::textChanged, this, [this]() {
        if (currentNoteId != -1) {
            hasUnsavedChanges = true;
            updateButtonStates();
        }


    });

    // 添加到容器
    editorContainerLayout->addWidget(contentEdit);

    editorLayout->addWidget(toolbar);
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
    toolbar = new QToolBar();
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    // 字体设置
    fontCombo = new QFontComboBox();
    fontCombo->setCurrentFont(QFont("Microsoft YaHei"));
    connect(fontCombo, &QFontComboBox::currentFontChanged, this, &Notepad::onFontChanged);

    fontSizeSpinBox = new QSpinBox();
    fontSizeSpinBox->setRange(8, 72);
    fontSizeSpinBox->setValue(12);
    fontSizeSpinBox->setSuffix(tr("pt"));
    connect(fontSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &Notepad::onFontSizeChanged);

    // 文本样式按钮
    boldBtn = new QPushButton(tr("B"));
    boldBtn->setCheckable(true);
    boldBtn->setStyleSheet("QPushButton { font-weight: bold; }");
    connect(boldBtn, &QPushButton::clicked, this, &Notepad::onBoldClicked);

    italicBtn = new QPushButton(tr("I"));
    italicBtn->setCheckable(true);
    italicBtn->setStyleSheet("QPushButton { font-style: italic; }");
    connect(italicBtn, &QPushButton::clicked, this, &Notepad::onItalicClicked);

    underlineBtn = new QPushButton(tr("U"));
    underlineBtn->setCheckable(true);
    underlineBtn->setStyleSheet("QPushButton { text-decoration: underline; }");
    connect(underlineBtn, &QPushButton::clicked, this, &Notepad::onUnderlineClicked);

    // 颜色按钮
    textColorBtn = new QPushButton(tr("🎨 字体颜色"));
    connect(textColorBtn, &QPushButton::clicked, this, &Notepad::onTextColorClicked);

    bgColorBtn = new QPushButton(tr("🖍️ 背景颜色"));
    connect(bgColorBtn, &QPushButton::clicked, this, &Notepad::onBackgroundColorClicked);

    // 对齐按钮
    alignLeftBtn = new QPushButton(tr("⬅️"));
    alignLeftBtn->setCheckable(true);
    alignLeftBtn->setChecked(true);
    connect(alignLeftBtn, &QPushButton::clicked, this, &Notepad::onAlignLeftClicked);

    alignCenterBtn = new QPushButton(tr("⬅️➡️"));
    alignCenterBtn->setCheckable(true);
    connect(alignCenterBtn, &QPushButton::clicked, this, &Notepad::onAlignCenterClicked);

    alignRightBtn = new QPushButton(tr("➡️"));
    alignRightBtn->setCheckable(true);
    connect(alignRightBtn, &QPushButton::clicked, this, &Notepad::onAlignRightClicked);

    alignJustifyBtn = new QPushButton(tr("⬅️➡️"));
    alignJustifyBtn->setCheckable(true);
    connect(alignJustifyBtn, &QPushButton::clicked, this, &Notepad::onAlignJustifyClicked);

    // 媒体插入按钮
    insertImageBtn = new QPushButton(tr("🖼️ 插入图片"));
    connect(insertImageBtn, &QPushButton::clicked, this, &Notepad::onInsertImage);

    insertMediaBtn = new QPushButton(tr("🎬 插入媒体"));
    connect(insertMediaBtn, &QPushButton::clicked, this, &Notepad::onInsertMedia);

    // 保存按钮
    saveBtn = new QPushButton(tr("💾 保存"));
    saveBtn->setStyleSheet("QPushButton { background-color: #007bff; color: white; } QPushButton:hover { background-color: #0056b3; }");
    connect(saveBtn, &QPushButton::clicked, this, &Notepad::onSaveNote);

    // 添加到工具栏
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
    toolbar->addSeparator();
    toolbar->addWidget(alignLeftBtn);
    toolbar->addWidget(alignCenterBtn);
    toolbar->addWidget(alignRightBtn);
    toolbar->addWidget(alignJustifyBtn);
    toolbar->addSeparator();
    toolbar->addWidget(insertImageBtn);
    toolbar->addWidget(insertMediaBtn);
    toolbar->addSeparator();
    toolbar->addWidget(saveBtn);
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
        // 多选时，清空编辑器
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
    QTextCursor cursor = contentEdit->textCursor();
    QTextCharFormat format = cursor.charFormat();
    format.setFontPointSize(fontSizeSpinBox->value());
    cursor.setCharFormat(format);
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

