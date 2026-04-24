#include "imagetextrecognition.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QFrame>
#include <QTemporaryDir>
#include <QUuid>
#include <QScreen>

REGISTER_DYNAMICOBJECT(ImageTextRecognition);

ImageTextRecognition::ImageTextRecognition() : QWidget(nullptr), DynamicObjectBase()
{
    setAcceptDrops(true);
    setupUI();
}

void ImageTextRecognition::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 6, 8, 6);
    mainLayout->setSpacing(4);

    setStyleSheet(R"(
        QPushButton { padding:4px 10px; border:none; border-radius:4px; font-size:9pt; color:#495057; background:transparent; }
        QPushButton:hover { background-color:#e9ecef; }
        QPushButton:pressed { background-color:#dee2e6; }
        QPushButton:disabled { color:#adb5bd; }
        QLabel { font-size:9pt; color:#495057; }
        QComboBox { font-size:9pt; padding:3px 8px; border:1px solid #dee2e6; border-radius:4px; background:white; }
        QComboBox::drop-down { border:none; }
        QTextEdit { font-size:9pt; border:1px solid #dee2e6; border-radius:4px; padding:4px; }
        QScrollArea { border:none; }
    )");

    // Toolbar
    auto* toolbar = new QHBoxLayout();
    toolbar->setSpacing(2);

    m_addBtn = new QPushButton(tr("添加图片"));
    m_pasteBtn = new QPushButton(tr("粘贴"));
    m_clearBtn = new QPushButton(tr("清空"));
    m_recognizeBtn = new QPushButton(tr("识别全部"));
    m_recognizeBtn->setStyleSheet(
        "QPushButton { padding:4px 10px; border:none; border-radius:4px; font-size:9pt; color:white; background:#228be6; }"
        "QPushButton:hover { background:#1c7ed6; }"
        "QPushButton:pressed { background:#1971c2; }"
    );
    m_copyAllBtn = new QPushButton(tr("复制全部"));
    m_copyNoSepBtn = new QPushButton(tr("复制全部(无分隔)"));

    toolbar->addWidget(m_addBtn);
    toolbar->addWidget(m_pasteBtn);
    toolbar->addWidget(m_clearBtn);
    toolbar->addWidget(m_recognizeBtn);
    toolbar->addWidget(m_copyAllBtn);
    toolbar->addWidget(m_copyNoSepBtn);
    toolbar->addStretch();

    auto* langLabel = new QLabel(tr("语言:"));
    m_langCombo = new QComboBox();
    m_langCombo->addItems({"chi_sim+eng", "eng", "chi_sim", "chi_tra+eng", "jpn+eng", "kor+eng"});
    m_langCombo->setCurrentIndex(0);

    toolbar->addWidget(langLabel);
    toolbar->addWidget(m_langCombo);

    mainLayout->addLayout(toolbar);

    // Scroll area
    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidgetResizable(true);
    m_resultsWidget = new QWidget();
    m_resultsLayout = new QVBoxLayout(m_resultsWidget);
    m_resultsLayout->setContentsMargins(0, 0, 0, 0);
    m_resultsLayout->setSpacing(0);
    m_resultsLayout->addStretch();
    m_scrollArea->setWidget(m_resultsWidget);
    mainLayout->addWidget(m_scrollArea, 1);

    // Status bar
    m_statusLabel = new QLabel(tr("共 0 张图片，已识别 0 张"));
    m_statusLabel->setStyleSheet("color:#868e96; font-size:8pt; padding:2px 0;");
    mainLayout->addWidget(m_statusLabel);

    // Connections
    connect(m_addBtn, &QPushButton::clicked, this, &ImageTextRecognition::onAddImages);
    connect(m_pasteBtn, &QPushButton::clicked, this, &ImageTextRecognition::onPasteFromClipboard);
    connect(m_clearBtn, &QPushButton::clicked, this, &ImageTextRecognition::onClearAll);
    connect(m_recognizeBtn, &QPushButton::clicked, this, &ImageTextRecognition::onRecognizeAll);
    connect(m_copyAllBtn, &QPushButton::clicked, this, &ImageTextRecognition::onCopyAll);
    connect(m_copyNoSepBtn, &QPushButton::clicked, this, &ImageTextRecognition::onCopyAllNoSeparator);
}

void ImageTextRecognition::onAddImages()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this, tr("选择图片"),
        QDir::homePath(),
        tr("图片文件 (*.png *.jpg *.jpeg *.bmp *.tiff *.tif *.webp)")
    );
    if (!files.isEmpty()) {
        addImageFiles(files);
    }
}

void ImageTextRecognition::onPasteFromClipboard()
{
    const QClipboard* clipboard = QApplication::clipboard();
    const QMimeData* mimeData = clipboard->mimeData();

    if (mimeData->hasImage()) {
        QPixmap pixmap = qvariant_cast<QPixmap>(mimeData->imageData());
        if (!pixmap.isNull()) {
            addImageFromPixmap(pixmap);
            return;
        }
    }

    if (mimeData->hasUrls()) {
        QStringList files;
        for (const QUrl& url : mimeData->urls()) {
            if (url.isLocalFile()) {
                files.append(url.toLocalFile());
            }
        }
        if (!files.isEmpty()) {
            addImageFiles(files);
            return;
        }
    }

    QMessageBox::information(this, tr("提示"), tr("剪贴板中没有图片"));
}

void ImageTextRecognition::onClearAll()
{
    m_items.clear();
    rebuildResultsView();
}

void ImageTextRecognition::onRecognizeAll()
{
    if (m_items.isEmpty()) {
        return;
    }

    if (!checkTesseract()) {
        return;
    }

    for (int i = 0; i < m_items.size(); ++i) {
        if (!m_items[i].recognized) {
            m_items[i].resultText = recognizeImage(m_items[i].imagePath);
            m_items[i].recognized = true;

            int recognized = 0;
            for (const auto& item : m_items) {
                if (item.recognized) recognized++;
            }
            m_statusLabel->setText(tr("共 %1 张图片，已识别 %2 张").arg(m_items.size()).arg(recognized));
            QApplication::processEvents();
        }
    }

    rebuildResultsView();
}

void ImageTextRecognition::onCopyAll()
{
    QStringList texts;
    for (const auto& item : m_items) {
        if (item.recognized && !item.resultText.isEmpty()) {
            texts.append(item.resultText);
        }
    }
    if (!texts.isEmpty()) {
        QApplication::clipboard()->setText(texts.join("\n-------------\n"));
    }
}

void ImageTextRecognition::onCopyAllNoSeparator()
{
    QStringList texts;
    for (const auto& item : m_items) {
        if (item.recognized && !item.resultText.isEmpty()) {
            texts.append(item.resultText);
        }
    }
    if (!texts.isEmpty()) {
        QApplication::clipboard()->setText(texts.join("\n\n"));
    }
}

void ImageTextRecognition::addImageFiles(const QStringList& files)
{
    for (const QString& file : files) {
        QPixmap pixmap(file);
        if (pixmap.isNull()) continue;

        OcrItem item;
        item.imagePath = file;
        item.pixmap = pixmap;
        m_items.append(item);
    }
    rebuildResultsView();
}

void ImageTextRecognition::addImageFromPixmap(const QPixmap& pixmap)
{
    // Save to temp file for tesseract
    QString tempDir = QDir::tempPath() + "/lele-ocr";
    QDir().mkpath(tempDir);
    QString tempPath = tempDir + "/" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".png";
    pixmap.save(tempPath, "PNG");

    OcrItem item;
    item.imagePath = tempPath;
    item.pixmap = pixmap;
    m_items.append(item);
    rebuildResultsView();
}

void ImageTextRecognition::rebuildResultsView()
{
    // Clear existing items from layout
    QLayoutItem* child;
    while ((child = m_resultsLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }

    qreal dpr = 1.0;
    QScreen* screen = QApplication::primaryScreen();
    if (screen) {
        dpr = screen->devicePixelRatio();
    }

    for (int i = 0; i < m_items.size(); ++i) {
        const auto& item = m_items[i];

        auto* rowWidget = new QWidget();
        auto* rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(4, 4, 4, 4);
        rowLayout->setSpacing(8);

        // Image preview as clickable button
        auto* imageBtn = new QPushButton();
        int displayWidth = 200;
        int pixelWidth = static_cast<int>(displayWidth * dpr);
        QPixmap scaled = item.pixmap.scaledToWidth(pixelWidth, Qt::SmoothTransformation);
        scaled.setDevicePixelRatio(dpr);
        imageBtn->setIcon(QIcon(scaled));
        imageBtn->setIconSize(QSize(displayWidth, static_cast<int>(scaled.height() / dpr)));
        imageBtn->setFixedWidth(displayWidth + 8);
        imageBtn->setMaximumHeight(158);
        imageBtn->setCursor(Qt::PointingHandCursor);
        imageBtn->setToolTip(tr("点击打开原图"));
        imageBtn->setStyleSheet("QPushButton { border:1px solid #dee2e6; border-radius:4px; padding:2px; background:white; } QPushButton:hover { border-color:#adb5bd; }");

        QString imgPath = item.imagePath;
        connect(imageBtn, &QPushButton::clicked, this, [imgPath]() {
            QDesktopServices::openUrl(QUrl::fromLocalFile(imgPath));
        });

        rowLayout->addWidget(imageBtn, 0, Qt::AlignTop);

        // Right side: text + copy button
        auto* rightWidget = new QWidget();
        auto* rightLayout = new QVBoxLayout(rightWidget);
        rightLayout->setContentsMargins(0, 0, 0, 0);
        rightLayout->setSpacing(2);

        auto* textEdit = new QTextEdit();
        textEdit->setReadOnly(true);
        if (item.recognized) {
            textEdit->setPlainText(item.resultText);
        } else {
            textEdit->setPlaceholderText(tr("尚未识别，请点击「识别全部」"));
        }
        textEdit->setMinimumHeight(100);
        rightLayout->addWidget(textEdit, 1);

        auto* copyBtn = new QPushButton(tr("复制"));
        copyBtn->setFixedWidth(50);
        QString resultText = item.resultText;
        connect(copyBtn, &QPushButton::clicked, this, [resultText]() {
            if (!resultText.isEmpty()) {
                QApplication::clipboard()->setText(resultText);
            }
        });

        auto* btnRow = new QHBoxLayout();
        btnRow->addStretch();
        btnRow->addWidget(copyBtn);
        rightLayout->addLayout(btnRow);

        rowLayout->addWidget(rightWidget, 1);

        m_resultsLayout->addWidget(rowWidget);

        // Separator line between items
        if (i < m_items.size() - 1) {
            auto* line = new QFrame();
            line->setFrameShape(QFrame::HLine);
            line->setFrameShadow(QFrame::Plain);
            line->setStyleSheet("color:#dee2e6;");
            line->setFixedHeight(1);
            m_resultsLayout->addWidget(line);
        }
    }

    m_resultsLayout->addStretch();

    // Update status
    int recognized = 0;
    for (const auto& item : m_items) {
        if (item.recognized) recognized++;
    }
    m_statusLabel->setText(tr("共 %1 张图片，已识别 %2 张").arg(m_items.size()).arg(recognized));
}

QString ImageTextRecognition::recognizeImage(const QString& imagePath)
{
    QProcess process;
    QString lang = m_langCombo->currentText();
    process.start("tesseract", QStringList() << imagePath << "stdout" << "-l" << lang);
    process.waitForFinished(30000);

    if (process.exitCode() != 0) {
        QString err = QString::fromUtf8(process.readAllStandardError());
        return tr("[识别失败] %1").arg(err.trimmed());
    }

    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

bool ImageTextRecognition::checkTesseract()
{
    QProcess p;
    p.start("tesseract", QStringList() << "--version");
    p.waitForFinished(5000);
    if (p.exitCode() != 0) {
        QMessageBox::warning(this, tr("Tesseract 未安装"),
            tr("请先安装 Tesseract OCR:\n\n"
               "macOS: brew install tesseract tesseract-lang\n"
               "Ubuntu: sudo apt install tesseract-ocr tesseract-ocr-chi-sim\n"
               "Windows: 从 GitHub 下载安装"));
        return false;
    }
    return true;
}

void ImageTextRecognition::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls() || event->mimeData()->hasImage()) {
        event->acceptProposedAction();
    }
}

void ImageTextRecognition::dropEvent(QDropEvent* event)
{
    const QMimeData* mimeData = event->mimeData();

    if (mimeData->hasImage()) {
        QPixmap pixmap = qvariant_cast<QPixmap>(mimeData->imageData());
        if (!pixmap.isNull()) {
            addImageFromPixmap(pixmap);
            return;
        }
    }

    if (mimeData->hasUrls()) {
        QStringList files;
        for (const QUrl& url : mimeData->urls()) {
            if (url.isLocalFile()) {
                QString path = url.toLocalFile();
                QStringList imageExts = {"png", "jpg", "jpeg", "bmp", "tiff", "tif", "webp"};
                QString ext = QFileInfo(path).suffix().toLower();
                if (imageExts.contains(ext)) {
                    files.append(path);
                }
            }
        }
        if (!files.isEmpty()) {
            addImageFiles(files);
        }
    }
}
