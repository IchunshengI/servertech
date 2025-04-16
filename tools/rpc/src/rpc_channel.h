/*
  Date	  :	2025年4月15日
  Author	:	chunsheng
  Brief		:	rpc客户端调用channel抽象类
*/

#ifndef RPC_CHANNEL_H
#define RPC_CHANNEL_H

#include "google/protobuf/service.h"
#include "boost/asio.hpp"
#include "result_with_message.h"
namespace rpc{

class RpcChannel : public google::protobuf::RpcChannel{
 public:
  RpcChannel() {};
  virtual ~RpcChannel() = default;
  virtual void CallMethod(const ::google::protobuf::MethodDescriptor* method,
                          google::protobuf::RpcController* controller,
                          const google::protobuf::Message* request,
                          google::protobuf::Message* response,
                          google::protobuf::Closure* done
                          ) = 0;
  virtual boost::asio::awaitable<error_with_message> Start() = 0;                        
};

std::shared_ptr<RpcChannel> create_rpc_channel(std::string, boost::asio::io_context&);

} // namespace rpc

#endif
