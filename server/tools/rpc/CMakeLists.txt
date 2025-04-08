cmake_minimum_required(VERSION 3.10)
project(rpc_demo)

# 设置Protobuf路径
set(PROTOBUF_ROOT_DIR "/home/tlx/project/protobuf-3.14.0")
set(PROTOBUF_BUILD_DIR "${PROTOBUF_ROOT_DIR}/src/.libs")

# 添加Protobuf头文件路径
include_directories(
  ${PROTOBUF_ROOT_DIR}/src  # Protobuf核心头文件
  ${CMAKE_CURRENT_BINARY_DIR}  # 生成的.pb.h文件路径
)

# 添加Protobuf库文件路径
link_directories(
  ${PROTOBUF_BUILD_DIR}
)

# 查找Protobuf编译器 (protoc)
find_program(PROTOBUF_PROTOC protoc
  HINTS "${PROTOBUF_ROOT_DIR}/src"  # 如果protoc在源码目录下
)

# 生成Protobuf代码
set(PROTO_SRC "${CMAKE_CURRENT_SOURCE_DIR}/rpc_demo.proto")
set(PROTO_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}")
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})  # 确保生成目录存在

# 执行protoc命令
execute_process(
  COMMAND ${PROTOBUF_PROTOC} 
  --proto_path=${CMAKE_CURRENT_SOURCE_DIR}
  --cpp_out=${PROTO_GEN_DIR}
  ${PROTO_SRC}
)

# 生成的代码文件路径
set(PROTO_GEN_SRC 
  "${PROTO_GEN_DIR}/rpc_demo.pb.cc"
  "${PROTO_GEN_DIR}/rpc_demo.pb.h"
)

# 添加可执行文件
add_executable(server server.cpp ${PROTO_GEN_SRC})
add_executable(client client.cpp ${PROTO_GEN_SRC})

# 链接Protobuf库
target_link_libraries(server protobuf)
target_link_libraries(client protobuf)

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)