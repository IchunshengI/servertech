# 设置Boost路径（使用你的实际路径）
#set(BOOST_ROOT "/home/tlx/project/boost_1_86_0")

project(rpc_static LANGUAGES CXX)
# 查找Boost头文件（不需要链接组件）
find_package(Boost REQUIRED COMPONENTS headers context coroutine PATHS /usr/local/lib)
# 设置Protobuf路径
set(PROTOBUF_ROOT_DIR "/home/tlx/project/protobuf-30.2")
set(PROTOBUF_INSTALL_DIR "${PROTOBUF_ROOT_DIR}/install")
# 自动查找所有 Abseil 库文件
file(GLOB ABSL_LIBS "${PROTOBUF_INSTALL_DIR}/lib/libabsl*.so")
# 添加Protobuf头文件路径
include_directories(
  ${PROTOBUF_INSTALL_DIR}/include  # Protobuf核心头文件
  ${CMAKE_CURRENT_SOURCE_DIR} # 生成的.pb.h文件路径
)

# 添加Protobuf库文件路径
link_directories(
  ${PROTOBUF_INSTALL_DIR}/lib
)


# 查找Protobuf编译器 (protoc)
find_program(PROTOBUF_PROTOC protoc
  HINTS "${PROTOBUF_ROOT_DIR}/src"  # 如果protoc在源码目录下
)

# 生成Protobuf代码
set(PROTO_SRC "${CMAKE_CURRENT_SOURCE_DIR}/rpc_meta.proto")
set(PROTO_GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})  # 确保生成目录存在

# 执行protoc命令
execute_process(
  COMMAND ${PROTOBUF_PROTOC} 
  --proto_path=${CMAKE_CURRENT_SOURCE_DIR}  # 关键修改：proto文件所在目录
  --cpp_out=${PROTO_GEN_DIR}
  ${PROTO_SRC}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}  # 设置工作目录
)
# 生成的代码文件路径
set(PROTO_GEN_SRC 
  "${PROTO_GEN_DIR}/rpc_meta.pb.cc"
  "${PROTO_GEN_DIR}/rpc_meta.pb.h"
)


# 定义OBJECT库目标
add_library(rpc_static  STATIC
    rpc_server.cpp
    rpc_channel.cpp
    zookeeper_op.cpp
    # 其他需要编译的.cpp文件...
		# log
		log/log_stream.cpp
		log/logger_wrapper.cpp
		log/log_persister.cpp
		log/logger.cpp

    # config
    config/config.cpp

    
		# proto
	#	src/rpc_meta.pb.cc
   ${PROTO_GEN_DIR}/rpc_meta.pb.cc
)

# 为OBJECT库单独设置编译选项
target_compile_options(rpc_static PRIVATE
    -Wall
    -Wextra
    # 其他编译选项...
)
if(ENABLE_DEBUG_INFO)
  target_compile_definitions(rpc_static PRIVATE DEBUG_INFO)
endif()
# 继承父项目的头文件路径（关键！）


# 链接Boost和其他依赖库
target_link_libraries(rpc_static 
    PUBLIC
    protobuf
    ${ABSL_LIBS}
    Boost::headers
    Boost::coroutine
    Boost::context
    zookeeper_mt
    pthread
)


add_subdirectory(ai_server)
