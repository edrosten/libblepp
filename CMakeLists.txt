cmake_minimum_required(VERSION 3.4)
project(ble++)
set(PROJECT_VERSION "1.2")
set(PROJECT_DESCRIPTION "Bluetooth LE interface for C++")

set(CMAKE_CXX_STANDARD 17)
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
    blepp/att_pdu.h)

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
    ${HEADERS})

LIST(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/modules)

find_package(Bluez REQUIRED)

include_directories(${PROJECT_SOURCE_DIR} ${BLUEZ_INCLUDE_DIRS})
add_library(${PROJECT_NAME} SHARED ${SRC})

target_link_libraries(${PROJECT_NAME} ${BLUEZ_LIBRARIES})
set_target_properties(${PROJECT_NAME} PROPERTIES 
    CXX_STANDARD 17
    CMAKE_CXX_STANDARD_REQUIRED YES
    SOVERSION 5)

#----------------------- EXAMPLES --------------------------------
if(WITH_EXAMPLES)
    set(EXAMPLES
            examples/lescan.cc
            examples/blelogger.cc
            examples/bluetooth.cc
            examples/lescan_simple.cc
            examples/temperature.cc)

    foreach (example_src ${EXAMPLES})
        get_filename_component(example_name ${example_src} NAME_WE)

        add_executable(${example_name} ${example_src})
        target_link_libraries(${example_name} ${BLUEZ_LIBRARIES} ${PROJECT_NAME})

        set_target_properties(${example_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY examples)
        set_target_properties(${example_name} PROPERTIES
            CXX_STANDARD 17
            CMAKE_CXX_STANDARD_REQUIRED YES
            RUNTIME_OUTPUT_DIRECTORY examples)
    endforeach()
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

