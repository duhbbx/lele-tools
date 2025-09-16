#include "mainwindow.h"

#include <iostream>
#include <QLabel>
#include <QListWidget>
#include <QBoxLayout>
#include "../tool-list/toollist.h"
#include <QObject>
#include "../common/dynamicobjectbase.h"

#include <QApplication>
#include <QScreen>
#include <QMessageBox>
#include <QSplitter>
#include <QMenu>
#include <QKeySequence>
#include <QDateTime>
#include <QProcess>
#include <QDebug>
#include <QDir>
#include <QTimer>
#include <QPushButton>
#include <QClipboard>
#include <QStandardPaths>
#include "../common/thirdparty/screen-capture/App/App.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif


MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), m_bPressed(false), isLeftPanelCollapsed(false),
                                          copyTooltip(nullptr)
#ifdef Q_OS_WIN
                                          , m_globalHotkeyRegistered(false)
#endif
{
    // 初始化设置和翻译器
    m_settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, "LeleTools", "Settings", this);
    m_translator = new QTranslator(this);
    m_languageActionGroup = new QActionGroup(this);

    // 加载保存的语言设置
    const QString savedLanguage = m_settings->value("language", "zh_CN").toString();
    loadLanguage(savedLanguage);

    // 截图功能固定为显示工具栏模式

    this->setWindowTitle(tr("乐乐的工具箱"));
    this->resize(1200, 800);

    // 移除标题栏，设置为无边框窗口
    this->setWindowFlags(Qt::FramelessWindowHint | Qt::Window);

    // 设置窗口可以调整大小
    this->setAttribute(Qt::WA_Hover, true);

    // 优化窗口缩放时的背景，防止黑色区域
    this->setAttribute(Qt::WA_OpaquePaintEvent, false);
    this->setAttribute(Qt::WA_NoSystemBackground, false);
    this->setAttribute(Qt::WA_TranslucentBackground, false);
    this->setAttribute(Qt::WA_StaticContents);
    this->setAttribute(Qt::WA_PaintOnScreen, false);
    // 设置全局样式
    const QString globalStyle = GlobalStyles::getGlobalStyle();

    this->setStyleSheet(globalStyle);

    // 创建主容器
    auto* mainWidget = new QWidget(this);

    // 主容器是垂直布局的
    auto* mainLayout = new QVBoxLayout(mainWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 创建自定义标题栏
    createTitleBar();
    mainLayout->addWidget(titleBar);

    // 创建主内容区域
    auto* contentWidget = new QWidget();
    auto* contentLayout = new QHBoxLayout(contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    // 创建左侧面板（功能菜单）
    leftPanel = new QWidget();
    leftPanel->setFixedWidth(200); // 进一步缩窄左侧宽度
    leftPanel->setContentsMargins(0, 0, 0, 0);
    leftPanel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    leftPanel->setStyleSheet("background-color: #f8f9fa;");

    // 创建右侧面板（工作区）
    rightPanel = new QWidget();
    rightPanel->setContentsMargins(0, 0, 0, 0);
    rightPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    rightPanel->setStyleSheet("background-color: #ffffff;");

    // 创建右侧标签页窗口
    rightTabWidget = new MainWindowTabWidget();
    rightTabWidget->setContentsMargins(0, 0, 0, 0);
    rightTabWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    rightTabWidget->setTabsClosable(true); // 启用关闭按钮
    rightTabWidget->setMovable(true); // 允许拖拽标签页

    // 创建欢迎页面
    QWidget* welcomePage = new QWidget();
    QVBoxLayout* welcomeLayout = new QVBoxLayout(welcomePage);
    QLabel* welcomeLabel = new QLabel("欢迎使用乐乐的工具箱\n\n请从左侧菜单选择要使用的工具");
    welcomeLabel->setAlignment(Qt::AlignCenter);
    welcomeLabel->setStyleSheet("font-size: 18px; color: #666; padding: 50px;");
    welcomeLayout->addWidget(welcomeLabel);

    rightTabWidget->addTab(welcomePage, "首页");

    // 设置标签页样式和功能
    setupTabWidget();

    // 设置标签页下拉菜单
    setupTabDropdown();

    // 设置右侧面板布局
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0); // 无边距
    rightLayout->setSpacing(0);
    rightLayout->addWidget(rightTabWidget, 1); // 设置拉伸因子为1，让标签页填满右侧面板

    // 创建工具列表并添加到左侧面板
    auto* toolList = new ToolList(this, nullptr);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(5, 5, 5, 5); // 保持适当边距用于美观
    leftLayout->setSpacing(0);
    leftLayout->addWidget(toolList, 1); // 设置拉伸因子为1，让ToolList填满整个左侧面板

    // 将左右面板添加到水平布局
    contentLayout->addWidget(leftPanel, 0); // 左侧固定宽度，不拉伸
    contentLayout->addWidget(rightPanel, 1); // 右侧拉伸填满剩余空间

    // 将内容区域添加到主布局
    mainLayout->addWidget(contentWidget, 1); // 设置拉伸因子为1，让内容区域填满可用空间

    // 创建状态栏
    createStatusBar();
    mainLayout->addWidget(statusBar);

    // 设置主容器为中央部件
    this->setCentralWidget(mainWidget);

    // 创建菜单栏内容
    createMenuBar();

    // 设置截图功能
    setupScreenCapture();

    // 移除中间折叠按钮，改用状态栏toggle

    // 添加窗口阴影效果
    QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect(this);
    shadowEffect->setOffset(0, 6);
    shadowEffect->setBlurRadius(40);
    shadowEffect->setColor(QColor(0, 0, 0, 180));
    this->setGraphicsEffect(shadowEffect);

    qDebug() << "MainWindow初始化完成，使用左右分栏布局";
}

