cmake_minimum_required(VERSION 3.20)
project(servertech_superbuild LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 统一开关：是否构建 tools/rpc 的 test（默认 OFF，避免 server 构建时也把 tools 的 test 拉进来）
option(RPC_BUILD_TESTS "Build tools/rpc tests" OFF)

# 先构建 rpc 框架（会导出 rpc_client / rpc_server / rpc_log / ai_server_proto 等 targets）
add_subdirectory(tools/rpc)

# 再构建 web server（它会链接 rpc_client）
add_subdirectory(server)
