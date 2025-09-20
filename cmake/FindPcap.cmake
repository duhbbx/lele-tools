# FindPcap.cmake - Find pcap library (Npcap on Windows, libpcap on Unix-like systems)
#
# This module defines:
#  Pcap_FOUND           - True if pcap is found
#  Pcap_INCLUDE_DIRS    - Include directories for pcap
#  Pcap_LIBRARIES       - Libraries to link for pcap
#  Pcap::Pcap           - Imported target for pcap
#  HAVE_PCAP            - Set to TRUE if pcap is found
#  WITH_WINDOWS_ICMP    - Set to TRUE on Windows if using ICMP API fallback
#
# Cache Variables:
#  PCAP_INCLUDE_DIR     - Include directory path
#  PCAP_LIBRARY         - Main pcap library path
#  PACKET_LIBRARY       - Packet library path (Windows only)
#
# Environment Variables:
#  PCAP_ROOT            - Root directory of pcap installation
#  NPCAP_SDK_PATH       - Path to Npcap SDK (Windows)

include(FindPackageHandleStandardArgs)

# Initialize variables
set(Pcap_FOUND FALSE)
set(Pcap_INCLUDE_DIRS "")
set(Pcap_LIBRARIES "")
set(HAVE_PCAP FALSE)
set(WITH_WINDOWS_ICMP FALSE)

# Allow manual specification of Npcap SDK path
set(NPCAP_SDK_ROOT "" CACHE PATH "Path to Npcap SDK root directory")

