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
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryFile>
#include <QProcessEnvironment>
#include <QShortcut>
#include <QKeySequence>

REGISTER_DYNAMICOBJECT(ImageTextRecognition);

// macOS .app 的 PATH 很短，需要补充常见路径才能找到 paddleocr / tesseract
static void enrichProcessPath(QProcess& proc)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString path = env.value("PATH");
    QStringList extraPaths = {
        "/opt/homebrew/bin",
        "/opt/homebrew/sbin",
        "/opt/homebrew/Caskroom/miniconda/base/bin",
        "/opt/homebrew/Caskroom/miniconda/base/condabin",
        "/usr/local/bin",
        "/usr/local/sbin",
        QDir::homePath() + "/miniconda3/bin",
        QDir::homePath() + "/anaconda3/bin",
        QDir::homePath() + "/.local/bin",
    };
    for (const auto& p : extraPaths) {
        if (QDir(p).exists() && !path.contains(p))
            path = p + ":" + path;
    }
    env.insert("PATH", path);
    proc.setProcessEnvironment(env);
}

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

    // Status bar with service controls
    auto* statusBar = new QHBoxLayout();
    statusBar->setContentsMargins(0, 0, 0, 0);
    statusBar->setSpacing(6);

    m_statusLabel = new QLabel(tr("共 0 张图片，已识别 0 张"));
    m_statusLabel->setStyleSheet("color:#868e96; font-size:8pt; padding:2px 0;");
    statusBar->addWidget(m_statusLabel);
    statusBar->addStretch();

    m_serviceStatusLabel = new QLabel(tr("● PaddleOCR 服务未启动"));
    m_serviceStatusLabel->setStyleSheet("color:#adb5bd; font-size:8pt;");
    statusBar->addWidget(m_serviceStatusLabel);

    m_serviceModeCombo = new QComboBox();
    m_serviceModeCombo->addItems({tr("极速"), tr("精准")});
    m_serviceModeCombo->setCurrentIndex(0);
    m_serviceModeCombo->setToolTip(tr("极速：轻量模型，速度快\n精准：大模型，精度高但较慢"));
    m_serviceModeCombo->setStyleSheet("QComboBox { font-size:8pt; padding:1px 4px; border:1px solid #dee2e6; border-radius:3px; }");
    statusBar->addWidget(m_serviceModeCombo);

    m_serviceBtn = new QPushButton(tr("启动服务"));
    m_serviceBtn->setStyleSheet(
        "QPushButton { font-size:8pt; padding:2px 8px; border:1px solid #dee2e6; border-radius:3px; background:white; color:#495057; }"
        "QPushButton:hover { background:#f1f3f5; }");
    statusBar->addWidget(m_serviceBtn);

    mainLayout->addLayout(statusBar);

    connect(m_serviceBtn, &QPushButton::clicked, this, [this]() {
        if (m_paddleService && m_paddleService->state() == QProcess::Running)
            stopPaddleService();
        else
            startPaddleService();
    });

    // Cmd+V / Ctrl+V 快捷粘贴
    auto* pasteShortcut = new QShortcut(QKeySequence::Paste, this);
    connect(pasteShortcut, &QShortcut::activated, this, &ImageTextRecognition::onPasteFromClipboard);

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

    // 优先检查文件 URLs（复制多张图片文件时走这条路径）
    if (mimeData->hasUrls()) {
        QStringList files;
        for (const QUrl& url : mimeData->urls()) {
            if (url.isLocalFile()) {
                QString path = url.toLocalFile();
                // 过滤只保留图片文件
                static const QStringList imgExts = {"png", "jpg", "jpeg", "bmp", "tiff", "tif", "webp", "gif"};
                if (imgExts.contains(QFileInfo(path).suffix().toLower()))
                    files.append(path);
            }
        }
        if (!files.isEmpty()) {
            addImageFiles(files);
            return;
        }
    }

    // 其次检查图片数据（截图、单张复制等）
    if (mimeData->hasImage()) {
        QPixmap pixmap = qvariant_cast<QPixmap>(mimeData->imageData());
        if (!pixmap.isNull()) {
            addImageFromPixmap(pixmap);
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

    m_statusLabel->setText(tr("正在检测 OCR 引擎..."));
    QApplication::processEvents();
    bool hasTess = checkTesseract();
    bool hasPaddle = checkPaddleOcr();
    if (!hasPaddle && !hasTess) {
        showInstallDialog();
        return;
    }

    QList<int> pendingIndexes;
    for (int i = 0; i < m_items.size(); ++i)
        if (!m_items[i].recognized) pendingIndexes.append(i);
    if (pendingIndexes.isEmpty()) return;

    m_recognizeBtn->setEnabled(false);
    int engineSel = m_engineCombo->currentIndex();
    bool usePaddle = (engineSel == Auto && hasPaddle) || engineSel == PaddleOCR;

    if (usePaddle) {
        if (m_serviceReady && m_paddleService && m_paddleService->state() == QProcess::Running) {
            // 使用常驻服务（最快）
            recognizeViaService(pendingIndexes, hasTess, engineSel);
        } else {
            // 批量模式
            startPaddleBatch(pendingIndexes, hasTess, engineSel);
        }
    } else {
        // Tesseract 逐张识别（Tesseract 本身启动很快）
        processNextTesseract(0, pendingIndexes);
    }
}

void ImageTextRecognition::recognizeSingle(int itemIdx)
{
    if (itemIdx < 0 || itemIdx >= m_items.size()) return;

    bool hasTess = checkTesseract();
    bool hasPaddle = checkPaddleOcr();
    int engineSel = m_engineCombo->currentIndex();
    bool usePaddle = (engineSel == Auto && hasPaddle) || engineSel == PaddleOCR;

    if (!hasPaddle && !hasTess) {
        showInstallDialog();
        return;
    }

    // 重置状态以便重新识别
    m_items[itemIdx].recognized = false;
    m_items[itemIdx].resultText.clear();
    m_items[itemIdx].elapsedMs = 0;
    rebuildResultsView();

    QList<int> single = {itemIdx};

    if (usePaddle) {
        if (m_serviceReady && m_paddleService && m_paddleService->state() == QProcess::Running) {
            recognizeViaService(single, hasTess, engineSel);
        } else {
            processNextPaddleCli(0, single, hasTess, engineSel);
        }
    } else {
        processNextTesseract(0, single);
    }
}

void ImageTextRecognition::startPaddleBatch(const QList<int>& indexes, bool hasTess, int engineSel)
{
    // 逐张调用 PaddleOCR CLI（每张都立即返回结果+耗时）
    processNextPaddleCli(0, indexes, hasTess, engineSel);
}

void ImageTextRecognition::processNextPaddleCli(int idx, const QList<int>& indexes, bool hasTess, int engineSel)
{
    if (idx >= indexes.size()) {
        m_recognizeBtn->setEnabled(true);
        int rec = 0;
        for (const auto& it : m_items) if (it.recognized) rec++;
        m_statusLabel->setText(tr("识别完成，共 %1 张").arg(rec));
        return;
    }

    int itemIdx = indexes[idx];
    m_statusLabel->setText(tr("正在识别第 %1/%2 张（PaddleOCR CLI）...").arg(idx + 1).arg(indexes.size()));

    QString jsonOutDir = QDir::tempPath() + "/lele_ocr_" + QUuid::createUuid().toString(QUuid::Id128);
    QDir().mkpath(jsonOutDir);

    QElapsedTimer timer;
    timer.start();

    auto* proc = new QProcess(this);
    enrichProcessPath(*proc);
    QString cmd = m_paddleOcrPath.isEmpty() ? "paddleocr" : m_paddleOcrPath;
    proc->setProgram(cmd);
    QStringList args = {"ocr", "-i", m_items[itemIdx].imagePath, "--save_path", jsonOutDir,
        "--use_doc_orientation_classify", "False",
        "--use_doc_unwarping", "False",
        "--use_textline_orientation", "False"};
    if (m_serviceModeCombo->currentIndex() == 0) {
        args << "--text_detection_model_name" << "PP-OCRv5_mobile_det"
             << "--text_recognition_model_name" << "PP-OCRv5_mobile_rec";
    }
    proc->setArguments(args);

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this, [this, proc, itemIdx, idx, indexes, hasTess, engineSel, jsonOutDir, timer]
        (int, QProcess::ExitStatus) {

        qint64 elapsed = timer.elapsed();
        QString result = parsePaddleJsonResult(jsonOutDir);
        QDir(jsonOutDir).removeRecursively();

        if (result.isEmpty() && hasTess && engineSel == Auto) {
            QProcess fb;
            enrichProcessPath(fb);
            fb.start("tesseract", {m_items[itemIdx].imagePath, "stdout", "-l", m_langCombo->currentText()});
            fb.waitForFinished(30000);
            result = QString::fromUtf8(fb.readAllStandardOutput()).trimmed();
            m_items[itemIdx].engineUsed = "Tesseract";
        } else {
            m_items[itemIdx].engineUsed = "PaddleOCR";
        }

        m_items[itemIdx].resultText = result.isEmpty() ? tr("[未识别到文字]") : result;
        m_items[itemIdx].recognized = true;
        m_items[itemIdx].elapsedMs = elapsed;

        rebuildResultsView();
        proc->deleteLater();

        QTimer::singleShot(0, this, [this, idx, indexes, hasTess, engineSel]() {
            processNextPaddleCli(idx + 1, indexes, hasTess, engineSel);
        });
    });

    proc->start();
}

