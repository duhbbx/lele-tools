#include "colortools.h"

REGISTER_DYNAMICOBJECT(ColorTools);

// ColorWheel 实现
ColorWheel::ColorWheel(QWidget *parent)
    : QWidget(parent)
    , m_currentColor(Qt::red)
    , m_wheelWidth(20)
    , m_mousePressed(false)
{
    setFixedSize(200, 200);
    setMouseTracking(true);
    updatePixmap();
    m_currentPoint = colorToPoint(m_currentColor);
}

void ColorWheel::setCurrentColor(const QColor &color)
{
    if (m_currentColor != color) {
        m_currentColor = color;
        m_currentPoint = colorToPoint(color);
        update();
        emit colorChanged(color);
    }
}

void ColorWheel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 绘制色轮
    painter.drawPixmap(0, 0, m_wheelPixmap);
    
    // 绘制当前颜色指示器
    painter.setPen(QPen(Qt::white, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(m_currentPoint, 6, 6);
    
    painter.setPen(QPen(Qt::black, 1));
    painter.drawEllipse(m_currentPoint, 6, 6);
}

void ColorWheel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_mousePressed = true;
        updateColorFromPoint(event->pos());
    }
}

void ColorWheel::mouseMoveEvent(QMouseEvent *event)
{
    if (m_mousePressed) {
        updateColorFromPoint(event->pos());
    }
}

void ColorWheel::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event)
    updatePixmap();
}

void ColorWheel::updateColorFromPoint(const QPoint &point)
{
    QPoint center = rect().center();
    int radius = qMin(width(), height()) / 2 - m_wheelWidth;
    
    QPoint relativePoint = point - center;
    double distance = sqrt(relativePoint.x() * relativePoint.x() + relativePoint.y() * relativePoint.y());
    
    if (distance <= radius + m_wheelWidth) {
        // 计算色相
        double angle = atan2(relativePoint.y(), relativePoint.x());
        if (angle < 0) angle += 2 * M_PI;
        int hue = (int)(angle * 180 / M_PI);
        
        // 计算饱和度
        double saturation = qMin(distance / radius, 1.0);
        
        QColor newColor = QColor::fromHsv(hue, (int)(saturation * 255), m_currentColor.value(), m_currentColor.alpha());
        
        if (newColor != m_currentColor) {
            m_currentColor = newColor;
            m_currentPoint = point;
            update();
            emit colorChanged(m_currentColor);
        }
    }
}

QPoint ColorWheel::colorToPoint(const QColor &color) const
{
    QPoint center = rect().center();
    int radius = qMin(width(), height()) / 2 - m_wheelWidth;
    
    double hue = color.hsvHue() * M_PI / 180.0;
    double saturation = color.hsvSaturation() / 255.0;
    
    int x = center.x() + (int)(cos(hue) * saturation * radius);
    int y = center.y() + (int)(sin(hue) * saturation * radius);
    
    return QPoint(x, y);
}

void ColorWheel::updatePixmap()
{
    int size = qMin(width(), height());
    m_wheelPixmap = QPixmap(size, size);
    m_wheelPixmap.fill(Qt::transparent);
    
    QPainter painter(&m_wheelPixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QPoint center(size / 2, size / 2);
    int radius = size / 2 - m_wheelWidth;
    
    // 绘制色轮
    for (int i = 0; i < 360; ++i) {
        double angle = i * M_PI / 180.0;
        
        QConicalGradient gradient(center, i);
        for (int j = 0; j <= radius; ++j) {
            double saturation = (double)j / radius;
            QColor color = QColor::fromHsv(i, (int)(saturation * 255), 255);
            gradient.setColorAt(saturation, color);
        }
        
        painter.setBrush(QBrush(gradient));
        painter.setPen(Qt::NoPen);
        
        QPolygonF sector;
        sector << center;
        sector << QPointF(center.x() + cos(angle) * radius, center.y() + sin(angle) * radius);
        sector << QPointF(center.x() + cos(angle + M_PI/180) * radius, center.y() + sin(angle + M_PI/180) * radius);
        
        painter.drawPolygon(sector);
    }
}

// ColorBar 实现
ColorBar::ColorBar(BarType type, QWidget *parent)
    : QWidget(parent)
    , m_type(type)
    , m_baseColor(Qt::red)
    , m_value(255)
    , m_mousePressed(false)
{
    setFixedHeight(20);
    setMinimumWidth(200);
    updatePixmap();
}

void ColorBar::setBaseColor(const QColor &color)
{
    if (m_baseColor != color) {
        m_baseColor = color;
        updatePixmap();
        update();
    }
}

void ColorBar::setValue(int value)
{
    value = qBound(0, value, 255);
    if (m_value != value) {
        m_value = value;
        update();
        emit valueChanged(value);
    }
}

void ColorBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    
    QPainter painter(this);
    painter.drawPixmap(0, 0, m_barPixmap);
    
    // 绘制当前值指示器
    int x = (int)((double)m_value / 255.0 * width());
    painter.setPen(QPen(Qt::white, 2));
    painter.drawLine(x, 0, x, height());
    painter.setPen(QPen(Qt::black, 1));
    painter.drawLine(x, 0, x, height());
}

void ColorBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_mousePressed = true;
        updateValueFromPoint(event->pos());
    }
}

