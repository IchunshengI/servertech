
#pragma once
#include <memory>
#include <boost/asio.hpp>
#include "logger.h" 
#include "log_stream.h"


namespace chat {

// 初始化函数（必须调用一次）
// boost::asio::io_context& io_context, size_t time, std::string file_path, size_t file_size, size_t default_buffer_size = 8192
void InitLogger(boost::asio::io_context& io_context, size_t time=1, std::string file_path="./", size_t file_size=10 * 1024 * 1024, size_t default_buffer_size = 8192);

// 获取全局 Logger 引用（需先初始化）
Logger& GetLogger();

/* 重置logger */
void LoggerReset();

// 日志宏（直接使用）
inline auto LOG(const char* level) {
		return GetLogger()(level);
}

} // namespace chat