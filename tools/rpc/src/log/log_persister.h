#ifndef SERVERTECHCHAT_SERVER_INCLUDE_LOGPERSISTER_H
#define SERVERTECHCHAT_SERVER_INCLUDE_LOGPERSISTER_H

#include "logger.h"
#include "log_stream.h"
#include <filesystem>
#include <cstdio>
#include <unistd.h>
namespace chat {


class LogPersister{
public:
    void read();
    LogPersister(Logger*,const std::string&, size_t);
    ~LogPersister();
private:
    void create_directory_if_needer(); /* 创建对应的文件目录 */
    void rotate_file();                /* 切换文件 */
    std::string generate_filename();   /* 生成带时间戳和序号的文件名 */
    void persiste_to_disk();           /* 持久化到磁盘 */

    Logger* logger_;              /* 前端日志系统 */
    std::string file_prefix_;     /* 日志文件的前缀 */
    size_t current_file_size_;    /* 当前日志文件的写入大小 */
    size_t max_file_size_;        /* 日志文件的最大值 */ 
    uint64_t file_index_;         /* 文件序号 */
    std::FILE* file_;              /* 文件结构体 */
    int fd_;                       /* 文件描述符 */

};
}

#endif