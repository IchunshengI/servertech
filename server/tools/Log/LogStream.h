#ifndef SERVERTECHCHAT_SERVER_INCLUDE_LOGSTREAM_H
#define SERVERTECHCHAT_SERVER_INCLUDE_LOGSTREAM_H

#include <sstream>
#include <chrono>
#include <iomanip>
#include <cstring>
#include <atomic>
#include "Logger.h"
namespace chat{

class LogPersister;
class LogStream : public std::ostringstream {
public:

    explicit LogStream(Logger*, const char* level);
    ~LogStream();
private:
    static std::string formatTime(const ::std::chrono::system_clock::time_point &tp);
    Logger* logger_;
    const char* level_;
};


/* 这个对象不负责文件的实际写入 */
class RingBuffer{
    friend class LogPersister;
public:
    RingBuffer(size_t);
    //bool push(const std::string& log);
    int push(const std::string& log);
    bool is_full() const;
    void set_full();
    bool clear();
    // void persistToFile(FILE* file);
    ~RingBuffer();
private:
    size_t get_free_space() const;
    size_t capacity = 4;
    std::atomic<size_t> write_pos_;
    std::atomic<size_t> read_pos_;
    std::atomic<bool> is_full_;
    std::vector<char> buffer_;
};

}

#endif