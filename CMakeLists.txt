cmake_minimum_required(VERSION 4.0)
project(ComputerControl)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
        asio
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
        GIT_TAG asio-1-38-0
)

FetchContent_MakeAvailable(asio)

add_library(EthernetCommunication
    src/Ethernet.cpp
)

target_include_directories(EthernetCommunication PUBLIC
        ${asio_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0A00)
    target_link_libraries(EthernetCommunication PRIVATE ws2_32)
endif()
target_compile_features(EthernetCommunication PRIVATE cxx_std_20)

add_executable(ComputerControl src/main.cpp)
target_link_libraries(ComputerControl PRIVATE EthernetCommunication)

option(BUILD_PYTHON_BINDINGS "Build python bindings" OFF)
if(BUILD_PYTHON_BINDINGS)
    add_subdirectory(bindings)
endif()