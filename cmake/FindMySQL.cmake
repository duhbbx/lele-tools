# FindMySQL.cmake - Find MySQL Client library
#
# This module finds the MySQL Client C/C++ library
#
# Variables defined by this module:
#   MySQL_FOUND         - True if MySQL was found
#   MySQL_INCLUDE_DIRS  - Include directories for MySQL
#   MySQL_LIBRARIES     - Libraries to link against
#   MySQL_VERSION       - Version of MySQL found
#
# You can set these variables to help the module find MySQL:
#   MySQL_ROOT_DIR      - Root directory of MySQL installation

# 尝试查找MySQL
find_path(MySQL_INCLUDE_DIR
    NAMES mysql.h mysql/mysql.h mysqlx/xapi.h
    HINTS
        ${MySQL_ROOT_DIR}
        $ENV{MySQL_ROOT_DIR}
        ${MYSQL_INCLUDE_DIR}
        $ENV{MYSQL_INCLUDE_DIR}
    PATHS
        /usr/include
        /usr/local/include
        /usr/include/mysql
        /usr/local/include/mysql
        /opt/local/include
        /opt/local/include/mysql
        ${CMAKE_PREFIX_PATH}/include
        ${CMAKE_PREFIX_PATH}/include/mysql
        "C:/vcpkg/installed/x64-windows/include"
        "C:/vcpkg/installed/x64-windows/include/mysql"
        "C:/Program Files/MySQL/MySQL Server 8.0/include"
        "C:/Program Files/MySQL/MySQL Server 5.7/include"
        "C:/Program Files (x86)/MySQL/MySQL Server 8.0/include"
        "C:/Program Files (x86)/MySQL/MySQL Server 5.7/include"
        "C:/mysql/include"
        "C:/mysql-connector/include"
    PATH_SUFFIXES
        mysql
        include
        include/mysql
        include/mysqlx
        include/jdbc
)

# 查找库文件
find_library(MySQL_LIBRARY
    NAMES mysqlclient mysql libmysql libmysqlclient mysqlcppconn8 mysqlcppconn
    HINTS
        ${MySQL_ROOT_DIR}
        $ENV{MySQL_ROOT_DIR}
        ${MYSQL_LIBRARY}
        $ENV{MYSQL_LIBRARY}
    PATHS
        /usr/lib
        /usr/local/lib
        /usr/lib/x86_64-linux-gnu
        /usr/lib/mysql
        /usr/local/lib/mysql
        /opt/local/lib
        /opt/local/lib/mysql
        ${CMAKE_PREFIX_PATH}/lib
        "C:/vcpkg/installed/x64-windows/lib"
        "C:/Program Files/MySQL/MySQL Server 8.0/lib"
        "C:/Program Files/MySQL/MySQL Server 5.7/lib"
        "C:/Program Files (x86)/MySQL/MySQL Server 8.0/lib"
        "C:/Program Files (x86)/MySQL/MySQL Server 5.7/lib"
        "C:/mysql/lib"
        "C:/mysql-connector/lib64"
    PATH_SUFFIXES
        mysql
        lib
        lib64
)

# Windows上可能需要额外的库
if(WIN32)
    find_library(MySQL_SSL_LIBRARY
        NAMES ssleay32 libssl
        HINTS
            ${MySQL_ROOT_DIR}
            $ENV{MySQL_ROOT_DIR}
        PATHS
            ${CMAKE_PREFIX_PATH}/lib
            "C:/vcpkg/installed/x64-windows/lib"
            "C:/Program Files/MySQL/MySQL Server 8.0/lib"
            "C:/Program Files/MySQL/MySQL Server 5.7/lib"
            "C:/mysql-connector/lib64"
    )

    find_library(MySQL_CRYPTO_LIBRARY
        NAMES libeay32 libcrypto
        HINTS
            ${MySQL_ROOT_DIR}
            $ENV{MySQL_ROOT_DIR}
        PATHS
            ${CMAKE_PREFIX_PATH}/lib
            "C:/vcpkg/installed/x64-windows/lib"
            "C:/Program Files/MySQL/MySQL Server 8.0/lib"
            "C:/Program Files/MySQL/MySQL Server 5.7/lib"
            "C:/mysql-connector/lib64"
    )
endif()

