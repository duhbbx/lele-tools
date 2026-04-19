#ifndef COLORTOOLS_H
#define COLORTOOLS_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QTabWidget>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QListWidget>
#include <QScrollArea>
#include <QFrame>
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QColorDialog>
#include <QClipboard>
#include <QApplication>
#include <QMessageBox>
#include <QTimer>
#include <QColor>
#include <QPixmap>
#include <QBitmap>
#include <QScreen>
#include <QCursor>
#include <QRandomGenerator>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFileDialog>
#include <QTextEdit>
#include <QProgressBar>
#include <QCheckBox>
#include <QButtonGroup>
#include <QRadioButton>
#include <cmath>

#include "../../common/dynamicobjectbase.h"

// 自定义色轮组件
class ColorWheel : public QWidget
{
    Q_OBJECT

public:
    explicit ColorWheel(QWidget *parent = nullptr);
    
    QColor currentColor() const { return m_currentColor; }
    void setCurrentColor(const QColor &color);

signals:
    void colorChanged(const QColor &color);
    void colorPicked(const QColor &color);  // 鼠标释放时触发，用于加入历史

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateColorFromPoint(const QPoint &point);
    QPoint colorToPoint(const QColor &color) const;
    void updatePixmap();
    
    QColor m_currentColor;
    QPixmap m_wheelPixmap;
    QPoint m_currentPoint;
    int m_wheelWidth;
    bool m_mousePressed;
};

// 颜色条组件（用于亮度、饱和度等）
class ColorBar : public QWidget
{
    Q_OBJECT

public:
    enum BarType {
        HueBar,
        SaturationBar,
        ValueBar,
        RedBar,
        GreenBar,
        BlueBar
    };

    explicit ColorBar(BarType type, QWidget *parent = nullptr);
    
    void setBaseColor(const QColor &color);
    void setValue(int value);
    int value() const { return m_value; }

signals:
    void valueChanged(int value);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void updateValueFromPoint(const QPoint &point);
    void updatePixmap();
    
    BarType m_type;
    QColor m_baseColor;
    int m_value;
    QPixmap m_barPixmap;
    bool m_mousePressed;
};

// 颜色预览组件
class ColorPreview : public QFrame
{
    Q_OBJECT

public:
    explicit ColorPreview(QWidget *parent = nullptr);
    
    void setColor(const QColor &color);
    QColor color() const { return m_color; }

signals:
    void colorClicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QColor m_color;
};

// 调色板组件
class ColorPalette : public QWidget
{
    Q_OBJECT

public:
    explicit ColorPalette(QWidget *parent = nullptr);
    
    void addColor(const QColor &color);
    void removeColor(int index);
    void clearColors();
    QList<QColor> colors() const { return m_colors; }
    void setColors(const QList<QColor> &colors);

signals:
    void colorSelected(const QColor &color);
    void colorDoubleClicked(const QColor &color);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    int colorIndexAt(const QPoint &point) const;
    QRect colorRectAt(int index) const;
    
    QList<QColor> m_colors;
    int m_colorSize;
    int m_columns;
};

// 主颜色工具类
class ColorTools : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit ColorTools();
    ~ColorTools();

private slots:
    void onColorChanged();
    void onWheelColorChanged(const QColor &color);
    void onWheelColorPicked(const QColor &color);
    void onHueChanged(int value);
    void onSaturationChanged(int value);
    void onValueChanged(int value);
    void onRedChanged(int value);
    void onGreenChanged(int value);
    void onBlueChanged(int value);
    void onAlphaChanged(int value);
    void onHexChanged();
    void onRgbChanged();
    void onHslChanged();
    void onHsvChanged();
    void onCmykChanged();
    void onPickColorFromScreen();
    void onAddToHistory();
    void onAddToFavorites();
    void onClearHistory();
    void onClearFavorites();
    void onCopyColor();
    void onPasteColor();
    void onRandomColor();
    void onComplementaryColor();
    void onAnalogousColors();
    void onTriadicColors();
    void onMonochromaticColors();
    void onContrastCheck();
    void onExportPalette();
    void onImportPalette();
    void onPresetSelected();

