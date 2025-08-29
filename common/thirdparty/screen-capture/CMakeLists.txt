# ScreenCapture CMake 项目配置文件
# 这是项目的主配置文件，定义了整个项目的构建规则
# 
# 使用方式：
# 1. 独立构建：直接在本目录运行cmake
# 2. 子项目集成：在父项目中使用add_subdirectory(ScreenCapture)

# 指定CMake的最低版本要求
# 3.16版本支持Qt6和现代C++特性
cmake_minimum_required(VERSION 3.16)

# 检查是否作为子项目被包含
if(NOT DEFINED PROJECT_NAME)
    set(SCREENCAPTURE_IS_MAIN_PROJECT TRUE)
else()
    set(SCREENCAPTURE_IS_MAIN_PROJECT FALSE)
endif()

# 只在主项目时设置系统版本
if(SCREENCAPTURE_IS_MAIN_PROJECT)
    set(CMAKE_SYSTEM_VERSION 10.0.19041.0)
endif()

# 定义项目名称、版本和使用的编程语言
# VERSION用于生成版本信息，LANGUAGES指定使用C++
project(ScreenCapture VERSION 1.0.0 LANGUAGES CXX)

# 设置C++标准为C++20（支持<numbers>头文件）
# CMAKE_CXX_STANDARD_REQUIRED确保必须使用指定的C++标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找Qt6组件
# REQUIRED表示如果找不到Qt6则停止构建
# COMPONENTS指定需要的Qt模块：Core(核心)、Widgets(界面)、Concurrent(并发)
if(NOT TARGET Qt6::Core)
    find_package(Qt6 REQUIRED COMPONENTS Core Widgets Concurrent)
endif()

# 启用Qt的自动化工具（只在主项目时设置全局变量）
# CMAKE_AUTOMOC: 自动运行MOC(Meta-Object Compiler)处理Q_OBJECT宏
# CMAKE_AUTOUIC: 自动处理.ui界面文件
# CMAKE_AUTORCC: 自动处理.qrc资源文件
if(SCREENCAPTURE_IS_MAIN_PROJECT)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)
endif()

# 设置输出目录（只在主项目时设置）
# 统一管理生成的可执行文件和库文件的位置
if(SCREENCAPTURE_IS_MAIN_PROJECT)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)  # 可执行文件输出目录
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)  # 动态库输出目录
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)  # 静态库输出目录
endif()

# Windows平台特定配置
if(WIN32)

    message("这是win32应用.....................")
    # 设置为Windows应用程序（而不是控制台应用）
    set(CMAKE_WIN32_EXECUTABLE ON)
    
    # 定义Windows宏以避免头文件冲突和减少编译时间
    # 只使用最基本的宏，不排除必要的API
#    add_compile_definitions(
#        WIN32_LEAN_AND_MEAN  # 减少Windows.h包含的内容，加快编译
#        NOMINMAX             # 防止Windows.h定义min/max宏与C++标准库冲突
#        UNICODE              # 启用Unicode支持
#        _UNICODE             # 启用Unicode支持（另一个宏）
#    )
    
    # 添加编译器选项以忽略特定警告
    add_compile_options(
        /wd4005  # 忽略宏重定义警告
        /wd4996  # 忽略不安全函数警告
        /D_CRT_SECURE_NO_WARNINGS  # 禁用CRT安全警告
    )
    
    # 强制使用系统已有的稳定版本避免头文件冲突
    # 不设置CMAKE_SYSTEM_VERSION，让CMake自动选择合适的版本
    # set(CMAKE_SYSTEM_VERSION "10.0.19041.0")
    
    # 定义Windows宏以避免头文件冲突和减少编译时间
    add_compile_definitions(
        WIN32_LEAN_AND_MEAN  # 减少Windows.h包含的内容，加快编译
        NOMINMAX             # 防止Windows.h定义min/max宏与C++标准库冲突
        UNICODE              # 启用Unicode支持
        _UNICODE             # 启用Unicode支持（另一个宏）
    )
