#include "faviconproduction.h"
#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>
#include <QPainter>
#include <QImageReader>
#include <QMimeData>
#include <QToolBar>
#include <QFrame>

REGISTER_DYNAMICOBJECT(FaviconProduction);

FaviconProduction::FaviconProduction()
    : QWidget(nullptr)
    , DynamicObjectBase()
{
    setupUI();
    setAcceptDrops(true);

    m_outputDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/app-icons";
    m_outputPathEdit->setText(m_outputDir);
}

FaviconProduction::~FaviconProduction() {}

void FaviconProduction::setupUI()
{
    const QString btnStyle =
        "QPushButton { padding:4px 10px; border:none; border-radius:4px; font-size:9pt; color:#495057; background:transparent; }"
        "QPushButton:hover { background-color:#e9ecef; }"
        "QPushButton:disabled { color:#adb5bd; }";

    const QString checkStyle = "QCheckBox { font-size:9pt; }";
    const QString labelStyle = "QLabel { font-size:9pt; color:#495057; }";

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // --- Toolbar ---
    auto* toolbar = new QWidget();
    toolbar->setStyleSheet("background:#f8f9fa; border-bottom:1px solid #dee2e6;");
    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(8, 4, 8, 4);
    toolbarLayout->setSpacing(2);

    m_selectBtn = new QPushButton(tr("选择图片"));
    m_selectBtn->setStyleSheet(btnStyle);
    m_clearBtn = new QPushButton(tr("清空"));
    m_clearBtn->setStyleSheet(btnStyle);
    m_generateBtn = new QPushButton(tr("生成全部"));
    m_generateBtn->setStyleSheet(
        "QPushButton { padding:4px 10px; border:none; border-radius:4px; font-size:9pt; color:#228be6; font-weight:bold; background:transparent; }"
        "QPushButton:hover { background-color:#e9ecef; }"
        "QPushButton:disabled { color:#adb5bd; font-weight:bold; }");
    m_generateBtn->setEnabled(false);
    m_openDirBtn = new QPushButton(tr("打开输出目录"));
    m_openDirBtn->setStyleSheet(btnStyle);

    toolbarLayout->addWidget(m_selectBtn);
    toolbarLayout->addWidget(m_clearBtn);
    toolbarLayout->addWidget(m_generateBtn);
    toolbarLayout->addWidget(m_openDirBtn);
    toolbarLayout->addStretch();

    mainLayout->addWidget(toolbar);

    // --- Middle area: source preview (left) + settings (right) ---
    auto* middleWidget = new QWidget();
    auto* middleLayout = new QHBoxLayout(middleWidget);
    middleLayout->setContentsMargins(12, 10, 12, 6);
    middleLayout->setSpacing(20);

    // Left: preview
    auto* leftWidget = new QWidget();
    auto* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(6);

    m_previewLabel = new QLabel();
    m_previewLabel->setFixedSize(200, 200);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet(
        "QLabel { border:2px dashed #ced4da; border-radius:6px; background:#f8f9fa; font-size:9pt; color:#adb5bd; }");
    m_previewLabel->setText(tr("拖放图片到此处\n或点击\"选择图片\""));

    m_fileNameLabel = new QLabel();
    m_fileNameLabel->setStyleSheet(labelStyle);
    m_fileSizeLabel = new QLabel();
    m_fileSizeLabel->setStyleSheet(labelStyle);

    leftLayout->addWidget(m_previewLabel, 0, Qt::AlignHCenter);
    leftLayout->addWidget(m_fileNameLabel);
    leftLayout->addWidget(m_fileSizeLabel);
    leftLayout->addStretch();

    // Right: settings
    auto* rightWidget = new QWidget();
    auto* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(8);

    auto* outputLabel = new QLabel(tr("输出目录:"));
    outputLabel->setStyleSheet(labelStyle);

    auto* outputRow = new QHBoxLayout();
    m_outputPathEdit = new QLineEdit();
    m_outputPathEdit->setStyleSheet("QLineEdit { font-size:9pt; padding:3px 6px; border:1px solid #ced4da; border-radius:4px; }");
    m_selectOutputBtn = new QPushButton(tr("浏览"));
    m_selectOutputBtn->setStyleSheet(btnStyle);
    outputRow->addWidget(m_outputPathEdit);
    outputRow->addWidget(m_selectOutputBtn);

    m_webCheck = new QCheckBox(tr("Web Favicon"));
    m_webCheck->setChecked(true);
    m_webCheck->setStyleSheet(checkStyle);
    m_androidCheck = new QCheckBox(tr("Android"));
    m_androidCheck->setChecked(true);
    m_androidCheck->setStyleSheet(checkStyle);
    m_iosCheck = new QCheckBox(tr("iOS"));
    m_iosCheck->setChecked(true);
    m_iosCheck->setStyleSheet(checkStyle);

    m_progressBar = new QProgressBar();
    m_progressBar->setFixedHeight(16);
    m_progressBar->setVisible(false);
    m_progressBar->setStyleSheet("QProgressBar { font-size:8pt; border:1px solid #ced4da; border-radius:4px; text-align:center; }"
                                  "QProgressBar::chunk { background:#228be6; border-radius:3px; }");

    m_statusLabel = new QLabel(tr("就绪"));
    m_statusLabel->setStyleSheet(labelStyle);

    rightLayout->addWidget(outputLabel);
    rightLayout->addLayout(outputRow);
    rightLayout->addSpacing(4);
    rightLayout->addWidget(m_webCheck);
    rightLayout->addWidget(m_androidCheck);
    rightLayout->addWidget(m_iosCheck);
    rightLayout->addSpacing(4);
    rightLayout->addWidget(m_progressBar);
    rightLayout->addWidget(m_statusLabel);
    rightLayout->addStretch();

    middleLayout->addWidget(leftWidget, 0);
    middleLayout->addWidget(rightWidget, 1);

    mainLayout->addWidget(middleWidget);

    // --- Separator ---
    auto* sep = new QFrame();
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color:#dee2e6;");
    mainLayout->addWidget(sep);

    // --- Result grid ---
    m_resultList = new QListWidget();
    m_resultList->setViewMode(QListView::IconMode);
    m_resultList->setIconSize(QSize(64, 64));
    m_resultList->setSpacing(8);
    m_resultList->setResizeMode(QListView::Adjust);
    m_resultList->setMovement(QListView::Static);
    m_resultList->setStyleSheet("QListWidget { border:none; background:transparent; font-size:8pt; }");
    mainLayout->addWidget(m_resultList, 1);

    // --- Connections ---
    connect(m_selectBtn, &QPushButton::clicked, this, &FaviconProduction::onSelectImage);
    connect(m_clearBtn, &QPushButton::clicked, this, &FaviconProduction::onClear);
    connect(m_generateBtn, &QPushButton::clicked, this, &FaviconProduction::onGenerate);
    connect(m_openDirBtn, &QPushButton::clicked, this, &FaviconProduction::onOpenOutputDir);
    connect(m_selectOutputBtn, &QPushButton::clicked, this, &FaviconProduction::onSelectOutputDir);
}

