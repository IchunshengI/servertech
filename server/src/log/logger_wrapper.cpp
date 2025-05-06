#include "log/logger_wrapper.h"
//#include <csignal>
#include <atomic>

namespace chat {

// 全局变量定义（仅在此文件中）
static std::unique_ptr<Logger> logger_ptr = nullptr;
// static boost::asio::io_context io_context;
// static std::atomic<bool> stop_flag{false};

// 信号处理函数
// void handle_signal(int signum) {
//     logger_ptr.reset(); // 安全释放 Logger
// }

/* 
        ioc, 
        1,                          // time (秒)
        "./",                       // 日志路径
        10 * 1024 * 1024,           // 日志文件大小 (10MB)
        8192                        // 8kb
*/
void InitLogger(boost::asio::io_context& io_context, size_t time, std::string file_path, size_t file_size, size_t default_buffer_size){
    //static std::once_flag init_flag;
   
    // std::signal(SIGINT, handle_signal);
    // std::signal(SIGTERM, handle_signal);
    
    // 创建 Logger 实例
    logger_ptr.reset(new chat::Logger(io_context,time,file_path, file_size,default_buffer_size));
    
    // 启动 io_context 线程（可选，根据实际需求）
    //std::thread([&]() { io_context.run(); }).detach();
   
}

void LoggerReset(){
    logger_ptr.reset(); // 安全释放 Logger
}

// 获取全局 Logger 引用
Logger& GetLogger() {
    if (!logger_ptr) {
        throw std::runtime_error("Logger not initialized! Call InitLogger first.");
    }
    return *logger_ptr;
}

} // namespace chat