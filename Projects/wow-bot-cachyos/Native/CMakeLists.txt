cmake_minimum_required(VERSION 3.20)
project(wowbot_nav LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(VMANGOS_ROOT "" CACHE PATH "Path to the VMaNGOS core source tree")
if(NOT VMANGOS_ROOT)
    message(FATAL_ERROR "Set VMANGOS_ROOT to the VMaNGOS core source tree")
endif()

set(DETOUR_ROOT "${VMANGOS_ROOT}/dep/recastnavigation/Detour")
if(NOT EXISTS "${DETOUR_ROOT}/Include/DetourNavMesh.h")
    message(FATAL_ERROR "Detour headers were not found below ${DETOUR_ROOT}")
endif()

file(GLOB DETOUR_SOURCES CONFIGURE_DEPENDS "${DETOUR_ROOT}/Source/*.cpp")

add_library(wowbot_nav SHARED wowbot_nav.cpp ${DETOUR_SOURCES})
target_include_directories(wowbot_nav PRIVATE "${DETOUR_ROOT}/Include")
target_compile_options(wowbot_nav PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(wowbot_nav PROPERTIES
    OUTPUT_NAME wowbot_nav
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

