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

# Platform-specific detection
if(WIN32)
    message(STATUS "Looking for Npcap SDK on Windows...")

    # Get hints from environment variables
    set(NPCAP_SDK_HINTS "")

    if(DEFINED ENV{NPCAP_SDK_PATH})
        list(APPEND NPCAP_SDK_HINTS "$ENV{NPCAP_SDK_PATH}")
    endif()

    if(DEFINED ENV{PCAP_ROOT})
        list(APPEND NPCAP_SDK_HINTS "$ENV{PCAP_ROOT}")
    endif()

    # Add common installation paths
    list(APPEND NPCAP_SDK_HINTS
        "C:/npcap-sdk-1.15"
        "C:/npcap-sdk-1.14"
        "C:/npcap-sdk-1.13"
        "C:/npcap-sdk"
        "C:/Program Files/Npcap/sdk"
        "C:/Program Files (x86)/Npcap/sdk"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/npcap-sdk"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/npcap-sdk"
    )

    # Find Npcap include directory
    find_path(PCAP_INCLUDE_DIR
        NAMES pcap.h pcap/pcap.h
        HINTS
            ${PCAP_INCLUDE_DIR}  # Allow manual override
            ${NPCAP_SDK_HINTS}
        PATH_SUFFIXES Include include
        DOC "Npcap include directory"
    )

    # Determine library architecture
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(NPCAP_LIB_SUFFIX "x64")
    else()
        set(NPCAP_LIB_SUFFIX "")
    endif()

    # Find wpcap library
    find_library(PCAP_LIBRARY
        NAMES wpcap
        HINTS
            ${PCAP_LIBRARY}  # Allow manual override
            ${NPCAP_SDK_HINTS}
        PATH_SUFFIXES "Lib/${NPCAP_LIB_SUFFIX}" "Lib/x64" "Lib" "lib"
        DOC "Npcap wpcap library"
    )

    # Find packet library
    find_library(PACKET_LIBRARY
        NAMES packet
        HINTS
            ${PACKET_LIBRARY}  # Allow manual override
            ${NPCAP_SDK_HINTS}
        PATH_SUFFIXES "Lib/${NPCAP_LIB_SUFFIX}" "Lib/x64" "Lib" "lib"
        DOC "Npcap packet library"
    )

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