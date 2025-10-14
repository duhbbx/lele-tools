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
#include <QMenu>

// 注册动态对象
REGISTER_DYNAMICOBJECT(KeyRemapper);

#ifdef Q_OS_WIN
// 静态成员初始化
HHOOK KeyRemapper::s_keyboardHook = nullptr;
KeyRemapper* KeyRemapper::s_instance = nullptr;
std::map<int, int> KeyRemapper::s_keyMappings;
bool KeyRemapper::s_hookEnabled = false;
#endif

// KeyCaptureEditor 实现
KeyCaptureEditor::KeyCaptureEditor(QWidget *parent)
    : QLineEdit(parent)
{
    setReadOnly(true);
    setPlaceholderText("双击输入按键...");
}

void KeyCaptureEditor::keyPressEvent(QKeyEvent *event)
{
    int keyCode = event->nativeVirtualKey();
    QString keyName = getKeyNameFromEvent(event, keyCode);

    setText(keyName);
    emit keyCaptured(keyCode, keyName);
    event->accept();
}

void KeyCaptureEditor::focusInEvent(QFocusEvent *event)
{
    QLineEdit::focusInEvent(event);
    setPlaceholderText("按下任意键...");
}

QString KeyCaptureEditor::getKeyNameFromEvent(QKeyEvent *event, int &keyCode)
{
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

    return keyName;
}

// KeyCaptureDelegate 实现
KeyCaptureDelegate::KeyCaptureDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QWidget* KeyCaptureDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);

    KeyCaptureEditor* editor = new KeyCaptureEditor(parent);

    // 连接信号
    connect(editor, &KeyCaptureEditor::keyCaptured, this, [this, index](int keyCode, const QString& keyName) {
        emit keyCaptured(index.row(), index.column(), keyCode, keyName);
    });

    return editor;
}

void KeyCaptureDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    KeyCaptureEditor* captureEditor = qobject_cast<KeyCaptureEditor*>(editor);
    if (captureEditor) {
        QString value = index.model()->data(index, Qt::DisplayRole).toString();
        captureEditor->setText(value);
    }
}

void KeyCaptureDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    KeyCaptureEditor* captureEditor = qobject_cast<KeyCaptureEditor*>(editor);
    if (captureEditor) {
        model->setData(index, captureEditor->text(), Qt::EditRole);
    }
}

void KeyCaptureDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(index);
    editor->setGeometry(option.rect);
}

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

    // 创建键捕获委托
    keyCaptureDelegate = new KeyCaptureDelegate(this);

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
    mainLayout->setSpacing(5);

    // 精简的控制区域
    controlGroup = new QGroupBox("控制面板");
    controlLayout = new QHBoxLayout(controlGroup);
    controlGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    // 主要按钮
    addButton = new QPushButton("➕ 新建");
    addButton->setFixedHeight(32);
    addButton->setFixedWidth(80);

    removeButton = new QPushButton("➖ 删除");
    removeButton->setFixedHeight(32);
    removeButton->setFixedWidth(80);

    clearButton = new QPushButton("🗑️ 清空");
    clearButton->setFixedHeight(32);
    clearButton->setFixedWidth(80);

    // 文件操作按钮
    saveButton = new QPushButton("💾 保存");
    saveButton->setFixedHeight(32);
    saveButton->setFixedWidth(80);

    loadButton = new QPushButton("📂 加载");
    loadButton->setFixedHeight(32);
    loadButton->setFixedWidth(80);

    // 全局钩子开关
    enableGlobalHook = new QCheckBox("启用全局拦截");
    enableGlobalHook->setChecked(false);

    statusLabel = new QLabel("已就绪");
    statusLabel->setStyleSheet("font-weight: bold; color: #27ae60;");

    controlLayout->addWidget(addButton);
    controlLayout->addWidget(removeButton);
    controlLayout->addWidget(clearButton);
    controlLayout->addWidget(new QLabel(" | "));
    controlLayout->addWidget(saveButton);
    controlLayout->addWidget(loadButton);
    controlLayout->addStretch();
    controlLayout->addWidget(enableGlobalHook);
    controlLayout->addWidget(new QLabel(" | "));
    controlLayout->addWidget(statusLabel);

    // 映射表格
    mappingTable = new QTableWidget();
    mappingTable->setContextMenuPolicy(Qt::CustomContextMenu);

    // 布局组装
    mainLayout->addWidget(controlGroup, 0); // 固定高度
    mainLayout->addWidget(mappingTable, 1); // 拉伸占据剩余空间

    // 不再需要KeyCaptureWidget，直接在表格中捕获
    keyCaptureWidget = nullptr;

    // 保留这些按钮的引用但不显示，用于保持兼容性
    addAppButton = nullptr;
    addCommandButton = nullptr;
    editButton = nullptr;
    resetButton = nullptr;
    captureGroup = nullptr;
}

