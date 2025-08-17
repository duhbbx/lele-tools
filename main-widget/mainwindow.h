#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QTabWidget>
#include <QPushButton>
#include <QListWidgetItem>
#include <QSplitter>
#include <QCloseEvent>
#include <QMenuBar>
#include <QAction>
#include <QMouseEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QPropertyAnimation>
#include <QApplication>
#include <QClipboard>
#include "mainwindowtabwidget.h"
#include <QGraphicsDropShadowEffect>
#include <QResizeEvent>
#include <QTranslator>
#include <QLocale>
#include <QSettings>
#include <QDir>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <windows.h>
#include <windowsx.h>
#endif


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void itemClickedSlot(QListWidgetItem *item);
    void showAbout();
    void showHelp();
    void exitApplication();
    void changeLanguage(const QString &language);

protected:
    void closeEvent(QCloseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    
#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif

private:
    MainWindowTabWidget * rightTabWidget;          // 右侧标签页工作区
    // QSplitter * mainSplitter;             // 主分割器 - 已移除，改用水平布局
    QWidget * leftPanel;                  // 左侧面板
    QWidget * rightPanel;                 // 右侧面板
    
    // 自定义标题栏相关
    QWidget * titleBar;                   // 自定义标题栏
    QMenuBar * customMenuBar;             // 自定义菜单栏
    QPushButton * minimizeButton;         // 最小化按钮
    QPushButton * maximizeButton;         // 最大化/还原按钮
    QPushButton * closeButton;            // 关闭按钮
    
    // 状态栏相关
    QWidget * statusBar;                  // 自定义状态栏
    QLabel * timeLabel;                   // 时间显示标签
    QPushButton * leftPanelToggle;        // 左侧面板收起/展开按钮
    QTimer * timer;                       // 定时器
    
    // 左侧菜单收起功能
    bool isLeftPanelCollapsed;            // 左侧面板是否收起
    
    // 复制提示相关
    QWidget * copyTooltip;                // 复制成功提示框
    
    // 标签页下拉菜单相关
    QPushButton * tabDropdownButton;      // 标签页下拉按钮
    QMenu * tabDropdownMenu;              // 标签页下拉菜单
    
    // 窗口拖拽相关
    bool m_bPressed;                      // 鼠标是否按下
    QPoint m_point;                       // 鼠标按下时的位置
    
    // 窗口缩放相关
    bool m_bResizing;                     // 是否正在缩放
    QPoint m_resizePoint;                 // 缩放起始点
    QRect m_originalGeometry;             // 原始窗口几何
    int m_resizeDirection;                // 缩放方向
    
    // 国际化相关
    QTranslator *m_translator;            // 翻译器
    QSettings *m_settings;                // 设置
    
    void createTitleBar();                // 创建自定义标题栏
    void createMenuBar();                 // 创建菜单栏
    void setupWindowControls();           // 设置窗口控制按钮
    void createStatusBar();               // 创建状态栏
    // setupCollapseButton已移除，改用状态栏toggle
    void updateTime();                    // 更新时间显示
    void toggleLeftPanel();               // 切换左侧面板显示状态
    void setupTabWidget();                // 设置标签页组件
    void closeTab(int index);             // 关闭标签页
    bool isDraggableArea(const QPoint &pos); // 判断是否为可拖拽区域
    int getResizeDirection(const QPoint &pos); // 获取缩放方向
    bool isResizeArea(const QPoint &pos); // 判断是否为缩放区域
    void copyTimeToClipboard();           // 复制时间到剪贴板
    void showCopyTooltip();               // 显示复制成功提示
    void setupTabDropdown();              // 设置标签页下拉菜单
    void updateTabDropdownMenu();         // 更新标签页下拉菜单
    void switchToTab(int index);          // 切换到指定标签页
    void loadLanguage(const QString &language); // 加载语言

public:
    void addTool(QWidget* w);
};
#endif // MAINWINDOW_H
