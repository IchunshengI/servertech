/*
  Date	  :	2025年4月23日
  Author	:	chunsheng
  Brief		:	rpc服务实现类
*/
#pragma once
#include <boost/asio/io_context.hpp>
#include "ai_server.pb.h"
#include <memory>
#include "boost/asio.hpp"
#include "boost/asio/cancel_at.hpp"
namespace chat{

class RedisClient;
class AiServerImpl : public AiServer,
                     public std::enable_shared_from_this<AiServerImpl>
{
 public:
  AiServerImpl(boost::asio::any_io_executor ex);
  ~AiServerImpl();

  // 设置初始化消息
  void SetInitInfo(::google::protobuf::RpcController* controller,
                      const InitRequest* request,
                      GeneralResponse* response,
                      ::google::protobuf::Closure* done) override;
  // 远端查询
  void Query(::google::protobuf::RpcController* controller,
                      const QueryRequest* request,
                      GeneralResponse* response,
                      ::google::protobuf::Closure* done) override;



 private:
  boost::asio::any_io_executor ex_;
  std::shared_ptr<RedisClient> redis_client_;
  
}; // namespace chat

}

 