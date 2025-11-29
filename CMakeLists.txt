cmake_minimum_required(VERSION 3.4)
project(ble++)
set(PROJECT_VERSION "1.2")
set(PROJECT_DESCRIPTION "Bluetooth LE interface for C++")

set(CMAKE_CXX_STANDARD 11)
set(TARGET1_NAME ${PROJECT_NAME} CACHE INTERNAL "")

set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
            STRING "Choose the type of build." FORCE)
    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
            "Debug" "Release")
endif()

option(WITH_EXAMPLES "Build examples" OFF)
option(WITH_BLUEZ_SUPPORT "Build with BlueZ transport support (HCI/L2CAP)" ON)
option(WITH_NIMBLE_SUPPORT "Build with Nimble transport support (/dev/atbm_ioctl)" OFF)
option(WITH_SERVER_SUPPORT "Build with BLE GATT server support" OFF)

include(GNUInstallDirs)

#----------------------- LIBRARY --------------------------------

set(HEADERS
    blepp/bledevice.h
    blepp/logging.h
    blepp/float.h
    blepp/uuid.h
    blepp/pretty_printers.h
    blepp/gap.h
    blepp/lescan.h
    blepp/xtoa.h
    blepp/att.h
    blepp/blestatemachine.h
    blepp/att_pdu.h
    blepp/blepp_config.h
    blepp/bleclienttransport.h)

set(SRC
    src/att_pdu.cc
    src/float.cc
    src/logging.cc
    src/uuid.cc
    src/blestatemachine.cc
    src/bledevice.cc
    src/pretty_printers.cc
    src/att.cc
    src/lescan.cc
    src/bleclienttransport.cc
    ${HEADERS})

# BlueZ transport support (client + optional server)
if(WITH_BLUEZ_SUPPORT)
    list(APPEND HEADERS
        blepp/bluez_client_transport.h)

    list(APPEND SRC
        src/bluez_client_transport.cc)
endif()

# Nimble transport support (client + optional server)
if(WITH_NIMBLE_SUPPORT)
    list(APPEND HEADERS
        blepp/nimble_client_transport.h)

    list(APPEND SRC
        src/nimble_client_transport.cc)
endif()

# Server support headers and sources
if(WITH_SERVER_SUPPORT)
    list(APPEND HEADERS
        blepp/bletransport.h
        blepp/bleattributedb.h
        blepp/gatt_services.h
        blepp/blegattserver.h)

    list(APPEND SRC
        src/bleattributedb.cc
        src/blegattserver.cc)

    # BlueZ server transport (only if BlueZ support enabled)
    if(WITH_BLUEZ_SUPPORT)
        list(APPEND HEADERS
            blepp/bluez_transport.h)

        list(APPEND SRC
            src/bluez_transport.cc)
    endif()

    # Nimble server transport (only if Nimble support enabled)
    if(WITH_NIMBLE_SUPPORT)
        list(APPEND HEADERS
            blepp/nimble_transport.h)

        list(APPEND SRC
            src/nimble_transport.cc)
    endif()
endif()

LIST(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/modules)

# Validate: require at least one transport
if(NOT WITH_BLUEZ_SUPPORT AND NOT WITH_NIMBLE_SUPPORT)
    message(FATAL_ERROR "At least one of WITH_BLUEZ_SUPPORT or WITH_NIMBLE_SUPPORT must be enabled")
endif()

# Find BlueZ if enabled
if(WITH_BLUEZ_SUPPORT)
    find_package(Bluez REQUIRED)
    include_directories(${BLUEZ_INCLUDE_DIRS})
    message(STATUS "BlueZ transport: ENABLED")
else()
    message(STATUS "BlueZ transport: DISABLED")
endif()

include_directories(${PROJECT_SOURCE_DIR})

# Add preprocessor definitions based on options
if(WITH_BLUEZ_SUPPORT)
    add_definitions(-DBLEPP_BLUEZ_SUPPORT)
endif()

