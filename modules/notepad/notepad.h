#ifndef NOTEPAD_H
#define NOTEPAD_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QListWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QToolBar>
#include <QAction>
#include <QComboBox>
#include <QSpinBox>
#include <QColorDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QTimer>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QUrl>
#include <QUuid>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QMovie>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTextImageFormat>
#include <QFont>
#include <QFontComboBox>
#include <QColorDialog>
#include <QProgressBar>
#include <QListWidgetItem>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QShortcut>
#include <QShowEvent>
#include <QHideEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QPaintEvent>
#include <QPainter>
#include <QRect>

#include "../../common/dynamicobjectbase.h"
#include "../../common/sqlite/SqliteManager.h"

// 自定义 QTextEdit 支持粘贴图片和图片缩放
class RichTextEdit : public QTextEdit
{
    Q_OBJECT

public:
    explicit RichTextEdit(QWidget* parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void insertFromMimeData(const QMimeData* source) override;
    void wheelEvent(QWheelEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

signals:
    void imageDropped(const QPixmap& pixmap);
    void fileDropped(const QString& filePath);
    void imageResized(); // 图片大小改变信号

private:
    static QString savePixmapToFile(const QPixmap& pixmap);
    QTextImageFormat getImageFormatAtCursor(const QTextCursor& cursor);
    void resizeImageAtCursor(const QTextCursor& cursor, double scaleFactor);
    void showImageContextMenu(const QPoint& pos, const QTextCursor& cursor);
    void resizeImageToWidth(const QTextCursor& cursor, int targetWidth);
    void resizeImageToOriginal(const QTextCursor& cursor);
    
    // 图片拖拽调整相关
    QRect getImageRect(const QTextCursor& cursor);
    enum ResizeHandle { None, TopLeft, TopRight, BottomLeft, BottomRight, Top, Bottom, Left, Right };
    ResizeHandle getResizeHandle(const QPoint& pos, const QRect& imageRect);
    void drawResizeHandles(QPainter& painter, const QRect& imageRect);
    void setCursorForHandle(ResizeHandle handle);
    
    // 拖拽状态
    bool m_isDragging;
    ResizeHandle m_currentHandle;
    QPoint m_dragStartPos;
    QRect m_originalImageRect;
    QTextCursor m_selectedImageCursor;
    QTextImageFormat m_selectedImageFormat;
};

struct NoteItem {
    int id;
    QString title;
    QString content;
    QDateTime created;
    QDateTime modified;
    bool isDeleted;
    
    NoteItem() : id(-1), isDeleted(false) {}
    NoteItem(int _id, const QString& _title, const QString& _content, 
             const QDateTime& _created, const QDateTime& _modified, bool _deleted = false)
        : id(_id), title(_title), content(_content), created(_created), modified(_modified), isDeleted(_deleted) {}
};

struct MediaItem {
    int id;
    QString fileName;
    QString relativePath;
    QString mimeType;
    qint64 fileSize;
    QDateTime created;
    
    MediaItem() : id(-1), fileSize(0) {}
    MediaItem(int _id, const QString& _fileName, const QString& _relativePath,
              const QString& _mimeType, qint64 _fileSize, const QDateTime& _created)
        : id(_id), fileName(_fileName), relativePath(_relativePath), 
          mimeType(_mimeType), fileSize(_fileSize), created(_created) {}
};

class Notepad : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit Notepad();
    ~Notepad();

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onSearchTextChanged();
    void onNoteSelectionChanged();
    void onItemSelectionChanged();
    void onNewNote();
    void onDeleteNote();
    void onDeleteSelectedNotes();
    void onShowContextMenu(const QPoint& pos);
    void onSaveNote();
    void updateLineNumbers();
    void onInsertImage();
    void onInsertMedia();
    void onFontChanged();
    void onFontSizeChanged();
    void onBoldClicked();
    void onItalicClicked();
    void onUnderlineClicked();
    void onTextColorClicked();
    void onBackgroundColorClicked();
    void onAlignLeftClicked();
    void onAlignCenterClicked();
    void onAlignRightClicked();
    void onAlignJustifyClicked();
    void onAutoSaveTimer();
    void onImagePasted(const QPixmap& pixmap);
    void onFileDropped(const QString& filePath);

private:
    void setupUI();
    void setupNoteListArea();
    void setupEditorArea();
    void setupToolbar();
    void setupStatusBar();
    void setupShortcuts();
    void initializeDatabase();
    void loadNotes();
    void loadNoteContent(int noteId);
    void saveCurrentNote();
    void createNewNoteFromContent();
    void updateNotesList();
    void addNoteToList(const NoteItem& note, bool insertAtTop = true);
    void updateNoteInList(const NoteItem& note);
    void removeNoteFromList(int noteId);
    void updateButtonStates();
    // void updateStatus(const QString& message, bool isError = false); // 移除状态栏
    QString saveMediaFile(const QString& filePath);
    QString getMediaStoragePath();
    void createMediaDirectory(const QString& path);
    bool isImageFile(const QString& filePath);
    bool isVideoFile(const QString& filePath);
    bool isAudioFile(const QString& filePath);
    void insertImageToEditor(const QString& imagePath);
    void insertMediaToEditor(const QString& mediaPath);
    QString formatFileSize(qint64 bytes);
    
    // UI 组件
    QVBoxLayout* mainLayout;
    QSplitter* mainSplitter;
    
    // 左侧笔记列表区域
    QWidget* noteListWidget;
    QVBoxLayout* noteListLayout;
    QLineEdit* searchEdit;
    QListWidget* notesList;
    QPushButton* newNoteBtn;
    QPushButton* deleteNoteBtn;
    
    // 右侧编辑区域
    QWidget* editorWidget;
    QVBoxLayout* editorLayout;
    QToolBar* toolbar;
    QWidget* editorContainer;  // 包含行号和编辑器的容器
    QHBoxLayout* editorContainerLayout;
    QTextEdit* lineNumberArea;  // 行号区域
    RichTextEdit* contentEdit;
    
    // 工具栏按钮和控件
    QFontComboBox* fontCombo;
    QSpinBox* fontSizeSpinBox;
    QPushButton* boldBtn;
    QPushButton* italicBtn;
    QPushButton* underlineBtn;
    QPushButton* textColorBtn;
    QPushButton* bgColorBtn;
    QPushButton* alignLeftBtn;
    QPushButton* alignCenterBtn;
    QPushButton* alignRightBtn;
    QPushButton* alignJustifyBtn;
    QPushButton* insertImageBtn;
    QPushButton* insertMediaBtn;
    QPushButton* saveBtn;
    
    // 数据相关
    SqliteWrapper::SqliteManager* dbManager;
    QList<NoteItem> notes;
    int currentNoteId;
    bool hasUnsavedChanges;
    QTimer* autoSaveTimer;
    QString mediaStorageBasePath;
    
    // 快捷键
    QShortcut* newNoteShortcut;
};

#endif // NOTEPAD_H