void ColorBar::mouseMoveEvent(QMouseEvent *event)
{
    if (m_mousePressed) {
        updateValueFromPoint(event->pos());
    }
}

void ColorBar::updateValueFromPoint(const QPoint &point)
{
    int newValue = (int)((double)point.x() / width() * 255);
    setValue(newValue);
}

void ColorBar::updatePixmap()
{
    m_barPixmap = QPixmap(width(), height());
    
    QPainter painter(&m_barPixmap);
    
    QLinearGradient gradient(0, 0, width(), 0);
    
    switch (m_type) {
    case HueBar:
        for (int i = 0; i <= 360; i += 10) {
            gradient.setColorAt((double)i / 360.0, QColor::fromHsv(i, 255, 255));
        }
        break;
    case SaturationBar:
        gradient.setColorAt(0, QColor::fromHsv(m_baseColor.hsvHue(), 0, m_baseColor.value()));
        gradient.setColorAt(1, QColor::fromHsv(m_baseColor.hsvHue(), 255, m_baseColor.value()));
        break;
    case ValueBar:
        gradient.setColorAt(0, QColor::fromHsv(m_baseColor.hsvHue(), m_baseColor.hsvSaturation(), 0));
        gradient.setColorAt(1, QColor::fromHsv(m_baseColor.hsvHue(), m_baseColor.hsvSaturation(), 255));
        break;
    case RedBar:
        gradient.setColorAt(0, QColor(0, m_baseColor.green(), m_baseColor.blue()));
        gradient.setColorAt(1, QColor(255, m_baseColor.green(), m_baseColor.blue()));
        break;
    case GreenBar:
        gradient.setColorAt(0, QColor(m_baseColor.red(), 0, m_baseColor.blue()));
        gradient.setColorAt(1, QColor(m_baseColor.red(), 255, m_baseColor.blue()));
        break;
    case BlueBar:
        gradient.setColorAt(0, QColor(m_baseColor.red(), m_baseColor.green(), 0));
        gradient.setColorAt(1, QColor(m_baseColor.red(), m_baseColor.green(), 255));
        break;
    }
    
    painter.fillRect(rect(), QBrush(gradient));
}

// ColorPreview 实现
ColorPreview::ColorPreview(QWidget *parent)
    : QFrame(parent)
    , m_color(Qt::white)
{
    setFixedSize(60, 60);
    setFrameStyle(QFrame::Box | QFrame::Raised);
    setLineWidth(2);
}

void ColorPreview::setColor(const QColor &color)
{
    if (m_color != color) {
        m_color = color;
        update();
    }
}

void ColorPreview::paintEvent(QPaintEvent *event)
{
    QFrame::paintEvent(event);
    
    QPainter painter(this);
    painter.fillRect(contentsRect(), m_color);
    
    // 如果颜色有透明度，绘制透明度棋盘格背景
    if (m_color.alpha() < 255) {
        painter.fillRect(contentsRect(), QBrush(Qt::white));
        
        int squareSize = 8;
        for (int x = 0; x < width(); x += squareSize) {
            for (int y = 0; y < height(); y += squareSize) {
                if ((x / squareSize + y / squareSize) % 2) {
                    painter.fillRect(x, y, squareSize, squareSize, QColor(200, 200, 200));
                }
            }
        }
        
        painter.fillRect(contentsRect(), m_color);
    }
}

void ColorPreview::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit colorClicked();
    }
}

// ColorPalette 实现
ColorPalette::ColorPalette(QWidget *parent)
    : QWidget(parent)
    , m_colorSize(20)
    , m_columns(10)
{
    setMinimumHeight(100);
}

void ColorPalette::addColor(const QColor &color)
{
    if (!m_colors.contains(color)) {
        m_colors.append(color);
        update();
    }
}

void ColorPalette::removeColor(int index)
{
    if (index >= 0 && index < m_colors.size()) {
        m_colors.removeAt(index);
        update();
    }
}

void ColorPalette::clearColors()
{
    m_colors.clear();
    update();
}

void ColorPalette::setColors(const QList<QColor> &colors)
{
    m_colors = colors;
    update();
}

void ColorPalette::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    
    QPainter painter(this);
    
    for (int i = 0; i < m_colors.size(); ++i) {
        QRect colorRect = colorRectAt(i);
        painter.fillRect(colorRect, m_colors[i]);
        painter.setPen(Qt::black);
        painter.drawRect(colorRect);
    }
}

void ColorPalette::mousePressEvent(QMouseEvent *event)
{
    int index = colorIndexAt(event->pos());
    if (index >= 0 && index < m_colors.size()) {
        emit colorSelected(m_colors[index]);
    }
}

void ColorPalette::mouseDoubleClickEvent(QMouseEvent *event)
{
    int index = colorIndexAt(event->pos());
    if (index >= 0 && index < m_colors.size()) {
        emit colorDoubleClicked(m_colors[index]);
    }
}

int ColorPalette::colorIndexAt(const QPoint &point) const
{
    int col = point.x() / m_colorSize;
    int row = point.y() / m_colorSize;
    int index = row * m_columns + col;
    
    return (index < m_colors.size()) ? index : -1;
}