MainWindow::~MainWindow() {
#ifdef Q_OS_WIN
    // 取消注册系统级热键
    if (m_globalHotkeyRegistered) {
        UnregisterHotKey(reinterpret_cast<HWND>(winId()), HOTKEY_ID);
        qDebug() << "系统级F1热键已取消注册";
    }
#endif
}

void MainWindow::closeEvent(QCloseEvent* event) {
    const QMessageBox::StandardButton resBtn = QMessageBox::question(this, "退出确认",
                                                                     tr("确定要退出乐乐的工具箱吗？"),
                                                                     QMessageBox::Cancel | QMessageBox::Yes,
                                                                     QMessageBox::Yes);
    if (resBtn != QMessageBox::Yes) {
        event->ignore();
    } else {
        event->accept();
    }
}

void MainWindow::itemClickedSlot(QListWidgetItem* item) {
    QString stringValue = item->data(Qt::UserRole).toString();
    QString title = item->data(Qt::DisplayRole).toString();
    DynamicObjectBase* object = DynamicObjectFactory::Instance()->CreateObject(stringValue.toStdString());

    if (!object) {
        // 创建一个错误提示对话框
        QMessageBox errorBox;
        errorBox.setIcon(QMessageBox::Critical);
        errorBox.setWindowTitle("错误提示");
        errorBox.setText("对应的模块还未实现");
        errorBox.setStandardButtons(QMessageBox::Ok);
        errorBox.exec();
        return;
    }

    QWidget* widget = dynamic_cast<QWidget*>(object);
    if (widget) {
        // 检查是否已经有相同标题的标签页
        for (int i = 0; i < rightTabWidget->count(); ++i) {
            if (rightTabWidget->tabText(i) == title) {
                // 如果已存在，切换到该标签页
                rightTabWidget->setCurrentIndex(i);
                return;
            }
        }

        // 添加新的工具页面作为标签页
        int index = rightTabWidget->addTab(widget, title);
        rightTabWidget->setCurrentIndex(index);

        // 更新窗口标题显示当前工具
        this->setWindowTitle(QString("乐乐的工具箱 - %1").arg(title));

        // 更新下拉菜单
        updateTabDropdownMenu();
    }
}

void MainWindow::createTitleBar() {
    titleBar = new QWidget();
    titleBar->setFixedHeight(35);
    titleBar->setStyleSheet("background-color: #f8f9fa; border-bottom: 1px solid #dee2e6;");

    QHBoxLayout* titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(5, 0, 0, 0);
    titleLayout->setSpacing(0);

    // 创建菜单栏
    customMenuBar = new QMenuBar();
    customMenuBar->setStyleSheet(
        "QMenuBar {"
        "    background-color: transparent;"
        "    color: #343a40;"
        "    border: none;"
        "    padding: 0px;"
        "    margin: 0px;"
        "    spacing: 0px;"
        "    outline: none;"
        "}"
        "QMenuBar::item {"
        "    background-color: transparent;"
        "    color: #343a40;"
        "    padding: 6px 10px;"
        "    margin: 0px 1px;"
        "    border-radius: 0px;"
        "    border: none;"
        "    outline: none;"
        "}"
        "QMenuBar::item:selected {"
        "    background-color: #e9ecef;"
        "    border: none;"
        "}"
        "QMenuBar::item:pressed {"
        "    background-color: #dee2e6;"
        "    border: none;"
        "}"
    );

    // 设置菜单栏的大小策略为最小
    customMenuBar->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    // 在这里我们稍后会填充菜单

    titleLayout->addWidget(customMenuBar);
    titleLayout->addStretch(); // 占用剩余空间，推动窗口控制按钮到右侧

    // 创建窗口控制按钮
    setupWindowControls();

    titleLayout->addWidget(minimizeButton);
    titleLayout->addWidget(maximizeButton);
    titleLayout->addWidget(closeButton);
}

