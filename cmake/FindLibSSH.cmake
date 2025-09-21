# FindLibSSH.cmake - Find libssh library
#
# This module finds the libssh C library for SSH connections
#
# Variables defined by this module:
#   LibSSH_FOUND         - True if libssh was found
#   LibSSH_INCLUDE_DIRS  - Include directories for libssh
#   LibSSH_LIBRARIES     - Libraries to link against
#   LibSSH_VERSION       - Version of libssh found
#
# You can set these variables to help the module find libssh:
#   LibSSH_ROOT_DIR      - Root directory of libssh installation
#   LIBSSH_INCLUDE_DIR   - Include directory path
#   LIBSSH_LIBRARY       - Library path

include(FindPackageHandleStandardArgs)

# Initialize variables
set(LibSSH_FOUND FALSE)
set(LibSSH_INCLUDE_DIRS "")
set(LibSSH_LIBRARIES "")

# Allow manual specification of libssh paths
set(LibSSH_ROOT_DIR "" CACHE PATH "Path to libssh root directory")

# Platform-specific detection
if(WIN32)
    message(STATUS "Looking for libssh on Windows...")

    # Get hints from environment variables and cache
    set(LIBSSH_HINTS "")

    # Prioritize manually specified path
    if(LibSSH_ROOT_DIR)
        list(APPEND LIBSSH_HINTS "${LibSSH_ROOT_DIR}")
        message(STATUS "Using manually specified LibSSH_ROOT_DIR: ${LibSSH_ROOT_DIR}")
    endif()

    if(DEFINED ENV{LIBSSH_ROOT})
        list(APPEND LIBSSH_HINTS "$ENV{LIBSSH_ROOT}")
    endif()

    if(DEFINED ENV{LIBSSH_DIR})
        list(APPEND LIBSSH_HINTS "$ENV{LIBSSH_DIR}")
    endif()

    # Add common installation paths (vcpkg优先)
    list(APPEND LIBSSH_HINTS
        # vcpkg installation paths (优先使用vcpkg)
        "${CMAKE_PREFIX_PATH}"
        "$ENV{VCPKG_INSTALLED_DIR}/x64-windows"
        "$ENV{VCPKG_ROOT}/installed/x64-windows"
        "C:/vcpkg/installed/x64-windows"
        "D:/vcpkg/installed/x64-windows"
        "D:/projects/vcpkg/installed/x64-windows"

        # GitHub Actions CI environment with vcpkg
        "D:/a/lele-tools/lele-tools/vcpkg/installed/x64-windows"
        "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows"

        # 备用手动安装路径
        "C:/libssh"
        "C:/libssh-0.10.6"
        "C:/Program Files/libssh"
        "C:/Program Files (x86)/libssh"

        # Project dependency paths
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libssh"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/libssh"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/libssh"
    )

    # Auto-detect libssh directories in common locations
    set(AUTO_DETECT_PATHS
        "C:/"
        "D:/"
        "${CMAKE_SOURCE_DIR}/.."
        "${CMAKE_SOURCE_DIR}/../.."
    )

    foreach(base_path IN LISTS AUTO_DETECT_PATHS)
        if(EXISTS "${base_path}")
            file(GLOB libssh_dirs "${base_path}/libssh*")
            foreach(dir IN LISTS libssh_dirs)
                if(IS_DIRECTORY "${dir}")
                    list(APPEND LIBSSH_HINTS "${dir}")
                endif()
            endforeach()
        endif()
    endforeach()

    # Debug: Print search paths (only if CMAKE_FIND_DEBUG_MODE is enabled)
    if(CMAKE_FIND_DEBUG_MODE)
        message(STATUS "Searching for libssh in the following locations:")
        foreach(hint IN LISTS LIBSSH_HINTS)
            message(STATUS "  - ${hint}")
        endforeach()
    endif()

    # Find libssh include directory
    find_path(LIBSSH_INCLUDE_DIR
        NAMES libssh/libssh.h
        HINTS
            ${LIBSSH_INCLUDE_DIR}  # Allow manual override
            ${LIBSSH_HINTS}
        PATH_SUFFIXES include
        DOC "libssh include directory"
    )

    if(CMAKE_FIND_DEBUG_MODE)
        if(LIBSSH_INCLUDE_DIR)
            message(STATUS "Found libssh include directory: ${LIBSSH_INCLUDE_DIR}")
        else()
            message(STATUS "libssh include directory not found")
        endif()
    endif()

    # Determine library architecture
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(LIBSSH_LIB_SUFFIX "x64")
        set(LIBSSH_LIB_ARCH_PATHS "lib64" "lib/x64" "lib")
    else()
        set(LIBSSH_LIB_SUFFIX "x86")
        set(LIBSSH_LIB_ARCH_PATHS "lib" "lib/x86")
    endif()

    # Find libssh library
    if(CMAKE_FIND_DEBUG_MODE)
        message(STATUS "Searching for libssh library with architecture suffix: ${LIBSSH_LIB_SUFFIX}")
    endif()

    find_library(LIBSSH_LIBRARY
        NAMES ssh libssh ssh_static libssh_static
        HINTS
            ${LIBSSH_LIBRARY}  # Allow manual override
            ${LIBSSH_HINTS}
        PATH_SUFFIXES
            lib
            lib64
            ${LIBSSH_LIB_ARCH_PATHS}
        DOC "libssh library"
    )

    if(CMAKE_FIND_DEBUG_MODE)
        if(LIBSSH_LIBRARY)
            message(STATUS "Found libssh library: ${LIBSSH_LIBRARY}")
        else()
            message(STATUS "libssh library not found")
        endif()
    endif()

    # Check if we found the required components
    if(LIBSSH_INCLUDE_DIR AND LIBSSH_LIBRARY)
        set(LibSSH_FOUND TRUE)
        set(LibSSH_INCLUDE_DIRS ${LIBSSH_INCLUDE_DIR})
        set(LibSSH_LIBRARIES ${LIBSSH_LIBRARY})

        # On Windows, we may need additional system libraries
        if(WIN32)
            list(APPEND LibSSH_LIBRARIES ws2_32 advapi32)
        endif()

        message(STATUS "libssh found:")
        message(STATUS "  Include: ${LIBSSH_INCLUDE_DIR}")
        message(STATUS "  Library: ${LIBSSH_LIBRARY}")

        # Try to get version information
        if(EXISTS "${LIBSSH_INCLUDE_DIR}/libssh/libssh_version.h")
            file(READ "${LIBSSH_INCLUDE_DIR}/libssh/libssh_version.h" _version_header)
            string(REGEX MATCH "#define LIBSSH_VERSION_MAJOR[ \\t]+([0-9]+)" _version_major_match "${_version_header}")
            string(REGEX MATCH "#define LIBSSH_VERSION_MINOR[ \\t]+([0-9]+)" _version_minor_match "${_version_header}")
            string(REGEX MATCH "#define LIBSSH_VERSION_MICRO[ \\t]+([0-9]+)" _version_micro_match "${_version_header}")

            if(_version_major_match AND _version_minor_match AND _version_micro_match)
                string(REGEX REPLACE ".*([0-9]+).*" "\\1" _version_major "${_version_major_match}")
                string(REGEX REPLACE ".*([0-9]+).*" "\\1" _version_minor "${_version_minor_match}")
                string(REGEX REPLACE ".*([0-9]+).*" "\\1" _version_micro "${_version_micro_match}")
                set(LibSSH_VERSION "${_version_major}.${_version_minor}.${_version_micro}")
            endif()
        endif()

        # Create imported target
        if(NOT TARGET LibSSH::LibSSH)
            add_library(LibSSH::LibSSH UNKNOWN IMPORTED)
            set_target_properties(LibSSH::LibSSH PROPERTIES
                IMPORTED_LOCATION ${LIBSSH_LIBRARY}
                INTERFACE_INCLUDE_DIRECTORIES "${LibSSH_INCLUDE_DIRS}"
                INTERFACE_COMPILE_DEFINITIONS "WITH_LIBSSH"
            )

            # Windows system libraries
            if(WIN32)
                set_target_properties(LibSSH::LibSSH PROPERTIES
                    INTERFACE_LINK_LIBRARIES "ws2_32;advapi32"
                )
            endif()
        endif()
    endif()