QRect ColorPalette::colorRectAt(int index) const
{
    int col = index % m_columns;
    int row = index / m_columns;
    
    return QRect(col * m_colorSize, row * m_colorSize, m_colorSize, m_colorSize);
}

// ColorTools 主类实现
ColorTools::ColorTools() : QWidget(nullptr), DynamicObjectBase()
{
    m_currentColor = QColor(255, 0, 0);
    m_previousColor = QColor(255, 255, 255);
    m_updatingColor = false;
    m_pickingColor = false;
    
    m_updateTimer = new QTimer(this);
    m_updateTimer->setSingleShot(true);
    m_updateTimer->setInterval(50);
    
    m_pickTimer = new QTimer(this);
    m_pickTimer->setInterval(100);
    
    setupUI();
    loadSettings();
    
    // 连接信号槽
    connect(colorWheel, &ColorWheel::colorChanged, this, &ColorTools::onWheelColorChanged);
    connect(hueBar, &ColorBar::valueChanged, this, &ColorTools::onHueChanged);
    connect(saturationBar, &ColorBar::valueChanged, this, &ColorTools::onSaturationChanged);
    connect(valueBar, &ColorBar::valueChanged, this, &ColorTools::onValueChanged);
    connect(redBar, &ColorBar::valueChanged, this, &ColorTools::onRedChanged);
    connect(greenBar, &ColorBar::valueChanged, this, &ColorTools::onGreenChanged);
    connect(blueBar, &ColorBar::valueChanged, this, &ColorTools::onBlueChanged);
    connect(alphaSlider, &QSlider::valueChanged, this, &ColorTools::onAlphaChanged);
    
    connect(hexEdit, &QLineEdit::textChanged, this, &ColorTools::onHexChanged);
    connect(rgbEdit, &QLineEdit::textChanged, this, &ColorTools::onRgbChanged);
    connect(hslEdit, &QLineEdit::textChanged, this, &ColorTools::onHslChanged);
    connect(hsvEdit, &QLineEdit::textChanged, this, &ColorTools::onHsvChanged);
    connect(cmykEdit, &QLineEdit::textChanged, this, &ColorTools::onCmykChanged);
    
    connect(redSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorTools::onRedChanged);
    connect(greenSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorTools::onGreenChanged);
    connect(blueSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorTools::onBlueChanged);
    connect(alphaSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorTools::onAlphaChanged);
    connect(hueSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorTools::onHueChanged);
    connect(saturationSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorTools::onSaturationChanged);
    connect(valueSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorTools::onValueChanged);
    
    connect(historyPalette, &ColorPalette::colorSelected, this, [this](const QColor &color) {
        setCurrentColor(color);
    });
    connect(favoritesPalette, &ColorPalette::colorSelected, this, [this](const QColor &color) {
        setCurrentColor(color);
    });
    connect(presetPalette, &ColorPalette::colorSelected, this, [this](const QColor &color) {
        setCurrentColor(color);
    });
    
    connect(pickFromScreenBtn, &QPushButton::clicked, this, &ColorTools::onPickColorFromScreen);
    connect(addToHistoryBtn, &QPushButton::clicked, this, &ColorTools::onAddToHistory);
    connect(addToFavoritesBtn, &QPushButton::clicked, this, &ColorTools::onAddToFavorites);
    connect(copyColorBtn, &QPushButton::clicked, this, &ColorTools::onCopyColor);
    connect(pasteColorBtn, &QPushButton::clicked, this, &ColorTools::onPasteColor);
    connect(randomColorBtn, &QPushButton::clicked, this, &ColorTools::onRandomColor);
    connect(complementaryBtn, &QPushButton::clicked, this, &ColorTools::onComplementaryColor);
    connect(analogousBtn, &QPushButton::clicked, this, &ColorTools::onAnalogousColors);
    connect(triadicBtn, &QPushButton::clicked, this, &ColorTools::onTriadicColors);
    connect(monochromaticBtn, &QPushButton::clicked, this, &ColorTools::onMonochromaticColors);
    connect(contrastCheckBtn, &QPushButton::clicked, this, &ColorTools::onContrastCheck);
    connect(exportPaletteBtn, &QPushButton::clicked, this, &ColorTools::onExportPalette);
    connect(importPaletteBtn, &QPushButton::clicked, this, &ColorTools::onImportPalette);
    connect(presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ColorTools::onPresetSelected);
    
    connect(m_updateTimer, &QTimer::timeout, this, &ColorTools::onColorChanged);
    connect(m_pickTimer, &QTimer::timeout, this, [this]() {
        if (m_pickingColor) {
            QPoint globalPos = QCursor::pos();
            QScreen *screen = QApplication::screenAt(globalPos);
            if (screen) {
                QPixmap pixmap = screen->grabWindow(0, globalPos.x(), globalPos.y(), 1, 1);
                QColor color = pixmap.toImage().pixelColor(0, 0);
                setCurrentColor(color);
            }
        }
    });
    
    // 设置初始颜色
    setCurrentColor(m_currentColor);
}

ColorTools::~ColorTools()
{
    saveSettings();
}

