cmake_minimum_required(VERSION 3.20)
project(rpc_modules LANGUAGES CXX)

find_package(Boost 1.86 REQUIRED COMPONENTS context coroutine system)
find_package(Protobuf CONFIG REQUIRED)
find_package(Threads REQUIRED)

# ---------- log ----------
add_subdirectory(log)

# ---------- ai_server proto ----------
add_subdirectory(ai_server)  # 生成 ai_server_proto

# ---------- rpc framework proto ----------
add_library(rpc_proto STATIC)

protobuf_generate(
  TARGET rpc_proto
  LANGUAGE cpp
  PROTOS ${CMAKE_CURRENT_SOURCE_DIR}/rpc_meta.proto
  IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
  PROTOC_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}
)

target_include_directories(rpc_proto PUBLIC
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}          # 让 include "rpc_meta.pb.h" 也能找到（如果你这么写）
)

target_link_libraries(rpc_proto PUBLIC
  protobuf::libprotobuf
)

# ---------- rpc_client ----------
add_library(rpc_client STATIC
  rpc_channel.cpp
  rpc_client.cpp
  zookeeper_op.cpp
  config/config.cpp
  until/xxhash.cpp
  
)

target_include_directories(rpc_client PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}          # tools/rpc/src
  ${CMAKE_CURRENT_BINARY_DIR}          # rpc_meta 生成头
  ${CMAKE_CURRENT_SOURCE_DIR}/config
)

target_link_libraries(rpc_client PUBLIC
  rpc_log
  rpc_proto
  ai_server_proto        # ✅ 关键：客户端需要 ai_server 的 pb
  Boost::headers
  Boost::coroutine
  Boost::context
  Boost::system
  Threads::Threads
  zookeeper_mt
)
target_compile_definitions(rpc_client PUBLIC THREADED)

# ---------- rpc_server（tools 侧服务端用；server 项目不需要 link 它） ----------
add_library(rpc_server STATIC
  rpc_server.cpp
  rpc_channel.cpp
  zookeeper_op.cpp
  config/config.cpp
  until/xxhash.cpp
)

find_path(ZOOKEEPER_INCLUDE_DIR NAMES zookeeper/zookeeper.h)
target_include_directories(rpc_server PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/config
  ${ZOOKEEPER_INCLUDE_DIR}
)

target_link_libraries(rpc_server PUBLIC
  rpc_log
  rpc_proto
  Boost::headers
  Boost::coroutine
  Boost::context
  Boost::system
  Threads::Threads
  zookeeper_mt
)
target_compile_definitions(rpc_server PUBLIC THREADED)