else()
    # Unix-like systems (Linux, macOS, BSD, etc.)
    message(STATUS "Looking for libssh on Unix-like system...")

    # Try pkg-config first
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(PC_LIBSSH QUIET libssh)
    endif()

    # Find libssh include directory
    find_path(LIBSSH_INCLUDE_DIR
        NAMES libssh/libssh.h
        HINTS
            ${PC_LIBSSH_INCLUDEDIR}
            ${PC_LIBSSH_INCLUDE_DIRS}
            ${LIBSSH_INCLUDE_DIR}  # Allow manual override
            $ENV{LIBSSH_ROOT}/include
        PATHS
            /usr/include
            /usr/local/include
            /opt/local/include
            /sw/include
        DOC "libssh include directory"
    )

    # Find libssh library
    find_library(LIBSSH_LIBRARY
        NAMES ssh libssh
        HINTS
            ${PC_LIBSSH_LIBDIR}
            ${PC_LIBSSH_LIBRARY_DIRS}
            ${LIBSSH_LIBRARY}  # Allow manual override
            $ENV{LIBSSH_ROOT}/lib
        PATHS
            /usr/lib
            /usr/local/lib
            /opt/local/lib
            /sw/lib
            /usr/lib/x86_64-linux-gnu
            /usr/lib/aarch64-linux-gnu
        DOC "libssh library"
    )

    # Check if we found the required components
    if(LIBSSH_INCLUDE_DIR AND LIBSSH_LIBRARY)
        set(LibSSH_FOUND TRUE)
        set(LibSSH_INCLUDE_DIRS ${LIBSSH_INCLUDE_DIR})
        set(LibSSH_LIBRARIES ${LIBSSH_LIBRARY})

        message(STATUS "libssh found:")
        message(STATUS "  Include: ${LIBSSH_INCLUDE_DIR}")
        message(STATUS "  Library: ${LIBSSH_LIBRARY}")

        # Try to get version from pkg-config
        if(PC_LIBSSH_VERSION)
            set(LibSSH_VERSION ${PC_LIBSSH_VERSION})
        endif()

        # Create imported target
        if(NOT TARGET LibSSH::LibSSH)
            add_library(LibSSH::LibSSH UNKNOWN IMPORTED)
            set_target_properties(LibSSH::LibSSH PROPERTIES
                IMPORTED_LOCATION "${LIBSSH_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${LIBSSH_INCLUDE_DIRS}"
                INTERFACE_COMPILE_DEFINITIONS "WITH_LIBSSH"
            )
        endif()
    endif()
