# OpenCV 集成指南

本文档说明如何在Lele Tools项目中集成和使用OpenCV计算机视觉库。

## 📋 OpenCV 版本支持

- **推荐版本**: OpenCV 4.10.0
- **最低版本**: OpenCV 4.5.0
- **平台支持**: Windows x64 (MSVC 2019/2022)

## 🚀 安装 OpenCV

### 方法一：预编译二进制文件（推荐）

1. **下载OpenCV**
   - 访问 [OpenCV Releases](https://github.com/opencv/opencv/releases)
   - 下载 `opencv-4.10.0-windows.exe`

2. **安装OpenCV**
   ```bash
   # 运行安装程序，解压到 C:\opencv
   opencv-4.10.0-windows.exe
   ```

3. **设置环境变量**
   ```bash
   # 添加到系统PATH（可选，用于运行时）
   C:\opencv\build\x64\vc16\bin
   
   # 设置OpenCV目录（用于CMake查找）
   OPENCV_DIR=C:\opencv\build
   ```

### 方法二：vcpkg 包管理器

```bash
# 安装vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat

# 安装OpenCV
.\vcpkg install opencv4[contrib]:x64-windows

# 集成到CMake
.\vcpkg integrate install
```

### 方法三：从源码编译

```bash
# 克隆源码
git clone https://github.com/opencv/opencv.git
git clone https://github.com/opencv/opencv_contrib.git

# 使用CMake配置和编译
mkdir build && cd build
cmake -G "Visual Studio 17 2022" -A x64 \
      -DCMAKE_BUILD_TYPE=Release \
      -DOPENCV_EXTRA_MODULES_PATH=../opencv_contrib/modules \
      -DBUILD_EXAMPLES=OFF \
      -DBUILD_TESTS=OFF \
      ../opencv

cmake --build . --config Release --parallel
cmake --install . --prefix C:/opencv/build
```

## 🔧 项目集成

### 1. CMake 配置

在你的模块的 `CMakeLists.txt` 中添加：

```cmake
# 查找OpenCV（可选）
find_package(OpenCV QUIET)

if(OpenCV_FOUND)
    message(STATUS "OpenCV found: ${OpenCV_VERSION}")
    
    # 添加支持OpenCV的源文件
    target_sources(your_module PRIVATE
        opencv_image_processor.cpp
        opencv_utils.cpp
    )
    
    # 链接OpenCV库
    target_link_libraries(your_module PRIVATE ${OpenCV_LIBS})
    
    # 添加头文件路径
    target_include_directories(your_module PRIVATE ${OpenCV_INCLUDE_DIRS})
    
    # 定义编译宏
    target_compile_definitions(your_module PRIVATE WITH_OPENCV)
    
else()
    message(WARNING "OpenCV not found for module: your_module")
endif()
```

### 2. 代码示例

创建一个支持OpenCV的图像处理模块：

```cpp
// opencv_image_processor.h
#pragma once

#include <QString>
#include <QPixmap>

#ifdef WITH_OPENCV
#include <opencv2/opencv.hpp>
#endif

class OpenCVImageProcessor {
public:
    // 图像基本操作
    static QPixmap resizeImage(const QPixmap& input, int width, int height);
    static QPixmap rotateImage(const QPixmap& input, double angle);
    static QPixmap cropImage(const QPixmap& input, int x, int y, int width, int height);
    
    // 图像滤镜
    static QPixmap blurImage(const QPixmap& input, int kernelSize);
    static QPixmap sharpenImage(const QPixmap& input);
    static QPixmap edgeDetection(const QPixmap& input);
    
    // 图像分析
    static QStringList detectObjects(const QPixmap& input);
    static QString analyzeImage(const QPixmap& input);
    
private:
#ifdef WITH_OPENCV
    static cv::Mat qPixmapToCvMat(const QPixmap& pixmap);
    static QPixmap cvMatToQPixmap(const cv::Mat& mat);
#endif
};
```

```cpp
// opencv_image_processor.cpp
#include "opencv_image_processor.h"
#include <QDebug>

#ifdef WITH_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>
#endif

QPixmap OpenCVImageProcessor::resizeImage(const QPixmap& input, int width, int height) {
#ifdef WITH_OPENCV
    cv::Mat mat = qPixmapToCvMat(input);
    cv::Mat resized;
    cv::resize(mat, resized, cv::Size(width, height));
    return cvMatToQPixmap(resized);
#else
    // Fallback using Qt
    return input.scaled(width, height, Qt::KeepAspectRatio, Qt::SmoothTransformation);
#endif
}

QPixmap OpenCVImageProcessor::blurImage(const QPixmap& input, int kernelSize) {
#ifdef WITH_OPENCV
    cv::Mat mat = qPixmapToCvMat(input);
    cv::Mat blurred;
    cv::GaussianBlur(mat, blurred, cv::Size(kernelSize, kernelSize), 0);
    return cvMatToQPixmap(blurred);
#else
    qWarning() << "OpenCV not available, blur effect not supported";
    return input;
#endif
}

QStringList OpenCVImageProcessor::detectObjects(const QPixmap& input) {
#ifdef WITH_OPENCV
    cv::Mat mat = qPixmapToCvMat(input);
    cv::HOGDescriptor hog;
    hog.setSVMDetector(cv::HOGDescriptor::getDefaultPeopleDetector());
    
    std::vector<cv::Rect> detections;
    hog.detectMultiScale(mat, detections);
    
    QStringList results;
    for (const auto& rect : detections) {
        results << QString("Person detected at (%1,%2) size %3x%4")
                   .arg(rect.x).arg(rect.y).arg(rect.width).arg(rect.height);
    }
    return results;
#else
    qWarning() << "OpenCV not available, object detection not supported";
    return QStringList() << "Object detection requires OpenCV";
#endif
}

#ifdef WITH_OPENCV
cv::Mat OpenCVImageProcessor::qPixmapToCvMat(const QPixmap& pixmap) {
    QImage qimg = pixmap.toImage().convertToFormat(QImage::Format_RGB888);
    return cv::Mat(qimg.height(), qimg.width(), CV_8UC3, 
                   (void*)qimg.constBits(), qimg.bytesPerLine()).clone();
}

QPixmap OpenCVImageProcessor::cvMatToQPixmap(const cv::Mat& mat) {
    QImage qimg;
    if (mat.channels() == 3) {
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        qimg = QImage(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
    } else if (mat.channels() == 1) {
        qimg = QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8);
    }
    return QPixmap::fromImage(qimg);
}
#endif
```

## 🏗️ 构建配置

### 本地构建

```powershell
# 使用支持OpenCV的构建脚本
.\script\build.ps1 -Version "1.0.0" -BuildType "Release"

# 构建脚本会自动检测OpenCV并复制相关DLL文件
```

### GitHub Actions

OpenCV已集成到CI/CD流程中：

```yaml
# 自动下载和安装OpenCV 4.10.0
# 设置环境变量和PATH
# 在CMake配置中包含OpenCV路径
```

## 📦 部署注意事项

### 1. DLL依赖

OpenCV应用需要以下DLL文件：

```
opencv_core4100.dll      # 核心功能
opencv_imgproc4100.dll   # 图像处理
opencv_imgcodecs4100.dll # 图像编解码
opencv_highgui4100.dll   # 高级GUI
# ... 其他需要的模块
```

构建脚本会自动复制这些文件到输出目录。

### 2. 安装包大小

- **不含OpenCV**: ~50MB
- **含OpenCV**: ~150-200MB

考虑提供两个版本：
- `LeleTools-Lite.exe` (无OpenCV)
- `LeleTools-Full.exe` (含OpenCV)

### 3. 系统要求

- Windows 10/11 (x64)
- Visual C++ Redistributable 2019/2022
- 至少500MB可用磁盘空间（含OpenCV版本）

## 🔍 使用示例

### 在现有模块中添加OpenCV功能

```cpp
// 在 image-compression 模块中添加高级功能
void ImageCompression::applyOpenCVFilter() {
#ifdef WITH_OPENCV
    if (currentPixmap.isNull()) return;
    
    // 使用OpenCV处理图像
    QPixmap processed = OpenCVImageProcessor::edgeDetection(currentPixmap);
    setProcessedImage(processed);
#else
    QMessageBox::information(this, "功能不可用", 
        "此功能需要OpenCV支持，请使用完整版本。");
#endif
}
```

### 创建新的OpenCV功能模块

```cpp
// 创建 modules/opencv-tools/ 目录
// 实现高级图像处理、计算机视觉功能
// 对象检测、人脸识别、图像特征提取等
```

## 🐛 故障排除

### 常见问题

1. **OpenCV库未找到**
   ```
   解决方案：
   - 检查环境变量 OPENCV_DIR
   - 确保路径指向 build 目录
   - 重新安装OpenCV到标准路径
   ```

2. **运行时DLL缺失**
   ```
   解决方案：
   - 运行 windeployqt 部署工具
   - 手动复制 opencv_*.dll 文件
   - 添加OpenCV bin目录到PATH
   ```

3. **版本不兼容**
   ```
   解决方案：
   - 使用推荐的OpenCV 4.10.0版本
   - 确保MSVC编译器版本匹配
   - 重新编译项目
   ```

## 📚 参考资源

- [OpenCV 官方文档](https://docs.opencv.org/)
- [OpenCV 教程](https://docs.opencv.org/4.x/d9/df8/tutorial_root.html)
- [Qt与OpenCV集成](https://wiki.qt.io/How_to_setup_Qt_and_openCV_on_Windows)
- [CMake FindOpenCV模块](https://cmake.org/cmake/help/latest/module/FindOpenCV.html)
