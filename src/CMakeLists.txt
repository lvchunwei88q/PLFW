
add_library(plfw ${PLFW_LIBRARY_TYPE}
                 "${PLFW_SOURCE_DIR}/include/PLFW/plfw3.h"
                 "${PLFW_SOURCE_DIR}/include/PLFW/plfw3native.h"
                 internal.h platform.h mappings.h
                 init.c input.c monitor.c platform.c window.c
                 null_platform.h null_joystick.h
                 null_init.c null_monitor.c null_window.c null_joystick.c)

# The time, thread and module code is shared between all backends on a given OS,
# including the null backend, which still needs those bits to be functional
if (APPLE)
    target_sources(plfw PRIVATE macos_time.h macos_time.c posix_thread.h
                                posix_module.c posix_thread.c)
elseif (WIN32)
    target_sources(plfw PRIVATE win32_time.h win32_thread.h win32_module.c
                                win32_time.c win32_thread.c)
else()
    target_sources(plfw PRIVATE posix_time.h posix_thread.h posix_module.c
                                posix_time.c posix_thread.c)
endif()

add_custom_target(update_mappings
    COMMAND "${CMAKE_COMMAND}" -P "${PLFW_SOURCE_DIR}/CMake/generate-mappings.cmake" mappings.h.in mappings.h
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Updating gamepad mappings from upstream repository"
    SOURCES mappings.h.in "${PLFW_SOURCE_DIR}/CMake/generate-mappings.cmake"
    VERBATIM)

set_target_properties(update_mappings PROPERTIES FOLDER "PLFW3")

if (PLFW_BUILD_WIN32)
    target_compile_definitions(plfw PRIVATE _PLFW_WIN32)
    target_sources(plfw PRIVATE win32_platform.h win32_joystick.h win32_init.c
                                win32_joystick.c win32_monitor.c win32_window.c)
endif()

if (PLFW_BUILD_WAYLAND)
    target_compile_definitions(plfw PRIVATE _PLFW_WAYLAND)
    target_sources(plfw PRIVATE wl_platform.h wl_init.c
                                wl_monitor.c wl_window.c)
endif()

if (PLFW_BUILD_WAYLAND)
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_sources(plfw PRIVATE linux_joystick.h linux_joystick.c)
    endif()
    target_sources(plfw PRIVATE posix_poll.h posix_poll.c)
endif()

if (PLFW_BUILD_WAYLAND)
    include(CheckIncludeFiles)
    include(CheckFunctionExists)
    check_function_exists(memfd_create HAVE_MEMFD_CREATE)
    if (HAVE_MEMFD_CREATE)
        target_compile_definitions(plfw PRIVATE HAVE_MEMFD_CREATE)
    endif()

    find_program(WAYLAND_SCANNER_EXECUTABLE NAMES wayland-scanner)
    if (NOT WAYLAND_SCANNER_EXECUTABLE)
        message(FATAL_ERROR "Failed to find wayland-scanner")
    endif()

    macro(generate_wayland_protocol protocol_file)
        set(protocol_path "${PLFW_SOURCE_DIR}/deps/wayland/${protocol_file}")

        string(REGEX REPLACE "\\.xml$" "-client-protocol.h" header_file ${protocol_file})
        string(REGEX REPLACE "\\.xml$" "-client-protocol-code.h" code_file ${protocol_file})

        add_custom_command(OUTPUT ${header_file}
            COMMAND "${WAYLAND_SCANNER_EXECUTABLE}" client-header "${protocol_path}" ${header_file}
            DEPENDS "${protocol_path}"
            VERBATIM)

        add_custom_command(OUTPUT ${code_file}
            COMMAND "${WAYLAND_SCANNER_EXECUTABLE}" private-code "${protocol_path}" ${code_file}
            DEPENDS "${protocol_path}"
            VERBATIM)

        target_sources(plfw PRIVATE ${header_file} ${code_file})
    endmacro()

    generate_wayland_protocol("wayland.xml")
    generate_wayland_protocol("viewporter.xml")
    generate_wayland_protocol("xdg-shell.xml")
    generate_wayland_protocol("idle-inhibit-unstable-v1.xml")
    generate_wayland_protocol("pointer-constraints-unstable-v1.xml")
    generate_wayland_protocol("relative-pointer-unstable-v1.xml")
    generate_wayland_protocol("fractional-scale-v1.xml")
    generate_wayland_protocol("xdg-activation-v1.xml")
    generate_wayland_protocol("xdg-decoration-unstable-v1.xml")
