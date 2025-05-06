cmake_minimum_required(VERSION 3.27)
project(mongo_test)

set(CMAKE_CXX_STANDARD 20)

# 设置 mongocxx 安装路径（如果你手动装到了 /usr/local）
set(CMAKE_PREFIX_PATH "/usr/local")

# 查找 mongocxx（依赖 bsoncxx 会自动找）
find_package(mongocxx REQUIRED)

add_executable(mongo_test main.cpp)

# 链接 mongocxx 和 bsoncxx
target_link_libraries(mongo_test
    mongo::mongocxx_shared
    mongo::bsoncxx_shared
)