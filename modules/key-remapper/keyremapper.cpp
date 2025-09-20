#include "keyremapper.h"
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QTextStream>
#include <QStandardPaths>
#include <QMessageBox>
#include <QHeaderView>
#include <QPainter>
#include <QKeySequence>
#include <QDir>
#include <QProcess>

// 注册动态对象
REGISTER_DYNAMICOBJECT(KeyRemapper);

#ifdef Q_OS_WIN
// 静态成员初始化
HHOOK KeyRemapper::s_keyboardHook = nullptr;
KeyRemapper* KeyRemapper::s_instance = nullptr;
std::map<int, int> KeyRemapper::s_keyMappings;
bool KeyRemapper::s_hookEnabled = false;
#endif

// KeyCaptureWidget 实现
KeyCaptureWidget::KeyCaptureWidget(QWidget *parent)
    : QFrame(parent), m_capturing(false), m_showCursor(true)
{
    setFixedHeight(120);
    setFrameStyle(QFrame::StyledPanel);
    setFocusPolicy(Qt::StrongFocus);

    m_blinkTimer = new QTimer(this);
    connect(m_blinkTimer, &QTimer::timeout, [this]() {
        m_showCursor = !m_showCursor;
        update();
    });

    setStyleSheet(R"(
        KeyCaptureWidget {
            border: 2px dashed #3498db;
            border-radius: 8px;
            background-color: #ecf0f1;
        }
        KeyCaptureWidget[capturing="true"] {
            border-color: #e74c3c;
            background-color: #fdf2f2;
        }
    )");
}

void KeyCaptureWidget::startCapture(const QString& prompt)
{
    m_capturing = true;
    m_promptText = prompt;
    m_showCursor = true;
    setFocus();
    setProperty("capturing", true);
    style()->unpolish(this);
    style()->polish(this);
    m_blinkTimer->start(500);
    update();
}

void KeyCaptureWidget::stopCapture()
{
    m_capturing = false;
    m_blinkTimer->stop();
    setProperty("capturing", false);
    style()->unpolish(this);
    style()->polish(this);
    update();
}

void KeyCaptureWidget::keyPressEvent(QKeyEvent *event)
{
    if (!m_capturing) {
        QFrame::keyPressEvent(event);
        return;
    }

    int keyCode = event->nativeVirtualKey();
    QString keyName;

#ifdef Q_OS_WIN
    // Windows 特殊键处理
    switch (keyCode) {
    case VK_ESCAPE: keyName = "Escape"; break;
    case VK_TAB: keyName = "Tab"; break;
    case VK_RETURN: keyName = "Enter"; break;
    case VK_SPACE: keyName = "Space"; break;
    case VK_BACK: keyName = "Backspace"; break;
    case VK_DELETE: keyName = "Delete"; break;
    case VK_INSERT: keyName = "Insert"; break;
    case VK_HOME: keyName = "Home"; break;
    case VK_END: keyName = "End"; break;
    case VK_PRIOR: keyName = "Page Up"; break;
    case VK_NEXT: keyName = "Page Down"; break;
    case VK_UP: keyName = "Up"; break;
    case VK_DOWN: keyName = "Down"; break;
    case VK_LEFT: keyName = "Left"; break;
    case VK_RIGHT: keyName = "Right"; break;
    case VK_F1: case VK_F2: case VK_F3: case VK_F4:
    case VK_F5: case VK_F6: case VK_F7: case VK_F8:
    case VK_F9: case VK_F10: case VK_F11: case VK_F12:
        keyName = QString("F%1").arg(keyCode - VK_F1 + 1);
        break;
    case VK_LSHIFT: keyName = "Left Shift"; break;
    case VK_RSHIFT: keyName = "Right Shift"; break;
    case VK_LCONTROL: keyName = "Left Ctrl"; break;
    case VK_RCONTROL: keyName = "Right Ctrl"; break;
    case VK_LMENU: keyName = "Left Alt"; break;
    case VK_RMENU: keyName = "Right Alt"; break;
    case VK_LWIN: keyName = "Left Win"; break;
    case VK_RWIN: keyName = "Right Win"; break;
    case VK_CAPITAL: keyName = "Caps Lock"; break;
    case VK_NUMLOCK: keyName = "Num Lock"; break;
    case VK_SCROLL: keyName = "Scroll Lock"; break;
    default:
        if (keyCode >= 'A' && keyCode <= 'Z') {
            keyName = QChar(keyCode);
        } else if (keyCode >= '0' && keyCode <= '9') {
            keyName = QChar(keyCode);
        } else {
            keyName = QString("Key_%1").arg(keyCode);
        }
        break;
    }
#else
    keyName = event->text().isEmpty() ? QString("Key_%1").arg(keyCode) : event->text().toUpper();
#endif

    stopCapture();
    emit keyCaptured(keyCode, keyName);
    event->accept();
}

