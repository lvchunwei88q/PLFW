cmake_minimum_required(VERSION 3.16...3.28 FATAL_ERROR)

project(PLFW VERSION 3.5.1 LANGUAGES C)

# macOS is not a supported target: the Cocoa backend was removed, so refuse to
# configure instead of silently degrading to a null-only build
if (APPLE)
    message(FATAL_ERROR "PLFW does not support macOS")
endif()

# Supported toolchains: MSVC on Windows and GCC on Linux.  Every per-compiler
# fork in this file and in src/CMakeLists.txt assumes exactly that pair, so
# anything else (MinGW, Clang, clang-cl, MSYS2, cross-compiling to Windows with
# mingw-w64) is refused instead of taking a branch meant for the other family.
if (WIN32 AND NOT MSVC)
    message(FATAL_ERROR
        "PLFW on Windows requires MSVC (found ${CMAKE_C_COMPILER_ID}); "
        "MinGW, Clang and clang-cl toolchains are not supported")
endif()
if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR
        "PLFW on Linux requires GCC (found ${CMAKE_C_COMPILER_ID})")
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

string(COMPARE EQUAL "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}" PLFW_STANDALONE)

# Emit compile_commands.json for clangd / editor tooling.  Only the Ninja and
# Makefile generators write this file; the Visual Studio generator ignores the
# setting, so use a Ninja build directory when you need editor support
# (see "编辑器集成" in docs/build.md)
if (PLFW_STANDALONE)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(PLFW_BUILD_EXAMPLES "Build the PLFW example programs" ${PLFW_STANDALONE})

include(CMakeDependentOption)

cmake_dependent_option(PLFW_BUILD_WIN32 "Build support for Win32" ON "WIN32" OFF)
cmake_dependent_option(PLFW_BUILD_WAYLAND "Build support for Wayland" ON "UNIX" OFF)

cmake_dependent_option(PLFW_USE_HYBRID_HPG "Force use of high-performance GPU on hybrid systems" OFF
                       "WIN32" OFF)
cmake_dependent_option(USE_MSVC_RUNTIME_LIBRARY_DLL "Use MSVC runtime library DLL" ON
                       "MSVC" OFF)

set(PLFW_LIBRARY_TYPE "${PLFW_LIBRARY_TYPE}" CACHE STRING
    "Library type override for PLFW (SHARED, STATIC, OBJECT, or empty to follow BUILD_SHARED_LIBS)")

if (PLFW_LIBRARY_TYPE)
    string(COMPARE EQUAL "${PLFW_LIBRARY_TYPE}" "SHARED" PLFW_BUILD_SHARED_LIBRARY)
else()
    set(PLFW_BUILD_SHARED_LIBRARY ${BUILD_SHARED_LIBS})
endif()

find_package(Threads REQUIRED)

#--------------------------------------------------------------------
# Report backend selection
#--------------------------------------------------------------------
if (PLFW_BUILD_WIN32)
    message(STATUS "Including Win32 support")
endif()
if (PLFW_BUILD_WAYLAND)
    message(STATUS "Including Wayland support")
endif()

#--------------------------------------------------------------------
# Apply Microsoft C runtime library option
# This is here because it also applies to tests and examples
#--------------------------------------------------------------------
if (MSVC AND NOT USE_MSVC_RUNTIME_LIBRARY_DLL)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

#--------------------------------------------------------------------
# Add subdirectories
#--------------------------------------------------------------------
add_subdirectory(src)

if (PLFW_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

