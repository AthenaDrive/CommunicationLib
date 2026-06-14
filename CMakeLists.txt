cmake_minimum_required(VERSION 4.0)
project(ComputerControll)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

FetchContent_Declare(
        asio
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
        GIT_TAG asio-1-38-0
)

FetchContent_MakeAvailable(asio)

add_executable(ComputerControll
        src/main.cpp src/Ethernet.cpp)

target_include_directories(ComputerControll PRIVATE
        ${asio_SOURCE_DIR}/include
        include
)

if(WIN32)
    target_link_libraries(ComputerControll PRIVATE ws2_32 atomic)
endif()

target_compile_features(ComputerControll PRIVATE cxx_std_20)
