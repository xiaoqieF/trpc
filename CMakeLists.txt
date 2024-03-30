cmake_minimum_required(VERSION 3.15)

project(trpc)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

IF("${CMAKE_BUILD_TYPE}" MATCHES "Debug")
    message(STATUS "building for: debugging")
    set(CXX_FORMAT_SIGNEDNESS "-Wformat-signedness")
    set(CXX_FORMAT_SECURITY "-Werror=format-security")
    set(CXX_STACK_PROTECTOR "-fstack-protector-all")
    set(CMAKE_CXX_FLAGS "-g -Wall")
ELSE()
    message(STATUS "building for: release")
    set(CMAKE_CXX_FLAGS "-O2")
ENDIF()

include_directories(${CMAKE_SOURCE_DIR}/third_party/clog)
include_directories(${CMAKE_SOURCE_DIR}/third_party/asio)
include_directories(${CMAKE_SOURCE_DIR}/third_party/msgpack-c/include)

add_subdirectory(${CMAKE_SOURCE_DIR}/test)