endif()

if (WIN32 AND PLFW_BUILD_SHARED_LIBRARY)
    configure_file(plfw.rc.in plfw.rc @ONLY)
    target_sources(plfw PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/plfw.rc")
endif()

if (UNIX AND PLFW_BUILD_SHARED_LIBRARY)
    # On Unix-like systems, shared libraries can use the soname system.
    set(PLFW_LIB_NAME plfw)
else()
    set(PLFW_LIB_NAME plfw3)
endif()
set(PLFW_LIB_NAME_SUFFIX "")

set_target_properties(plfw PROPERTIES
                      OUTPUT_NAME ${PLFW_LIB_NAME}
                      VERSION ${PLFW_VERSION_MAJOR}.${PLFW_VERSION_MINOR}
                      SOVERSION ${PLFW_VERSION_MAJOR}
                      POSITION_INDEPENDENT_CODE ON
                      C_STANDARD 99
                      C_EXTENSIONS OFF
                      DEFINE_SYMBOL _PLFW_BUILD_DLL
                      FOLDER "PLFW3")

target_include_directories(plfw PUBLIC
                           "$<BUILD_INTERFACE:${PLFW_SOURCE_DIR}/include>"
                           "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
target_include_directories(plfw PRIVATE
                           "${PLFW_SOURCE_DIR}/src"
                           "${PLFW_BINARY_DIR}/src")
target_link_libraries(plfw PRIVATE Threads::Threads)

if (PLFW_BUILD_WIN32)
    list(APPEND plfw_PKG_LIBS "-lgdi32")
endif()

if (PLFW_BUILD_WAYLAND)
    include(FindPkgConfig)

    pkg_check_modules(Wayland REQUIRED
        wayland-client>=0.2.7
        wayland-cursor>=0.2.7
        xkbcommon>=0.5.0)

    target_include_directories(plfw PRIVATE ${Wayland_INCLUDE_DIRS})

    if (NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_package(EpollShim)
        if (EPOLLSHIM_FOUND)
            target_include_directories(plfw PRIVATE ${EPOLLSHIM_INCLUDE_DIRS})
            target_link_libraries(plfw PRIVATE ${EPOLLSHIM_LIBRARIES})
        endif()
    endif()
endif()

if (UNIX AND NOT APPLE)
    find_library(RT_LIBRARY rt)
    mark_as_advanced(RT_LIBRARY)
    if (RT_LIBRARY)
        target_link_libraries(plfw PRIVATE "${RT_LIBRARY}")
        list(APPEND plfw_PKG_LIBS "-lrt")
    endif()

    find_library(MATH_LIBRARY m)
    mark_as_advanced(MATH_LIBRARY)
    if (MATH_LIBRARY)
        target_link_libraries(plfw PRIVATE "${MATH_LIBRARY}")
        list(APPEND plfw_PKG_LIBS "-lm")
    endif()

    if (CMAKE_DL_LIBS)
        target_link_libraries(plfw PRIVATE "${CMAKE_DL_LIBS}")
        list(APPEND plfw_PKG_LIBS "-l${CMAKE_DL_LIBS}")
    endif()
endif()

if (APPLE)
    target_link_libraries(plfw PRIVATE "-framework CoreFoundation")
    list(APPEND plfw_PKG_LIBS "-framework CoreFoundation")
endif()

if (WIN32)
    if (PLFW_USE_HYBRID_HPG)
        target_compile_definitions(plfw PRIVATE _PLFW_USE_HYBRID_HPG)
    endif()
endif()

# Enable a reasonable set of warnings
# NOTE: The order matters here, Clang-CL matches both MSVC and Clang
if (MSVC)
    target_compile_options(plfw PRIVATE "/W3")
elseif (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR
        CMAKE_C_COMPILER_ID STREQUAL "Clang" OR
        CMAKE_C_COMPILER_ID STREQUAL "AppleClang")

    target_compile_options(plfw PRIVATE "-Wall")
endif()

if (PLFW_BUILD_WIN32)
    target_compile_definitions(plfw PRIVATE UNICODE _UNICODE)
endif()

# Workaround for the MS CRT deprecating parts of the standard library
if (MSVC OR CMAKE_C_SIMULATE_ID STREQUAL "MSVC")
    target_compile_definitions(plfw PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Workaround for -std=c99 on Linux disabling _DEFAULT_SOURCE (POSIX 2008 and more)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(plfw PRIVATE _DEFAULT_SOURCE)
endif()

if (PLFW_BUILD_SHARED_LIBRARY)
    if (WIN32)
        if (MINGW)
            # Remove the lib prefix on the DLL (but not the import library)
            set_target_properties(plfw PROPERTIES PREFIX "")

            # Add a suffix to the import library to avoid naming conflicts
            set_target_properties(plfw PROPERTIES IMPORT_SUFFIX "dll.a")
        else()
            # Add a suffix to the import library to avoid naming conflicts
            set_target_properties(plfw PROPERTIES IMPORT_SUFFIX "dll.lib")
        endif()
        set (PLFW_LIB_NAME_SUFFIX "dll")

        target_compile_definitions(plfw INTERFACE PLFW_DLL)
    endif()

    if (MINGW)
        # Enable link-time exploit mitigation features enabled by default on MSVC
        include(CheckCCompilerFlag)
        include(CMakePushCheckState)

        # Compatibility with data execution prevention (DEP)
        cmake_push_check_state()
        set(CMAKE_REQUIRED_FLAGS "-Wl,--nxcompat")
        check_c_compiler_flag("" _PLFW_HAS_DEP)
        if (_PLFW_HAS_DEP)
            target_link_libraries(plfw PRIVATE "-Wl,--nxcompat")
        endif()
        cmake_pop_check_state()

        # Compatibility with address space layout randomization (ASLR)
        cmake_push_check_state()
        set(CMAKE_REQUIRED_FLAGS "-Wl,--dynamicbase")
        check_c_compiler_flag("" _PLFW_HAS_ASLR)
        if (_PLFW_HAS_ASLR)
            target_link_libraries(plfw PRIVATE "-Wl,--dynamicbase")
        endif()
        cmake_pop_check_state()

        # Compatibility with 64-bit address space layout randomization (ASLR)
        cmake_push_check_state()
        set(CMAKE_REQUIRED_FLAGS "-Wl,--high-entropy-va")
        check_c_compiler_flag("" _PLFW_HAS_64ASLR)
        if (_PLFW_HAS_64ASLR)
            target_link_libraries(plfw PRIVATE "-Wl,--high-entropy-va")
        endif()
        cmake_pop_check_state()
    endif()

    if (UNIX)
        # Hide symbols not explicitly tagged for export from the shared library
        target_compile_options(plfw PRIVATE "-fvisibility=hidden")
    endif()
endif()

list(JOIN plfw_PKG_DEPS " " deps)
list(JOIN plfw_PKG_LIBS " " libs)

set(PLFW_PKG_CONFIG_REQUIRES_PRIVATE "${deps}" CACHE INTERNAL
    "PLFW pkg-config Requires.private")
set(PLFW_PKG_CONFIG_LIBS_PRIVATE "${libs}" CACHE INTERNAL
    "PLFW pkg-config Libs.private")

configure_file("${PLFW_SOURCE_DIR}/CMake/plfw3.pc.in" plfw3.pc @ONLY)

if (PLFW_INSTALL)
    install(TARGETS plfw
            EXPORT plfwTargets
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
endif()

