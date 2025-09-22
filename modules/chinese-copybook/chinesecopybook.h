#ifndef CHINESECOPYBOOK_H
#define CHINESECOPYBOOK_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QTextEdit>
#include <QComboBox>
#include <QGroupBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QButtonGroup>
#include <QRadioButton>
#include <QScrollArea>
#include <QGridLayout>
#include <QPainter>
#include <QPrintDialog>
#include <QPrinter>
#include <QPageSetupDialog>
#include <QFont>
#include <QFontDatabase>
#include <QColorDialog>
#include <QSlider>
#include <QSpacerItem>
#include <QSplitter>
#include <QFrame>

#include "../../common/dynamicobjectbase.h"

/**
 * @brief 格子类型枚举
 */
enum class GridType {
    None,           // 无格子
    Square,         // 方格
    TianZi,         // 田字格
    MiZi,           // 米字格
    MiZiHuiGong,    // 米字回宫格
    HuiGong         // 回宫格
};

/**
 * @brief 字帖类型枚举
 */
enum class CopybookType {
    Tracing,        // 描字帖
    StrokeOrder,    // 笔顺贴
    PenControl,     // 控笔贴
    Template        // 字帖模板
};

/**
 * @brief 汉字字帖配置结构
 */
struct CopybookConfig {
    CopybookType type;
    QString text;
    GridType gridType;
    QColor gridColor;
    bool showPinyinRow;
    bool fillPinyin;
    bool showChineseRow;
    bool fillChinese;
    int fontSize;
    int gridSize;
    int rowsPerPage;
    int colsPerPage;
    QFont textFont;

    CopybookConfig()
        : type(CopybookType::Tracing)
        , gridType(GridType::TianZi)
        , gridColor(QColor(0, 128, 0))  // 绿色
        , showPinyinRow(true)
        , fillPinyin(false)
        , showChineseRow(true)
        , fillChinese(false)
        , fontSize(48)
        , gridSize(80)
        , rowsPerPage(8)
        , colsPerPage(10)
        , textFont("楷体", 48) {}
};

/**
 * @brief 汉字字帖生成工具主界面
 */
class ChineseCopybook : public QWidget, public DynamicObjectBase {
    Q_OBJECT

public:
    explicit ChineseCopybook(QWidget* parent = nullptr);
    ~ChineseCopybook();

private slots:
    void onTabChanged(int index);
    void onTextChanged();
    void onGridTypeChanged(int id);
    void onGridColorChanged(int id);
    void onCustomColorChanged();
    void onOptionsChanged();
    void onGenerate();
    void onPrint();
    void onPreview();
    void onFontSizeChanged(int size);
    void onGridSizeChanged(int size);
    void onPrevPage();
    void onNextPage();

private:
    void setupUI();
    void setupTabWidget();
    void setupContentArea();
    QVBoxLayout* setupOptionsArea();
    QHBoxLayout* setupControlArea();
    void setupPreviewArea();

    void updateConfig();
    void generateCopybook();
    void generateMultiPageCopybook();
    void drawPage(QPainter& painter, const QRect& pageRect, const QString& text, int& charIndex);
    void drawA4Border(QPainter& painter, const QRect& pageRect);
    void updatePageInfo();
    void drawGrid(QPainter& painter, const QRect& rect, GridType type, const QColor& color);
    void drawTianZiGrid(QPainter& painter, const QRect& rect, const QColor& color);
    void drawMiZiGrid(QPainter& painter, const QRect& rect, const QColor& color);
    void drawMiZiHuiGongGrid(QPainter& painter, const QRect& rect, const QColor& color);
    void drawHuiGongGrid(QPainter& painter, const QRect& rect, const QColor& color);
    void drawSquareGrid(QPainter& painter, const QRect& rect, const QColor& color);

    void drawCharacter(QPainter& painter, const QRect& rect, const QString& character,
                      bool isTemplate = false, bool showStroke = false);
    void drawPinyinRow(QPainter& painter, const QRect& rect, const QString& pinyin);

    QString getCharacterPinyin(const QString& character);
    QStringList getCharacterStrokeOrder(const QString& character);

    void printCopybook(QPrinter* printer);

private:
    // UI组件
    QVBoxLayout* m_mainLayout;
    QTabWidget* m_tabWidget;
    QTextEdit* m_textEdit;

    // 选项区域
    QGroupBox* m_gridGroup;
    QButtonGroup* m_gridButtonGroup;
    QRadioButton* m_noneGridRadio;
    QRadioButton* m_squareGridRadio;
    QRadioButton* m_tianZiGridRadio;
    QRadioButton* m_miZiGridRadio;
    QRadioButton* m_miZiHuiGongGridRadio;
    QRadioButton* m_huiGongGridRadio;

    QGroupBox* m_colorGroup;
    QButtonGroup* m_colorButtonGroup;
    QRadioButton* m_greenColorRadio;
    QRadioButton* m_blackColorRadio;
    QRadioButton* m_redColorRadio;
    QPushButton* m_customColorButton;
    QColor m_customColor;

    QGroupBox* m_optionsGroup;
    QCheckBox* m_showPinyinRowCheck;
    QCheckBox* m_fillPinyinCheck;
    QCheckBox* m_showChineseRowCheck;
    QCheckBox* m_fillChineseCheck;

    QGroupBox* m_sizeGroup;
    QLabel* m_fontSizeLabel;
    QSlider* m_fontSizeSlider;
    QSpinBox* m_fontSizeSpinBox;
    QLabel* m_gridSizeLabel;
    QSlider* m_gridSizeSlider;
    QSpinBox* m_gridSizeSpinBox;

    // 控制按钮
    QPushButton* m_generateButton;
    QPushButton* m_printButton;
    QPushButton* m_previewButton;

    // 预览区域
    QScrollArea* m_previewArea;
    QLabel* m_previewLabel;

    // 配置和数据
    CopybookConfig m_config;
    QPixmap m_previewPixmap;
    QList<QPixmap> m_pagePixmaps;  // 存储多页内容
    int m_currentPageIndex;        // 当前预览页面索引

    // 页面控制
    QPushButton* m_prevPageButton;
    QPushButton* m_nextPageButton;
    QLabel* m_pageInfoLabel;
};


#endif // CHINESECOPYBOOK_H