private:
    void setupUI();
    void setupColorWheel();
    void setupColorBars();
    void setupColorInputs();
    void setupColorPreview();
    void setupColorPalettes();
    void setupColorHistory();
    void setupColorAnalysis();
    void setupStatusArea();
    
    void updateColorInputs();
    void updateColorBars();
    void updateColorPreview();
    void setCurrentColor(const QColor &color, bool updateInputs = true);
    
    // 颜色转换函数
    QString colorToHex(const QColor &color) const;
    QString colorToRgb(const QColor &color) const;
    QString colorToHsl(const QColor &color) const;
    QString colorToHsv(const QColor &color) const;
    QString colorToCmyk(const QColor &color) const;
    QColor hexToColor(const QString &hex) const;
    QColor rgbToColor(const QString &rgb) const;
    QColor hslToColor(const QString &hsl) const;
    QColor hsvToColor(const QString &hsv) const;
    QColor cmykToColor(const QString &cmyk) const;
    
    // 颜色分析函数
    double getContrastRatio(const QColor &color1, const QColor &color2) const;
    double getLuminance(const QColor &color) const;
    QList<QColor> getComplementaryColors(const QColor &color) const;
    QList<QColor> getAnalogousColors(const QColor &color) const;
    QList<QColor> getTriadicColors(const QColor &color) const;
    QList<QColor> getMonochromaticColors(const QColor &color) const;
    
    // 数据管理
    void loadSettings();
    void saveSettings();
    void addColorToHistory(const QColor &color);
    void addColorToFavorites(const QColor &color);
    
    // UI组件
    QVBoxLayout *mainLayout;
    QSplitter *mainSplitter;
    QTabWidget *tabWidget;
    
    // 色轮和颜色条
    QWidget *colorPickerWidget;
    ColorWheel *colorWheel;
    ColorBar *hueBar;
    ColorBar *saturationBar;
    ColorBar *valueBar;
    ColorBar *redBar;
    ColorBar *greenBar;
    ColorBar *blueBar;
    QSlider *alphaSlider;
    
    // 颜色输入
    QLabel *colorInputGroup;
    QLineEdit *hexEdit;
    QLineEdit *rgbEdit;
    QLineEdit *hslEdit;
    QLineEdit *hsvEdit;
    QLineEdit *cmykEdit;
    QSpinBox *redSpin;
    QSpinBox *greenSpin;
    QSpinBox *blueSpin;
    QSpinBox *alphaSpin;
    QSpinBox *hueSpin;
    QSpinBox *saturationSpin;
    QSpinBox *valueSpin;
    
    // 颜色预览
    QLabel *previewGroup;
    ColorPreview *currentColorPreview;
    ColorPreview *previousColorPreview;
    QLabel *colorInfoLabel;
    
    // 调色板
    QLabel *paletteGroup;
    ColorPalette *historyPalette;
    ColorPalette *favoritesPalette;
    ColorPalette *presetPalette;
    QComboBox *presetCombo;
    
    // 颜色分析
    QLabel *analysisGroup;
    QTextEdit *analysisText;
    ColorPreview *contrastPreview1;
    ColorPreview *contrastPreview2;
    QLabel *contrastRatioLabel;
    
    // 操作按钮
    QHBoxLayout *buttonLayout;
    QPushButton *pickFromScreenBtn;
    QPushButton *addToHistoryBtn;
    QPushButton *addToFavoritesBtn;
    QPushButton *copyColorBtn;
    QPushButton *pasteColorBtn;
    QPushButton *randomColorBtn;
    QPushButton *complementaryBtn;
    QPushButton *analogousBtn;
    QPushButton *triadicBtn;
    QPushButton *monochromaticBtn;
    QPushButton *contrastCheckBtn;
    QPushButton *exportPaletteBtn;
    QPushButton *importPaletteBtn;
    
    // 状态栏
    QHBoxLayout *statusLayout;
    QLabel *statusLabel;
    QProgressBar *progressBar;
    
    // 数据成员
    QColor m_currentColor;
    QColor m_previousColor;
    QList<QColor> m_colorHistory;
    QList<QColor> m_favoriteColors;
    QTimer *m_updateTimer;
    bool m_updatingColor;
    
    // 屏幕取色相关
    QTimer *m_pickTimer;
    bool m_pickingColor;
};

#endif // COLORTOOLS_H