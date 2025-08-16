#ifndef IMAGECOMPRESSION_H
#define IMAGECOMPRESSION_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QSlider>
#include <QComboBox>
#include <QProgressBar>
#include <QGroupBox>
#include <QScrollArea>
#include <QCheckBox>
#include <QFileDialog>
#include <QPixmap>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QThread>
#include <QMutex>
#include <QTimer>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QRubberBand>
#include <QMouseEvent>

#include "../../common/dynamicobjectbase.h"

// 图片处理工作线程
class ImageProcessWorker : public QObject
{
    Q_OBJECT

public:
    struct ProcessParams {
        QString inputPath;
        QString outputPath;
        QString format;
        int quality;
        int width;
        int height;
        bool maintainAspectRatio;
        QRect cropRect;
        bool enableCrop;
    };

public slots:
    void processImage(const ProcessParams& params);

signals:
    void imageProcessed(bool success, const QString& message, qint64 originalSize, qint64 newSize);
    void progressUpdated(int percentage);
};

// 自定义图片查看器，支持裁剪选择
class ImageViewer : public QGraphicsView
{
    Q_OBJECT

public:
    explicit ImageViewer(QWidget* parent = nullptr);
    void setImage(const QPixmap& pixmap);
    QRect getCropRect() const { return cropRect; }
    void setCropEnabled(bool enabled);
    void clearCropSelection();

signals:
    void cropRectChanged(const QRect& rect);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void updateCropRect();
    
    QGraphicsScene* scene;
    QGraphicsPixmapItem* pixmapItem;
    QRubberBand* rubberBand;
    QPoint startPoint;
    QRect cropRect;
    bool cropEnabled;
    bool dragging;
};

class ImageCompression : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit ImageCompression();
    ~ImageCompression();

public slots:
    void onSelectImage();
    void onSaveImage();
    void onPreviewChanges();
    void onResetSettings();
    void onClearAll();
    void onQualityChanged();
    void onSizeChanged();
    void onFormatChanged();
    void onCropToggled();
    void onCropRectChanged(const QRect& rect);
    void onImageProcessed(bool success, const QString& message, qint64 originalSize, qint64 newSize);
    void onProgressUpdated(int percentage);

private:
    void setupUI();
    void setupFileArea();
    void setupImageViewer();
    void setupControlArea();
    void setupPreviewArea();
    void loadImage(const QString& filePath);
    void updateImageInfo();
    void updatePreview();
    void updateStatus(const QString& message, bool isError = false);
    QString formatFileSize(qint64 bytes);
    void resetControls();
    
    // UI组件
    QVBoxLayout* mainLayout;
    QSplitter* mainSplitter;
    QSplitter* rightSplitter;
    
    // 文件选择区域
    QGroupBox* fileGroup;
    QVBoxLayout* fileLayout;
    QHBoxLayout* fileButtonLayout;
    QPushButton* selectBtn;
    QPushButton* saveBtn;
    QPushButton* clearBtn;
    QLabel* filePathLabel;
    QLabel* originalSizeLabel;
    
    // 图片查看器
    QGroupBox* viewerGroup;
    QVBoxLayout* viewerLayout;
    ImageViewer* imageViewer;
    QHBoxLayout* viewerButtonLayout;
    QPushButton* resetViewBtn;
    QPushButton* fitToWindowBtn;
    
    // 控制区域
    QWidget* controlWidget;
    QVBoxLayout* controlLayout;
    
    // 压缩设置
    QGroupBox* compressGroup;
    QGridLayout* compressLayout;
    QLabel* qualityLabel;
    QSlider* qualitySlider;
    QSpinBox* qualitySpinBox;
    QLabel* formatLabel;
    QComboBox* formatCombo;
    
    // 尺寸设置
    QGroupBox* sizeGroup;
    QGridLayout* sizeLayout;
    QCheckBox* resizeCheck;
    QLabel* widthLabel;
    QSpinBox* widthSpinBox;
    QLabel* heightLabel;
    QSpinBox* heightSpinBox;
    QCheckBox* aspectRatioCheck;
    QLabel* originalSizeInfoLabel;
    
    // 裁剪设置
    QGroupBox* cropGroup;
    QGridLayout* cropLayout;
    QCheckBox* cropCheck;
    QLabel* cropXLabel;
    QSpinBox* cropXSpinBox;
    QLabel* cropYLabel;
    QSpinBox* cropYSpinBox;
    QLabel* cropWidthLabel;
    QSpinBox* cropWidthSpinBox;
    QLabel* cropHeightLabel;
    QSpinBox* cropHeightSpinBox;
    QPushButton* clearCropBtn;
    
    // 预览区域
    QGroupBox* previewGroup;
    QVBoxLayout* previewLayout;
    QPushButton* previewBtn;
    QPushButton* resetBtn;
    QLabel* previewSizeLabel;
    QLabel* compressionRatioLabel;
    QProgressBar* progressBar;
    QLabel* statusLabel;
    
    // 数据和状态
    QString currentImagePath;
    QPixmap originalPixmap;
    QPixmap previewPixmap;
    QImage originalImage;
    QSize originalImageSize;
    qint64 originalFileSize;
    qint64 previewFileSize;
    
    // 工作线程
    QThread* workerThread;
    ImageProcessWorker* worker;
    
    // 设置
    bool isProcessing;
    double originalAspectRatio;
};

#endif // IMAGECOMPRESSION_H