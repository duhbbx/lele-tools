# Win模块 - 窗口管理和界面
# 包含各种窗口类、画布、遮罩、像素信息等界面相关功能

# 收集Win模块的所有源文件
set(WIN_SOURCES
    # 基础窗口类
    WinBase.cpp         # 窗口基类
    WinBase.h
    
    # 主要窗口类
    WinFull.cpp         # 全屏截图窗口
    WinFull.h
    WinPin.cpp          # 贴图窗口
    WinPin.h
    WinPinBtns.cpp      # 贴图窗口按钮
    WinPinBtns.h
    WinLong.cpp         # 长截图窗口
    WinLong.h
    WinLongTip.cpp      # 长截图提示窗口
    WinLongTip.h
    
    # 界面组件
    Canvas.cpp          # 画布组件
    Canvas.h
    CutMask.cpp         # 裁剪遮罩
    CutMask.h
    PixelInfo.cpp       # 像素信息显示
    PixelInfo.h
    
    # 功能定义
    LongCapFuncs.h      # 长截图功能定义
)

# 创建Win静态库
add_library(ScreenCaptureWin STATIC ${WIN_SOURCES})

# 设置库的包含目录
target_include_directories(ScreenCaptureWin
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}  # 当前目录（Win/）
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..  # screen-capture项目根目录
    ${CMAKE_CURRENT_SOURCE_DIR}/../Shape    # Shape模块目录
    ${CMAKE_CURRENT_SOURCE_DIR}/../Tool     # Tool模块目录
    ${CMAKE_CURRENT_SOURCE_DIR}/../App      # App模块目录
)

# 链接Qt库和其他模块
# Win模块是最高层模块，依赖所有其他模块
target_link_libraries(ScreenCaptureWin
    PUBLIC
    Qt6::Core           # Qt核心功能
    Qt6::Widgets        # Qt界面组件
    Qt6::Concurrent     # Qt并发处理
    
    # 项目内部模块依赖
    ScreenCaptureApp    # 应用程序模块
    ScreenCaptureShape  # 形状处理模块
    ScreenCaptureTool   # 工具栏模块
)

# Windows平台特定的系统库
if(WIN32)
    target_link_libraries(ScreenCaptureWin
        PUBLIC
        user32      # Windows用户界面API
        gdi32       # Windows图形设备接口
        dwmapi      # Desktop Window Manager API
    )
endif()

# 设置库属性
set_target_properties(ScreenCaptureWin PROPERTIES
    OUTPUT_NAME "ScreenCaptureWin"      # 库文件名
    DEBUG_POSTFIX "_d"                  # Debug版本添加_d后缀
)