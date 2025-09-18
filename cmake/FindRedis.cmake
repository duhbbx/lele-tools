# FindRedis.cmake - Find redis-plus-plus library
#
# This module defines:
#  Redis_FOUND           - True if redis-plus-plus is found
#  Redis_INCLUDE_DIRS    - Include directories for redis-plus-plus
#  Redis_LIBRARIES       - Libraries to link for redis-plus-plus
#  Redis::Redis          - Imported target for redis-plus-plus
#
# Environment variables:
#  REDIS_PLUS_PLUS_INCLUDE_DIR - Include directory hint
#  REDIS_PLUS_PLUS_LIBRARY     - Library path hint
#  HIREDIS_LIBRARY             - Hiredis library path hint

include(FindPackageHandleStandardArgs)

# 初始化变量
set(Redis_FOUND FALSE)
set(Redis_INCLUDE_DIRS "")
set(Redis_LIBRARIES "")

# 获取环境变量提示和从外部传入的变量
if(DEFINED ENV{REDIS_PLUS_PLUS_INCLUDE_DIR})
    set(REDIS_PLUS_PLUS_INCLUDE_DIR_HINT $ENV{REDIS_PLUS_PLUS_INCLUDE_DIR})
elseif(REDIS_PLUS_PLUS_INCLUDE_DIR)
    set(REDIS_PLUS_PLUS_INCLUDE_DIR_HINT ${REDIS_PLUS_PLUS_INCLUDE_DIR})
endif()

if(DEFINED ENV{REDIS_PLUS_PLUS_LIBRARY})
    set(REDIS_PLUS_PLUS_LIBRARY_HINT $ENV{REDIS_PLUS_PLUS_LIBRARY})
elseif(REDIS_PLUS_PLUS_LIBRARY)
    set(REDIS_PLUS_PLUS_LIBRARY_HINT ${REDIS_PLUS_PLUS_LIBRARY})
endif()

if(DEFINED ENV{HIREDIS_LIBRARY})
    set(HIREDIS_LIBRARY_HINT $ENV{HIREDIS_LIBRARY})
elseif(HIREDIS_LIBRARY)
    set(HIREDIS_LIBRARY_HINT ${HIREDIS_LIBRARY})
endif()

# 设置默认路径（特别是vcpkg）
if(NOT REDIS_PLUS_PLUS_INCLUDE_DIR_HINT)
    if(DEFINED ENV{VCPKG_ROOT})
        if(WIN32)
            set(REDIS_PLUS_PLUS_INCLUDE_DIR_HINT "$ENV{VCPKG_ROOT}/installed/x64-windows/include")
            set(REDIS_PLUS_PLUS_LIBRARY_HINT "$ENV{VCPKG_ROOT}/installed/x64-windows/lib/redis++.lib")
            set(HIREDIS_LIBRARY_HINT "$ENV{VCPKG_ROOT}/installed/x64-windows/lib/hiredis.lib")
        elseif(UNIX AND NOT APPLE)
            set(REDIS_PLUS_PLUS_INCLUDE_DIR_HINT "$ENV{VCPKG_ROOT}/installed/x64-linux/include")
            set(REDIS_PLUS_PLUS_LIBRARY_HINT "$ENV{VCPKG_ROOT}/installed/x64-linux/lib/libredis++.a")
            set(HIREDIS_LIBRARY_HINT "$ENV{VCPKG_ROOT}/installed/x64-linux/lib/libhiredis.a")
        elseif(APPLE)
            set(REDIS_PLUS_PLUS_INCLUDE_DIR_HINT "$ENV{VCPKG_ROOT}/installed/arm64-osx/include")
            set(REDIS_PLUS_PLUS_LIBRARY_HINT "$ENV{VCPKG_ROOT}/installed/arm64-osx/lib/libredis++.a")
            set(HIREDIS_LIBRARY_HINT "$ENV{VCPKG_ROOT}/installed/arm64-osx/lib/libhiredis.a")
        endif()
    # 尝试在D:/projects/vcpkg路径
    elseif(EXISTS "D:/projects/vcpkg" AND WIN32)
        set(REDIS_PLUS_PLUS_INCLUDE_DIR_HINT "D:/projects/vcpkg/installed/x64-windows/include")
        set(REDIS_PLUS_PLUS_LIBRARY_HINT "D:/projects/vcpkg/installed/x64-windows/lib/redis++.lib")
        set(HIREDIS_LIBRARY_HINT "D:/projects/vcpkg/installed/x64-windows/lib/hiredis.lib")
    endif()
endif()

# 采用改进的检测逻辑（基于原来的工作方式）
message(STATUS "Checking for redis-plus-plus ...")

message(STATUS "REDIS_PLUS_PLUS_INCLUDE_DIR = ${REDIS_PLUS_PLUS_INCLUDE_DIR_HINT}")
message(STATUS "REDIS_PLUS_PLUS_LIBRARY     = ${REDIS_PLUS_PLUS_LIBRARY_HINT}")
message(STATUS "HIREDIS_LIBRARY             = ${HIREDIS_LIBRARY_HINT}")

