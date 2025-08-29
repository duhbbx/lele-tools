# 示例项目：演示如何将ScreenCapture作为子项目集成
cmake_minimum_required(VERSION 3.16)
project(ScreenCaptureIntegrationExample VERSION 1.0.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Concurrent)

# 启用Qt自动化工具
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

message(STATUS "=== 集成示例项目配置 ===")
message(STATUS "项目名称: ${PROJECT_NAME}")
message(STATUS "项目版本: ${PROJECT_VERSION}")
message(STATUS "C++标准: ${CMAKE_CXX_STANDARD}")

# 添加ScreenCapture子项目
# 注意：这里假设ScreenCapture在父目录中
message(STATUS "添加ScreenCapture子项目...")
add_subdirectory(.. ScreenCapture)

# 验证ScreenCapture是否正确加载
if(TARGET ScreenCapture::ScreenCapture)
    message(STATUS "✅ ScreenCapture库已成功加载")
    get_target_property(SC_VERSION ScreenCaptureLib VERSION)
    message(STATUS "ScreenCapture版本: ${SC_VERSION}")
else()
    message(FATAL_ERROR "❌ 无法找到ScreenCapture库")
endif()

# 创建示例应用程序
add_executable(IntegrationExample
    main.cpp
    MainWindow.cpp
    MainWindow.h
    MainWindow.ui
)

# 链接库
target_link_libraries(IntegrationExample 
    PRIVATE 
    ScreenCapture::ScreenCapture    # ScreenCapture库
    Qt6::Core
    Qt6::Widgets
)

# 设置应用程序属性
set_target_properties(IntegrationExample PROPERTIES
    OUTPUT_NAME "ScreenCaptureExample"
    DEBUG_POSTFIX "_d"
    WIN32_EXECUTABLE TRUE
)

# Windows特定配置
if(WIN32)
    # 设置图标等资源
    # target_sources(IntegrationExample PRIVATE app.rc)
endif()

message(STATUS "=== 配置完成 ===")
message(STATUS "构建目标: IntegrationExample")
message(STATUS "输出目录: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
