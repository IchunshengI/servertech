cmake_minimum_required(VERSION 3.12)
project(rpc LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_DEBUG_INFO "Enable debug logging" ON)

# ✅ 这个开关给 superbuild 用：默认 OFF，避免 server 构建时也编 tools/rpc/test
option(RPC_BUILD_TESTS "Build rpc tests" OFF)

# ✅ 只有“单独编 tools/rpc”时才设置输出目录；作为子目录被 add_subdirectory 时不改全局
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
  file(MAKE_DIRECTORY ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-fcoroutines)
endif()

add_subdirectory(src)

if(RPC_BUILD_TESTS)
  add_subdirectory(test)
endif()
