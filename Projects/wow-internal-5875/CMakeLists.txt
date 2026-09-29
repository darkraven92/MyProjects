cmake_minimum_required(VERSION 3.20)

project(
    wow_internal_5875
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================
# Internal WoW DLL
# ============================================================

add_library(
    wow_internal
    SHARED
    src/dllmain.cpp
)

target_include_directories(
    wow_internal
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(
    wow_internal
    PRIVATE
    version
)

target_link_options(
    wow_internal
    PRIVATE
    -static
)

set_target_properties(
    wow_internal
    PROPERTIES
    PREFIX ""
    OUTPUT_NAME "wow_internal"
)

# ============================================================
# Test host
# ============================================================

add_executable(
    testhost
    src/testhost.cpp
)

target_link_options(
    testhost
    PRIVATE
    -static
)

# ============================================================
# WoW loader
# ============================================================

add_executable(
    wow_loader
    src/loader.cpp
)

target_link_options(
    wow_loader
    PRIVATE
    -static
)

# ============================================================
# Phase 14H.0 - external GUI control panel
# ============================================================

add_executable(
    wow_gui
    WIN32
    src/gui.cpp
)

target_link_libraries(
    wow_gui
    PRIVATE
    comctl32
    comdlg32
    shell32
)

target_link_options(
    wow_gui
    PRIVATE
    -static
    -municode
)

# BEGIN PHASE10A DETOUR
# Runtime path queries use the Detour sources from the user's local VMaNGOS checkout.
set(VMANGOS_ROOT "/home/ludvig/Programming/Projects/vmangos-core" CACHE PATH "Path to the VMaNGOS core source tree")
set(DETOUR_ROOT "${VMANGOS_ROOT}/dep/recastnavigation/Detour")
if(NOT EXISTS "${DETOUR_ROOT}/Include/DetourNavMesh.h")
    message(FATAL_ERROR "Detour headers were not found below ${DETOUR_ROOT}")
endif()
file(GLOB PHASE10A_DETOUR_SOURCES CONFIGURE_DEPENDS "${DETOUR_ROOT}/Source/*.cpp")
target_sources(wow_internal PRIVATE ${PHASE10A_DETOUR_SOURCES})
target_include_directories(wow_internal PRIVATE "${DETOUR_ROOT}/Include")
# END PHASE10A DETOUR