void KeyCaptureWidget::mousePressEvent(QMouseEvent *event)
{
    if (m_capturing) {
        setFocus();
    }
    QFrame::mousePressEvent(event);
}

void KeyCaptureWidget::paintEvent(QPaintEvent *event)
{
    QFrame::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QFont font = painter.font();
    font.setPointSize(12);
    painter.setFont(font);

    if (m_capturing) {
        painter.setPen(QColor("#e74c3c"));
        QString text = m_promptText;
        if (m_showCursor) {
            text += " |";
        }
        painter.drawText(rect(), Qt::AlignCenter, text);
    } else {
        painter.setPen(QColor("#7f8c8d"));
        painter.drawText(rect(), Qt::AlignCenter, "点击此处，然后按下要捕获的按键");
    }
}

// KeyRemapper 实现
KeyRemapper::KeyRemapper(QWidget *parent)
    : QWidget(parent), currentEditingRow(-1), capturingOriginal(true), currentMappingType(MappingType::KeyToKey)
{
#ifdef Q_OS_WIN
    s_instance = this;
#endif

    setupUI();
    setupTable();
    setupConnections();
    setupAppSelectorDialog();
    applyStyles();

    // 加载默认配置
    QString defaultProfilePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/key_mappings.json";
    if (QFile::exists(defaultProfilePath)) {
        loadProfileFromFile(defaultProfilePath);
    }
}

KeyRemapper::~KeyRemapper()
{
    uninstallGlobalHook();

    // 保存当前配置
    QString defaultProfilePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/key_mappings.json";
    QDir().mkpath(QFileInfo(defaultProfilePath).absolutePath());
    saveProfileToFile(defaultProfilePath);

#ifdef Q_OS_WIN
    s_instance = nullptr;
#endif
}

void KeyRemapper::setupUI()
{
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // 创建分割器
    mainSplitter = new QSplitter(Qt::Vertical);

    // 控制区域
    controlGroup = new QGroupBox("控制面板");
    controlLayout = new QHBoxLayout(controlGroup);

    addButton = new QPushButton("➕ 按键映射");
    addButton->setFixedHeight(35);

    addAppButton = new QPushButton("🚀 启动应用");
    addAppButton->setFixedHeight(35);

    addCommandButton = new QPushButton("⚡ 执行命令");
    addCommandButton->setFixedHeight(35);

    removeButton = new QPushButton("➖ 删除映射");
    removeButton->setFixedHeight(35);

    editButton = new QPushButton("✏️ 编辑映射");
    editButton->setFixedHeight(35);

    clearButton = new QPushButton("🗑️ 清空所有");
    clearButton->setFixedHeight(35);

    saveButton = new QPushButton("💾 保存配置");
    saveButton->setFixedHeight(35);

    loadButton = new QPushButton("📂 加载配置");
    loadButton->setFixedHeight(35);

    resetButton = new QPushButton("🔄 重置默认");
    resetButton->setFixedHeight(35);

    enableGlobalHook = new QCheckBox("启用全局按键拦截");
    enableGlobalHook->setChecked(false);

    statusLabel = new QLabel("状态: 已准备");
    statusLabel->setStyleSheet("font-weight: bold; color: #27ae60;");

    controlLayout->addWidget(addButton);
    controlLayout->addWidget(addAppButton);
    controlLayout->addWidget(addCommandButton);
    controlLayout->addWidget(editButton);
    controlLayout->addWidget(removeButton);
    controlLayout->addWidget(clearButton);
    controlLayout->addWidget(saveButton);
    controlLayout->addWidget(loadButton);
    controlLayout->addWidget(resetButton);
    controlLayout->addStretch();
    controlLayout->addWidget(enableGlobalHook);
    controlLayout->addWidget(statusLabel);

    // 按键捕获区域
    captureGroup = new QGroupBox("按键捕获");
    QVBoxLayout* captureLayout = new QVBoxLayout(captureGroup);

    keyCaptureWidget = new KeyCaptureWidget();
    captureLayout->addWidget(keyCaptureWidget);

    // 映射表格
    mappingTable = new QTableWidget();

    // 布局组装
    mainSplitter->addWidget(controlGroup);
    mainSplitter->addWidget(captureGroup);
    mainSplitter->addWidget(mappingTable);

    // 设置分割器比例
    mainSplitter->setStretchFactor(0, 0); // 控制面板固定
    mainSplitter->setStretchFactor(1, 0); // 捕获区域固定
    mainSplitter->setStretchFactor(2, 1); // 表格可拉伸

    mainLayout->addWidget(mainSplitter);
}