void ColorTools::setupUI()
{
    mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    
    // 创建标签页
    tabWidget = new QTabWidget();
    
    setupColorWheel();
    setupColorBars();
    setupColorInputs();
    setupColorPreview();
    setupColorPalettes();
    setupColorHistory();
    setupColorAnalysis();
    setupStatusArea();
    
    mainLayout->addWidget(tabWidget);
    mainLayout->addLayout(buttonLayout);
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
        QLineEdit, QSpinBox {
            font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;
            padding: 6px;
            border: 2px solid #ced4da;
            border-radius: 4px;
            font-size: 11pt;
            background-color: white;
        }
        QLineEdit:focus, QSpinBox:focus {
            border-color: #80bdff;
            outline: 0;
        }
        QLabel {
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
        QTabBar::close-button {
            subcontrol-position: right;
            border: none;
            width: 16px;
            height: 16px;
            margin: 1px;
            background-color: transparent;
            border-radius: 8px;
            image: none;
        }
        QTabBar::tab:hover QTabBar::close-button {
            image: url(:/resources/close.svg);
        }
        QTabBar::close-button:hover {
            background-color: rgba(220, 53, 69, 0.1);
            border: 1px solid #dc3545;
        }
        QTabBar::close-button:pressed {
            background-color: rgba(200, 35, 51, 0.2);
            border: 1px solid #c82333;
        }
    )");
}

void ColorTools::setupColorWheel()
{
    colorPickerWidget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(colorPickerWidget);
    
    QGroupBox *wheelGroup = new QGroupBox("🎨 色轮选择器");
    QVBoxLayout *wheelLayout = new QVBoxLayout(wheelGroup);
    
    colorWheel = new ColorWheel();
    wheelLayout->addWidget(colorWheel, 0, Qt::AlignCenter);
    
    layout->addWidget(wheelGroup);
    
    tabWidget->addTab(colorPickerWidget, "色轮");
}

void ColorTools::setupColorBars()
{
    QWidget *barsWidget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(barsWidget);
    
    QGroupBox *hsvGroup = new QGroupBox("🌈 HSV 调节");
    QGridLayout *hsvLayout = new QGridLayout(hsvGroup);
    
    hsvLayout->addWidget(new QLabel("色相:"), 0, 0);
    hueBar = new ColorBar(ColorBar::HueBar);
    hsvLayout->addWidget(hueBar, 0, 1);
    hueSpin = new QSpinBox();
    hueSpin->setRange(0, 360);
    hueSpin->setSuffix("°");
    hsvLayout->addWidget(hueSpin, 0, 2);
    
    hsvLayout->addWidget(new QLabel("饱和度:"), 1, 0);
    saturationBar = new ColorBar(ColorBar::SaturationBar);
    hsvLayout->addWidget(saturationBar, 1, 1);
    saturationSpin = new QSpinBox();
    saturationSpin->setRange(0, 255);
    hsvLayout->addWidget(saturationSpin, 1, 2);
    
    hsvLayout->addWidget(new QLabel("明度:"), 2, 0);
    valueBar = new ColorBar(ColorBar::ValueBar);
    hsvLayout->addWidget(valueBar, 2, 1);
    valueSpin = new QSpinBox();
    valueSpin->setRange(0, 255);
    hsvLayout->addWidget(valueSpin, 2, 2);
    
    layout->addWidget(hsvGroup);
    
    QGroupBox *rgbGroup = new QGroupBox("🔴 RGB 调节");
    QGridLayout *rgbLayout = new QGridLayout(rgbGroup);
    
    rgbLayout->addWidget(new QLabel("红色:"), 0, 0);
    redBar = new ColorBar(ColorBar::RedBar);
    rgbLayout->addWidget(redBar, 0, 1);
    redSpin = new QSpinBox();
    redSpin->setRange(0, 255);
    rgbLayout->addWidget(redSpin, 0, 2);
    
    rgbLayout->addWidget(new QLabel("绿色:"), 1, 0);
    greenBar = new ColorBar(ColorBar::GreenBar);
    rgbLayout->addWidget(greenBar, 1, 1);
    greenSpin = new QSpinBox();
    greenSpin->setRange(0, 255);
    rgbLayout->addWidget(greenSpin, 1, 2);
    
    rgbLayout->addWidget(new QLabel("蓝色:"), 2, 0);
    blueBar = new ColorBar(ColorBar::BlueBar);
    rgbLayout->addWidget(blueBar, 2, 1);
    blueSpin = new QSpinBox();
    blueSpin->setRange(0, 255);
    rgbLayout->addWidget(blueSpin, 2, 2);
    
    layout->addWidget(rgbGroup);
    
    QGroupBox *alphaGroup = new QGroupBox("🔍 透明度");
    QHBoxLayout *alphaLayout = new QHBoxLayout(alphaGroup);
    
    alphaSlider = new QSlider(Qt::Horizontal);
    alphaSlider->setRange(0, 255);
    alphaSlider->setValue(255);
    alphaLayout->addWidget(alphaSlider);
    
    alphaSpin = new QSpinBox();
    alphaSpin->setRange(0, 255);
    alphaSpin->setValue(255);
    alphaLayout->addWidget(alphaSpin);
    
    layout->addWidget(alphaGroup);
    layout->addStretch();
    
    tabWidget->addTab(barsWidget, "调节器");
}

