


#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include "ai_server.pb.h"
#include <memory>
#include "config/config.h"
#include "log/logger_wrapper.h"
#include "rpc_channel.h"
#include "rpc_controller.h"
#include "rpc_closure.h"
#include <boost/asio/experimental/channel.hpp>
#include <boost/system/detail/error_code.hpp>

using channel_t = boost::asio::experimental::channel<void(boost::system::error_code)>;
int main(){

  boost::asio::io_context ex;
  chat::InitLogger(ex);
  chat::Config::Instance().LoadConfigFile("/home/tlx/project/servertech-chat/tools/rpc/test/zoo.cfg");

  auto request = std::make_shared<InitRequest>();
  auto response = std::make_shared<GeneralResponse>();
  auto controller = std::make_shared<rpc::RpcController>();
  request->set_user_id(1);
  request->set_session_id(1);
  request->set_api_key("sk-13791da59b264b5eb778cae83765878c");

  // 创建strand保证顺序执行
  auto strand = boost::asio::make_strand(ex);
  auto channel = rpc::create_rpc_channel("AiServer", ex);
  auto rpc_done_channel = std::make_shared<channel_t>(strand, 1);
  boost::asio::co_spawn(ex,
    [&]() -> boost::asio::awaitable<void> {
        
        auto error = co_await channel->Start();
        if(error.ec){
          std::cout << "错误! " << error.msg << std::endl;
          co_return;
        }
        
        // 使用protobuf自动生成的存根类
        AiServer_Stub stub(channel.get());

        // 创建闭包
        auto* closure = rpc::RpcClosure::Create(
            [&](){               
              boost::asio::co_spawn(ex,
                  [=]() -> boost::asio::awaitable<void> {
                      std::cout << "\n=== RPC Callback ===" << std::endl;
                      if (controller->Failed()) {
                          std::cerr << "RPC Failed: " 
                                  << controller->ErrorText() << std::endl;
                      } else {
                          std::cout << "Serial Number: " 
                                  << response->respon_message() << std::endl;
                      }
                      co_await rpc_done_channel->async_send(boost::system::error_code{}, boost::asio::use_awaitable);
                      co_return;
                  },
                  boost::asio::detached
              );                          
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


  auto query_request = std::make_shared<QueryRequest>();
  auto query_response = std::make_shared<GeneralResponse>();

  query_request->set_query_message("rpc是什么? 计算机领域");
  boost::asio::co_spawn(ex,[&]() -> boost::asio::awaitable<void> 
  {
    // 等待前一个协程完成
      // 等待通道信号（异步阻塞）
    //boost::system::error_code ec;
    std::cout << " IN two 222 " << std::endl;
    co_await rpc_done_channel->async_receive(boost::asio::use_awaitable);                    
    // 这里可以添加RPC调用逻辑
    std::cout << "Coroutine 2 executed" << std::endl;


    AiServer_Stub stub(channel.get());
    auto* closure = rpc::RpcClosure::Create(
      [=](){      
        boost::system::error_code ec;
        std::cout << "\n=== RPC Callback ===" << std::endl;
        if (controller->Failed()) {
            std::cerr << "RPC Failed: " << controller->ErrorText() << std::endl;
        } else {
            std::cout << "接收数据为: " << query_response->respon_message() << std::endl;
        }
      });

    // 发起异步调用
    stub.Query(controller.get(),query_request.get(),
        query_response.get(),closure);    
    co_return;
    },
    boost::asio::detached);

  // 运行事件循环
  std::cout << "Starting IO context..." << std::endl;
  try {
      ex.run();
      std::cout << "IO context exited normally" << std::endl;
  } catch (const std::exception& e) {
      std::cerr << "IO context error: " << e.what() << std::endl;
  }

  return 0;
}