
# Only example left after the graphics removal: a console program that
# demonstrates window creation, input and event handling with no graphics
# API at all.

link_libraries(glfw)

if (MATH_LIBRARY)
    link_libraries("${MATH_LIBRARY}")
endif()

# Workaround for the MS CRT deprecating parts of the standard library
if (MSVC OR CMAKE_C_SIMULATE_ID STREQUAL "MSVC")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

add_executable(window-input window-input.c)

set_target_properties(window-input PROPERTIES
                      C_STANDARD 99
                      FOLDER "GLFW3/Examples")
