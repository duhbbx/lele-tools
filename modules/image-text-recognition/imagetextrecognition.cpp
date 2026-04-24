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
#include <QRegularExpression>
#include <QTimer>
#include <QThread>
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
    m_resultsLayout->setAlignment(Qt::AlignTop);
    // 不加 stretch，避免底部大量空白
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
    if (m_items.isEmpty()) return;

    // 预检测引擎
    bool hasPaddle = checkPaddleOcr();
    bool hasTess = checkTesseract();
    if (!hasPaddle && !hasTess) {
        showInstallDialog();
        return;
    }

    m_recognizeBtn->setEnabled(false);
    m_statusLabel->setText(tr("正在识别..."));

    // 收集待识别的索引和路径
    QList<int> pendingIndexes;
    QStringList pendingPaths;
    for (int i = 0; i < m_items.size(); ++i) {
        if (!m_items[i].recognized) {
            pendingIndexes.append(i);
            pendingPaths.append(m_items[i].imagePath);
        }
    }

    if (pendingIndexes.isEmpty()) {
        m_recognizeBtn->setEnabled(true);
        return;
    }

    // 在后台线程逐个识别
    auto* thread = QThread::create([this, pendingPaths, hasPaddle, hasTess]() -> QStringList {
        QStringList results;
        for (const QString& path : pendingPaths) {
            QString result;
            OcrEngine engine = static_cast<OcrEngine>(m_engineCombo->currentIndex());

            if (engine == Auto) {
                if (hasPaddle) {
                    result = recognizeWithPaddle(path);
                    if (result.startsWith(tr("[PaddleOCR 识别失败]")) && hasTess)
                        result = recognizeWithTesseract(path);
                } else if (hasTess) {
                    result = recognizeWithTesseract(path);
                }
            } else if (engine == PaddleOCR && hasPaddle) {
                result = recognizeWithPaddle(path);
            } else if (hasTess) {
                result = recognizeWithTesseract(path);
            }

            if (result.isEmpty()) result = tr("[未识别到文字]");
            results.append(result);
        }
        return results;
    });

    connect(thread, &QThread::finished, this, [this, thread, pendingIndexes]() {
        // 这里无法直接拿返回值，改用另一种方式
        thread->deleteLater();
    });

    // 用 QtConcurrent 更方便拿返回值
    // 简化方案：用 QTimer 单步异步
    delete thread; // 不用上面的了

    // 简化：用定时器逐个识别，每个完成后更新 UI
    auto* timer = new QTimer(this);
    auto* idx = new int(0);
    auto* indexes = new QList<int>(pendingIndexes);

    connect(timer, &QTimer::timeout, this, [this, timer, idx, indexes, hasPaddle, hasTess]() {
        if (*idx >= indexes->size()) {
            timer->stop();
            timer->deleteLater();
            delete idx;
            delete indexes;
            m_recognizeBtn->setEnabled(true);
            rebuildResultsView();
            return;
        }

        int itemIdx = indexes->at(*idx);
        m_statusLabel->setText(tr("正在识别第 %1/%2 张...").arg(*idx + 1).arg(indexes->size()));
        QApplication::processEvents();

        QString path = m_items[itemIdx].imagePath;
        QString result;
        OcrEngine engine = static_cast<OcrEngine>(m_engineCombo->currentIndex());

        if (engine == Auto) {
            if (hasPaddle) {
                result = recognizeWithPaddle(path);
                if (result.startsWith("[PaddleOCR") && hasTess)
                    result = recognizeWithTesseract(path);
            } else {
                result = recognizeWithTesseract(path);
            }
        } else if (engine == PaddleOCR) {
            result = recognizeWithPaddle(path);
        } else {
            result = recognizeWithTesseract(path);
        }

        m_items[itemIdx].resultText = result.isEmpty() ? tr("[未识别到文字]") : result;
        m_items[itemIdx].recognized = true;

        int recognized = 0;
        for (const auto& item : m_items) if (item.recognized) recognized++;
        m_statusLabel->setText(tr("共 %1 张图片，已识别 %2 张").arg(m_items.size()).arg(recognized));

        (*idx)++;

        // 如果是最后一个，立刻触发（不等下次 timer）
        if (*idx >= indexes->size()) {
            rebuildResultsView();
            m_recognizeBtn->setEnabled(true);
            timer->stop();
            timer->deleteLater();
            delete idx;
            delete indexes;
        }
    });

    timer->start(10); // 10ms 后开始第一个，给 UI 刷新的机会
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

    // 不加 stretch，避免底部大量空白

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
    QString lang = m_langCombo->currentText();

    // 尝试用选定语言识别
    QProcess process;
    process.start("tesseract", QStringList() << imagePath << "stdout" << "-l" << lang);
    process.waitForFinished(30000);

    if (process.exitCode() != 0) {
        QString err = QString::fromUtf8(process.readAllStandardError());

        // 如果是语言包缺失，自动降级到 eng
        if (err.contains("Failed loading language") && lang != "eng") {
            QProcess fallback;
            fallback.start("tesseract", QStringList() << imagePath << "stdout" << "-l" << "eng");
            fallback.waitForFinished(30000);
            if (fallback.exitCode() == 0) {
                QString result = QString::fromUtf8(fallback.readAllStandardOutput()).trimmed();
                return result + tr("\n\n[注意: 语言包 %1 未安装，已降级为英文识别。\n安装中文支持: brew install tesseract-lang]").arg(lang);
            }
        }

        return tr("[Tesseract 识别失败] %1").arg(err.trimmed());
    }
    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

