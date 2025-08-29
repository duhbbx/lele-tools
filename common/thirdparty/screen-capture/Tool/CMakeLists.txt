# Tool模块 - 工具栏和控件
# 包含各种工具按钮、颜色控制、笔触控制等界面控件

# 收集Tool模块的所有源文件
set(TOOL_SOURCES
    # 基础按钮类
    BtnBase.cpp         # 按钮基类
    BtnBase.h
    Btn.cpp             # 普通按钮
    Btn.h
    BtnCheck.cpp        # 复选按钮
    BtnCheck.h
    
    # 控制组件
    ColorCtrl.cpp       # 颜色控制器
    ColorCtrl.h
    StrokeCtrl.cpp      # 笔触控制器
    StrokeCtrl.h
    
    # 工具栏组件
    ToolBase.cpp        # 工具栏基类
    ToolBase.h
    ToolMain.cpp        # 主工具栏
    ToolMain.h
    ToolSub.cpp         # 子工具栏
    ToolSub.h
    ToolLong.cpp        # 长截图工具栏
    ToolLong.h
)

# 创建Tool静态库
add_library(ScreenCaptureTool STATIC ${TOOL_SOURCES})

# 设置库的包含目录
target_include_directories(ScreenCaptureTool
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}  # 当前目录（Tool/）
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..  # screen-capture项目根目录
)

# 链接Qt库
# Tool模块需要Qt的核心功能和界面组件
target_link_libraries(ScreenCaptureTool
    PUBLIC
    Qt6::Core       # Qt核心功能
    Qt6::Widgets    # Qt界面组件
    ScreenCaptureApp  # App模块（用于Lang等）
)

# 设置库属性
set_target_properties(ScreenCaptureTool PROPERTIES
    OUTPUT_NAME "ScreenCaptureTool"     # 库文件名
    DEBUG_POSTFIX "_d"                  # Debug版本添加_d后缀
)