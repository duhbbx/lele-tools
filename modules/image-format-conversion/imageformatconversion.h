#ifndef IMAGEFORMATCONVERSION_H
#define IMAGEFORMATCONVERSION_H

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QImage>

#include "../../common/dynamicobjectbase.h"

class QListWidget;
class QListWidgetItem;
class QComboBox;
class QLineEdit;
class QPushButton;
class QSlider;
class QSpinBox;
class QLabel;
class QProgressBar;
class QRadioButton;
class QCheckBox;

class ImageFormatConversion : public QWidget, public DynamicObjectBase
{
    Q_OBJECT
public:
    explicit ImageFormatConversion();

protected:
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private slots:
    void onAddFiles();
    void onRemoveSelected();
    void onClearAll();
    void onPickOutputDir();
    void onConvert();
    void onTargetFormatChanged();
    void onResizeModeChanged();

private:
    void setupUI();
    void addPaths(const QStringList& paths);
    QImage loadImage(const QString& path) const;
    QImage applyResize(const QImage& src) const;
    bool saveImage(const QImage& img, const QString& outPath, const QString& fmt, QString* err) const;
    bool saveAsSvgWrappedRaster(const QImage& img, const QString& outPath, QString* err) const;
    // 在不超过 limitBytes 的前提下保存：先二分降质量（仅 lossy），再按比例缩小图片
    bool saveWithSizeLimit(const QImage& img, const QString& outPath, const QString& fmt,
                           qint64 limitBytes, QString* err) const;

    enum ResizeMode { ResizeKeep, ResizeMaxDim, ResizeExact };

    // 文件列表
    QListWidget* m_listWidget = nullptr;
    QPushButton* m_addBtn = nullptr;
    QPushButton* m_removeBtn = nullptr;
    QPushButton* m_clearBtn = nullptr;
    QLabel* m_listStatus = nullptr;

    // 输出选项
    QComboBox* m_formatCombo = nullptr;
    QLineEdit* m_outputDirEdit = nullptr;
    QPushButton* m_outputDirBtn = nullptr;
    QSlider* m_qualitySlider = nullptr;
    QLabel* m_qualityLabel = nullptr;

    // 调整尺寸
    QRadioButton* m_resizeKeepRadio = nullptr;
    QRadioButton* m_resizeMaxRadio = nullptr;
    QRadioButton* m_resizeExactRadio = nullptr;
    QSpinBox* m_maxDimSpin = nullptr;
    QSpinBox* m_exactWSpin = nullptr;
    QSpinBox* m_exactHSpin = nullptr;
    QCheckBox* m_keepAspectCheck = nullptr;

    // 操作
    QPushButton* m_convertBtn = nullptr;
    QProgressBar* m_progress = nullptr;
    QLabel* m_statusLabel = nullptr;

    // 覆盖策略
    QCheckBox* m_overwriteCheck = nullptr;

    // 最大输出大小限制
    QCheckBox* m_sizeLimitCheck = nullptr;
    QSpinBox* m_sizeLimitSpin = nullptr;
};

#endif // IMAGEFORMATCONVERSION_H
