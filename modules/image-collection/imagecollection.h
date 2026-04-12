#ifndef IMAGECOLLECTION_H
#define IMAGECOLLECTION_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QListWidget>
#include <QStyledItemDelegate>
#include <QHash>
#include <QPixmap>
#include <QStringList>
#include <QDateTime>
#include <QMenu>
#include <QFileDialog>
#include <QMessageBox>
#include <QImageReader>
#include <QUuid>
#include <QDesktopServices>
#include <QClipboard>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QPainter>
#include <QApplication>

#include "../../common/dynamicobjectbase.h"
#include "../../common/sqlite/SqliteManager.h"

struct ImageInfo {
    int id = 0;
    QString fileName;
    QString storedName;
    qint64 fileSize = 0;
    int width = 0;
    int height = 0;
    QStringList tags;
    QString notes;
    bool isDeleted = false;
    QDateTime createdAt;
    QDateTime deletedAt;
};

Q_DECLARE_METATYPE(ImageInfo)

class ImageCardDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit ImageCardDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

    void setThumbnailCache(QHash<QString, QPixmap>* cache) { m_cache = cache; }

private:
    QHash<QString, QPixmap>* m_cache = nullptr;

    struct TagColor {
        QColor background;
        QColor foreground;
    };
    static QHash<QString, TagColor> tagColors();
};

class ImageCollection : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit ImageCollection();
    ~ImageCollection() override;

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onAddImages();
    void onPasteFromClipboard();
    void onToggleRecycleBin();
    void onSearchTextChanged(const QString& text);
    void onTagFilterChanged(int index);
    void onItemDoubleClicked(QListWidgetItem* item);
    void onItemContextMenu(const QPoint& pos);
    void onSoftDelete();
    void onRestore();
    void onPermanentDelete();
    void onEmptyRecycleBin();
    void onEditTags();
    void onOpenInExplorer();

private:
    void setupUI();
    void initDatabase();
    void loadImages();
    void importImage(const QString& filePath);
    QPixmap getThumbnail(const QString& storedName, int maxWidth, int maxHeight);
    QString storagePath() const;
    void updateStatusBar();
    QString formatFileSize(qint64 bytes) const;
    ImageInfo imageInfoFromRecord(const QVariantMap& record) const;

    // Predefined tags
    static const QStringList predefinedTags;

    // UI
    QVBoxLayout* m_mainLayout = nullptr;
    QHBoxLayout* m_toolbarLayout = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QComboBox* m_tagFilter = nullptr;
    QPushButton* m_addBtn = nullptr;
    QPushButton* m_pasteBtn = nullptr;
    QPushButton* m_recycleBinBtn = nullptr;
    QListWidget* m_listWidget = nullptr;
    QLabel* m_statusLabel = nullptr;

    // State
    bool m_showingRecycleBin = false;
    QHash<QString, QPixmap> m_thumbnailCache;
    ImageCardDelegate* m_delegate = nullptr;
    SqliteWrapper::SqliteManager* m_db = nullptr;
};

#endif // IMAGECOLLECTION_H
