# Emscripten toolchain file for WebAssembly builds
# This file configures CMake to use Emscripten for compiling C++ to WASM

# Check if EMSDK environment variable is set
if(NOT DEFINED ENV{EMSDK})
    message(WARNING "EMSDK environment variable not set. Please install Emscripten SDK.")
endif()

# Set the Emscripten compiler
set(CMAKE_SYSTEM_NAME Emscripten)
set(CMAKE_CROSSCOMPILING TRUE)

# Release build flags
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
else()
    # Debug build flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g")
endif()
