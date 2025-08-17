#ifndef QRCODEGEN_H
#define QRCODEGEN_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QTabWidget>
#include "../../common/customtabwidget.h"
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QCheckBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QColorDialog>
#include <QFileDialog>
#include <QProgressBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QListWidget>
#include <QListWidgetItem>
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
#include <QImageReader>
#include <QImageWriter>
#include <QBuffer>
#include <QClipboard>
#include <QApplication>
#include <QMessageBox>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>
#include <QStandardPaths>
#include <QDir>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>

#include "../../common/dynamicobjectbase.h"
#include "../../common/colorbutton.h"

// 二维码数据类型枚举
enum class QRDataType {
    Text,           // 纯文本
    URL,            // 网址
    Email,          // 邮箱
    Phone,          // 电话
    SMS,            // 短信
    WiFi,           // WiFi连接
    VCard,          // 电子名片
    Event,          // 日历事件
    Location        // 地理位置
};

// 二维码样式配置
struct QRStyle {
    QColor foregroundColor;     // 前景色
    QColor backgroundColor;     // 背景色
    int borderSize;             // 边框大小
    bool hasLogo;               // 是否有Logo
    QPixmap logoPixmap;         // Logo图片
    int logoSize;               // Logo大小
    bool roundedCorners;        // 圆角
    int cornerRadius;           // 圆角半径
    QString pattern;            // 图案样式
    
    QRStyle() : foregroundColor(Qt::black), backgroundColor(Qt::white), 
                borderSize(4), hasLogo(false), logoSize(50), 
                roundedCorners(false), cornerRadius(10), pattern("square") {}
};

// 二维码项目结构
struct QRCodeItem {
    QString text;               // 二维码内容
    QRDataType type;           // 数据类型
    QPixmap qrPixmap;          // 二维码图片
    QRStyle style;             // 样式配置
    QString fileName;           // 文件名
    QDateTime createTime;       // 创建时间
    
    QRCodeItem() : type(QRDataType::Text) {}
};

// 二维码预览组件
class QRCodePreview : public QFrame
{
    Q_OBJECT

public:
    explicit QRCodePreview(QWidget *parent = nullptr);
    
    void setQRCode(const QPixmap &pixmap);
    void setQRCodeItem(const QRCodeItem &item);
    QPixmap getQRCode() const { return m_qrPixmap; }
    void clearQRCode();

signals:
    void qrCodeClicked();
    void saveRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    QPixmap m_qrPixmap;
    QRCodeItem m_qrItem;
    bool m_hasQRCode;
};

// ColorButton现在在common/colorbutton.h中定义

// 批量处理工作线程
class BatchProcessWorker : public QObject
{
    Q_OBJECT

public:
    enum ProcessType {
        Generate,
        Parse
    };

public slots:
    void processBatch(const QStringList &inputs, ProcessType type, const QRStyle &style);

signals:
    void progressUpdate(int current, int total);
    void itemProcessed(const QRCodeItem &item);
    void batchCompleted();
    void errorOccurred(const QString &error);

public:
    QPixmap generateQRCode(const QString &text, const QRStyle &style);
    QString parseQRCode(const QPixmap &pixmap);

private:
    // 二维码矩阵生成相关
    QVector<QVector<bool>> generateQRMatrix(const QString &text);
    void drawFinderPattern(QVector<QVector<bool>> &matrix, int x, int y);
    void drawSeparators(QVector<QVector<bool>> &matrix, int size);
    void drawTimingPatterns(QVector<QVector<bool>> &matrix, int size);
    void fillDataModules(QVector<QVector<bool>> &matrix, const QString &text, int size);
    bool canFillModule(const QVector<QVector<bool>> &matrix, int row, int col, int size);
    QPixmap generateErrorPixmap(const QString &errorMsg, const QRStyle &style);
};

// 主二维码工具类
class QrCodeGen : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit QrCodeGen();
    ~QrCodeGen();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    // 生成相关
    void onGenerateQRCode();
    void onTextChanged();
    void onDataTypeChanged();
    void onErrorCorrectionChanged();
    void onSizeChanged();
    void onStyleChanged();
    void onForegroundColorChanged(const QColor &color);
    void onBackgroundColorChanged(const QColor &color);
    void onSelectLogo();
    void onRemoveLogo();
    void onPresetSelected();
    
    // 解析相关
    void onUploadImage();
    void onParseFromClipboard();
    void onClearParseResult();
    
    // 批量处理
    void onBatchGenerate();
    void onBatchParse();
    void onAddBatchItem();
    void onClearBatchList();
    void onBatchProgressUpdate(int current, int total);
    void onBatchItemProcessed(const QRCodeItem &item);
    void onBatchCompleted();
    void onBatchError(const QString &error);
    
    // 保存和导出
    void onSaveQRCode();
    void onSaveAsImage();
    void onCopyToClipboard();
    void onExportBatch();
    void onImportBatch();
    
    // 历史记录
    void onAddToHistory();
    void onClearHistory();
    void onHistoryItemClicked(QListWidgetItem *item);

