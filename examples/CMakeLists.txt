
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

# The native Wayland accessor is only compiled into the library when that
# backend is enabled; tell the example so it does not reference a symbol that
# a Wayland-less build does not export.
if (PLFW_BUILD_WAYLAND)
    target_compile_definitions(multi-window PRIVATE PLFW_EXAMPLE_HAS_WAYLAND)
endif()

set_target_properties(window-input multi-window PROPERTIES
                      C_STANDARD 99
                      FOLDER "PLFW3/Examples")