void MainWindow::createMenuBar() {
    // 文件菜单
    QMenu* fileMenu = customMenuBar->addMenu(tr("文件(&F)"));
    QString menuStyle =
        "QMenu {"
        "    background-color: #ffffff;"
        "    color: #343a40;"
        "    border: 1px solid #e0e0e0;"
        "    border-radius: 0px;"
        "    padding: 8px 4px;"
        "    outline: none;"
        "    selection-background-color: #e9ecef;"
        "}"
        "QMenu::item {"
        "    background-color: transparent;"
        "    color: #343a40;"
        "    padding: 8px 16px;"
        "    border-radius: 0px;"
        "    margin: 2px 6px;"
        "    border: none;"
        "    min-width: 120px;"
        "}"
        "QMenu::item:selected {"
        "    background-color: #e9ecef;"
        "    border: none;"
        "    color: #343a40;"
        "}"
        "QMenu::item:pressed {"
        "    background-color: #dee2e6;"
        "}"
        "QMenu::separator {"
        "    height: 1px;"
        "    background-color: #e0e0e0;"
        "    margin: 8px 16px;"
        "    border: none;"
        "}";

    fileMenu->setStyleSheet(menuStyle);

    QAction* exitAction = new QAction(tr("退出(&X)"), this);
    exitAction->setShortcut(QKeySequence::Quit);
    exitAction->setStatusTip(tr("退出应用程序"));
    // 设置退出图标 (Unicode字符)
    exitAction->setIcon(this->style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    connect(exitAction, &QAction::triggered, this, &MainWindow::exitApplication);
    fileMenu->addAction(exitAction);

    // 工具菜单
    QMenu* toolsMenu = customMenuBar->addMenu(tr("工具(&T)"));
    toolsMenu->setStyleSheet(menuStyle); // 使用相同样式

    QAction* backToHomeAction = new QAction(tr("返回首页(&H)"), this);
    backToHomeAction->setShortcut(QKeySequence(tr("Ctrl+H")));
    backToHomeAction->setStatusTip(tr("返回到工具选择页面"));
    backToHomeAction->setIcon(this->style()->standardIcon(QStyle::SP_DirHomeIcon));
    connect(backToHomeAction, &QAction::triggered, [this]() {
        rightTabWidget->setCurrentIndex(0);
        this->setWindowTitle("乐乐的工具箱");
    });
    toolsMenu->addAction(backToHomeAction);

    toolsMenu->addSeparator();

    QAction* screenshotAction = new QAction(tr("截图(&S)\tF1"), this);
    // 快捷键通过全局QShortcut实现，不在菜单中设置
    // screenshotAction->setShortcut(QKeySequence(Qt::Key_F1));
    screenshotAction->setStatusTip(tr("按F1或点击此处开始截图"));
    screenshotAction->setIcon(this->style()->standardIcon(QStyle::SP_DesktopIcon));
    connect(screenshotAction, &QAction::triggered, this, &MainWindow::startScreenCapture);
    toolsMenu->addAction(screenshotAction);

    // 帮助菜单
    QMenu* helpMenu = customMenuBar->addMenu(tr("帮助(&H)"));
    helpMenu->setStyleSheet(menuStyle); // 使用相同样式

    QAction* helpAction = new QAction(tr("使用说明(&U)"), this);
    // helpAction->setShortcut(QKeySequence::HelpContents);
    helpAction->setStatusTip(tr("查看使用说明"));
    helpAction->setIcon(this->style()->standardIcon(QStyle::SP_DialogHelpButton));
    connect(helpAction, &QAction::triggered, this, &MainWindow::showHelp);
    helpMenu->addAction(helpAction);

    helpMenu->addSeparator();

    QAction* aboutAction = new QAction(tr("关于(&A)"), this);
    aboutAction->setStatusTip(tr("关于乐乐的工具箱"));
    aboutAction->setIcon(this->style()->standardIcon(QStyle::SP_MessageBoxInformation));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
    helpMenu->addAction(aboutAction);

    // 语言菜单
    QMenu* languageMenu = customMenuBar->addMenu(tr("语言(&L)"));
    languageMenu->setStyleSheet(menuStyle); // 使用相同样式

    QAction* chineseAction = new QAction(tr("中文（简体）"), this);
    chineseAction->setCheckable(true);
    chineseAction->setData("zh_CN");
    m_languageActionGroup->addAction(chineseAction);
    connect(chineseAction, &QAction::triggered, [this]() {
        changeLanguage("zh_CN");
    });
    languageMenu->addAction(chineseAction);

    QAction* englishAction = new QAction(tr("English"), this);
    englishAction->setCheckable(true);
    englishAction->setData("en_US");
    m_languageActionGroup->addAction(englishAction);
    connect(englishAction, &QAction::triggered, [this]() {
        changeLanguage("en_US");
    });
    languageMenu->addAction(englishAction);

    // 设置当前语言的选中状态
    QString currentLang = m_settings->value("language", "zh_CN").toString();
    if (currentLang == "zh_CN") {
        chineseAction->setChecked(true);
    } else {
        englishAction->setChecked(true);
    }
}

void MainWindow::setupWindowControls() {
    QString buttonStyle =
        "QPushButton {"
        "    background-color: transparent;"
        "    border: none;"
        "    color: #343a40;"
        "    font-size: 12px;"
        "    width: 20px;"
        "    height: 20px;"
        "    margin: 1px;"
        "    border-radius: 0px;"
        "    font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;"
        "    font-weight: normal;"
        "}"
        "QPushButton:hover {"
        "    background-color: #e9ecef;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #dee2e6;"
        "}";

    QString closeButtonStyle =
        "QPushButton {"
        "    background-color: transparent;"
        "    border: none;"
        "    color: #343a40;"
        "    font-size: 12px;"
        "    width: 20px;"
        "    height: 20px;"
        "    margin: 1px;"
        "    border-radius: 0px;"
        "    font-family: 'Segoe UI', Arial, sans-serif;"
        "    font-weight: normal;"
        "}"
        "QPushButton:hover {"
        "    background-color: #e74c3c;"
        "    color: #ffffff;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #c0392b;"
        "    color: #ffffff;"
        "}";

    // 最小化按钮
    minimizeButton = new QPushButton("−");
    minimizeButton->setStyleSheet(buttonStyle);
    minimizeButton->setToolTip("最小化");
    connect(minimizeButton, &QPushButton::clicked, this, &QWidget::showMinimized);

    // 最大化/还原按钮
    maximizeButton = new QPushButton("□");
    maximizeButton->setStyleSheet(buttonStyle);
    maximizeButton->setToolTip("最大化");
    connect(maximizeButton, &QPushButton::clicked, [this]() {
        if (this->isMaximized()) {
            this->showNormal();
            maximizeButton->setText("□");
            maximizeButton->setToolTip("最大化");
        } else {
            this->showMaximized();
            maximizeButton->setText("◱");
            maximizeButton->setToolTip("还原");
        }
    });

    // 关闭按钮
    closeButton = new QPushButton("×");
    closeButton->setStyleSheet(closeButtonStyle);
    closeButton->setToolTip("关闭");
    connect(closeButton, &QPushButton::clicked, this, &MainWindow::close);
}

void MainWindow::createStatusBar() {
    statusBar = new QWidget();
    statusBar->setFixedHeight(28);
    statusBar->setStyleSheet("background-color: #f0f0f0; border-top: 1px solid #ddd;");

    auto* statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(10, 3, 10, 3);

    // 添加左侧面板toggle按钮
    leftPanelToggle = new QPushButton();
    leftPanelToggle->setFixedSize(85, 22);
    leftPanelToggle->setText("◀ 收起");
    leftPanelToggle->setStyleSheet(
        "QPushButton {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #f8f9fa, stop:1 #e9ecef);"
        "    border: 1px solid #ced4da;"
        "    border-radius: 0px;"
        "    color: #495057;"
        "    font-size: 12px;"
        "    font-weight: bold;"
        "    font-family: 'Microsoft YaHei', '微软雅黑', sans-serif;"
        "    padding: 2px 6px;"
        "    text-align: center;"
        "}"
        "QPushButton:hover {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #e3f2fd, stop:1 #bbdefb);"
        "    border-color: #2196F3;"
        "    color: #1976D2;"
        "}"
        "QPushButton:pressed {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #bbdefb, stop:1 #90caf9);"
        "    border-color: #1976D2;"
        "    color: #0d47a1;"
        "}"
        "QPushButton:focus {"
        "    outline: none;"
        "    border-color: #2196F3;"
        "}"
    );
    leftPanelToggle->setToolTip("收起/展开左侧工具栏");
    connect(leftPanelToggle, &QPushButton::clicked, this, &MainWindow::toggleLeftPanel);

    // 添加时间标签
    timeLabel = new QLabel();
    timeLabel->setStyleSheet(
        "QLabel {"
        "    color: #6c757d;"
        "    font-size: 12px;"
        "    padding: 2px 8px;"
        "    border-radius: 0px;"
        "}"
        "QLabel:hover {"
        "    background-color: #e9ecef;"
        "}"
    );
    timeLabel->setToolTip("点击复制时间");
    updateTime(); // 初始化时间显示

    // 使时间标签可点击
    timeLabel->setAttribute(Qt::WA_Hover, true);
    timeLabel->installEventFilter(this);

    statusLayout->addWidget(leftPanelToggle);
    statusLayout->addStretch(); // 推到右侧
    statusLayout->addWidget(timeLabel);

    // 创建定时器，每秒更新一次时间
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateTime);
    timer->start(1000); // 1秒间隔
}

