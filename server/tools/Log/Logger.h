#ifndef SERVERTECHCHAT_SERVER_INCLUDE_LOGGER_H
#define SERVERTECHCHAT_SERVER_INCLUDE_LOGGER_H


#include <vector>
#include <string>
#include <mutex>


namespace chat{
class LogStream;
class RingBuffer;


class Logger{

public:

    static Logger& instance();
    void append(const std::string log); 
    RingBuffer* getBufferPtr();
    LogStream operator()(const char*);   

private:
    Logger(size_t default_buffer_size = 8192);
    RingBuffer* currBufferPtr;
    RingBuffer* nextBufferPtr;
    RingBuffer* fullBufferPtr;
    std::mutex mutex_;
};

}

#endif