void ColorTools::setupColorInputs()
{
    QWidget *inputWidget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(inputWidget);
    
    colorInputGroup = new QGroupBox("🔢 颜色值输入");
    QGridLayout *inputLayout = new QGridLayout(colorInputGroup);
    
    inputLayout->addWidget(new QLabel("HEX:"), 0, 0);
    hexEdit = new QLineEdit();
    hexEdit->setPlaceholderText("#FF0000");
    inputLayout->addWidget(hexEdit, 0, 1);
    
    inputLayout->addWidget(new QLabel("RGB:"), 1, 0);
    rgbEdit = new QLineEdit();
    rgbEdit->setPlaceholderText("rgb(255, 0, 0)");
    inputLayout->addWidget(rgbEdit, 1, 1);
    
    inputLayout->addWidget(new QLabel("HSL:"), 2, 0);
    hslEdit = new QLineEdit();
    hslEdit->setPlaceholderText("hsl(0, 100%, 50%)");
    inputLayout->addWidget(hslEdit, 2, 1);
    
    inputLayout->addWidget(new QLabel("HSV:"), 3, 0);
    hsvEdit = new QLineEdit();
    hsvEdit->setPlaceholderText("hsv(0, 100%, 100%)");
    inputLayout->addWidget(hsvEdit, 3, 1);
    
    inputLayout->addWidget(new QLabel("CMYK:"), 4, 0);
    cmykEdit = new QLineEdit();
    cmykEdit->setPlaceholderText("cmyk(0%, 100%, 100%, 0%)");
    inputLayout->addWidget(cmykEdit, 4, 1);
    
    layout->addWidget(colorInputGroup);
    layout->addStretch();
    
    tabWidget->addTab(inputWidget, "输入");
}

void ColorTools::setupColorPreview()
{
    QWidget *previewWidget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(previewWidget);
    
    previewGroup = new QGroupBox("🖼️ 颜色预览");
    QHBoxLayout *previewLayout = new QHBoxLayout(previewGroup);
    
    QVBoxLayout *currentLayout = new QVBoxLayout();
    currentLayout->addWidget(new QLabel("当前颜色"));
    currentColorPreview = new ColorPreview();
    currentLayout->addWidget(currentColorPreview);
    previewLayout->addLayout(currentLayout);
    
    QVBoxLayout *previousLayout = new QVBoxLayout();
    previousLayout->addWidget(new QLabel("之前颜色"));
    previousColorPreview = new ColorPreview();
    previousLayout->addWidget(previousColorPreview);
    previewLayout->addLayout(previousLayout);
    
    colorInfoLabel = new QLabel("颜色信息");
    colorInfoLabel->setWordWrap(true);
    previewLayout->addWidget(colorInfoLabel);
    
    layout->addWidget(previewGroup);
    layout->addStretch();
    
    tabWidget->addTab(previewWidget, "预览");
}

void ColorTools::setupColorPalettes()
{
    QWidget *paletteWidget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(paletteWidget);
    
    paletteGroup = new QGroupBox("🎨 调色板");
    QVBoxLayout *paletteLayout = new QVBoxLayout(paletteGroup);
    
    presetCombo = new QComboBox();
    presetCombo->addItem("选择预设调色板...");
    presetCombo->addItem("基础颜色");
    presetCombo->addItem("网页安全色");
    presetCombo->addItem("Material Design");
    presetCombo->addItem("彩虹色");
    paletteLayout->addWidget(presetCombo);
    
    presetPalette = new ColorPalette();
    paletteLayout->addWidget(presetPalette);
    
    layout->addWidget(paletteGroup);
    
    tabWidget->addTab(paletteWidget, "调色板");
}

void ColorTools::setupColorHistory()
{
    QWidget *historyWidget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(historyWidget);
    
    QGroupBox *historyGroup = new QGroupBox("📜 颜色历史");
    QVBoxLayout *historyLayout = new QVBoxLayout(historyGroup);
    
    historyPalette = new ColorPalette();
    historyLayout->addWidget(historyPalette);
    
    layout->addWidget(historyGroup);
    
    QGroupBox *favoritesGroup = new QGroupBox("⭐ 收藏颜色");
    QVBoxLayout *favoritesLayout = new QVBoxLayout(favoritesGroup);
    
    favoritesPalette = new ColorPalette();
    favoritesLayout->addWidget(favoritesPalette);
    
    layout->addWidget(favoritesGroup);
    
    tabWidget->addTab(historyWidget, "历史");
}

void ColorTools::setupColorAnalysis()
{
    QWidget *analysisWidget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(analysisWidget);
    
    analysisGroup = new QGroupBox("🔍 颜色分析");
    QVBoxLayout *analysisLayout = new QVBoxLayout(analysisGroup);
    
    analysisText = new QTextEdit();
    analysisText->setMaximumHeight(150);
    analysisText->setReadOnly(true);
    analysisLayout->addWidget(analysisText);
    
    QHBoxLayout *contrastLayout = new QHBoxLayout();
    contrastLayout->addWidget(new QLabel("对比度检查:"));
    contrastPreview1 = new ColorPreview();
    contrastPreview2 = new ColorPreview();
    contrastRatioLabel = new QLabel("1:1");
    contrastLayout->addWidget(contrastPreview1);
    contrastLayout->addWidget(new QLabel("vs"));
    contrastLayout->addWidget(contrastPreview2);
    contrastLayout->addWidget(contrastRatioLabel);
    contrastLayout->addStretch();
    
    analysisLayout->addLayout(contrastLayout);
    layout->addWidget(analysisGroup);
    layout->addStretch();
    
    tabWidget->addTab(analysisWidget, "分析");
}