void KeyRemapper::setupTable()
{
    // 设置列：启用、原始按键（可点击捕获）、类型、目标（可点击捕获/选择）、操作
    QStringList headers = {"启用", "原始按键", "映射类型", "目标/动作", "详细信息", "操作"};
    mappingTable->setColumnCount(headers.size());
    mappingTable->setHorizontalHeaderLabels(headers);

    // 表格属性
    mappingTable->setAlternatingRowColors(true);
    mappingTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mappingTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    mappingTable->setSortingEnabled(false);
    mappingTable->setEditTriggers(QAbstractItemView::DoubleClicked); // 允许双击编辑

    // 设置键捕获委托到"原始按键"和"目标按键"列
    mappingTable->setItemDelegateForColumn(1, keyCaptureDelegate);
    mappingTable->setItemDelegateForColumn(3, keyCaptureDelegate);

    // 表头属性
    QHeaderView* header = mappingTable->horizontalHeader();
    header->setStretchLastSection(true);
    header->setSectionResizeMode(QHeaderView::Interactive);

    // 设置列宽
    mappingTable->setColumnWidth(0, 60);   // 启用
    mappingTable->setColumnWidth(1, 150);  // 原始按键
    mappingTable->setColumnWidth(2, 100);  // 类型
    mappingTable->setColumnWidth(3, 200);  // 目标
    mappingTable->setColumnWidth(4, 250);  // 详细信息
    mappingTable->setColumnWidth(5, 120);  // 操作

    // 垂直表头
    mappingTable->verticalHeader()->setVisible(false);
    mappingTable->verticalHeader()->setDefaultSectionSize(35);
}

void KeyRemapper::setupConnections()
{
    connect(addButton, &QPushButton::clicked, [this]() {
        // 弹出菜单选择映射类型
        QMenu menu(this);
        QAction* keyToKeyAction = menu.addAction("➕ 按键到按键");
        QAction* keyToAppAction = menu.addAction("🚀 按键启动应用");
        QAction* keyToCommandAction = menu.addAction("⚡ 按键执行命令");

        QAction* selected = menu.exec(QCursor::pos());
        if (selected == keyToKeyAction) {
            addNewMapping();
        } else if (selected == keyToAppAction) {
            addAppLauncher();
        } else if (selected == keyToCommandAction) {
            addCommandLauncher();
        }
    });

    connect(removeButton, &QPushButton::clicked, this, &KeyRemapper::removeMapping);
    connect(clearButton, &QPushButton::clicked, this, &KeyRemapper::clearAllMappings);
    connect(saveButton, &QPushButton::clicked, this, &KeyRemapper::saveProfile);
    connect(loadButton, &QPushButton::clicked, this, &KeyRemapper::loadProfile);

    connect(enableGlobalHook, &QCheckBox::toggled, [this](bool enabled) {
        if (enabled) {
            installGlobalHook();
        } else {
            uninstallGlobalHook();
        }
    });

    // 表格单元格点击事件
    connect(mappingTable, &QTableWidget::cellClicked, this, &KeyRemapper::toggleMapping);
    connect(mappingTable, &QTableWidget::cellDoubleClicked, this, &KeyRemapper::onKeyMappingDoubleClick);

    // 右键菜单
    connect(mappingTable, &QTableWidget::customContextMenuRequested, [this](const QPoint& pos) {
        QTableWidgetItem* item = mappingTable->itemAt(pos);
        if (!item) return;

        QMenu menu(this);
        QAction* editAction = menu.addAction("✏️ 编辑映射");
        QAction* deleteAction = menu.addAction("➖ 删除映射");
        menu.addSeparator();
        QAction* duplicateAction = menu.addAction("📋 复制映射");

        QAction* selected = menu.exec(mappingTable->viewport()->mapToGlobal(pos));
        if (selected == editAction) {
            editMapping();
        } else if (selected == deleteAction) {
            removeMapping();
        } else if (selected == duplicateAction) {
            // 实现复制功能
            int row = item->row();
            if (row >= 0 && row < keyMappings.size()) {
                KeyMapping newMapping = keyMappings[row];
                newMapping.originalKey = 0;
                newMapping.originalKeyName = "双击设置";
                keyMappings.append(newMapping);
                populateTable();
            }
        }
    });

    // 委托按键捕获信号
    connect(keyCaptureDelegate, &KeyCaptureDelegate::keyCaptured, this, [this](int row, int column, int keyCode, const QString& keyName) {
        if (row >= 0 && row < keyMappings.size()) {
            if (column == 1) { // 原始按键列
                keyMappings[row].originalKey = keyCode;
                keyMappings[row].originalKeyName = keyName;
            } else if (column == 3 && keyMappings[row].type == MappingType::KeyToKey) { // 目标按键列（仅按键映射类型）
                keyMappings[row].mappedKey = keyCode;
                keyMappings[row].mappedKeyName = keyName;
            }
            populateTable();
            updateGlobalMappings();
            showStatusMessage("按键已更新", 2000);
        }
    });
}