private:
    void setupUI();
    void setupGenerateArea();
    void setupParseArea();
    void setupBatchArea();
    void setupHistoryArea();
    void setupPreviewArea();
    void setupStyleArea();
    void setupStatusArea();
    
    void initializePresets();
    void addPreset(const QString &name, QRDataType type, const QString &template_text, const QString &description);
    
    QPixmap generateQRCodeInternal(const QString &text, const QRStyle &style);
    QString parseQRCodeInternal(const QPixmap &pixmap);
    QRStyle getCurrentStyle() const;
    void updateQRCodePreview();
    void updateStylePreview();
    
    QString formatQRData(const QString &text, QRDataType type) const;
    QString getWiFiQRString(const QString &ssid, const QString &password, const QString &security) const;
    QString getVCardQRString(const QString &name, const QString &phone, const QString &email) const;
    QString getEventQRString(const QString &title, const QString &location, const QDateTime &startTime) const;
    
    void loadSettings();
    void saveSettings();
    void addToHistory(const QRCodeItem &item);
    
    // UI组件
    QVBoxLayout *mainLayout;
    CustomTabWidget *tabWidget;
    
    // 生成区域
    QWidget *generateWidget;
    QSplitter *generateSplitter;
    QGroupBox *inputGroup;
    QVBoxLayout *inputLayout;
    QComboBox *dataTypeCombo;
    QTextEdit *textEdit;
    QComboBox *presetCombo;
    
    // 特殊类型输入
    QWidget *specialInputWidget;
    QLineEdit *urlEdit;
    QLineEdit *emailEdit;
    QLineEdit *phoneEdit;
    QLineEdit *wifiSSIDEdit;
    QLineEdit *wifiPasswordEdit;
    QComboBox *wifiSecurityCombo;
    QLineEdit *vcardNameEdit;
    QLineEdit *vcardPhoneEdit;
    QLineEdit *vcardEmailEdit;
    
    // 设置区域
    QGroupBox *settingsGroup;
    QGridLayout *settingsLayout;
    QComboBox *errorCorrectionCombo;
    QSpinBox *sizeSpinBox;
    QSlider *sizeSlider;
    QSpinBox *borderSpinBox;
    
    // 样式区域
    QWidget *styleWidget;
    QGroupBox *styleGroup;
    QGridLayout *styleLayout;
    ColorButton *foregroundColorBtn;
    ColorButton *backgroundColorBtn;
    QCheckBox *logoCheckBox;
    QPushButton *selectLogoBtn;
    QPushButton *removeLogoBtn;
    QLabel *logoPreviewLabel;
    QSpinBox *logoSizeSpinBox;
    QCheckBox *roundedCornersCheckBox;
    QSpinBox *cornerRadiusSpinBox;
    QComboBox *patternCombo;
    
    // 预览区域
    QGroupBox *previewGroup;
    QVBoxLayout *previewLayout;
    QRCodePreview *qrPreview;
    QHBoxLayout *previewButtonLayout;
    QPushButton *generateBtn;
    QPushButton *saveBtn;
    QPushButton *copyBtn;
    QPushButton *addToHistoryBtn;
    
    // 解析区域
    QWidget *parseWidget;
    QGroupBox *parseInputGroup;
    QVBoxLayout *parseInputLayout;
    QPushButton *uploadImageBtn;
    QPushButton *parseClipboardBtn;
    QLabel *uploadedImageLabel;
    QGroupBox *parseResultGroup;
    QVBoxLayout *parseResultLayout;
    QTextEdit *parseResultEdit;
    QPushButton *clearParseBtn;
    QPushButton *copyParseResultBtn;
    
    // 批量处理区域
    QWidget *batchWidget;
    QSplitter *batchSplitter;
    QGroupBox *batchInputGroup;
    QVBoxLayout *batchInputLayout;
    QTextEdit *batchTextEdit;
    QHBoxLayout *batchInputButtonLayout;
    QPushButton *addBatchItemBtn;
    QPushButton *clearBatchListBtn;
    QPushButton *importBatchBtn;
    
    QGroupBox *batchResultGroup;
    QVBoxLayout *batchResultLayout;
    QTableWidget *batchTable;
    QHBoxLayout *batchButtonLayout;
    QPushButton *batchGenerateBtn;
    QPushButton *batchParseBtn;
    QPushButton *exportBatchBtn;
    QProgressBar *batchProgressBar;
    QLabel *batchStatusLabel;
    
    // 历史记录区域
    QWidget *historyWidget;
    QGroupBox *historyGroup;
    QVBoxLayout *historyLayout;
    QListWidget *historyList;
    QHBoxLayout *historyButtonLayout;
    QPushButton *clearHistoryBtn;
    QLabel *historyCountLabel;
    
    // 状态栏
    QHBoxLayout *statusLayout;
    QLabel *statusLabel;
    QProgressBar *progressBar;
    
    // 数据成员
    QString m_currentText;
    QRDataType m_currentDataType;
    QRStyle m_currentStyle;
    QPixmap m_currentQRCode;
    QPixmap m_logoPixmap;
    QList<QRCodeItem> m_historyItems;
    QList<QRCodeItem> m_batchItems;
    
    // 预设数据
    struct QRPreset {
        QString name;
        QRDataType type;
        QString templateText;
        QString description;
    };
    QList<QRPreset> m_presets;
    
    // 工作线程
    QThread *m_workerThread;
    BatchProcessWorker *m_batchWorker;
    
    QTimer *m_statusTimer;
    QTimer *m_updateTimer;
    
    bool m_updatingUI;
};

#endif // QRCODEGEN_H