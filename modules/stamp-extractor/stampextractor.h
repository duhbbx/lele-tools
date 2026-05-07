#ifndef STAMPEXTRACTOR_H
#define STAMPEXTRACTOR_H

#include <QWidget>
#include <QImage>
#include <QColor>
#include <QPainterPath>
#include <QPolygonF>
#include <QPointF>

#include "../../common/dynamicobjectbase.h"

class QLabel;
class QPushButton;
class QSlider;
class QCheckBox;

namespace stampextractor {

// 可缩放、可平移、可套索的图片视图
class ImageView : public QWidget
{
    Q_OBJECT
public:
    explicit ImageView(QWidget* parent = nullptr);

    void setImage(const QImage& img);  // 自动调用 fitToWidget
    QImage image() const { return m_image; }

    double zoom() const { return m_zoom; }
    QPointF offset() const { return m_offset; }
    void setView(double zoom, QPointF offset);
    void fitToWidget();

    void setShowCheckerboard(bool b);

    // 套索
    bool lassoMode() const { return m_lassoMode; }
    void setLassoMode(bool on);
    void clearLasso();
    QPainterPath lassoPath() const { return m_lassoPath; }   // image coords
    bool hasLasso() const { return !m_lassoPath.isEmpty(); }

    // 占位文字（无图时显示）
    void setPlaceholder(const QString& text);

signals:
    void viewChanged(double zoom, QPointF offset);
    void lassoCompleted(const QPainterPath& imagePath);

protected:
    void paintEvent(QPaintEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    QPointF widgetToImage(const QPointF& w) const;
    QPointF imageToWidget(const QPointF& i) const;

    QImage m_image;
    double m_zoom = 1.0;
    QPointF m_offset = QPointF(0, 0);
    bool m_showChecker = false;
    QString m_placeholder;

    bool m_lassoMode = false;
    bool m_drawingLasso = false;
    QPolygonF m_lassoPoly;        // image coords, in-progress
    QPainterPath m_lassoPath;     // image coords, finalized

    bool m_panning = false;
    QPoint m_lastMousePos;
};

} // namespace stampextractor


class StampExtractor : public QWidget, public DynamicObjectBase
{
    Q_OBJECT
public:
    explicit StampExtractor();

protected:
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private slots:
    void onOpenImage();
    void onPickColor();
    void onSave();
    void onResetThresholds();
    void onToggleLasso(bool on);
    void onClearLasso();
    void onFitView();

private:
    void setupUi();
    void loadImageFromFile(const QString& path);
    void recomputeExtraction();   // 跑算法 + 把结果给到 m_dstView
    void applyColorButtonStyle();
    QColor effectiveColor() const;   // 基础色经 H/S 微调后的颜色

    static QImage extractRedStamp(const QImage& src, int redMin, int sensMin,
                                  const QColor& outColor, const QImage& mask);
    static QImage rasterizeMask(const QPainterPath& path, const QSize& size);
    static QImage trimTransparent(const QImage& src);
    static bool isExtractEmpty(const QImage& img);

    QImage m_source;        // 原图（缩放后）
    QImage m_extracted;     // 当前提取结果
    QColor m_color;

    // UI
    QPushButton* m_openBtn = nullptr;
    QPushButton* m_colorBtn = nullptr;
    QPushButton* m_saveBtn = nullptr;
    QPushButton* m_resetBtn = nullptr;
    QPushButton* m_lassoBtn = nullptr;
    QPushButton* m_clearLassoBtn = nullptr;
    QPushButton* m_fitBtn = nullptr;

    stampextractor::ImageView* m_srcView = nullptr;
    stampextractor::ImageView* m_dstView = nullptr;

    QLabel* m_hint = nullptr;
    QSlider* m_redSlider = nullptr;
    QSlider* m_sensSlider = nullptr;
    QSlider* m_hueSlider = nullptr;
    QSlider* m_satSlider = nullptr;
    QLabel* m_redValueLabel = nullptr;
    QLabel* m_sensValueLabel = nullptr;
    QLabel* m_hueValueLabel = nullptr;
    QLabel* m_satValueLabel = nullptr;
    QLabel* m_effectiveSwatch = nullptr;
    QCheckBox* m_autoTrim = nullptr;
};

#endif // STAMPEXTRACTOR_H
