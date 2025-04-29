/*
  Date	  :	2025年4月29日
  Author	:	chunsheng
  Brief		:	redis 具体插入删除的操作类
*/
#include <memory>
#pragma
#include <boost/asio/io_context.hpp>
#include "redis_pool.h"
#include "result_with_message.h"

namespace chat {

using rpc::result_with_message;
using rpc::error_with_message;
using boost::asio::awaitable;
/* 
  会话对应的基本信息单元
*/
struct SessionInfo{
  int user_id_;
  int session_id_;
  std::string api_key_;
};


class RedisClient{
public:
  RedisClient(boost::asio::io_context&);
  ~RedisClient();

  /* 传递初始信息并返回生成的session */
  awaitable<result_with_message<std::string>> SessionInit(SessionInfo);
private:
  static std::string GetRedisKey(std::string_view session);
  boost::asio::io_context& iox_;
  std::unique_ptr<RedisPool> redis_pool_;
};

std::unique_ptr<RedisClient> CreateRedisClient(boost::asio::io_context& iox);

} // namespace chat
