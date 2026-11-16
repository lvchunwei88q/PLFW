
# The upstream examples were all graphics rendering demos, so they went away
# together with the context layer.  What remains is written for this build: it
# demonstrates windowing, input, monitors and event handling with no graphics
# API at all.

link_libraries(plfw)

if (MATH_LIBRARY)
    link_libraries("${MATH_LIBRARY}")
endif()

# Workaround for the MS CRT deprecating parts of the standard library
if (MSVC OR CMAKE_C_SIMULATE_ID STREQUAL "MSVC")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

add_executable(window-input window-input.c)
add_executable(multi-window multi-window.c)
add_executable(hello-window hello-window.c)

# On Windows the window icon comes from an icon resource named PLFW_ICON inside
# the executable itself, so the examples compile plfw.rc (which pulls in
# plfw.ico) into every one of them.  There is no equivalent on other platforms:
# Wayland has no window-icon protocol at all and ELF binaries carry no icon
# resource, so on Linux the icon belongs to a .desktop file instead.
if (WIN32)
    enable_language(RC)

    # windres shells out to a C preprocessor and looks it up on PATH, which
    # fails whenever the toolchain directory is not exported; point it at the
    # compiler CMake was configured with instead.
    if (CMAKE_RC_COMPILER MATCHES "windres")
        set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} --preprocessor \"${CMAKE_C_COMPILER}\" --preprocessor-arg=-E --preprocessor-arg=-x --preprocessor-arg=c-header")
    endif()

    target_sources(window-input PRIVATE plfw.rc)
    target_sources(multi-window PRIVATE plfw.rc)
    target_sources(hello-window PRIVATE plfw.rc)

    # windres does not report the .ico as a dependency of the .rc it compiles,
    # so replacing the icon would otherwise leave a stale resource object.
    set_source_files_properties(plfw.rc PROPERTIES
                                OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/plfw.ico")
endif()

# The native Wayland accessor is only compiled into the library when that
# backend is enabled; tell the example so it does not reference a symbol that
# a Wayland-less build does not export.
if (PLFW_BUILD_WAYLAND)
    target_compile_definitions(multi-window PRIVATE PLFW_EXAMPLE_HAS_WAYLAND)
endif()

set_target_properties(window-input multi-window hello-window PROPERTIES
                      C_STANDARD 99
                      FOLDER "PLFW3/Examples")
