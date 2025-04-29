# 设置C++20标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE "Debug")

# 为GCC/Clang添加协程支持
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fcoroutines)
endif()

# protobuf相关
set(PROTOBUF_ROOT_DIR "/home/tlx/project/protobuf-30.2")
set(PROTOBUF_INSTALL_DIR "${PROTOBUF_ROOT_DIR}/install")
link_directories(
  ${PROTOBUF_INSTALL_DIR}/lib
)

include_directories(
  ${PROTOBUF_INSTALL_DIR}/include  # Protobuf核心头文件
  ${CMAKE_SOURCE_DIR}/ai_server  # 生成的.pb.h文件路径
)

find_package(Boost REQUIRED COMPONENTS headers context coroutine json PATHS /usr/local/lib)
find_package(OpenSSL REQUIRED)

set(AI_SERVER_DIR "${CMAKE_SOURCE_DIR}/ai_server")

add_executable(ai_server 
               server.cpp
               ${AI_SERVER_DIR}/ai_server_impl.cpp
               ${AI_SERVER_DIR}/method_process.cpp
               ${AI_SERVER_DIR}/ai_server.pb.cc)


# # 链接父项目的静态库
target_link_libraries(ai_server PRIVATE 
    rpc_static  # 直接使用父项目定义的库目标
    OpenSSL::SSL
    Boost::json
)
# 继承父项目的头文件路径（关键！）
target_include_directories(ai_server PRIVATE
    ${CMAKE_SOURCE_DIR}/src  # 父项目源文件中的头文件
)
# 设置 ai_server 的输出目录为 bin/client
set_target_properties(ai_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/server"
)
# 根据选项决定是否添加 DEBUG_LOG 宏
if(ENABLE_DEBUG_INFO)
  target_compile_definitions(ai_server PRIVATE DEBUG_INFO)
endif()

add_executable(ai_client 
               client.cpp 
               ${AI_SERVER_DIR}/ai_server.pb.cc)
# # 链接父项目的静态库
target_link_libraries(ai_client
    PRIVATE 
    rpc_static  # 直接使用父项目定义的库目标
)
# # 继承父项目的头文件路径（关键！）
target_include_directories(ai_client
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src  # 父项目源文件中的头文件
)
# 设置 ai_client 的输出目录为 bin/client
set_target_properties(ai_client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/client"
)


add_executable(test_function 
               test_function.cpp
               ${AI_SERVER_DIR}/ai_server_impl.cpp
               ${AI_SERVER_DIR}/method_process.cpp
               ${AI_SERVER_DIR}/ai_server.pb.cc)

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
)

set_target_properties(test_function PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/function"
)


add_executable(redis_test 
                redis_test.cpp
               ${AI_SERVER_DIR}/ai_server_impl.cpp
               ${AI_SERVER_DIR}/method_process.cpp
               ${AI_SERVER_DIR}/redis_pool.cpp
               ${AI_SERVER_DIR}/ai_server.pb.cc
               ${AI_SERVER_DIR}/boost/boost_redis.cpp)

if(ENABLE_DEBUG_INFO)
 target_compile_definitions(test_function PRIVATE DEBUG_INFO)
endif()
# # 链接父项目的静态库
target_link_libraries(redis_test PRIVATE 
    rpc_static  # 直接使用父项目定义的库目标
    OpenSSL::SSL
    Boost::json
)
# 继承父项目的头文件路径（关键！）
target_include_directories(redis_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src  # 父项目源文件中的头文件
)

set_target_properties(redis_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/function"
)
