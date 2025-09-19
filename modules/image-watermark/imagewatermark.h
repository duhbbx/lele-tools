#ifndef IMAGEWATERMARK_H
#define IMAGEWATERMARK_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QGroupBox>
#include <QTextEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QComboBox>
#include <QColorDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <QSplitter>
#include <QTimer>
#include <QPainter>
#include <QPixmap>
#include <QFont>
#include <QFontDialog>
#include <QColor>
#include <QTextOption>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QUrl>
#include <QStandardPaths>
#include <QDir>
#include <QImageReader>
#include <QImageWriter>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include "../../common/dynamicobjectbase.h"

// 水印配置结构
struct WatermarkConfig {
    QString text;                   // 水印文字
    QColor color;                   // 文字颜色
    QFont font;                     // 字体
    int opacity;                    // 透明度 (0-100)
    int spacing;                    // 间隔 (像素)
    Qt::Alignment position;         // 位置
    bool tiled;                     // 是否平铺
    int rotation;                   // 旋转角度

    WatermarkConfig() {
        text = "xxx专用";
        color = QColor(128, 128, 128);
        font = QFont("Microsoft YaHei", 16);
        opacity = 30;
        spacing = 50;
        position = Qt::AlignCenter;
        tiled = true;
        rotation = -30;
    }
};

// 批量处理工作线程
class WatermarkWorker : public QObject {
    Q_OBJECT

public:
    explicit WatermarkWorker(QObject* parent = nullptr);

    void processImages(const QStringList& inputFiles, const QString& outputDir, const WatermarkConfig& config);

signals:
    void progressUpdated(int current, int total, const QString& currentFile);
    void imageProcessed(const QString& inputFile, const QString& outputFile, bool success, const QString& error);
    void finished();

private slots:
    void doWork();

private:
    QPixmap addWatermarkToImage(const QPixmap& image, const WatermarkConfig& config);

    QStringList m_inputFiles;
    QString m_outputDir;
    WatermarkConfig m_config;
    QMutex m_mutex;
    bool m_shouldStop;
};

// 单个图片水印组件
class SingleImageWatermark : public QWidget {
    Q_OBJECT

public:
    explicit SingleImageWatermark(QWidget* parent = nullptr);

private slots:
    void onSelectImageClicked();
    void onSaveImageClicked();
    void onCopyImageClicked();
    void onWatermarkConfigChanged();
    void onColorButtonClicked();
    void onFontButtonClicked();
    void onPositionChanged();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void setupUI();
    void updatePreview();
    QPixmap addWatermark(const QPixmap& image, const WatermarkConfig& config);
    void resetUI();

    // UI组件
    QVBoxLayout* m_mainLayout;
    QSplitter* m_mainSplitter;

    // 左侧配置区域
    QWidget* m_configWidget;
    QVBoxLayout* m_configLayout;

    QGroupBox* m_imageGroup;
    QVBoxLayout* m_imageGroupLayout;
    QPushButton* m_selectImageBtn;
    QLabel* m_imageInfoLabel;

    QGroupBox* m_watermarkGroup;
    QFormLayout* m_watermarkLayout;
    QLineEdit* m_watermarkText;
    QPushButton* m_colorBtn;
    QSlider* m_opacitySlider;
    QLabel* m_opacityLabel;
    QSlider* m_spacingSlider;
    QLabel* m_spacingLabel;
    QSpinBox* m_fontSizeSpinBox;
    QPushButton* m_fontBtn;
    QComboBox* m_positionCombo;
    QSlider* m_rotationSlider;
    QLabel* m_rotationLabel;

    QHBoxLayout* m_buttonLayout;
    QPushButton* m_saveBtn;
    QPushButton* m_copyBtn;

    // 右侧预览区域
    QWidget* m_previewWidget;
    QVBoxLayout* m_previewLayout;
    QLabel* m_previewLabel;
    QScrollArea* m_previewScrollArea;
    QLabel* m_previewImageLabel;

    // 数据
    QString m_originalImagePath;
    QPixmap m_originalPixmap;
    QPixmap m_watermarkedPixmap;
    WatermarkConfig m_config;
};

// 批量图片水印组件
class BatchImageWatermark : public QWidget {
    Q_OBJECT

public:
    explicit BatchImageWatermark(QWidget* parent = nullptr);
    ~BatchImageWatermark() override;

private slots:
    void onAddImagesClicked();
    void onRemoveSelectedClicked();
    void onClearAllClicked();
    void onSelectOutputDirClicked();
    void onStartProcessingClicked();
    void onStopProcessingClicked();
    void onWatermarkConfigChanged();
    void onColorButtonClicked();
    void onFontButtonClicked();

    void onProgressUpdated(int current, int total, const QString& currentFile);
    void onImageProcessed(const QString& inputFile, const QString& outputFile, bool success, const QString& error);
    void onProcessingFinished();

private:
    void setupUI();
    void updateUI();
    void startProcessing();
    void stopProcessing();

    // UI组件
    QVBoxLayout* m_mainLayout;
    QSplitter* m_mainSplitter;

    // 左侧配置区域
    QWidget* m_configWidget;
    QVBoxLayout* m_configLayout;

    QGroupBox* m_filesGroup;
    QVBoxLayout* m_filesGroupLayout;
    QListWidget* m_filesList;
    QHBoxLayout* m_filesButtonLayout;
    QPushButton* m_addImagesBtn;
    QPushButton* m_removeSelectedBtn;
    QPushButton* m_clearAllBtn;

    QGroupBox* m_outputGroup;
    QVBoxLayout* m_outputGroupLayout;
    QHBoxLayout* m_outputDirLayout;
    QLineEdit* m_outputDirEdit;
    QPushButton* m_selectOutputDirBtn;

    QGroupBox* m_watermarkGroup;
    QFormLayout* m_watermarkLayout;
    QLineEdit* m_watermarkText;
    QPushButton* m_colorBtn;
    QSlider* m_opacitySlider;
    QLabel* m_opacityLabel;
    QSlider* m_spacingSlider;
    QLabel* m_spacingLabel;
    QSpinBox* m_fontSizeSpinBox;
    QPushButton* m_fontBtn;
    QSlider* m_rotationSlider;
    QLabel* m_rotationLabel;

    // 右侧处理状态区域
    QWidget* m_statusWidget;
    QVBoxLayout* m_statusLayout;

    QGroupBox* m_controlGroup;
    QVBoxLayout* m_controlGroupLayout;
    QHBoxLayout* m_controlButtonLayout;
    QPushButton* m_startBtn;
    QPushButton* m_stopBtn;

    QGroupBox* m_progressGroup;
    QVBoxLayout* m_progressGroupLayout;
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
    QLabel* m_currentFileLabel;

    QGroupBox* m_resultsGroup;
    QVBoxLayout* m_resultsGroupLayout;
    QTextEdit* m_resultsText;

    // 处理相关
    QThread* m_workerThread;
    WatermarkWorker* m_worker;
    bool m_isProcessing;
    WatermarkConfig m_config;
};

// 图片水印工具主类
class ImageWatermark final : public QWidget, public DynamicObjectBase {
    Q_OBJECT

public:
    explicit ImageWatermark(QWidget* parent = nullptr);
    ~ImageWatermark() override = default;

private:
    void setupUI();

    // UI组件
    QVBoxLayout* m_mainLayout;
    QTabWidget* m_tabWidget;
    SingleImageWatermark* m_singleWatermark;
    BatchImageWatermark* m_batchWatermark;
};

#endif // IMAGEWATERMARK_H