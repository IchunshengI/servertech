#include <boost/asio/awaitable.hpp>
#include <boost/system/detail/error_code.hpp>
#include "log/logger_wrapper.h"
#include "redis_client.h"
#include "result_with_message.h"
#include "until/general.hpp"
namespace chat
{

using boost::asio::awaitable;
using chat::LOG;
using rpc::error_with_message;

RedisClient::RedisClient(boost::asio::io_context&iox) : iox_(iox){
  redis_pool_ = create_redis_pool(iox);
}

RedisClient::~RedisClient()
{

}

awaitable<result_with_message<std::string>> RedisClient::SessionInit(SessionInfo info)
{
  std::string redis_key = GetRedisKey(GenerateSessionToken());
  auto result = co_await redis_pool_->GetConnection();
  if (result.has_error())
  {
    LOG("Error") << "getconnection error";
    co_return error_with_message{rpc::errc::not_found, "can not get redis connection"};
  }
  auto& conn = result.value();
  using RespType = std::tuple<int, int>; 
  boost::redis::request req;
  boost::redis::ignore_t resp; 
  std::chrono::seconds ttl_seconds(3600*24);
  // 使用 HSET 存储结构化数据到 Hash
  req.push("HSET", redis_key,
           "user_id",    std::to_string(info.user_id_),
           "session_id", std::to_string(info.session_id_),
           "api_key",    info.api_key_);
  req.push("EXPIRE", redis_key, ttl_seconds.count());
  boost::system::error_code ec;
   co_await conn->async_exec(req, resp, boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    if (ec){
      LOG("Error") << "redis async_exec error";
      co_return error_with_message{ec, ec.message()};
    }

  co_return redis_key;

}


std::string RedisClient::GetRedisKey(std::string_view session)
{
  constexpr std::string_view prefix = "ai_server_";

  std::string res;
  res.reserve(prefix.size() + session.size());
  res += prefix;
  res += session;
  return res;
}

awaitable<result_with_message<std::string>> RedisClient::GetApiKey(std::string& session_token)
{
  std::string api_key;
  boost::system::error_code ec;
  auto conn_result = co_await redis_pool_->GetConnection();
  if (conn_result.has_error())
  {
    LOG("Error") << "Get redisconnection error";
    co_return error_with_message{rpc::errc::redis_runtime_error, "Get redisconnection error"};
  }

  auto& conn = conn_result.value();
  boost::redis::request req;
  boost::redis::response<std::int64_t, std::map<std::string, std::string>> resp;
  req.push("EXISTS", session_token);
  req.push("HGETALL", session_token);  

  co_await conn->async_exec(req, resp, boost::asio::redirect_error(boost::asio::use_awaitable, ec));

  if (ec) {
    LOG("Error") << "redis async_exec error: " << ec.message();
    co_return error_with_message{ec, "redis async_exec error"};
  }

  auto key_exists = std::get<0>(resp);  // EXISTS 返回 0/1
  if (key_exists == 0) {
    LOG("Error") << "session key not found in redis" << std::endl;
    co_return error_with_message{rpc::errc::not_found, "session key not found in redis"};
  }

  const auto& hash = std::get<1>(resp);
  api_key = hash->at("api_key");
  co_return api_key;
}

} // namespace chat