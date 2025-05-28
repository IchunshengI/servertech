cmake_minimum_required(VERSION 3.10)
project(ProtoVsJsonTest)

# 设置C++20标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE "Debug")


find_package(Boost REQUIRED COMPONENTS json PATHS /usr/local/lib)

set(PROTOBUF_ROOT_DIR "/home/tlx/project/protobuf-30.2")
set(PROTOBUF_INSTALL_DIR "${PROTOBUF_ROOT_DIR}/install")
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

add_executable(performance
               performance.cpp
               bigstringdata.pb.cc
              )

if(ENABLE_DEBUG_INFO)
 target_compile_definitions(performance PRIVATE DEBUG_INFO)
endif()
# # 链接父项目的静态库
target_link_libraries(performance PRIVATE 
    Boost::json
    protobuf  
    ${ABSL_LIBS}
)
