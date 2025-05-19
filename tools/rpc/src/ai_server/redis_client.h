/*
  Date	  :	2025年4月29日
  Author	:	chunsheng
  Brief		:	redis 具体插入删除的操作类
*/
#include <boost/asio/awaitable.hpp>
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
  RedisClient(boost::asio::any_io_executor);
  ~RedisClient();

  /* 传递初始信息并返回生成的session */
  awaitable<result_with_message<std::string>> SessionInit(SessionInfo);
  awaitable<result_with_message<std::string>> GetApiKey(std::string& session_token);
  awaitable<result_with_message<SessionInfo>> GetSessionInfo(const std::string& session_token);
private:
  static std::string GetRedisKey(std::string_view session);
  boost::asio::any_io_executor ex_;
  std::unique_ptr<RedisPool> redis_pool_;
};

std::unique_ptr<RedisClient> CreateRedisClient(boost::asio::any_io_executor ex);

} // namespace chat
