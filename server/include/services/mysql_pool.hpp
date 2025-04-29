#ifndef SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_MYSQL_POOL_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_MYSQL_POOL_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/core/span.hpp>
#include <boost/variant2/variant.hpp>
#include <boost/mysql.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <deque>
#include <mutex>
#include "business_types.hpp"
#include "error.hpp"


namespace chat{
/* 数据库连接池 */
class mysql_pool : public std::enable_shared_from_this<mysql_pool>{
    
public:
    struct config{
        std::chrono::seconds max_idle_time{300};     /* 最大空闲时间 */
        std::size_t max_connections{20};                 /* 最大连接数 */
        std::size_t min_connections{5};                  /*  最小空闲连接 */
    };

    using connection_type = boost::mysql::tcp_connection;

    mysql_pool(boost::asio::any_io_executor ex, std::string host, boost::mysql::handshake_params params, config cfg);
    ~mysql_pool();
    /* 异步获取连接? */
    // 协程专用接口
    std::pair<boost::system::error_code, connection_type>  get_connection(boost::asio::yield_context yield);
    void return_connection(connection_type conn);
    void init();
    void close();
private:
    void start_maintenance();
    void run_maintenance(boost::asio::yield_context yield);
    bool create_new_connection(boost::asio::yield_context yield);
   // void schedule_idle_check();
    /* 获取的连接 */
    struct idle_entry {
        connection_type conn;
        std::chrono::steady_clock::time_point last_used;
    };

    std::mutex mutex_;
    boost::asio::any_io_executor ex_;       /* 异步执行器*/
    std::string host_;                      /* 连接目前ip*/
    boost::mysql::handshake_params params_; /* 连接参数 */
    config cfg_;                            /* 连接池配置信息 */
    boost::asio::steady_timer timer_;       /* 定时器 */
    std::deque<idle_entry> idle_conns_;     /* 目前空闲的连接队列 */ /* 指定大小然后加个锁 */
    std::atomic<int> total_count_{0};            /* 总连接数目 */
    //std::deque<std::function<void(boost::system::error_code, connection_type)>> waiting_queue_; /* 等待队列 */
    // bool closed_{false};
    // bool idle_check_scheduled_{false};
};


}

#endif