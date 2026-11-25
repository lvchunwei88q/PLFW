
# Self-checking smoke test for the windowing-only build: it creates a hidden
# window and exercises the window, monitor, input, clipboard and event APIs
# without touching any graphics API.

link_libraries(plfw)

# Workaround for the MS CRT deprecating parts of the standard library
if (MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

add_executable(smoke smoke.c)

set_target_properties(smoke PROPERTIES
                      C_STANDARD 99
                      FOLDER "PLFW3/Tests")

enable_testing()
add_test(NAME smoke COMMAND smoke)
