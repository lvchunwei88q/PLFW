cmake_minimum_required(VERSION 3.16...3.28 FATAL_ERROR)

project(PLFW VERSION 3.5.1 LANGUAGES C)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

string(COMPARE EQUAL "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}" PLFW_STANDALONE)

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(PLFW_BUILD_EXAMPLES "Build the PLFW example programs" ${PLFW_STANDALONE})
option(PLFW_BUILD_TESTS "Build the PLFW test programs" ${PLFW_STANDALONE})
option(PLFW_INSTALL "Generate installation target" ON)

include(GNUInstallDirs)
include(CMakeDependentOption)

if (GLFW_USE_OSMESA)
    message(FATAL_ERROR "GLFW_USE_OSMESA has been removed; set the PLFW_PLATFORM init hint")
endif()

if (DEFINED GLFW_USE_WAYLAND AND UNIX AND NOT APPLE)
    message(FATAL_ERROR
        "GLFW_USE_WAYLAND has been removed; delete the CMake cache and set PLFW_BUILD_WAYLAND instead")
endif()

cmake_dependent_option(PLFW_BUILD_WIN32 "Build support for Win32" ON "WIN32" OFF)
cmake_dependent_option(PLFW_BUILD_WAYLAND "Build support for Wayland" ON "UNIX;NOT APPLE" OFF)

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

list(APPEND CMAKE_MODULE_PATH "${PLFW_SOURCE_DIR}/CMake/modules")

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
# Create generated files
#--------------------------------------------------------------------
include(CMakePackageConfigHelpers)

set(PLFW_CONFIG_PATH "${CMAKE_INSTALL_LIBDIR}/cmake/plfw3")

configure_package_config_file(CMake/plfw3Config.cmake.in
                              src/plfw3Config.cmake
                              INSTALL_DESTINATION "${PLFW_CONFIG_PATH}"
                              NO_CHECK_REQUIRED_COMPONENTS_MACRO)

write_basic_package_version_file(src/plfw3ConfigVersion.cmake
                                 VERSION ${PLFW_VERSION}
                                 COMPATIBILITY SameMajorVersion)

#--------------------------------------------------------------------
# Add subdirectories
#--------------------------------------------------------------------
add_subdirectory(src)

if (PLFW_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if (PLFW_BUILD_TESTS)
    # Registered at the top level so that "ctest --test-dir build" finds the
    # smoke test instead of requiring the build/tests subdirectory
    enable_testing()
    add_subdirectory(tests)
endif()

#--------------------------------------------------------------------
# Install files other than the library
# The library is installed by src/CMakeLists.txt
#--------------------------------------------------------------------
if (PLFW_INSTALL)
    install(DIRECTORY include/PLFW DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            FILES_MATCHING PATTERN plfw3.h PATTERN plfw3native.h)

    install(FILES "${PLFW_BINARY_DIR}/src/plfw3Config.cmake"
                  "${PLFW_BINARY_DIR}/src/plfw3ConfigVersion.cmake"
            DESTINATION "${PLFW_CONFIG_PATH}")

    install(EXPORT plfwTargets FILE plfw3Targets.cmake
            EXPORT_LINK_INTERFACE_LIBRARIES
            DESTINATION "${PLFW_CONFIG_PATH}")
    install(FILES "${PLFW_BINARY_DIR}/src/plfw3.pc"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

    # Only generate this target if no higher-level project already has
    if (NOT TARGET uninstall)
        configure_file(CMake/cmake_uninstall.cmake.in
                       cmake_uninstall.cmake IMMEDIATE @ONLY)

        add_custom_target(uninstall
                          "${CMAKE_COMMAND}" -P
                          "${PLFW_BINARY_DIR}/cmake_uninstall.cmake")
        set_target_properties(uninstall PROPERTIES FOLDER "PLFW3")
    endif()
endif()