# Platform-specific detection
if(WIN32)
    message(STATUS "Looking for Npcap SDK on Windows...")

    # Get hints from environment variables and cache
    set(NPCAP_SDK_HINTS "")

    # Prioritize manually specified path
    if(NPCAP_SDK_ROOT)
        list(APPEND NPCAP_SDK_HINTS "${NPCAP_SDK_ROOT}")
        message(STATUS "Using manually specified NPCAP_SDK_ROOT: ${NPCAP_SDK_ROOT}")
    endif()

    if(DEFINED ENV{NPCAP_SDK_PATH})
        list(APPEND NPCAP_SDK_HINTS "$ENV{NPCAP_SDK_PATH}")
    endif()

    if(DEFINED ENV{PCAP_ROOT})
        list(APPEND NPCAP_SDK_HINTS "$ENV{PCAP_ROOT}")
    endif()

    # Check for other potential environment variables
    foreach(env_var IN ITEMS "NPCAP_ROOT" "NPCAP_DIR" "WINPCAP_DIR" "PCAP_DIR")
        if(DEFINED ENV{${env_var}})
            list(APPEND NPCAP_SDK_HINTS "$ENV{${env_var}}")
            message(STATUS "Found environment variable ${env_var}: $ENV{${env_var}}")
        endif()
    endforeach()

    # Add common installation paths
    list(APPEND NPCAP_SDK_HINTS
        # GitHub Actions CI environment
        "${CMAKE_CURRENT_SOURCE_DIR}/npcap-sdk"
        "D:/a/lele-tools/lele-tools/npcap-sdk"
        "${CMAKE_SOURCE_DIR}/npcap-sdk"

        # Local development paths
        "C:/npcap-sdk-1.15"
        "C:/npcap-sdk-1.14"
        "C:/npcap-sdk-1.13"
        "C:/npcap-sdk"

        # System installation paths
        "C:/Program Files/Npcap/sdk"
        "C:/Program Files (x86)/Npcap/sdk"

        # Project dependency paths
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/npcap-sdk"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/npcap-sdk"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/npcap-sdk"
        "${CMAKE_BINARY_DIR}/npcap-sdk"

        # Relative to source directory
        "${CMAKE_SOURCE_DIR}/../npcap-sdk"
        "${CMAKE_SOURCE_DIR}/../../npcap-sdk"
    )

    # Add dynamic path detection for CI environments
    if(DEFINED ENV{GITHUB_WORKSPACE})
        list(APPEND NPCAP_SDK_HINTS
            "$ENV{GITHUB_WORKSPACE}/npcap-sdk"
            "$ENV{GITHUB_ACTIONS_HOME}/npcap-sdk"
        )
        message(STATUS "GitHub Actions environment detected, adding CI-specific paths")
    endif()

    # Add environment-specific paths based on common CI patterns
    if(DEFINED ENV{CI} OR DEFINED ENV{GITHUB_ACTIONS})
        message(STATUS "CI environment detected, adding additional search paths")

        # Common CI patterns
        list(APPEND NPCAP_SDK_HINTS
            "${CMAKE_SOURCE_DIR}/npcap-sdk"
            "${CMAKE_BINARY_DIR}/../npcap-sdk"
            "${CMAKE_CURRENT_BINARY_DIR}/npcap-sdk"

            # Different drive letters on Windows CI
            "D:/npcap-sdk"
            "C:/npcap-sdk"

            # GitHub Actions specific patterns
            "D:/a/${PROJECT_NAME}/${PROJECT_NAME}/npcap-sdk"
            "C:/a/${PROJECT_NAME}/${PROJECT_NAME}/npcap-sdk"
        )
    endif()

    # Add current working directory relative paths
    get_filename_component(CURRENT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
    list(APPEND NPCAP_SDK_HINTS
        "${CURRENT_DIR}/npcap-sdk"
        "${CURRENT_DIR}/../npcap-sdk"
    )

    # Auto-detect npcap-sdk directories in common locations
    set(AUTO_DETECT_PATHS
        "C:/"
        "D:/"
        "${CMAKE_SOURCE_DIR}/.."
        "${CMAKE_SOURCE_DIR}/../.."
    )

    foreach(base_path IN LISTS AUTO_DETECT_PATHS)
        if(EXISTS "${base_path}")
            file(GLOB npcap_dirs "${base_path}/npcap-sdk*")
            foreach(dir IN LISTS npcap_dirs)
                if(IS_DIRECTORY "${dir}")
                    list(APPEND NPCAP_SDK_HINTS "${dir}")
                endif()
            endforeach()
        endif()
    endforeach()

    # Debug: Print search paths (only if CMAKE_FIND_DEBUG_MODE is enabled)
    if(CMAKE_FIND_DEBUG_MODE)
        message(STATUS "Searching for Npcap in the following locations:")
        foreach(hint IN LISTS NPCAP_SDK_HINTS)
            message(STATUS "  - ${hint}")
        endforeach()
    endif()

    # Find Npcap include directory
    find_path(PCAP_INCLUDE_DIR
        NAMES pcap.h pcap/pcap.h
        HINTS
            ${PCAP_INCLUDE_DIR}  # Allow manual override
            ${NPCAP_SDK_HINTS}
        PATH_SUFFIXES Include include
        DOC "Npcap include directory"
    )

    if(CMAKE_FIND_DEBUG_MODE)
        if(PCAP_INCLUDE_DIR)
            message(STATUS "Found Npcap include directory: ${PCAP_INCLUDE_DIR}")
        else()
            message(STATUS "Npcap include directory not found")
        endif()
    endif()

    # Determine library architecture
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(NPCAP_LIB_SUFFIX "x64")
    else()
        set(NPCAP_LIB_SUFFIX "")
    endif()

    # Find wpcap library
    if(CMAKE_FIND_DEBUG_MODE)
        message(STATUS "Searching for wpcap library with architecture suffix: ${NPCAP_LIB_SUFFIX}")
    endif()

    find_library(PCAP_LIBRARY
        NAMES wpcap
        HINTS
            ${PCAP_LIBRARY}  # Allow manual override
            ${NPCAP_SDK_HINTS}
        PATH_SUFFIXES "Lib/${NPCAP_LIB_SUFFIX}" "Lib/x64" "Lib" "lib"
        DOC "Npcap wpcap library"
    )

    if(CMAKE_FIND_DEBUG_MODE)
        if(PCAP_LIBRARY)
            message(STATUS "Found wpcap library: ${PCAP_LIBRARY}")
        else()
            message(STATUS "wpcap library not found")
        endif()
    endif()

    # Find packet library
    find_library(PACKET_LIBRARY
        NAMES packet
        HINTS
            ${PACKET_LIBRARY}  # Allow manual override
            ${NPCAP_SDK_HINTS}
        PATH_SUFFIXES "Lib/${NPCAP_LIB_SUFFIX}" "Lib/x64" "Lib" "lib"
        DOC "Npcap packet library"
    )

    if(CMAKE_FIND_DEBUG_MODE)
        if(PACKET_LIBRARY)
            message(STATUS "Found packet library: ${PACKET_LIBRARY}")
        else()
            message(STATUS "packet library not found (optional)")
        endif()
    endif()

    # Check if we found the required components
    if(PCAP_INCLUDE_DIR AND PCAP_LIBRARY)
        set(Pcap_FOUND TRUE)
        set(Pcap_INCLUDE_DIRS ${PCAP_INCLUDE_DIR})
        set(Pcap_LIBRARIES ${PCAP_LIBRARY})

        if(PACKET_LIBRARY)
            list(APPEND Pcap_LIBRARIES ${PACKET_LIBRARY})
        endif()

        set(HAVE_PCAP TRUE)

        message(STATUS "Npcap found:")
        message(STATUS "  Include: ${PCAP_INCLUDE_DIR}")
        message(STATUS "  WinPcap Library: ${PCAP_LIBRARY}")
        if(PACKET_LIBRARY)
            message(STATUS "  Packet Library: ${PACKET_LIBRARY}")
        endif()

        # Create imported target
        if(NOT TARGET Pcap::Pcap)
            add_library(Pcap::Pcap UNKNOWN IMPORTED)
            set_target_properties(Pcap::Pcap PROPERTIES
                IMPORTED_LOCATION "${PCAP_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${PCAP_INCLUDE_DIR}"
                INTERFACE_COMPILE_DEFINITIONS "WITH_PCAP"
            )

            if(PACKET_LIBRARY)
                set_property(TARGET Pcap::Pcap APPEND PROPERTY
                    INTERFACE_LINK_LIBRARIES "${PACKET_LIBRARY}"
                )
            endif()
        endif()
    else()
        # Fallback to Windows ICMP API
        set(WITH_WINDOWS_ICMP TRUE)
        message(STATUS "Npcap SDK not found. Will use Windows ICMP API fallback.")

        # Create a pseudo-target for Windows ICMP API
        if(NOT TARGET Pcap::Pcap)
            add_library(Pcap::Pcap INTERFACE IMPORTED)
            set_target_properties(Pcap::Pcap PROPERTIES
                INTERFACE_COMPILE_DEFINITIONS "WITH_WINDOWS_ICMP"
                INTERFACE_LINK_LIBRARIES "iphlpapi;ws2_32"
            )
        endif()

        # Set as found but with fallback
        set(Pcap_FOUND TRUE)
        set(Pcap_LIBRARIES "iphlpapi;ws2_32")
    endif()

else()
    # Unix-like systems (Linux, macOS, BSD, etc.)
    message(STATUS "Looking for libpcap on Unix-like system...")

    # Try pkg-config first
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(PC_PCAP QUIET libpcap)
    endif()

    # Find pcap include directory
    find_path(PCAP_INCLUDE_DIR
        NAMES pcap.h pcap/pcap.h
        HINTS
            ${PC_PCAP_INCLUDEDIR}
            ${PC_PCAP_INCLUDE_DIRS}
            ${PCAP_INCLUDE_DIR}  # Allow manual override
            $ENV{PCAP_ROOT}/include
        PATHS
            /usr/include
            /usr/local/include
            /opt/local/include
            /sw/include
        PATH_SUFFIXES pcap
        DOC "libpcap include directory"
    )

    # Find pcap library
    find_library(PCAP_LIBRARY
        NAMES pcap
        HINTS
            ${PC_PCAP_LIBDIR}
            ${PC_PCAP_LIBRARY_DIRS}
            ${PCAP_LIBRARY}  # Allow manual override
            $ENV{PCAP_ROOT}/lib
        PATHS
            /usr/lib
            /usr/local/lib
            /opt/local/lib
            /sw/lib
            /usr/lib/x86_64-linux-gnu
            /usr/lib/aarch64-linux-gnu
        DOC "libpcap library"
    )

    # Check if we found the required components
    if(PCAP_INCLUDE_DIR AND PCAP_LIBRARY)
        set(Pcap_FOUND TRUE)
        set(Pcap_INCLUDE_DIRS ${PCAP_INCLUDE_DIR})
        set(Pcap_LIBRARIES ${PCAP_LIBRARY})
        set(HAVE_PCAP TRUE)

        message(STATUS "libpcap found:")
        message(STATUS "  Include: ${PCAP_INCLUDE_DIR}")
        message(STATUS "  Library: ${PCAP_LIBRARY}")

        # Create imported target
        if(NOT TARGET Pcap::Pcap)
            add_library(Pcap::Pcap UNKNOWN IMPORTED)
            set_target_properties(Pcap::Pcap PROPERTIES
                IMPORTED_LOCATION "${PCAP_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${PCAP_INCLUDE_DIR}"
                INTERFACE_COMPILE_DEFINITIONS "WITH_PCAP"
            )
        endif()
    endif()
endif()

# Handle standard arguments
if(WIN32 AND WITH_WINDOWS_ICMP)
    # Special case for Windows with ICMP fallback
    find_package_handle_standard_args(Pcap
        DEFAULT_MSG
        REQUIRED_VARS WITH_WINDOWS_ICMP
    )
else()
    # Normal case
    find_package_handle_standard_args(Pcap
        FOUND_VAR Pcap_FOUND
        REQUIRED_VARS Pcap_INCLUDE_DIRS Pcap_LIBRARIES
        FAIL_MESSAGE "Could not find pcap library."
    )
endif()

# Provide installation instructions if not found
if(NOT Pcap_FOUND AND NOT WITH_WINDOWS_ICMP)
    message(STATUS "To install pcap:")
    if(WIN32)
        message(STATUS "  Windows:")
        message(STATUS "    1. Download Npcap from: https://npcap.com/")
        message(STATUS "    2. Install with 'Install Npcap SDK' option checked")
        message(STATUS "    3. Or download SDK separately from GitHub releases")
        message(STATUS "    4. Set NPCAP_SDK_PATH environment variable to SDK location")
        message(STATUS "    5. Or set PCAP_INCLUDE_DIR and PCAP_LIBRARY cache variables")
    elseif(APPLE)
        message(STATUS "  macOS:")
        message(STATUS "    brew install libpcap")
        message(STATUS "    or: port install libpcap")
    elseif(UNIX)
        message(STATUS "  Linux:")
        message(STATUS "    Ubuntu/Debian: sudo apt-get install libpcap-dev")
        message(STATUS "    CentOS/RHEL/Fedora: sudo yum install libpcap-devel")
        message(STATUS "    or: sudo dnf install libpcap-devel")
        message(STATUS "    Arch: sudo pacman -S libpcap")
        message(STATUS "    openSUSE: sudo zypper install libpcap-devel")
    endif()
endif()

# Export variables for compatibility
if(Pcap_FOUND OR WITH_WINDOWS_ICMP)
    set(PCAP_FOUND TRUE)

    # Set compile definitions based on what we found
    if(HAVE_PCAP)
        add_compile_definitions(WITH_PCAP)
    elseif(WITH_WINDOWS_ICMP)
        add_compile_definitions(WITH_WINDOWS_ICMP)
    endif()
endif()

# Mark cache variables as advanced
mark_as_advanced(PCAP_INCLUDE_DIR PCAP_LIBRARY PACKET_LIBRARY)