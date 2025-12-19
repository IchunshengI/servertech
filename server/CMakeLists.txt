cmake_minimum_required(VERSION 3.16.0)
project(servertech_chat LANGUAGES CXX)

# C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 建议用命令行控制：cmake -DCMAKE_BUILD_TYPE=Debug
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

# GCC/Clang 协程支持（你想保留就保留）
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-fcoroutines)
endif()

# ---- 依赖 ----
find_package(Boost REQUIRED COMPONENTS headers context json regex url coroutine system)
find_package(OpenSSL REQUIRED)
find_package(ICU COMPONENTS data i18n uc REQUIRED)

# mongo-cxx-driver is typically installed under /usr/local in our dev container.
#list(PREPEND CMAKE_PREFIX_PATH "/usr/local")
find_package(mongocxx REQUIRED)
find_package(bsoncxx REQUIRED)

# ---- 主库 ----
add_library(servertech_chat STATIC
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
  src/services/mongodb_client.cpp
  src/services/persist_http_publisher.cpp
  src/services/persist_pending_retry_service.cpp
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

)

target_include_directories(servertech_chat
  PUBLIC
    include
    ${CMAKE_SOURCE_DIR}/tools/rpc/src
)

target_link_libraries(servertech_chat
  PUBLIC
    Boost::headers
    Boost::coroutine
    Boost::context
    Boost::json
    Boost::regex
    Boost::url
    Boost::system

    OpenSSL::Crypto
    OpenSSL::SSL

    ICU::data
    ICU::i18n
    ICU::uc

    mongo::mongocxx_shared
    mongo::bsoncxx_shared

    boost_charconv
    pthread

    rpc_client
)

# Separate compilation 宏
target_compile_definitions(servertech_chat
  PUBLIC
    BOOST_BEAST_SEPARATE_COMPILATION
    BOOST_ASIO_SEPARATE_COMPILATION
    BOOST_MYSQL_SEPARATE_COMPILATION
)

# Precompiled headers
target_precompile_headers(servertech_chat
  PRIVATE
    include/pch.hpp
)

# 可执行文件
add_executable(server-chat src/main.cpp)
target_link_libraries(server-chat PRIVATE servertech_chat rpc_client)

# Testing
# include(CTest)
# if (BUILD_TESTING)
#   add_subdirectory(test)
# endif()
