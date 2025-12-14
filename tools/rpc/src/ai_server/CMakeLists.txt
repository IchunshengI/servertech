find_package(Protobuf CONFIG REQUIRED)

add_library(ai_server_proto STATIC)

# 生成 ai_server.proto -> ai_server.pb.cc/.h
protobuf_generate(
  TARGET ai_server_proto
  LANGUAGE cpp
  PROTOS ${CMAKE_CURRENT_SOURCE_DIR}/ai_server.proto
  IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
  PROTOC_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}
)

# 暴露生成头文件路径
target_include_directories(ai_server_proto PUBLIC
  ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(ai_server_proto PUBLIC
  protobuf::libprotobuf
)