endif()

# Handle standard arguments
find_package_handle_standard_args(LibSSH
    FOUND_VAR LibSSH_FOUND
    REQUIRED_VARS LibSSH_INCLUDE_DIRS LibSSH_LIBRARIES
    VERSION_VAR LibSSH_VERSION
    FAIL_MESSAGE "Could not find libssh library. Install libssh development package or set LibSSH_ROOT_DIR."
)

# Provide installation instructions if not found
if(NOT LibSSH_FOUND)
    message(STATUS "To install libssh:")
    if(WIN32)
        message(STATUS "  Windows:")
        message(STATUS "    1. Download libssh from: https://www.libssh.org/")
        message(STATUS "    2. Extract to C:/libssh or set LibSSH_ROOT_DIR")
        message(STATUS "    3. Or use vcpkg: vcpkg install libssh")
        message(STATUS "    4. Set LIBSSH_INCLUDE_DIR and LIBSSH_LIBRARY manually if needed")
    elseif(APPLE)
        message(STATUS "  macOS:")
        message(STATUS "    brew install libssh")
        message(STATUS "    or: port install libssh")
    elseif(UNIX)
        message(STATUS "  Linux:")
        message(STATUS "    Ubuntu/Debian: sudo apt-get install libssh-dev")
        message(STATUS "    CentOS/RHEL/Fedora: sudo yum install libssh-devel")
        message(STATUS "    or: sudo dnf install libssh-devel")
        message(STATUS "    Arch: sudo pacman -S libssh")
        message(STATUS "    openSUSE: sudo zypper install libssh-devel")
    endif()
endif()

# Export variables for compatibility
if(LibSSH_FOUND)
    set(LIBSSH_FOUND TRUE)
endif()

# Mark cache variables as advanced
mark_as_advanced(LIBSSH_INCLUDE_DIR LIBSSH_LIBRARY)