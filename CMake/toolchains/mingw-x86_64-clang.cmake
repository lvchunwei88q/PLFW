# MinGW-w64 交叉编译工具链：64 位 Windows 目标，Clang。
#
# 用法：
#   cmake -S . -B build-win \
#         -DCMAKE_TOOLCHAIN_FILE=CMake/toolchains/mingw-x86_64-clang.cmake
#
set(CMAKE_SYSTEM_NAME     Windows)
set(CMAKE_SYSTEM_VERSION 1)

set(CMAKE_C_COMPILER   x86_64-w64-mingw32-clang)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-clang++)
set(CMAKE_RC_COMPILER  x86_64-w64-mingw32-windres)
set(CMAKE_RANLIB       x86_64-w64-mingw32-ranlib)

set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
