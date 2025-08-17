
#ifndef BARCODEGEN_H
#define BARCODEGEN_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QCheckBox>
#include <QColorDialog>
#include <QFileDialog>
#include <QProgressBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QScrollArea>
#include <QFrame>
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QPixmap>
#include <QImage>
#include <QClipboard>
#include <QApplication>
#include <QMessageBox>
#include <QTimer>
#include <QStandardPaths>
#include <QDir>
#include <QSettings>
#include <QDateTime>
#include <QRegularExpression>

#include "../../common/dynamicobjectbase.h"
#include "../../common/colorbutton.h"

// 条形码类型枚举
enum class BarcodeType {
    Code128,        // Code 128
    Code39,         // Code 39
    Code93,         // Code 93
    EAN13,          // EAN-13
    EAN8,           // EAN-8
    UPCA,           // UPC-A
    UPCE,           // UPC-E
    ITF,            // Interleaved 2 of 5
    Codabar,        // Codabar
    DataMatrix,     // Data Matrix
    PDF417          // PDF417
};

// 条形码样式配置
struct BarcodeStyle {
    QColor foregroundColor;     // 前景色
    QColor backgroundColor;     // 背景色
    int width;                  // 宽度
    int height;                 // 高度
    bool showText;              // 显示文本
    QString fontFamily;         // 字体
    int fontSize;               // 字体大小
    int margin;                 // 边距
    
    BarcodeStyle() : foregroundColor(Qt::black), backgroundColor(Qt::white),
                    width(300), height(100), showText(true), 
                    fontFamily("Arial"), fontSize(12), margin(10) {}
};

// 条形码项目
struct BarcodeItem {
    QString text;               // 条形码内容
    BarcodeType type;          // 条形码类型
    QPixmap barcodePixmap;     // 条形码图片
    BarcodeStyle style;        // 样式配置
    QString fileName;          // 文件名
    QDateTime createTime;      // 创建时间
    bool isValid;              // 是否有效
    QString errorMessage;      // 错误信息
    
    BarcodeItem() : type(BarcodeType::Code128), isValid(false) {}
};

// 条形码预览组件
class BarcodePreview : public QFrame
{
    Q_OBJECT

public:
    explicit BarcodePreview(QWidget *parent = nullptr);
    
    void setBarcode(const QPixmap &pixmap);
    void setBarcodeItem(const BarcodeItem &item);
    QPixmap getBarcode() const { return m_barcodePixmap; }
    void clearBarcode();

signals:
    void barcodeClicked();
    void saveRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    QPixmap m_barcodePixmap;
    BarcodeItem m_barcodeItem;
    bool m_hasBarcode;
};

// ColorButton现在在common/colorbutton.h中定义

class BarcodeGen : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit BarcodeGen();
    ~BarcodeGen();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    // 生成相关
    void onGenerateBarcode();
    void onTextChanged();
    void onBarcodeTypeChanged();
    void onStyleChanged();
    void onForegroundColorChanged(const QColor &color);
    void onBackgroundColorChanged(const QColor &color);
    void onPresetSelected();
    
    // 批量处理
    void onBatchGenerate();
    void onAddBatchItem();
    void onClearBatchList();
    void onImportBatch();
    void onExportBatch();
    
    // 保存和导出
    void onSaveBarcode();
    void onCopyToClipboard();
    void onAddToHistory();
    
    // 历史记录
    void onClearHistory();
    void onHistoryItemClicked(QTableWidgetItem *item);

