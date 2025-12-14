cmake_minimum_required(VERSION 3.20)
project(ProtoVsJsonTest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Debug 可选：你也可以用 cmake -DCMAKE_BUILD_TYPE=Debug 控制
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

# Boost JSON（Boost 装在 /usr/local 的话一般不用 PATHS；你要留也可以）
find_package(Boost 1.86 REQUIRED COMPONENTS json)

# Protobuf：用 CONFIG 模式（来自 /usr/local/lib/cmake/protobuf）
find_package(Protobuf CONFIG REQUIRED)

add_executable(performance
  performance.cpp
  addressbook.pb.cc
)

if(ENABLE_DEBUG_INFO)
  target_compile_definitions(performance PRIVATE DEBUG_INFO)
endif()

# 不要再写 protobuf / ${ABSL_LIBS}，直接用导出的 target
target_link_libraries(performance PRIVATE
  Boost::json
  protobuf::libprotobuf
)
