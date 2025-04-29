#include <boost/asio/io_context.hpp>
#include "config/config.h"
#include "ai_server_impl.h"
#include "log/logger_wrapper.h"
#include "rpc_server.h"

using chat::AiServerImpl;
using chat::LOG;

int main(){

  boost::asio::io_context iox;
  auto rpc_server_ptr = rpc::create_rpc_server(iox);
  chat::Config::Instance().LoadConfigFile("/home/tlx/project/servertech-chat/tools/rpc/test/doc/zoo.cfg");
  AiServerImpl* ai_server_ptr = new AiServerImpl(iox);
  if(!rpc_server_ptr->RegisterService(ai_server_ptr)){
    LOG("Error") << "rpc registerServer error";
    return -1;
  }
  std::string server_addr("0.0.0.0:8000");

  boost::asio::co_spawn(
    iox,
    [&]() -> boost::asio::awaitable<void> {
        auto error = co_await rpc_server_ptr->Start(server_addr);
        if (error.ec) {
            std::cerr << "Error starting server: " << error.ec.message() << std::endl;
        }
        co_return;
    },
    boost::asio::detached
  );

  iox.run();
  return 0;
}