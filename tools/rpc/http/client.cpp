


#include <boost/asio/io_context.hpp>
#include "ai_server.pb.h"
#include <memory>
#include "config/config.h"
#include "log/logger_wrapper.h"
#include "rpc_channel.h"
#include "rpc_controller.h"
#include "rpc_closure.h"
int main(){

  boost::asio::io_context iox;
  chat::InitLogger(iox);
  chat::Config::Instance().LoadConfigFile("/home/tlx/project/servertech-chat/tools/rpc/test/zoo.cfg");

  auto request = std::make_shared<InitRequest>();
  auto response = std::make_shared<GeneralResponse>();
  auto controller = std::make_shared<rpc::RpcController>();
  request->set_user_id(1);
  request->set_session_id(1);
  request->set_api_key("hello ai rpc");

  auto channel = rpc::create_rpc_channel("AiServer", iox);

      boost::asio::co_spawn(iox,
        [channel , // 使用移动捕获 (C++14)
         request, 
         response, 
         controller]() -> boost::asio::awaitable<void> {
            
            auto error = co_await channel->Start();
            if(error.ec){
              std::cout << "错误! " << error.msg << std::endl;
              co_return;
            }
            
            // 使用protobuf自动生成的存根类
            AiServer_Stub stub(channel.get());

            // 创建闭包
            auto* closure = rpc::RpcClosure::Create(
                [controller, response]() {
                    std::cout << "\n=== RPC Callback ===" << std::endl;
                    if (controller->Failed()) {
                        std::cerr << "RPC Failed: " 
                                << controller->ErrorText() << std::endl;
                    } else {
                        std::cout << "Serial Number: " 
                                << response->serial_number() << std::endl;
                    }
                });

            // 发起异步调用
            stub.SetInitInfo(
                controller.get(),
                request.get(),
                response.get(),
                closure);

            co_return;
        },
        boost::asio::detached
    );

    // 运行事件循环
    std::cout << "Starting IO context..." << std::endl;
    try {
        iox.run();
        std::cout << "IO context exited normally" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "IO context error: " << e.what() << std::endl;
    }

    return 0;
  return 0;
}