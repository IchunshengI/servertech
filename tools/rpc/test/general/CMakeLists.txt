# 设置C++20标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE "Debug")

# 为GCC/Clang添加协程支持
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fcoroutines)
endif()




include_directories(
  ${AI_SERVER_DIR}  # 生成的.pb.h文件路径
)

find_package(Boost REQUIRED COMPONENTS headers context coroutine json system  PATHS /usr/local/lib)
find_package(Threads REQUIRED)



add_executable(boost_async_compose 
               boost_async_compose.cpp
               )

if(ENABLE_DEBUG_INFO)
 target_compile_definitions(boost_async_compose PRIVATE DEBUG_INFO)
endif()
# # 链接父项目的静态库

target_compile_options(boost_async_compose
  PRIVATE
    -pthread
)
target_link_libraries(boost_async_compose PRIVATE 
    Boost::json
    Boost::headers
    Boost::coroutine
    Boost::context
    Boost::system
    Threads::Threads
)

set_target_properties(boost_async_compose PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/function"
)