void ImageTextRecognition::processNextTesseract(int idx, const QList<int>& indexes)
{
    if (idx >= indexes.size()) {
        m_recognizeBtn->setEnabled(true);
        int rec = 0;
        for (const auto& it : m_items) if (it.recognized) rec++;
        m_statusLabel->setText(tr("识别完成，共 %1 张").arg(rec));
        return;
    }

    int itemIdx = indexes[idx];
    m_statusLabel->setText(tr("正在识别第 %1/%2 张（Tesseract）...").arg(idx + 1).arg(indexes.size()));

    QElapsedTimer timer;
    timer.start();

    auto* proc = new QProcess(this);
    enrichProcessPath(*proc);
    proc->setProgram("tesseract");
    proc->setArguments({m_items[itemIdx].imagePath, "stdout", "-l", m_langCombo->currentText()});

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this, [this, proc, itemIdx, idx, indexes, timer](int, QProcess::ExitStatus) {

        QString result = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        m_items[itemIdx].resultText = result.isEmpty() ? tr("[未识别到文字]") : result;
        m_items[itemIdx].engineUsed = "Tesseract";
        m_items[itemIdx].recognized = true;
        m_items[itemIdx].elapsedMs = timer.elapsed();

        rebuildResultsView();

        proc->deleteLater();
        QTimer::singleShot(0, this, [this, idx, indexes]() {
            processNextTesseract(idx + 1, indexes);
        });
    });

    proc->start();
}