QString ImageTextRecognition::recognizeWithPaddle(const QString& imagePath)
{
    QString cmd = m_paddleOcrPath.isEmpty() ? "paddleocr" : m_paddleOcrPath;

    // 新版 PaddleOCR CLI: paddleocr ocr -i image.png
    // 旧版: paddleocr --image_dir image.png --lang ch
    QProcess process;

    // 先尝试新版语法
    process.start(cmd, QStringList() << "ocr" << "-i" << imagePath);
    if (!process.waitForStarted(3000)) {
        return tr("[PaddleOCR 识别失败] 无法启动 paddleocr");
    }
    process.waitForFinished(120000); // 首次可能需要下载模型

    QString output = QString::fromUtf8(process.readAllStandardOutput());
    QString err = QString::fromUtf8(process.readAllStandardError());

    // 如果新版语法失败，尝试旧版
    if (process.exitCode() != 0 && (err.contains("invalid choice") || output.isEmpty())) {
        QProcess oldProcess;
        QString lang = "ch";
        QString langSel = m_langCombo->currentText();
        if (langSel.startsWith("eng")) lang = "en";
        else if (langSel.startsWith("jpn")) lang = "japan";
        else if (langSel.startsWith("kor")) lang = "korean";

        oldProcess.start(cmd, QStringList()
            << "--image_dir" << imagePath
            << "--lang" << lang
            << "--use_angle_cls" << "true"
            << "--show_log" << "false");
        oldProcess.waitForFinished(120000);

        if (oldProcess.exitCode() != 0) {
            QString oldErr = QString::fromUtf8(oldProcess.readAllStandardError());
            return tr("[PaddleOCR 识别失败] %1").arg(oldErr.trimmed().left(200));
        }
        output = QString::fromUtf8(oldProcess.readAllStandardOutput());
    } else if (process.exitCode() != 0) {
        return tr("[PaddleOCR 识别失败] %1").arg(err.trimmed().left(200));
    }

    // 解析输出——提取文字部分
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    QStringList textLines;

    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;

        // 新版输出格式: {'text': '文字', 'score': 0.98, ...}
        // 旧版输出格式: ('文字', 0.98) 或 [('文字', 0.98)]
        QRegularExpression textRegex(R"('text'\s*:\s*'([^']*)')");
        QRegularExpressionMatch match = textRegex.match(trimmed);
        if (match.hasMatch()) {
            textLines.append(match.captured(1));
            continue;
        }

        // 旧版格式
        int start = trimmed.indexOf('\'');
        int end = trimmed.indexOf('\'', start + 1);
        if (start >= 0 && end > start) {
            QString text = trimmed.mid(start + 1, end - start - 1);
            if (!text.isEmpty() && text != "text") {
                textLines.append(text);
            }
            continue;
        }

        // 跳过日志行
        if (trimmed.startsWith('[') || trimmed.startsWith('{') || trimmed.startsWith('(')
            || trimmed.contains("ppocr") || trimmed.contains("WARNING")
            || trimmed.contains("INFO") || trimmed.contains("DEBUG"))
            continue;

        // 其他普通文本行
        textLines.append(trimmed);
    }

    if (textLines.isEmpty()) {
        return tr("[PaddleOCR 识别完成但未提取到文字]\n\n原始输出:\n%1").arg(output.left(500));
    }

    return textLines.join('\n');
}

