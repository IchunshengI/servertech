cmake_minimum_required(VERSION 3.20)

# test 子目录一般不需要 project()，但写了也没事
project(rpc_test_ai_server LANGUAGES CXX)

# C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

# GCC/Clang 协程支持（你要保留就保留）
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-fcoroutines)
endif()

# ---- 依赖（test 自己用到的才找）----
find_package(Boost 1.86 REQUIRED COMPONENTS headers context coroutine json)
find_package(OpenSSL REQUIRED)
find_package(mongocxx REQUIRED)
find_package(Threads REQUIRED)

# 这里是 tools/rpc/src/ai_server 的源码目录
set(AI_SERVER_DIR "${CMAKE_SOURCE_DIR}/src/ai_server")

# ------------------- ai_server (测试用服务端) -------------------
add_executable(ai_server
  server.cpp

  ${AI_SERVER_DIR}/ai_server_impl.cpp
  ${AI_SERVER_DIR}/method_process.cpp

  ${AI_SERVER_DIR}/redis_pool.cpp
  ${AI_SERVER_DIR}/boost/boost_redis.cpp
  ${AI_SERVER_DIR}/redis_client.cpp

  ${AI_SERVER_DIR}/thread_poll/thread_poll.cpp
  ${AI_SERVER_DIR}/mongodb/mongodb_client.cpp

  ${CMAKE_SOURCE_DIR}/src/until/base64.cpp
)

target_include_directories(ai_server PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${AI_SERVER_DIR}
)

# ✅ 关键：服务端用 rpc_server（不是 rpc_static）
# ✅ pb 复用 ai_server_proto（不要再 protobuf_generate）
target_link_libraries(ai_server PRIVATE
  rpc_server
  ai_server_proto

  OpenSSL::SSL
  Boost::json
  mongo::mongocxx_shared
  mongo::bsoncxx_shared
  Threads::Threads
)

set_target_properties(ai_server PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/server"
)

if(ENABLE_DEBUG_INFO)
  target_compile_definitions(ai_server PRIVATE DEBUG_INFO)
endif()

# ------------------- ai_client (测试用客户端) -------------------
add_executable(ai_client
  client.cpp
)

target_include_directories(ai_client PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${AI_SERVER_DIR}
)

# ✅ 关键：客户端用 rpc_client
# ✅ 同样复用 ai_server_proto（需要 pb.h + pb.cc）
target_link_libraries(ai_client PRIVATE
  rpc_client
  ai_server_proto
)

set_target_properties(ai_client PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/client"
)

# add_executable(test_function 
#                test_function.cpp
#                ${AI_SERVER_DIR}/ai_server_impl.cpp
#                ${AI_SERVER_DIR}/method_process.cpp
#                ${AI_SERVER_DIR}/ai_server.pb.cc
#                ${AI_SERVER_DIR}/boost/boost_redis.cpp
#                ${AI_SERVER_DIR}/redis_pool.cpp
#                ${AI_SERVER_DIR}/redis_client.cpp
#                ${CMAKE_SOURCE_DIR}/src/until/general.hpp
#                ${CMAKE_SOURCE_DIR}/src/until/base64.cpp
#                )

# if(ENABLE_DEBUG_INFO)
#  target_compile_definitions(test_function PRIVATE DEBUG_INFO)
# endif()
# # # 链接父项目的静态库
# target_link_libraries(test_function PRIVATE 
#     rpc_static  # 直接使用父项目定义的库目标
#     OpenSSL::SSL
#     Boost::json
# )
# # 继承父项目的头文件路径（关键！）
# target_include_directories(test_function PRIVATE
#     ${CMAKE_SOURCE_DIR}/src  # 父项目源文件中的头文件
# )

# set_target_properties(test_function PROPERTIES
#     RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/function"
# )


# add_executable(redis_test 
#                 redis_test.cpp
#                ${AI_SERVER_DIR}/ai_server_impl.cpp
#                ${AI_SERVER_DIR}/method_process.cpp
#                ${AI_SERVER_DIR}/redis_pool.cpp
#                ${AI_SERVER_DIR}/ai_server.pb.cc
#                ${AI_SERVER_DIR}/boost/boost_redis.cpp
               
#                ${AI_SERVER_DIR}/redis_client.cpp
#                ${CMAKE_SOURCE_DIR}/src/until/general.hpp
#                ${CMAKE_SOURCE_DIR}/src/until/base64.cpp
#                )

# if(ENABLE_DEBUG_INFO)
#  target_compile_definitions(test_function PRIVATE DEBUG_INFO)
# endif()
# # # 链接父项目的静态库
# target_link_libraries(redis_test PRIVATE 
#     rpc_static  # 直接使用父项目定义的库目标
#     OpenSSL::SSL
#     Boost::json
# )
# # 继承父项目的头文件路径（关键！）
# target_include_directories(redis_test PRIVATE
#     ${CMAKE_SOURCE_DIR}/src  # 父项目源文件中的头文件
# )

# set_target_properties(redis_test PROPERTIES
#     RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/function"
# )