// ── PaddleOCR 常驻服务 ──

void ImageTextRecognition::startPaddleService()
{
    if (m_paddleService && m_paddleService->state() == QProcess::Running) {
        return; // 已运行
    }

    // 查找 python3 解释器
    QProcess which;
    enrichProcessPath(which);
    which.start("which", {"python3"});
    which.waitForFinished(3000);
    QString pythonPath = QString::fromUtf8(which.readAllStandardOutput()).trimmed();
    if (pythonPath.isEmpty()) pythonPath = "python3";

    // 脚本路径（与可执行文件同目录，或源码目录）
    QString scriptPath;
    QStringList candidates = {
        QCoreApplication::applicationDirPath() + "/paddle_service.py",
        QCoreApplication::applicationDirPath() + "/../Resources/paddle_service.py",
    };
    // 开发模式：从源码目录加载
    QString srcScript = QFileInfo(__FILE__).absolutePath() + "/paddle_service.py";
    candidates.prepend(srcScript);

    for (const auto& p : candidates) {
        if (QFile::exists(p)) { scriptPath = p; break; }
    }
    if (scriptPath.isEmpty()) {
        m_serviceStatusLabel->setText(tr("● 脚本未找到"));
        m_serviceStatusLabel->setStyleSheet("color:#e03131; font-size:8pt;");
        return;
    }

    m_paddleService = new QProcess(this);
    enrichProcessPath(*m_paddleService);
    // 禁用 Python 缓冲
    QProcessEnvironment env = m_paddleService->processEnvironment();
    env.insert("PYTHONUNBUFFERED", "1");
    m_paddleService->setProcessEnvironment(env);

    m_serviceBuffer.clear();
    m_serviceReady = false;

    connect(m_paddleService, &QProcess::readyReadStandardOutput, this, &ImageTextRecognition::onServiceReadyRead);
    connect(m_paddleService, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ImageTextRecognition::onServiceFinished);

    QString mode = (m_serviceModeCombo->currentIndex() == 0) ? "fast" : "accurate";
    m_paddleService->start(pythonPath, {"-u", scriptPath, "--mode", mode});

    m_serviceStatusLabel->setText(tr("● 服务启动中..."));
    m_serviceStatusLabel->setStyleSheet("color:#f59f00; font-size:8pt;");
    m_serviceBtn->setText(tr("停止服务"));
    m_serviceModeCombo->setEnabled(false);
}

