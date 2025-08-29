# App模块 - 应用程序核心逻辑
# 包含应用程序的主要业务逻辑、语言处理、工具函数等

# 收集App模块的所有源文件
set(APP_SOURCES
    App.cpp         # 主应用程序类
    App.h
    Lang.cpp        # 语言和本地化处理
    Lang.h
    Util.cpp        # 工具函数和辅助功能
    Util.h
    Logger.cpp      # 日志系统实现
    Logger.h        # 日志系统头文件
    State.h         # 应用程序状态定义
    Res.h           # 资源定义
    Res.rc          # Windows资源文件
)

# 创建App静态库
# STATIC表示创建静态库，在链接时会被包含到最终可执行文件中
add_library(ScreenCaptureApp STATIC ${APP_SOURCES})

# 设置库的包含目录
# PUBLIC表示这些目录对使用此库的其他目标也可见
target_include_directories(ScreenCaptureApp
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}  # 当前目录（App/）
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..  # screen-capture项目根目录
)

# 链接Qt库
# App模块需要Qt的核心功能和界面组件
target_link_libraries(ScreenCaptureApp
    PUBLIC
    Qt6::Core       # Qt核心功能
    Qt6::Widgets    # Qt界面组件
)

# 设置库属性
set_target_properties(ScreenCaptureApp PROPERTIES
    OUTPUT_NAME "ScreenCaptureApp"  # 库文件名
    DEBUG_POSTFIX "_d"              # Debug版本添加_d后缀
)