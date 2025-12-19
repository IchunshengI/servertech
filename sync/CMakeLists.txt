cmake_minimum_required(VERSION 3.16)
project(servertech_sync LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(PREPEND CMAKE_PREFIX_PATH "/usr/local")

find_package(mongocxx REQUIRED)
find_package(bsoncxx REQUIRED)

# librocketmq.so (built from rocketmq-client-cpp) commonly uses Boost.Log symbols
# in the "v2s_*" namespace (static-link variant). Linking Boost as static here
# satisfies those undefined references at final link time.
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
find_package(Boost REQUIRED COMPONENTS
  atomic
  chrono
  date_time
  filesystem
  iostreams
  locale
  log
  log_setup
  regex
  serialization
  system
  thread
)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

find_library(JSONCPP_LIB jsoncpp REQUIRED)
find_path(JSONCPP_INCLUDE_DIR json/json.h PATH_SUFFIXES jsoncpp REQUIRED)

find_library(ROCKETMQ_LIB rocketmq REQUIRED)
find_path(ROCKETMQ_INCLUDE_DIR rocketmq/DefaultMQPushConsumer.h REQUIRED)

# Be explicit about mongo-c-driver libs to avoid "DSO missing from command line"
find_library(MONGOC_LIB mongoc-1.0 REQUIRED)
find_library(BSON_LIB bson-1.0 REQUIRED)

add_executable(servertech-sync
  src/main.cpp
)

target_include_directories(servertech-sync
  PRIVATE
    ${JSONCPP_INCLUDE_DIR}
    ${ROCKETMQ_INCLUDE_DIR}
)

target_link_libraries(servertech-sync
  PRIVATE
    mongo::mongocxx_shared
    mongo::bsoncxx_shared
    ${JSONCPP_LIB}
    ${ROCKETMQ_LIB}
    Boost::atomic
    Boost::chrono
    Boost::date_time
    Boost::filesystem
    Boost::iostreams
    Boost::locale
    Boost::log
    Boost::log_setup
    Boost::regex
    Boost::serialization
    Boost::system
    Boost::thread
    ZLIB::ZLIB
    ${MONGOC_LIB}
    ${BSON_LIB}
    Threads::Threads
)

# librocketmq.so depends on Boost.*; with some toolchains/linkers the default
# --as-needed can drop transitive dependencies, causing undefined references.
if (UNIX AND NOT APPLE)
  target_link_options(servertech-sync PRIVATE "LINKER:--no-as-needed")
endif()
