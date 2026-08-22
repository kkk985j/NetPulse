cmake_minimum_required(VERSION 3.22)

project(
    NetPulse
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(
    netpulse_network
    STATIC
    src/network/socket.cpp
)

target_include_directories(
    netpulse_network
    PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_options(
    netpulse_network
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_executable(
    netpulse_server
    apps/server/main.cpp
)

target_link_libraries(
    netpulse_server
    PRIVATE
    netpulse_network
)

target_compile_options(
    netpulse_server
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_executable(
    device_simulator
    tools/device_simulator/main.cpp
)

target_compile_options(
    device_simulator
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

include(CTest)

if(BUILD_TESTING)
    add_executable(
        socket_test
        tests/unit/socket_test.cpp
    )

    target_link_libraries(
        socket_test
        PRIVATE
        netpulse_network
    )

    target_compile_options(
        socket_test
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    add_test(
        NAME socket_raii
        COMMAND socket_test
    )
endif()