bool ImageTextRecognition::checkPaddleOcr()
{
    QString cmd = m_paddleOcrPath.isEmpty() ? "paddleocr" : m_paddleOcrPath;
    QProcess p;
    p.start(cmd, QStringList() << "--help");
    if (!p.waitForStarted(3000)) return false; // 命令不存在
    p.waitForFinished(5000);
    return p.exitStatus() == QProcess::NormalExit && p.error() == QProcess::UnknownError;
}

bool ImageTextRecognition::checkTesseract()
{
    QProcess p;
    p.start("tesseract", QStringList() << "--version");
    if (!p.waitForStarted(3000)) return false; // 命令不存在
    p.waitForFinished(5000);
    return p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
}

void ImageTextRecognition::showInstallDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("安装 OCR 引擎"));
    dialog.setMinimumWidth(520);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setSpacing(10);
    layout->setContentsMargins(16, 12, 16, 12);

    auto* infoLabel = new QLabel(tr(
        "未检测到可用的 OCR 引擎，请选择安装："));
    infoLabel->setStyleSheet("font-size:10pt; font-weight:bold; color:#212529;");
    layout->addWidget(infoLabel);

    // Tesseract
    auto* tessFrame = new QWidget();
    auto* tessLayout = new QVBoxLayout(tessFrame);
    tessLayout->setContentsMargins(8, 8, 8, 8);
    tessFrame->setStyleSheet("background:#f8f9fa; border:1px solid #dee2e6; border-radius:6px;");

    auto* tessTitle = new QLabel(tr("Tesseract OCR（推荐，轻量）"));
    tessTitle->setStyleSheet("font-weight:bold; font-size:9pt;");
    tessLayout->addWidget(tessTitle);

    auto* tessDesc = new QLabel(tr("Google 开源 OCR 引擎，支持 100+ 语言"));
    tessDesc->setStyleSheet("color:#868e96; font-size:8pt;");
    tessLayout->addWidget(tessDesc);

#ifdef Q_OS_MACOS
    QString tessCmd = "brew install tesseract tesseract-lang";
#elif defined(Q_OS_LINUX)
    QString tessCmd = "sudo apt install -y tesseract-ocr tesseract-ocr-chi-sim tesseract-ocr-eng";
#else
    QString tessCmd = "";
#endif

    auto* tessBtn = new QPushButton(tr("一键安装 Tesseract"));
    tessBtn->setStyleSheet(
        "QPushButton { padding:6px 16px; background:#228be6; color:white; border:none; border-radius:4px; font-size:9pt; }"
        "QPushButton:hover { background:#1c7ed6; }");
    tessLayout->addWidget(tessBtn);

#ifdef Q_OS_WIN
    tessBtn->setText(tr("打开下载页面"));
    connect(tessBtn, &QPushButton::clicked, &dialog, [&dialog]() {
        QDesktopServices::openUrl(QUrl("https://github.com/UB-Mannheim/tesseract/wiki"));
        dialog.accept();
    });
