# ScreenCapture CMake子项目集成指南

## 📋 概述

ScreenCapture项目已经配置为可以作为CMake子项目被其他项目直接包含。这种方式提供了源码级别的集成，用户可以直接将ScreenCapture作为子目录添加到他们的项目中。

## 🎯 优势

- **✅ 源码级集成** - 完全控制编译选项和依赖
- **🔄 版本同步** - 与主项目使用相同的编译器和Qt版本
- **🛠️ 自定义构建** - 可以修改源码满足特定需求
- **📦 单一构建** - 主项目和ScreenCapture一起编译
- **🎯 简化部署** - 不需要单独分发库文件

## 🚀 快速开始

### 1. 添加子项目

有几种方式将ScreenCapture添加到您的项目：

#### 方式1：Git子模块（推荐）
```bash
# 在你的项目根目录
git submodule add https://github.com/your-repo/ScreenCapture.git third-party/ScreenCapture
git submodule update --init --recursive
```

#### 方式2：直接复制
```bash
# 将ScreenCapture文件夹复制到你的项目中
cp -r /path/to/ScreenCapture ./third-party/ScreenCapture
```

#### 方式3：Git Subtree
```bash
git subtree add --prefix=third-party/ScreenCapture https://github.com/your-repo/ScreenCapture.git main --squash
```

### 2. 修改主项目CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.16)
project(MyApplication VERSION 1.0.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Concurrent)

# 启用Qt自动化工具
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 添加ScreenCapture子项目
add_subdirectory(third-party/ScreenCapture)

# 创建你的应用程序
add_executable(MyApplication
    main.cpp
    MainWindow.cpp
    MainWindow.h
    # ... 其他源文件
)

# 链接ScreenCapture库
target_link_libraries(MyApplication 
    PRIVATE 
    ScreenCapture::ScreenCapture  # 使用别名
    Qt6::Core
    Qt6::Widgets
)
```

### 3. 在代码中使用

```cpp
// main.cpp
#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    MainWindow window;
    window.show();
    
    return app.exec();
}
```

```cpp
// MainWindow.h
#pragma once

#include <QMainWindow>
#include <QShortcut>
#include "ScreenCaptureAPI.h"  // 包含ScreenCapture API

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onCaptureCompleted(ScreenCaptureAPI::CaptureResult result, const QImage& image);
    void onF1Pressed();

private:
    ScreenCaptureAPI* m_captureAPI;
    QShortcut* m_f1Shortcut;
};
```

```cpp
// MainWindow.cpp
#include "MainWindow.h"
#include <QKeySequence>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_captureAPI(nullptr)
    , m_f1Shortcut(nullptr)
{
    // 创建ScreenCapture API实例
    m_captureAPI = new ScreenCaptureAPI(this);
    
    // 连接信号
    connect(m_captureAPI, &ScreenCaptureAPI::captureCompleted,
            this, &MainWindow::onCaptureCompleted);
    
    // 设置F1快捷键
    m_f1Shortcut = new QShortcut(QKeySequence(Qt::Key_F1), this);
    connect(m_f1Shortcut, &QShortcut::activated,
            this, &MainWindow::onF1Pressed);
    
    // 设置UI
    auto* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    auto* layout = new QVBoxLayout(centralWidget);
    
    auto* label = new QLabel("按F1键进行截图", this);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);
    
    auto* button = new QPushButton("点击截图", this);
    connect(button, &QPushButton::clicked, [this]() {
        m_captureAPI->quickCapture();
    });
    layout->addWidget(button);
    
    setWindowTitle("我的应用程序 - 集成ScreenCapture");
    resize(400, 300);
}

MainWindow::~MainWindow()
{
    // ScreenCaptureAPI会被Qt的父子关系自动清理
}

void MainWindow::onCaptureCompleted(ScreenCaptureAPI::CaptureResult result, const QImage& image)
{
    switch (result) {
    case ScreenCaptureAPI::CaptureResult::Success:
        QMessageBox::information(this, "成功", 
            QString("截图成功！尺寸：%1x%2").arg(image.width()).arg(image.height()));
        break;
        
    case ScreenCaptureAPI::CaptureResult::Cancelled:
        QMessageBox::information(this, "取消", "截图被取消");
        break;
        
    case ScreenCaptureAPI::CaptureResult::Error:
        QMessageBox::warning(this, "错误", "截图发生错误");
        break;
        
    case ScreenCaptureAPI::CaptureResult::Timeout:
        QMessageBox::warning(this, "超时", "截图操作超时");
        break;
    }
}