void MainWindow::updateTime() const {
    QDateTime currentTime = QDateTime::currentDateTime();

    // 获取当前应用程序的语言环境
    QLocale locale = QLocale::system();

    // 如果应用程序设置了翻译器，使用翻译器的语言
    if (m_translator && !m_translator->isEmpty()) {
        // 根据当前语言设置选择合适的语言环境
        QString currentLang = m_settings->value("language", "zh_CN").toString();
        if (currentLang == "zh_CN") {
            locale = QLocale(QLocale::Chinese, QLocale::China);
        } else if (currentLang == "en_US") {
            locale = QLocale(QLocale::English, QLocale::UnitedStates);
        }
    }

    // 使用语言环境格式化日期时间
    QString dateTimeStr = locale.toString(currentTime, "yyyy-MM-dd hh:mm:ss dddd");
    timeLabel->setText(dateTimeStr);
}

void MainWindow::toggleLeftPanel() {
    if (isLeftPanelCollapsed) {
        // 展开左侧面板
        leftPanel->setVisible(true);
        leftPanelToggle->setText("◀ 收起");
        leftPanelToggle->setToolTip("收起左侧工具栏");
        isLeftPanelCollapsed = false;
    } else {
        // 收起左侧面板
        leftPanel->setVisible(false);
        leftPanelToggle->setText("▶ 展开");
        leftPanelToggle->setToolTip("展开左侧工具栏");
        isLeftPanelCollapsed = true;
    }
}

void MainWindow::setupTabWidget() {
    // 标签页样式已在 MainWindowTabWidget 中设置


    // 连接标签页关闭信号
    connect(rightTabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);

    // 连接标签页切换信号
    connect(rightTabWidget, &QTabWidget::currentChanged, [this](int index) {
        if (index >= 0 && index < rightTabWidget->count()) {
            QString title = rightTabWidget->tabText(index);
            if (title == "首页") {
                this->setWindowTitle("乐乐的工具箱");
            } else {
                this->setWindowTitle(QString("乐乐的工具箱 - %1").arg(title));
            }
        }
        updateTabDropdownMenu(); // 更新下拉菜单
    });
}

void MainWindow::closeTab(int index) {
    // 防止关闭首页
    if (index == 0) {
        return;
    }

    QWidget* widget = rightTabWidget->widget(index);
    if (widget) {
        rightTabWidget->removeTab(index);
        widget->deleteLater(); // 安全删除widget及其资源

        // 如果关闭后只剩首页，更新窗口标题
        if (rightTabWidget->count() == 1) {
            this->setWindowTitle("乐乐的工具箱");
        }

        // 更新下拉菜单
        updateTabDropdownMenu();
    }
}