void KeyRemapper::setupTable()
{
    // 设置列
    QStringList headers = {"启用", "原始按键", "类型", "目标", "详细信息"};
    mappingTable->setColumnCount(headers.size());
    mappingTable->setHorizontalHeaderLabels(headers);

    // 表格属性
    mappingTable->setAlternatingRowColors(true);
    mappingTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mappingTable->setSelectionMode(QAbstractItemView::SingleSelection);
    mappingTable->setSortingEnabled(false);

    // 表头属性
    QHeaderView* header = mappingTable->horizontalHeader();
    header->setStretchLastSection(false);
    header->setSectionResizeMode(QHeaderView::ResizeToContents);

    // 设置列宽
    mappingTable->setColumnWidth(0, 60);   // 启用
    mappingTable->setColumnWidth(1, 120);  // 原始按键
    mappingTable->setColumnWidth(2, 80);   // 类型
    mappingTable->setColumnWidth(3, 150);  // 目标
    mappingTable->setColumnWidth(4, 200);  // 详细信息

    // 垂直表头
    mappingTable->verticalHeader()->setVisible(false);
    mappingTable->verticalHeader()->setDefaultSectionSize(30);
}

void KeyRemapper::setupConnections()
{
    connect(addButton, &QPushButton::clicked, this, &KeyRemapper::addNewMapping);
    connect(addAppButton, &QPushButton::clicked, this, &KeyRemapper::addAppLauncher);
    connect(addCommandButton, &QPushButton::clicked, this, &KeyRemapper::addCommandLauncher);
    connect(removeButton, &QPushButton::clicked, this, &KeyRemapper::removeMapping);
    connect(editButton, &QPushButton::clicked, this, &KeyRemapper::editMapping);
    connect(clearButton, &QPushButton::clicked, this, &KeyRemapper::clearAllMappings);
    connect(saveButton, &QPushButton::clicked, this, &KeyRemapper::saveProfile);
    connect(loadButton, &QPushButton::clicked, this, &KeyRemapper::loadProfile);
    connect(resetButton, &QPushButton::clicked, this, &KeyRemapper::resetToDefault);

    connect(enableGlobalHook, &QCheckBox::toggled, [this](bool enabled) {
        if (enabled) {
            installGlobalHook();
        } else {
            uninstallGlobalHook();
        }
    });

    connect(mappingTable, &QTableWidget::cellChanged, this, &KeyRemapper::toggleMapping);
    connect(mappingTable, &QTableWidget::cellDoubleClicked, this, &KeyRemapper::onKeyMappingDoubleClick);

    connect(keyCaptureWidget, &KeyCaptureWidget::keyCaptured, this, [this](int keyCode, const QString& keyName) {
        if (capturingOriginal) {
            onOriginalKeyCaptured(keyCode, keyName);
        } else {
            onMappedKeyCaptured(keyCode, keyName);
        }
    });
}

void KeyRemapper::addNewMapping()
{
    currentEditingRow = -1; // 新增模式
    currentMappingType = MappingType::KeyToKey;
    capturingOriginal = true;
    keyCaptureWidget->startCapture("请按下要重新映射的原始按键...");
    showStatusMessage("按键映射: 请按下要重新映射的原始按键", 0);
}

void KeyRemapper::addAppLauncher()
{
    currentEditingRow = -1; // 新增模式
    currentMappingType = MappingType::KeyToApp;
    capturingOriginal = true;
    keyCaptureWidget->startCapture("请按下要启动应用程序的按键...");
    showStatusMessage("应用启动: 请按下要启动应用程序的按键", 0);
}

void KeyRemapper::addCommandLauncher()
{
    currentEditingRow = -1; // 新增模式
    currentMappingType = MappingType::KeyToCommand;
    capturingOriginal = true;
    keyCaptureWidget->startCapture("请按下要执行命令的按键...");
    showStatusMessage("命令执行: 请按下要执行命令的按键", 0);
}

void KeyRemapper::removeMapping()
{
    int currentRow = mappingTable->currentRow();
    if (currentRow >= 0 && currentRow < keyMappings.size()) {
        keyMappings.removeAt(currentRow);
        populateTable();
        updateGlobalMappings();
        showStatusMessage("已删除按键映射");
    }
}

