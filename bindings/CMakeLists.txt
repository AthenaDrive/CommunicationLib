set(PYBIND11_FINDPYTHON ON)
find_package(pybind11 QUIET)

if(NOT pybind11_FOUND)
    message(STATUS "pybind11 not found, skipping Python bindings")
    return()
endif()

message("Building python bindings.")
pybind11_add_module(EthernetPython bindings.cpp)
target_link_libraries(EthernetPython
        PRIVATE EthernetCommunication
)

set_target_properties(EthernetPython PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bindings
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bindings
)