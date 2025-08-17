# 自定义 TabBar 使用说明

## 概述

项目中实现了自定义的 TabBar，支持在鼠标悬停到标签页时才显示关闭按钮的功能。

## 实现方案

由于 QSS 的选择器限制，无法通过纯样式表实现 "悬停标签页显示关闭按钮" 的效果，因此采用了子类化 QTabBar 的方案。

## 文件结构

### 主窗口专用
- `main-widget/customtabbar.h` - 主窗口专用的自定义 TabBar
- `main-widget/customtabbar.cpp` - TabBar 实现文件
- `main-widget/mainwindowtabwidget.h` - 主窗口专用的自定义 TabWidget
- `main-widget/mainwindowtabwidget.cpp` - TabWidget 实现文件

### 通用组件
- `common/customtabwidget.h` - 通用的自定义 TabWidget
- `common/customtabwidget.cpp` - 实现文件

## 使用方法

### 方法1: 使用 CustomTabWidget (推荐)

```cpp
#include "../../common/customtabwidget.h"

// 在类声明中
CustomTabWidget *tabWidget;

// 在构造函数中
tabWidget = new CustomTabWidget();
tabWidget->addTab(widget1, "标签页1");
tabWidget->addTab(widget2, "标签页2");
```

### 方法2: 使用主窗口专用 TabWidget

```cpp
#include "main-widget/mainwindowtabwidget.h"

// 在类声明中
MainWindowTabWidget *tabWidget;

// 在构造函数中
tabWidget = new MainWindowTabWidget();
tabWidget->addTab(widget1, "标签页1");
tabWidget->addTab(widget2, "标签页2");
```

### 注意事项

由于 `QTabWidget::setTabBar()` 是 protected 方法，不能直接从外部调用，因此：
- 主窗口使用 `MainWindowTabWidget` 类
- 其他模块使用 `CustomTabWidget` 类
- 两者都已经在构造函数中正确设置了自定义的 TabBar

## 功能特点

1. **悬停显示**: 只有当鼠标悬停在标签页上时才显示关闭按钮
2. **自动隐藏**: 鼠标移开后关闭按钮自动隐藏
3. **视觉反馈**: 悬停和点击关闭按钮时有视觉反馈
4. **SVG图标**: 使用红色的 SVG 关闭图标

## 技术实现

### 核心机制
- 重写 `mouseMoveEvent` 跟踪鼠标位置
- 重写 `enterEvent` 和 `leaveEvent` 处理进入/离开事件
- 使用 `QTimer` 延迟更新，避免频繁刷新
- 通过 `findChildren<QToolButton*>()` 查找关闭按钮
- 动态设置/清除按钮图标

### 关键代码
```cpp
// 悬停检测
void CustomTabBar::mouseMoveEvent(QMouseEvent *event)
{
    int newHoveredTab = tabAt(event->pos());
    if (newHoveredTab != m_hoveredTab) {
        m_hoveredTab = newHoveredTab;
        m_updateTimer->start();
    }
}

// 更新按钮可见性
void CustomTabBar::updateCloseButtons()
{
    hideAllCloseButtons();
    
    if (m_hoveredTab >= 0 && m_hoveredTab < count()) {
        showCloseButtonForTab(m_hoveredTab);
    }
}
```

## 样式配置

关闭按钮的样式在 QSS 中配置：

```css
QTabBar::close-button {
    subcontrol-position: right;
    border: none;
    width: 16px;
    height: 16px;
    margin: 1px;
    background-color: transparent;
    border-radius: 8px;
    image: none;  /* 默认无图标 */
}

QTabBar::close-button:hover {
    background-color: rgba(220, 53, 69, 0.1);
    border: 1px solid #dc3545;
}

QTabBar::close-button:pressed {
    background-color: rgba(200, 35, 51, 0.2);
    border: 1px solid #c82333;
}
```

## 已应用的模块

- ✅ 主窗口 (`main-widget/mainwindow.cpp`)
- ✅ 二维码生成器 (`modules/qr-code-gen/qrcodegen.cpp`)

## 扩展到其他模块

要为其他模块添加此功能，只需要：

1. 包含头文件: `#include "../../common/customtabwidget.h"`
2. 将 `QTabWidget` 替换为 `CustomTabWidget`
3. 移除原有的关闭按钮 QSS 样式

## 注意事项

1. 确保 `:/resources/close.svg` 资源文件存在
2. 自定义 TabBar 会自动启用 `setTabsClosable(true)`
3. 如果需要禁用某些标签页的关闭功能，需要额外处理
