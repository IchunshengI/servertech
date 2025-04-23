cmake_minimum_required(VERSION 3.10)
project(boost_asio_coroutine_example)

set(CMAKE_CXX_STANDARD 17)

# 查找Boost组件
find_package(Boost 1.66.0 REQUIRED 
    COMPONENTS 
    system 
    coroutine 
    context 
    headers
)

# 显示找到的Boost路径
message(STATUS "Boost found in: ${Boost_INCLUDE_DIRS}")
message(STATUS "Boost libraries: ${Boost_LIBRARIES}")

add_executable(boost_test 
    boost_test.cpp
)

# 链接Boost库
target_link_libraries(boost_test
    PRIVATE
    Boost::boost
    Boost::system
    Boost::coroutine
    Boost::context
    pthread  # 需要链接pthread
)