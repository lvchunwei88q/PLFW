
# Self-checking smoke test for the windowing-only build: it creates a hidden
# window and exercises the window, monitor, input, clipboard and event APIs
# without touching any graphics API.
#
# This target is a plain executable on purpose: it is not registered with
# CTest, so it can be run directly (or launched from an external test driver)
# and its exit code tells whether every check passed.

link_libraries(plfw)

# Workaround for the MS CRT deprecating parts of the standard library
if (MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

add_executable(smoke smoke.c)