void FaviconProduction::loadImage(const QString& filePath)
{
    QImageReader reader(filePath);
    reader.setAutoTransform(true);
    QImage img = reader.read();
    if (img.isNull()) {
        m_statusLabel->setText(tr("无法加载图片"));
        m_statusLabel->setStyleSheet("QLabel { font-size:9pt; color:#e03131; }");
        return;
    }

    m_sourcePath = filePath;
    m_sourcePixmap = QPixmap::fromImage(img);

    // Retina-adapted preview
    qreal dpr = this->devicePixelRatioF();
    int displaySize = 200;
    int pixelSize = static_cast<int>(displaySize * dpr);
    QPixmap scaled = m_sourcePixmap.scaled(pixelSize, pixelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(dpr);
    m_previewLabel->setPixmap(scaled);

    QFileInfo fi(filePath);
    m_fileNameLabel->setText(fi.fileName());
    m_fileSizeLabel->setText(QString("%1 x %2 px").arg(m_sourcePixmap.width()).arg(m_sourcePixmap.height()));

    m_generateBtn->setEnabled(true);
    m_statusLabel->setText(tr("图片已加载"));
    m_statusLabel->setStyleSheet("QLabel { font-size:9pt; color:#495057; }");
}

void FaviconProduction::onSelectImage()
{
    QString filePath = QFileDialog::getOpenFileName(
        this, tr("选择图片"), QString(),
        tr("图片文件 (*.png *.jpg *.jpeg *.bmp *.svg *.webp)"));
    if (!filePath.isEmpty()) {
        loadImage(filePath);
    }
}

void FaviconProduction::onClear()
{
    m_sourcePath.clear();
    m_sourcePixmap = QPixmap();
    m_previewLabel->clear();
    m_previewLabel->setText(tr("拖放图片到此处\n或点击\"选择图片\""));
    m_fileNameLabel->clear();
    m_fileSizeLabel->clear();
    m_generateBtn->setEnabled(false);
    m_resultList->clear();
    m_statusLabel->setText(tr("已清空"));
    m_statusLabel->setStyleSheet("QLabel { font-size:9pt; color:#495057; }");
}

void FaviconProduction::onGenerate()
{
    if (m_sourcePixmap.isNull()) return;

    m_outputDir = m_outputPathEdit->text();
    if (m_outputDir.isEmpty()) return;

    bool doWeb = m_webCheck->isChecked();
    bool doAndroid = m_androidCheck->isChecked();
    bool doIos = m_iosCheck->isChecked();

    if (!doWeb && !doAndroid && !doIos) {
        m_statusLabel->setText(tr("请至少选择一个平台"));
        m_statusLabel->setStyleSheet("QLabel { font-size:9pt; color:#e03131; }");
        return;
    }

    m_resultList->clear();

    // Collect tasks: (subdir, filename, size)
    struct Task { QString subdir; QString filename; int size; };
    QList<Task> tasks;

    if (doWeb) {
        QList<int> webSizes = {16, 32, 48, 64, 128, 256};
        for (int s : webSizes)
            tasks.append({"web", QString("favicon-%1x%2.png").arg(s).arg(s), s});
    }
    if (doAndroid) {
        struct AndroidEntry { QString folder; int size; };
        QList<AndroidEntry> androidEntries = {
            {"android/mipmap-mdpi", 48},
            {"android/mipmap-hdpi", 72},
            {"android/mipmap-xhdpi", 96},
            {"android/mipmap-xxhdpi", 144},
            {"android/mipmap-xxxhdpi", 192}
        };
        for (const auto& e : androidEntries)
            tasks.append({e.folder, "ic_launcher.png", e.size});
    }
    if (doIos) {
        QList<int> iosSizes = {20, 29, 40, 58, 60, 76, 80, 87, 120, 152, 167, 180, 1024};
        for (int s : iosSizes)
            tasks.append({"ios", QString("AppIcon-%1x%2.png").arg(s).arg(s), s});
    }

    m_progressBar->setMaximum(tasks.size());
    m_progressBar->setValue(0);
    m_progressBar->setVisible(true);

    int count = 0;
    for (const auto& task : tasks) {
        QString dirPath = m_outputDir + "/" + task.subdir;
        QDir().mkpath(dirPath);

        QPixmap icon = generateIcon(m_sourcePixmap, task.size);
        QString filePath = dirPath + "/" + task.filename;
        icon.save(filePath, "PNG");

        addResultItem(filePath, task.size);

        count++;
        m_progressBar->setValue(count);
        QApplication::processEvents();
    }

    m_progressBar->setVisible(false);
    m_statusLabel->setText(QString(tr("完成，共生成 %1 个图标文件")).arg(count));
    m_statusLabel->setStyleSheet("QLabel { font-size:9pt; color:#2b8a3e; }");
}

void FaviconProduction::onOpenOutputDir()
{
    QString dir = m_outputPathEdit->text();
    if (!dir.isEmpty()) {
        QDir().mkpath(dir);
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    }
}

void FaviconProduction::onSelectOutputDir()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("选择输出目录"), m_outputDir);
    if (!dir.isEmpty()) {
        m_outputDir = dir;
        m_outputPathEdit->setText(dir);
    }
}

