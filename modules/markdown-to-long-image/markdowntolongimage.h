#ifndef MARKDOWNTOLONGIMAGE_H
#define MARKDOWNTOLONGIMAGE_H

#include <QWidget>
#include <QImage>

#include "../../common/dynamicobjectbase.h"

class QPlainTextEdit;
class QLabel;
class QScrollArea;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QCheckBox;
class QPushButton;
class QTimer;
class QPainter;

class MarkdownToLongImage : public QWidget, public DynamicObjectBase
{
    Q_OBJECT
public:
    explicit MarkdownToLongImage();
    ~MarkdownToLongImage() override;

protected:
    void resizeEvent(QResizeEvent* e) override;

private slots:
    void onRender();          // 渲染当前 MD → m_currentImage 并刷新预览
    void onExportPng();
    void onSliceExport();     // 按 H1 切片导出多张
    void onCopyImage();
    void scheduleRender();    // 编辑/选项变化时调用，启动防抖

private:
    void setupUI();
    QImage renderToImage() const;
    QImage renderMarkdownToImage(const QString& markdownText) const;
    QString buildCss(int themeIndex) const;
    void applyWatermark(QPainter& painter, const QSize& imgSize) const;
    void refreshPreview();

    QPlainTextEdit* m_editor = nullptr;
    QLabel*        m_previewLabel = nullptr;
    QScrollArea*   m_previewScroll = nullptr;

    QComboBox*  m_widthCombo = nullptr;
    QComboBox*  m_scaleCombo = nullptr;     // 渲染倍数 @1x/@2x/@3x
    QComboBox*  m_themeCombo = nullptr;
    QLineEdit*  m_watermarkEdit = nullptr;
    QSpinBox*   m_watermarkOpacity = nullptr;
    QComboBox*  m_watermarkPosCombo = nullptr;
    QCheckBox*  m_syncScrollCheck = nullptr;
    bool        m_isSyncing = false;     // 防止滚动联动死循环

    QPushButton* m_copyBtn   = nullptr;
    QPushButton* m_exportBtn = nullptr;
    QPushButton* m_sliceBtn  = nullptr;
    QComboBox*   m_sliceLevelCombo = nullptr;
    QLabel*      m_sizeLabel = nullptr;

    QTimer* m_debounce = nullptr;
    QImage  m_currentImage;
};

#endif // MARKDOWNTOLONGIMAGE_H
