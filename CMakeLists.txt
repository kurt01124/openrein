cmake_minimum_required(VERSION 3.18)
project(openrein CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# pybind11 (provided by scikit-build-core)
find_package(pybind11 REQUIRED)

# nlohmann/json header-only — fetched via FetchContent
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
    DOWNLOAD_NO_EXTRACT TRUE
    DOWNLOAD_DIR ${CMAKE_CURRENT_BINARY_DIR}/include/nlohmann
)
FetchContent_MakeAvailable(nlohmann_json)

# Source files
set(PYCORTEX_SOURCES
    src/bindings.cpp
    src/engine.cpp
    src/subagent.cpp
    src/compact.cpp
    src/tools/read.cpp
    src/tools/write.cpp
    src/tools/edit.cpp
    src/tools/bash.cpp
    src/tools/grep.cpp
    src/tools/glob.cpp
    src/tools/web_fetch.cpp
    src/tools/web_search.cpp
    src/mcp/mcp_client.cpp
    src/mcp/stdio_transport.cpp
    src/mcp/http_transport.cpp
)

# pybind11 module (_openrein.pyd / _openrein.so)
pybind11_add_module(_openrein ${PYCORTEX_SOURCES})

target_include_directories(_openrein PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mcp
    ${CMAKE_CURRENT_BINARY_DIR}/include
)

# Compiler warnings
if(MSVC)
    target_compile_options(_openrein PRIVATE /W3 /utf-8)
else()
    target_compile_options(_openrein PRIVATE -Wall -Wextra)
    # Unix: Bash tool uses pthread (fork/select/SIGKILL)
    target_link_libraries(_openrein PRIVATE pthread)
endif()

# Install location inside the wheel (under python/openrein/)
install(TARGETS _openrein DESTINATION openrein)
