#
# Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

cmake_minimum_required(VERSION 3.16.0)
project(servertech_chat)

# 设置C++20标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE "Debug")

# 为GCC/Clang添加协程支持
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fcoroutines)
endif()


set(PROTOBUF_ROOT_DIR "/home/tlx/project/protobuf-30.2")
set(PROTOBUF_INSTALL_DIR "${PROTOBUF_ROOT_DIR}/install")
# 自动查找所有 Abseil 库文件
file(GLOB ABSL_LIBS "${PROTOBUF_INSTALL_DIR}/lib/libabsl*.so")

# 添加Protobuf库文件路径
link_directories(
  ${PROTOBUF_INSTALL_DIR}/lib
)
# Boost.Context is required to run stackful coroutines
# find_package(Boost REQUIRED COMPONENTS headers context json regex url)
# Darren修改boost路径为/usr/local/lib，因为我们安装的boost是在该路径下
find_package(Boost REQUIRED COMPONENTS headers context json regex url coroutine system PATHS /usr/local/lib)
#find_package(Boost REQUIRED COMPONENTS headers context json regex url)
 
# OpenSSL is required by Boost.Redis
find_package(OpenSSL REQUIRED)

# ICU is required by Boost.Regex
find_package(ICU COMPONENTS data i18n uc REQUIRED)

# 先生成rpc所需要的头文件
add_subdirectory(src/rpc)
# This library is consumed by the actual server and the tests
add_library(
    servertech_chat
    STATIC

    # Header-only libraries with optional separate-source mode
    src/separate-compilation/boost_asio.cpp
    src/separate-compilation/boost_beast.cpp
    src/separate-compilation/boost_redis.cpp
    src/separate-compilation/boost_mysql.cpp

    # Utilities
    src/util/base64.cpp
    src/util/email.cpp
    src/util/scrypt.cpp
    src/util/password_hash.cpp
    src/util/cookie.cpp
    src/util/websocket.cpp

    # Services
    src/services/redis_serialization.cpp
    src/services/redis_client.cpp
    src/services/mysql_client.cpp
    src/services/session_store.cpp
    src/services/cookie_auth_service.cpp
    src/services/room_history_service.cpp
    src/services/pubsub_service.cpp
    src/services/mysql_pool.cpp

    # API
    src/api/api_types.cpp
    src/api/auth.cpp
    src/api/chat_websocket.cpp

    # Server
    src/static_files.cpp
    src/listener.cpp
    src/http_session.cpp
    src/request_context.cpp
    src/shared_state.cpp
    src/error.cpp

    # Log 
    src/log/logger.cpp
    src/log/logger_wrapper.cpp
    src/log/log_stream.cpp
    src/log/log_persister.cpp

    # rpc
    src/rpc/rpc_meta.pb.cc
    src/rpc/ai_server.pb.cc
    src/rpc/rpc_channel.cpp 
    src/rpc/rpc_client.cpp
    src/rpc/zookeeper_op.cpp
    src/rpc/config/config.cpp

)



target_include_directories(servertech_chat
   PUBLIC include
   ${PROTOBUF_INSTALL_DIR}/include)

target_link_libraries(
    servertech_chat
    PUBLIC
    Boost::headers
    Boost::coroutine
    Boost::context
    Boost::json
    Boost::regex
    Boost::url
    OpenSSL::Crypto
    OpenSSL::SSL
    ICU::data
    ICU::i18n
    ICU::uc
    Boost::system
    boost_charconv
    pthread
    ${ABSL_LIBS}
    protobuf
    zookeeper_mt
)

# These are required because we're building Boost.Asio, Boost.Beast and Boost.MySQL
# in separate-compilation mode (see boost_asio.cpp, boost_beast.cpp, boost_mysql.cpp).
# Boost.Redis doesn't need such definition, even when building it like we're doing.
target_compile_definitions(
    servertech_chat
    PUBLIC
    BOOST_BEAST_SEPARATE_COMPILATION
    BOOST_ASIO_SEPARATE_COMPILATION
    BOOST_MYSQL_SEPARATE_COMPILATION
)

# Precompiled headers, to reduce build times
target_precompile_headers(
    servertech_chat
    PRIVATE
    include/pch.hpp
)

# The actual server executable
add_executable(server-chat src/main.cpp)
target_link_libraries(server-chat PRIVATE servertech_chat)

# Testing
include(CTest)
if (BUILD_TESTING)
    add_subdirectory(test)
endif()
