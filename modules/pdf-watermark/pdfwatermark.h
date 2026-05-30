#ifndef PDFWATERMARK_H
#define PDFWATERMARK_H

#include <QWidget>
#include <QColor>
#include <QImage>
#include <QSet>

#include "../../common/dynamicobjectbase.h"

class QLineEdit;
class QPushButton;
class QLabel;
class QComboBox;
class QSpinBox;
class QListWidget;
class QListWidgetItem;
class QPdfDocument;
class QTimer;
class QScrollArea;

class PdfWatermark : public QWidget, public DynamicObjectBase
{
    Q_OBJECT
public:
    explicit PdfWatermark();
    ~PdfWatermark() override;

protected:
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private slots:
    void onOpenPdf();
    void onExportPdf();
    void onChooseImageWatermark();
    void onChooseColor();
    void onTypeChanged(int idx);
    void onRangePreset();    // 全部 / 奇数 / 偶数 / 仅首页 / 跳过首页
    void onPageSelected();   // 用户在缩略图列表里选了一页
    void schedulePreview();  // 水印参数变更 → 防抖触发 refreshPreview

private:
    void setupUI();
    void loadPdf(const QString& path);
    void buildPageList();
    QSet<int> parsePageRange() const;   // 把"1-3,5,7-9"解析成具体页索引集合（0-based）
    void drawWatermarkOnImage(QImage& img, qreal logicalScale);
    void refreshPreview();              // 把当前选中页渲到预览 label

    QString m_pdfPath;
    QPdfDocument* m_pdfDoc = nullptr;

    // UI
    QPushButton* m_openBtn = nullptr;
    QPushButton* m_exportBtn = nullptr;
    QLabel*      m_pdfInfoLabel = nullptr;
    QListWidget* m_pageList = nullptr;
    QLabel*      m_previewLabel = nullptr;
    QTimer*      m_previewDebounce = nullptr;

    // 应用范围
    QLineEdit* m_rangeEdit = nullptr;       // "1-3,5,7-9"
    QPushButton* m_allBtn = nullptr;
    QPushButton* m_oddBtn = nullptr;
    QPushButton* m_evenBtn = nullptr;
    QPushButton* m_firstBtn = nullptr;
    QPushButton* m_skipFirstBtn = nullptr;

    // 水印类型
    QComboBox* m_typeCombo = nullptr;       // 文字 / 图片

    // 文字水印
    QLineEdit* m_textEdit = nullptr;
    QSpinBox*  m_fontSizeSpin = nullptr;
    QSpinBox*  m_opacitySpin = nullptr;
    QSpinBox*  m_rotationSpin = nullptr;
    QComboBox* m_positionCombo = nullptr;   // 斜铺 / 居中 / 右下角 / 底部居中 / 顶部居中
    QPushButton* m_colorBtn = nullptr;
    QColor m_textColor = QColor(80, 80, 80);

    // 图片水印
    QPushButton* m_chooseImgBtn = nullptr;
    QLabel*      m_imgPathLabel = nullptr;
    QSpinBox*    m_imgScaleSpin = nullptr;  // 宽度占页面宽度百分比
    QSpinBox*    m_imgOpacitySpin = nullptr;
    QComboBox*   m_imgPositionCombo = nullptr;
    QString      m_imageWatermarkPath;
    QImage       m_imageWatermark;

    // 文字相关行（图片模式下隐藏）
    QWidget* m_textRow1 = nullptr;
    QWidget* m_textRow2 = nullptr;
    QWidget* m_imgRow1  = nullptr;
    QWidget* m_imgRow2  = nullptr;

    // 输出质量
    QComboBox* m_qualityCombo = nullptr;

    QLabel* m_statusLabel = nullptr;
};

#endif // PDFWATERMARK_H
