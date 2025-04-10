
#pragma once
#include <memory>
#include <boost/asio.hpp>
#include "Logger.h" 
#include "LogStream.h"


namespace chat {

// 初始化函数（必须调用一次）
// boost::asio::io_context& io_context, size_t time, std::string file_path, size_t file_size, size_t default_buffer_size = 8192
void InitLogger(boost::asio::io_context& io_context, size_t time, std::string file_path, size_t file_size, size_t default_buffer_size = 8192);

// 获取全局 Logger 引用（需先初始化）
Logger& GetLogger();

// 日志宏（直接使用）
#define LOG(level) (chat::GetLogger())(level)

} // namespace chat