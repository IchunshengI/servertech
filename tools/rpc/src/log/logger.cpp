#include "log/logger.h"
#include <iostream>
#include "log/log_stream.h"
#include "log/log_persister.h"

namespace chat{

Logger::Logger(boost::asio::io_context& io_context, 
               size_t time, 
               std::string file_path,
               size_t file_size, 
               size_t default_buffer_size) : io_context_(io_context),
                                             steady_timer_(io_context),
																						 time_(time) {
    currBufferPtr_ = new RingBuffer(default_buffer_size);
    nextBufferPtr_ = new RingBuffer(default_buffer_size);
    logPersister_ = new LogPersister(this, std::move(file_path), file_size);
		fullBufferPtr_ = nullptr;
    TimerCallBack(); /* 启用定时回调 */
}

Logger::~Logger(){
    steady_timer_.cancel();
    logPersister_->read();
    delete currBufferPtr_;
    delete nextBufferPtr_;
    delete logPersister_;
}

void Logger::TimerCallBack(){
    steady_timer_.expires_after(std::chrono::seconds(time_));
    steady_timer_.async_wait([&](const boost::system::error_code& ec) {
        if (!ec) {
            TimerCallBack();
        }
    });
    logPersister_->read();
}

LogStream Logger::operator()(const char* level = "INFO"){
    return LogStream(this, level);
}


RingBuffer* Logger::getBufferPtr(){
    if(fullBufferPtr_) return fullBufferPtr_;
    else return currBufferPtr_;
}

void Logger::append(const std::string log){
    std::cout << log;
    std::lock_guard<std::mutex> lock(mutex_);

    int result = currBufferPtr_->push(log);
    int count = 0;
    if(result > 0){
        auto p = currBufferPtr_;
        currBufferPtr_->set_full();
        fullBufferPtr_ = currBufferPtr_;
        while(true){
            if(!nextBufferPtr_->is_full()){
                currBufferPtr_ = nextBufferPtr_;
                nextBufferPtr_ = p;
                std::cout << currBufferPtr_ << std::endl;
                // nextBufferPtr->clear();
                result = currBufferPtr_->push(log.substr(log.size() - result));
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
} // namespace chat

}