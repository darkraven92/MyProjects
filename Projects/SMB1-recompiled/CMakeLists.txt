cmake_minimum_required(VERSION 3.16)

project(Mario LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(
    vendored/SDL
    EXCLUDE_FROM_ALL
)

add_executable(Mario
    src/main.cpp
    src/nes_memory.cpp
    src/nes_cpu.cpp
    src/nes_ppu.cpp
)

target_link_libraries(
    Mario
    PRIVATE SDL3::SDL3
)