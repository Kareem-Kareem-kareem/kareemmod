cmake_minimum_required(VERSION 3.21)

project(RLModKit
    VERSION 0.1.0
    DESCRIPTION "Educational BakkesMod-style plugin framework for Rocket League (non-EAC modes only)"
    LANGUAGES CXX)

# ----------------------------------------------------------------------------
#  Global settings
# ----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Rocket League is a 32-bit process, so the injected DLL MUST be 32-bit (x86).
# Configure with:  cmake -A Win32 -B build
if(NOT WIN32)
    message(WARNING
        "RLModKit targets Windows/MSVC. Configuring on a non-Windows host is only "
        "useful for editing/linting - the DLL and injector will not build here.")
endif()

if(MSVC)
    add_compile_options(/W4 /permissive- /MP)
    # Static runtime keeps the injected DLL self-contained.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Put all build artifacts in one place so the injector can find the core DLL.
set(RLMK_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${RLMK_OUTPUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${RLMK_OUTPUT_DIR})

# Where compiled plugin DLLs are dropped for the core to auto-load at runtime.
set(RLMK_PLUGIN_DIR "${RLMK_OUTPUT_DIR}/plugins")

include(cmake/dependencies.cmake)

# The public SDK is header-only.
add_library(rlmk_sdk INTERFACE)
target_include_directories(rlmk_sdk INTERFACE "${CMAKE_SOURCE_DIR}/sdk")
target_compile_features(rlmk_sdk INTERFACE cxx_std_20)

add_subdirectory(core)
add_subdirectory(injector)
add_subdirectory(plugins/example)

message(STATUS "RLModKit configured. Artifacts -> ${RLMK_OUTPUT_DIR}")
