cmake_minimum_required(VERSION 3.10)

# Project name
project(FastDice)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Detect if we're building for WebAssembly
if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    set(BUILD_WASM ON)
    message(STATUS "Building for WebAssembly with Emscripten")
else()
    set(BUILD_WASM OFF)
endif()

# Core library (shared between native and WASM)
add_library(FastDiceCore STATIC
    src/rangedist.cpp
)

# Set include directories for the library
target_include_directories(FastDiceCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Native executables (skip when building for WASM)
if(NOT BUILD_WASM)
    # Add the main executable
    add_executable(FastDice
        src/main.cpp
    )
    target_link_libraries(FastDice PRIVATE FastDiceCore)

    # Add bench.cpp to the build
    add_executable(Bench
        src/bench.cpp
    )
    target_link_libraries(Bench PRIVATE FastDiceCore)
endif()

# WebAssembly target
if(BUILD_WASM)
    add_executable(FastDiceWasm
        src/wasm_interface.cpp
    )
    target_link_libraries(FastDiceWasm PRIVATE FastDiceCore)
    
    # Set output filename
    set_target_properties(FastDiceWasm PROPERTIES
        OUTPUT_NAME "fastdice"
        SUFFIX ".js"
    )
    
    # Emscripten-specific settings
    target_link_options(FastDiceWasm PRIVATE
        "SHELL:-s ALLOW_MEMORY_GROWTH=1"
        "SHELL:-s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','setValue','getValue','HEAPF32','HEAP32']"
        "SHELL:-s EXPORTED_FUNCTIONS=['_malloc','_free']"
        "SHELL:-s MODULARIZE=1"
        "SHELL:-s EXPORT_NAME='createFastDiceModule'"
        "SHELL:-s NO_EXIT_RUNTIME=1"
    )
endif()