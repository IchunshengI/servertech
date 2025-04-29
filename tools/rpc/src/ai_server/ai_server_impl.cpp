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

      /* 从redis连接池中取数据 */
      /* 获取连接池中的api_key 和 user_id、session_id等信息 */

      std::string query = request->query_message();
      MethodProcess methodProcess(this->iox_);
      methodProcess.Init("dashscope.aliyuncs.com");

      /* 取api_key */
      /* 这里之后要co_await 远端服务，然后写回 */
      // auto result = co_await methodProcess.CallModelByHttps(query, session_info_.api_key_);
      // if (result.has_error()) {
      //     std::cerr << "Error starting server: " << result.error().ec.message() << std::endl;
      // }
      // response->set_respon_message(result.value());
      //response->set_respon_message("hello");
      delete request;
      delete controller;
      done->Run();

      //std::cout << "二次打印数据为: \n" << result.value() << std::endl;
      
      /* 数据处理部分，入库等操作 */

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
  