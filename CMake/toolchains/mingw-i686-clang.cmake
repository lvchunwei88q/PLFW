# MinGW-w64 交叉编译工具链：32 位 Windows 目标，Clang。
#
# 用法：
#   cmake -S . -B build-win32 \
#         -DCMAKE_TOOLCHAIN_FILE=CMake/toolchains/mingw-i686-clang.cmake
#
set(CMAKE_SYSTEM_NAME     Windows)
set(CMAKE_SYSTEM_VERSION 1)

set(CMAKE_C_COMPILER   i686-w64-mingw32-clang)
set(CMAKE_CXX_COMPILER i686-w64-mingw32-clang++)
set(CMAKE_RC_COMPILER  i686-w64-mingw32-windres)
set(CMAKE_RANLIB       i686-w64-mingw32-ranlib)

set(CMAKE_FIND_ROOT_PATH /usr/i686-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