void MainWindow::showAbout() {
    QMessageBox::about(this, tr("关于乐乐的工具箱"),
                       tr("<h2>乐乐的工具箱</h2>"
                           "<p>版本 1.0</p>"
                           "<p>一个集成了多种实用工具的桌面应用程序。</p>"
                           "<p>包含了编码解码、格式转换、文本处理、图像处理等多种常用工具。</p>"
                           "<p>© 2024 乐乐工作室</p>"));
}

void MainWindow::showHelp() {
    QMessageBox::information(this, tr("使用说明"),
                             tr("<h3>使用说明</h3>"
                                 "<p><b>基本操作：</b></p>"
                                 "<ul>"
                                 "<li>从左侧菜单选择需要使用的工具</li>"
                                 "<li>工具将在右侧工作区打开</li>"
                                 "<li>可以通过菜单栏快速返回首页</li>"
                                 "</ul>"
                                 "<p><b>截图功能：</b></p>"
                                 "<ul>"
                                 "<li>按F1键开始截图</li>"
                                 "<li>拖拽选择要截图的区域</li>"
                                 "<li>释放鼠标后会显示编辑工具栏</li>"
                                 "<li>可以添加箭头、文字、马赛克等</li>"
                                 "<li>完成编辑后点击保存按钮</li>"
                                 "</ul>"
                                 "<p><b>快捷键：</b></p>"
                                 "<ul>"
                                 "<li>F1 - 截图</li>"
                                 "<li>ESC - 取消截图</li>"
                                 "<li>Ctrl+H - 返回首页</li>"
                                 "<li>Ctrl+Q - 退出程序</li>"
                                 "</ul>"));
}

void MainWindow::exitApplication() {
    close();
}

// 鼠标事件处理 - 实现窗口拖拽
void MainWindow::mousePressEvent(QMouseEvent* event) {
    // 由于使用了Windows原生缩放，只需要处理拖拽逻辑
    if (event->button() == Qt::LeftButton && isDraggableArea(event->pos())) {
        m_bPressed = true;
        m_point = event->pos();
    }
    QMainWindow::mousePressEvent(event);
}

bool MainWindow::isDraggableArea(const QPoint& pos) const {
    // 检查是否在标题栏区域
    if (titleBar && titleBar->geometry().contains(pos)) {
        // 计算标题栏内的相对位置
        QPoint titleBarPos = pos - titleBar->pos();

        // 精确计算菜单栏区域（只包括菜单项，不包括右侧空白）
        QRect menuBarRect = customMenuBar->geometry();

        // 菜单栏的实际宽度（只包含菜单项）
        int menuWidth = 0;
        for (QAction* action : customMenuBar->actions()) {
            if (action->isVisible()) {
                menuWidth += customMenuBar->actionGeometry(action).width();
            }
        }

        // 只有在实际菜单项区域内才排除拖拽
        QRect actualMenuRect = QRect(menuBarRect.x(), menuBarRect.y(), menuWidth, menuBarRect.height());
        if (actualMenuRect.contains(titleBarPos)) {
            return false;
        }

        // 排除窗口控制按钮区域（精确计算，减少排除区域）
        int buttonAreaWidth = minimizeButton->width() + maximizeButton->width() + closeButton->width() + 5;
        QRect controlsRect = QRect(titleBar->width() - buttonAreaWidth, 0, buttonAreaWidth, titleBar->height());
        if (controlsRect.contains(titleBarPos)) {
            return false;
        }

        return true;
    }

    // 检查是否在状态栏区域
    if (statusBar && statusBar->geometry().contains(pos)) {
        // 排除时间标签区域
        QPoint statusBarPos = pos - statusBar->pos();
        if (timeLabel && timeLabel->geometry().contains(statusBarPos)) {
            return false;
        }
        return true;
    }

    return false;
}

// getResizeDirection 和 isResizeArea 方法已移除，使用Windows原生缩放

void MainWindow::mouseMoveEvent(QMouseEvent* event) {
    // 移除自定义缩放逻辑，使用Windows原生缩放
    if (m_bPressed && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_point);
        setCursor(Qt::ArrowCursor); // 使用普通箭头光标
    } else {
        setCursor(Qt::ArrowCursor); // 始终使用箭头光标
    }
    QMainWindow::mouseMoveEvent(event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_bPressed = false;
        setCursor(Qt::ArrowCursor); // 重置为箭头光标
    }
    QMainWindow::mouseReleaseEvent(event);
}

void MainWindow::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (isDraggableArea(event->pos())) {
            // 双击可拖拽区域实现最大化/还原
            if (this->isMaximized()) {
                this->showNormal();
                maximizeButton->setText("□");
                maximizeButton->setToolTip("最大化");
            } else {
                this->showMaximized();
                maximizeButton->setText("◱");
                maximizeButton->setToolTip("还原");
            }
        }
    }
    QMainWindow::mouseDoubleClickEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);

    // 确保窗口内容在缩放时正确重绘
    this->update();

    // 更新所有子组件
    if (titleBar)
        titleBar->update();
    if (statusBar)
        statusBar->update();
    if (rightTabWidget)
        rightTabWidget->update();
    if (leftPanel)
        leftPanel->update();
}

void MainWindow::copyTimeToClipboard() {
    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setText(timeLabel->text());

    // 显示复制成功提示
    showCopyTooltip();
}