void ImageTextRecognition::stopPaddleService()
{
    if (!m_paddleService) return;

    m_serviceReady = false;
    m_paddleService->closeWriteChannel(); // 通知 Python 退出
    if (!m_paddleService->waitForFinished(3000)) {
        m_paddleService->kill();
        m_paddleService->waitForFinished(1000);
    }
    m_paddleService->deleteLater();
    m_paddleService = nullptr;

    m_serviceStatusLabel->setText(tr("● 服务已停止"));
    m_serviceStatusLabel->setStyleSheet("color:#adb5bd; font-size:8pt;");
    m_serviceBtn->setText(tr("启动服务"));
    m_serviceModeCombo->setEnabled(true);
}

void ImageTextRecognition::onServiceReadyRead()
{
    if (!m_paddleService) return;

    m_serviceBuffer.append(m_paddleService->readAllStandardOutput());

    while (true) {
        int nlPos = m_serviceBuffer.indexOf('\n');
        if (nlPos < 0) break;

        QByteArray line = m_serviceBuffer.left(nlPos).trimmed();
        m_serviceBuffer.remove(0, nlPos + 1);

        if (line.isEmpty()) continue;

        QJsonDocument doc = QJsonDocument::fromJson(line);
        if (!doc.isObject()) continue;
        QJsonObject obj = doc.object();

        // 启动就绪信号
        if (obj.contains("status")) {
            QString status = obj["status"].toString();
            if (status == "ready") {
                m_serviceReady = true;
                m_serviceStatusLabel->setText(tr("● 服务运行中"));
                m_serviceStatusLabel->setStyleSheet("color:#40c057; font-size:8pt;");
                m_serviceBtn->setText(tr("停止服务"));
            } else if (status == "error") {
                m_serviceStatusLabel->setText(tr("● 启动失败: %1").arg(obj["message"].toString()));
                m_serviceStatusLabel->setStyleSheet("color:#e03131; font-size:8pt;");
            }
            continue;
        }

        // OCR 识别结果响应（单张）
        if (obj.contains("id") && obj["id"].toString() == m_pendingRequestId) {
            qint64 elapsed = m_serviceTimer.elapsed();
            int itemIdx = m_serviceQueue.value(m_serviceQueuePos, -1);

            if (itemIdx >= 0 && itemIdx < m_items.size()) {
                QJsonArray texts = obj["texts"].toArray();
                QJsonArray polys = obj["polys"].toArray();

                QString layoutText = buildLayoutText(texts, polys);

                if (layoutText.isEmpty() && m_pendingHasTess && m_pendingEngineSel == Auto) {
                    QProcess fb;
                    enrichProcessPath(fb);
                    fb.start("tesseract", {m_items[itemIdx].imagePath, "stdout", "-l", m_langCombo->currentText()});
                    fb.waitForFinished(30000);
                    layoutText = QString::fromUtf8(fb.readAllStandardOutput()).trimmed();
                    m_items[itemIdx].engineUsed = "Tesseract";
                } else {
                    m_items[itemIdx].engineUsed = "PaddleOCR";
                }

                m_items[itemIdx].resultText = layoutText.isEmpty() ? tr("[未识别到文字]") : layoutText;
                m_items[itemIdx].recognized = true;
                m_items[itemIdx].elapsedMs = elapsed;

                // 立即刷新 UI 显示这张的结果
                rebuildResultsView();
            }

            // 发送下一张
            m_serviceQueuePos++;
            QTimer::singleShot(0, this, [this]() { serviceSendNext(); });
        }
    }
}