void KeyRemapper::editMapping()
{
    int currentRow = mappingTable->currentRow();
    if (currentRow >= 0 && currentRow < keyMappings.size()) {
        currentEditingRow = currentRow;
        capturingOriginal = true;
        keyCaptureWidget->startCapture("请按下要重新映射的原始按键...");
        showStatusMessage("编辑模式: 请按下要重新映射的原始按键", 0);
    }
}

void KeyRemapper::toggleMapping(int row, int column)
{
    if (column == 0 && row >= 0 && row < keyMappings.size()) { // 启用列
        QTableWidgetItem* item = mappingTable->item(row, column);
        if (item) {
            keyMappings[row].enabled = (item->checkState() == Qt::Checked);
            updateGlobalMappings();
            showStatusMessage(keyMappings[row].enabled ? "已启用按键映射" : "已禁用按键映射");
        }
    }
}

void KeyRemapper::clearAllMappings()
{
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "确认清空", "确定要清空所有按键映射吗？",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        keyMappings.clear();
        populateTable();
        updateGlobalMappings();
        showStatusMessage("已清空所有按键映射");
    }
}

void KeyRemapper::saveProfile()
{
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "保存按键映射配置",
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + "/key_mappings.json",
        "JSON 文件 (*.json)");

    if (!fileName.isEmpty()) {
        saveProfileToFile(fileName);
        showStatusMessage("配置已保存到: " + fileName);
    }
}

void KeyRemapper::loadProfile()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "加载按键映射配置",
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
        "JSON 文件 (*.json)");

    if (!fileName.isEmpty()) {
        loadProfileFromFile(fileName);
        showStatusMessage("配置已从文件加载: " + fileName);
    }
}

void KeyRemapper::resetToDefault()
{
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "重置确认", "确定要重置到默认配置吗？这将清空所有自定义映射。",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        keyMappings.clear();
        populateTable();
        updateGlobalMappings();
        enableGlobalHook->setChecked(false);
        showStatusMessage("已重置到默认配置");
    }
}

void KeyRemapper::onKeyMappingDoubleClick(int row, int column)
{
    if (column == 1 || column == 2) { // 原始按键或映射按键列
        currentEditingRow = row;
        capturingOriginal = (column == 1);

        QString prompt = capturingOriginal ?
            "请按下新的原始按键..." :
            "请按下要映射到的目标按键...";

        keyCaptureWidget->startCapture(prompt);
        showStatusMessage(prompt, 0);
    }
}

void KeyRemapper::onOriginalKeyCaptured(int keyCode, const QString& keyName)
{
    if (currentEditingRow == -1) {
        // 新增模式
        KeyMapping newMapping;
        newMapping.originalKey = keyCode;
        newMapping.originalKeyName = keyName;
        newMapping.enabled = true;
        newMapping.type = currentMappingType;

        keyMappings.append(newMapping);
        currentEditingRow = keyMappings.size() - 1;

        if (currentMappingType == MappingType::KeyToKey) {
            // 按键映射：继续捕获目标按键
            capturingOriginal = false;
            keyCaptureWidget->startCapture("请按下要映射到的目标按键...");
            showStatusMessage("请按下要映射到的目标按键", 0);
        } else {
            // 应用启动或命令执行：显示应用选择对话框
            showAppSelector(currentMappingType);
        }
    } else {
        // 编辑模式，更新原始按键
        keyMappings[currentEditingRow].originalKey = keyCode;
        keyMappings[currentEditingRow].originalKeyName = keyName;
        populateTable();
        updateGlobalMappings();
        showStatusMessage("原始按键已更新");
        currentEditingRow = -1;
    }
}

void KeyRemapper::onMappedKeyCaptured(int keyCode, const QString& keyName)
{
    if (currentEditingRow >= 0 && currentEditingRow < keyMappings.size()) {
        keyMappings[currentEditingRow].mappedKey = keyCode;
        keyMappings[currentEditingRow].mappedKeyName = keyName;
        populateTable();
        updateGlobalMappings();
        showStatusMessage(QString("按键映射已添加: %1 → %2")
                         .arg(keyMappings[currentEditingRow].originalKeyName)
                         .arg(keyMappings[currentEditingRow].mappedKeyName));
        currentEditingRow = -1;
    }
}