QPixmap FaviconProduction::generateIcon(const QPixmap& source, int targetSize)
{
    QPixmap result(targetSize, targetSize);
    result.fill(Qt::transparent);
    QPixmap scaled = source.scaled(targetSize, targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPainter painter(&result);
    int x = (targetSize - scaled.width()) / 2;
    int y = (targetSize - scaled.height()) / 2;
    painter.drawPixmap(x, y, scaled);
    return result;
}

void FaviconProduction::addResultItem(const QString& filePath, int size)
{
    QPixmap thumb(filePath);
    if (thumb.isNull()) return;

    QPixmap display = thumb.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QFileInfo fi(filePath);
    QString label = QString("%1\n%2x%2").arg(fi.fileName()).arg(size);

    auto* item = new QListWidgetItem(QIcon(display), label);
    item->setToolTip(filePath);
    m_resultList->addItem(item);
}

void FaviconProduction::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        const auto urls = event->mimeData()->urls();
        for (const auto& url : urls) {
            if (url.isLocalFile()) {
                QString path = url.toLocalFile().toLower();
                if (path.endsWith(".png") || path.endsWith(".jpg") || path.endsWith(".jpeg") ||
                    path.endsWith(".bmp") || path.endsWith(".svg") || path.endsWith(".webp")) {
                    event->acceptProposedAction();
                    return;
                }
            }
        }
    }
}

void FaviconProduction::dropEvent(QDropEvent* event)
{
    const auto urls = event->mimeData()->urls();
    if (!urls.isEmpty() && urls.first().isLocalFile()) {
        loadImage(urls.first().toLocalFile());
    }
}
