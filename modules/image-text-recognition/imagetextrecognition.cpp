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
#include <QSettings>
#include <QDialog>
#include <QLineEdit>
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

    auto* engineLabel = new QLabel(tr("引擎:"));
    m_engineCombo = new QComboBox();
    m_engineCombo->addItems({"自动", "PaddleOCR", "Tesseract"});
    m_engineCombo->setCurrentIndex(0);

    m_configBtn = new QPushButton(tr("设置"));

    toolbar->addWidget(engineLabel);
    toolbar->addWidget(m_engineCombo);
    toolbar->addWidget(m_configBtn);

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
    connect(m_configBtn, &QPushButton::clicked, this, &ImageTextRecognition::onConfigEngine);

    loadEngineSettings();
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

    // 不在这里检查引擎，让 recognizeImage 自己选择可用引擎
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
    OcrEngine engine = static_cast<OcrEngine>(m_engineCombo->currentIndex());

    if (engine == Auto) {
        // 优先 PaddleOCR，识别失败或不可用则降级到 Tesseract
        if (checkPaddleOcr()) {
            QString result = recognizeWithPaddle(imagePath);
            if (!result.startsWith(tr("[PaddleOCR 识别失败]")))
                return result;
            // PaddleOCR 识别失败，降级
        }
        if (checkTesseract())
            return recognizeWithTesseract(imagePath);
        return tr("[识别失败] 未找到可用的 OCR 引擎。\n请安装 Tesseract: brew install tesseract tesseract-lang\n或 PaddleOCR: pip install paddleocr paddlepaddle");
    } else if (engine == PaddleOCR) {
        if (!checkPaddleOcr()) return tr("[识别失败] PaddleOCR 不可用");
        return recognizeWithPaddle(imagePath);
    } else {
        if (!checkTesseract()) return tr("[识别失败] Tesseract 不可用");
        return recognizeWithTesseract(imagePath);
    }
}

QString ImageTextRecognition::recognizeWithTesseract(const QString& imagePath)
{
    QProcess process;
    QString lang = m_langCombo->currentText();
    process.start("tesseract", QStringList() << imagePath << "stdout" << "-l" << lang);
    process.waitForFinished(30000);

    if (process.exitCode() != 0) {
        QString err = QString::fromUtf8(process.readAllStandardError());
        return tr("[Tesseract 识别失败] %1").arg(err.trimmed());
    }
    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

QString ImageTextRecognition::recognizeWithPaddle(const QString& imagePath)
{
    QString cmd = m_paddleOcrPath.isEmpty() ? "paddleocr" : m_paddleOcrPath;

    // PaddleOCR CLI: paddleocr --image_dir image.png --lang ch --use_angle_cls true
    QString lang = "ch"; // 默认中文
    QString langSel = m_langCombo->currentText();
    if (langSel.startsWith("eng")) lang = "en";
    else if (langSel.startsWith("jpn")) lang = "japan";
    else if (langSel.startsWith("kor")) lang = "korean";
    else if (langSel.contains("chi_tra")) lang = "chinese_cht";

    QProcess process;
    process.start(cmd, QStringList()
        << "--image_dir" << imagePath
        << "--lang" << lang
        << "--use_angle_cls" << "true"
        << "--show_log" << "false");
    process.waitForFinished(60000); // PaddleOCR 可能较慢

    if (process.exitCode() != 0) {
        QString err = QString::fromUtf8(process.readAllStandardError());
        return tr("[PaddleOCR 识别失败] %1").arg(err.trimmed());
    }

    // 解析输出：PaddleOCR 输出格式为每行一个识别结果
    // [文字, 置信度] 或纯文本
    QString output = QString::fromUtf8(process.readAllStandardOutput());
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    QStringList textLines;

    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        // 跳过日志行（以数字开头的时间戳等）
        if (trimmed.isEmpty()) continue;
        // PaddleOCR 输出格式: ('文字', 0.98) 或类似
        // 提取引号中的文字
        int start = trimmed.indexOf('\'');
        int end = trimmed.lastIndexOf('\'');
        if (start >= 0 && end > start) {
            textLines.append(trimmed.mid(start + 1, end - start - 1));
        } else if (!trimmed.startsWith('[') && !trimmed.startsWith('(') && !trimmed.contains("ppocr")) {
            textLines.append(trimmed);
        }
    }

    return textLines.join('\n');
}

