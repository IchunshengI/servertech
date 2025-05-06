

# 设置可执行文件输出到构建目录下的bin文件夹
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
# 设置Protobuf路径
###################################################################
set(PROTOBUF_ROOT_DIR "/home/tlx/project/protobuf-30.2")
set(PROTOBUF_INSTALL_DIR "${PROTOBUF_ROOT_DIR}/install")
# 自动查找所有 Abseil 库文件
file(GLOB ABSL_LIBS "${PROTOBUF_INSTALL_DIR}/lib/libabsl*.so")
# 添加Protobuf头文件路径
include_directories(
  ${PROTOBUF_INSTALL_DIR}/include  # Protobuf核心头文件
  ${CMAKE_CURRENT_SOURCE_DIR}/src  # 生成的.pb.h文件路径
)

# 添加Protobuf库文件路径
link_directories(
  ${PROTOBUF_INSTALL_DIR}/lib
)

###################################################################

# 查找Protobuf编译器 (protoc)
find_program(PROTOBUF_PROTOC protoc
  HINTS "${PROTOBUF_ROOT_DIR}/src"  # 如果protoc在源码目录下
)

# 生成rpc_meata_Protobuf代码
set(PROTO_SRC "${CMAKE_CURRENT_SOURCE_DIR}/rpc_meta.proto")
set(PROTO_GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})  # 确保生成目录存在

# 执行protoc命令
execute_process(
  COMMAND ${PROTOBUF_PROTOC} 
  --proto_path=${CMAKE_CURRENT_SOURCE_DIR}
  --cpp_out=${PROTO_GEN_DIR}
  ${PROTO_SRC}
)

# 生成ai_server_Protobuf代码
set(PROTO_SRC "${CMAKE_CURRENT_SOURCE_DIR}/ai_server.proto")
set(PROTO_GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})  # 确保生成目录存在

# 执行protoc命令
execute_process(
  COMMAND ${PROTOBUF_PROTOC} 
  --proto_path=${CMAKE_CURRENT_SOURCE_DIR}
  --cpp_out=${PROTO_GEN_DIR}
  ${PROTO_SRC}
)

