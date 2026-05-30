cmake_minimum_required(VERSION 3.10)
project(lele-tools VERSION 1.2.0 LANGUAGES CXX C)

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




# ip2region 离线IP数据库（静态库）
add_library(ip2region STATIC
    common/thirdparty/ip2region/xdb_searcher.c
    common/thirdparty/ip2region/xdb_util.c
)
set_target_properties(ip2region PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
)
target_include_directories(ip2region PUBLIC common/thirdparty/ip2region)

# 将模块的源码加进来
SET(MODULE_SOURCES ${SOURCES} ${COMMON_SOURCES})



# 必需的Qt组件 (包含SQL用于SQLite，MySQL使用原生驱动)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools Network Concurrent Sql PrintSupport Svg)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools Network Concurrent Sql PrintSupport Svg)

# Qt Pdf 可选支持（用于PDF盖章/签名工具，渲染PDF页面）
option(WITH_QT_PDF "Enable Qt Pdf support for PDF stamp tool" ON)
if(WITH_QT_PDF)
    find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS Pdf)
    if(Qt${QT_VERSION_MAJOR}Pdf_FOUND)
        message(STATUS "Qt Pdf found: ${Qt${QT_VERSION_MAJOR}Pdf_VERSION}")
        set(HAVE_QT_PDF TRUE)
        set(QT_PDF_LIBS Qt${QT_VERSION_MAJOR}::Pdf)
    else()
        message(WARNING "Qt Pdf not found. PDF stamp tool will be disabled.")
        message(STATUS "  Install qtpdf module (e.g. brew install qtwebengine on macOS) to enable")
        set(HAVE_QT_PDF FALSE)
        set(QT_PDF_LIBS "")
    endif()
else()
    message(STATUS "Qt Pdf support disabled")
    set(HAVE_QT_PDF FALSE)
    set(QT_PDF_LIBS "")
endif()

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
        main-widget/aboutdialog.cpp main-widget/aboutdialog.h
        main-widget/toolhelp.cpp main-widget/toolhelp.h
        main-widget/customtabbar.cpp main-widget/customtabbar.h
        main-widget/mainwindowtabwidget.cpp main-widget/mainwindowtabwidget.h
        common/dynamicobjectbase.cpp common/dynamicobjectbase.h
        common/customtabwidget.cpp common/customtabwidget.h
        common/customtablewidget.cpp common/customtablewidget.h
        common/global-styles.cpp common/global-styles.h
        common/thirdparty/pugixml/pugixml.cpp common/thirdparty/pugixml/pugixml.hpp common/thirdparty/pugixml/pugiconfig.hpp
        common/validator/myintvalidator.cpp
        ${TS_FILES}
        ${WINDOWS_RC_FILES}
        common/delegate/MyItemEditDelegate.h
        common/delegate/ComboBoxEditDelegate.h
        common/delegate/ComboBoxEditDelegate.cpp
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(lele-tools
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        ${MODULE_SOURCES}
        tool-list/toollist.h tool-list/toollist.cpp
        common/string-utils/stringutils.h common/string-utils/stringutils.cpp
        tool-list/module-meta.h
        common/datetime-utils/datetimeutils.h common/datetime-utils/datetimeutils.cpp
        common/toolusagetracker.h common/toolusagetracker.cpp
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
            ${MODULE_SOURCES}
        )
    endif()

    # 为Qt5创建翻译文件
    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

# WebEngine 已移除：项目无源码使用，且体积占比极大（~200MB+）

