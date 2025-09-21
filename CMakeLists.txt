cmake_minimum_required(VERSION 3.10)
project(lele-tools VERSION 0.1 LANGUAGES CXX)

# 添加自定义模块路径，告诉 CMake 去哪里找 Find*.cmake
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# 加载环境配置模块
include(LoadEnvironment)

# 设置默认环境（可通过命令行覆盖： -DENVIRONMENT=xxx）
if(DEFINED ENVIRONMENT)
    set(ENVIRONMENT "台式机" CACHE STRING "Environment name for .env file")
    # 加载环境文件（会自动设置为配置依赖，文件修改时会触发重新配置）
    load_environment_file("${ENVIRONMENT}")
endif()

# 验证关键的环境变量是否存在（可选）
# require_environment_variables(REDIS_PLUS_PLUS_INCLUDE_DIR REDIS_PLUS_PLUS_LIBRARY)

# 设置默认值（如果某些变量未定义）
# set_environment_default(SOME_VAR "default_value")

set(CMAKE_AUTOMOC ON)   # 自动处理 moc (信号与槽)
set(CMAKE_AUTORCC ON)   # 自动处理 qrc 资源文件
set(CMAKE_AUTOUIC ON)   # 自动处理 .ui 文件

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows MSVC 编译器标志
if(WIN32 AND MSVC)
    # 允许多个 CL.EXE 写入同一个 .PDB 文件
    add_compile_options(/FS)
endif()


# Redis 可选支持已移至下方
# OpenCV 可选支持
option(WITH_OPENCV "Enable OpenCV support" ON)
# MySQL 可选支持
option(WITH_MYSQL "Enable MySQL support" ON)

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

# Redis 支持检测
if(ENABLE_REDIS)
    find_package(Redis)
    if(Redis_FOUND)
        set(HAVE_REDIS TRUE)
        message(STATUS "Redis support enabled")
    else()
        set(HAVE_REDIS FALSE)
        message(WARNING "Redis support requested but redis-plus-plus not found")
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
# 临时禁用Qt AUTOGEN功能给yaml-cpp（它不需要Qt）
set(CMAKE_AUTOMOC_BACKUP ${CMAKE_AUTOMOC})
set(CMAKE_AUTOUIC_BACKUP ${CMAKE_AUTOUIC})
set(CMAKE_AUTORCC_BACKUP ${CMAKE_AUTORCC})
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

add_subdirectory(common/thirdparty/yaml-cpp)

# 恢复Qt AUTOGEN设置
set(CMAKE_AUTOMOC ${CMAKE_AUTOMOC_BACKUP})
set(CMAKE_AUTOUIC ${CMAKE_AUTOUIC_BACKUP})
set(CMAKE_AUTORCC ${CMAKE_AUTORCC_BACKUP})

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



# 必需的Qt组件 (包含SQL用于SQLite，MySQL使用原生驱动)
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

# Windows资源文件
set(WINDOWS_RC_FILES "")
if(WIN32)
    if(EXISTS "${CMAKE_SOURCE_DIR}/resources/app.rc")
        set(WINDOWS_RC_FILES "${CMAKE_SOURCE_DIR}/resources/app.rc")
        message(STATUS "Including Windows resource file: ${WINDOWS_RC_FILES}")
    endif()
endif()

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
        ${WINDOWS_RC_FILES}
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
    target_link_libraries(lele-tools PRIVATE Redis::Redis)
endif()

# 添加Windows Socket库 (Redis需要)
if(WIN32 AND HAVE_REDIS)
    target_link_libraries(lele-tools PRIVATE ws2_32 wsock32)
endif()

# MySQL 可选支持
if(WITH_MYSQL)
    message(STATUS "Checking for MySQL ...")

    # 使用自定义FindMySQL模块
    find_package(MySQL QUIET)

    if(MySQL_FOUND)
        message(STATUS "MySQL found via FindMySQL module")
        set(HAVE_MYSQL TRUE)
    else()
        message(WARNING "MySQL not found. Building without MySQL support.")
        set(HAVE_MYSQL FALSE)
    endif()
else()
    message(STATUS "MySQL support disabled")
    set(HAVE_MYSQL FALSE)
endif()

# MySQL库链接
if(HAVE_MYSQL)
    message(STATUS "Linking MySQL to target")
    target_compile_definitions(lele-tools PRIVATE WITH_MYSQL)

    if(TARGET MySQL::MySQL)
        target_link_libraries(lele-tools PRIVATE MySQL::MySQL)
    else()
        message(STATUS "MySQL target not found, linking manually")
        target_include_directories(lele-tools PRIVATE ${MySQL_INCLUDE_DIRS})
        target_link_libraries(lele-tools PRIVATE ${MySQL_LIBRARIES})
    endif()
endif()

# Npcap/libpcap支持 (网络工具的可选功能)
# Pcap 支持检测 - 可选依赖，不影响主要功能
option(WITH_PCAP "Enable pcap support for enhanced network tools" ON)

if(WITH_PCAP)
    find_package(Pcap QUIET)
    if(Pcap_FOUND)
        message(STATUS "Pcap support enabled - enhanced network capture available")
        target_link_libraries(lele-tools PRIVATE Pcap::Pcap)
        set(HAVE_PCAP TRUE)
    else()
        message(STATUS "Pcap not found - network tools will use system fallback methods")
        message(STATUS "  To enable enhanced network capture, install Npcap from https://npcap.com/")
        set(HAVE_PCAP FALSE)
    endif()
else()
    message(STATUS "Pcap support disabled")
    set(HAVE_PCAP FALSE)
endif()

# 确保无论是否找到Pcap，都定义相关宏
if(HAVE_PCAP)
    target_compile_definitions(lele-tools PRIVATE WITH_PCAP)
else()
    target_compile_definitions(lele-tools PRIVATE WITHOUT_PCAP)
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

# 设置应用程序图标
if(WIN32)
    # Windows图标
    if(EXISTS "${CMAKE_SOURCE_DIR}/resources/app-icon.ico")
        set_target_properties(lele-tools PROPERTIES
            WIN32_EXECUTABLE_ICON "${CMAKE_SOURCE_DIR}/resources/app-icon.ico"
        )
        message(STATUS "Windows application icon configured: ${CMAKE_SOURCE_DIR}/resources/app-icon.ico")
    endif()
elseif(APPLE)
    # macOS图标 (如果有icns文件)
    if(EXISTS "${CMAKE_SOURCE_DIR}/resources/app-icon.icns")
        set_target_properties(lele-tools PROPERTIES
            MACOSX_BUNDLE_ICON_FILE "app-icon.icns"
        )
        # 复制图标到bundle
        configure_file("${CMAKE_SOURCE_DIR}/resources/app-icon.icns"
                      "${CMAKE_BINARY_DIR}/lele-tools.app/Contents/Resources/app-icon.icns"
                      COPYONLY)
    endif()
endif()

include(GNUInstallDirs)
install(TARGETS lele-tools
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(lele-tools)
endif()
