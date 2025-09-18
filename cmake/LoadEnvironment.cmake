# LoadEnvironment.cmake - Load environment variables from .env files
#
# This module provides a function to load environment variables from .env files
# and automatically reconfigure when the file changes.
#
# Usage:
#   load_environment_file(ENVIRONMENT_NAME)
#
# Parameters:
#   ENVIRONMENT_NAME - The environment name suffix (e.g., "台式机" for .env.台式机)
#
# Variables set:
#   ENV_FILE_LOADED - TRUE if an environment file was successfully loaded
#   ENV_FILE_PATH   - Path to the loaded environment file

function(load_environment_file ENVIRONMENT_NAME)
    # 构建 .env 文件路径
    set(ENV_FILE "${CMAKE_SOURCE_DIR}/.env.${ENVIRONMENT_NAME}")

    # 将文件设置为 CMake 配置依赖
    # 这样当文件修改时会触发重新配置
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${ENV_FILE}")

    # 同时检查通用的 .env 文件
    set(DEFAULT_ENV_FILE "${CMAKE_SOURCE_DIR}/.env")
    if(EXISTS "${DEFAULT_ENV_FILE}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${DEFAULT_ENV_FILE}")
    endif()

    # 清空之前的环境变量（如果需要的话）
    set(ENV_FILE_LOADED FALSE PARENT_SCOPE)
    set(ENV_FILE_PATH "" PARENT_SCOPE)

    # 优先级：.env.环境名 > .env
    set(ENV_FILES_TO_TRY "${ENV_FILE}" "${DEFAULT_ENV_FILE}")

    foreach(env_file_to_try IN LISTS ENV_FILES_TO_TRY)
        if(EXISTS "${env_file_to_try}")
            message(STATUS "Loading environment from ${env_file_to_try}")

            # 获取文件修改时间用于缓存验证
            file(TIMESTAMP "${env_file_to_try}" ENV_FILE_TIMESTAMP)

            # 读取文件内容
            file(STRINGS "${env_file_to_try}" ENV_LINES)

            # 解析环境变量
            foreach(line IN LISTS ENV_LINES)
                # 跳过空行和注释行
                if(line MATCHES "^\\s*#" OR line MATCHES "^\\s*$")
                    continue()
                endif()

                # 解析 KEY=VALUE 格式
                if(line MATCHES "^\\s*([^=]+)=(.*)$")
                    string(STRIP "${CMAKE_MATCH_1}" VAR_NAME)
                    string(STRIP "${CMAKE_MATCH_2}" VAR_VALUE)

                    # 移除引号（如果存在）
                    if(VAR_VALUE MATCHES "^[\"'](.*)[\"\']$")
                        set(VAR_VALUE "${CMAKE_MATCH_1}")
                    endif()

                    # 展开变量引用 (支持 ${VAR} 格式)
                    string(CONFIGURE "${VAR_VALUE}" VAR_VALUE @ONLY)

                    # 设置变量
                    set(${VAR_NAME} "${VAR_VALUE}" PARENT_SCOPE)

                    # 同时设置为缓存变量（可选，用于持久化）
                    set(${VAR_NAME} "${VAR_VALUE}" CACHE STRING "Variable from ${env_file_to_try}" FORCE)

                    # 输出加载的变量（可选择性隐藏敏感信息）
                    if(VAR_NAME MATCHES "PASSWORD|SECRET|KEY|TOKEN")
                        message(STATUS "  ${VAR_NAME} = [HIDDEN]")
                    else()
                        message(STATUS "  ${VAR_NAME} = ${VAR_VALUE}")
                    endif()
                endif()
            endforeach()

            set(ENV_FILE_LOADED TRUE PARENT_SCOPE)
            set(ENV_FILE_PATH "${env_file_to_try}" PARENT_SCOPE)

            # 记录文件时间戳用于验证
            set(ENV_FILE_TIMESTAMP_CACHE "${ENV_FILE_TIMESTAMP}" CACHE INTERNAL
                "Timestamp of loaded environment file")

            message(STATUS "Environment loaded successfully from ${env_file_to_try}")
            break()
        endif()
    endforeach()

    if(NOT ENV_FILE_LOADED)
        message(STATUS "No environment file found for '${ENVIRONMENT_NAME}'")
        message(STATUS "Tried: ${ENV_FILES_TO_TRY}")
    endif()
endfunction()

# 辅助函数：重新加载环境文件（如果已修改）
function(check_and_reload_environment)
    if(DEFINED ENV_FILE_PATH AND EXISTS "${ENV_FILE_PATH}")
        file(TIMESTAMP "${ENV_FILE_PATH}" CURRENT_TIMESTAMP)
        if(NOT "${CURRENT_TIMESTAMP}" STREQUAL "${ENV_FILE_TIMESTAMP_CACHE}")
            message(STATUS "Environment file has been modified, triggering reconfiguration...")
            # CMake 会自动重新配置，因为文件在 CMAKE_CONFIGURE_DEPENDS 中
        endif()
    endif()
endfunction()

# 辅助函数：验证必需的环境变量
function(require_environment_variables)
    foreach(var_name IN LISTS ARGN)
        if(NOT DEFINED ${var_name} OR "${${var_name}}" STREQUAL "")
            message(FATAL_ERROR "Required environment variable '${var_name}' is not set!")
        endif()
    endforeach()
endfunction()

# 辅助函数：设置默认值（如果变量未定义）
function(set_environment_default VAR_NAME DEFAULT_VALUE)
    if(NOT DEFINED ${VAR_NAME} OR "${${VAR_NAME}}" STREQUAL "")
        set(${VAR_NAME} "${DEFAULT_VALUE}" PARENT_SCOPE)
        set(${VAR_NAME} "${DEFAULT_VALUE}" CACHE STRING "Default value" FORCE)
        message(STATUS "  ${VAR_NAME} = ${DEFAULT_VALUE} (default)")
    endif()
endfunction()