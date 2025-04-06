#include "Logger.h"
#include <iostream>
#include "LogStream.h"

namespace chat{

Logger::Logger(size_t default_buffer_size):fullBufferPtr(nullptr){
    currBufferPtr = new RingBuffer(default_buffer_size);
    nextBufferPtr = new RingBuffer(default_buffer_size);
}

Logger& Logger::instance(){
    static Logger logger;
    return logger;
}

LogStream Logger::operator()(const char* level = "INFO"){
    return LogStream(this, level);
}


RingBuffer* Logger::getBufferPtr(){
    if(fullBufferPtr) return fullBufferPtr;
    else return currBufferPtr;
}

void Logger::append(const std::string log){
    std::cout << log;
    std::lock_guard<std::mutex> lock(mutex_);

    int result = currBufferPtr->push(log);
    int count = 0;
    if(result > 0){
        auto p = currBufferPtr;
        currBufferPtr->set_full();
        fullBufferPtr = currBufferPtr;
        while(true){
            if(!nextBufferPtr->is_full()){
                currBufferPtr = nextBufferPtr;
                nextBufferPtr = p;
                std::cout << currBufferPtr << std::endl;
                // nextBufferPtr->clear();
                result = currBufferPtr->push(log.substr(log.size() - result));
                break;
            }else{ /* 临界保护，强制退出 */
                count++;
                if(count > 100){
                    std::cerr << "Logger append error, can not switch the nextfullBuffer" << std::endl;
                    exit(0);
                }
                continue;
            }
        }

    }
}

}