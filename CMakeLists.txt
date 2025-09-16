cmake_minimum_required(VERSION 3.5)

project(lele-tools VERSION 0.1 LANGUAGES CXX)

# 告诉 CMake 去哪里找 Qt，使用-DCMAKE_PREFIX_PATH=C:/Qt/6.9.1/msvc2022_64/lib/cmake这个指定避免代码中硬编码
# set(CMAKE_PREFIX_PATH "C:/Qt/6.9.1/msvc2022_64/lib/cmake/Qt6")

set(CMAKE_AUTOMOC ON)   # 自动处理 moc (信号与槽)
set(CMAKE_AUTORCC ON)   # 自动处理 qrc 资源文件
set(CMAKE_AUTOUIC ON)   # 自动处理 .ui 文件

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Redis 可选支持 (暂时禁用，等待库安装完成)
option(WITH_REDIS "Enable Redis support" ON)

# OpenCV 可选支持
option(WITH_OPENCV "Enable OpenCV support" ON)

if(WITH_OPENCV)
    # 尝试不同的OpenCV路径
    if(NOT OpenCV_DIR AND EXISTS "C:/opencv/build")
        set(OpenCV_DIR "C:/opencv/build")
    endif()
    
    # 查找OpenCV（可选）
    find_package(OpenCV QUIET)
    
    if(OpenCV_FOUND)
        message(STATUS "OpenCV found: ${OpenCV_VERSION}")
        message(STATUS "OpenCV include path: ${OpenCV_INCLUDE_DIRS}")
        message(STATUS "OpenCV lib path: ${OpenCV_LIBS}")
        
        # 定义宏表示OpenCV可用
        add_definitions(-DWITH_OPENCV)
        set(HAVE_OPENCV TRUE)
    else()
        message(WARNING "OpenCV not found. Building without OpenCV support.")
        message(STATUS "Set OpenCV_DIR to OpenCV installation path to enable OpenCV")
        set(HAVE_OPENCV FALSE)
    endif()
else()
    message(STATUS "OpenCV support disabled")
    set(HAVE_OPENCV FALSE)
endif()

option(ENABLE_REDIS "Enable Redis support" ON)

if(ENABLE_REDIS)
    message(STATUS "Checking for redis-plus-plus ...")
    
    # 尝试多种方式查找redis-plus-plus
    find_package(redis++ QUIET CONFIG)
    
    if(redis++_FOUND)
        message(STATUS "redis-plus-plus found via CONFIG")
        set(HAVE_REDIS TRUE)
    else()
        # 如果CONFIG方式失败，尝试手动查找
        # 直接指定vcpkg路径
        set(REDIS_PLUS_PLUS_INCLUDE_DIR "D:/projects/vcpkg/installed/x64-windows/include")
        set(REDIS_PLUS_PLUS_LIBRARY "D:/projects/vcpkg/installed/x64-windows/lib/redis++.lib")
        set(HIREDIS_LIBRARY "D:/projects/vcpkg/installed/x64-windows/lib/hiredis.lib")
        
        # 检查文件是否存在
        if(EXISTS "${REDIS_PLUS_PLUS_INCLUDE_DIR}/sw/redis++/redis++.h" AND 
           EXISTS "${REDIS_PLUS_PLUS_LIBRARY}" AND 
           EXISTS "${HIREDIS_LIBRARY}")
            message(STATUS "redis-plus-plus found at: ${REDIS_PLUS_PLUS_INCLUDE_DIR}")
            message(STATUS "redis++ library: ${REDIS_PLUS_PLUS_LIBRARY}")
            message(STATUS "hiredis library: ${HIREDIS_LIBRARY}")
        else()
            set(REDIS_PLUS_PLUS_INCLUDE_DIR "")
            set(REDIS_PLUS_PLUS_LIBRARY "")
            set(HIREDIS_LIBRARY "")
        endif()
        
        if(REDIS_PLUS_PLUS_INCLUDE_DIR AND REDIS_PLUS_PLUS_LIBRARY)
            message(STATUS "redis-plus-plus found manually")
            message(STATUS "  Include: ${REDIS_PLUS_PLUS_INCLUDE_DIR}")
            message(STATUS "  Library: ${REDIS_PLUS_PLUS_LIBRARY}")
            set(HAVE_REDIS TRUE)
            
            # 创建导入目标
            add_library(redis++::redis++ UNKNOWN IMPORTED)
            set_target_properties(redis++::redis++ PROPERTIES
                IMPORTED_LOCATION ${REDIS_PLUS_PLUS_LIBRARY}
                INTERFACE_INCLUDE_DIRECTORIES ${REDIS_PLUS_PLUS_INCLUDE_DIR}
                INTERFACE_LINK_LIBRARIES ${HIREDIS_LIBRARY}
            )
        else()
            message(WARNING "redis-plus-plus not found. Building without Redis support.")
            message(STATUS "To install redis-plus-plus:")
            message(STATUS "  Windows: vcpkg install redis-plus-plus")
            message(STATUS "  Linux: sudo apt-get install libhiredis-dev libsw-redis++-dev")
            message(STATUS "  macOS: brew install redis-plus-plus")
            set(HAVE_REDIS FALSE)
        endif()
    endif()
