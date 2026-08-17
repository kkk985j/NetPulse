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

add_executable(
	netpulse_server
	apps/server/main.cpp
)

add_executable(
	device_simulator
	tools/device_simulator/main.cpp
)

target_compile_options(
	netpulse_server
	PRIVATE
	-Wall
	-Wextra
	-Wpedantic
)

target_compile_options(
	device_simulator
	PRIVATE
	-Wall
	-Wextra
	-Wpedantic
)
