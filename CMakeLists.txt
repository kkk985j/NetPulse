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

find_package(Threads REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(
    MySQL
    REQUIRED
    IMPORTED_TARGET
    mysqlclient
)

option(
    NETPULSE_ENABLE_INTEGRATION_TESTS
    "Enable tests that require external services"
    OFF
)


add_library(
    netpulse_concurrency
    STATIC
    src/concurrency/thread_pool.cpp
)

target_include_directories(
    netpulse_concurrency
    PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(
    netpulse_concurrency
    PUBLIC
    Threads::Threads
)

target_compile_options(
    netpulse_concurrency
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)


add_library(
    netpulse_network
    STATIC
    src/network/socket.cpp
    src/network/io.cpp
    src/network/nonblocking.cpp
    src/network/epoll.cpp
    src/network/event_fd.cpp
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


add_library(
    netpulse_protocol
    STATIC
    src/protocol/frame.cpp
    src/protocol/frame_decoder.cpp
    src/protocol/frame_write_buffer.cpp
)

target_include_directories(
    netpulse_protocol
    PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(
    netpulse_protocol
    PUBLIC
    netpulse_network
)

target_compile_options(
    netpulse_protocol
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)


add_library(
    netpulse_telemetry
    STATIC
    src/telemetry/telemetry_parser.cpp
)

target_include_directories(
    netpulse_telemetry
    PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(
    netpulse_telemetry
    PUBLIC
    nlohmann_json::nlohmann_json
)

target_compile_options(
    netpulse_telemetry
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)


add_library(
    netpulse_storage
    STATIC
    src/storage/database_config.cpp
    src/storage/mysql_connection.cpp
    src/storage/telemetry_repository.cpp
)

target_include_directories(
    netpulse_storage
    PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(
    netpulse_storage
    PUBLIC
    PkgConfig::MySQL
)

target_compile_options(
    netpulse_storage
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)


add_library(
    netpulse_server_core
    STATIC
    src/server/connection.cpp
    src/server/event_loop.cpp
    src/server/processing.cpp
    src/server/processing_dispatcher.cpp
)

target_include_directories(
    netpulse_server_core
    PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(
    netpulse_server_core
    PUBLIC
    netpulse_protocol
    netpulse_concurrency
    PRIVATE
    netpulse_telemetry
    netpulse_storage
)

target_compile_options(
    netpulse_server_core
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
    netpulse_server_core
    netpulse_storage
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

target_link_libraries(
    device_simulator
    PRIVATE
    netpulse_protocol
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


    add_executable(
        io_test
        tests/unit/io_test.cpp
    )

    target_link_libraries(
        io_test
        PRIVATE
        netpulse_network
    )

    target_compile_options(
        io_test
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    add_test(
        NAME reliable_io
        COMMAND io_test
    )


    add_executable(
        frame_test
        tests/unit/frame_test.cpp
    )

    target_link_libraries(
        frame_test
        PRIVATE
        netpulse_protocol
    )

    target_compile_options(
        frame_test
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    add_test(
        NAME message_framing
        COMMAND frame_test
    )


    add_executable(
        nonblocking_test
        tests/unit/nonblocking_test.cpp
    )

    target_link_libraries(
        nonblocking_test
        PRIVATE
        netpulse_network
    )

    target_compile_options(
        nonblocking_test
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    add_test(
        NAME nonblocking_socket
        COMMAND nonblocking_test
    )


    add_executable(
        epoll_test
        tests/unit/epoll_test.cpp
    )

    target_link_libraries(
        epoll_test
        PRIVATE
        netpulse_network
    )

    target_compile_options(
        epoll_test
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    add_test(
        NAME epoll_wrapper
        COMMAND epoll_test
    )


    add_executable(
        frame_decoder_test
        tests/unit/frame_decoder_test.cpp
    )

    target_link_libraries(
        frame_decoder_test
        PRIVATE
        netpulse_protocol
    )

    target_compile_options(
        frame_decoder_test
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    add_test(
        NAME incremental_frame_decoder
        COMMAND frame_decoder_test
    )


    add_executable(
        frame_write_buffer_test
        tests/unit/frame_write_buffer_test.cpp
    )

    target_link_libraries(
        frame_write_buffer_test
        PRIVATE
        netpulse_protocol
    )

    target_compile_options(
        frame_write_buffer_test
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    add_test(
        NAME nonblocking_frame_writer
        COMMAND frame_write_buffer_test
    )


    add_executable(
        connection_test
        tests/unit/connection_test.cpp
    )

    target_link_libraries(
        connection_test
        PRIVATE
        netpulse_server_core
    )

    target_compile_options(
        connection_test
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    add_test(
        NAME connection_state
        COMMAND connection_test
    )


    add_executable(
        bounded_queue_test
        tests/unit/bounded_queue_test.cpp
    )

    target_link_libraries(
        bounded_queue_test
        PRIVATE
        netpulse_concurrency
    )

    target_compile_options(
        bounded_queue_test
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    add_test(
        NAME bounded_queue
        COMMAND bounded_queue_test
    )


    add_executable(
        thread_pool_test
        tests/unit/thread_pool_test.cpp
    )

    target_link_libraries(
        thread_pool_test
        PRIVATE
        netpulse_concurrency
    )

    target_compile_options(
        thread_pool_test
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    add_test(
        NAME thread_pool
        COMMAND thread_pool_test
    )


    add_executable(
        event_fd_test
        tests/unit/event_fd_test.cpp
    )

    target_link_libraries(
        event_fd_test
        PRIVATE
        netpulse_network
    )

    target_compile_options(
        event_fd_test
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    add_test(
        NAME event_fd
        COMMAND event_fd_test
    )


    add_executable(
        processing_test
        tests/unit/processing_test.cpp
    )

    target_link_libraries(
        processing_test
        PRIVATE
        netpulse_server_core
    )

    target_compile_options(
        processing_test
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    add_test(
        NAME frame_processing
        COMMAND processing_test
    )


    add_executable(
        processing_dispatcher_test
        tests/unit/processing_dispatcher_test.cpp
    )

    target_link_libraries(
        processing_dispatcher_test
        PRIVATE
        netpulse_server_core
    )

    target_compile_options(
        processing_dispatcher_test
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    add_test(
        NAME processing_dispatcher
        COMMAND processing_dispatcher_test
    )


    add_executable(
        telemetry_parser_test
        tests/unit/telemetry_parser_test.cpp
    )

    target_link_libraries(
        telemetry_parser_test
        PRIVATE
        netpulse_telemetry
    )

    target_compile_options(
        telemetry_parser_test
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    add_test(
        NAME telemetry_parser
        COMMAND telemetry_parser_test
    )


    add_executable(
        database_config_test
        tests/unit/database_config_test.cpp
    )

    target_link_libraries(
        database_config_test
        PRIVATE
        netpulse_storage
    )

    target_compile_options(
        database_config_test
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    add_test(
        NAME database_config
        COMMAND database_config_test
    )


    if(NETPULSE_ENABLE_INTEGRATION_TESTS)
        add_executable(
            mysql_connection_test
            tests/integration/mysql_connection_test.cpp
        )

        target_link_libraries(
            mysql_connection_test
            PRIVATE
            netpulse_storage
        )

        target_compile_options(
            mysql_connection_test
            PRIVATE
            -Wall
            -Wextra
            -Wpedantic
        )

        add_test(
            NAME mysql_connection
            COMMAND mysql_connection_test
        )

        set_tests_properties(
            mysql_connection
            PROPERTIES
            LABELS integration
        )


        add_executable(
            telemetry_repository_test
            tests/integration/telemetry_repository_test.cpp
        )

        target_link_libraries(
            telemetry_repository_test
            PRIVATE
            netpulse_storage
            netpulse_telemetry
        )

        target_compile_options(
            telemetry_repository_test
            PRIVATE
            -Wall
            -Wextra
            -Wpedantic
        )

        add_test(
            NAME telemetry_repository
            COMMAND telemetry_repository_test
        )

        set_tests_properties(
            telemetry_repository
            PROPERTIES
            LABELS integration
        )
    endif()
endif()