endif()



# 添加子目录，每个子目录包含一个CMakeLists.txt
# 这些子目录会被按顺序处理，构建对应的库
add_subdirectory(App)      # 应用程序核心逻辑模块
add_subdirectory(Win)      # 窗口管理和界面模块
add_subdirectory(Tool)     # 工具栏和控件模块
add_subdirectory(Shape)    # 图形绘制和形状处理模块

# =============================================================================
# 创建ScreenCapture库（总是创建，供父项目使用）
# =============================================================================

# 创建ScreenCapture静态库，包含所有功能
add_library(ScreenCaptureLib STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/ScreenCaptureAPI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ScreenCaptureAPI.h
    res.qrc         # Qt资源文件（图标、字体等）
)

# 为库设置Qt自动化工具
set_target_properties(ScreenCaptureLib PROPERTIES
    AUTOMOC ON
    AUTOUIC ON  
    AUTORCC ON
)

# 链接内部库到主库
target_link_libraries(ScreenCaptureLib
    PUBLIC
    # Qt6库（公开给使用者）
    Qt6::Core           # Qt核心功能
    Qt6::Widgets        # Qt界面组件
    Qt6::Concurrent     # Qt并发处理
    
    PRIVATE
    # 项目内部库（按依赖关系排序）
    ScreenCaptureWin    # Win模块依赖其他所有模块
    ScreenCaptureApp    # App模块被Win模块使用
    ScreenCaptureTool   # Tool模块被Win模块使用
    ScreenCaptureShape  # Shape模块被Win模块使用
)

# Windows平台特定的系统库链接
if(WIN32)
    target_link_libraries(ScreenCaptureLib 
        PRIVATE 
        user32      # Windows用户界面API
        gdi32       # Windows图形设备接口
        dwmapi      # Desktop Window Manager API
        shell32     # Windows Shell API
    )
endif()

# 设置库的包含目录
target_include_directories(ScreenCaptureLib
    PUBLIC
    # 公开给使用者的头文件目录
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
    PRIVATE
    # 内部使用的头文件目录
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/App
    ${CMAKE_CURRENT_SOURCE_DIR}/Win
    ${CMAKE_CURRENT_SOURCE_DIR}/Tool
    ${CMAKE_CURRENT_SOURCE_DIR}/Shape
)

# 设置库属性
set_target_properties(ScreenCaptureLib PROPERTIES
    OUTPUT_NAME "ScreenCapture"     # 输出文件名
    DEBUG_POSTFIX "_d"              # Debug版本添加_d后缀
    VERSION ${PROJECT_VERSION}      # 设置版本
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME ScreenCapture       # 导出名称
)

# 创建别名，方便父项目使用
add_library(ScreenCapture::ScreenCapture ALIAS ScreenCaptureLib)

# =============================================================================
# 创建可执行文件（仅在主项目时）
# =============================================================================

if(SCREENCAPTURE_IS_MAIN_PROJECT)
    # 创建主可执行文件
    # 包含主程序入口文件
    add_executable(ScreenCaptureApp
        main.cpp        # 程序入口点
    )
    
    # 为可执行文件设置Qt自动化工具
    set_target_properties(ScreenCaptureApp PROPERTIES
        AUTOMOC ON
        AUTOUIC ON  
        AUTORCC ON
    )

    # 链接库到主可执行文件
    target_link_libraries(ScreenCaptureApp
        PRIVATE
        ScreenCaptureLib    # 使用我们创建的库
    )

    # 设置可执行文件属性
    set_target_properties(ScreenCaptureApp PROPERTIES
        OUTPUT_NAME "ScreenCapture"     # 输出文件名
        DEBUG_POSTFIX "_d"              # Debug版本添加_d后缀
        WIN32_EXECUTABLE TRUE           # Windows GUI应用程序
    )
    
    message(STATUS "ScreenCapture: 构建独立可执行文件")
else()
    message(STATUS "ScreenCapture: 作为子项目构建，只创建库")
endif()