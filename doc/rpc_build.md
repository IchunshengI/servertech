# 编译说明
rpc部分学习参考使用的是此[开源项目](https://github.com/attackoncs/rpc/tree/main#)实现

编译的前提是已根据聊天室安装好了对应的boost

# 编译组件
**· Muduo**

    muduo的编译需要修改boost的路径，在muduo顶层cmakelists作如下修改
```c++
set(BOOST_ROOT "/home/tlx/project/boost_1_86_0")
set(Boost_INCLUDE_DIR "${BOOST_ROOT}/boost")  # 你的头文件在boost_1_86_0/boost下
set(Boost_LIBRARY_DIR "${BOOST_ROOT}/stage/lib")
set(Boost_ADDITIONAL_VERSIONS "1.86" "1.86.0")  # 帮助CMake识别版本
find_package(Boost 
  REQUIRED 
  COMPONENTS unit_test_framework  # 按需添加其他组件如system、program_options
  HINTS ${Boost_LIBRARY_DIR}      # 指定库搜索路径
  PATHS ${Boost_INCLUDE_DIR}      # 指定头文件搜索路径
)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(
  ${Boost_INCLUDE_DIR}   
  ${PROJECT_SOURCE_DIR}
)
link_directories(${Boost_LIBRARY_DIR})

// 之后在base/tests目录下的cmakelists将编译语句替换
if(Boost_FOUND)
message(STATUS "Boost_FOUND !!!!!!!!!!!!!!!!!!!!!!!")
add_executable(logstream_test LogStream_test.cc) // 主要是这个报错了
target_link_libraries(logstream_test muduo_base ${Boost_LIBRARIES}) // 手动添加boost
add_test(NAME logstream_test COMMAND logstream_test)
endif()
```




**· Zookeeper**

    安装zookeeper服务端、客户端以及开发套件
    sudo apt install zookeeper zookeeperd libzookeeper-mt-dev



*** google/protobuf**

    达到

# 编译
需要修改原有开源项目的cmakelists，去设置三个编译组件的库目录和头文件目录
```c++
# 设置muduo路径
set(MUDUO_ROOT_DIR "/home/tlx/project/muduo") 
set(MUDUO_BUILD_DIR "${MUDUO_ROOT_DIR}/build")     # Muduo编译目录
# 添加 Muduo 头文件路径（直接引用源码目录）
include_directories(
  ${MUDUO_ROOT_DIR}  # 核心头文件位于源码的 muduo 子目录下
)
# 添加 Muduo 编译后的库文件路径
link_directories(
  ${MUDUO_BUILD_DIR}/build/lib      # 指向编译生成的库文件目录
)
```