# 尝试多种方式查找redis-plus-plus
find_package(redis++ QUIET CONFIG)

if(redis++_FOUND)
    message(STATUS "redis-plus-plus found via CONFIG")
    set(Redis_FOUND TRUE)
    set(HAVE_REDIS TRUE)

    # 创建别名以保持一致性
    if(NOT TARGET Redis::Redis)
        add_library(Redis::Redis ALIAS redis++::redis++)
    endif()
else()
    # 如果CONFIG方式失败，尝试手动查找
    # 检查文件是否存在
    if(EXISTS "${REDIS_PLUS_PLUS_INCLUDE_DIR_HINT}/sw/redis++/redis++.h" AND
       EXISTS "${REDIS_PLUS_PLUS_LIBRARY_HINT}" AND
       EXISTS "${HIREDIS_LIBRARY_HINT}")

        message(STATUS "redis-plus-plus found at: ${REDIS_PLUS_PLUS_INCLUDE_DIR_HINT}")
        message(STATUS "redis++ library: ${REDIS_PLUS_PLUS_LIBRARY_HINT}")
        message(STATUS "hiredis library: ${HIREDIS_LIBRARY_HINT}")

        set(Redis_FOUND TRUE)
        set(HAVE_REDIS TRUE)
        set(Redis_INCLUDE_DIRS ${REDIS_PLUS_PLUS_INCLUDE_DIR_HINT})
        set(Redis_LIBRARIES ${REDIS_PLUS_PLUS_LIBRARY_HINT} ${HIREDIS_LIBRARY_HINT})

        # 创建导入目标（使用原来的方式）
        if(NOT TARGET Redis::Redis)
            add_library(Redis::Redis UNKNOWN IMPORTED)
            set_target_properties(Redis::Redis PROPERTIES
                IMPORTED_LOCATION ${REDIS_PLUS_PLUS_LIBRARY_HINT}
                INTERFACE_INCLUDE_DIRECTORIES ${REDIS_PLUS_PLUS_INCLUDE_DIR_HINT}
                INTERFACE_LINK_LIBRARIES ${HIREDIS_LIBRARY_HINT}
            )
            message(STATUS "redis-plus-plus found manually")
            message(STATUS "  Include: ${REDIS_PLUS_PLUS_INCLUDE_DIR_HINT}")
            message(STATUS "  Library: ${REDIS_PLUS_PLUS_LIBRARY_HINT}")
        endif()
    else()
        set(REDIS_PLUS_PLUS_INCLUDE_DIR_HINT "")
        set(REDIS_PLUS_PLUS_LIBRARY_HINT "")
        set(HIREDIS_LIBRARY_HINT "")

        message(WARNING "redis-plus-plus not found. Building without Redis support.")
        message(STATUS "To install redis-plus-plus:")
        message(STATUS "  Windows: vcpkg install redis-plus-plus")
        message(STATUS "  Linux: sudo apt-get install libhiredis-dev libsw-redis++-dev")
        message(STATUS "  macOS: brew install redis-plus-plus")
        set(Redis_FOUND FALSE)
        set(HAVE_REDIS FALSE)
    endif()
endif()

# 使用标准的结果处理
if(Redis_FOUND)
    find_package_handle_standard_args(Redis
        FOUND_VAR Redis_FOUND
        REQUIRED_VARS Redis_FOUND
    )
else()
    find_package_handle_standard_args(Redis
        FOUND_VAR Redis_FOUND
        REQUIRED_VARS REDIS_PLUS_PLUS_INCLUDE_DIR_HINT REDIS_PLUS_PLUS_LIBRARY_HINT
        FAIL_MESSAGE "Could not find redis-plus-plus library. Please install it via package manager or set REDIS_PLUS_PLUS_INCLUDE_DIR and REDIS_PLUS_PLUS_LIBRARY."
    )
endif()

# 提供安装提示
if(NOT Redis_FOUND)
    message(STATUS "To install redis-plus-plus:")
    if(WIN32)
        message(STATUS "  Windows: vcpkg install redis-plus-plus")
    elseif(UNIX AND NOT APPLE)
        message(STATUS "  Ubuntu/Debian: sudo apt-get install libhiredis-dev libsw-redis++-dev")
        message(STATUS "  CentOS/RHEL: sudo yum install hiredis-devel")
        message(STATUS "  Or build from source: https://github.com/sewenew/redis-plus-plus")
    elseif(APPLE)
        message(STATUS "  macOS: brew install redis-plus-plus")
    endif()
    message(STATUS "  Or use vcpkg: vcpkg install redis-plus-plus")
endif()

# Export variables for compatibility
if(Redis_FOUND)
    set(REDIS_FOUND TRUE)
endif()

# Mark cache variables as advanced
mark_as_advanced(REDIS_PLUS_PLUS_INCLUDE_DIR_HINT REDIS_PLUS_PLUS_LIBRARY_HINT HIREDIS_LIBRARY_HINT)