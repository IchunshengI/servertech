
#include <iostream>

//#define LOG(level) chat::Logger::instance()(level)
#include <boost/asio.hpp>

#include "LoggerWrapper.h"

boost::asio::io_context io_context;




int main()
{
    // 注册信号处理（Ctrl+C）

    chat::InitLogger(
        io_context, 
        1,                          // time (秒)
        "/home/tlx/project/servertech-chat/server/tools/Log/",   // 日志路径
        10 * 1024 * 1024,           // 日志文件大小 (10MB)
        8192                        // 缓冲区大小 (默认)
    );

    LOG("DEBUG") << "Thread message ";
    //persister.read();7
    LOG("DEBUG") << "HHHH1 ";
    //persister.read();
    LOG("DEBUG") << "HHHH2 ";
    LOG("DEBUG") << "HHHH3 ";
    LOG("DEBUG") << "HHHH4 ";
    LOG("DEBUG") << "HHHH5 ";
    //persister.read();
    LOG("DEBUG") << "HHHH6 ";
    LOG("DEBUG") << "HHHH1 ";
    LOG("DEBUG") << "HHHH2 ";
    io_context.run();
    
    return 0;
}