bool ImageTextRecognition::checkPaddleOcr()
{
    QString cmd = m_paddleOcrPath.isEmpty() ? "paddleocr" : m_paddleOcrPath;
    QProcess p;
    p.start(cmd, QStringList() << "--help");
    p.waitForFinished(5000);
    return p.exitCode() == 0 || p.exitStatus() == QProcess::NormalExit;
}

bool ImageTextRecognition::checkTesseract()
{
    QProcess p;
    p.start("tesseract", QStringList() << "--version");
    p.waitForFinished(5000);
    return p.exitCode() == 0;
}

void ImageTextRecognition::onConfigEngine()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("OCR 引擎设置"));
    dialog.setMinimumWidth(500);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setSpacing(10);
    layout->setContentsMargins(16, 12, 16, 12);

    // 说明
    auto* infoLabel = new QLabel(tr(
        "<b>OCR 引擎说明</b><br><br>"
        "<b>Tesseract</b>（默认）：<br>"
        "macOS: <code>brew install tesseract tesseract-lang</code><br>"
        "Ubuntu: <code>sudo apt install tesseract-ocr tesseract-ocr-chi-sim</code><br>"
        "Windows: 从 <a href='https://github.com/UB-Mannheim/tesseract/wiki'>GitHub</a> 下载<br><br>"
        "<b>PaddleOCR</b>（更高精度，推荐中文）：<br>"
        "<code>pip install paddleocr paddlepaddle</code><br>"
        "首次运行会自动下载模型（约 100MB）<br>"
        "官网: <a href='https://github.com/PaddlePaddle/PaddleOCR'>github.com/PaddlePaddle/PaddleOCR</a>"
    ));
    infoLabel->setWordWrap(true);
    infoLabel->setOpenExternalLinks(true);
    infoLabel->setStyleSheet("font-size:9pt; color:#495057;");
    infoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    layout->addWidget(infoLabel);

    // PaddleOCR 路径
    auto* pathLayout = new QHBoxLayout();
    auto* pathLabel = new QLabel(tr("PaddleOCR 路径:"));
    auto* pathEdit = new QLineEdit(m_paddleOcrPath);
    pathEdit->setPlaceholderText(tr("留空使用系统 PATH 中的 paddleocr"));
    auto* browseBtn = new QPushButton(tr("浏览"));
    pathLayout->addWidget(pathLabel);
    pathLayout->addWidget(pathEdit, 1);
    pathLayout->addWidget(browseBtn);
    layout->addLayout(pathLayout);

    connect(browseBtn, &QPushButton::clicked, &dialog, [&pathEdit, &dialog]() {
        QString path = QFileDialog::getOpenFileName(&dialog, "选择 paddleocr", "", "All (*)");
        if (!path.isEmpty()) pathEdit->setText(path);
    });

    // 检测状态
    auto* statusLayout = new QHBoxLayout();
    auto* detectBtn = new QPushButton(tr("检测引擎"));
    auto* detectLabel = new QLabel();
    detectLabel->setStyleSheet("font-size:9pt;");
    statusLayout->addWidget(detectBtn);
    statusLayout->addWidget(detectLabel, 1);
    layout->addLayout(statusLayout);

    connect(detectBtn, &QPushButton::clicked, &dialog, [this, detectLabel, pathEdit]() {
        m_paddleOcrPath = pathEdit->text().trimmed();
        QString status;
        if (checkPaddleOcr()) status += "PaddleOCR: 可用  ";
        else status += "PaddleOCR: 不可用  ";
        if (checkTesseract()) status += "Tesseract: 可用";
        else status += "Tesseract: 不可用";
        detectLabel->setText(status);
    });

    // 按钮
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto* okBtn = new QPushButton(tr("确定"));
    auto* cancelBtn = new QPushButton(tr("取消"));
    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(okBtn);
    layout->addLayout(btnLayout);

    connect(okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        m_paddleOcrPath = pathEdit->text().trimmed();
        saveEngineSettings();
    }
}

void ImageTextRecognition::saveEngineSettings()
{
    QSettings settings;
    settings.setValue("ocr/paddleOcrPath", m_paddleOcrPath);
}

void ImageTextRecognition::loadEngineSettings()
{
    QSettings settings;
    m_paddleOcrPath = settings.value("ocr/paddleOcrPath", "").toString();
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