void ImageTextRecognition::onServiceFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(exitCode)
    Q_UNUSED(status)
    m_serviceReady = false;
    m_serviceStatusLabel->setText(tr("● 服务已退出"));
    m_serviceStatusLabel->setStyleSheet("color:#e03131; font-size:8pt;");
    m_serviceBtn->setText(tr("启动服务"));
    m_serviceModeCombo->setEnabled(true);

    if (m_paddleService) {
        m_paddleService->deleteLater();
        m_paddleService = nullptr;
    }

    // 如果有挂起的请求，回退到批量模式
    if (!m_pendingRequestId.isEmpty()) {
        m_pendingRequestId.clear();
        m_statusLabel->setText(tr("服务异常退出，正在切换批量模式..."));
        QList<int> indexes = m_serviceQueue;
        m_serviceQueue.clear();
        startPaddleBatch(indexes, m_pendingHasTess, m_pendingEngineSel);
    }
}

void ImageTextRecognition::recognizeViaService(const QList<int>& indexes, bool hasTess, int engineSel)
{
    m_serviceQueue = indexes;
    m_serviceQueuePos = 0;
    m_pendingHasTess = hasTess;
    m_pendingEngineSel = engineSel;

    serviceSendNext();
}

void ImageTextRecognition::serviceSendNext()
{
    if (m_serviceQueuePos >= m_serviceQueue.size()) {
        // 全部完成
        m_pendingRequestId.clear();
        m_serviceQueue.clear();
        m_recognizeBtn->setEnabled(true);
        int rec = 0;
        for (const auto& it : m_items) if (it.recognized) rec++;
        m_statusLabel->setText(tr("识别完成，共 %1 张").arg(rec));
        return;
    }

    int itemIdx = m_serviceQueue[m_serviceQueuePos];
    m_statusLabel->setText(tr("正在识别第 %1/%2 张...").arg(m_serviceQueuePos + 1).arg(m_serviceQueue.size()));

    m_pendingRequestId = QUuid::createUuid().toString(QUuid::Id128);
    m_serviceTimer.start();

    QJsonObject req;
    req["id"] = m_pendingRequestId;
    req["image"] = m_items[itemIdx].imagePath;

    QByteArray data = QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n";
    m_paddleService->write(data);
}

