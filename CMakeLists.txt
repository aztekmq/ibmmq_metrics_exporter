cmake_minimum_required(VERSION 3.20)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Minimum CMake policy version for dependencies")
project(ibmmq_exporter VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Compiler-specific flags ---
if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN NOMINMAX)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(NOT WIN32)
        add_compile_options(-fPIC)
    endif()
endif()

# --- Options ---
option(IBMMQ_EXPORTER_USE_STUB_MQ "Build with stub IBM MQ headers (no real MQ dependency)" OFF)
option(IBMMQ_EXPORTER_ENABLE_TLS  "Enable TLS/HTTPS support (requires OpenSSL)" OFF)

# --- FetchContent for dependencies ---
include(FetchContent)

# yaml-cpp
FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG 0.8.0
    GIT_SHALLOW TRUE
)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)

# CLI11
FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.4.2
    GIT_SHALLOW TRUE
)

# spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.14.1
    GIT_SHALLOW TRUE
)

# prometheus-cpp
FetchContent_Declare(
    prometheus-cpp
    GIT_REPOSITORY https://github.com/jupp0r/prometheus-cpp.git
    GIT_TAG v1.2.4
    GIT_SHALLOW TRUE
)
set(ENABLE_PUSH OFF CACHE BOOL "" FORCE)
set(ENABLE_COMPRESSION OFF CACHE BOOL "" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(GENERATE_PKGCONFIG OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(yaml-cpp CLI11 spdlog prometheus-cpp)

# --- Find IBM MQ ---
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(IBMMQ_EXPORTER_USE_STUB_MQ)
    message(STATUS "Using stub IBM MQ headers (no real MQ library)")
    set(IBMMQ_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/ibmmq_exporter/mq_stubs")
    set(IBMMQ_FOUND TRUE)
    set(IBMMQ_LIBRARIES "")
else()
    find_package(IBMMQ)
    if(NOT IBMMQ_FOUND)
        message(WARNING "IBM MQ client not found. Building with stub headers.\n"
                        "Set MQ_HOME or install IBM MQ client to link against real MQ libraries.")
        set(IBMMQ_EXPORTER_USE_STUB_MQ ON)
        set(IBMMQ_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/ibmmq_exporter/mq_stubs")
        set(IBMMQ_LIBRARIES "")
    endif()
endif()

# --- Main executable ---
set(SOURCES
    src/main.cpp
    src/config.cpp
    src/mqclient.cpp
    src/pcf_parser.cpp
    src/pcf_inquiry.cpp
    src/metrics_collector.cpp
    src/collector.cpp
    src/resource_monitor.cpp
    src/http_server.cpp
)

add_executable(ibmmq-exporter ${SOURCES})

target_include_directories(ibmmq-exporter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${IBMMQ_INCLUDE_DIR}
)

target_link_libraries(ibmmq-exporter PRIVATE
    yaml-cpp::yaml-cpp
    CLI11::CLI11
    spdlog::spdlog
    prometheus-cpp::pull
    ${IBMMQ_LIBRARIES}
)

if(IBMMQ_EXPORTER_USE_STUB_MQ)
    target_compile_definitions(ibmmq-exporter PRIVATE IBMMQ_STUB_MODE=1)
endif()

# --- Optional TLS support ---
if(IBMMQ_EXPORTER_ENABLE_TLS)
    find_package(OpenSSL REQUIRED)
    target_compile_definitions(ibmmq-exporter PRIVATE IBMMQ_EXPORTER_TLS=1)
    target_link_libraries(ibmmq-exporter PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    message(STATUS "TLS/HTTPS support enabled (OpenSSL ${OPENSSL_VERSION})")
endif()

# Platform-specific link libraries
if(WIN32)
    target_link_libraries(ibmmq-exporter PRIVATE ws2_32)
elseif(UNIX)
    target_link_libraries(ibmmq-exporter PRIVATE pthread dl)
endif()

# Version info
target_compile_definitions(ibmmq-exporter PRIVATE
    PROJECT_VERSION="${PROJECT_VERSION}"
)

# Install
install(TARGETS ibmmq-exporter RUNTIME DESTINATION bin)
install(DIRECTORY configs/ DESTINATION etc/ibmmq-exporter)
