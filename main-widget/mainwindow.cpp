#include "mainwindow.h"
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


MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), m_bPressed(false), isLeftPanelCollapsed(false), copyTooltip(nullptr) {
    this->setWindowTitle("乐乐的工具箱");
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
    QString globalStyle = 
        "QMainWindow { background-color: #ffffff; border-radius: 8px; }"
        
        // 美化所有文本编辑框
        "QTextEdit {"
        "    border: 2px solid #dee2e6;"
        "    border-radius: 6px;"
        "    padding: 8px;"
        "    background-color: #ffffff;"
        "    color: #495057;"
        "    font-size: 13px;"
        "    selection-background-color: #007bff;"
        "    selection-color: white;"
        "}"
        "QTextEdit:focus {"
        "    border-color: #007bff;"
        "    background-color: #ffffff;"
        "}"
        "QTextEdit:hover {"
        "    border-color: #6c757d;"
        "}"
        
        // 美化所有纯文本编辑框
        "QPlainTextEdit {"
        "    border: 2px solid #dee2e6;"
        "    border-radius: 6px;"
        "    padding: 8px;"
        "    background-color: #ffffff;"
        "    color: #495057;"
        "    font-size: 13px;"
        "    selection-background-color: #007bff;"
        "    selection-color: white;"
        "}"
        "QPlainTextEdit:focus {"
        "    border-color: #007bff;"
        "    background-color: #ffffff;"
        "}"
        "QPlainTextEdit:hover {"
        "    border-color: #6c757d;"
        "}"
        
        // 美化所有单行输入框
        "QLineEdit {"
        "    border: 2px solid #dee2e6;"
        "    border-radius: 6px;"
        "    padding: 8px 12px;"
        "    background-color: #ffffff;"
        "    color: #495057;"
        "    font-size: 13px;"
        "    selection-background-color: #007bff;"
        "    selection-color: white;"
        "}"
        "QLineEdit:focus {"
        "    border-color: #007bff;"
        "    background-color: #ffffff;"
        "}"
        "QLineEdit:hover {"
        "    border-color: #6c757d;"
        "}"
        
        // 美化组合框
        "QComboBox {"
        "    border: 2px solid #dee2e6;"
        "    border-radius: 6px;"
        "    padding: 8px 12px;"
        "    background-color: #ffffff;"
        "    color: #495057;"
        "    font-size: 13px;"
        "}"
        "QComboBox:focus {"
        "    border-color: #007bff;"
        "}"
        "QComboBox:hover {"
        "    border-color: #6c757d;"
        "}"
        "QComboBox::drop-down {"
        "    border: none;"
        "    width: 30px;"
        "}"
        "QComboBox::down-arrow {"
        "    image: none;"
        "    border: none;"
        "    width: 12px;"
        "    height: 12px;"
        "}"
        
        // 美化数字输入框
        "QSpinBox, QDoubleSpinBox {"
        "    border: 2px solid #dee2e6;"
        "    border-radius: 6px;"
        "    padding: 8px 12px;"
        "    background-color: #ffffff;"
        "    color: #495057;"
        "    font-size: 13px;"
        "}"
        "QSpinBox:focus, QDoubleSpinBox:focus {"
        "    border-color: #007bff;"
        "}"
        "QSpinBox:hover, QDoubleSpinBox:hover {"
        "    border-color: #6c757d;"
        "}";
        
    this->setStyleSheet(globalStyle);

    // 创建主容器
    QWidget *mainWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(mainWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // 创建自定义标题栏
    createTitleBar();
    mainLayout->addWidget(titleBar);

    // 创建主分割器
    mainSplitter = new QSplitter(Qt::Horizontal, this);
    
    // 创建左侧面板（功能菜单）
    leftPanel = new QWidget();
    leftPanel->setFixedWidth(300);  // 固定左侧宽度
    leftPanel->setStyleSheet("background-color: #f5f5f5; border-right: 1px solid #ddd;");
    
    // 创建右侧面板（工作区）
    rightPanel = new QWidget();
    
    // 创建右侧标签页窗口
    rightTabWidget = new QTabWidget();
    rightTabWidget->setContentsMargins(10, 10, 10, 10);
    rightTabWidget->setTabsClosable(true); // 启用关闭按钮
    rightTabWidget->setMovable(true);      // 允许拖拽标签页
    
    // 创建欢迎页面
    QWidget *welcomePage = new QWidget();
    QVBoxLayout *welcomeLayout = new QVBoxLayout(welcomePage);
    QLabel *welcomeLabel = new QLabel("欢迎使用乐乐的工具箱\n\n请从左侧菜单选择要使用的工具");
    welcomeLabel->setAlignment(Qt::AlignCenter);
    welcomeLabel->setStyleSheet("font-size: 18px; color: #666; padding: 50px;");
    welcomeLayout->addWidget(welcomeLabel);
    
    rightTabWidget->addTab(welcomePage, "首页");
    
    // 设置标签页样式和功能
    setupTabWidget();
    
    // 设置标签页下拉菜单
    setupTabDropdown();
    
    // 设置右侧面板布局
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->addWidget(rightTabWidget);
    
    // 创建工具列表并添加到左侧面板
    ToolList *toolList = new ToolList(this, nullptr);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(5, 5, 5, 5);
    leftLayout->addWidget(toolList);
    
    // 将左右面板添加到分割器
    mainSplitter->addWidget(leftPanel);
    mainSplitter->addWidget(rightPanel);
    
    // 设置分割器比例，左侧固定，右侧自适应
    mainSplitter->setStretchFactor(0, 0);  // 左侧不拉伸
    mainSplitter->setStretchFactor(1, 1);  // 右侧可拉伸
    
    // 将分割器添加到主布局
    mainLayout->addWidget(mainSplitter);
    
    // 创建状态栏
    createStatusBar();
    mainLayout->addWidget(statusBar);
    
    // 设置主容器为中央部件
    this->setCentralWidget(mainWidget);
    
    // 创建菜单栏内容
    createMenuBar();
    
    // 设置收起按钮
    setupCollapseButton();
    
    // 添加窗口阴影效果
    QGraphicsDropShadowEffect *shadowEffect = new QGraphicsDropShadowEffect(this);
    shadowEffect->setOffset(0, 2);
    shadowEffect->setBlurRadius(20);
    shadowEffect->setColor(QColor(0, 0, 0, 100));
    this->setGraphicsEffect(shadowEffect);
    
    qDebug() << "MainWindow初始化完成，使用左右分栏布局";
}

MainWindow::~MainWindow()
{

}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QMessageBox::StandardButton resBtn = QMessageBox::question(this, "退出确认",
                                                                tr("确定要退出乐乐的工具箱吗？"),
                                                                QMessageBox::Cancel | QMessageBox::Yes,
                                                                QMessageBox::Yes);
    if (resBtn != QMessageBox::Yes) {
        event->ignore();
    } else {
        event->accept();
    }
}