#else
    QString tessCmdCopy = tessCmd;
    connect(tessBtn, &QPushButton::clicked, &dialog, [this, tessCmdCopy, &dialog]() {
        m_statusLabel->setText(tr("正在安装 Tesseract..."));
        dialog.accept();
        QProcess* proc = new QProcess(this);
        QString cmdForError = tessCmdCopy;
        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, proc, cmdForError](int exitCode, QProcess::ExitStatus) {
            if (exitCode == 0) {
                m_statusLabel->setText(tr("Tesseract 安装成功！"));
                QMessageBox::information(this, tr("安装完成"), tr("Tesseract OCR 安装成功，可以开始识别了。"));
            } else {
                QString err = QString::fromUtf8(proc->readAllStandardError());
                m_statusLabel->setText(tr("安装失败"));
                QMessageBox::critical(this, tr("安装失败"), tr("Tesseract 安装失败:\n%1\n\n请手动执行:\n%2").arg(err.trimmed(), cmdForError));
            }
            proc->deleteLater();
        });
#ifdef Q_OS_MACOS
        proc->start("brew", QStringList() << "install" << "tesseract" << "tesseract-lang");
#else
        proc->start("bash", QStringList() << "-c" << tessCmd);
#endif
    });
#endif

    layout->addWidget(tessFrame);

    // PaddleOCR
    auto* paddleFrame = new QWidget();
    auto* paddleLayout = new QVBoxLayout(paddleFrame);
    paddleLayout->setContentsMargins(8, 8, 8, 8);
    paddleFrame->setStyleSheet("background:#f8f9fa; border:1px solid #dee2e6; border-radius:6px;");

    auto* paddleTitle = new QLabel(tr("PaddleOCR（高精度，中文推荐）"));
    paddleTitle->setStyleSheet("font-weight:bold; font-size:9pt;");
    paddleLayout->addWidget(paddleTitle);

    auto* paddleDesc = new QLabel(tr("百度开源 OCR，精度更高，首次运行需下载模型（~100MB）"));
    paddleDesc->setStyleSheet("color:#868e96; font-size:8pt;");
    paddleLayout->addWidget(paddleDesc);

    auto* paddleBtn = new QPushButton(tr("一键安装 PaddleOCR"));
    paddleBtn->setStyleSheet(
        "QPushButton { padding:6px 16px; background:#40c057; color:white; border:none; border-radius:4px; font-size:9pt; }"
        "QPushButton:hover { background:#37b24d; }");
    paddleLayout->addWidget(paddleBtn);

    connect(paddleBtn, &QPushButton::clicked, &dialog, [this, &dialog]() {
        m_statusLabel->setText(tr("正在安装 PaddleOCR（可能需要几分钟）..."));
        dialog.accept();
        QProcess* proc = new QProcess(this);
        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, proc](int exitCode, QProcess::ExitStatus) {
            if (exitCode == 0) {
                m_statusLabel->setText(tr("PaddleOCR 安装成功！"));
                QMessageBox::information(this, tr("安装完成"), tr("PaddleOCR 安装成功，可以开始识别了。\n首次识别会自动下载模型。"));
            } else {
                QString err = QString::fromUtf8(proc->readAllStandardError());
                m_statusLabel->setText(tr("安装失败"));
                QMessageBox::critical(this, tr("安装失败"),
                    tr("PaddleOCR 安装失败:\n%1\n\n请手动执行:\npip install paddleocr paddlepaddle").arg(err.trimmed()));
            }
            proc->deleteLater();
        });
        proc->start("pip3", QStringList() << "install" << "paddleocr" << "paddlepaddle");
    });

    layout->addWidget(paddleFrame);

    // 取消
    auto* cancelBtn = new QPushButton(tr("稍后安装"));
    cancelBtn->setStyleSheet("font-size:9pt;");
    layout->addWidget(cancelBtn);
    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

    dialog.exec();
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