if(WITH_NIMBLE_SUPPORT)
    add_definitions(-DBLEPP_NIMBLE_SUPPORT)

    # Find Nimble BLE stack (required for Nimble transport)
    set(NIMBLE_ROOT_DEFAULT "${CMAKE_SOURCE_DIR}/../atbm-wifi/ble_host/nimble_v42")

    if(DEFINED ENV{NIMBLE_ROOT})
        set(NIMBLE_ROOT $ENV{NIMBLE_ROOT})
    elseif(DEFINED NIMBLE_ROOT)
        # Use the NIMBLE_ROOT passed via -DNIMBLE_ROOT=...
    else()
        set(NIMBLE_ROOT ${NIMBLE_ROOT_DEFAULT})
    endif()

    if(NOT EXISTS "${NIMBLE_ROOT}")
        message(FATAL_ERROR "NIMBLE_ROOT not found at ${NIMBLE_ROOT}. "
                "Please set NIMBLE_ROOT environment variable or pass -DNIMBLE_ROOT=/path/to/nimble_v42")
    endif()

    message(STATUS "Nimble transport: ENABLED (using Nimble from ${NIMBLE_ROOT})")

    # Add Nimble include directories
    include_directories(
        ${NIMBLE_ROOT}/include
        ${NIMBLE_ROOT}/nimble/include
        ${NIMBLE_ROOT}/nimble/host/include
        ${NIMBLE_ROOT}/nimble/host/services/gap/include
        ${NIMBLE_ROOT}/nimble/host/services/gatt/include
        ${NIMBLE_ROOT}/nimble/host/util/include
        ${NIMBLE_ROOT}/porting/nimble/include
        ${NIMBLE_ROOT}/ext/tinycrypt/include
    )

    # Find Nimble libraries (dynamic linking)
    # Look for libnimble.so or libnimble.a in NIMBLE_ROOT/lib
    find_library(NIMBLE_LIBRARY
        NAMES nimble
        HINTS ${NIMBLE_ROOT}/lib ${NIMBLE_ROOT}/build
        NO_DEFAULT_PATH
    )

    if(NOT NIMBLE_LIBRARY)
        message(FATAL_ERROR "Nimble library not found. Please build Nimble as a shared library first.\n"
                "Expected location: ${NIMBLE_ROOT}/lib/libnimble.so or ${NIMBLE_ROOT}/build/libnimble.so\n"
                "See README for instructions on building Nimble.")
    endif()

    message(STATUS "Found Nimble library: ${NIMBLE_LIBRARY}")
    set(NIMBLE_LIBRARIES ${NIMBLE_LIBRARY})
else()
    message(STATUS "Nimble transport: DISABLED")
endif()

if(WITH_SERVER_SUPPORT)
    add_definitions(-DBLEPP_SERVER_SUPPORT)
    message(STATUS "Server support: ENABLED")
else()
    message(STATUS "Server support: DISABLED")
endif()

add_library(${PROJECT_NAME} SHARED ${SRC})

# Link BlueZ libraries if enabled
if(WITH_BLUEZ_SUPPORT)
    target_link_libraries(${PROJECT_NAME} ${BLUEZ_LIBRARIES})
endif()

# Link Nimble libraries if enabled
if(WITH_NIMBLE_SUPPORT)
    target_link_libraries(${PROJECT_NAME} ${NIMBLE_LIBRARIES})
endif()

# Add pthread for server support (needed for std::thread and semaphores)
if(WITH_SERVER_SUPPORT)
    find_package(Threads REQUIRED)
    target_link_libraries(${PROJECT_NAME} Threads::Threads)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 11
    CMAKE_CXX_STANDARD_REQUIRED YES
    SOVERSION 5)

#----------------------- EXAMPLES --------------------------------
if(WITH_EXAMPLES)
    # Transport-agnostic examples (work with any transport)
    set(CORE_EXAMPLES
            examples/lescan_transport.cc)

    foreach (example_src ${CORE_EXAMPLES})
        get_filename_component(example_name ${example_src} NAME_WE)
        add_executable(${example_name} ${example_src})
        target_link_libraries(${example_name} ${PROJECT_NAME})
        set_target_properties(${example_name} PROPERTIES
            CXX_STANDARD 11
            CMAKE_CXX_STANDARD_REQUIRED YES
            RUNTIME_OUTPUT_DIRECTORY examples)
    endforeach()

    # BlueZ-specific examples (only build when BlueZ support is enabled)
    # These use BLEGATTStateMachine::connect_blocking or HCIScanner
    if(WITH_BLUEZ_SUPPORT)
        set(BLUEZ_EXAMPLES
                examples/lescan.cc
                examples/blelogger.cc
                examples/bluetooth.cc
                examples/lescan_simple.cc
                examples/temperature.cc
                examples/read_device_name.cc
                examples/write.cc)

        foreach (example_src ${BLUEZ_EXAMPLES})
            get_filename_component(example_name ${example_src} NAME_WE)
            add_executable(${example_name} ${example_src})
            target_link_libraries(${example_name} ${BLUEZ_LIBRARIES} ${PROJECT_NAME})
            set_target_properties(${example_name} PROPERTIES
                CXX_STANDARD 11
                CMAKE_CXX_STANDARD_REQUIRED YES
                RUNTIME_OUTPUT_DIRECTORY examples)
        endforeach()
    endif()
endif()

#----------------------- PKG CONFIGURATION --------------------------------
message(STATUS "Package Ble++ for ${CMAKE_BUILD_TYPE} version ${PROJECT_VERSION}")
set(TARGET1 ${TARGET1_NAME})
set(PackagingTemplatesDir "${CMAKE_CURRENT_SOURCE_DIR}/packaging")
# Generate pkg-config file
configure_file("${PackagingTemplatesDir}/libblepp.pc.in" ${CMAKE_CURRENT_BINARY_DIR}/libblepp.pc @ONLY)

#----------------------- INSTALL --------------------------------
install(TARGETS ${TARGET1_NAME} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY blepp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libblepp.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