QString ImageTextRecognition::buildLayoutText(const QJsonArray& texts, const QJsonArray& polys)
{
    if (texts.isEmpty()) return {};

    struct TextBlock {
        QString text;
        int xMin, yMin, xMax, yMax;
    };
    QList<TextBlock> blocks;

    for (int i = 0; i < texts.size(); ++i) {
        QString text = texts[i].toString().trimmed();
        if (text.isEmpty()) continue;

        if (i < polys.size()) {
            QJsonArray poly = polys[i].toArray();
            int xMin = INT_MAX, yMin = INT_MAX, xMax = 0, yMax = 0;
            for (const auto& pt : poly) {
                QJsonArray p = pt.toArray();
                int x = p[0].toInt();
                int y = p[1].toInt();
                xMin = qMin(xMin, x);
                yMin = qMin(yMin, y);
                xMax = qMax(xMax, x);
                yMax = qMax(yMax, y);
            }
            blocks.append({text, xMin, yMin, xMax, yMax});
        } else {
            blocks.append({text, 0, i * 30, 100, i * 30 + 20});
        }
    }

    if (blocks.isEmpty()) return {};

    // 按 yMin 排序
    std::sort(blocks.begin(), blocks.end(), [](const TextBlock& a, const TextBlock& b) {
        return a.yMin < b.yMin;
    });

    // 分行
    QList<QList<TextBlock>> lines;
    QList<TextBlock> currentLine;
    currentLine.append(blocks[0]);

    for (int i = 1; i < blocks.size(); ++i) {
        int prevMidY = (currentLine.last().yMin + currentLine.last().yMax) / 2;
        int curMidY = (blocks[i].yMin + blocks[i].yMax) / 2;
        int lineHeight = currentLine.last().yMax - currentLine.last().yMin;
        int threshold = qMax(lineHeight / 2, 10);

        if (qAbs(curMidY - prevMidY) <= threshold) {
            currentLine.append(blocks[i]);
        } else {
            lines.append(currentLine);
            currentLine.clear();
            currentLine.append(blocks[i]);
        }
    }
    if (!currentLine.isEmpty())
        lines.append(currentLine);

    // 平均字符宽度
    double totalCharWidth = 0;
    int charCount = 0;
    for (const auto& b : blocks) {
        if (!b.text.isEmpty()) {
            totalCharWidth += (b.xMax - b.xMin);
            charCount += b.text.length();
        }
    }
    double avgCharWidth = (charCount > 0) ? (totalCharWidth / charCount) : 10.0;

    // 构建布局文本
    QStringList resultLines;
    for (auto& line : lines) {
        std::sort(line.begin(), line.end(), [](const TextBlock& a, const TextBlock& b) {
            return a.xMin < b.xMin;
        });

        QString lineText;
        int curCol = 0;
        for (const auto& b : line) {
            int targetCol = static_cast<int>(b.xMin / avgCharWidth);
            if (targetCol > curCol) {
                lineText += QString(targetCol - curCol, ' ');
            }
            lineText += b.text;
            curCol = targetCol + b.text.length();
        }
        while (lineText.endsWith(' ')) lineText.chop(1);
        resultLines.append(lineText);
    }

    return resultLines.join('\n');
}

QString ImageTextRecognition::parsePaddleJsonResult(const QString& jsonDir, const QString& jsonFileName)
{
    QDir dir(jsonDir);
    QString filePath;
    if (!jsonFileName.isEmpty()) {
        filePath = dir.filePath(jsonFileName);
    } else {
        QStringList jsonFiles = dir.entryList({"*_res.json"}, QDir::Files);
        if (jsonFiles.isEmpty())
            jsonFiles = dir.entryList({"*.json"}, QDir::Files);
        if (jsonFiles.isEmpty())
            return {};
        filePath = dir.filePath(jsonFiles.first());
    }

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
        return {};

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject())
        return {};

    QJsonObject root = doc.object();
    QJsonArray textsArr = root["rec_texts"].toArray();
    QJsonArray polysArr = root["dt_polys"].toArray();

    return buildLayoutText(textsArr, polysArr);
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

        auto* btnRow = new QHBoxLayout();

        // 显示引擎标签 + 耗时
        if (item.recognized && !item.engineUsed.isEmpty()) {
            auto* engineLabel = new QLabel(item.engineUsed);
            QString badgeColor = (item.engineUsed == "PaddleOCR") ? "#1890ff" : "#52c41a";
            engineLabel->setStyleSheet(
                QString("QLabel { color: white; background: %1; border-radius: 3px; "
                        "padding: 2px 8px; font-size: 11px; }").arg(badgeColor));
            btnRow->addWidget(engineLabel);

            if (item.elapsedMs > 0) {
                QString timeStr;
                if (item.elapsedMs >= 1000)
                    timeStr = QString("%1s").arg(item.elapsedMs / 1000.0, 0, 'f', 1);
                else
                    timeStr = QString("%1ms").arg(item.elapsedMs);
                auto* timeLabel = new QLabel(timeStr);
                timeLabel->setStyleSheet("QLabel { color: #868e96; font-size: 11px; padding: 2px 4px; }");
                btnRow->addWidget(timeLabel);
            }
        }

        btnRow->addStretch();

        auto* recBtn = new QPushButton(item.recognized ? tr("重新识别") : tr("识别"));
        recBtn->setFixedWidth(70);
        connect(recBtn, &QPushButton::clicked, this, [this, i]() {
            recognizeSingle(i);
        });
        btnRow->addWidget(recBtn);

        auto* copyBtn = new QPushButton(tr("复制"));
        copyBtn->setFixedWidth(50);
        QString resultText = item.resultText;
        connect(copyBtn, &QPushButton::clicked, this, [resultText]() {
            if (!resultText.isEmpty()) {
                QApplication::clipboard()->setText(resultText);
            }
        });
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
    enrichProcessPath(process);
    process.start("tesseract", QStringList() << imagePath << "stdout" << "-l" << lang);
    process.waitForFinished(30000);

    if (process.exitCode() != 0) {
        QString err = QString::fromUtf8(process.readAllStandardError());

        // 如果是语言包缺失，自动降级到 eng
        if (err.contains("Failed loading language") && lang != "eng") {
            QProcess fallback;
            enrichProcessPath(fallback);
            fallback.start("tesseract", QStringList() << imagePath << "stdout" << "-l" << "eng");
            fallback.waitForFinished(30000);
            if (fallback.exitCode() == 0) {
                QString result = QString::fromUtf8(fallback.readAllStandardOutput()).trimmed();
                return result + tr("\n\n[注意: 语言包 %1 未安装，已降级为英文识别。\n安装中文支持: brew install tesseract-lang]").arg(lang);
            }
        }

        return tr("[Tesseract 识别失败] %1").arg(err.trimmed());
    }
    QString result = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    if (result.isEmpty()) {
        result = tr("[图片中未检测到文字]\n提示：安装中文语言包可提高识别率\nbrew install tesseract-lang");
    }
    return result;
}

