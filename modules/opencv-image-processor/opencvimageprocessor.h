#ifndef OPENCVIMAGEPROCESSOR_H
#define OPENCVIMAGEPROCESSOR_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
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
#include <QTabWidget>
#include <QButtonGroup>
#include <QRadioButton>
#include <QTextEdit>
#include <QSizePolicy>
#include <QApplication>
#include <QMessageBox>
#include <QDebug>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QPainter>
#include <QFrame>

#ifdef WITH_OPENCV
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/features2d.hpp>
#endif

#include "../../common/dynamicobjectbase.h"

// Forward declarations
class ImageDisplayWidget;
class ParameterPanel;
class ProcessingWorker;

/**
 * @brief OpenCV图像处理工具
 *
 * 功能包括：
 * - 基础图像处理：灰度化、二值化、模糊、锐化、直方图均衡化
 * - 边缘检测：Canny、Sobel、Laplacian
 * - 角点检测：Harris、Shi-Tomasi
 * - 几何变换：缩放、旋转、仿射变换、透视变换
 * - 滤波：均值、Gaussian、Sobel、Laplacian等
 * - 图像I/O：读写图片、视频、摄像头流
 */
class OpenCvImageProcessor : public QWidget, public DynamicObjectBase {
    Q_OBJECT

public:
    explicit OpenCvImageProcessor();
    ~OpenCvImageProcessor();

    // 处理模式枚举
    enum ProcessingMode {
        NONE = 0,
        // 基础处理
        GRAYSCALE,
        BINARY_THRESHOLD,
        ADAPTIVE_THRESHOLD,
        GAUSSIAN_BLUR,
        MEDIAN_BLUR,
        BILATERAL_FILTER,
        SHARPEN,
        HISTOGRAM_EQUALIZATION,
        CLAHE,

        // 边缘检测
        CANNY_EDGE,
        SOBEL_EDGE,
        LAPLACIAN_EDGE,

        // 角点检测
        HARRIS_CORNERS,
        SHI_TOMASI_CORNERS,

        // 几何变换
        RESIZE,
        ROTATE,
        AFFINE_TRANSFORM,
        PERSPECTIVE_TRANSFORM,

        // 滤波器
        MEAN_FILTER,
        GAUSSIAN_FILTER,
        SOBEL_FILTER,
        LAPLACIAN_FILTER,
        CUSTOM_FILTER
    };

private slots:
    void openImage();
    void saveImage();
    void resetImage();
    void copyToClipboard();
    void pasteFromClipboard();

    // 处理功能槽函数
    void onProcessingModeChanged();
    void onParameterChanged();
    void processImage();
    void onProcessingFinished();

    // 视频/摄像头相关
    void openVideo();
    void openCamera();
    void stopVideo();
    void onFrameReady();

private:
    void setupUI();
    void setupImageDisplay();
    void setupControlPanel();
    QHBoxLayout* setupStatusBar();
    void connectSignals();

    // OpenCV相关方法
#ifdef WITH_OPENCV
    cv::Mat qImageToCvMat(const QImage& qimage);
    QImage cvMatToQImage(const cv::Mat& cvmat);

    // 图像处理方法
    cv::Mat processBasicOperations(const cv::Mat& input, ProcessingMode mode);
    cv::Mat processEdgeDetection(const cv::Mat& input, ProcessingMode mode);
    cv::Mat processCornerDetection(const cv::Mat& input, ProcessingMode mode);
    cv::Mat processGeometricTransform(const cv::Mat& input, ProcessingMode mode);
    cv::Mat processFiltering(const cv::Mat& input, ProcessingMode mode);

    // 参数获取方法
    void getCannyParameters(double& low, double& high);
    void getThresholdParameters(double& thresh, double& maxval);
    void getBlurParameters(int& kernelSize);
    void getResizeParameters(double& scaleX, double& scaleY);
    void getRotateParameters(double& angle, cv::Point2f& center);
#endif

    void updateImageDisplay();
    void updateStatusInfo();
    void showError(const QString& message);

private:
    // UI组件
    QSplitter* m_mainSplitter;
    ImageDisplayWidget* m_imageDisplay;
    QTabWidget* m_controlTabs;
    ParameterPanel* m_parameterPanel;
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;

    // 控制按钮
    QPushButton* m_openButton;
    QPushButton* m_saveButton;
    QPushButton* m_resetButton;
    QPushButton* m_copyButton;
    QPushButton* m_pasteButton;
    QPushButton* m_processButton;

    // 处理模式选择
    QComboBox* m_modeCombo;
    ProcessingMode m_currentMode;

    // 图像数据
#ifdef WITH_OPENCV
    cv::Mat m_originalImage;
    cv::Mat m_processedImage;
    cv::VideoCapture m_videoCapture;
#endif
    QImage m_currentQImage;
    QString m_currentImagePath;

    // 视频相关
    QTimer* m_videoTimer;
    bool m_isVideoMode;
    bool m_isCameraMode;

    // 处理相关
    ProcessingWorker* m_worker;
    QThread* m_workerThread;
    bool m_isProcessing;
};

/**
 * @brief 图像显示组件
 */
class ImageDisplayWidget : public QWidget {
    Q_OBJECT

public:
    explicit ImageDisplayWidget(QWidget* parent = nullptr);
    void setImage(const QImage& image);
    void clearImage();

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void updateTransform();
    void fitToWindow();
    void zoomIn();
    void zoomOut();
    void resetZoom();

private:
    QImage m_image;
    QPixmap m_scaledPixmap;
    double m_scaleFactor;
    QPoint m_lastPanPoint;
    QPoint m_offset;
    bool m_dragging;
    QRect m_imageRect;
};

/**
 * @brief 参数面板组件
 */
class ParameterPanel : public QWidget {
    Q_OBJECT

public:
    explicit ParameterPanel(QWidget* parent = nullptr);
    void updateForMode(OpenCvImageProcessor::ProcessingMode mode);

    // 参数获取方法
    double getDoubleParameter(const QString& name) const;
    int getIntParameter(const QString& name) const;
    bool getBoolParameter(const QString& name) const;
    QString getStringParameter(const QString& name) const;

signals:
    void parameterChanged();

private slots:
    void onParameterChanged();

private:
    void clearParameters();
    void addDoubleParameter(const QString& name, const QString& label,
                           double value, double min, double max, double step = 0.1);
    void addIntParameter(const QString& name, const QString& label,
                        int value, int min, int max, int step = 1);
    void addBoolParameter(const QString& name, const QString& label, bool value);
    void addComboParameter(const QString& name, const QString& label,
                          const QStringList& items, int currentIndex = 0);

private:
    QGridLayout* m_layout;
    QMap<QString, QWidget*> m_parameters;
    int m_currentRow;
};

/**
 * @brief 图像处理工作线程
 */
class ProcessingWorker : public QObject {
    Q_OBJECT

public:
    explicit ProcessingWorker(QObject* parent = nullptr);

#ifdef WITH_OPENCV
    void setInputImage(const cv::Mat& image);
    void setProcessingMode(OpenCvImageProcessor::ProcessingMode mode);
    void setParameters(const QMap<QString, QVariant>& params);
#endif

public slots:
    void processImage();

signals:
    void imageProcessed(const QImage& result);
    void processingError(const QString& error);
    void progressChanged(int percentage);

private:
#ifdef WITH_OPENCV
    cv::Mat m_inputImage;
    OpenCvImageProcessor::ProcessingMode m_mode;
    QMap<QString, QVariant> m_parameters;
#endif
};

#endif // OPENCVIMAGEPROCESSOR_H