void MainWindow::itemClickedSlot(QListWidgetItem *item) {
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

    QWidget *widget = dynamic_cast<QWidget *>(object);
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

void MainWindow::createTitleBar()
{
    titleBar = new QWidget();
    titleBar->setFixedHeight(35);
    titleBar->setStyleSheet("background-color: #f8f9fa; border-bottom: 1px solid #dee2e6;");
    
    QHBoxLayout *titleLayout = new QHBoxLayout(titleBar);
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
        "}"
        "QMenuBar::item {"
        "    background-color: transparent;"
        "    color: #343a40;"
        "    padding: 8px 12px;"
        "    margin: 0px;"
        "    border-radius: 4px;"
        "}"
        "QMenuBar::item:selected {"
        "    background-color: #e9ecef;"
        "}"
        "QMenuBar::item:pressed {"
        "    background-color: #dee2e6;"
        "}"
    );
    
    // 在这里我们稍后会填充菜单
    
    titleLayout->addWidget(customMenuBar);
    titleLayout->addStretch(); // 占用剩余空间
    
    // 创建窗口控制按钮
    setupWindowControls();
    
    titleLayout->addWidget(minimizeButton);
    titleLayout->addWidget(maximizeButton);
    titleLayout->addWidget(closeButton);
}

void MainWindow::createMenuBar()
{
    // 文件菜单
    QMenu *fileMenu = customMenuBar->addMenu(tr("文件(&F)"));
    fileMenu->setStyleSheet(
        "QMenu {"
        "    background-color: #ffffff;"
        "    color: #343a40;"
        "    border: 1px solid #dee2e6;"
        "    border-radius: 6px;"
        "    padding: 4px;"
        "}"
        "QMenu::item {"
        "    background-color: transparent;"
        "    color: #343a40;"
        "    padding: 8px 16px;"
        "    border-radius: 4px;"
        "    margin: 1px;"
        "}"
        "QMenu::item:selected {"
        "    background-color: #e9ecef;"
        "}"
        "QMenu::separator {"
        "    height: 1px;"
        "    background-color: #dee2e6;"
        "    margin: 4px 8px;"
        "}"
    );
    
    QAction *exitAction = new QAction(tr("退出(&X)"), this);
    exitAction->setShortcut(QKeySequence::Quit);
    exitAction->setStatusTip(tr("退出应用程序"));
    // 设置退出图标 (Unicode字符)
    exitAction->setIcon(this->style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    connect(exitAction, &QAction::triggered, this, &MainWindow::exitApplication);
    fileMenu->addAction(exitAction);
    
    // 工具菜单
    QMenu *toolsMenu = customMenuBar->addMenu(tr("工具(&T)"));
    toolsMenu->setStyleSheet(fileMenu->styleSheet()); // 使用相同样式
    
    QAction *backToHomeAction = new QAction(tr("返回首页(&H)"), this);
    backToHomeAction->setShortcut(QKeySequence(tr("Ctrl+H")));
    backToHomeAction->setStatusTip(tr("返回到工具选择页面"));
    backToHomeAction->setIcon(this->style()->standardIcon(QStyle::SP_DirHomeIcon));
    connect(backToHomeAction, &QAction::triggered, [this]() {
        rightTabWidget->setCurrentIndex(0);
        this->setWindowTitle("乐乐的工具箱");
    });
    toolsMenu->addAction(backToHomeAction);
    
    // 帮助菜单
    QMenu *helpMenu = customMenuBar->addMenu(tr("帮助(&H)"));
    helpMenu->setStyleSheet(fileMenu->styleSheet()); // 使用相同样式
    
    QAction *helpAction = new QAction(tr("使用说明(&U)"), this);
    helpAction->setShortcut(QKeySequence::HelpContents);
    helpAction->setStatusTip(tr("查看使用说明"));
    helpAction->setIcon(this->style()->standardIcon(QStyle::SP_DialogHelpButton));
    connect(helpAction, &QAction::triggered, this, &MainWindow::showHelp);
    helpMenu->addAction(helpAction);
    
    helpMenu->addSeparator();
    
    QAction *aboutAction = new QAction(tr("关于(&A)"), this);
    aboutAction->setStatusTip(tr("关于乐乐的工具箱"));
    aboutAction->setIcon(this->style()->standardIcon(QStyle::SP_MessageBoxInformation));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
    helpMenu->addAction(aboutAction);
}

void MainWindow::setupWindowControls()
{
    QString buttonStyle = 
        "QPushButton {"
        "    background-color: transparent;"
        "    border: none;"
        "    color: #343a40;"
        "    font-size: 16px;"
        "    width: 30px;"
        "    height: 30px;"
        "    margin: 2px;"
        "    border-radius: 4px;"
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
        "    font-size: 16px;"
        "    width: 30px;"
        "    height: 30px;"
        "    margin: 2px;"
        "    border-radius: 4px;"
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

void MainWindow::createStatusBar()
{
    statusBar = new QWidget();
    statusBar->setFixedHeight(25);
    statusBar->setStyleSheet("background-color: #f0f0f0; border-top: 1px solid #ddd;");
    
    QHBoxLayout *statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(10, 2, 10, 2);
    
    // 添加时间标签
    timeLabel = new QLabel();
    timeLabel->setStyleSheet(
        "QLabel {"
        "    color: #6c757d;"
        "    font-size: 12px;"
        "    padding: 2px 8px;"
        "    border-radius: 3px;"
        "}"
        "QLabel:hover {"
        "    background-color: #e9ecef;"
        "    cursor: pointer;"
        "}"
    );
    timeLabel->setToolTip("点击复制时间");
    updateTime(); // 初始化时间显示
    
    // 使时间标签可点击
    timeLabel->setAttribute(Qt::WA_Hover, true);
    timeLabel->installEventFilter(this);
    
    statusLayout->addStretch(); // 推到右侧
    statusLayout->addWidget(timeLabel);
    
    // 创建定时器，每秒更新一次时间
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateTime);
    timer->start(1000); // 1秒间隔
}

void MainWindow::updateTime()
{
    QDateTime currentTime = QDateTime::currentDateTime();
    timeLabel->setText(currentTime.toString("yyyy-MM-dd hh:mm:ss"));
}

void MainWindow::setupCollapseButton()
{
    collapseButton = new QPushButton();
    collapseButton->setFixedSize(8, 100); // 更小的宽度
    
    // 创建一个竖线样式的按钮
    collapseButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #e9ecef;"
        "    border: none;"
        "    color: #6c757d;"
        "    font-size: 8px;"
        "    border-left: 1px solid #dee2e6;"
        "    border-right: 1px solid #dee2e6;"
        "    border-radius: 0px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #007bff;"
        "    color: white;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #0056b3;"
        "}"
    );
    
    // 设置按钮文本为小点
    collapseButton->setText("⋯");
    collapseButton->setToolTip("收起/展开左侧菜单");
    
    // 将按钮添加到右侧面板的布局中
    QVBoxLayout *rightLayout = qobject_cast<QVBoxLayout*>(rightPanel->layout());
    if (rightLayout) {
        // 创建一个包含按钮的容器
        QWidget *buttonContainer = new QWidget();
        buttonContainer->setFixedWidth(8);
        
        QVBoxLayout *buttonLayout = new QVBoxLayout(buttonContainer);
        buttonLayout->setContentsMargins(0, 0, 0, 0);
        buttonLayout->addStretch();
        buttonLayout->addWidget(collapseButton);
        buttonLayout->addStretch();
        
        // 将按钮容器添加到分割器的右侧
        mainSplitter->insertWidget(1, buttonContainer);
        mainSplitter->insertWidget(2, rightPanel);
    }
    
    connect(collapseButton, &QPushButton::clicked, this, &MainWindow::toggleLeftPanel);
}

