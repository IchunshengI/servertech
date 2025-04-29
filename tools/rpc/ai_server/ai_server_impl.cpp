#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include "ai_server.pb.h"
#include "ai_server_impl.h"
#include "log/logger_wrapper.h"
#include "method_process.h"
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

  /* 这里其实也一样，要搞一个连接池，然后根据user_id和session_id生成一个token回发过去*/
  session_info_.user_id_ = request->user_id();
  session_info_.session_id_ = request->session_id();
  session_info_.api_key_ = request->api_key();
#ifdef DEBUG_INFO
  LOG("Debug") << "session_id is : " << session_info_.session_id_;
  LOG("Debug") << "id is : " << session_info_.user_id_;
  LOG("Debug") << "api key is : " << session_info_.api_key_;
#endif
  response->set_respon_message("set init info success!");
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
    [=,this]() -> boost::asio::awaitable<void> {

      /* 从redis连接池中取数据 */
      /* 获取连接池中的api_key 和 user_id、session_id等信息 */

      std::string query = request->query_message();
      MethodProcess methodProcess(this->iox_);
      methodProcess.Init("dashscope.aliyuncs.com");
      /* 这里之后要co_await 远端服务，然后写回 */
      auto result = co_await methodProcess.CallModelByHttps(query, session_info_.api_key_);
      if (result.has_error()) {
          std::cerr << "Error starting server: " << result.error().ec.message() << std::endl;
      }
      response->set_respon_message(result.value());
      delete request;
      delete controller;
      done->Run();

      std::cout << "二次打印数据为: \n" << result.value() << std::endl;
      
      /* 数据处理部分，入库等操作 */

      co_return;
    },
    boost::asio::detached
    );
}            
} // namespace chat
  