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
#include <QMenu>

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
        font = QFont("Microsoft YaHei", 40);
        opacity = 30;
        spacing = 200;
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
    void onSelectImagesClicked();
    void onSaveImageClicked();
    void onSaveAllClicked();
    void onCopyImageClicked();
    void onWatermarkConfigChanged();
    void onColorButtonClicked();
    void onFontButtonClicked();
    void onPositionChanged();
    void onImageListSelectionChanged();
    void onPreviewContextMenu(const QPoint& pos);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void setupUI();
    void updatePreview();
    QPixmap addWatermark(const QPixmap& image, const WatermarkConfig& config);
    void resetUI();
    void addImageFiles(const QStringList& files);

    // UI组件
    QHBoxLayout* m_mainLayout;

    // 左侧: 文件列表 + 配置
    QWidget* m_leftWidget;
    QVBoxLayout* m_leftLayout;

    QListWidget* m_imageListWidget;
    QHBoxLayout* m_imageButtonLayout;
    QPushButton* m_selectImagesBtn;
    QPushButton* m_removeSelectedBtn;

    QFormLayout* m_watermarkLayout;
    QLineEdit* m_watermarkText;
    QPushButton* m_colorBtn;
    QSlider* m_opacitySlider;
    QLabel* m_opacityLabel;
    QSlider* m_spacingSlider;
    QLabel* m_spacingLabel;
    QSlider* m_fontSizeSlider;
    QSpinBox* m_fontSizeSpinBox;
    QPushButton* m_fontBtn;
    QComboBox* m_positionCombo;
    QSlider* m_rotationSlider;
    QLabel* m_rotationLabel;

    QHBoxLayout* m_buttonLayout;
    QPushButton* m_saveBtn;
    QPushButton* m_saveAllBtn;
    QPushButton* m_copyBtn;

    // 右侧预览区域
    QWidget* m_previewWidget;
    QVBoxLayout* m_previewLayout;
    QScrollArea* m_previewScrollArea;
    QLabel* m_previewImageLabel;

    // 数据
    QStringList m_imagePaths;
    QPixmap m_originalPixmap;
    QPixmap m_watermarkedPixmap;
    WatermarkConfig m_config;
    int m_currentIndex;
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
    QHBoxLayout* m_mainLayout;

    // 左侧配置区域
    QWidget* m_configWidget;
    QVBoxLayout* m_configLayout;

    QListWidget* m_filesList;
    QHBoxLayout* m_filesButtonLayout;
    QPushButton* m_addImagesBtn;
    QPushButton* m_removeSelectedBtn;
    QPushButton* m_clearAllBtn;

    QHBoxLayout* m_outputDirLayout;
    QLineEdit* m_outputDirEdit;
    QPushButton* m_selectOutputDirBtn;

    QFormLayout* m_watermarkLayout;
    QLineEdit* m_watermarkText;
    QPushButton* m_colorBtn;
    QSlider* m_opacitySlider;
    QLabel* m_opacityLabel;
    QSlider* m_spacingSlider;
    QLabel* m_spacingLabel;
    QSlider* m_fontSizeSlider;
    QSpinBox* m_fontSizeSpinBox;
    QPushButton* m_fontBtn;
    QSlider* m_rotationSlider;
    QLabel* m_rotationLabel;

    // 右侧处理状态区域
    QWidget* m_statusWidget;
    QVBoxLayout* m_statusLayout;

    QHBoxLayout* m_controlButtonLayout;
    QPushButton* m_startBtn;
    QPushButton* m_stopBtn;

    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
    QLabel* m_currentFileLabel;

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
