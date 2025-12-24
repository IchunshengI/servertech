#include <cstdint>
#include <memory>
#include "log/logger_wrapper.h"
#include "result_with_message.h"
#include "rpc_client.h"
#include "ai_server/ai_server.pb.h"
#include "rpc_closure.h"
#include "signal.hpp"
namespace rpc {

RpcClient::RpcClient(boost::asio::any_io_executor ex) : RpcClient(std::move(ex), {})
{
}

RpcClient::RpcClient(boost::asio::any_io_executor ex, std::string token) : ex_(ex), token_(std::move(token))
{
  channel_ = rpc::create_rpc_channel("AiServer", ex_);
  controller_ = std::make_shared<RpcController>();
  ai_server_stub_ = std::make_shared<AiServer_Stub>(channel_.get());
}

RpcClient::RpcClient(boost::asio::any_io_executor ex, std::string token, std::string hash_key)
    : ex_(ex), token_(std::move(token)), hash_key_(std::move(hash_key))
{
  channel_ = rpc::create_rpc_channel("AiServer", ex_);
  channel_->SetHashKey(hash_key_);
  controller_ = std::make_shared<RpcController>();
  ai_server_stub_ = std::make_shared<AiServer_Stub>(channel_.get());
}

RpcClient::~RpcClient()
{

}

awaitable<result_with_message<std::string>> RpcClient::Query(std::string query)
{
  bool flag = true;
  std::string result;
  auto query_request = std::make_shared<QueryRequest>();
  auto query_response = std::make_shared<GeneralResponse>();

  query_request->set_query_message(std::move(query));
  query_request->set_token(token_);

  //AiServer_Stub stub(channel_.get());

  auto signal = std::make_shared<chat::SimpleSignal>(ex_);
  if (!started_)
  {
    if (!hash_key_.empty())
      channel_->SetHashKey(hash_key_);
    auto error = co_await channel_->Start();
    if(error.ec){
      co_return rpc::error_with_message{rpc::errc::rpc_error, error.msg};
    }
    started_ = true;
  }
  auto* closure = rpc::RpcClosure::Create(
    [&]() {
      if (controller_->Failed()) {
          chat::LOG("Error") << "RPC Query Failed: " << controller_->ErrorText() << std::endl;
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

} // namespace rpc 