void KeyRemapper::populateTable()
{
    mappingTable->setRowCount(keyMappings.size());

    for (int i = 0; i < keyMappings.size(); ++i) {
        const KeyMapping& mapping = keyMappings[i];

        // 启用列
        QTableWidgetItem* enableItem = new QTableWidgetItem();
        enableItem->setCheckState(mapping.enabled ? Qt::Checked : Qt::Unchecked);
        enableItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        mappingTable->setItem(i, 0, enableItem);

        // 其他列
        mappingTable->setItem(i, 1, new QTableWidgetItem(mapping.originalKeyName));
        mappingTable->setItem(i, 2, new QTableWidgetItem(getMappingTypeString(mapping.type)));
        mappingTable->setItem(i, 3, new QTableWidgetItem(getMappingDisplayText(mapping)));

        QString details;
        if (mapping.type == MappingType::KeyToApp) {
            details = QString("路径: %1").arg(mapping.appPath);
            if (!mapping.arguments.isEmpty()) {
                details += QString(" | 参数: %1").arg(mapping.arguments);
            }
        } else if (mapping.type == MappingType::KeyToCommand) {
            details = mapping.command;
        } else {
            details = QString("键码: %1 → %2").arg(mapping.originalKey).arg(mapping.mappedKey);
        }
        mappingTable->setItem(i, 4, new QTableWidgetItem(details));

        // 设置行颜色
        QColor rowColor;
        if (!mapping.enabled) {
            rowColor = QColor(245, 245, 245); // 禁用：灰色
        } else {
            switch (mapping.type) {
            case MappingType::KeyToKey:
                rowColor = QColor(232, 245, 255); // 按键映射：淡蓝色
                break;
            case MappingType::KeyToApp:
                rowColor = QColor(245, 255, 232); // 应用启动：淡绿色
                break;
            case MappingType::KeyToCommand:
                rowColor = QColor(255, 245, 232); // 命令执行：淡橙色
                break;
            }
        }

        for (int j = 0; j < 5; ++j) {
            if (mappingTable->item(i, j)) {
                mappingTable->item(i, j)->setBackground(rowColor);
                if (j > 0) { // 除了第一列，其他列不可编辑
                    mappingTable->item(i, j)->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
                }
            }
        }
    }
}

void KeyRemapper::updateGlobalMappings()
{
#ifdef Q_OS_WIN
    s_keyMappings.clear();
    for (const KeyMapping& mapping : keyMappings) {
        if (mapping.enabled) {
            s_keyMappings[mapping.originalKey] = mapping.mappedKey;
        }
    }
#endif
}

void KeyRemapper::installGlobalHook()
{
#ifdef Q_OS_WIN
    if (s_keyboardHook) {
        return; // 已经安装
    }

    s_keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(nullptr), 0);
    if (s_keyboardHook) {
        s_hookEnabled = true;
        statusLabel->setText("状态: 全局钩子已启用");
        statusLabel->setStyleSheet("font-weight: bold; color: #27ae60;");
        showStatusMessage("全局按键拦截已启用");
    } else {
        statusLabel->setText("状态: 钩子安装失败");
        statusLabel->setStyleSheet("font-weight: bold; color: #e74c3c;");
        showStatusMessage("全局按键拦截启用失败");
        enableGlobalHook->setChecked(false);
    }
#endif
}

void KeyRemapper::uninstallGlobalHook()
{
#ifdef Q_OS_WIN
    if (s_keyboardHook) {
        UnhookWindowsHookEx(s_keyboardHook);
        s_keyboardHook = nullptr;
        s_hookEnabled = false;
        statusLabel->setText("状态: 全局钩子已禁用");
        statusLabel->setStyleSheet("font-weight: bold; color: #7f8c8d;");
        showStatusMessage("全局按键拦截已禁用");
    }
#endif
}

void KeyRemapper::showStatusMessage(const QString& message, int timeout)
{
    if (timeout > 0) {
        QTimer::singleShot(timeout, [this]() {
            statusLabel->setText("状态: 已准备");
            statusLabel->setStyleSheet("font-weight: bold; color: #27ae60;");
        });
    }

    statusLabel->setText("状态: " + message);
    statusLabel->setStyleSheet("font-weight: bold; color: #3498db;");
}

void KeyRemapper::saveProfileToFile(const QString& filePath)
{
    QJsonArray jsonArray;
    for (const KeyMapping& mapping : keyMappings) {
        jsonArray.append(mappingToJson(mapping));
    }

    QJsonObject rootObject;
    rootObject["mappings"] = jsonArray;
    rootObject["version"] = "1.0";
    rootObject["globalHookEnabled"] = enableGlobalHook->isChecked();

    QJsonDocument doc(rootObject);
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
    }
}