void MainWindow::showCopyTooltip() {
    // 如果已经有提示框，先删除
    if (copyTooltip) {
        copyTooltip->deleteLater();
        copyTooltip = nullptr;
    }

    // 创建提示框
    copyTooltip = new QWidget(this, Qt::ToolTip | Qt::FramelessWindowHint);
    copyTooltip->setFixedSize(120, 40);
    copyTooltip->setStyleSheet(
        "QWidget {"
        "    background-color: #28a745;"
        "    color: white;"
        "    border-radius: 0px;"
        "    font-size: 12px;"
        "    font-weight: bold;"
        "}"
    );

    QHBoxLayout* tooltipLayout = new QHBoxLayout(copyTooltip);
    tooltipLayout->setContentsMargins(10, 5, 10, 5);

    QLabel* tooltipLabel = new QLabel("复制成功！");
    tooltipLabel->setAlignment(Qt::AlignCenter);
    tooltipLabel->setStyleSheet("background-color: transparent; border: none;");
    tooltipLayout->addWidget(tooltipLabel);

    // 计算提示框位置（在时间标签上方）
    QPoint timePos = timeLabel->mapToGlobal(QPoint(0, 0));
    copyTooltip->move(timePos.x() - 30, timePos.y() - 50);

    // 显示提示框
    copyTooltip->show();
    copyTooltip->raise();

    // 2秒后自动隐藏并销毁
    QTimer::singleShot(2000, [this]() {
        if (copyTooltip) {
            copyTooltip->deleteLater();
            copyTooltip = nullptr;
        }
    });
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
    if (obj == timeLabel && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            copyTimeToClipboard();
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

#ifdef Q_OS_WIN
bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
    MSG* msg = static_cast<MSG*>(message);

    // 处理系统级热键
    if (msg->message == WM_HOTKEY) {
        if (msg->wParam == HOTKEY_ID) {
            qDebug() << "系统级F1热键被触发";
            // 在主线程中调用截图功能
            QMetaObject::invokeMethod(this, "startScreenCapture", Qt::QueuedConnection);
            return true;
        }
    }

    if (msg->message == WM_NCHITTEST) {
        const LONG borderWidth = 8; // 缩放敏感区域宽度
        RECT winrect;
        GetWindowRect(HWND(winId()), &winrect);

        long x = GET_X_LPARAM(msg->lParam);
        long y = GET_Y_LPARAM(msg->lParam);

        // 检查是否在标题栏区域（用于拖拽）
        // 获取标题栏高度
        int titleBarHeight = 40; // 标题栏高度
        if (titleBar) {
            titleBarHeight = titleBar->height();
        }

        // 检查是否在窗口边缘（缩放区域）
        bool onLeft = (x >= winrect.left && x < winrect.left + borderWidth);
        bool onRight = (x < winrect.right && x >= winrect.right - borderWidth);
        bool onTop = (y >= winrect.top && y < winrect.top + borderWidth);
        bool onBottom = (y < winrect.bottom && y >= winrect.bottom - borderWidth);

        // 四个角落
        if (onTop && onLeft) {
            *result = HTTOPLEFT;
            return true;
        }
        if (onTop && onRight) {
            *result = HTTOPRIGHT;
            return true;
        }
        if (onBottom && onLeft) {
            *result = HTBOTTOMLEFT;
            return true;
        }
        if (onBottom && onRight) {
            *result = HTBOTTOMRIGHT;
            return true;
        }

        // 四个边
        if (onLeft) {
            *result = HTLEFT;
            return true;
        }
        if (onRight) {
            *result = HTRIGHT;
            return true;
        }
        if (onTop) {
            *result = HTTOP;
            return true;
        }
        if (onBottom) {
            *result = HTBOTTOM;
            return true;
        }

        // 检查是否在标题栏拖拽区域
        if (y >= winrect.top && y < winrect.top + titleBarHeight) {
            // 转换为窗口相对坐标
            QPoint globalPos(x, y);
            QPoint localPos = mapFromGlobal(globalPos);

            // 检查是否在可拖拽区域（避开按钮等控件）
            if (isDraggableArea(localPos)) {
                *result = HTCAPTION; // 标题栏区域，允许拖拽
                return true;
            }
        }
    }

    return QMainWindow::nativeEvent(eventType, message, result);
}
#endif

void MainWindow::setupTabDropdown() {
    // 创建下拉按钮
    tabDropdownButton = new QPushButton("⋮");
    tabDropdownButton->setFixedSize(25, 25);
    tabDropdownButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #f8f9fa;"
        "    border: 1px solid #dee2e6;"
        "    border-radius: 0px;"
        "    color: #495057;"
        "    font-size: 12px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background-color: #e9ecef;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #dee2e6;"
        "}"
    );
    tabDropdownButton->setToolTip("显示所有标签页");

    // 创建下拉菜单
    tabDropdownMenu = new QMenu(this);
    tabDropdownMenu->setStyleSheet(
        "QMenu {"
        "    background-color: #ffffff;"
        "    color: #343a40;"
        "    border: 1px solid #dee2e6;"
        "    border-radius: 0px;"
        "    padding: 4px;"
        "}"
        "QMenu::item {"
        "    background-color: transparent;"
        "    color: #343a40;"
        "    padding: 6px 12px;"
        "    border-radius: 0px;"
        "    margin: 1px;"
        "}"
        "QMenu::item:selected {"
        "    background-color: #007bff;"
        "    color: white;"
        "}"
    );

    // 将按钮添加到标签页条的右上角
    rightTabWidget->setCornerWidget(tabDropdownButton, Qt::TopRightCorner);

    // 连接按钮点击事件
    connect(tabDropdownButton, &QPushButton::clicked, [this]() {
        updateTabDropdownMenu();
        QPoint buttonPos = tabDropdownButton->mapToGlobal(QPoint(0, tabDropdownButton->height()));
        tabDropdownMenu->exec(buttonPos);
    });
}

