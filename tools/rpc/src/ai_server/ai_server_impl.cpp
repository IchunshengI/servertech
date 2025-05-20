#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <memory>
#include "ai_server.pb.h"
#include "ai_server_impl.h"
#include "log/logger_wrapper.h"
// #include "method_process.h"
#include "mongodb/mongodb_client.h"
#include "redis_client.h"
#include "thread_poll/thread_poll.hpp"
namespace chat{
AiServerImpl::AiServerImpl(boost::asio::any_io_executor ex) : ex_(ex), method_process_(ex), thraed_pool_(4, 8)
{
  redis_client_ = CreateRedisClient(ex);
  method_process_.Init("dashscope.aliyuncs.com");
  std::string uri = "mongodb://tlx:hpcl6tlx@localhost:27017/?authSource=chat";
  monogodb_client_ptr_ = std::make_shared<MonogodbClient>(uri, "chat");
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
  boost::asio::co_spawn(ex_,[=,this]()-> boost::asio::awaitable<void> {
    
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
    ex_,
    [=,this]() -> boost::asio::awaitable<void> {

      auto query = std::make_shared<std::string>(request->query_message());
      std::shared_ptr<std::string> respon_data; 
      std::string token = request->token();
      auto result = co_await redis_client_->GetSessionInfo(token);

      
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
        SessionInfo session_info = result.value();
        /* 云端服务调用 */
        auto call_result = co_await method_process_.CallModelByHttps(*query, session_info.api_key_);
        if (call_result.has_error()) {
          LOG("Error") << "CallModelByhttps error : " << call_result.error().message();
          response->set_respon_message(call_result.error().message());
        } else {
          response->set_respon_message(call_result.value());
          // 移动 string 到 shared_ptr 中
          respon_data = std::make_shared<std::string>(std::move(call_result.value()));
        }        

        
        /* 这里做入库操作 */
        //monogodb_client_ptr_->insertMessage();
         monogodb_client_ptr_->insertMessage(session_info.user_id_,
                                             session_info.session_id_,
                                             "user",
                                             *query);
         monogodb_client_ptr_->insertMessage(session_info.user_id_,
                                             session_info.session_id_,
                                             "root",
                                             *respon_data);                                             
                                             
      }

      delete request;
      delete controller;
      done->Run();

      co_return;
    },
    boost::asio::detached
    );
} 

std::unique_ptr<RedisClient> CreateRedisClient(boost::asio::any_io_executor ex)
{
  return std::make_unique<RedisClient>(ex);
}
} // namespace chat
  