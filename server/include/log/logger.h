#ifndef SERVERTECHCHAT_SERVER_INCLUDE_LOGGER_H
#define SERVERTECHCHAT_SERVER_INCLUDE_LOGGER_H


#include <vector>
#include <string>
#include <mutex>
#include <boost/asio.hpp>

namespace chat{
class LogStream;
class RingBuffer;
class LogPersister;


class Logger{
friend class LogPersister;
public:

    //static Logger& instance();
    Logger(boost::asio::io_context& io_context, size_t time, std::string file_path, size_t file_size, size_t default_buffer_size = 8192);
    void append(const std::string log); 
    RingBuffer* getBufferPtr();
    LogStream operator()(const char*);   
    ~Logger(); 
private:
   
    void TimerCallBack(); 

    /* 后端写入组件 */
    boost::asio::io_context& io_context_;   /* 日志系统中异步操作使用的上下文 */
    boost::asio::steady_timer steady_timer_; /* 异步操作定时器 */
    LogPersister* logPersister_;
    size_t time_;                           /* 定时时间*/
    size_t file_size;                       /* 日志文件大小 */
    /* 前端写入组件 */
    RingBuffer* currBufferPtr_;
    RingBuffer* nextBufferPtr_;
    RingBuffer* fullBufferPtr_;
    size_t buffer_size_;
    std::mutex mutex_;
};

}

#endif