
#ifndef BASE64ENCODEDECODE_H
#define BASE64ENCODEDECODE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QGroupBox>
#include <QScrollArea>
#include <QFileDialog>
#include <QApplication>
#include <QClipboard>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPixmap>
#include <QMouseEvent>

#include "../../common/dynamicobjectbase.h"

// 前向声明
class ImagePreviewLabel;

// 自定义图片预览标签类
class ImagePreviewLabel : public QLabel {
    Q_OBJECT

public:
    explicit ImagePreviewLabel(QWidget *parent = nullptr);
    
    void setImageFromPath(const QString &imagePath);
    void setImageFromBase64(const QString &base64Data);
    void clearImage();

signals:
    void imageDropped(const QString &imagePath);
    void imageClicked();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QPixmap originalPixmap;
    
    void updateDisplayedPixmap();
    bool isImageFile(const QString &fileName);
};

// Base64编码解码主类
class Base64EncodeDecode : public QWidget, public DynamicObjectBase {
    Q_OBJECT

public:
    explicit Base64EncodeDecode();

private slots:
    // 文本编码解码
    void onEncodeText();
    void onDecodeText();
    void onClearText();
    void onCopyResult();
    
    // 图片编码解码
    void onImageDropped(const QString &imagePath);
    void onImageSelectClicked();
    void onImageBase64Changed();
    void onSaveImageClicked();

private:
    // UI 组件
    QTabWidget *mainTabWidget;
    
    // 文本编码解码页面
    QWidget *textTab;
    QTextEdit *inputTextEdit;
    QTextEdit *outputTextEdit;
    QPushButton *encodeButton;
    QPushButton *decodeButton;
    QPushButton *clearButton;
    QPushButton *copyButton;
    
    // 图片编码解码页面
    QWidget *imageTab;
    ImagePreviewLabel *imagePreviewLabel;
    QScrollArea *imageScrollArea;
    QTextEdit *imageBase64Edit;
    QPushButton *selectImageButton;
    QPushButton *saveImageButton;
    QPushButton *decodePreviewButton;
    QLabel *imageInfoLabel;
    
    // 私有方法
    void setupUI();
    void setupTextTab();
    void setupImageTab();
    void applyStyles();
    void showMessage(const QString &message, bool isError = false);
    void loadImageFromPath(const QString &imagePath);
    void updateImageInfo(const QString &fileName, qint64 fileSize);
};

#endif // BASE64ENCODEDECODE_H
