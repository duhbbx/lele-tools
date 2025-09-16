#include "qrcodegen.h"

REGISTER_DYNAMICOBJECT(QrCodeGen);

// QRCodePreview 实现
QRCodePreview::QRCodePreview(QWidget *parent)
    : QFrame(parent)
    , m_hasQRCode(false)
{
    setFixedSize(300, 300);
    setFrameStyle(QFrame::Box | QFrame::Raised);
    setLineWidth(2);
    setStyleSheet("QFrame { background-color: white; border: 2px solid #dee2e6; border-radius: 8px; }");
}

void QRCodePreview::setQRCode(const QPixmap &pixmap)
{
    m_qrPixmap = pixmap;
    m_hasQRCode = !pixmap.isNull();
    update();
}

void QRCodePreview::setQRCodeItem(const QRCodeItem &item)
{
    m_qrItem = item;
    setQRCode(item.qrPixmap);
}

void QRCodePreview::clearQRCode()
{
    m_qrPixmap = QPixmap();
    m_qrItem = QRCodeItem();
    m_hasQRCode = false;
    update();
}

void QRCodePreview::paintEvent(QPaintEvent *event)
{
    QFrame::paintEvent(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    if (m_hasQRCode && !m_qrPixmap.isNull()) {
        // 计算居中位置
        QRect contentRect = contentsRect();
        QSize pixmapSize = m_qrPixmap.size();
        
        // 保持宽高比缩放
        QSize scaledSize = pixmapSize.scaled(contentRect.width(), contentRect.height(), Qt::KeepAspectRatio);
        
        int x = contentRect.x() + (contentRect.width() - scaledSize.width()) / 2;
        int y = contentRect.y() + (contentRect.height() - scaledSize.height()) / 2;
        
        QRect targetRect(x, y, scaledSize.width(), scaledSize.height());
        painter.drawPixmap(targetRect, m_qrPixmap);
    } else {
        // 显示占位符
        painter.setPen(QPen(QColor("#6c757d"), 2, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        
        QRect placeholderRect = contentsRect().adjusted(20, 20, -20, -20);
        painter.drawRect(placeholderRect);
        
        painter.setPen(QColor("#6c757d"));
        painter.setFont(QFont("Microsoft YaHei", 12));
        painter.drawText(contentsRect(), Qt::AlignCenter, "二维码预览\n\n点击生成按钮\n创建二维码");
    }
}

void QRCodePreview::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit qrCodeClicked();
    }
    QFrame::mousePressEvent(event);
}

void QRCodePreview::contextMenuEvent(QContextMenuEvent *event)
{
    if (m_hasQRCode) {
        emit saveRequested();
    }
    QFrame::contextMenuEvent(event);
}

// ColorButton实现现在在common/colorbutton.cpp中

// BatchProcessWorker 实现
void BatchProcessWorker::processBatch(const QStringList &inputs, ProcessType type, const QRStyle &style)
{
    for (int i = 0; i < inputs.size(); ++i) {
        emit progressUpdate(i, inputs.size());
        
        QRCodeItem item;
        item.text = inputs[i];
        item.style = style;
        item.createTime = QDateTime::currentDateTime();
        
        try {
            if (type == Generate) {
                item.qrPixmap = generateQRCode(inputs[i], style);
                item.fileName = QString("qr_%1.png").arg(i + 1);
            } else {
                // 批量解析功能需要输入为图片文件路径
                QPixmap pixmap(inputs[i]);
                if (!pixmap.isNull()) {
                    item.text = parseQRCode(pixmap);
                    item.qrPixmap = pixmap;
                    item.fileName = QFileInfo(inputs[i]).baseName();
                }
            }
            
            emit itemProcessed(item);
            
        } catch (const std::exception &e) {
            emit errorOccurred(QString("处理项目 %1 时出错: %2").arg(i + 1).arg(e.what()));
        }
        
        // 模拟处理时间
        QThread::msleep(100);
    }
    
    emit batchCompleted();
}

QPixmap BatchProcessWorker::generateQRCode(const QString &text, const QRStyle &style)
{
    // 简化的二维码生成实现
    // 注意：这是一个基础实现，真实项目建议使用专业的QR码库
    
    // 生成二维码数据矩阵
    QVector<QVector<bool>> qrMatrix = generateQRMatrix(text);
    
    if (qrMatrix.isEmpty()) {
        // 如果生成失败，返回错误提示图像
        return generateErrorPixmap("生成失败", style);
    }
    
    int matrixSize = qrMatrix.size();
    int pixelSize = 8; // 每个模块的像素大小
    int margin = style.borderSize * 4;
    int totalSize = matrixSize * pixelSize + 2 * margin;
    
    QPixmap pixmap(totalSize, totalSize);
    pixmap.fill(style.backgroundColor);
    
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, false); // 二维码不需要抗锯齿
    
    // 绘制二维码矩阵
    painter.setPen(Qt::NoPen);
    painter.setBrush(style.foregroundColor);
    
    for (int y = 0; y < matrixSize; ++y) {
        for (int x = 0; x < matrixSize; ++x) {
            if (qrMatrix[y][x]) { // true表示黑色模块
                QRect moduleRect(
                    margin + x * pixelSize,
                    margin + y * pixelSize,
                    pixelSize,
                    pixelSize
                );
                painter.fillRect(moduleRect, style.foregroundColor);
            }
        }
    }
    
    // 如果有Logo，绘制在中心（需要确保不覆盖关键区域）
    if (style.hasLogo && !style.logoPixmap.isNull()) {
        int logoSize = qMin(style.logoSize, totalSize / 5); // 限制Logo大小
        int logoX = (totalSize - logoSize) / 2;
        int logoY = (totalSize - logoSize) / 2;
        
        // 绘制白色背景
        painter.setBrush(Qt::white);
        painter.setPen(QPen(style.foregroundColor, 2));
        QRect logoBg(logoX - 4, logoY - 4, logoSize + 8, logoSize + 8);
        painter.fillRect(logoBg, Qt::white);
        painter.drawRect(logoBg);
        
        // 绘制Logo
        QRect logoRect(logoX, logoY, logoSize, logoSize);
        painter.drawPixmap(logoRect, style.logoPixmap.scaled(logoSize, logoSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    
    return pixmap;
}

// 简化的二维码矩阵生成
QVector<QVector<bool>> BatchProcessWorker::generateQRMatrix(const QString &text)
{
    // 这是一个非常简化的二维码生成算法
    // 真实的QR码生成需要考虑纠错码、版本选择、掩码模式等
    
    if (text.isEmpty()) {
        return QVector<QVector<bool>>();
    }
    
    // 根据文本长度确定矩阵大小（简化版本）
    int size = 21; // QR码最小尺寸
    if (text.length() > 10) size = 25;
    if (text.length() > 20) size = 29;
    if (text.length() > 40) size = 33;
    if (text.length() > 60) size = 37;
    
    // 初始化矩阵
    QVector<QVector<bool>> matrix(size, QVector<bool>(size, false));
    
    // 1. 绘制定位图案（左上、右上、左下）
    drawFinderPattern(matrix, 0, 0);           // 左上
    drawFinderPattern(matrix, size - 7, 0);   // 右上  
    drawFinderPattern(matrix, 0, size - 7);   // 左下
    
    // 2. 绘制分隔符
    drawSeparators(matrix, size);
    
    // 3. 绘制定时图案
    drawTimingPatterns(matrix, size);
    
    // 4. 填充数据（简化实现）
    fillDataModules(matrix, text, size);
    
    return matrix;
}

void BatchProcessWorker::drawFinderPattern(QVector<QVector<bool>> &matrix, int x, int y)
{
    // 绘制7x7的定位图案
    // 外框
    for (int i = 0; i < 7; ++i) {
        for (int j = 0; j < 7; ++j) {
            if (i == 0 || i == 6 || j == 0 || j == 6) {
                if (x + i < matrix.size() && y + j < matrix[0].size()) {
                    matrix[x + i][y + j] = true;
                }
            }
        }
    }
    
    // 内部3x3实心方块
    for (int i = 2; i < 5; ++i) {
        for (int j = 2; j < 5; ++j) {
            if (x + i < matrix.size() && y + j < matrix[0].size()) {
                matrix[x + i][y + j] = true;
            }
        }
    }
}

void BatchProcessWorker::drawSeparators(QVector<QVector<bool>> &matrix, int size)
{
    // 在定位图案周围绘制白色分隔符（这里不需要特别处理，默认为false即白色）
    // 实际实现中需要确保分隔符区域为白色
}

void BatchProcessWorker::drawTimingPatterns(QVector<QVector<bool>> &matrix, int size)
{
    // 绘制定时图案（第6行和第6列的交替黑白模块）
    for (int i = 8; i < size - 8; ++i) {
        matrix[6][i] = (i % 2 == 0);  // 第6行
        matrix[i][6] = (i % 2 == 0);  // 第6列
    }
}

void BatchProcessWorker::fillDataModules(QVector<QVector<bool>> &matrix, const QString &text, int size)
{
    // 简化的数据填充：根据文本内容生成伪随机模式
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(text.toUtf8());
    QByteArray hashResult = hash.result();
    
    int hashIndex = 0;
    
    // 从右下角开始，按Z字形填充数据模块
    for (int col = size - 1; col > 0; col -= 2) {
        if (col == 6) col--; // 跳过定时列
        
        for (int row = 0; row < size; ++row) {
            int actualRow = (col + 1) % 4 < 2 ? size - 1 - row : row;
            
            // 检查是否可以填充（避免覆盖功能模块）
            if (canFillModule(matrix, actualRow, col, size)) {
                bool bit = (hashResult[hashIndex % hashResult.size()] >> (hashIndex % 8)) & 1;
                matrix[actualRow][col] = bit;
                hashIndex++;
            }
            
            if (canFillModule(matrix, actualRow, col - 1, size)) {
                bool bit = (hashResult[hashIndex % hashResult.size()] >> (hashIndex % 8)) & 1;
                matrix[actualRow][col - 1] = bit;
                hashIndex++;
            }
        }
    }
}

bool BatchProcessWorker::canFillModule(const QVector<QVector<bool>> &matrix, int row, int col, int size)
{
    if (row < 0 || row >= size || col < 0 || col >= size) {
        return false;
    }
    
    // 不能填充定位图案区域
    if ((row < 9 && col < 9) ||                    // 左上定位图案
        (row < 9 && col >= size - 8) ||           // 右上定位图案  
        (row >= size - 8 && col < 9)) {           // 左下定位图案
        return false;
    }
    
    // 不能填充定时图案
    if (row == 6 || col == 6) {
        return false;
    }
    
    return true;
}

QPixmap BatchProcessWorker::generateErrorPixmap(const QString &errorMsg, const QRStyle &style)
{
    QPixmap pixmap(200, 200);
    pixmap.fill(style.backgroundColor);
    
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(style.foregroundColor);
    painter.setFont(QFont("Arial", 12));
    
    painter.drawText(pixmap.rect(), Qt::AlignCenter | Qt::TextWordWrap, errorMsg);
    
    return pixmap;
}

QString BatchProcessWorker::parseQRCode(const QPixmap &pixmap)
{
    // 这里是一个简化的二维码解析实现
    // 在实际项目中，您需要集成QR码解析库（如 QZXing）
    
    Q_UNUSED(pixmap)
    
    // 返回示例解析结果
    return "示例解析结果: 这是一个模拟的二维码内容";
}

// QrCodeGen 主类实现
QrCodeGen::QrCodeGen() : QWidget(nullptr), DynamicObjectBase()
{
    m_currentDataType = QRDataType::Text;
    m_updatingUI = false;
    
    m_statusTimer = new QTimer(this);
    m_statusTimer->setSingleShot(true);
    m_statusTimer->setInterval(3000);
    
    m_updateTimer = new QTimer(this);
    m_updateTimer->setSingleShot(true);
    m_updateTimer->setInterval(500);
    
    // 创建工作线程
    m_workerThread = new QThread(this);
    m_batchWorker = new BatchProcessWorker();
    m_batchWorker->moveToThread(m_workerThread);
    m_workerThread->start();
    
    setupUI();
    initializePresets();
    loadSettings();
    
    // 连接信号槽
    connect(generateBtn, &QPushButton::clicked, this, &QrCodeGen::onGenerateQRCode);
    connect(textEdit, &QTextEdit::textChanged, this, &QrCodeGen::onTextChanged);
    connect(dataTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QrCodeGen::onDataTypeChanged);
    connect(errorCorrectionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QrCodeGen::onErrorCorrectionChanged);
    connect(sizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &QrCodeGen::onSizeChanged);
    connect(sizeSlider, &QSlider::valueChanged, this, &QrCodeGen::onSizeChanged);
    connect(presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QrCodeGen::onPresetSelected);
    
    connect(foregroundColorBtn, &ColorButton::colorChanged, this, &QrCodeGen::onForegroundColorChanged);
    connect(backgroundColorBtn, &ColorButton::colorChanged, this, &QrCodeGen::onBackgroundColorChanged);
    connect(selectLogoBtn, &QPushButton::clicked, this, &QrCodeGen::onSelectLogo);
    connect(removeLogoBtn, &QPushButton::clicked, this, &QrCodeGen::onRemoveLogo);
    connect(logoCheckBox, &QCheckBox::toggled, this, &QrCodeGen::onStyleChanged);
    connect(roundedCornersCheckBox, &QCheckBox::toggled, this, &QrCodeGen::onStyleChanged);
    connect(logoSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &QrCodeGen::onStyleChanged);
    connect(cornerRadiusSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &QrCodeGen::onStyleChanged);
    connect(patternCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QrCodeGen::onStyleChanged);
    
    connect(saveBtn, &QPushButton::clicked, this, &QrCodeGen::onSaveQRCode);
    connect(copyBtn, &QPushButton::clicked, this, &QrCodeGen::onCopyToClipboard);
    connect(addToHistoryBtn, &QPushButton::clicked, this, &QrCodeGen::onAddToHistory);
    
    connect(uploadImageBtn, &QPushButton::clicked, this, &QrCodeGen::onUploadImage);
    connect(parseClipboardBtn, &QPushButton::clicked, this, &QrCodeGen::onParseFromClipboard);
    connect(clearParseBtn, &QPushButton::clicked, this, &QrCodeGen::onClearParseResult);
    connect(copyParseResultBtn, &QPushButton::clicked, [this]() {
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setText(parseResultEdit->toPlainText());
        statusLabel->setText("解析结果已复制到剪贴板");
        m_statusTimer->start();
    });
    
    connect(batchGenerateBtn, &QPushButton::clicked, this, &QrCodeGen::onBatchGenerate);
    connect(batchParseBtn, &QPushButton::clicked, this, &QrCodeGen::onBatchParse);
    connect(addBatchItemBtn, &QPushButton::clicked, this, &QrCodeGen::onAddBatchItem);
    connect(clearBatchListBtn, &QPushButton::clicked, this, &QrCodeGen::onClearBatchList);
    connect(exportBatchBtn, &QPushButton::clicked, this, &QrCodeGen::onExportBatch);
    connect(importBatchBtn, &QPushButton::clicked, this, &QrCodeGen::onImportBatch);
    
    connect(historyList, &QListWidget::itemClicked, this, &QrCodeGen::onHistoryItemClicked);
    connect(clearHistoryBtn, &QPushButton::clicked, this, &QrCodeGen::onClearHistory);
    
    connect(m_batchWorker, &BatchProcessWorker::progressUpdate, this, &QrCodeGen::onBatchProgressUpdate);
    connect(m_batchWorker, &BatchProcessWorker::itemProcessed, this, &QrCodeGen::onBatchItemProcessed);
    connect(m_batchWorker, &BatchProcessWorker::batchCompleted, this, &QrCodeGen::onBatchCompleted);
    connect(m_batchWorker, &BatchProcessWorker::errorOccurred, this, &QrCodeGen::onBatchError);
    
    connect(m_updateTimer, &QTimer::timeout, this, &QrCodeGen::updateQRCodePreview);
    connect(m_statusTimer, &QTimer::timeout, [this]() {
        statusLabel->setText("就绪");
    });
    
    // 启用拖拽
    setAcceptDrops(true);
    
    // 设置初始状态
    onDataTypeChanged();
}

QrCodeGen::~QrCodeGen()
{
    saveSettings();
    
    if (m_workerThread->isRunning()) {
        m_workerThread->quit();
        m_workerThread->wait();
    }
}

void QrCodeGen::setupUI()
{
    mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    
    // 创建标签页
    tabWidget = new CustomTabWidget();
    
    setupGenerateArea();
    setupParseArea();
    setupBatchArea();
    setupHistoryArea();
    
    tabWidget->addTab(generateWidget, "🎨 生成二维码");
    tabWidget->addTab(parseWidget, "🔍 解析二维码");
    tabWidget->addTab(batchWidget, "📊 批量处理");
    tabWidget->addTab(historyWidget, "📜 历史记录");
    
    mainLayout->addWidget(tabWidget);
    
    setupStatusArea();
    mainLayout->addLayout(statusLayout);
    
    // 应用样式
    setStyleSheet(R"(
        QWidget {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
        }
        QPushButton {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            padding: 8px 16px;
            border: 1px solid #ccc;
            border-radius: 4px;
            font-size: 11pt;
            font-weight: normal;
            min-width: 80px;
            background-color: #f8f9fa;
        }
        QPushButton:hover { 
            background-color: #e9ecef; 
            border-color: #adb5bd;
        }
        QPushButton:pressed {
            background-color: #dee2e6;
        }
        QPushButton:disabled {
            background-color: #e9ecef;
            color: #6c757d;
            border-color: #dee2e6;
        }
        QGroupBox {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            font-weight: bold;
            border: 2px solid #dee2e6;
            border-radius: 8px;
            margin-top: 1ex;
            padding-top: 10px;
            font-size: 12pt;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px 0 5px;
        }
        QLineEdit, QTextEdit, QPlainTextEdit {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            padding: 8px;
            border: 2px solid #ced4da;
            border-radius: 4px;
            font-size: 11pt;
            background-color: white;
        }
        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {
            border-color: #80bdff;
            outline: 0;
        }
        QComboBox, QSpinBox {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            padding: 6px;
            border: 2px solid #ced4da;
            border-radius: 4px;
            font-size: 11pt;
            background-color: white;
            min-width: 80px;
        }
        QLabel {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            font-size: 11pt;
        }
        QCheckBox, QRadioButton {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            font-size: 11pt;
        }
        QTabWidget::pane {
            border: 2px solid #dee2e6;
            border-radius: 8px;
        }
        QTabBar::tab {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            padding: 8px 16px;
            margin: 2px;
            border-radius: 4px;
            background-color: #f8f9fa;
        }
        QTabBar::tab:selected {
            background-color: #007bff;
            color: white;
        }
        QTabBar::tab:hover {
            background-color: #e9ecef;
        }

    )");
}

void QrCodeGen::setupGenerateArea()
{
    generateWidget = new QWidget();
    generateSplitter = new QSplitter(Qt::Horizontal);
    
    // 左侧：输入和设置
    QWidget *leftPanel = new QWidget();
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    
    // 输入区域
    inputGroup = new QGroupBox("📝 内容输入");
    inputLayout = new QVBoxLayout(inputGroup);
    
    QHBoxLayout *typeLayout = new QHBoxLayout();
    typeLayout->addWidget(new QLabel("数据类型:"));
    dataTypeCombo = new QComboBox();
    dataTypeCombo->addItems({"文本", "网址", "邮箱", "电话", "短信", "WiFi", "电子名片", "日历事件", "地理位置"});
    typeLayout->addWidget(dataTypeCombo);
    typeLayout->addStretch();
    inputLayout->addLayout(typeLayout);
    
    QHBoxLayout *presetLayout = new QHBoxLayout();
    presetLayout->addWidget(new QLabel("快速模板:"));
    presetCombo = new QComboBox();
    presetCombo->addItem("选择模板...");
    presetLayout->addWidget(presetCombo);
    presetLayout->addStretch();
    inputLayout->addLayout(presetLayout);
    
    textEdit = new QTextEdit();
    textEdit->setPlaceholderText("在此输入要生成二维码的内容...");
    textEdit->setMaximumHeight(120);
    inputLayout->addWidget(textEdit);
    
    // 特殊类型输入（初始隐藏）
    specialInputWidget = new QWidget();
    QGridLayout *specialLayout = new QGridLayout(specialInputWidget);
    
    // URL输入
    specialLayout->addWidget(new QLabel("网址:"), 0, 0);
    urlEdit = new QLineEdit();
    urlEdit->setPlaceholderText("https://example.com");
    specialLayout->addWidget(urlEdit, 0, 1);
    
    // 邮箱输入
    specialLayout->addWidget(new QLabel("邮箱:"), 1, 0);
    emailEdit = new QLineEdit();
    emailEdit->setPlaceholderText("example@email.com");
    specialLayout->addWidget(emailEdit, 1, 1);
    
    // 电话输入
    specialLayout->addWidget(new QLabel("电话:"), 2, 0);
    phoneEdit = new QLineEdit();
    phoneEdit->setPlaceholderText("+86 138 0013 8000");
    specialLayout->addWidget(phoneEdit, 2, 1);
    
    // WiFi输入
    specialLayout->addWidget(new QLabel("WiFi名称:"), 3, 0);
    wifiSSIDEdit = new QLineEdit();
    wifiSSIDEdit->setPlaceholderText("WiFi网络名称");
    specialLayout->addWidget(wifiSSIDEdit, 3, 1);
    
    specialLayout->addWidget(new QLabel("WiFi密码:"), 4, 0);
    wifiPasswordEdit = new QLineEdit();
    wifiPasswordEdit->setPlaceholderText("WiFi密码");
    specialLayout->addWidget(wifiPasswordEdit, 4, 1);
    
    specialLayout->addWidget(new QLabel("加密方式:"), 5, 0);
    wifiSecurityCombo = new QComboBox();
    wifiSecurityCombo->addItems({"WPA", "WEP", "无密码"});
    specialLayout->addWidget(wifiSecurityCombo, 5, 1);
    
    // 电子名片输入
    specialLayout->addWidget(new QLabel("姓名:"), 6, 0);
    vcardNameEdit = new QLineEdit();
    vcardNameEdit->setPlaceholderText("张三");
    specialLayout->addWidget(vcardNameEdit, 6, 1);
    
    specialLayout->addWidget(new QLabel("名片电话:"), 7, 0);
    vcardPhoneEdit = new QLineEdit();
    vcardPhoneEdit->setPlaceholderText("+86 138 0013 8000");
    specialLayout->addWidget(vcardPhoneEdit, 7, 1);
    
    specialLayout->addWidget(new QLabel("名片邮箱:"), 8, 0);
    vcardEmailEdit = new QLineEdit();
    vcardEmailEdit->setPlaceholderText("zhang@example.com");
    specialLayout->addWidget(vcardEmailEdit, 8, 1);
    
    specialInputWidget->setVisible(false);
    inputLayout->addWidget(specialInputWidget);
    
    leftLayout->addWidget(inputGroup);
    
    // 设置区域
    settingsGroup = new QGroupBox("⚙️ 生成设置");
    settingsLayout = new QGridLayout(settingsGroup);
    
    settingsLayout->addWidget(new QLabel("容错级别:"), 0, 0);
    errorCorrectionCombo = new QComboBox();
    errorCorrectionCombo->addItems({"低 (L)", "中 (M)", "高 (Q)", "最高 (H)"});
    errorCorrectionCombo->setCurrentIndex(1);
    settingsLayout->addWidget(errorCorrectionCombo, 0, 1);
    
    settingsLayout->addWidget(new QLabel("图片大小:"), 1, 0);
    QHBoxLayout *sizeLayout = new QHBoxLayout();
    sizeSpinBox = new QSpinBox();
    sizeSpinBox->setRange(100, 1000);
    sizeSpinBox->setValue(300);
    sizeSpinBox->setSuffix(" px");
    sizeLayout->addWidget(sizeSpinBox);
    
    sizeSlider = new QSlider(Qt::Horizontal);
    sizeSlider->setRange(100, 1000);
    sizeSlider->setValue(300);
    sizeLayout->addWidget(sizeSlider);
    settingsLayout->addLayout(sizeLayout, 1, 1);
    
    settingsLayout->addWidget(new QLabel("边框大小:"), 2, 0);
    borderSpinBox = new QSpinBox();
    borderSpinBox->setRange(0, 20);
    borderSpinBox->setValue(4);
    borderSpinBox->setSuffix(" 模块");
    settingsLayout->addWidget(borderSpinBox, 2, 1);
    
    leftLayout->addWidget(settingsGroup);
    
    setupStyleArea();
    leftLayout->addWidget(styleGroup);
    leftLayout->addStretch();
    
    // 右侧：预览
    setupPreviewArea();
    
    generateSplitter->addWidget(leftPanel);
    generateSplitter->addWidget(previewGroup);
    generateSplitter->setStretchFactor(0, 1);
    generateSplitter->setStretchFactor(1, 1);
    generateSplitter->setSizes({400, 400});
    
    QVBoxLayout *generateLayout = new QVBoxLayout(generateWidget);
    generateLayout->addWidget(generateSplitter);
}

void QrCodeGen::setupStyleArea()
{
    styleGroup = new QGroupBox("🎨 样式设置");
    styleLayout = new QGridLayout(styleGroup);
    
    styleLayout->addWidget(new QLabel("前景色:"), 0, 0);
    foregroundColorBtn = new ColorButton(Qt::black);
    styleLayout->addWidget(foregroundColorBtn, 0, 1);
    
    styleLayout->addWidget(new QLabel("背景色:"), 0, 2);
    backgroundColorBtn = new ColorButton(Qt::white);
    styleLayout->addWidget(backgroundColorBtn, 0, 3);
    
    logoCheckBox = new QCheckBox("添加Logo");
    styleLayout->addWidget(logoCheckBox, 1, 0);
    
    selectLogoBtn = new QPushButton("📁 选择Logo");
    selectLogoBtn->setEnabled(false);
    styleLayout->addWidget(selectLogoBtn, 1, 1);
    
    removeLogoBtn = new QPushButton("🗑️ 移除");
    removeLogoBtn->setEnabled(false);
    styleLayout->addWidget(removeLogoBtn, 1, 2);
    
    logoPreviewLabel = new QLabel();
    logoPreviewLabel->setFixedSize(40, 40);
    logoPreviewLabel->setStyleSheet("border: 1px solid #ccc; border-radius: 4px;");
    logoPreviewLabel->setAlignment(Qt::AlignCenter);
    logoPreviewLabel->setText("Logo");
    styleLayout->addWidget(logoPreviewLabel, 1, 3);
    
    styleLayout->addWidget(new QLabel("Logo大小:"), 2, 0);
    logoSizeSpinBox = new QSpinBox();
    logoSizeSpinBox->setRange(20, 100);
    logoSizeSpinBox->setValue(50);
    logoSizeSpinBox->setSuffix(" px");
    logoSizeSpinBox->setEnabled(false);
    styleLayout->addWidget(logoSizeSpinBox, 2, 1);
    
    roundedCornersCheckBox = new QCheckBox("圆角");
    styleLayout->addWidget(roundedCornersCheckBox, 2, 2);
    
    styleLayout->addWidget(new QLabel("圆角半径:"), 3, 0);
    cornerRadiusSpinBox = new QSpinBox();
    cornerRadiusSpinBox->setRange(0, 50);
    cornerRadiusSpinBox->setValue(10);
    cornerRadiusSpinBox->setSuffix(" px");
    cornerRadiusSpinBox->setEnabled(false);
    styleLayout->addWidget(cornerRadiusSpinBox, 3, 1);
    
    styleLayout->addWidget(new QLabel("图案样式:"), 3, 2);
    patternCombo = new QComboBox();
    patternCombo->addItems({"方形", "圆形", "圆角方形"});
    styleLayout->addWidget(patternCombo, 3, 3);
    
    // 连接Logo相关信号
    connect(logoCheckBox, &QCheckBox::toggled, [this](bool checked) {
        selectLogoBtn->setEnabled(checked);
        logoSizeSpinBox->setEnabled(checked && !m_logoPixmap.isNull());
        if (!checked) {
            removeLogoBtn->setEnabled(false);
            logoPreviewLabel->clear();
            logoPreviewLabel->setText("Logo");
            m_logoPixmap = QPixmap();
        }
        onStyleChanged();
    });
    
    connect(roundedCornersCheckBox, &QCheckBox::toggled, [this](bool checked) {
        cornerRadiusSpinBox->setEnabled(checked);
        onStyleChanged();
    });
}

void QrCodeGen::setupPreviewArea()
{
    previewGroup = new QGroupBox("🖼️ 二维码预览");
    previewLayout = new QVBoxLayout(previewGroup);
    
    qrPreview = new QRCodePreview();
    previewLayout->addWidget(qrPreview, 0, Qt::AlignCenter);
    
    // 预览操作按钮
    previewButtonLayout = new QHBoxLayout();
    
    generateBtn = new QPushButton("🎨 生成二维码");
    generateBtn->setStyleSheet("QPushButton { background-color: #28a745; color: white; font-weight: bold; min-width: 120px; } QPushButton:hover { background-color: #218838; }");
    previewButtonLayout->addWidget(generateBtn);
    
    saveBtn = new QPushButton("💾 保存");
    saveBtn->setStyleSheet("QPushButton { background-color: #007bff; color: white; } QPushButton:hover { background-color: #0056b3; }");
    saveBtn->setEnabled(false);
    previewButtonLayout->addWidget(saveBtn);
    
    copyBtn = new QPushButton("📋 复制");
    copyBtn->setEnabled(false);
    previewButtonLayout->addWidget(copyBtn);
    
    addToHistoryBtn = new QPushButton("📜 添加到历史");
    addToHistoryBtn->setEnabled(false);
    previewButtonLayout->addWidget(addToHistoryBtn);
    
    previewLayout->addLayout(previewButtonLayout);
}

void QrCodeGen::setupParseArea()
{
    parseWidget = new QWidget();
    QHBoxLayout *parseMainLayout = new QHBoxLayout(parseWidget);
    
    // 左侧：上传区域
    parseInputGroup = new QGroupBox("📁 图片上传");
    parseInputLayout = new QVBoxLayout(parseInputGroup);
    
    uploadImageBtn = new QPushButton("📂 选择图片");
    uploadImageBtn->setStyleSheet("QPushButton { background-color: #007bff; color: white; min-height: 40px; } QPushButton:hover { background-color: #0056b3; }");
    parseInputLayout->addWidget(uploadImageBtn);
    
    parseClipboardBtn = new QPushButton("📋 从剪贴板解析");
    parseClipboardBtn->setStyleSheet("QPushButton { background-color: #6f42c1; color: white; } QPushButton:hover { background-color: #5a32a3; }");
    parseInputLayout->addWidget(parseClipboardBtn);
    
    uploadedImageLabel = new QLabel();
    uploadedImageLabel->setFixedSize(250, 250);
    uploadedImageLabel->setStyleSheet("border: 2px dashed #ccc; border-radius: 8px; background-color: #f8f9fa;");
    uploadedImageLabel->setAlignment(Qt::AlignCenter);
    uploadedImageLabel->setText("拖拽图片到此处\n或点击选择图片");
    uploadedImageLabel->setWordWrap(true);
    parseInputLayout->addWidget(uploadedImageLabel);
    
    parseInputLayout->addStretch();
    parseMainLayout->addWidget(parseInputGroup);
    
    // 右侧：解析结果
    parseResultGroup = new QGroupBox("📋 解析结果");
    parseResultLayout = new QVBoxLayout(parseResultGroup);
    
    parseResultEdit = new QTextEdit();
    parseResultEdit->setPlaceholderText("解析结果将显示在这里...");
    parseResultEdit->setReadOnly(true);
    parseResultLayout->addWidget(parseResultEdit);
    
    QHBoxLayout *parseButtonLayout = new QHBoxLayout();
    clearParseBtn = new QPushButton("🗑️ 清空");
    clearParseBtn->setStyleSheet("QPushButton { background-color: #dc3545; color: white; } QPushButton:hover { background-color: #c82333; }");
    parseButtonLayout->addWidget(clearParseBtn);
    
    copyParseResultBtn = new QPushButton("📋 复制结果");
    copyParseResultBtn->setStyleSheet("QPushButton { background-color: #28a745; color: white; } QPushButton:hover { background-color: #218838; }");
    copyParseResultBtn->setEnabled(false);
    parseButtonLayout->addWidget(copyParseResultBtn);
    
    parseButtonLayout->addStretch();
    parseResultLayout->addLayout(parseButtonLayout);
    
    parseMainLayout->addWidget(parseResultGroup);
}

void QrCodeGen::setupBatchArea()
{
    batchWidget = new QWidget();
    batchSplitter = new QSplitter(Qt::Horizontal);
    
    // 左侧：批量输入
    batchInputGroup = new QGroupBox("📝 批量输入");
    batchInputLayout = new QVBoxLayout(batchInputGroup);
    
    batchTextEdit = new QTextEdit();
    batchTextEdit->setPlaceholderText("每行一个内容，用于批量生成二维码...\n例如:\nhttps://example1.com\nhttps://example2.com\nhttps://example3.com");
    batchTextEdit->setMaximumHeight(200);
    batchInputLayout->addWidget(batchTextEdit);
    
    batchInputButtonLayout = new QHBoxLayout();
    addBatchItemBtn = new QPushButton("➕ 添加项目");
    batchInputButtonLayout->addWidget(addBatchItemBtn);
    
    clearBatchListBtn = new QPushButton("🗑️ 清空列表");
    clearBatchListBtn->setStyleSheet("QPushButton { background-color: #dc3545; color: white; } QPushButton:hover { background-color: #c82333; }");
    batchInputButtonLayout->addWidget(clearBatchListBtn);
    
    importBatchBtn = new QPushButton("📁 导入文件");
    batchInputButtonLayout->addWidget(importBatchBtn);
    
    batchInputButtonLayout->addStretch();
    batchInputLayout->addLayout(batchInputButtonLayout);
    
    batchSplitter->addWidget(batchInputGroup);
    
    // 右侧：批量结果
    batchResultGroup = new QGroupBox("📊 批量结果");
    batchResultLayout = new QVBoxLayout(batchResultGroup);
    
    batchTable = new QTableWidget();
    batchTable->setColumnCount(4);
    batchTable->setHorizontalHeaderLabels({"内容", "状态", "大小", "创建时间"});
    batchTable->setAlternatingRowColors(true);
    batchTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    batchTable->verticalHeader()->setVisible(false);
    
    // 设置列宽
    QHeaderView* header = batchTable->horizontalHeader();
    header->setStretchLastSection(true);
    batchTable->setColumnWidth(0, 300);
    batchTable->setColumnWidth(1, 80);
    batchTable->setColumnWidth(2, 100);
    
    batchResultLayout->addWidget(batchTable);
    
    batchButtonLayout = new QHBoxLayout();
    
    batchGenerateBtn = new QPushButton("🎨 批量生成");
    batchGenerateBtn->setStyleSheet("QPushButton { background-color: #28a745; color: white; font-weight: bold; } QPushButton:hover { background-color: #218838; }");
    batchButtonLayout->addWidget(batchGenerateBtn);
    
    batchParseBtn = new QPushButton("🔍 批量解析");
    batchParseBtn->setStyleSheet("QPushButton { background-color: #17a2b8; color: white; } QPushButton:hover { background-color: #138496; }");
    batchButtonLayout->addWidget(batchParseBtn);
    
    exportBatchBtn = new QPushButton("💾 导出结果");
    exportBatchBtn->setEnabled(false);
    batchButtonLayout->addWidget(exportBatchBtn);
    
    batchButtonLayout->addStretch();
    
    batchProgressBar = new QProgressBar();
    batchProgressBar->setVisible(false);
    batchProgressBar->setMaximumWidth(200);
    batchButtonLayout->addWidget(batchProgressBar);
    
    batchResultLayout->addLayout(batchButtonLayout);
    
    batchStatusLabel = new QLabel("就绪");
    batchStatusLabel->setStyleSheet("color: #6c757d; font-size: 11pt;");
    batchResultLayout->addWidget(batchStatusLabel);
    
    batchSplitter->addWidget(batchResultGroup);
    
    QVBoxLayout *batchMainLayout = new QVBoxLayout(batchWidget);
    batchMainLayout->addWidget(batchSplitter);
}

void QrCodeGen::setupHistoryArea()
{
    historyWidget = new QWidget();
    QVBoxLayout *historyMainLayout = new QVBoxLayout(historyWidget);
    
    historyGroup = new QGroupBox("📜 历史记录");
    historyLayout = new QVBoxLayout(historyGroup);
    
    historyList = new QListWidget();
    historyList->setAlternatingRowColors(true);
    historyLayout->addWidget(historyList);
    
    historyButtonLayout = new QHBoxLayout();
    
    clearHistoryBtn = new QPushButton("🗑️ 清空历史");
    clearHistoryBtn->setStyleSheet("QPushButton { background-color: #dc3545; color: white; } QPushButton:hover { background-color: #c82333; }");
    historyButtonLayout->addWidget(clearHistoryBtn);
    
    historyButtonLayout->addStretch();
    
    historyCountLabel = new QLabel("历史记录: 0 项");
    historyCountLabel->setStyleSheet("color: #6c757d; font-weight: bold;");
    historyButtonLayout->addWidget(historyCountLabel);
    
    historyLayout->addLayout(historyButtonLayout);
    historyMainLayout->addWidget(historyGroup);
}

void QrCodeGen::setupStatusArea()
{
    statusLayout = new QHBoxLayout();
    
    statusLabel = new QLabel("就绪");
    statusLabel->setStyleSheet("color: #28a745; font-weight: bold;");
    
    progressBar = new QProgressBar();
    progressBar->setVisible(false);
    progressBar->setMaximumHeight(20);
    progressBar->setMaximumWidth(200);
    
    statusLayout->addWidget(statusLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(progressBar);
}

void QrCodeGen::initializePresets()
{
    addPreset("网站链接", QRDataType::URL, "https://example.com", "生成网站链接二维码");
    addPreset("邮箱地址", QRDataType::Email, "example@email.com", "生成邮箱地址二维码");
    addPreset("电话号码", QRDataType::Phone, "+86 138 0013 8000", "生成电话号码二维码");
    addPreset("短信内容", QRDataType::SMS, "SMS:+86138001380000:Hello", "生成短信二维码");
    addPreset("WiFi连接", QRDataType::WiFi, "WIFI:T:WPA;S:MyWiFi;P:password123;;", "生成WiFi连接二维码");
    addPreset("电子名片", QRDataType::VCard, "BEGIN:VCARD\nVERSION:3.0\nFN:张三\nTEL:+86138001380000\nEMAIL:zhang@example.com\nEND:VCARD", "生成电子名片二维码");
    addPreset("地理位置", QRDataType::Location, "geo:39.9042,116.4074", "生成地理位置二维码");
    
    // 填充预设下拉框
    for (const QRPreset &preset : m_presets) {
        presetCombo->addItem(preset.name);
    }
}

void QrCodeGen::addPreset(const QString &name, QRDataType type, const QString &templateText, const QString &description)
{
    QRPreset preset;
    preset.name = name;
    preset.type = type;
    preset.templateText = templateText;
    preset.description = description;
    m_presets.append(preset);
}

// 槽函数实现
void QrCodeGen::onGenerateQRCode()
{
    QString text = textEdit->toPlainText().trimmed();
    if (text.isEmpty()) {
        statusLabel->setText("请输入要生成二维码的内容");
        return;
    }
    
    statusLabel->setText("正在生成二维码...");
    progressBar->setVisible(true);
    progressBar->setRange(0, 0);
    
    // 格式化数据
    QString formattedText = formatQRData(text, m_currentDataType);
    QRStyle style = getCurrentStyle();
    
    // 生成二维码
    QPixmap qrPixmap = generateQRCodeInternal(formattedText, style);
    
    if (!qrPixmap.isNull()) {
        m_currentQRCode = qrPixmap;
        qrPreview->setQRCode(qrPixmap);
        
        // 启用按钮
        saveBtn->setEnabled(true);
        copyBtn->setEnabled(true);
        addToHistoryBtn->setEnabled(true);
        
        statusLabel->setText("二维码生成成功");
    } else {
        statusLabel->setText("二维码生成失败");
    }
    
    progressBar->setVisible(false);
    m_statusTimer->start();
}

void QrCodeGen::onTextChanged()
{
    m_currentText = textEdit->toPlainText();
    // 延迟更新预览
    m_updateTimer->start();
}

void QrCodeGen::onDataTypeChanged()
{
    int index = dataTypeCombo->currentIndex();
    m_currentDataType = static_cast<QRDataType>(index);
    
    // 显示/隐藏特殊输入控件
    bool showSpecial = (m_currentDataType != QRDataType::Text);
    specialInputWidget->setVisible(showSpecial);
    
    // 根据类型显示相应的输入控件
    urlEdit->setVisible(m_currentDataType == QRDataType::URL);
    emailEdit->setVisible(m_currentDataType == QRDataType::Email);
    phoneEdit->setVisible(m_currentDataType == QRDataType::Phone);
    
    bool showWiFi = (m_currentDataType == QRDataType::WiFi);
    wifiSSIDEdit->setVisible(showWiFi);
    wifiPasswordEdit->setVisible(showWiFi);
    wifiSecurityCombo->setVisible(showWiFi);
    
    bool showVCard = (m_currentDataType == QRDataType::VCard);
    vcardNameEdit->setVisible(showVCard);
    vcardPhoneEdit->setVisible(showVCard);
    vcardEmailEdit->setVisible(showVCard);
}

void QrCodeGen::onErrorCorrectionChanged() { onStyleChanged(); }
void QrCodeGen::onSizeChanged() { 
    // 同步滑块和数值框
    if (sender() == sizeSpinBox) {
        sizeSlider->setValue(sizeSpinBox->value());
    } else if (sender() == sizeSlider) {
        sizeSpinBox->setValue(sizeSlider->value());
    }
    onStyleChanged(); 
}

void QrCodeGen::onStyleChanged()
{
    if (!m_updatingUI) {
        m_updateTimer->start();
    }
}

void QrCodeGen::onForegroundColorChanged(const QColor &color)
{
    m_currentStyle.foregroundColor = color;
    onStyleChanged();
}

void QrCodeGen::onBackgroundColorChanged(const QColor &color)
{
    m_currentStyle.backgroundColor = color;
    onStyleChanged();
}

void QrCodeGen::onSelectLogo()
{
    QString filePath = QFileDialog::getOpenFileName(this, "选择Logo图片", "", 
                                                   "Image Files (*.png *.jpg *.jpeg *.bmp *.gif)");
    if (!filePath.isEmpty()) {
        m_logoPixmap = QPixmap(filePath);
        if (!m_logoPixmap.isNull()) {
            // 显示Logo预览
            QPixmap preview = m_logoPixmap.scaled(40, 40, Qt::KeepAspectRatio);
            logoPreviewLabel->setPixmap(preview);
            
            removeLogoBtn->setEnabled(true);
            logoSizeSpinBox->setEnabled(true);
            
            statusLabel->setText("Logo已加载");
            m_statusTimer->start();
            onStyleChanged();
        }
    }
}

void QrCodeGen::onRemoveLogo()
{
    m_logoPixmap = QPixmap();
    logoPreviewLabel->clear();
    logoPreviewLabel->setText("Logo");
    removeLogoBtn->setEnabled(false);
    logoSizeSpinBox->setEnabled(false);
    
    statusLabel->setText("Logo已移除");
    m_statusTimer->start();
    onStyleChanged();
}

void QrCodeGen::onPresetSelected()
{
    int index = presetCombo->currentIndex() - 1; // 减1因为第一项是提示文本
    if (index >= 0 && index < m_presets.size()) {
        const QRPreset &preset = m_presets[index];
        
        dataTypeCombo->setCurrentIndex(static_cast<int>(preset.type));
        textEdit->setPlainText(preset.templateText);
        
        statusLabel->setText("已加载模板: " + preset.description);
        m_statusTimer->start();
        
        presetCombo->setCurrentIndex(0); // 重置为提示文本
    }
}

// 解析相关槽函数
void QrCodeGen::onUploadImage()
{
    QString filePath = QFileDialog::getOpenFileName(this, "选择包含二维码的图片", "", 
                                                   "Image Files (*.png *.jpg *.jpeg *.bmp *.gif)");
    if (!filePath.isEmpty()) {
        QPixmap pixmap(filePath);
        if (!pixmap.isNull()) {
            // 显示上传的图片
            QPixmap preview = pixmap.scaled(250, 250, Qt::KeepAspectRatio);
            uploadedImageLabel->setPixmap(preview);
            
            // 解析二维码
            QString result = parseQRCodeInternal(pixmap);
            parseResultEdit->setPlainText(result);
            copyParseResultBtn->setEnabled(!result.isEmpty());
            
            statusLabel->setText("图片已上传并解析");
        } else {
            statusLabel->setText("无法加载图片文件");
        }
        m_statusTimer->start();
    }
}

void QrCodeGen::onParseFromClipboard()
{
    QClipboard *clipboard = QApplication::clipboard();
    const QMimeData *mimeData = clipboard->mimeData();
    
    if (mimeData->hasImage()) {
        QPixmap pixmap = qvariant_cast<QPixmap>(mimeData->imageData());
        if (!pixmap.isNull()) {
            QPixmap preview = pixmap.scaled(250, 250, Qt::KeepAspectRatio);
            uploadedImageLabel->setPixmap(preview);
            
            QString result = parseQRCodeInternal(pixmap);
            parseResultEdit->setPlainText(result);
            copyParseResultBtn->setEnabled(!result.isEmpty());
            
            statusLabel->setText("已从剪贴板解析图片");
        } else {
            statusLabel->setText("剪贴板中没有有效的图片");
        }
    } else {
        statusLabel->setText("剪贴板中没有图片数据");
    }
    m_statusTimer->start();
}

void QrCodeGen::onClearParseResult()
{
    uploadedImageLabel->clear();
    uploadedImageLabel->setText("拖拽图片到此处\n或点击选择图片");
    parseResultEdit->clear();
    copyParseResultBtn->setEnabled(false);
    statusLabel->setText("解析结果已清空");
    m_statusTimer->start();
}

// 批量处理槽函数
void QrCodeGen::onBatchGenerate()
{
    QStringList lines = batchTextEdit->toPlainText().split('\n', Qt::SkipEmptyParts);
    if (lines.isEmpty()) {
        statusLabel->setText("请输入批量生成的内容");
        return;
    }
    
    batchTable->setRowCount(0);
    m_batchItems.clear();
    
    batchProgressBar->setVisible(true);
    batchProgressBar->setRange(0, lines.size());
    batchStatusLabel->setText("正在批量生成...");
    
    QRStyle style = getCurrentStyle();
    
    // 使用工作线程进行批量处理
    QMetaObject::invokeMethod(m_batchWorker, "processBatch", Qt::QueuedConnection,
                             Q_ARG(QStringList, lines),
                             Q_ARG(BatchProcessWorker::ProcessType, BatchProcessWorker::Generate),
                             Q_ARG(QRStyle, style));
}

void QrCodeGen::onBatchParse()
{
    // 批量解析需要图片文件路径列表
    QStringList filePaths = QFileDialog::getOpenFileNames(this, "选择要解析的图片文件", "", 
                                                         "Image Files (*.png *.jpg *.jpeg *.bmp *.gif)");
    if (filePaths.isEmpty()) {
        return;
    }
    
    batchTable->setRowCount(0);
    m_batchItems.clear();
    
    batchProgressBar->setVisible(true);
    batchProgressBar->setRange(0, filePaths.size());
    batchStatusLabel->setText("正在批量解析...");
    
    QRStyle style = getCurrentStyle();
    
    QMetaObject::invokeMethod(m_batchWorker, "processBatch", Qt::QueuedConnection,
                             Q_ARG(QStringList, filePaths),
                             Q_ARG(BatchProcessWorker::ProcessType, BatchProcessWorker::Parse),
                             Q_ARG(QRStyle, style));
}

void QrCodeGen::onAddBatchItem()
{
    QString text = textEdit->toPlainText().trimmed();
    if (!text.isEmpty()) {
        QString currentBatch = batchTextEdit->toPlainText();
        if (!currentBatch.isEmpty()) {
            currentBatch += "\n";
        }
        currentBatch += text;
        batchTextEdit->setPlainText(currentBatch);
        
        statusLabel->setText("已添加到批量列表");
        m_statusTimer->start();
    }
}


void QrCodeGen::onClearBatchList()
{
    batchTextEdit->clear();
    batchTable->setRowCount(0);
    m_batchItems.clear();
    batchStatusLabel->setText("就绪");
    exportBatchBtn->setEnabled(false);
}

void QrCodeGen::onBatchProgressUpdate(int current, int total)
{
    batchProgressBar->setValue(current);
    batchStatusLabel->setText(QString("处理进度: %1/%2").arg(current).arg(total));
}

void QrCodeGen::onBatchItemProcessed(const QRCodeItem &item)
{
    m_batchItems.append(item);
    
    int row = batchTable->rowCount();
    batchTable->insertRow(row);
    
    batchTable->setItem(row, 0, new QTableWidgetItem(item.text.left(50) + (item.text.length() > 50 ? "..." : "")));
    batchTable->setItem(row, 1, new QTableWidgetItem(item.qrPixmap.isNull() ? "失败" : "成功"));
    batchTable->setItem(row, 2, new QTableWidgetItem(QString("%1x%2").arg(item.qrPixmap.width()).arg(item.qrPixmap.height())));
    batchTable->setItem(row, 3, new QTableWidgetItem(item.createTime.toString("hh:mm:ss")));
    
    // 设置状态颜色
    QTableWidgetItem *statusItem = batchTable->item(row, 1);
    if (item.qrPixmap.isNull()) {
        statusItem->setForeground(QBrush(QColor("#dc3545")));
    } else {
        statusItem->setForeground(QBrush(QColor("#28a745")));
    }
}

void QrCodeGen::onBatchCompleted()
{
    batchProgressBar->setVisible(false);
    batchStatusLabel->setText(QString("批量处理完成，共处理 %1 项").arg(m_batchItems.size()));
    exportBatchBtn->setEnabled(!m_batchItems.isEmpty());
    
    statusLabel->setText("批量处理完成");
    m_statusTimer->start();
}

void QrCodeGen::onBatchError(const QString &error)
{
    batchProgressBar->setVisible(false);
    batchStatusLabel->setText("处理出错: " + error);
    statusLabel->setText("批量处理出错");
    m_statusTimer->start();
}

// 保存和复制
void QrCodeGen::onSaveQRCode()
{
    if (m_currentQRCode.isNull()) {
        return;
    }
    
    QString fileName = QString("qrcode_%1.png").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    QString filePath = QFileDialog::getSaveFileName(this, "保存二维码", fileName, 
                                                   "PNG Files (*.png);;JPEG Files (*.jpg);;All Files (*)");
    
    if (!filePath.isEmpty()) {
        if (m_currentQRCode.save(filePath)) {
            statusLabel->setText("二维码保存成功");
        } else {
            statusLabel->setText("二维码保存失败");
        }
        m_statusTimer->start();
    }
}

void QrCodeGen::onSaveAsImage() { onSaveQRCode(); }

void QrCodeGen::onCopyToClipboard()
{
    if (!m_currentQRCode.isNull()) {
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setPixmap(m_currentQRCode);
        statusLabel->setText("二维码已复制到剪贴板");
        m_statusTimer->start();
    }
}

void QrCodeGen::onExportBatch()
{
    if (m_batchItems.isEmpty()) {
        return;
    }
    
    QString dirPath = QFileDialog::getExistingDirectory(this, "选择导出目录");
    if (!dirPath.isEmpty()) {
        int successCount = 0;
        for (int i = 0; i < m_batchItems.size(); ++i) {
            const QRCodeItem &item = m_batchItems[i];
            if (!item.qrPixmap.isNull()) {
                QString fileName = QString("batch_qr_%1.png").arg(i + 1, 3, 10, QChar('0'));
                QString filePath = QDir(dirPath).filePath(fileName);
                if (item.qrPixmap.save(filePath)) {
                    successCount++;
                }
            }
        }
        
        statusLabel->setText(QString("批量导出完成，成功导出 %1 个文件").arg(successCount));
        m_statusTimer->start();
    }
}

void QrCodeGen::onImportBatch()
{
    QString filePath = QFileDialog::getOpenFileName(this, "导入批量列表", "", "Text Files (*.txt)");
    if (!filePath.isEmpty()) {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            in.setEncoding(QStringConverter::Utf8);
            QString content = in.readAll();
            batchTextEdit->setPlainText(content);
            
            statusLabel->setText("批量列表导入成功");
            m_statusTimer->start();
        }
    }
}

// 历史记录
void QrCodeGen::onAddToHistory()
{
    if (!m_currentQRCode.isNull()) {
        QRCodeItem item;
        item.text = m_currentText;
        item.type = m_currentDataType;
        item.qrPixmap = m_currentQRCode;
        item.style = m_currentStyle;
        item.createTime = QDateTime::currentDateTime();
        
        addToHistory(item);
        statusLabel->setText("已添加到历史记录");
        m_statusTimer->start();
    }
}

void QrCodeGen::onClearHistory()
{
    m_historyItems.clear();
    historyList->clear();
    historyCountLabel->setText("历史记录: 0 项");
    statusLabel->setText("历史记录已清空");
    m_statusTimer->start();
}

void QrCodeGen::onHistoryItemClicked(QListWidgetItem *item)
{
    int index = historyList->row(item);
    if (index >= 0 && index < m_historyItems.size()) {
        const QRCodeItem &historyItem = m_historyItems[index];
        
        // 恢复设置
        dataTypeCombo->setCurrentIndex(static_cast<int>(historyItem.type));
        textEdit->setPlainText(historyItem.text);
        qrPreview->setQRCodeItem(historyItem);
        m_currentQRCode = historyItem.qrPixmap;
        
        // 启用按钮
        saveBtn->setEnabled(true);
        copyBtn->setEnabled(true);
        addToHistoryBtn->setEnabled(true);
        
        statusLabel->setText("已加载历史记录");
        m_statusTimer->start();
    }
}

// 工具函数
QRStyle QrCodeGen::getCurrentStyle() const
{
    QRStyle style;
    style.foregroundColor = foregroundColorBtn->color();
    style.backgroundColor = backgroundColorBtn->color();
    style.borderSize = borderSpinBox->value();
    style.hasLogo = logoCheckBox->isChecked() && !m_logoPixmap.isNull();
    style.logoPixmap = m_logoPixmap;
    style.logoSize = logoSizeSpinBox->value();
    style.roundedCorners = roundedCornersCheckBox->isChecked();
    style.cornerRadius = cornerRadiusSpinBox->value();
    style.pattern = patternCombo->currentText();
    return style;
}

QPixmap QrCodeGen::generateQRCodeInternal(const QString &text, const QRStyle &style)
{
    // 调用批量处理工作器的生成方法
    return m_batchWorker->generateQRCode(text, style);
}

QString QrCodeGen::parseQRCodeInternal(const QPixmap &pixmap)
{
    // 调用批量处理工作器的解析方法
    return m_batchWorker->parseQRCode(pixmap);
}

void QrCodeGen::updateQRCodePreview()
{
    // 实时预览更新（可选实现）
}

void QrCodeGen::updateStylePreview()
{
    // 样式预览更新（可选实现）
}

QString QrCodeGen::formatQRData(const QString &text, QRDataType type) const
{
    switch (type) {
    case QRDataType::URL:
        if (!text.startsWith("http://") && !text.startsWith("https://")) {
            return "https://" + text;
        }
        return text;
        
    case QRDataType::Email:
        return "mailto:" + text;
        
    case QRDataType::Phone:
        return "tel:" + text;
        
    case QRDataType::SMS:
        return "sms:" + text;
        
    case QRDataType::WiFi:
        return getWiFiQRString(wifiSSIDEdit->text(), wifiPasswordEdit->text(), wifiSecurityCombo->currentText());
        
    case QRDataType::VCard:
        return getVCardQRString(vcardNameEdit->text(), vcardPhoneEdit->text(), vcardEmailEdit->text());
        
    case QRDataType::Location:
        return "geo:" + text;
        
    default:
        return text;
    }
}

QString QrCodeGen::getWiFiQRString(const QString &ssid, const QString &password, const QString &security) const
{
    return QString("WIFI:T:%1;S:%2;P:%3;;").arg(security).arg(ssid).arg(password);
}

QString QrCodeGen::getVCardQRString(const QString &name, const QString &phone, const QString &email) const
{
    return QString("BEGIN:VCARD\nVERSION:3.0\nFN:%1\nTEL:%2\nEMAIL:%3\nEND:VCARD")
           .arg(name).arg(phone).arg(email);
}

QString QrCodeGen::getEventQRString(const QString &title, const QString &location, const QDateTime &startTime) const
{
    return QString("BEGIN:VEVENT\nSUMMARY:%1\nLOCATION:%2\nDTSTART:%3\nEND:VEVENT")
           .arg(title).arg(location).arg(startTime.toString("yyyyMMddThhmmss"));
}

void QrCodeGen::addToHistory(const QRCodeItem &item)
{
    m_historyItems.prepend(item);
    
    // 限制历史记录数量
    if (m_historyItems.size() > 50) {
        m_historyItems.removeLast();
    }
    
    // 更新历史列表显示
    historyList->clear();
    for (const QRCodeItem &historyItem : m_historyItems) {
        QString displayText = QString("[%1] %2")
                             .arg(historyItem.createTime.toString("MM-dd hh:mm"))
                             .arg(historyItem.text.left(50) + (historyItem.text.length() > 50 ? "..." : ""));
        
        QListWidgetItem *listItem = new QListWidgetItem(displayText);
        listItem->setToolTip(historyItem.text);
        historyList->addItem(listItem);
    }
    
    historyCountLabel->setText(QString("历史记录: %1 项").arg(m_historyItems.size()));
}

// 拖拽事件处理
void QrCodeGen::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty()) {
            QString filePath = urls.first().toLocalFile();
            QStringList imageFormats = {"png", "jpg", "jpeg", "bmp", "gif"};
            QString extension = QFileInfo(filePath).suffix().toLower();
            if (imageFormats.contains(extension)) {
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

void QrCodeGen::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty()) {
        QString filePath = urls.first().toLocalFile();
        QStringList imageFormats = {"png", "jpg", "jpeg", "bmp", "gif"};
        QString extension = QFileInfo(filePath).suffix().toLower();
        if (imageFormats.contains(extension)) {
            // 切换到解析标签页
            tabWidget->setCurrentIndex(1);
            
            // 加载图片并解析
            QPixmap pixmap(filePath);
            if (!pixmap.isNull()) {
                QPixmap preview = pixmap.scaled(250, 250, Qt::KeepAspectRatio);
                uploadedImageLabel->setPixmap(preview);
                
                QString result = parseQRCodeInternal(pixmap);
                parseResultEdit->setPlainText(result);
                copyParseResultBtn->setEnabled(!result.isEmpty());
                
                statusLabel->setText("图片已拖拽并解析");
                m_statusTimer->start();
            }
            
            event->acceptProposedAction();
            return;
        }
    }
    event->ignore();
}

void QrCodeGen::loadSettings() {}
void QrCodeGen::saveSettings() {}

#include "qrcodegen.moc"
