# Shape模块 - 图形绘制和形状处理
# 包含各种绘制形状的类：箭头、椭圆、线条、矩形、文本等

# 收集Shape模块的所有源文件
set(SHAPE_SOURCES
    # 基础形状类
    ShapeBase.cpp           # 形状基类
    ShapeBase.h
    ShapeState.h            # 形状状态定义
    
    # 线条相关形状
    ShapeLineBase.cpp       # 线条基类
    ShapeLineBase.h
    ShapeLine.cpp           # 直线
    ShapeLine.h
    ShapeArrow.cpp          # 箭头
    ShapeArrow.h
    
    # 矩形相关形状
    ShapeRectBase.cpp       # 矩形基类
    ShapeRectBase.h
    ShapeRect.cpp           # 矩形
    ShapeRect.h
    ShapeEllipse.cpp        # 椭圆
    ShapeEllipse.h
    
    # 特殊功能形状
    ShapeEraserLine.cpp     # 橡皮擦线条
    ShapeEraserLine.h
    ShapeEraserRect.cpp     # 橡皮擦矩形
    ShapeEraserRect.h
    ShapeMosaicLine.cpp     # 马赛克线条
    ShapeMosaicLine.h
    ShapeMosaicRect.cpp     # 马赛克矩形
    ShapeMosaicRect.h
    
    # 文本和数字
    ShapeText.cpp           # 文本形状
    ShapeText.h
    ShapeTextContainer.cpp  # 文本容器
    ShapeTextContainer.h
    ShapeTextInput.cpp      # 文本输入
    ShapeTextInput.h
    ShapeNumber.cpp         # 数字标记
    ShapeNumber.h
)

# 创建Shape静态库
add_library(ScreenCaptureShape STATIC ${SHAPE_SOURCES})

# 设置库的包含目录
target_include_directories(ScreenCaptureShape
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}  # 当前目录（Shape/）
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..  # screen-capture项目根目录
    ${CMAKE_CURRENT_SOURCE_DIR}/../App  # App模块目录（用于访问State.h等）
)

# 链接Qt库
# Shape模块需要Qt的核心功能、界面组件和并发处理
target_link_libraries(ScreenCaptureShape
    PUBLIC
    Qt6::Core           # Qt核心功能
    Qt6::Widgets        # Qt界面组件
    Qt6::Concurrent     # Qt并发处理（用于图像处理）
)

# 设置库属性
set_target_properties(ScreenCaptureShape PROPERTIES
    OUTPUT_NAME "ScreenCaptureShape"    # 库文件名
    DEBUG_POSTFIX "_d"                  # Debug版本添加_d后缀
)