void ColorTools::setupStatusArea()
{
    // 操作按钮 - 分为两行
    buttonLayout = new QHBoxLayout();
    
    // 第一行：常用操作
    QHBoxLayout *row1Layout = new QHBoxLayout();
    
    pickFromScreenBtn = new QPushButton("🎯 取色");
    pickFromScreenBtn->setStyleSheet("QPushButton { background-color: #28a745; color: white; min-width: 60px; } QPushButton:hover { background-color: #218838; }");
    row1Layout->addWidget(pickFromScreenBtn);
    
    copyColorBtn = new QPushButton("📋 复制");
    copyColorBtn->setStyleSheet("QPushButton { background-color: #007bff; color: white; min-width: 60px; } QPushButton:hover { background-color: #0056b3; }");
    row1Layout->addWidget(copyColorBtn);
    
    pasteColorBtn = new QPushButton("📋 粘贴");
    pasteColorBtn->setMinimumWidth(60);
    row1Layout->addWidget(pasteColorBtn);
    
    randomColorBtn = new QPushButton("🎲 随机");
    randomColorBtn->setStyleSheet("QPushButton { background-color: #6f42c1; color: white; min-width: 60px; } QPushButton:hover { background-color: #5a32a3; }");
    row1Layout->addWidget(randomColorBtn);
    
    addToHistoryBtn = new QPushButton("📜 历史");
    addToHistoryBtn->setMinimumWidth(60);
    row1Layout->addWidget(addToHistoryBtn);
    
    addToFavoritesBtn = new QPushButton("⭐ 收藏");
    addToFavoritesBtn->setMinimumWidth(60);
    row1Layout->addWidget(addToFavoritesBtn);
    
    row1Layout->addStretch();
    
    // 第二行：配色工具
    QHBoxLayout *row2Layout = new QHBoxLayout();
    
    complementaryBtn = new QPushButton("🔄 互补");
    complementaryBtn->setMinimumWidth(60);
    row2Layout->addWidget(complementaryBtn);
    
    analogousBtn = new QPushButton("🌈 类似");
    analogousBtn->setMinimumWidth(60);
    row2Layout->addWidget(analogousBtn);
    
    triadicBtn = new QPushButton("🔺 三角");
    triadicBtn->setMinimumWidth(60);
    row2Layout->addWidget(triadicBtn);
    
    monochromaticBtn = new QPushButton("🎨 单色");
    monochromaticBtn->setMinimumWidth(60);
    row2Layout->addWidget(monochromaticBtn);
    
    contrastCheckBtn = new QPushButton("🔍 对比");
    contrastCheckBtn->setMinimumWidth(60);
    row2Layout->addWidget(contrastCheckBtn);
    
    exportPaletteBtn = new QPushButton("💾 导出");
    exportPaletteBtn->setMinimumWidth(60);
    row2Layout->addWidget(exportPaletteBtn);
    
    importPaletteBtn = new QPushButton("📁 导入");
    importPaletteBtn->setMinimumWidth(60);
    row2Layout->addWidget(importPaletteBtn);
    
    row2Layout->addStretch();
    
    // 将两行添加到垂直布局
    QVBoxLayout *buttonsVLayout = new QVBoxLayout();
    buttonsVLayout->setSpacing(5);
    buttonsVLayout->addLayout(row1Layout);
    buttonsVLayout->addLayout(row2Layout);
    
    buttonLayout->addLayout(buttonsVLayout);
    
    // 状态栏
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

// 核心功能实现
void ColorTools::setCurrentColor(const QColor &color, bool updateInputs)
{
    if (m_updatingColor) return;
    
    m_updatingColor = true;
    m_previousColor = m_currentColor;
    m_currentColor = color;
    
    if (updateInputs) {
        updateColorInputs();
        updateColorBars();
    }
    updateColorPreview();
    
    m_updatingColor = false;
}

void ColorTools::updateColorInputs()
{
    hexEdit->setText(colorToHex(m_currentColor));
    rgbEdit->setText(colorToRgb(m_currentColor));
    hslEdit->setText(colorToHsl(m_currentColor));
    hsvEdit->setText(colorToHsv(m_currentColor));
    cmykEdit->setText(colorToCmyk(m_currentColor));
    
    redSpin->setValue(m_currentColor.red());
    greenSpin->setValue(m_currentColor.green());
    blueSpin->setValue(m_currentColor.blue());
    alphaSpin->setValue(m_currentColor.alpha());
    hueSpin->setValue(m_currentColor.hsvHue());
    saturationSpin->setValue(m_currentColor.hsvSaturation());
    valueSpin->setValue(m_currentColor.value());
}

void ColorTools::updateColorBars()
{
    hueBar->setValue(m_currentColor.hsvHue());
    saturationBar->setBaseColor(m_currentColor);
    saturationBar->setValue(m_currentColor.hsvSaturation());
    valueBar->setBaseColor(m_currentColor);
    valueBar->setValue(m_currentColor.value());
    redBar->setBaseColor(m_currentColor);
    redBar->setValue(m_currentColor.red());
    greenBar->setBaseColor(m_currentColor);
    greenBar->setValue(m_currentColor.green());
    blueBar->setBaseColor(m_currentColor);
    blueBar->setValue(m_currentColor.blue());
    alphaSlider->setValue(m_currentColor.alpha());
}

void ColorTools::updateColorPreview()
{
    currentColorPreview->setColor(m_currentColor);
    previousColorPreview->setColor(m_previousColor);
    
    QString info = QString("RGB: %1, %2, %3\nHSV: %4°, %5%, %6%\nHEX: %7")
                   .arg(m_currentColor.red())
                   .arg(m_currentColor.green())
                   .arg(m_currentColor.blue())
                   .arg(m_currentColor.hsvHue())
                   .arg(m_currentColor.hsvSaturation() * 100 / 255)
                   .arg(m_currentColor.value() * 100 / 255)
                   .arg(colorToHex(m_currentColor));
    
    colorInfoLabel->setText(info);
}

// 颜色转换函数
QString ColorTools::colorToHex(const QColor &color) const
{
    return color.name().toUpper();
}

QString ColorTools::colorToRgb(const QColor &color) const
{
    return QString("rgb(%1, %2, %3)").arg(color.red()).arg(color.green()).arg(color.blue());
}

QString ColorTools::colorToHsl(const QColor &color) const
{
    return QString("hsl(%1, %2%, %3%)")
           .arg(color.hslHue())
           .arg(color.hslSaturation() * 100 / 255)
           .arg(color.lightness() * 100 / 255);
}

QString ColorTools::colorToHsv(const QColor &color) const
{
    return QString("hsv(%1, %2%, %3%)")
           .arg(color.hsvHue())
           .arg(color.hsvSaturation() * 100 / 255)
           .arg(color.value() * 100 / 255);
}

QString ColorTools::colorToCmyk(const QColor &color) const
{
    return QString("cmyk(%1%, %2%, %3%, %4%)")
           .arg(color.cyan() * 100 / 255)
           .arg(color.magenta() * 100 / 255)
           .arg(color.yellow() * 100 / 255)
           .arg(color.black() * 100 / 255);
}

// 槽函数实现
void ColorTools::onColorChanged()
{
    addColorToHistory(m_currentColor);
    statusLabel->setText("颜色已更新");
}

void ColorTools::onWheelColorChanged(const QColor &color)
{
    setCurrentColor(color);
}

void ColorTools::onHueChanged(int value)
{
    QColor newColor = QColor::fromHsv(value, m_currentColor.hsvSaturation(), m_currentColor.value(), m_currentColor.alpha());
    setCurrentColor(newColor);
}

void ColorTools::onSaturationChanged(int value)
{
    QColor newColor = QColor::fromHsv(m_currentColor.hsvHue(), value, m_currentColor.value(), m_currentColor.alpha());
    setCurrentColor(newColor);
}

void ColorTools::onValueChanged(int value)
{
    QColor newColor = QColor::fromHsv(m_currentColor.hsvHue(), m_currentColor.hsvSaturation(), value, m_currentColor.alpha());
    setCurrentColor(newColor);
}

void ColorTools::onRedChanged(int value)
{
    QColor newColor(value, m_currentColor.green(), m_currentColor.blue(), m_currentColor.alpha());
    setCurrentColor(newColor);
}

void ColorTools::onGreenChanged(int value)
{
    QColor newColor(m_currentColor.red(), value, m_currentColor.blue(), m_currentColor.alpha());
    setCurrentColor(newColor);
}

void ColorTools::onBlueChanged(int value)
{
    QColor newColor(m_currentColor.red(), m_currentColor.green(), value, m_currentColor.alpha());
    setCurrentColor(newColor);
}

void ColorTools::onAlphaChanged(int value)
{
    QColor newColor = m_currentColor;
    newColor.setAlpha(value);
    setCurrentColor(newColor);
}

void ColorTools::onHexChanged()
{
    if (m_updatingColor) return;
    QColor color = hexToColor(hexEdit->text());
    if (color.isValid()) {
        setCurrentColor(color, false);
    }
}

void ColorTools::onRgbChanged()
{
    if (m_updatingColor) return;
    QColor color = rgbToColor(rgbEdit->text());
    if (color.isValid()) {
        setCurrentColor(color, false);
    }
}

void ColorTools::onHslChanged() {}
void ColorTools::onHsvChanged() {}
void ColorTools::onCmykChanged() {}

void ColorTools::onPickColorFromScreen()
{
    m_pickingColor = !m_pickingColor;
    if (m_pickingColor) {
        pickFromScreenBtn->setText("🛑 停止取色");
        m_pickTimer->start();
        statusLabel->setText("屏幕取色中...");
    } else {
        pickFromScreenBtn->setText("🎯 屏幕取色");
        m_pickTimer->stop();
        statusLabel->setText("停止取色");
    }
}

void ColorTools::onAddToHistory()
{
    addColorToHistory(m_currentColor);
    statusLabel->setText("已添加到历史");
}

void ColorTools::onAddToFavorites()
{
    addColorToFavorites(m_currentColor);
    statusLabel->setText("已添加到收藏");
}

void ColorTools::onClearHistory()
{
    m_colorHistory.clear();
    historyPalette->clearColors();
    statusLabel->setText("历史已清空");
}

void ColorTools::onClearFavorites()
{
    m_favoriteColors.clear();
    favoritesPalette->clearColors();
    statusLabel->setText("收藏已清空");
}

void ColorTools::onCopyColor()
{
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(colorToHex(m_currentColor));
    statusLabel->setText("颜色已复制到剪贴板");
}

void ColorTools::onPasteColor()
{
    QClipboard *clipboard = QApplication::clipboard();
    QString text = clipboard->text().trimmed();
    QColor color = hexToColor(text);
    if (color.isValid()) {
        setCurrentColor(color);
        statusLabel->setText("颜色已从剪贴板粘贴");
    } else {
        statusLabel->setText("剪贴板中没有有效的颜色值");
    }
}

void ColorTools::onRandomColor()
{
    QRandomGenerator *generator = QRandomGenerator::global();
    QColor randomColor = QColor::fromHsv(
        generator->bounded(360),
        generator->bounded(128, 256),
        generator->bounded(128, 256)
    );
    setCurrentColor(randomColor);
    statusLabel->setText("已生成随机颜色");
}

void ColorTools::onComplementaryColor() {}
void ColorTools::onAnalogousColors() {}
void ColorTools::onTriadicColors() {}
void ColorTools::onMonochromaticColors() {}
void ColorTools::onContrastCheck() {}
void ColorTools::onExportPalette() {}
void ColorTools::onImportPalette() {}

void ColorTools::onPresetSelected()
{
    int index = presetCombo->currentIndex();
    QList<QColor> colors;
    
    switch (index) {
    case 1: // 基础颜色
        colors << Qt::red << Qt::green << Qt::blue << Qt::yellow << Qt::cyan << Qt::magenta
               << Qt::black << Qt::white << Qt::gray << Qt::darkRed << Qt::darkGreen << Qt::darkBlue;
        break;
    case 2: // 网页安全色
        for (int r = 0; r <= 255; r += 51) {
            for (int g = 0; g <= 255; g += 51) {
                for (int b = 0; b <= 255; b += 51) {
                    colors << QColor(r, g, b);
                }
            }
        }
        break;
    case 3: // Material Design
        colors << QColor("#F44336") << QColor("#E91E63") << QColor("#9C27B0") << QColor("#673AB7")
               << QColor("#3F51B5") << QColor("#2196F3") << QColor("#03A9F4") << QColor("#00BCD4")
               << QColor("#009688") << QColor("#4CAF50") << QColor("#8BC34A") << QColor("#CDDC39")
               << QColor("#FFEB3B") << QColor("#FFC107") << QColor("#FF9800") << QColor("#FF5722");
        break;
    case 4: // 彩虹色
        for (int i = 0; i < 360; i += 30) {
            colors << QColor::fromHsv(i, 255, 255);
        }
        break;
    }
    
    if (!colors.isEmpty()) {
        presetPalette->setColors(colors);
    }
}

// 辅助函数
QColor ColorTools::hexToColor(const QString &hex) const
{
    QString cleanHex = hex.trimmed();
    if (cleanHex.startsWith("#")) {
        cleanHex = cleanHex.mid(1);
    }
    
    if (cleanHex.length() == 6) {
        bool ok;
        int r = cleanHex.mid(0, 2).toInt(&ok, 16);
        if (!ok) return QColor();
        int g = cleanHex.mid(2, 2).toInt(&ok, 16);
        if (!ok) return QColor();
        int b = cleanHex.mid(4, 2).toInt(&ok, 16);
        if (!ok) return QColor();
        
        return QColor(r, g, b);
    }
    
    return QColor();
}

QColor ColorTools::rgbToColor(const QString &rgb) const
{
    // 简化实现，仅支持 rgb(r, g, b) 格式
    QRegularExpression regex("rgb\\s*\\(\\s*(\\d+)\\s*,\\s*(\\d+)\\s*,\\s*(\\d+)\\s*\\)");
    QRegularExpressionMatch match = regex.match(rgb);
    
    if (match.hasMatch()) {
        int r = match.captured(1).toInt();
        int g = match.captured(2).toInt();
        int b = match.captured(3).toInt();
        return QColor(r, g, b);
    }
    
    return QColor();
}

QColor ColorTools::hslToColor(const QString &hsl) const { return QColor(); }
QColor ColorTools::hsvToColor(const QString &hsv) const { return QColor(); }
QColor ColorTools::cmykToColor(const QString &cmyk) const { return QColor(); }

void ColorTools::addColorToHistory(const QColor &color)
{
    if (!m_colorHistory.contains(color)) {
        m_colorHistory.prepend(color);
        if (m_colorHistory.size() > 50) {
            m_colorHistory.removeLast();
        }
        historyPalette->setColors(m_colorHistory);
    }
}

void ColorTools::addColorToFavorites(const QColor &color)
{
    if (!m_favoriteColors.contains(color)) {
        m_favoriteColors.append(color);
        favoritesPalette->setColors(m_favoriteColors);
    }
}

void ColorTools::loadSettings() {}
void ColorTools::saveSettings() {}

