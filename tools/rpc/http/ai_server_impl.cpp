#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include "ai_server.pb.h"
#include "ai_server_impl.h"
#include "log/logger_wrapper.h"

namespace chat{
AiServerImpl::AiServerImpl(boost::asio::io_context& iox) : iox_(iox)
{

}
AiServerImpl::~AiServerImpl(){

}

// 设置初始化消息
void AiServerImpl::SetInitInfo(::google::protobuf::RpcController* controller,
                const InitRequest* request,
                GeneralResponse* response,
                ::google::protobuf::Closure* done
                )
{
  session_info_.user_id_ = request->user_id();
  session_info_.session_id_ = request->session_id();
  session_info_.api_key_ = request->api_key();
#ifdef DEBUG_LOG
  LOG("Debug") << "session_id is : " << session_info_.session_id_;
  LOG("Debug") << "id is : " << session_info_.user_id_;
  LOG("Debug") << "api key is : " << session_info_.api_key_;
#endif
  response->set_serial_number("set init info success!");
  delete request;
  delete controller;
  done->Run();
}
// 远端查询
void AiServerImpl::Query(::google::protobuf::RpcController* controller,
            const QueryRequest* request,
            GeneralResponse* response,
            ::google::protobuf::Closure* done
            )
{
  boost::asio::co_spawn(
    iox_,
    [&]() -> boost::asio::awaitable<void> {
      co_return;
    },
    boost::asio::detached
    );
}            
} // namespace chat
  