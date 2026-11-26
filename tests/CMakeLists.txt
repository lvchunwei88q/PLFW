
# Self-checking smoke test for the windowing-only build: it creates a hidden
# window and exercises the window, monitor, input, clipboard and event APIs
# without touching any graphics API.
#
# It is both a CTest case named "smoke" and a plain executable, so it can be
# run through "ctest" or launched directly (its exit code says whether every
# check passed).

link_libraries(plfw)

# Workaround for the MS CRT deprecating parts of the standard library
if (MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

add_executable(smoke smoke.c)

add_test(NAME smoke COMMAND smoke)
