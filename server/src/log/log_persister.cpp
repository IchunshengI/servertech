#include "log/log_persister.h"

#include <iostream>
namespace chat {

LogPersister::LogPersister(Logger* log, const std::string& file_prefix,size_t max_size):logger_(log),file_prefix_(file_prefix),max_file_size_(max_size),file_(nullptr){

    /* 创建对应的目录及文件*/
    create_directory_if_needer();
    rotate_file();
}


LogPersister::~LogPersister(){
    persiste_to_disk();
    fclose(file_);
}


/* 创建对应的文件目录 */
void LogPersister::create_directory_if_needer(){
    std::filesystem::path p(file_prefix_);
    if(p.has_parent_path() && !std::filesystem::exists(p.parent_path())){
        std::filesystem::create_directories(p.parent_path());
    }
}
/* 切换文件 */
void LogPersister::rotate_file(){
    if(file_){
        fclose(file_);
    }

    file_ = std::fopen(generate_filename().c_str(),"a");
    if(!file_){
        throw std::runtime_error("Failed to open log file");
    }
    fd_ = fileno(file_);
    current_file_size_ = 0;
}

/* 生成带时间戳和序号的文件名 */   
std::string LogPersister::generate_filename(){
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&timestamp);

    std::ostringstream oss;
    oss << file_prefix_  << std::put_time(&tm, "%Y%m%d_%H%M%S") << "_" <<++file_index_ <<".log";
    return oss.str();
}

void LogPersister::persiste_to_disk(){
    fflush(file_);
    fsync(fd_);
}

void LogPersister::read(){
    RingBuffer* read_buffer = logger_->getBufferPtr();
    const size_t curr_read = read_buffer->read_pos_.load();
    const size_t curr_write = read_buffer->write_pos_.load();

    /* 说明目前没有数据 */
    if(curr_read == curr_write) return;

    char* data_ptr = data_ptr = read_buffer->buffer_.data() + curr_read;
    int len = 0;


    /* 这里替换成写入文件的操作就可以 */
    if(curr_write > curr_read){
        len = curr_write - curr_read;
        if(current_file_size_ + len > max_file_size_){
            std::cerr << "切换文件" << std::endl;
            rotate_file();
        }
        fwrite(data_ptr,1,len,file_);
        current_file_size_ += len;
        read_buffer->read_pos_.store(curr_write);
        persiste_to_disk();
        
    }else{
        len = read_buffer->capacity - curr_read;
        if(current_file_size_ + len > max_file_size_){
            rotate_file();
        }
        fwrite(data_ptr,1,len,file_);
        current_file_size_ += len;
        read_buffer->read_pos_.store(0);
        read(); /* 递归一下 */
    }
}

}