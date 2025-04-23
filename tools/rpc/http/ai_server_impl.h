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

class AiServerImpl : public AiServer,
                     public std::enable_shared_from_this<AiServerImpl>
{
 public:
  AiServerImpl(boost::asio::io_context& iox);
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
  struct SessionInfo{
    int user_id_;
    int session_id_;
    std::string api_key_;
  };
  boost::asio::io_context& iox_;
  SessionInfo session_info_;
  
}; // namespace chat

}

