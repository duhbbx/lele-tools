# 在你的主 CMakeLists.txt 中添加这些内容来支持OpenCV

# 设置CMake模块路径（如果使用自定义FindOpenCV.cmake）
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# 可选的OpenCV支持
option(WITH_OPENCV "Enable OpenCV support" ON)

if(WITH_OPENCV)
    # 查找OpenCV
    find_package(OpenCV QUIET)
    
    if(OpenCV_FOUND)
        message(STATUS "OpenCV found: ${OpenCV_VERSION}")
        message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")
        message(STATUS "OpenCV libraries: ${OpenCV_LIBS}")
        
        # 定义预处理器宏，表示OpenCV可用
        add_definitions(-DWITH_OPENCV)
        
        # 设置全局变量供其他模块使用
        set(HAVE_OPENCV TRUE CACHE BOOL "OpenCV is available")
        
        # 创建OpenCV接口库（如果不存在）
        if(NOT TARGET opencv::opencv)
            add_library(opencv::opencv INTERFACE IMPORTED)
            target_include_directories(opencv::opencv INTERFACE ${OpenCV_INCLUDE_DIRS})
            target_link_libraries(opencv::opencv INTERFACE ${OpenCV_LIBS})
        endif()
        
    else()
        message(WARNING "OpenCV not found. Building without OpenCV support.")
        set(HAVE_OPENCV FALSE CACHE BOOL "OpenCV is not available")
    endif()
else()
    message(STATUS "OpenCV support disabled by user")
    set(HAVE_OPENCV FALSE CACHE BOOL "OpenCV support disabled")
endif()

# 示例：如何在模块中使用OpenCV
# 在你的模块 CMakeLists.txt 中：

# if(HAVE_OPENCV)
#     # 添加OpenCV支持的源文件
#     target_sources(your_target PRIVATE
#         opencv_module.cpp
#         image_processing.cpp
#     )
#     
#     # 链接OpenCV
#     target_link_libraries(your_target PRIVATE opencv::opencv)
#     
#     # 添加编译定义
#     target_compile_definitions(your_target PRIVATE WITH_OPENCV)
# endif()

# 示例：条件编译的头文件保护
# 在你的C++代码中:
# 
# #ifdef WITH_OPENCV
# #include <opencv2/opencv.hpp>
# 
# void processImage(const std::string& imagePath) {
#     cv::Mat image = cv::imread(imagePath);
#     // OpenCV图像处理代码
# }
# #else
# void processImage(const std::string& imagePath) {
#     // 没有OpenCV时的fallback实现
#     qDebug() << "OpenCV not available, skipping image processing";
# }
# #endif
