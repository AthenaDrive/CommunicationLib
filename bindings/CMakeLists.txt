set(PYBIND11_FINDPYTHON ON)
find_package(pybind11 QUIET)

if(NOT pybind11_FOUND)
    message(STATUS "pybind11 not found, skipping Python bindings")
    return()
endif()

pybind11_add_module(EthernetPython bindings.cpp)
target_link_libraries(EthernetPython
        PRIVATE EthernetCommunication
)

set_target_properties(EthernetPython PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bindings
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bindings
)

option(GENERATE_PYTHON_STUBS "Generate python stubs" OFF)
if (GENERATE_PYTHON_STUBS)
    add_custom_command(
            TARGET EthernetPython
            POST_BUILD
            COMMAND
            ${Python_EXECUTABLE}
            -m pybind11_stubgen
            EthernetPython
            -o "${CMAKE_SOURCE_DIR}/bindings"
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:EthernetPython>"
            COMMENT "Generating Python stubs for EthernetPython"
            VERBATIM
    )
endif ()