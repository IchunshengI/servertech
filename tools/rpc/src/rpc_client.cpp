#include <cstdint>
#include <memory>
#include "log/logger_wrapper.h"
#include "result_with_message.h"
#include "rpc_client.h"
#include "ai_server/ai_server.pb.h"
#include "rpc_closure.h"
#include "signal.hpp"
namespace chat{

RpcClient::RpcClient(boost::asio::io_context& iox) : iox_(iox)
{
  channel_ = rpc::create_rpc_channel("AiServer", iox_);
  controller_ = std::make_shared<RpcController>();
  ai_server_stub_ = std::make_shared<AiServer_Stub>(channel_.get());
}

RpcClient::~RpcClient()
{

}

awaitable<bool> RpcClient::SetInitInfo(uint32_t user_id, uint32_t session_id, std::string api_key)
{
  bool flag = true;
  auto request = std::make_shared<InitRequest>();
  auto response = std::make_shared<GeneralResponse>();
  request->set_user_id(user_id);
  request->set_session_id(session_id);
  request->set_api_key(api_key);

  auto error = co_await channel_->Start();
  if(error.ec){
    std::cout << "错误! " << error.msg << std::endl;
    co_return false;
  }
  
  auto signal = std::make_shared<SimpleSignal>(iox_.get_executor());

  auto* closure = rpc::RpcClosure::Create(
    [&](){               
      if (controller_->Failed()) {
        LOG("Error") << "RPC Failed: " << controller_->ErrorText() << std::endl;
        flag = false;
      } 

      token_ = response->respon_message();
      signal->Signal();      
    });
  // 使用protobuf自动生成的存根类
  AiServer_Stub stub(channel_.get());
  ai_server_stub_->SetInitInfo(
    controller_.get(),
    request.get(),
    response.get(),
    closure);
  co_await signal->Wait();
  if (!flag) co_return false;
  co_return true;
}



awaitable<result_with_message<std::string>> RpcClient::Query(std::string query)
{
  bool flag = true;
  std::string result;
  auto query_request = std::make_shared<QueryRequest>();
  auto query_response = std::make_shared<GeneralResponse>();

  query_request->set_query_message(std::move(query));
  query_request->set_token(token_);

  AiServer_Stub stub(channel_.get());

  auto signal = std::make_shared<SimpleSignal>(iox_.get_executor());
  auto* closure = rpc::RpcClosure::Create(
    [&]() {
      if (controller_->Failed()) {
          LOG("Error") << "RPC Query Failed: " << controller_->ErrorText() << std::endl;
          result = controller_->ErrorText();
          flag = false;
      } else {
          // std::cout << "接收数据为: " << query_response->respon_message() << std::endl;
          result = query_response->respon_message();
      }
      signal->Signal();
    });
  ai_server_stub_->Query(controller_.get(), query_request.get(), query_response.get(), closure);
  co_await signal->Wait();
  if (!flag) co_return rpc::error_with_message{rpc::errc::rpc_error, std::move(result)};
  co_return result;
}

} // namespace chat

