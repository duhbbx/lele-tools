# FindOpenCV.cmake - 帮助CMake找到OpenCV的自定义模块

# 尝试使用OpenCV的官方CMake配置
find_package(OpenCV QUIET CONFIG)

if(OpenCV_FOUND)
    message(STATUS "Found OpenCV ${OpenCV_VERSION} using config mode")
    # OpenCV_LIBS 和 OpenCV_INCLUDE_DIRS 已由config设置
else()
    # 如果config模式失败，尝试手动查找
    message(STATUS "OpenCV config not found, trying manual detection...")
    
    # 查找OpenCV头文件
    find_path(OpenCV_INCLUDE_DIRS
        NAMES opencv2/opencv.hpp
        PATHS
            ${OpenCV_DIR}/include
            C:/opencv/build/include
            $ENV{OPENCV_DIR}/include
            /usr/local/include
            /usr/include
        PATH_SUFFIXES opencv4
    )
    
    # 查找OpenCV库文件
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(OpenCV_LIB_SUFFIX "d")
    else()
        set(OpenCV_LIB_SUFFIX "")
    endif()
    
    # 常用的OpenCV模块
    set(OpenCV_MODULES
        opencv_core
        opencv_imgproc
        opencv_imgcodecs
        opencv_highgui
        opencv_features2d
        opencv_calib3d
        opencv_objdetect
        opencv_video
        opencv_videoio
    )
    
    set(OpenCV_LIBS "")
    
    foreach(module ${OpenCV_MODULES})
        find_library(${module}_LIB
            NAMES ${module}${OpenCV_VERSION_MAJOR}${OpenCV_VERSION_MINOR}${OpenCV_VERSION_PATCH}${OpenCV_LIB_SUFFIX}
                  ${module}${OpenCV_LIB_SUFFIX}
            PATHS
                ${OpenCV_DIR}/lib
                ${OpenCV_DIR}/x64/vc16/lib
                ${OpenCV_DIR}/x64/vc15/lib
                C:/opencv/build/x64/vc16/lib
                C:/opencv/build/x64/vc15/lib
                $ENV{OPENCV_DIR}/lib
                /usr/local/lib
                /usr/lib
        )
        
        if(${module}_LIB)
            list(APPEND OpenCV_LIBS ${${module}_LIB})
            message(STATUS "Found OpenCV module: ${module}")
        endif()
    endforeach()
    
    # 检查是否找到了基本组件
    if(OpenCV_INCLUDE_DIRS AND OpenCV_LIBS)
        set(OpenCV_FOUND TRUE)
        message(STATUS "Found OpenCV manually:")
        message(STATUS "  Include dirs: ${OpenCV_INCLUDE_DIRS}")
        message(STATUS "  Libraries: ${OpenCV_LIBS}")
    else()
        set(OpenCV_FOUND FALSE)
        message(STATUS "OpenCV not found")
    endif()
endif()

# 创建导入目标（如果没有的话）
if(OpenCV_FOUND AND NOT TARGET opencv::opencv)
    add_library(opencv::opencv INTERFACE IMPORTED)
    target_include_directories(opencv::opencv INTERFACE ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(opencv::opencv INTERFACE ${OpenCV_LIBS})
endif()

# 标准的CMake find_package结果变量
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenCV
    REQUIRED_VARS OpenCV_LIBS OpenCV_INCLUDE_DIRS
    VERSION_VAR OpenCV_VERSION
)
