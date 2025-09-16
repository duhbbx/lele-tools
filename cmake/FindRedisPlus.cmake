# FindRedisPlus.cmake - Find redis-plus-plus library
# 
# This module finds the redis-plus-plus C++ client library for Redis
# 
# Variables defined by this module:
#   RedisPlus_FOUND         - True if redis-plus-plus was found
#   RedisPlus_INCLUDE_DIRS  - Include directories for redis-plus-plus
#   RedisPlus_LIBRARIES     - Libraries to link against
#   RedisPlus_VERSION       - Version of redis-plus-plus found
#
# You can set these variables to help the module find redis-plus-plus:
#   RedisPlus_ROOT_DIR      - Root directory of redis-plus-plus installation

# 尝试查找redis-plus-plus
find_path(RedisPlus_INCLUDE_DIR
    NAMES sw/redis++/redis++.h
    HINTS
        ${RedisPlus_ROOT_DIR}
        $ENV{RedisPlus_ROOT_DIR}
    PATHS
        /usr/include
        /usr/local/include
        /opt/local/include
        ${CMAKE_PREFIX_PATH}/include
        "C:/vcpkg/installed/x64-windows/include"
        "C:/redis-plus-plus/include"
    PATH_SUFFIXES
        redis-plus-plus
        sw
)

# 查找库文件
find_library(RedisPlus_LIBRARY
    NAMES redis++ redis-plus-plus
    HINTS
        ${RedisPlus_ROOT_DIR}
        $ENV{RedisPlus_ROOT_DIR}
    PATHS
        /usr/lib
        /usr/local/lib
        /opt/local/lib
        ${CMAKE_PREFIX_PATH}/lib
        "C:/vcpkg/installed/x64-windows/lib"
        "C:/redis-plus-plus/lib"
    PATH_SUFFIXES
        redis-plus-plus
        lib
)

# 查找hiredis依赖
find_path(HIREDIS_INCLUDE_DIR
    NAMES hiredis/hiredis.h
    HINTS
        ${RedisPlus_ROOT_DIR}
        $ENV{RedisPlus_ROOT_DIR}
    PATHS
        /usr/include
        /usr/local/include
        /opt/local/include
        ${CMAKE_PREFIX_PATH}/include
        "C:/vcpkg/installed/x64-windows/include"
        "C:/hiredis/include"
)

find_library(HIREDIS_LIBRARY
    NAMES hiredis
    HINTS
        ${RedisPlus_ROOT_DIR}
        $ENV{RedisPlus_ROOT_DIR}
    PATHS
        /usr/lib
        /usr/local/lib
        /opt/local/lib
        ${CMAKE_PREFIX_PATH}/lib
        "C:/vcpkg/installed/x64-windows/lib"
        "C:/hiredis/lib"
)

# 检查是否找到所有必需组件
if(RedisPlus_INCLUDE_DIR AND RedisPlus_LIBRARY AND HIREDIS_INCLUDE_DIR AND HIREDIS_LIBRARY)
    set(RedisPlus_FOUND TRUE)
    
    set(RedisPlus_INCLUDE_DIRS 
        ${RedisPlus_INCLUDE_DIR}
        ${HIREDIS_INCLUDE_DIR}
    )
    
    set(RedisPlus_LIBRARIES 
        ${RedisPlus_LIBRARY}
        ${HIREDIS_LIBRARY}
    )
    
    # 尝试获取版本信息
    if(EXISTS "${RedisPlus_INCLUDE_DIR}/sw/redis++/version.h")
        file(READ "${RedisPlus_INCLUDE_DIR}/sw/redis++/version.h" _version_header)
        string(REGEX MATCH "#define REDIS_PLUS_PLUS_VERSION \"([0-9]+\\.[0-9]+\\.[0-9]+)\"" _version_match "${_version_header}")
        if(_version_match)
            set(RedisPlus_VERSION ${CMAKE_MATCH_1})
        endif()
    endif()
    
    # 创建导入目标
    if(NOT TARGET RedisPlus::RedisPlus)
        add_library(RedisPlus::RedisPlus UNKNOWN IMPORTED)
        set_target_properties(RedisPlus::RedisPlus PROPERTIES
            IMPORTED_LOCATION ${RedisPlus_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES "${RedisPlus_INCLUDE_DIRS}"
            INTERFACE_LINK_LIBRARIES ${HIREDIS_LIBRARY}
        )
    endif()
else()
    set(RedisPlus_FOUND FALSE)
endif()

# 处理标准的find_package参数
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(RedisPlus
    FOUND_VAR RedisPlus_FOUND
    REQUIRED_VARS RedisPlus_LIBRARY RedisPlus_INCLUDE_DIR HIREDIS_LIBRARY HIREDIS_INCLUDE_DIR
    VERSION_VAR RedisPlus_VERSION
)

# 设置变量的可见性
mark_as_advanced(
    RedisPlus_INCLUDE_DIR
    RedisPlus_LIBRARY
    HIREDIS_INCLUDE_DIR
    HIREDIS_LIBRARY
)

if(RedisPlus_FOUND)
    message(STATUS "Found redis-plus-plus: ${RedisPlus_LIBRARY}")
    if(RedisPlus_VERSION)
        message(STATUS "redis-plus-plus version: ${RedisPlus_VERSION}")
    endif()
    message(STATUS "redis-plus-plus include dirs: ${RedisPlus_INCLUDE_DIRS}")
    message(STATUS "redis-plus-plus libraries: ${RedisPlus_LIBRARIES}")
else()
    message(STATUS "redis-plus-plus not found. You can:")
    message(STATUS "  1. Install via vcpkg: vcpkg install redis-plus-plus")
    message(STATUS "  2. Set RedisPlus_ROOT_DIR to installation directory")
    message(STATUS "  3. Build from source: https://github.com/sewenew/redis-plus-plus")
endif()