void MainWindow::toggleLeftPanel()
{
    if (isLeftPanelCollapsed) {
        // 展开左侧面板
        leftPanel->setVisible(true);
        leftPanel->setFixedWidth(300);
        collapseButton->setText("⋯");
        collapseButton->setToolTip("收起左侧菜单");
        isLeftPanelCollapsed = false;
    } else {
        // 收起左侧面板
        leftPanel->setVisible(false);
        leftPanel->setFixedWidth(0);
        collapseButton->setText("⋮");
        collapseButton->setToolTip("展开左侧菜单");
        isLeftPanelCollapsed = true;
    }
}

void MainWindow::setupTabWidget()
{
    // 设置标签页样式
    rightTabWidget->setStyleSheet(
        "QTabWidget::pane {"
        "    border: 1px solid #dee2e6;"
        "    background-color: #ffffff;"
        "}"
        "QTabBar::tab {"
        "    background-color: #f8f9fa;"
        "    color: #495057;"
        "    padding: 8px 16px;"
        "    margin-right: 2px;"
        "    border: 1px solid #dee2e6;"
        "    border-bottom: none;"
        "    min-width: 80px;"
        "}"
        "QTabBar::tab:selected {"
        "    background-color: #ffffff;"
        "    color: #212529;"
        "    border-bottom: 1px solid #ffffff;"
        "}"
        "QTabBar::close-button {"
        "    subcontrol-position: right;"
        "    border: none;"
        "    width: 16px;"
        "    height: 16px;"
        "    margin: 1px;"
        "    background-color: transparent;"
        "    border-radius: 8px;"
        "}"
        "QTabBar::close-button:hover {"
        "    background-color: #dc3545;"
        "}"
        "QTabBar::close-button:pressed {"
        "    background-color: #c82333;"
        "}"
    );
    

    
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

void MainWindow::closeTab(int index)
{
    // 防止关闭首页
    if (index == 0) {
        return;
    }

    QWidget *widget = rightTabWidget->widget(index);
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

void MainWindow::showAbout()
{
    QMessageBox::about(this, tr("关于乐乐的工具箱"),
                       tr("<h2>乐乐的工具箱</h2>"
                          "<p>版本 1.0</p>"
                          "<p>一个集成了多种实用工具的桌面应用程序。</p>"
                          "<p>包含了编码解码、格式转换、文本处理、图像处理等多种常用工具。</p>"
                          "<p>© 2024 乐乐工作室</p>"));
}

void MainWindow::showHelp()
{
    QMessageBox::information(this, tr("使用说明"),
                             tr("<h3>使用说明</h3>"
                                "<p><b>基本操作：</b></p>"
                                "<ul>"
                                "<li>从左侧菜单选择需要使用的工具</li>"
                                "<li>工具将在右侧工作区打开</li>"
                                "<li>可以通过菜单栏快速返回首页</li>"
                                "</ul>"
                                "<p><b>快捷键：</b></p>"
                                "<ul>"
                                "<li>Ctrl+H - 返回首页</li>"
                                "<li>Ctrl+Q - 退出程序</li>"
                                "<li>F1 - 查看帮助</li>"
                                "</ul>"));
}

void MainWindow::exitApplication()
{
    close();
}

// 鼠标事件处理 - 实现窗口拖拽
void MainWindow::mousePressEvent(QMouseEvent *event)
{
    // 由于使用了Windows原生缩放，只需要处理拖拽逻辑
    if (event->button() == Qt::LeftButton && isDraggableArea(event->pos())) {
        m_bPressed = true;
        m_point = event->pos();
    }
    QMainWindow::mousePressEvent(event);
}

bool MainWindow::isDraggableArea(const QPoint &pos)
{
    // 检查是否在标题栏区域
    if (titleBar && titleBar->geometry().contains(pos)) {
        // 计算标题栏内的相对位置
        QPoint titleBarPos = pos - titleBar->pos();
        
        // 精确计算菜单栏区域（只包括菜单项，不包括右侧空白）
        QRect menuBarRect = customMenuBar->geometry();
        
        // 菜单栏的实际宽度（只包含菜单项）
        int menuWidth = 0;
        for (QAction *action : customMenuBar->actions()) {
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

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    // 移除自定义缩放逻辑，使用Windows原生缩放
    if (m_bPressed && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPos() - m_point);
        setCursor(Qt::ClosedHandCursor); // 拖拽时的抓取光标
    } else {
        // Windows原生缩放会自动处理光标，只需要设置拖拽区域的光标
        if (isDraggableArea(event->pos())) {
            setCursor(Qt::OpenHandCursor);
        } else {
            setCursor(Qt::ArrowCursor);
        }
    }
    QMainWindow::mouseMoveEvent(event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_bPressed = false;
        
        // 重置光标
        if (isDraggableArea(event->pos())) {
            setCursor(Qt::OpenHandCursor);
        } else {
            setCursor(Qt::ArrowCursor);
        }
    }
    QMainWindow::mouseReleaseEvent(event);
}

void MainWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
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

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    
    // 确保窗口内容在缩放时正确重绘
    this->update();
    
    // 更新所有子组件
    if (titleBar) titleBar->update();
    if (statusBar) statusBar->update();
    if (rightTabWidget) rightTabWidget->update();
    if (leftPanel) leftPanel->update();
}

void MainWindow::copyTimeToClipboard()
{
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(timeLabel->text());
    
    // 显示复制成功提示
    showCopyTooltip();
}

void MainWindow::showCopyTooltip()
{
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
        "    border-radius: 8px;"
        "    font-size: 12px;"
        "    font-weight: bold;"
        "}"
    );
    
    QHBoxLayout *tooltipLayout = new QHBoxLayout(copyTooltip);
    tooltipLayout->setContentsMargins(10, 5, 10, 5);
    
    QLabel *tooltipLabel = new QLabel("复制成功！");
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

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == timeLabel && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            copyTimeToClipboard();
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

#ifdef Q_OS_WIN
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    MSG *msg = static_cast<MSG *>(message);
    
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

void MainWindow::setupTabDropdown()
{
    // 创建下拉按钮
    tabDropdownButton = new QPushButton("⋮");
    tabDropdownButton->setFixedSize(25, 25);
    tabDropdownButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #f8f9fa;"
        "    border: 1px solid #dee2e6;"
        "    border-radius: 4px;"
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
        "    border-radius: 6px;"
        "    padding: 4px;"
        "}"
        "QMenu::item {"
        "    background-color: transparent;"
        "    color: #343a40;"
        "    padding: 6px 12px;"
        "    border-radius: 4px;"
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

void MainWindow::updateTabDropdownMenu()
{
    tabDropdownMenu->clear();
    
    for (int i = 0; i < rightTabWidget->count(); ++i) {
        QString tabText = rightTabWidget->tabText(i);
        QAction *action = tabDropdownMenu->addAction(tabText);
        
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

void MainWindow::switchToTab(int index)
{
    if (index >= 0 && index < rightTabWidget->count()) {
        rightTabWidget->setCurrentIndex(index);
    }
}