QString ImageTextRecognition::recognizeWithPaddle(const QString& imagePath)
{
    QString cmd = m_paddleOcrPath.isEmpty() ? "paddleocr" : m_paddleOcrPath;

    // 新版 PaddleOCR CLI: paddleocr ocr -i image.png
    // 旧版: paddleocr --image_dir image.png --lang ch
    QProcess process;
    enrichProcessPath(process);

    // 先尝试新版语法
    process.start(cmd, QStringList() << "ocr" << "-i" << imagePath);
    if (!process.waitForStarted(3000)) {
        return tr("[PaddleOCR 识别失败] 无法启动 paddleocr");
    }
    process.waitForFinished(120000); // 首次可能需要下载模型

    // 合并 stdout + stderr（PaddleOCR 混合输出）
    QString output = QString::fromUtf8(process.readAllStandardOutput())
                   + QString::fromUtf8(process.readAllStandardError());

    // Strip ANSI 颜色码
    static QRegularExpression ansiRegex("\x1b\\[[0-9;]*m");
    output.remove(ansiRegex);

    // 如果新版语法失败，尝试旧版
    if (process.exitCode() != 0 && (output.contains("invalid choice") || output.trimmed().isEmpty())) {
        QProcess oldProcess;
        enrichProcessPath(oldProcess);
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
        output = QString::fromUtf8(oldProcess.readAllStandardOutput())
               + QString::fromUtf8(oldProcess.readAllStandardError());
        output.remove(ansiRegex);
    } else if (process.exitCode() != 0 && output.trimmed().isEmpty()) {
        return tr("[PaddleOCR 识别失败] %1").arg(output.trimmed().left(200));
    }

    // 解析输出——提取文字部分
    // PaddleOCR v3 新版输出格式包含 'rec_texts': ['文字1', '文字2', ...]
    QStringList textLines;

    // 方法1: 提取 rec_texts 数组
    QRegularExpression recTextsRegex(R"('rec_texts'\s*:\s*\[([^\]]*)\])");
    QRegularExpressionMatch recMatch = recTextsRegex.match(output);
    if (recMatch.hasMatch()) {
        QString textsStr = recMatch.captured(1);
        // 提取每个 '文字'
        QRegularExpression itemRegex(R"('([^']*)')");
        auto it = itemRegex.globalMatch(textsStr);
        while (it.hasNext()) {
            auto m = it.next();
            QString text = m.captured(1);
            if (!text.isEmpty())
                textLines.append(text);
        }
    }

    // 方法2: 提取 'text': '文字' 格式（旧版）
    if (textLines.isEmpty()) {
        QRegularExpression textRegex(R"('text'\s*:\s*'([^']*)')");
        auto it = textRegex.globalMatch(output);
        while (it.hasNext()) {
            textLines.append(it.next().captured(1));
        }
    }

    // 方法3: 旧版格式 ('文字', 0.98)
    if (textLines.isEmpty()) {
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        for (const QString& line : lines) {
            QString trimmed = line.trimmed();
            int start = trimmed.indexOf('\'');
            int end = trimmed.indexOf('\'', start + 1);
            if (start >= 0 && end > start) {
                QString text = trimmed.mid(start + 1, end - start - 1);
                if (!text.isEmpty() && text != "text" && text != "rec_texts")
                    textLines.append(text);
            }
        }
    }

    if (textLines.isEmpty()) {
        QString detail = output;
        return tr("[PaddleOCR 识别完成但未提取到文字]\n\n原始输出:\n%1").arg(detail.left(500));
    }

    return textLines.join('\n');
}

