#ifndef MARKDOWNTOPDF_H
#define MARKDOWNTOPDF_H

#include <QWidget>
#include <QString>
#include <QUrl>

#include "../../common/dynamicobjectbase.h"

class QPlainTextEdit;
class QTextBrowser;
class QPushButton;
class QComboBox;
class QSpinBox;
class QLabel;
class QSplitter;
class QTimer;
class QCheckBox;

class MarkdownToPdf : public QWidget, public DynamicObjectBase
{
    Q_OBJECT
public:
    explicit MarkdownToPdf();

protected:
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private slots:
    void onOpenMd();
    void onSaveMd();
    void onExportPdf();
    void onMarkdownChanged();
    void renderPreview();

private:
    void setupUi();
    void loadMarkdownFile(const QString& path);
    QString currentStyleSheet() const;

    // UI
    QPushButton* m_openBtn = nullptr;
    QPushButton* m_saveBtn = nullptr;
    QPushButton* m_exportBtn = nullptr;
    QComboBox* m_pageSizeCombo = nullptr;
    QComboBox* m_orientationCombo = nullptr;
    QComboBox* m_themeCombo = nullptr;
    QSpinBox* m_fontSizeSpin = nullptr;
    QSpinBox* m_marginSpin = nullptr;
    QCheckBox* m_pageNumbersCheck = nullptr;
    QCheckBox* m_tocCheck = nullptr;
    QSpinBox* m_tocDepthSpin = nullptr;
    QLabel* m_statusLabel = nullptr;

    QSplitter* m_splitter = nullptr;
    QPlainTextEdit* m_editor = nullptr;
    QTextBrowser* m_preview = nullptr;

    QTimer* m_debounce = nullptr;

    QString m_currentMdPath;   // 来源文件路径，用于解析相对图片路径
};

#endif // MARKDOWNTOPDF_H