void KeyRemapper::loadProfileFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject rootObject = doc.object();

    keyMappings.clear();
    QJsonArray jsonArray = rootObject["mappings"].toArray();
    for (const QJsonValue& value : jsonArray) {
        KeyMapping mapping = mappingFromJson(value.toObject());
        keyMappings.append(mapping);
    }

    bool globalHookEnabled = rootObject["globalHookEnabled"].toBool();
    enableGlobalHook->setChecked(globalHookEnabled);

    populateTable();
    updateGlobalMappings();
}

QJsonObject KeyRemapper::mappingToJson(const KeyMapping& mapping)
{
    QJsonObject obj;
    obj["originalKey"] = mapping.originalKey;
    obj["mappedKey"] = mapping.mappedKey;
    obj["originalKeyName"] = mapping.originalKeyName;
    obj["mappedKeyName"] = mapping.mappedKeyName;
    obj["enabled"] = mapping.enabled;
    obj["type"] = static_cast<int>(mapping.type);
    obj["appPath"] = mapping.appPath;
    obj["appName"] = mapping.appName;
    obj["command"] = mapping.command;
    obj["workingDir"] = mapping.workingDir;
    obj["arguments"] = mapping.arguments;
    return obj;
}

KeyMapping KeyRemapper::mappingFromJson(const QJsonObject& obj)
{
    KeyMapping mapping;
    mapping.originalKey = obj["originalKey"].toInt();
    mapping.mappedKey = obj["mappedKey"].toInt();
    mapping.originalKeyName = obj["originalKeyName"].toString();
    mapping.mappedKeyName = obj["mappedKeyName"].toString();
    mapping.enabled = obj["enabled"].toBool();
    mapping.type = static_cast<MappingType>(obj["type"].toInt(static_cast<int>(MappingType::KeyToKey)));
    mapping.appPath = obj["appPath"].toString();
    mapping.appName = obj["appName"].toString();
    mapping.command = obj["command"].toString();
    mapping.workingDir = obj["workingDir"].toString();
    mapping.arguments = obj["arguments"].toString();
    return mapping;
}

