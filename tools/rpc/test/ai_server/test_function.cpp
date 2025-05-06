#include <boost/asio/io_context.hpp>
#include "config/config.h"
#include "ai_server_impl.h"
#include "log/logger_wrapper.h"
#include "rpc_server.h"
#include "method_process.h"
using chat::AiServerImpl;
using chat::LOG;
using chat::MethodProcess;
int main(){

  boost::asio::io_context iox;
  MethodProcess methodProcess(iox.get_executor());
  methodProcess.Init("dashscope.aliyuncs.com");
  std::string api_key = "sk-13791da59b264b5eb778cae83765878c";
  std::string query = "rpc是什么？";

  boost::asio::co_spawn(
    iox,
    [&]() -> boost::asio::awaitable<void> {
        auto result = co_await methodProcess.CallModelByHttps(query, api_key);
        if (result.has_error()) {
            std::cerr << "Error starting server: " << result.error().ec.message() << std::endl;
        }
        co_return;
    },
    boost::asio::detached
  );

  iox.run();
  return 0;
}