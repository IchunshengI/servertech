#pragma once

/*
  Date	  :	2025年5月4日
  Author	:	chunsheng
  Brief		:	rpc客户端实现类，boost协程接口，方便用户直接引用, 其实现的内容是和ai_server_impl.h的rpc接口是对应的
*/

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <string>
#include <memory>
#include "rpc/result_with_message.h"
#include "rpc/rpc_channel.h"
#include "rpc/rpc_controller.h"
#include "rpc/ai_server.pb.h"
namespace rpc {

using boost::asio::awaitable;
using rpc::result_with_message;
using rpc::RpcController;
using rpc::RpcChannel;
class RpcClient{

public:
  RpcClient(boost::asio::any_io_executor ex);
  ~RpcClient();

  // 这两个都要设置成协程接口的形式
  awaitable<bool> SetInitInfo(uint32_t user_id, uint32_t session_id, std::string api_key);
  awaitable<result_with_message<std::string>> Query(std::string query);
private:
  boost::asio::any_io_executor ex_;
  std::shared_ptr<RpcController> controller_;
  std::shared_ptr<RpcChannel> channel_;
  std::shared_ptr<AiServer_Stub> ai_server_stub_;
  std::string token_;  
};

}