bool ImageTextRecognition::checkPaddleOcr()
{
    QString cmd = m_paddleOcrPath.isEmpty() ? "paddleocr" : m_paddleOcrPath;
    QProcess p;
    enrichProcessPath(p);
    p.start(cmd, QStringList() << "-v");
    if (!p.waitForStarted(2000)) return false;
    if (!p.waitForFinished(5000)) {
        p.kill();
        return false;
    }
    return p.exitStatus() == QProcess::NormalExit;
}

bool ImageTextRecognition::checkTesseract()
{
    QProcess p;
    enrichProcessPath(p);
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
        enrichProcessPath(*proc);
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
        enrichProcessPath(*proc);
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

    // 改为 QTextEdit 显示详细信息
    auto* detectResult = new QTextEdit();
    detectResult->setReadOnly(true);
    detectResult->setMaximumHeight(100);
    detectResult->setStyleSheet("font-size:8pt; font-family:Menlo,Consolas,monospace; background:#f8f9fa; border:1px solid #dee2e6; border-radius:4px;");
    layout->addWidget(detectResult);
    detectLabel->hide(); // 隐藏旧的

    connect(detectBtn, &QPushButton::clicked, &dialog, [this, detectResult, pathEdit]() {
        m_paddleOcrPath = pathEdit->text().trimmed();
        QString info;

        // 检测 Tesseract
        {
            QProcess p;
            enrichProcessPath(p);
            p.start("tesseract", QStringList() << "--version");
            if (p.waitForStarted(3000) && p.waitForFinished(5000) && p.exitCode() == 0) {
                QString ver = QString::fromUtf8(p.readAllStandardOutput()).trimmed().split('\n').first();
                // 获取路径
                QProcess which;
                enrichProcessPath(which);
                which.start("which", QStringList() << "tesseract");
                which.waitForFinished(3000);
                QString path = QString::fromUtf8(which.readAllStandardOutput()).trimmed();
                // 获取语言包
                QProcess langs;
                enrichProcessPath(langs);
                langs.start("tesseract", QStringList() << "--list-langs");
                langs.waitForFinished(3000);
                QString langList = QString::fromUtf8(langs.readAllStandardOutput()).trimmed();

                info += "Tesseract: 可用\n";
                info += "  版本: " + ver + "\n";
                info += "  路径: " + path + "\n";
                info += "  语言: " + langList.replace('\n', ", ") + "\n";
            } else {
                info += "Tesseract: 不可用\n";
            }
        }

        info += "\n";

        // 检测 PaddleOCR
        {
            QString cmd = m_paddleOcrPath.isEmpty() ? "paddleocr" : m_paddleOcrPath;
            QProcess p;
            enrichProcessPath(p);
            p.start(cmd, QStringList() << "-v");
            if (p.waitForStarted(3000) && p.waitForFinished(5000)) {
                QString ver = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
                if (ver.isEmpty()) ver = QString::fromUtf8(p.readAllStandardError()).trimmed();
                QProcess which;
                enrichProcessPath(which);
                which.start("which", QStringList() << cmd);
                which.waitForFinished(3000);
                QString path = QString::fromUtf8(which.readAllStandardOutput()).trimmed();

                info += "PaddleOCR: 可用\n";
                info += "  版本: " + ver.split('\n').first() + "\n";
                info += "  路径: " + (path.isEmpty() ? cmd : path) + "\n";
            } else {
                info += "PaddleOCR: 不可用\n";
            }
        }

        detectResult->setPlainText(info);
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
