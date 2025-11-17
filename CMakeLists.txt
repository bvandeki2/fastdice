cmake_minimum_required(VERSION 3.10)

# Project name
project(FastDice)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Wren library
set(WREN_SRC_DIR lib/wren/src)

# Add bench.cpp to the build
add_executable(Bench
    src/bench.cpp
    src/rangedist.cpp
)

# Add the source files
add_executable(FastDice
    src/main.cpp
    src/rangedist.cpp
)

add_compile_definitions(
    WREN_OPT_META=0
    WREN_OPT_RANDOM=0
)

add_library(wren
    ${WREN_SRC_DIR}/vm/wren_compiler.c
    ${WREN_SRC_DIR}/vm/wren_core.c
    ${WREN_SRC_DIR}/vm/wren_core.wren.inc
    ${WREN_SRC_DIR}/vm/wren_debug.c
    ${WREN_SRC_DIR}/vm/wren_primitive.c
    ${WREN_SRC_DIR}/vm/wren_utils.c
    ${WREN_SRC_DIR}/vm/wren_value.c
    ${WREN_SRC_DIR}/vm/wren_vm.c
)

target_include_directories(wren PUBLIC 
    ${WREN_SRC_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(FastDice wren)
target_include_directories(FastDice PUBLIC ${WREN_SRC_DIR}/include)