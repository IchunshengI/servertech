/*
  Date	  :	2025年4月27日
  Author	:	chunsheng
  Brief		:	redis连接池
*/

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/redis.hpp>
#include <boost/asio.hpp>
#include <boost/redis/config.hpp>
#include <boost/redis/connection.hpp>
#include <cstddef>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include "result_with_message.h"
#include <deque>
namespace chat{

using rpc::result_with_message;
using rpc::error_with_message;
using boost::asio::awaitable;


class GuardConnection;
class RedisPool 
{
using connection_type = boost::redis::connection;
public:
  /* 连接池的配置信息类 */
  struct RedisPoolConfig{
    RedisPoolConfig(std::string passward=""){
      redis_cfg_.addr.host = "127.0.0.1";
      redis_cfg_.addr.port = "6379"; 
      redis_cfg_.database_index = 1;
      redis_cfg_.password = passward;
      // 超时设置
      redis_cfg_.connect_timeout = std::chrono::seconds(3);
      redis_cfg_.resolve_timeout = std::chrono::seconds(5);
    }

    std::chrono::seconds max_idle_time_{300};  /* 连接最大空闲时间*/
    std::size_t max_connection_{20};               /* 最大连接数 */
    std::size_t min_connection_{5};                /* 最小空闲连接 */
    boost::redis::config redis_cfg_;               /* redis的连接配置 */
  }; /* RedisPoolConfig */

  RedisPool(boost::asio::any_io_executor ex, RedisPoolConfig redis_pool_config);
  ~RedisPool();
  void ReturnConnection(std::shared_ptr<connection_type>);
  awaitable<void> Init();
  awaitable<result_with_message<GuardConnection>> GetConnection();
  void Close();

private:
  void StartMaintenance();
  awaitable<void> RunMaintenance();
  awaitable<error_with_message> CreateNewConnection();

  struct idle_entry{
    std::shared_ptr<connection_type> conn;
    std::chrono::steady_clock::time_point last_used;
  };
  std::mutex mutex_;
  boost::asio::any_io_executor ex_;
  RedisPoolConfig redis_pool_config_;
  boost::asio::steady_timer timer_;
  std::atomic<int> total_count_;
  std::deque<idle_entry> idle_conns_;    /* 空闲的连接链表，这里用链表的原因是redis connection不支持移动赋值=语义 */
  std::atomic<bool> closed_;            /* 连接池关闭 */
  
};

/* RAII保护 */
class GuardConnection
{
using connection_type = boost::redis::connection;
public:
  GuardConnection(RedisPool* redis_pool, std::shared_ptr<connection_type> conn) : redis_pool_ptr_(redis_pool), conn_(conn), has_value_(true)
  {
    
    
  }
  GuardConnection(const connection_type&) = delete;
    // 添加移动构造函数
  GuardConnection(GuardConnection&& rhs) noexcept
      : redis_pool_ptr_(rhs.redis_pool_ptr_),
        conn_(std::move(rhs.conn_)),
        has_value_(true) {
        rhs.has_value_ = false; // 防止 rhs 析构时归还连接
  }

  ~GuardConnection()
  { 
   
    if (!has_value_) 
    { 
      return;
    }
    redis_pool_ptr_->ReturnConnection(conn_);
  }

  GuardConnection& operator=(const GuardConnection&) = delete;
  GuardConnection& operator=(GuardConnection&& rhs) = delete;


  connection_type* operator->() noexcept {return conn_.get();}
private:
  bool has_value_;
  std::shared_ptr<connection_type> conn_;
  RedisPool* redis_pool_ptr_;
};

std::unique_ptr<RedisPool> create_redis_pool(boost::asio::any_io_executor ex);

}; /* namespace chat */
