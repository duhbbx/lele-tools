#ifndef PDFSTAMP_H
#define PDFSTAMP_H

#include <QWidget>
#include <QVector>
#include <QImage>
#include <QPixmap>
#include <QString>
#include <QPoint>
#include <QPointer>

#include "../../common/dynamicobjectbase.h"

#ifdef WITH_QT_PDF
#include <QPdfDocument>
#endif

class QPushButton;
class QLabel;
class QVBoxLayout;
class QHBoxLayout;
class QScrollArea;
class QListWidget;
class QListWidgetItem;
class QComboBox;
class QLineEdit;

namespace pdfstamp {

// 一个浮动覆盖物：图章 / 签名 / 日期文字
class OverlayItem : public QWidget
{
    Q_OBJECT
public:
    enum Kind { Stamp, Signature, DateText };

    OverlayItem(Kind kind, QWidget* parent);

    // 图片型（图章/签名）
    void setImage(const QImage& img);
    QImage image() const { return m_image; }

    // 文字型（日期）
    void setText(const QString& text);
    QString text() const { return m_text; }

    Kind kind() const { return m_kind; }

    // 缩放因子（相对原始尺寸）
    void setScale(double s);
    double scale() const { return m_scale; }

    // 选中状态
    bool isSelected() const { return m_selected; }
    void setSelected(bool s);

signals:
    void clicked(OverlayItem* self);

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;

private:
    void recomputeSize();

    Kind m_kind;
    QImage m_image;
    QString m_text;
    double m_scale = 1.0;
    bool m_dragging = false;
    bool m_selected = false;
    QPoint m_dragStart;
};

// 单个PDF页面的视图（渲染后的位图 + 可放置覆盖物）
class PdfPageView : public QWidget
{
    Q_OBJECT
public:
    explicit PdfPageView(int pageIndex, const QImage& rendered, QWidget* parent = nullptr);

    int pageIndex() const { return m_pageIndex; }
    QImage rendered() const { return m_rendered; }

    // 添加覆盖物（位置以view坐标）
    OverlayItem* addOverlay(OverlayItem::Kind kind, const QImage& img, const QPoint& centerPos);
    OverlayItem* addDateOverlay(const QString& text, const QPoint& centerPos);

    QList<OverlayItem*> overlays() const;

    static constexpr const char* kAssetMime = "application/x-pdf-stamp-asset";

signals:
    // 点击空白（非覆盖物）位置：用于放置模式或取消选中
    void emptyClicked(PdfPageView* self, const QPoint& posOnPage);
    // 点击了某个覆盖物（用于选中）
    void overlayClicked(PdfPageView* self, OverlayItem* item);
    // 来自外部（资源列表）拖入的资产被放下
    void assetDropped(PdfPageView* self, const QString& kindStr, const QString& path, const QPoint& posOnPage);

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dragMoveEvent(QDragMoveEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private:
    int m_pageIndex;
    QImage m_rendered;
};

} // namespace pdfstamp


class PdfStamp : public QWidget, public DynamicObjectBase
{
    Q_OBJECT
public:
    explicit PdfStamp();
    ~PdfStamp() override;

private slots:
    void onOpenPdf();
    void onSaveAs();
    void onAddStampFromFile();
    void onAddSignatureFromFile();
    void onDrawSignature();
    void onUseSelectedAsset();
    void onRemoveSelectedAsset();
    void onAddDate();
    void onRemoveSelectedOverlay();

    // 放置/选中处理
    void onPageEmptyClicked(pdfstamp::PdfPageView* page, const QPoint& posOnPage);
    void onOverlayClicked(pdfstamp::PdfPageView* page, pdfstamp::OverlayItem* item);
    void onAssetDropped(pdfstamp::PdfPageView* page, const QString& kindStr,
                        const QString& path, const QPoint& posOnPage);
    void cancelPlaceMode();

private:
    void setupUi();
    void renderAllPages();
    pdfstamp::PdfPageView* currentPageView() const;

    // 资源管理
    void reloadAssets();
    QString assetsDir() const;
    void saveAssetCopy(const QString& srcPath, const QString& kind);  // kind: stamp/signature
    void saveAssetImage(const QImage& img, const QString& kind);

    // UI
    QPushButton* m_openBtn = nullptr;
    QPushButton* m_saveAsBtn = nullptr;
    QPushButton* m_addStampBtn = nullptr;
    QPushButton* m_addSignBtn = nullptr;
    QPushButton* m_drawSignBtn = nullptr;
    QPushButton* m_addDateBtn = nullptr;
    QPushButton* m_useAssetBtn = nullptr;
    QPushButton* m_delAssetBtn = nullptr;

    QListWidget* m_assetList = nullptr;
    QComboBox* m_dateFormatCombo = nullptr;
    QLineEdit* m_customDateEdit = nullptr;

    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_pagesContainer = nullptr;
    QVBoxLayout* m_pagesLayout = nullptr;
    QLabel* m_statusLabel = nullptr;

    QString m_currentPdfPath;
    QVector<QPointer<pdfstamp::PdfPageView>> m_pageViews;

    // 放置模式状态
    enum PlaceMode { PlaceNone, PlaceImage, PlaceDate };
    PlaceMode m_placeMode = PlaceNone;
    pdfstamp::OverlayItem::Kind m_pendingKind = pdfstamp::OverlayItem::Stamp;
    QImage m_pendingImage;
    QString m_pendingText;

    QPointer<pdfstamp::OverlayItem> m_selectedOverlay;
    QPushButton* m_delOverlayBtn = nullptr;

    void enterPlaceImageMode(pdfstamp::OverlayItem::Kind kind, const QImage& img);
    void enterPlaceDateMode(const QString& text);
    void selectOverlay(pdfstamp::OverlayItem* it);

#ifdef WITH_QT_PDF
    QPdfDocument* m_pdfDoc = nullptr;
#endif
};

#endif // PDFSTAMP_H