# 将版本号传递给源码
target_compile_definitions(lele-tools PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
    APP_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    APP_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    APP_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

target_link_libraries(lele-tools PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Concurrent Qt${QT_VERSION_MAJOR}::Sql Qt${QT_VERSION_MAJOR}::PrintSupport Qt${QT_VERSION_MAJOR}::Svg ${QT_MULTIMEDIA_LIBS} ${QT_PDF_LIBS} yaml-cpp ScreenCaptureLib ip2region)

if(HAVE_QT_PDF)
    target_compile_definitions(lele-tools PRIVATE WITH_QT_PDF)
endif()


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

    # 优先用 MariaDB Connector/C — 它和 libmysqlclient 源码级 API 兼容
    # （同一个 mysql.h、同一组函数），但同时原生支持 mysql_native_password
    # 与 caching_sha2_password 两种认证协议；MySQL 9 客户端则把
    # mysql_native_password 插件彻底删除了，连老服务器会失败。
    set(HAVE_MYSQL FALSE)
    set(USING_MARIADB_CLIENT FALSE)

    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(MARIADB_PC QUIET libmariadb)
        if(MARIADB_PC_FOUND)
            message(STATUS "MariaDB Connector/C found (pkg-config): ${MARIADB_PC_LIBRARIES}")
            target_compile_definitions(lele-tools PRIVATE WITH_MYSQL)
            target_include_directories(lele-tools PRIVATE ${MARIADB_PC_INCLUDE_DIRS})
            target_link_libraries(lele-tools PRIVATE ${MARIADB_PC_LDFLAGS})
            set(HAVE_MYSQL TRUE)
            set(USING_MARIADB_CLIENT TRUE)
        endif()
    endif()

    if(NOT USING_MARIADB_CLIENT)
        # 手动找 MariaDB Connector/C（无 pkg-config 时）
        find_path(MARIADB_INC_DIR mysql.h
            HINTS /opt/homebrew/include/mariadb
                  /opt/homebrew/opt/mariadb-connector-c/include/mariadb
                  /usr/local/include/mariadb
                  /usr/include/mariadb
            DOC "MariaDB Connector/C 头文件目录（含 mysql.h）")
        find_library(MARIADB_LIB
            NAMES mariadb mariadbclient libmariadb
            HINTS /opt/homebrew/lib
                  /opt/homebrew/opt/mariadb-connector-c/lib
                  /usr/local/lib /usr/lib
                  /usr/lib/x86_64-linux-gnu
            DOC "MariaDB Connector/C 共享库（libmariadb）")
        if(MARIADB_INC_DIR AND MARIADB_LIB)
            message(STATUS "MariaDB Connector/C found (manual): ${MARIADB_LIB}")
            target_compile_definitions(lele-tools PRIVATE WITH_MYSQL)
            target_include_directories(lele-tools PRIVATE ${MARIADB_INC_DIR})
            target_link_libraries(lele-tools PRIVATE ${MARIADB_LIB})
            set(HAVE_MYSQL TRUE)
            set(USING_MARIADB_CLIENT TRUE)
        endif()
    endif()

    # 没找到 MariaDB Connector/C → 回退到 libmysqlclient
    if(NOT USING_MARIADB_CLIENT)
        find_package(MySQL QUIET)
        if(MySQL_FOUND)
            message(STATUS "MariaDB Connector/C not found; falling back to libmysqlclient")
            message(STATUS "  注意：MySQL 9 客户端不支持 mysql_native_password 认证")
            message(STATUS "  建议 brew install mariadb-connector-c 以获得最佳兼容性")
            target_compile_definitions(lele-tools PRIVATE WITH_MYSQL)
            if(TARGET MySQL::MySQL)
                target_link_libraries(lele-tools PRIVATE MySQL::MySQL)
            else()
                target_include_directories(lele-tools PRIVATE ${MySQL_INCLUDE_DIRS})
                target_link_libraries(lele-tools PRIVATE ${MySQL_LIBRARIES})
            endif()
            set(HAVE_MYSQL TRUE)
        else()
            message(WARNING "Neither MariaDB Connector/C nor libmysqlclient found - MySQL 支持已禁用")
        endif()
    endif()
else()
    message(STATUS "MySQL support disabled")
    set(HAVE_MYSQL FALSE)
endif()

# PostgreSQL libpq 支持（db-compare 模块用）
option(WITH_LIBPQ "Enable PostgreSQL libpq client (db-compare module)" ON)

if(WITH_LIBPQ)
    find_package(PostgreSQL QUIET)
    if(PostgreSQL_FOUND)
        message(STATUS "PostgreSQL libpq found: ${PostgreSQL_LIBRARIES}")
        target_compile_definitions(lele-tools PRIVATE WITH_LIBPQ)
        if(TARGET PostgreSQL::PostgreSQL)
            target_link_libraries(lele-tools PRIVATE PostgreSQL::PostgreSQL)
        else()
            target_include_directories(lele-tools PRIVATE ${PostgreSQL_INCLUDE_DIRS})
            target_link_libraries(lele-tools PRIVATE ${PostgreSQL_LIBRARIES})
        endif()
        set(HAVE_LIBPQ TRUE)
    else()
        message(STATUS "PostgreSQL libpq not found - db-compare 的 PostgreSQL 驱动会禁用")
        set(HAVE_LIBPQ FALSE)
    endif()
else()
    set(HAVE_LIBPQ FALSE)
endif()

# Oracle OCI 支持（可选 — Oracle Instant Client 许可限制，需用户自行安装）
# 启用方式：cmake -DWITH_ORACLE_OCI=ON ...
#   并通过环境变量 ORACLE_HOME（包含 sdk/include 和 lib）或显式
#   -DORACLE_OCI_INCLUDE_DIR=... -DORACLE_OCI_LIBRARY=... 指定路径
option(WITH_ORACLE_OCI "Enable Oracle OCI client (db-compare module)" OFF)

if(WITH_ORACLE_OCI)
    find_path(ORACLE_OCI_INCLUDE_DIR oci.h
        HINTS $ENV{ORACLE_HOME}/sdk/include
              $ENV{OCI_INC_DIR}
              /opt/oracle/instantclient/sdk/include
              /opt/oracle/instantclient_*/sdk/include
              /usr/include/oracle/*/client64
        DOC "Oracle OCI 头文件目录（含 oci.h）")
    find_library(ORACLE_OCI_LIBRARY
        NAMES clntsh oci
        HINTS $ENV{ORACLE_HOME}/lib
              $ENV{OCI_LIB_DIR}
              /opt/oracle/instantclient
              /opt/oracle/instantclient_*/
              /usr/lib/oracle/*/client64/lib
        DOC "Oracle OCI 库（libclntsh / oci.dll）")

    if(ORACLE_OCI_INCLUDE_DIR AND ORACLE_OCI_LIBRARY)
        message(STATUS "Oracle OCI found: ${ORACLE_OCI_LIBRARY}")
        target_compile_definitions(lele-tools PRIVATE WITH_ORACLE_OCI)
        target_include_directories(lele-tools PRIVATE ${ORACLE_OCI_INCLUDE_DIR})
        target_link_libraries(lele-tools PRIVATE ${ORACLE_OCI_LIBRARY})
        set(HAVE_ORACLE_OCI TRUE)
    else()
        message(STATUS "Oracle OCI 头/库未找到 — 跳过（db-compare 的 Oracle 驱动会显示未启用）")
        set(HAVE_ORACLE_OCI FALSE)
    endif()
else()
    set(HAVE_ORACLE_OCI FALSE)
endif()

# VTK 支持（科学可视化 demo 模块）
option(WITH_VTK "Enable VTK demo module (point cloud / surface / heatmap / slice / model compare)" ON)
if(WITH_VTK)
    # VTK 9.5+ 的目标依赖 JsonCpp::JsonCpp，需先 find_package
    find_package(jsoncpp QUIET)
    find_package(VTK 9 QUIET COMPONENTS
        CommonCore CommonDataModel CommonTransforms
        CommonComputationalGeometry CommonColor
        FiltersCore FiltersGeneral FiltersSources FiltersModeling FiltersGeometry
        RenderingCore RenderingOpenGL2 RenderingAnnotation RenderingFreeType
        RenderingVolumeOpenGL2 RenderingContextOpenGL2
        InteractionStyle InteractionWidgets
        ImagingCore ImagingSources ImagingHybrid
        GUISupportQt
    )
    if(VTK_FOUND)
        message(STATUS "VTK ${VTK_VERSION} found")
        target_compile_definitions(lele-tools PRIVATE WITH_VTK)
        target_link_libraries(lele-tools PRIVATE ${VTK_LIBRARIES})
        # VTK 9 需要显式调 vtk_module_autoinit 来注册工厂
        vtk_module_autoinit(TARGETS lele-tools MODULES ${VTK_LIBRARIES})
        set(HAVE_VTK TRUE)
    else()
        message(STATUS "VTK not found — VTK demo 模块会被禁用")
        set(HAVE_VTK FALSE)
    endif()
else()
    set(HAVE_VTK FALSE)
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

# LibSSH 支持 (SSH客户端和SFTP功能)
option(WITH_LIBSSH "Enable LibSSH support for SSH and SFTP" ON)

if(WITH_LIBSSH)
    find_package(LibSSH QUIET)
    if(LibSSH_FOUND)
        message(STATUS "LibSSH support enabled - SSH and SFTP functionality available")
        target_link_libraries(lele-tools PRIVATE LibSSH::LibSSH)
        target_include_directories(lele-tools PRIVATE ${LibSSH_INCLUDE_DIRS})
        target_compile_definitions(lele-tools PRIVATE WITH_LIBSSH)
        set(HAVE_LIBSSH TRUE)
    else()
        message(STATUS "LibSSH not found - SSH and SFTP tools will be disabled")
        message(STATUS "  To enable SSH functionality, install libssh-dev or set LibSSH_ROOT_DIR")
        target_compile_definitions(lele-tools PRIVATE WITHOUT_LIBSSH)
        set(HAVE_LIBSSH FALSE)
    endif()
else()
    message(STATUS "LibSSH support disabled")
    target_compile_definitions(lele-tools PRIVATE WITHOUT_LIBSSH)
    set(HAVE_LIBSSH FALSE)
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