void KeyRemapper::addNewMapping()
{
    // 直接添加一个空的映射到表格
    KeyMapping newMapping;
    newMapping.originalKey = 0;
    newMapping.mappedKey = 0;
    newMapping.originalKeyName = "双击设置";
    newMapping.mappedKeyName = "双击设置";
    newMapping.enabled = true;
    newMapping.type = MappingType::KeyToKey;

    keyMappings.append(newMapping);
    populateTable();

    // 自动选中新添加的行
    int newRow = keyMappings.size() - 1;
    mappingTable->selectRow(newRow);

    showStatusMessage("已添加新映射，请双击单元格设置按键", 3000);
}

void KeyRemapper::addAppLauncher()
{
    // 直接添加一个空的应用启动映射到表格
    KeyMapping newMapping;
    newMapping.originalKey = 0;
    newMapping.originalKeyName = "双击设置";
    newMapping.enabled = true;
    newMapping.type = MappingType::KeyToApp;
    newMapping.appName = "未设置";

    keyMappings.append(newMapping);
    currentEditingRow = keyMappings.size() - 1;
    populateTable();

    // 直接显示应用选择对话框
    showAppSelector(MappingType::KeyToApp);
}

void KeyRemapper::addCommandLauncher()
{
    // 直接添加一个空的命令执行映射到表格
    KeyMapping newMapping;
    newMapping.originalKey = 0;
    newMapping.originalKeyName = "双击设置";
    newMapping.enabled = true;
    newMapping.type = MappingType::KeyToCommand;
    newMapping.command = "未设置";

    keyMappings.append(newMapping);
    currentEditingRow = keyMappings.size() - 1;
    populateTable();

    // 直接显示命令配置对话框
    showAppSelector(MappingType::KeyToCommand);
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
        const KeyMapping& mapping = keyMappings[currentRow];
        if (mapping.type == MappingType::KeyToApp || mapping.type == MappingType::KeyToCommand) {
            // 对于应用和命令类型，显示配置对话框
            currentEditingRow = currentRow;
            showAppSelector(mapping.type);
        } else {
            // 对于按键映射，提示用户双击单元格编辑
            showStatusMessage("请双击单元格编辑按键", 2000);
        }
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
    if (row < 0 || row >= keyMappings.size()) {
        return;
    }

    const KeyMapping& mapping = keyMappings[row];

    // 列1：原始按键列 - 始终允许双击编辑（由delegate处理）
    if (column == 1) {
        // 由KeyCaptureDelegate自动处理
        return;
    }

    // 列3：目标/动作列
    if (column == 3) {
        if (mapping.type == MappingType::KeyToKey) {
            // 按键映射类型：由delegate处理按键捕获
            return;
        } else {
            // 应用启动或命令执行类型：打开配置对话框
            currentEditingRow = row;
            showAppSelector(mapping.type);
        }
    }
}

void KeyRemapper::onOriginalKeyCaptured(int keyCode, const QString& keyName)
{
    // 此函数保留为空，不再使用KeyCaptureWidget
    // 按键捕获现在通过表格的委托直接处理
    Q_UNUSED(keyCode);
    Q_UNUSED(keyName);
}

void KeyRemapper::onMappedKeyCaptured(int keyCode, const QString& keyName)
{
    // 此函数保留为空，不再使用KeyCaptureWidget
    // 按键捕获现在通过表格的委托直接处理
    Q_UNUSED(keyCode);
    Q_UNUSED(keyName);
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

        // 原始按键列 - 允许编辑
        QTableWidgetItem* originalKeyItem = new QTableWidgetItem(mapping.originalKeyName);
        originalKeyItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
        mappingTable->setItem(i, 1, originalKeyItem);

        // 映射类型列
        mappingTable->setItem(i, 2, new QTableWidgetItem(getMappingTypeString(mapping.type)));

        // 目标/动作列 - 只有按键映射类型允许编辑
        QTableWidgetItem* targetItem = new QTableWidgetItem(getMappingDisplayText(mapping));
        if (mapping.type == MappingType::KeyToKey) {
            targetItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
        } else {
            targetItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        }
        mappingTable->setItem(i, 3, targetItem);

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
    if (currentEditingRow < 0 || currentEditingRow >= keyMappings.size()) {
        return;
    }

    const KeyMapping& mapping = keyMappings[currentEditingRow];

    if (type == MappingType::KeyToApp) {
        appSelectorDialog->setWindowTitle("配置应用程序启动");
        appPathEdit->setPlaceholderText("选择要启动的应用程序文件...");

        // 填充现有数据
        appPathEdit->setText(mapping.appPath);
        appNameEdit->setText(mapping.appName);
        workingDirEdit->setText(mapping.workingDir);
        argumentsEdit->setText(mapping.arguments);
    } else if (type == MappingType::KeyToCommand) {
        appSelectorDialog->setWindowTitle("配置命令执行");
        appPathEdit->setPlaceholderText("输入要执行的命令...");

        // 填充现有数据
        appPathEdit->setText(mapping.command);
        workingDirEdit->setText(mapping.workingDir);
        argumentsEdit->setText(mapping.arguments);
    }

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

