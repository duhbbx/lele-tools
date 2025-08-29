#ifndef SCREENCAPTURE_MODULE_H
#define SCREENCAPTURE_MODULE_H

#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QComboBox>
#include <QTextEdit>
#include <QSettings>
#include "../../common/dynamicobjectbase.h"

// 前向声明ScreenCapture库的类
// 这些类来自common/thirdparty/screen-capture
class App;

class ScreenCapture : public QWidget, public DynamicObjectBase
{
    Q_OBJECT

public:
    ScreenCapture(QWidget *parent = nullptr);
    ~ScreenCapture();

    // 静态方法，供F1快捷键调用
    static void quickScreenshot();

private slots:
    void onQuickScreenshotClicked();
    void onCustomScreenshotClicked();
    void onFullScreenshotClicked();
    void onLongScreenshotClicked();
    void onAreaScreenshotClicked();
    void onOpenScreenCaptureDirClicked();

private:
    void setupUI();
    void setupControlArea();
    void setupQuickActions();
    void setupAdvancedOptions();
    void setupStatusArea();
    
    void executeScreenCapture(const QString &mode = "custom");
    
    // UI组件
    QVBoxLayout *mainLayout;
    QGroupBox *controlGroup;
    QGroupBox *quickGroup;
    QGroupBox *advancedGroup;
    QGroupBox *statusGroup;
    
    // 快捷操作按钮
    QPushButton *quickBtn;
    QPushButton *customBtn;
    QPushButton *fullScreenBtn;
    QPushButton *longScreenBtn;
    QPushButton *areaScreenBtn;
    
    // 高级选项
    QCheckBox *saveToClipboardCheck;
    QCheckBox *compressCheck;
    QLineEdit *savePathEdit;
    QPushButton *browseDirBtn;
    QComboBox *languageCombo;
    
    // 状态显示
    QTextEdit *statusText;
    QLabel *statusLabel;
    
    // 设置
    QString lastSavePath;
};

#endif // SCREENCAPTURE_MODULE_H