private:
    void setupUI();
    void setupInputArea();
    void setupPreviewArea();
    void setupStyleArea();
    void setupBatchArea();
    void setupHistoryArea();
    void setupStatusArea();
    
    void initializePresets();
    void addPreset(const QString &name, BarcodeType type, const QString &templateText, const QString &description);
    
    QPixmap generateBarcodeInternal(const QString &text, BarcodeType type, const BarcodeStyle &style);
    BarcodeStyle getCurrentStyle() const;
    void updateBarcodePreview();
    void displayResult(const BarcodeItem &item);
    void clearResultDisplay();
    
    // 条形码生成核心方法
    QVector<int> generateBarcodePattern(const QString &text, BarcodeType type);
    void drawBarcodePattern(QPainter &painter, const QRect &rect, const QVector<int> &pattern, const QColor &color);
    
    // 不同类型条形码的模式生成
    QVector<int> generateCode128Pattern(const QString &text);
    QVector<int> generateCode39Pattern(const QString &text);
    QVector<int> generateEAN13Pattern(const QString &text);
    QVector<int> generateEAN8Pattern(const QString &text);
    QVector<int> generateUPCAPattern(const QString &text);
    
    // 辅助方法
    QString getBarcodeTypeName(BarcodeType type) const;
    void showBatchResults();
    void saveBatchResults();
    
    bool isValidBarcodeData(const QString &text, BarcodeType type) const;
    QString formatBarcodeData(const QString &text, BarcodeType type) const;
    
    void loadSettings();
    void saveSettings();
    void addToHistory(const BarcodeItem &item);
    
    // UI组件
    QVBoxLayout *mainLayout;
    QSplitter *mainSplitter;
    
    // 左侧输入区域
    QWidget *leftPanel;
    QVBoxLayout *leftLayout;
    
    // 输入区域
    QGroupBox *inputGroup;
    QGridLayout *inputLayout;
    QComboBox *barcodeTypeCombo;
    QLineEdit *textEdit;
    QComboBox *presetCombo;
    QPushButton *generateBtn;
    QPushButton *clearBtn;
    
    // 样式区域
    QGroupBox *styleGroup;
    QGridLayout *styleLayout;
    ColorButton *foregroundColorBtn;
    ColorButton *backgroundColorBtn;
    QSpinBox *widthSpinBox;
    QSpinBox *heightSpinBox;
    QSlider *widthSlider;
    QSlider *heightSlider;
    QCheckBox *showTextCheckBox;
    QComboBox *fontComboBox;
    QSpinBox *fontSizeSpinBox;
    QSpinBox *marginSpinBox;
    
    // 批量处理区域
    QGroupBox *batchGroup;
    QVBoxLayout *batchLayout;
    QTextEdit *batchTextEdit;
    QTableWidget *batchTable;
    QHBoxLayout *batchButtonLayout;
    QPushButton *batchGenerateBtn;
    QPushButton *addBatchItemBtn;
    QPushButton *clearBatchListBtn;
    QPushButton *importBatchBtn;
    QPushButton *exportBatchBtn;
    QProgressBar *batchProgressBar;
    QLabel *batchStatusLabel;
    
    // 右侧预览和历史区域
    QWidget *rightPanel;
    QVBoxLayout *rightLayout;
    
    // 预览区域
    QGroupBox *previewGroup;
    QVBoxLayout *previewLayout;
    BarcodePreview *barcodePreview;
    QHBoxLayout *previewButtonLayout;
    QPushButton *saveBtn;
    QPushButton *copyBtn;
    QPushButton *addToHistoryBtn;
    
    // 历史记录区域
    QGroupBox *historyGroup;
    QVBoxLayout *historyLayout;
    QTableWidget *historyTable;
    QHBoxLayout *historyButtonLayout;
    QPushButton *clearHistoryBtn;
    QLabel *historyCountLabel;
    
    // 状态栏
    QHBoxLayout *statusLayout;
    QLabel *statusLabel;
    QProgressBar *progressBar;
    
    // 数据成员
    QString m_currentText;
    BarcodeType m_currentType;
    BarcodeStyle m_currentStyle;
    QPixmap m_currentBarcode;
    QList<BarcodeItem> m_historyItems;
    QList<BarcodeItem> m_batchItems;
    
    // 预设数据
    struct BarcodePreset {
        QString name;
        BarcodeType type;
        QString templateText;
        QString description;
    };
    QList<BarcodePreset> m_presets;
    
    QTimer *m_statusTimer;
    QTimer *m_updateTimer;
    
    bool m_updatingUI;
};

#endif // BARCODEGEN_H