void KeyRemapper::setupAppSelectorDialog()
{
    // 创建应用选择对话框
    appSelectorDialog = new QWidget(this, Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    appSelectorDialog->setWindowTitle("配置应用程序/命令");
    appSelectorDialog->setFixedSize(500, 350);
    appSelectorDialog->hide();

    QVBoxLayout* dialogLayout = new QVBoxLayout(appSelectorDialog);

    // 应用程序路径
    QGroupBox* appGroup = new QGroupBox("应用程序设置");
    QGridLayout* appLayout = new QGridLayout(appGroup);

    appLayout->addWidget(new QLabel("应用程序路径:"), 0, 0);
    appPathEdit = new QLineEdit();
    appPathEdit->setPlaceholderText("选择要启动的应用程序文件...");
    appLayout->addWidget(appPathEdit, 0, 1);

    browseAppButton = new QPushButton("浏览...");
    appLayout->addWidget(browseAppButton, 0, 2);

    appLayout->addWidget(new QLabel("显示名称:"), 1, 0);
    appNameEdit = new QLineEdit();
    appNameEdit->setPlaceholderText("应用程序显示名称（可选）");
    appLayout->addWidget(appNameEdit, 1, 1, 1, 2);

    appLayout->addWidget(new QLabel("工作目录:"), 2, 0);
    workingDirEdit = new QLineEdit();
    workingDirEdit->setPlaceholderText("应用程序工作目录（可选）");
    appLayout->addWidget(workingDirEdit, 2, 1);

    browseWorkDirButton = new QPushButton("浏览...");
    appLayout->addWidget(browseWorkDirButton, 2, 2);

    appLayout->addWidget(new QLabel("命令行参数:"), 3, 0);
    argumentsEdit = new QLineEdit();
    argumentsEdit->setPlaceholderText("命令行参数（可选）");
    appLayout->addWidget(argumentsEdit, 3, 1, 1, 2);

    dialogLayout->addWidget(appGroup);

    // 按钮区域
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    confirmAppButton = new QPushButton("确认");
    cancelAppButton = new QPushButton("取消");

    buttonLayout->addStretch();
    buttonLayout->addWidget(confirmAppButton);
    buttonLayout->addWidget(cancelAppButton);

    dialogLayout->addLayout(buttonLayout);

    // 连接信号
    connect(browseAppButton, &QPushButton::clicked, this, &KeyRemapper::selectApplicationFile);
    connect(browseWorkDirButton, &QPushButton::clicked, this, &KeyRemapper::browseWorkingDirectory);
    connect(confirmAppButton, &QPushButton::clicked, this, [this]() {
        if (currentEditingRow >= 0 && currentEditingRow < keyMappings.size()) {
            KeyMapping& mapping = keyMappings[currentEditingRow];

            if (mapping.type == MappingType::KeyToApp) {
                mapping.appPath = appPathEdit->text();
                mapping.appName = appNameEdit->text().isEmpty() ?
                    QFileInfo(mapping.appPath).baseName() : appNameEdit->text();
                mapping.workingDir = workingDirEdit->text();
                mapping.arguments = argumentsEdit->text();
            } else if (mapping.type == MappingType::KeyToCommand) {
                mapping.command = appPathEdit->text();
                mapping.workingDir = workingDirEdit->text();
                mapping.arguments = argumentsEdit->text();
            }

            populateTable();
            updateGlobalMappings();
            hideAppSelector();

            QString actionName = (mapping.type == MappingType::KeyToApp) ? "应用启动" : "命令执行";
            showStatusMessage(QString("%1映射已添加: %2").arg(actionName).arg(mapping.originalKeyName));
            currentEditingRow = -1;
        }
    });

    connect(cancelAppButton, &QPushButton::clicked, this, [this]() {
        if (currentEditingRow >= 0 && currentEditingRow < keyMappings.size()) {
            keyMappings.removeAt(currentEditingRow);
        }
        hideAppSelector();
        currentEditingRow = -1;
    });
}

void KeyRemapper::showAppSelector(MappingType type)
{
    if (type == MappingType::KeyToApp) {
        appSelectorDialog->setWindowTitle("配置应用程序启动");
        appPathEdit->setPlaceholderText("选择要启动的应用程序文件...");
    } else if (type == MappingType::KeyToCommand) {
        appSelectorDialog->setWindowTitle("配置命令执行");
        appPathEdit->setPlaceholderText("输入要执行的命令...");
    }

    // 清空输入框
    appPathEdit->clear();
    appNameEdit->clear();
    workingDirEdit->clear();
    argumentsEdit->clear();

    appSelectorDialog->show();
    appSelectorDialog->raise();
    appSelectorDialog->activateWindow();
}

void KeyRemapper::hideAppSelector()
{
    appSelectorDialog->hide();
}

void KeyRemapper::selectApplicationFile()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "选择应用程序",
        "",
        "可执行文件 (*.exe);;所有文件 (*.*)");

    if (!fileName.isEmpty()) {
        appPathEdit->setText(fileName);
        if (appNameEdit->text().isEmpty()) {
            appNameEdit->setText(QFileInfo(fileName).baseName());
        }
        if (workingDirEdit->text().isEmpty()) {
            workingDirEdit->setText(QFileInfo(fileName).absolutePath());
        }
    }
}

void KeyRemapper::browseWorkingDirectory()
{
    QString dirName = QFileDialog::getExistingDirectory(
        this,
        "选择工作目录");

    if (!dirName.isEmpty()) {
        workingDirEdit->setText(dirName);
    }
}

void KeyRemapper::executeMapping(const KeyMapping& mapping)
{
    if (mapping.type == MappingType::KeyToApp) {
        // 启动应用程序
        QProcess* process = new QProcess();

        QString program = mapping.appPath;
        QStringList arguments = mapping.arguments.split(' ', Qt::SkipEmptyParts);
        QString workingDir = mapping.workingDir.isEmpty() ?
            QFileInfo(program).absolutePath() : mapping.workingDir;

        process->setWorkingDirectory(workingDir);
        process->start(program, arguments);

        if (!process->waitForStarted(3000)) {
            showStatusMessage(QString("启动应用失败: %1").arg(mapping.appName));
            process->deleteLater();
        } else {
            showStatusMessage(QString("已启动应用: %1").arg(mapping.appName));
            // 进程将在后台运行，不需要等待完成
            connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    process, &QProcess::deleteLater);
        }
    } else if (mapping.type == MappingType::KeyToCommand) {
        // 执行命令
        QProcess* process = new QProcess();

        QString command = mapping.command;
        QString workingDir = mapping.workingDir.isEmpty() ?
            QDir::currentPath() : mapping.workingDir;

        process->setWorkingDirectory(workingDir);

#ifdef Q_OS_WIN
        process->start("cmd", QStringList() << "/C" << command);
#else
        process->start("/bin/sh", QStringList() << "-c" << command);
#endif

        if (!process->waitForStarted(3000)) {
            showStatusMessage(QString("执行命令失败: %1").arg(command));
            process->deleteLater();
        } else {
            showStatusMessage(QString("已执行命令: %1").arg(command));
            connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    process, &QProcess::deleteLater);
        }
    }
}