void MainWindow::updateTabDropdownMenu() {
    tabDropdownMenu->clear();

    for (int i = 0; i < rightTabWidget->count(); ++i) {
        QString tabText = rightTabWidget->tabText(i);
        QAction* action = tabDropdownMenu->addAction(tabText);

        // 当前激活的标签页显示特殊样式
        if (i == rightTabWidget->currentIndex()) {
            action->setText("● " + tabText);
        }

        // 连接点击事件
        connect(action, &QAction::triggered, [this, i]() {
            switchToTab(i);
        });
    }
}

void MainWindow::switchToTab(int index) {
    if (index >= 0 && index < rightTabWidget->count()) {
        rightTabWidget->setCurrentIndex(index);
    }
}

void MainWindow::loadLanguage(const QString& language) {
    // 移除之前的翻译器
    QApplication::removeTranslator(m_translator);

    // 加载新的翻译文件
    QString qmFile = QString(":/i18n/lele-tools_%1.qm").arg(language);
    qDebug() << "MainWindow::loadLanguage - Loading:" << language;
    qDebug() << "MainWindow::loadLanguage - QM file path:" << qmFile;

    if (m_translator->load(qmFile)) {
        QApplication::installTranslator(m_translator);
        qDebug() << "MainWindow::loadLanguage - Translation loaded and installed successfully";
    } else {
        qDebug() << "MainWindow::loadLanguage - Failed to load translation file";
    }
}

void MainWindow::changeLanguage(const QString& language) {
    // 保存语言设置
    m_settings->setValue("language", language);

    // 更新菜单选中状态
    foreach(QAction *action, m_languageActionGroup->actions()) {
        if (action->data().toString() == language) {
            action->setChecked(true);
            break;
        }
    }

    // 加载新语言
    loadLanguage(language);

    // 显示重启提示
    QString languageName = (language == "zh_CN") ? tr("中文（简体）") : tr("English");
    QMessageBox msgBox(this);
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setWindowTitle(tr("语言已更改"));
    msgBox.setText(tr("语言已更改为 %1。\n请重启应用程序以使更改完全生效。").arg(languageName));

    // 创建按钮并保存引用
    QPushButton* restartNowBtn = msgBox.addButton(tr("立即重启"), QMessageBox::AcceptRole);
    QPushButton* restartLaterBtn = msgBox.addButton(tr("稍后重启"), QMessageBox::RejectRole);

    int ret = msgBox.exec();
    QPushButton* clickedBtn = qobject_cast<QPushButton*>(msgBox.clickedButton());

    qDebug() << "changeLanguage - 返回值：" << ret;
    qDebug() << "changeLanguage - 点击的按钮：" << (clickedBtn ? clickedBtn->text() : "未知");
    qDebug() << "changeLanguage - 是否是立即重启按钮：" << (clickedBtn == restartNowBtn);

    if (clickedBtn == restartNowBtn) {
        // 重启应用程序
        // 获取应用程序路径和参数
        QString program = QApplication::applicationFilePath();
        QStringList arguments = QApplication::arguments();
        arguments.removeFirst(); // 移除程序名

        qDebug() << "Restart - Program path:" << program;
        qDebug() << "Restart - Arguments:" << arguments;
        qDebug() << "Restart - Working directory:" << QDir::currentPath();

        // 尝试不同的重启方法
        bool startResult = false;

        // 方法1：使用完整路径和工作目录
        QString workingDir = QApplication::applicationDirPath();
        startResult = QProcess::startDetached(program, arguments, workingDir);
        qDebug() << "Restart - Method 1 result:" << startResult;

        if (!startResult) {
            // 方法2：不带参数启动
            startResult = QProcess::startDetached(program, QStringList(), workingDir);
            qDebug() << "Restart - Method 2 result:" << startResult;
        }

        if (!startResult) {
            // 方法3：使用系统命令
#ifdef Q_OS_WIN
            QString command = QString("start \"\" \"%1\"").arg(program);
            startResult = (QProcess::execute("cmd", QStringList() << "/c" << command) == 0);
            qDebug() << "Restart - Method 3 (Windows) result:" << startResult;
#else
            startResult = QProcess::startDetached(program);
            qDebug() << "Restart - Method 3 (Unix) result:" << startResult;
#endif
        }

        if (startResult) {
            qDebug() << "Restart - New instance started successfully, quitting current instance";
            // 延迟退出，确保新实例启动
            QTimer::singleShot(500, []() {
                QApplication::quit();
            });
        } else {
            qDebug() << "Restart - All methods failed";
            QMessageBox::warning(this, tr("重启失败"),
                                 tr("无法启动新实例，请手动重启应用程序。\n\n程序路径: %1").arg(program));
        }
    }
}

void MainWindow::setupScreenCapture() {
    qDebug() << "设置截图功能...";
    
#ifdef WITH_SCREEN_CAPTURE
    // 创建截图API实例
    m_screenCapture = new ScreenCaptureAPI(this);

    // 连接截图完成信号
    connect(m_screenCapture, &ScreenCaptureAPI::captureCompleted, this, &MainWindow::onCaptureCompleted);
#endif

    // 注册系统级全局快捷键F1
#ifdef Q_OS_WIN
    // 注册系统级热键 F1
    if (RegisterHotKey(reinterpret_cast<HWND>(winId()), HOTKEY_ID, 0, VK_F1)) {
        m_globalHotkeyRegistered = true;
        qDebug() << "系统级F1热键注册成功";
    } else {
        qDebug() << "系统级F1热键注册失败，回退到应用级快捷键";
        // 回退到应用级快捷键
        m_screenshotShortcut = new QShortcut(QKeySequence(Qt::Key_F1), this);
        m_screenshotShortcut->setContext(Qt::ApplicationShortcut);
        connect(m_screenshotShortcut, &QShortcut::activated, this, &MainWindow::startScreenCapture);
    }
#else
    // 非Windows系统使用应用级快捷键
    m_screenshotShortcut = new QShortcut(QKeySequence(Qt::Key_F1), this);
    m_screenshotShortcut->setContext(Qt::ApplicationShortcut);
    connect(m_screenshotShortcut, &QShortcut::activated, this, &MainWindow::startScreenCapture);
#endif

    qDebug() << "截图功能已初始化，F1快捷键已设置";
}