void MainWindow::onF1Pressed()
{
    // F1键被按下，开始截图
    m_captureAPI->quickCapture();
}
```

## 📁 项目结构示例

```
MyApplication/
├── CMakeLists.txt              # 主项目配置
├── main.cpp                    # 程序入口
├── MainWindow.cpp              # 主窗口实现
├── MainWindow.h                # 主窗口头文件
├── third-party/                # 第三方库目录
│   └── ScreenCapture/          # ScreenCapture子项目
│       ├── CMakeLists.txt      # 子项目配置
│       ├── ScreenCaptureAPI.h  # 公共API头文件
│       ├── ScreenCaptureAPI.cpp# API实现
│       ├── App/                # 应用程序模块
│       ├── Win/                # 窗口模块
│       ├── Tool/               # 工具模块
│       ├── Shape/              # 形状模块
│       └── res.qrc             # 资源文件
└── build/                      # 构建目录
```

## 🔧 高级配置

### 1. 自定义编译选项

```cmake
# 在add_subdirectory之前设置选项
set(SCREENCAPTURE_BUILD_EXAMPLES OFF)  # 不构建示例
set(SCREENCAPTURE_ENABLE_LOGGING ON)   # 启用日志

add_subdirectory(third-party/ScreenCapture)
```

### 2. 条件包含

```cmake
option(ENABLE_SCREENSHOT "Enable screenshot functionality" ON)

if(ENABLE_SCREENSHOT)
    add_subdirectory(third-party/ScreenCapture)
    target_compile_definitions(MyApplication PRIVATE SCREENSHOT_ENABLED)
endif()
```

### 3. 版本检查

```cmake
# 检查ScreenCapture版本
add_subdirectory(third-party/ScreenCapture)

if(ScreenCapture_VERSION VERSION_LESS "1.0.0")
    message(FATAL_ERROR "需要ScreenCapture版本1.0.0或更高")
endif()
```

## 🛠️ 构建和运行

### 构建项目
```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### 运行程序
```bash
# Windows
./Release/MyApplication.exe

# Linux/Mac  
./MyApplication
```

## 📊 CMake变量说明

当ScreenCapture作为子项目时，会设置以下变量：

| 变量名 | 说明 | 默认值 |
|--------|------|--------|
| `SCREENCAPTURE_IS_MAIN_PROJECT` | 是否为主项目 | `FALSE` |
| `ScreenCapture_VERSION` | ScreenCapture版本 | `1.0.0` |
| `ScreenCapture_VERSION_MAJOR` | 主版本号 | `1` |

可用的目标：

| 目标名 | 说明 | 类型 |
|--------|------|------|
| `ScreenCaptureLib` | 实际库目标 | 静态库 |
| `ScreenCapture::ScreenCapture` | 别名目标（推荐使用） | 别名 |

## 🔍 故障排除

### Q: 编译时找不到Qt6？
A: 确保在主项目的CMakeLists.txt中正确设置了Qt6路径：
```cmake
set(CMAKE_PREFIX_PATH "C:/Qt/6.5.0/msvc2022_64")
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Concurrent)
```

### Q: 链接错误？
A: 确保使用正确的目标名称：
```cmake
target_link_libraries(MyApplication PRIVATE ScreenCapture::ScreenCapture)
```

### Q: 头文件找不到？
A: ScreenCapture的头文件会自动添加到包含路径，直接包含即可：
```cpp
#include "ScreenCaptureAPI.h"
```

### Q: 资源文件问题？
A: 资源文件会自动编译到库中，无需额外处理。

### Q: 运行时错误？
A: 确保Qt的DLL文件在PATH中，或者使用Qt的部署工具：
```bash
windeployqt MyApplication.exe
```

## 📝 最佳实践

1. **版本管理**：使用Git子模块时，固定到特定的commit或tag
2. **依赖隔离**：将第三方库放在独立的目录中
3. **编译选项**：在主项目中统一设置编译选项
4. **文档更新**：在项目文档中说明ScreenCapture的使用方法
5. **测试覆盖**：确保截图功能在CI/CD中得到测试

## 🎉 完成！

现在您已经成功将ScreenCapture作为子项目集成到您的应用程序中！用户可以通过F1键或程序调用来使用强大的截图功能，而且完全不会干扰主程序的运行。
