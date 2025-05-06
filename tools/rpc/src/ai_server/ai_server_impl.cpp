#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <memory>
#include "ai_server.pb.h"
#include "ai_server_impl.h"
#include "log/logger_wrapper.h"
#include "method_process.h"
#include "redis_client.h"
namespace chat{
AiServerImpl::AiServerImpl(boost::asio::io_context& iox) : iox_(iox)
{
  redis_client_ = CreateRedisClient(iox);
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
  boost::asio::co_spawn(iox_,[=,this]()-> boost::asio::awaitable<void> {
    
    SessionInfo info;
    info.user_id_ = request->user_id();
    info.session_id_ = request->session_id();
    info.api_key_ = request->api_key();
#ifdef DEBUG_INFO
    LOG("Debug") << "session_id is : " << info.session_id_;
    LOG("Debug") << "id is : " << info.user_id_;
    LOG("Debug") << "api key is : " << info.api_key_;
#endif
    //awaitable<result_with_message<std::string>> SessionInit(SessionInfo);
    auto result = co_await this->redis_client_->SessionInit(info);
    if (result.has_error())
    {
      LOG("Error") << "sessionInit error" << result.error().ec.message();
      response->set_respon_message("set init info error !");
    }else{
      std::string respn = result.value();
      std::cout << "redis键为: " << respn << std::endl;
      response->set_respon_message(respn);
    }
    delete request;
    delete controller;
    done->Run();
    co_return;
    },
    boost::asio::detached
  );
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
    [=,this]() -> boost::asio::awaitable<void> {

      std::string query = request->query_message();
      MethodProcess methodProcess(this->iox_);
      methodProcess.Init("dashscope.aliyuncs.com");
      std::string token = request->token();
      auto result = co_await redis_client_->GetApiKey(token);

      /* 处理apikey的获取结果*/
      if (result.has_error())
      {
        if (result.error().ec == rpc::errc::not_found) /* 有错误直接回发 */
        {
          response->set_respon_message("can not found api key"); 
        }else{
          response->set_respon_message("server error, please try again");
        }
      } else{
        std::string api_key = result.value();
        /* 云端服务调用 */
        auto call_result = co_await methodProcess.CallModelByHttps(query, api_key);
        if (call_result.has_error()) {
          LOG("Error") << "CallModelByhttps error : " << call_result.error().message();
          response->set_respon_message(call_result.error().message());
        } else {
          response->set_respon_message(call_result.value());
        }        
      }

      /* 这里做入库操作 */
      delete request;
      delete controller;
      done->Run();

      co_return;
    },
    boost::asio::detached
    );
} 

std::unique_ptr<RedisClient> CreateRedisClient(boost::asio::io_context& iox)
{
  return std::make_unique<RedisClient>(iox);
}
} // namespace chat
  