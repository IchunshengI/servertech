#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/system/detail/error_code.hpp>
#include <chrono>
#include <cmath>
#include <memory>
#include <mutex>
#include "redis_pool.h"
#include "result_with_message.h"
#include "log/logger_wrapper.h"

namespace chat{
using rpc::error_with_message;
using connection_type = boost::redis::connection;
using chat::LOG;

RedisPool::RedisPool(boost::asio::any_io_executor ex, RedisPoolConfig redis_pool_config) : ex_(ex), closed_(false),
                     timer_(ex),
                     redis_pool_config_(std::move(redis_pool_config))
{

}

RedisPool::~RedisPool()
{
 Close();
}


awaitable<void> RedisPool::Init()
{
  co_await RunMaintenance();
  StartMaintenance();
}


/* 获取一个连接 */
//awaitable<result_with_message<std::shared_ptr<connection_type>>> RedisPool::GetConnection()
//GuardConnection
awaitable<result_with_message<GuardConnection>> RedisPool::GetConnection()
{

  {
    /* 检擦是否有空闲*/
    std::unique_lock<std::mutex> lock(mutex_);
    if(!idle_conns_.empty())
    {
      auto conn = idle_conns_.back().conn;
      idle_conns_.pop_back();
      total_count_--;
      co_return GuardConnection(this, conn);
    }
  }

  if (total_count_.load() < redis_pool_config_.max_connection_)
  {
    auto result = co_await CreateNewConnection();
    if(result.ec){
      LOG("Error") << " : " << result.ec.message();
      co_return error_with_message{result.ec, "CreateNewConnection error"};
    }
    co_return co_await GetConnection();
  }

  co_return error_with_message{rpc::errc::nums_error, "redis pool max nums limit"};
}

void RedisPool::StartMaintenance()
{
  timer_.expires_after(redis_pool_config_.max_idle_time_);
  timer_.async_wait([this](boost::system::error_code ec){
    if (!ec && this->closed_.load()){

      boost::asio::co_spawn(ex_, [this]() -> boost::asio::awaitable<void>{
        co_await this->RunMaintenance();
        this->StartMaintenance();
        co_return;
      },
      boost::asio::detached
      );
    }
  });
}

awaitable<void> RedisPool::RunMaintenance()
{ 
  {
    std::unique_lock<std::mutex> lock(mutex_);
    const auto now = std::chrono::steady_clock::now();

    /* 这里删除后会直接析构掉原来的连接对象 */
    // auto it = idle_conns_.remove_if([&](idle_entry& entry) {
    //   if((now - entry.last_used) > redis_pool_config_.max_idle_time_)
    //   {
    //     entry.conn->cancel();
    //     return true;
    //   }
    //   return false;
    // });
    auto it = std::remove_if(this->idle_conns_.begin(),
    this->idle_conns_.end(),
    [&](const auto& entry){
        return (now - entry.last_used) > this->redis_pool_config_.max_idle_time_;
    });
    this->idle_conns_.erase(it, this->idle_conns_.end());
    total_count_.store(idle_conns_.size());
  }

  while(total_count_.load() < redis_pool_config_.min_connection_){
    auto result = co_await CreateNewConnection();
    if(result.ec){
      LOG("Error") << "RunMaintenance error : " << result.ec.message();
      co_return;
    }
  }

}

awaitable<error_with_message> RedisPool::CreateNewConnection()
{
  boost::system::error_code ec;

  /* 这里必须延迟释放，不然后台线程--> boost这里是异步启动，有个后台线程用到了这块内存 */
  std::shared_ptr<connection_type> conn = std::shared_ptr<connection_type>(
    new connection_type(ex_),
    [&](connection_type* p) {
      p->cancel();
      boost::asio::post(ex_, [p]() {
        delete p;
      });
    }
  );

  conn->async_run(redis_pool_config_.redis_cfg_, {}, boost::asio::detached);

  /* 选择数据库 */
  if (redis_pool_config_.redis_cfg_.database_index.has_value() && *(redis_pool_config_.redis_cfg_.database_index) != 0)
  {
    boost::redis::request req;
    req.push("SELECT", std::to_string(*redis_pool_config_.redis_cfg_.database_index));
    boost::redis::response<std::string> select_resp;
    co_await conn->async_exec( 
        req, 
        select_resp,
        boost::asio::redirect_error(boost::asio::use_awaitable, ec)
    );
   if (ec) {
    LOG("Error") << "redis conn async_exec select error : " << ec.message();
    co_return error_with_message{ec, "redis conn async_exec error select error"};
  }

    auto& result = std::get<0>(select_resp);
    if (result.has_error()){
      LOG("Error") << "Select redis databases index error";
      co_return error_with_message{rpc::errc::redis_runtime_error, "Select redis databases index error"};
    }
  }

  std::unique_lock<std::mutex> lock(mutex_);
  if (total_count_.load() < redis_pool_config_.max_connection_)
  {
    idle_conns_.push_back({conn, std::chrono::steady_clock::now()});
    total_count_++;
  } else{
    LOG("Error") << "redis pool max nums limit";
    co_return error_with_message{rpc::errc::nums_error, "redis pool max nums limit"};
  }

  co_return error_with_message{ec, ""};
}

/* 
  返回一个连接到连接池中
*/
void RedisPool::ReturnConnection(std::shared_ptr<connection_type> conn)
{
  std::unique_lock<std::mutex> lock(mutex_);
   if (!closed_.load() && total_count_.load() < redis_pool_config_.max_connection_){
    idle_conns_.push_back({conn, std::chrono::steady_clock::now()});
    total_count_++;
  } 
}

/* 
  连接池关闭
*/
void RedisPool::Close()
{
  closed_.store(true);
  timer_.cancel();
  std::unique_lock<std::mutex> lock(mutex_);
  idle_conns_.clear();
  total_count_.store(0);
}


/*
  创建唯一实例 
*/
std::unique_ptr<RedisPool> create_redis_pool(boost::asio::any_io_executor ex)
{
  RedisPool::RedisPoolConfig redis_cfg("");
  return std::make_unique<RedisPool>(ex, std::move(redis_cfg));
}

}; /* namespace chat */