else()
    message(STATUS "Redis support disabled")
    set(HAVE_REDIS FALSE)
endif()


# 头文件目录
if(HAVE_OPENCV)
    include_directories(${OpenCV_INCLUDE_DIRS})
endif()


# 第三方的代码
add_subdirectory(common/thirdparty/yaml-cpp)

# 截图功能仅在Windows平台可用
if(WIN32)
    add_subdirectory(common/thirdparty/screen-capture)
    set(HAVE_SCREEN_CAPTURE TRUE)
    add_definitions(-DWITH_SCREEN_CAPTURE)
else()
    message(STATUS "Screen capture disabled on non-Windows platforms")
    set(HAVE_SCREEN_CAPTURE FALSE)
    # 创建一个空的ScreenCaptureLib目标以避免链接错误
    add_library(ScreenCaptureLib INTERFACE)
endif()



# 源码加进来
file(GLOB_RECURSE SOURCES ./modules/*.cpp)
file(GLOB_RECURSE COMMON_SOURCES ./common/*.cpp)
# 排除第三方库的源码，它们有自己的编译规则
list(FILTER COMMON_SOURCES EXCLUDE REGEX ".*thirdparty.*")




# 将模块的源码加进来
SET(MODULE_SOURCES ${SOURCES} ${COMMON_SOURCES})



# 必需的Qt组件 (包含SQL)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools Network Concurrent Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools Network Concurrent Sql)

# 可选的Qt多媒体组件
option(WITH_QT_MULTIMEDIA "Enable Qt Multimedia support" ON)

if(WITH_QT_MULTIMEDIA)
    find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS Multimedia MultimediaWidgets)
    
    if(Qt${QT_VERSION_MAJOR}Multimedia_FOUND)
        message(STATUS "Qt Multimedia found: ${Qt${QT_VERSION_MAJOR}Multimedia_VERSION}")
        add_definitions(-DWITH_QT_MULTIMEDIA)
        set(HAVE_QT_MULTIMEDIA TRUE)
        
        # Check if MultimediaWidgets is available separately or included in Multimedia
        if(Qt${QT_VERSION_MAJOR}MultimediaWidgets_FOUND)
            set(QT_MULTIMEDIA_LIBS Qt${QT_VERSION_MAJOR}::Multimedia Qt${QT_VERSION_MAJOR}::MultimediaWidgets)
            message(STATUS "Qt MultimediaWidgets found separately")
        else()
            # In some Qt versions, MultimediaWidgets is included in Multimedia
            set(QT_MULTIMEDIA_LIBS Qt${QT_VERSION_MAJOR}::Multimedia)
            message(STATUS "Using Qt Multimedia (MultimediaWidgets included)")
        endif()
    else()
        message(WARNING "Qt Multimedia not found. Camera tool will be disabled.")
        message(STATUS "To enable Qt Multimedia, install the qtmultimedia module")
        set(HAVE_QT_MULTIMEDIA FALSE)
        set(QT_MULTIMEDIA_LIBS "")
    endif()
else()
    message(STATUS "Qt Multimedia support disabled")
    set(HAVE_QT_MULTIMEDIA FALSE)
    set(QT_MULTIMEDIA_LIBS "")
endif()

qt_standard_project_setup() # Qt6 推荐做法

set(TS_FILES 
    translations/lele-tools_zh_CN.ts
    translations/lele-tools_en_US.ts
)

set(PROJECT_SOURCES
        main.cpp
        main-widget/mainwindow.cpp main-widget/mainwindow.h
        main-widget/customtabbar.cpp main-widget/customtabbar.h
        main-widget/mainwindowtabwidget.cpp main-widget/mainwindowtabwidget.h
        common/dynamicobjectbase.cpp common/dynamicobjectbase.h
        common/customtabwidget.cpp common/customtabwidget.h
        common/global-styles.cpp common/global-styles.h
        common/thirdparty/pugixml/pugixml.cpp common/thirdparty/pugixml/pugixml.hpp common/thirdparty/pugixml/pugiconfig.hpp
        common/validator/myintvalidator.cpp
        ${TS_FILES}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(lele-tools
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        ${MODULE_SOURCES}
        tool-list/toollist.h tool-list/toollist.cpp
        common/string-utils/stringutils.h common/string-utils/stringutils.cpp
        tool-list/module-meta.h
        tool-list/module-meta-data.h
        common/datetime-utils/datetimeutils.h common/datetime-utils/datetimeutils.cpp
        my.qrc
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET lele-tools APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation

    # 添加翻译文件到目标（这会自动处理.qm文件的生成）
    qt_add_translations(lele-tools TS_FILES ${TS_FILES})
else()
    if(ANDROID)
        add_library(lele-tools SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(lele-tools
            ${PROJECT_SOURCES}
        )
    endif()

    # 为Qt5创建翻译文件
    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

# WebEngine components (optional for GitHub Actions compatibility)
find_package(Qt6 COMPONENTS WebEngineCore QUIET)
if(Qt6WebEngineCore_FOUND)
    message(STATUS "Qt6 WebEngineCore found")
    target_link_libraries(lele-tools PRIVATE Qt6::WebEngineCore)
    add_definitions(-DWITH_WEBENGINE_CORE)
else()
    message(WARNING "Qt6 WebEngineCore not found - some features may be disabled")
endif()

find_package(Qt6 COMPONENTS WebEngineWidgets QUIET)
if(Qt6WebEngineWidgets_FOUND)
    message(STATUS "Qt6 WebEngineWidgets found")
    target_link_libraries(lele-tools PRIVATE Qt6::WebEngineWidgets)
    add_definitions(-DWITH_WEBENGINE_WIDGETS)
else()
    message(WARNING "Qt6 WebEngineWidgets not found - some features may be disabled")
endif()

target_link_libraries(lele-tools PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Concurrent Qt${QT_VERSION_MAJOR}::Sql ${QT_MULTIMEDIA_LIBS} yaml-cpp ScreenCaptureLib)


# 链接库
if(HAVE_OPENCV)
    target_link_libraries(lele-tools PRIVATE ${OpenCV_LIBS})
endif()

# Redis库链接 (移到目标定义之后)
if(HAVE_REDIS)
    message(STATUS "Linking redis-plus-plus to target")
    target_compile_definitions(lele-tools PRIVATE WITH_REDIS_PLUS_PLUS)
    
    if(TARGET redis++::redis++)
        target_link_libraries(lele-tools PRIVATE redis++::redis++)
    elseif(TARGET redis++)
        target_link_libraries(lele-tools PRIVATE redis++)
    else()
        message(WARNING "Redis target not found, linking manually")
        target_include_directories(lele-tools PRIVATE ${REDIS_PLUS_PLUS_INCLUDE_DIR})
        target_link_libraries(lele-tools PRIVATE ${REDIS_PLUS_PLUS_LIBRARY} ${HIREDIS_LIBRARY})
    endif()
endif()

# 添加Windows Socket库 (Redis需要)
if(WIN32 AND HAVE_REDIS)
    target_link_libraries(lele-tools PRIVATE ws2_32 wsock32)
endif()

# Npcap/libpcap支持 (Traceroute工具需要)
option(WITH_PCAP "Enable pcap support for network tools" ON)

if(WITH_PCAP)
    if(WIN32)
        # Windows平台查找Npcap
        # --- 允许用户手动指定路径 ---
        set(PCAP_INCLUDE_DIR "" CACHE PATH "Npcap include directory (pcap.h)")
        set(PCAP_LIBRARY "" CACHE FILEPATH "Npcap library (wpcap.lib)")
        set(PACKET_LIBRARY "" CACHE FILEPATH "Packet library (packet.lib)")

        # --- 如果未指定，才使用 find_path/find_library 搜索 ---
        if(NOT PCAP_INCLUDE_DIR)
            find_path(PCAP_INCLUDE_DIR
                NAMES pcap.h
                PATHS
                    "C:/npcap-sdk-1.15/Include"
                    "C:/Program Files/Npcap/Include"
                    "C:/Program Files (x86)/Npcap/Include"
                    "${CMAKE_PREFIX_PATH}/include"
                DOC "Npcap include directory"
                NO_DEFAULT_PATH
            )
        endif()

        if(NOT PCAP_LIBRARY)
            find_library(PCAP_LIBRARY
                NAMES wpcap pcap
                PATHS
                    "C:/npcap-sdk-1.15/Lib/x64"
                    "C:/Program Files/Npcap/Lib/x64"
                    "C:/Program Files (x86)/Npcap/Lib/x64"
                    "${CMAKE_PREFIX_PATH}/lib"
                DOC "Npcap library"
                NO_DEFAULT_PATH
            )
        endif()

        if(NOT PACKET_LIBRARY)
            find_library(PACKET_LIBRARY
                NAMES packet
                PATHS
                    "C:/npcap-sdk-1.15/Lib/x64"
                    "C:/Program Files/Npcap/Lib/x64"
                    "C:/Program Files (x86)/Npcap/Lib/x64"
                    "${CMAKE_PREFIX_PATH}/lib"
                DOC "Packet library"
                NO_DEFAULT_PATH
            )
        endif()

        # --- 配置目标 ---
        if(PCAP_INCLUDE_DIR AND PCAP_LIBRARY)
            message(STATUS "Npcap found:")
            message(STATUS "  Include: ${PCAP_INCLUDE_DIR}")
            message(STATUS "  Library: ${PCAP_LIBRARY}")
            set(HAVE_PCAP TRUE)
            add_definitions(-DWITH_PCAP)
            target_include_directories(lele-tools PRIVATE ${PCAP_INCLUDE_DIR})
            target_link_libraries(lele-tools PRIVATE ${PCAP_LIBRARY})
            if(PACKET_LIBRARY)
                target_link_libraries(lele-tools PRIVATE ${PACKET_LIBRARY})
            endif()
        else()
            message(WARNING "Npcap SDK not found. Traceroute tool will use Windows API fallback.")
            message(STATUS "To enable full pcap functionality:")
            message(STATUS "  1. Download Npcap from: https://npcap.com/")
            message(STATUS "  2. Install with 'Install Npcap SDK' option checked")
            message(STATUS "  3. Or download SDK separately from GitHub releases")

            # 使用Windows系统API作为备用方案
            message(STATUS "Using Windows ICMP API fallback")
            set(HAVE_PCAP FALSE)
            add_definitions(-DWITH_WINDOWS_ICMP)
        endif()
    else()
        # Linux/macOS平台查找libpcap
        find_path(PCAP_INCLUDE_DIR
            NAMES pcap/pcap.h pcap.h
            PATHS /usr/include /usr/local/include
            DOC "libpcap include directory"
        )
        
        find_library(PCAP_LIBRARY
            NAMES pcap
            PATHS /usr/lib /usr/local/lib
            DOC "libpcap library"
        )
        
        if(PCAP_INCLUDE_DIR AND PCAP_LIBRARY)
            message(STATUS "libpcap found:")
            message(STATUS "  Include: ${PCAP_INCLUDE_DIR}")
            message(STATUS "  Library: ${PCAP_LIBRARY}")
            set(HAVE_PCAP TRUE)
            add_definitions(-DWITH_PCAP)
            target_include_directories(lele-tools PRIVATE ${PCAP_INCLUDE_DIR})
            target_link_libraries(lele-tools PRIVATE ${PCAP_LIBRARY})
        else()
            message(WARNING "libpcap not found. Traceroute tool will be disabled.")
            message(STATUS "To install libpcap:")
            message(STATUS "  Ubuntu/Debian: sudo apt-get install libpcap-dev")
            message(STATUS "  CentOS/RHEL: sudo yum install libpcap-devel")
            message(STATUS "  macOS: brew install libpcap")
            set(HAVE_PCAP FALSE)
        endif()
    endif()
else()
    message(STATUS "pcap support disabled")
    set(HAVE_PCAP FALSE)
endif()

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.lele-tools)
endif()
set_target_properties(lele-tools PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS lele-tools
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(lele-tools)
endif()
