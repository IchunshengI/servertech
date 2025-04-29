cmake_minimum_required(VERSION 3.12)
project(rpc LANGUAGES CXX)
# 设置C++20标准
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 定义一个选项（OPTION），允许手动开关调试日志
option(ENABLE_DEBUG_INFO "Enable debug logging" ON)  # 默认开启



# 设置静态库输出目录为项目根目录下的lib文件夹
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
# 确保目录存在
file(MAKE_DIRECTORY ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})

# 添加协程支持（GCC/Clang）
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fcoroutines)
endif()

# 添加子目录
add_subdirectory(src)
add_subdirectory(test)