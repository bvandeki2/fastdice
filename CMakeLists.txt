cmake_minimum_required(VERSION 3.10)

# Project name
project(FastDice)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add the source files
add_executable(FastDice
    src/main.cpp
    src/rangedist.cpp
    src/graph.cpp
    src/fft.cpp
)

# Add bench.cpp to the build
add_executable(Bench
    src/bench.cpp
    src/rangedist.cpp
    src/graph.cpp
    src/fft.cpp
)