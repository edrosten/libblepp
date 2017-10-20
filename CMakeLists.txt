cmake_minimum_required(VERSION 3.4)
project(ble++)

set(CMAKE_CXX_STANDARD 11)

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

set(EXAMPLES
    examples/lescan.cc
    examples/blelogger.cc
    examples/bluetooth.cc
    examples/lescan_simple.cc
    examples/temperature.cc)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

find_package(Bluez REQUIRED)

include_directories(${PROJECT_SOURCE_DIR} ${BLUEZ_INCLUDE_DIRS})
add_library(${PROJECT_NAME} SHARED ${SRC})
set_target_properties(${PROJECT_NAME} PROPERTIES SOVERSION 5)

foreach (example_src ${EXAMPLES})
    get_filename_component(example_name ${example_src} NAME_WE)

    add_executable(${example_name} ${example_src})
    target_link_libraries(${example_name} ${PROJECT_NAME} ${BLUEZ_LIBRARIES})
    set_target_properties(${example_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY examples)
endforeach()

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib)
install(DIRECTORY blepp DESTINATION include)
