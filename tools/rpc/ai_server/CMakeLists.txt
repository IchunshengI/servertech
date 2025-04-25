

# 设置可执行文件输出到构建目录下的bin文件夹
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
# 设置Protobuf路径
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


# 查找Protobuf编译器 (protoc)
find_program(PROTOBUF_PROTOC protoc
  HINTS "${PROTOBUF_ROOT_DIR}/src"  # 如果protoc在源码目录下
)
# 生成Protobuf代码
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

# 生成的代码文件路径
set(PROTO_GEN_SRC 
  "${PROTO_GEN_DIR}/ai_server.pb.cc"
  "${PROTO_GEN_DIR}/ai_server.pb.h"
)
# 设置C++20标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 为GCC/Clang添加协程支持
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fcoroutines)
endif()

# # 设置可执行文件输出目录
# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# # 查找Boost组件（使用你指定的组件）
# Boost.Charconv is required by Boost.MySQL
find_package(Boost REQUIRED COMPONENTS headers context coroutine json PATHS /usr/local/lib)

add_executable(ai_server 
               server.cpp
               ai_server_impl.cpp
               method_process.cpp
               ${PROTO_GEN_SRC})


# # 链接父项目的静态库
target_link_libraries(ai_server PRIVATE 
    rpc_static  # 直接使用父项目定义的库目标
    OpenSSL::SSL
    Boost::json
)
# 继承父项目的头文件路径（关键！）
target_include_directories(ai_server PRIVATE
    ${CMAKE_SOURCE_DIR}/src  # 父项目源文件中的头文件
    ${CMAKE_BINARY_DIR}/src  # 如果父项目有生成的头文件
)

# 根据选项决定是否添加 DEBUG_LOG 宏
if(ENABLE_DEBUG_INFO)
  target_compile_definitions(ai_server PRIVATE DEBUG_INFO)
endif()

add_executable(ai_client 
               client.cpp 
               ${PROTO_GEN_SRC})
# # 链接父项目的静态库
target_link_libraries(ai_client
    PRIVATE 
    rpc_static  # 直接使用父项目定义的库目标
)
# # 继承父项目的头文件路径（关键！）
target_include_directories(ai_client
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src  # 父项目源文件中的头文件
    ${CMAKE_BINARY_DIR}/src  # 如果父项目有生成的头文件
)



find_package(OpenSSL REQUIRED)
add_executable(test_function 
               test_function.cpp
               ai_server_impl.cpp
               method_process.cpp
               ${PROTO_GEN_SRC})

if(ENABLE_DEBUG_INFO)
 target_compile_definitions(test_function PRIVATE DEBUG_INFO)
endif()
# # 链接父项目的静态库
target_link_libraries(test_function PRIVATE 
    rpc_static  # 直接使用父项目定义的库目标
    OpenSSL::SSL
    Boost::json
)
# 继承父项目的头文件路径（关键！）
target_include_directories(test_function PRIVATE
    ${CMAKE_SOURCE_DIR}/src  # 父项目源文件中的头文件
    ${CMAKE_BINARY_DIR}/src  # 如果父项目有生成的头文件
)

