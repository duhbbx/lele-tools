#ifndef FAVICONPRODUCTION_H
#define FAVICONPRODUCTION_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QProgressBar>
#include <QGroupBox>
#include <QScrollArea>
#include <QCheckBox>
#include <QFileDialog>
#include <QPixmap>
#include <QImage>
#include <QListWidget>
#include <QListWidgetItem>
#include <QTextEdit>
#include <QTabWidget>

#include "../../common/dynamicobjectbase.h"

struct FaviconSize {
    int width;
    int height;
    QString description;
    bool isSelected;
};

class FaviconProduction : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    explicit FaviconProduction();
    ~FaviconProduction();

public slots:
    void onSelectImage();
    void onGenerateFavicons();
    void onSelectOutputDir();
    void onClearAll();
    void onPreviewSize();
    void onSizeSelectionChanged();
    void onGenerateHtml();

private:
    void setupUI();
    void setupInputArea();
    void setupSizeSelection();
    void setupPreviewArea();
    void setupOutputArea();
    void loadImage(const QString& filePath);
    void initializeSizes();
    void generateFavicon(const FaviconSize& size, const QString& outputDir);
    void updatePreview();
    void updateStatus(const QString& message, bool isError = false);
    QString generateHtmlCode();
    
    // UI组件
    QVBoxLayout* mainLayout;
    QSplitter* mainSplitter;
    
    // 输入区域
    QGroupBox* inputGroup;
    QVBoxLayout* inputLayout;
    QHBoxLayout* inputButtonLayout;
    QPushButton* selectImageBtn;
    QPushButton* clearBtn;
    QLabel* imagePathLabel;
    QLabel* imageSizeLabel;
    QLabel* imagePreviewLabel;
    
    // 尺寸选择区域
    QGroupBox* sizeGroup;
    QVBoxLayout* sizeLayout;
    QListWidget* sizeListWidget;
    QHBoxLayout* sizeButtonLayout;
    QPushButton* selectAllBtn;
    QPushButton* deselectAllBtn;
    QPushButton* previewBtn;
    
    // 预览区域
    QGroupBox* previewGroup;
    QScrollArea* previewScrollArea;
    QWidget* previewWidget;
    QGridLayout* previewGridLayout;
    
    // 输出区域
    QGroupBox* outputGroup;
    QVBoxLayout* outputLayout;
    QHBoxLayout* outputPathLayout;
    QLineEdit* outputPathEdit;
    QPushButton* selectOutputBtn;
    QHBoxLayout* outputButtonLayout;
    QPushButton* generateBtn;
    QPushButton* generateHtmlBtn;
    QProgressBar* progressBar;
    QLabel* statusLabel;
    
    // HTML代码区域
    QTabWidget* codeTabWidget;
    QTextEdit* htmlCodeEdit;
    QTextEdit* manifestCodeEdit;
    
    // 数据
    QString currentImagePath;
    QPixmap originalPixmap;
    QList<FaviconSize> faviconSizes;
    QString outputDirectory;
    bool isGenerating;
};

#endif // FAVICONPRODUCTION_H