#include "Logger.h"
#include "LogStream.h"
#include <iostream>
#include "LogPersister.h"
#define LOG(level) chat::Logger::instance()(level)

std::string path = "/home/tlx/project/servertech-chat/server/tools/Log/";
size_t log_size = 1024*1024*10;
chat::Logger& imp_log = chat::Logger::instance();

#include <boost/asio.hpp>

namespace asio = boost::asio;


void timer_callback(chat::LogPersister& persister, 
                   asio::steady_timer& timer) {
    timer.expires_after(std::chrono::seconds(1));
    timer.async_wait([&](const boost::system::error_code& ec) {
        if (!ec) {
            timer_callback(persister, timer);
        }
    });
    persister.read();
}

int a = 5;
#include <vector>
std::vector<std::vector<int>> g{{2,3},{2,2}};
int main()
{

    asio::io_context io_context;
    chat::LogPersister persister(&imp_log, path, log_size);

    asio::steady_timer timer(io_context);
    timer_callback(persister, timer);
//    chat::LogPersister persister(&imp_log, path, log_size);
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
    //persister.read();

    /* 这里要安排一个类，把里面的数据都给读出来先 */
    
   // persister.read();
    //std::FILE* file;
    return 0;
}