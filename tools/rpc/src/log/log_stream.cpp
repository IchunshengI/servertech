#include "log/log_stream.h"
#include <iostream>
namespace chat{

LogStream::LogStream(Logger* log_ptr, const char* level):logger_(log_ptr), level_(level){
    auto now = std::chrono::system_clock::now();
    *this << "[" << level_ <<"] " << "[" << formatTime(now) << "] ";
}


std::string LogStream::formatTime(const std::chrono::system_clock::time_point &tp){
    auto t = std::chrono::system_clock::to_time_t(tp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()) %1000;

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S") << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

LogStream::~LogStream(){
    logger_->append(this->str()+"\n"); /* 这里不用手动std::move 编译器会自动开启优化 */
}


RingBuffer::RingBuffer(size_t c): capacity(c), write_pos_(0),read_pos_(0),is_full_(false),buffer_(c){

}

RingBuffer::~RingBuffer()
{
    
}

bool RingBuffer::clear(){
    if(is_full_.load()){ /* 满的时候才可以清空 */
        read_pos_.store(0);
        write_pos_.store(1);
        is_full_.store(false);  /* 这里只会被一个后台线程调用，读取完数据后清空计数 */
        return true;
    }else{
        return false;
    }
}

inline size_t RingBuffer::get_free_space() const{
    const size_t curr_write = write_pos_.load();
    size_t next_write = (curr_write+1) % capacity;
    const size_t curr_read = read_pos_.load();

    if(curr_write == curr_read) return capacity-1;
    if(next_write == curr_read){
        return 0;  /* 说明当前缓冲区已经满了，停止刷入 */
    }else if(curr_write > curr_read){
        return capacity - curr_write + curr_read;
    }else{
        return curr_read - curr_write;
    }
}

int RingBuffer::push(const std::string& log){

    const size_t free_space = get_free_space();
    size_t log_size = log.size();

    if(free_space == 0) return log_size;

    
    const size_t writable  = std::min(log_size,free_space);
    const size_t first_chunk = std::min(writable , capacity - write_pos_.load());
    memcpy(buffer_.data()+write_pos_.load(), log.data(), first_chunk);

    if(first_chunk < writable ){
        memcpy(buffer_.data(), log.data()+first_chunk, writable - first_chunk);
    }

    write_pos_.store((write_pos_.load() + writable ) % capacity);
    return log_size - writable; /* 返回剩余未写入的数据 */
}

/* 压入数据，之前是会加锁的，所以这里不用加锁了,和后台线程是通过原子变量来同步的，不用加锁  */
// bool RingBuffer::push(const std::string&log){
//     // size_t  curr_write = write_pos_.load();
//      //size_t next_write = (curr_write+1) % capacity;

//     // if(next_write == read_pos_.load()){
//     //     is_full_.store(true);
//     //     return false;
//     // }
//     // buffer_[curr_write] = std::move(log);
//     // write_pos_.store(next_write);
//     return true;
// }

// void RingBuffer::persistToFile(FILE* file) {
//     size_t curr_read = read_pos_.load();
//     size_t curr_write = write_pos_.load();

//     if(curr_read != curr_write){
//         /* 计算可读数据量 */
//         size_t data_count = (curr_write > curr_read) ? curr_write - curr_read : (capacity - curr_read + curr_write);
//         /* 直接按顺序一次性写进去 */
//         for(size_t i=0; i<data_count; ++i){
//             size_t pos = (curr_read + i) % capacity;
//             fprintf(file, "%s", buffer_[pos].c_str());
//         }
//     }
// }


/* 判断是否为空 */
bool RingBuffer::is_full() const{
    return is_full_.load();
}

void RingBuffer::set_full(){
    is_full_.store(true);
}


}