QString KeyRemapper::getMappingTypeString(MappingType type)
{
    switch (type) {
    case MappingType::KeyToKey:
        return "按键";
    case MappingType::KeyToApp:
        return "应用";
    case MappingType::KeyToCommand:
        return "命令";
    default:
        return "未知";
    }
}

QString KeyRemapper::getMappingDisplayText(const KeyMapping& mapping)
{
    switch (mapping.type) {
    case MappingType::KeyToKey:
        return mapping.mappedKeyName;
    case MappingType::KeyToApp:
        return mapping.appName.isEmpty() ?
            QFileInfo(mapping.appPath).baseName() : mapping.appName;
    case MappingType::KeyToCommand:
        return mapping.command.length() > 30 ?
            mapping.command.left(27) + "..." : mapping.command;
    default:
        return "";
    }
}

void KeyRemapper::applyStyles()
{
    this->setStyleSheet(R"(
        QGroupBox {
            font-weight: bold;
            border: 2px solid #bdc3c7;
            border-radius: 8px;
            margin-top: 10px;
            padding-top: 10px;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            left: 15px;
            padding: 0 8px 0 8px;
            color: #2c3e50;
        }

        QPushButton {
            background-color: #3498db;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 8px 16px;
            font-weight: bold;
            min-width: 100px;
        }

        QPushButton:hover {
            background-color: #2980b9;
        }

        QPushButton:pressed {
            background-color: #21618c;
        }

        QCheckBox {
            font-weight: bold;
            color: #2c3e50;
        }

        QCheckBox::indicator {
            width: 18px;
            height: 18px;
        }

        QCheckBox::indicator:unchecked {
            border: 2px solid #bdc3c7;
            border-radius: 3px;
            background-color: white;
        }

        QCheckBox::indicator:checked {
            border: 2px solid #27ae60;
            border-radius: 3px;
            background-color: #27ae60;
            image: url(data:image/svg+xml;base64,PHN2ZyB3aWR0aD0iMTIiIGhlaWdodD0iMTIiIHZpZXdCb3g9IjAgMCAxMiAxMiIgZmlsbD0ibm9uZSIgeG1sbnM9Imh0dHA6Ly93d3cudzMub3JnLzIwMDAvc3ZnIj4KPHBhdGggZD0iTTEwIDNMNC41IDguNUwyIDYiIHN0cm9rZT0id2hpdGUiIHN0cm9rZS13aWR0aD0iMiIgc3Ryb2tlLWxpbmVjYXA9InJvdW5kIiBzdHJva2UtbGluZWpvaW49InJvdW5kIi8+Cjwvc3ZnPgo=);
        }

        QTableWidget {
            border: 1px solid #bdc3c7;
            border-radius: 4px;
            gridline-color: #ecf0f1;
            background-color: white;
            selection-background-color: #3498db;
        }

        QTableWidget::item {
            padding: 8px;
        }

        QTableWidget::item:selected {
            background-color: #3498db;
            color: white;
        }

        QHeaderView::section {
            background-color: #34495e;
            color: white;
            border: none;
            padding: 8px;
            font-weight: bold;
        }
    )");
}

#ifdef Q_OS_WIN
LRESULT CALLBACK KeyRemapper::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0 && s_hookEnabled && s_instance) {
        KBDLLHOOKSTRUCT* kbStruct = (KBDLLHOOKSTRUCT*)lParam;

        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            int vkCode = kbStruct->vkCode;

            // 在主线程中查找并执行映射
            for (const KeyMapping& mapping : s_instance->keyMappings) {
                if (mapping.enabled && mapping.originalKey == vkCode) {
                    if (mapping.type == MappingType::KeyToKey) {
                        // 发送映射的按键
                        keybd_event(mapping.mappedKey, 0, 0, 0);
                    } else {
                        // 通过信号执行应用启动或命令执行
                        QMetaObject::invokeMethod(s_instance, [mapping]() {
                            s_instance->executeMapping(mapping);
                        }, Qt::QueuedConnection);
                    }

                    // 阻止原始按键
                    return 1;
                }
            }
        }
    }

    return CallNextHookEx(s_keyboardHook, nCode, wParam, lParam);
}
#endif

#include "keyremapper.moc"