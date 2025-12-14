add_library(rpc_log STATIC
  log_stream.cpp
  logger_wrapper.cpp
  log_persister.cpp
  logger.cpp
)

# 让 #include "log/xxx.h" 生效：把 tools/rpc/src 加进 include
target_include_directories(rpc_log PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/..     # => tools/rpc/src
)

target_compile_options(rpc_log PRIVATE -Wall -Wextra)