void MainWindow::startScreenCapture() {
    qDebug() << "F1快捷键被触发，开始截图...";

#ifdef WITH_SCREEN_CAPTURE
    // 隐藏主窗口，避免影响截图
    this->hide();

    // TODO YANGXU 这个已经去掉了短暂延迟，确保窗口完全隐藏

    // 设置截图模式为显示工具栏
    App::setCustomCap(2); // 2 = 显示工具栏进行编辑

    qDebug() << "开始截图，将显示工具栏";

    // 配置截图参数
    ScreenCaptureAPI::CaptureConfig config;
    config.mode = ScreenCaptureAPI::CaptureMode::SelectArea; // 选择区域模式
    config.includeDecorations = false; // 不包含窗口装饰
    config.hideCursor = false; // 显示鼠标光标
    config.quality = 100; // 最高质量
    config.format = "PNG"; // PNG格式
    config.timeoutMs = 0; // 禁用超时，允许用户慢慢编辑

    // 开始截图
    if (!m_screenCapture->startCapture(config)) {
        qDebug() << "启动截图失败";
        // 如果截图失败，重新显示主窗口
        this->show();
    }
#else
    qDebug() << "截图功能在此平台不可用";
    showScreenshotTooltip("截图功能仅在Windows平台可用");
#endif
}

#ifdef WITH_SCREEN_CAPTURE
void MainWindow::onCaptureCompleted(ScreenCaptureAPI::CaptureResult result, const QImage& image) {
    qDebug() << "截图完成，结果：" << static_cast<int>(result);

    // 重新显示主窗口,截图完成不显示主窗口
    this->show();          // 确保窗口显示出来
    this->showMinimized();
    // this->raise();         // 把窗口放到最上层（防止被自己其他窗口挡住）
    // this->activateWindow();// 请求获得焦点，成为当前活动窗口

    switch (result) {
    case ScreenCaptureAPI::CaptureResult::Success:
        if (!image.isNull()) {
            qDebug() << "截图成功，图片大小：" << image.size();

            // 将图片复制到剪贴板
            QClipboard* clipboard = QApplication::clipboard();
            clipboard->setImage(image);

            // 可选：保存到默认位置
            QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
            if (defaultPath.isEmpty()) {
                defaultPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
            }

            const QString fileName = QString("screenshot_%1.png").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
            const QString fullPath = QDir(defaultPath).filePath(fileName);

            if (image.save(fullPath)) {
                qDebug() << "截图已保存到：" << fullPath;

                // 显示成功提示
                showScreenshotTooltip("截图成功！\n已保存到剪贴板和图片文件夹");
            } else {
                qDebug() << "保存截图失败：" << fullPath;
                showScreenshotTooltip("截图成功！\n已保存到剪贴板");
            }
        } else {
            qDebug() << "截图成功但图片为空";
            showScreenshotTooltip("截图失败：图片为空");
        }
        break;

    case ScreenCaptureAPI::CaptureResult::Cancelled:
        qDebug() << "用户取消了截图";
        // 用户取消时不显示任何提示
        break;

    case ScreenCaptureAPI::CaptureResult::Error:
        qDebug() << "截图过程中发生错误";
        showScreenshotTooltip("截图失败：发生错误");
        break;

    case ScreenCaptureAPI::CaptureResult::Timeout:
        qDebug() << "截图超时";
        showScreenshotTooltip("截图失败：操作超时");
        break;
    }
}
#endif

void MainWindow::showScreenshotTooltip(const QString& message) {
    // 创建截图提示框
    QWidget* tooltip = new QWidget(this, Qt::ToolTip | Qt::FramelessWindowHint);
    tooltip->setFixedSize(200, 60);
    tooltip->setStyleSheet(
        "QWidget {"
        "    background-color: #2c3e50;"
        "    color: white;"
        "    border-radius: 8px;"
        "    font-size: 12px;"
        "    font-weight: bold;"
        "}"
    );

    QVBoxLayout* tooltipLayout = new QVBoxLayout(tooltip);
    tooltipLayout->setContentsMargins(15, 10, 15, 10);

    QLabel* tooltipLabel = new QLabel(message);
    tooltipLabel->setAlignment(Qt::AlignCenter);
    tooltipLabel->setStyleSheet("background-color: transparent; border: none; color: white;");
    tooltipLabel->setWordWrap(true);
    tooltipLayout->addWidget(tooltipLabel);

    // 计算提示框位置（屏幕右下角）
    QScreen* screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();
    int x = screenGeometry.right() - tooltip->width() - 20;
    int y = screenGeometry.bottom() - tooltip->height() - 20;
    tooltip->move(x, y);

    // 显示提示框
    tooltip->show();
    tooltip->raise();

    // 3秒后自动隐藏并销毁
    QTimer::singleShot(3000, tooltip, &QWidget::deleteLater);
}