# 如果没有找到，尝试更灵活的搜索方式
if(NOT MySQL_INCLUDE_DIR OR NOT MySQL_LIBRARY)
    message(STATUS "Standard MySQL search failed, trying flexible search...")

    # 尝试查找MySQL Connector/C++的头文件
    find_path(MySQL_INCLUDE_DIR
        NAMES mysqlx/xapi.h jdbc/mysql_connection.h mysql.h
        PATHS
            "C:/mysql-connector/include"
            ${CMAKE_PREFIX_PATH}/include
            "C:/vcpkg/installed/x64-windows/include"
        NO_DEFAULT_PATH
    )

    # 尝试查找MySQL Connector/C++的库文件
    find_library(MySQL_LIBRARY
        NAMES
            mysqlcppconn8
            mysqlcppconn
            mysqlcppconn-10-vs14
            mysqlcppconnx-2-vs14
        PATHS
            "C:/mysql-connector/lib64"
            "C:/mysql-connector/lib"
            ${CMAKE_PREFIX_PATH}/lib
            "C:/vcpkg/installed/x64-windows/lib"
        NO_DEFAULT_PATH
    )

    # 如果没有找到.lib文件，尝试查找DLL文件（用于动态链接）
    if(NOT MySQL_LIBRARY)
        find_file(MySQL_DLL
            NAMES
                mysqlcppconn-10-vs14.dll
                mysqlcppconnx-2-vs14.dll
                mysqlcppconn8.dll
                mysqlcppconn.dll
            PATHS
                "C:/mysql-connector/lib64"
                "C:/mysql-connector/bin"
                ${CMAKE_PREFIX_PATH}/bin
                ${CMAKE_PREFIX_PATH}/lib
            NO_DEFAULT_PATH
        )

        if(MySQL_DLL)
            # 使用DLL文件路径作为库文件（某些项目可能支持直接链接DLL）
            set(MySQL_LIBRARY ${MySQL_DLL})
            message(STATUS "Found MySQL DLL: ${MySQL_DLL}")
        endif()
    endif()

    if(MySQL_INCLUDE_DIR AND MySQL_LIBRARY)
        message(STATUS "Found MySQL using flexible search")
    endif()
endif()

# 检查是否找到所有必需组件
if(MySQL_INCLUDE_DIR AND MySQL_LIBRARY)
    set(MySQL_FOUND TRUE)

    set(MySQL_INCLUDE_DIRS ${MySQL_INCLUDE_DIR})
    set(MySQL_LIBRARIES ${MySQL_LIBRARY})

    # Windows上添加额外库
    if(WIN32)
        if(MySQL_SSL_LIBRARY)
            list(APPEND MySQL_LIBRARIES ${MySQL_SSL_LIBRARY})
        endif()
        if(MySQL_CRYPTO_LIBRARY)
            list(APPEND MySQL_LIBRARIES ${MySQL_CRYPTO_LIBRARY})
        endif()
        # Windows系统库
        list(APPEND MySQL_LIBRARIES ws2_32 advapi32 secur32)
    endif()

    # 尝试获取版本信息
    if(EXISTS "${MySQL_INCLUDE_DIR}/mysql_version.h")
        file(READ "${MySQL_INCLUDE_DIR}/mysql_version.h" _version_header)
        string(REGEX MATCH "#define MYSQL_SERVER_VERSION[ \t]+\"([0-9]+\\.[0-9]+\\.[0-9]+)\"" _version_match "${_version_header}")
        if(_version_match)
            set(MySQL_VERSION ${CMAKE_MATCH_1})
        else()
            # 尝试另一种版本格式
            string(REGEX MATCH "#define MYSQL_VERSION_ID[ \t]+([0-9]+)" _version_id_match "${_version_header}")
            if(_version_id_match)
                set(MySQL_VERSION_ID ${CMAKE_MATCH_1})
                # 转换版本ID为版本字符串 (例如 80032 -> 8.0.32)
                math(EXPR _major "${MySQL_VERSION_ID} / 10000")
                math(EXPR _minor "(${MySQL_VERSION_ID} % 10000) / 100")
                math(EXPR _patch "${MySQL_VERSION_ID} % 100")
                set(MySQL_VERSION "${_major}.${_minor}.${_patch}")
            endif()
        endif()
    endif()

    # 创建导入目标
    if(NOT TARGET MySQL::MySQL)
        add_library(MySQL::MySQL UNKNOWN IMPORTED)
        set_target_properties(MySQL::MySQL PROPERTIES
            IMPORTED_LOCATION ${MySQL_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES "${MySQL_INCLUDE_DIRS}"
        )

        # Windows上设置额外的链接库
        if(WIN32)
            set_target_properties(MySQL::MySQL PROPERTIES
                INTERFACE_LINK_LIBRARIES "${MySQL_SSL_LIBRARY};${MySQL_CRYPTO_LIBRARY};ws2_32;advapi32;secur32"
            )
        endif()
    endif()
else()
    set(MySQL_FOUND FALSE)
endif()

# 处理标准的find_package参数
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MySQL
    FOUND_VAR MySQL_FOUND
    REQUIRED_VARS MySQL_LIBRARY MySQL_INCLUDE_DIR
    VERSION_VAR MySQL_VERSION
)

# 设置变量的可见性
mark_as_advanced(
    MySQL_INCLUDE_DIR
    MySQL_LIBRARY
    MySQL_SSL_LIBRARY
    MySQL_CRYPTO_LIBRARY
)

if(MySQL_FOUND)
    message(STATUS "Found MySQL: ${MySQL_LIBRARY}")
    if(MySQL_VERSION)
        message(STATUS "MySQL version: ${MySQL_VERSION}")
    endif()
    message(STATUS "MySQL include dirs: ${MySQL_INCLUDE_DIRS}")
    message(STATUS "MySQL libraries: ${MySQL_LIBRARIES}")
else()
    message(STATUS "MySQL not found. You can:")
    message(STATUS "  1. Install via vcpkg: vcpkg install libmysql")
    message(STATUS "  2. Install MySQL Server with development headers")
    message(STATUS "  3. Set MySQL_ROOT_DIR to installation directory")
    message(STATUS "  4. Set MYSQL_INCLUDE_DIR and MYSQL_LIBRARY manually")
endif()