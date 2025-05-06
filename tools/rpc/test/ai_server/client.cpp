


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
#include <boost/asio/this_coro.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include "rpc_client.h"
#include "signal.hpp"

using chat::SimpleSignal;


// boost::asio::awaitable<void> query_rpc(
//     std::shared_ptr<rpc::RpcChannel> channel,
//     std::shared_ptr<rpc::RpcController> controller,
//     std::shared_ptr<SimpleSignal> signal,
//     const std::string& token)
// {
//   // co_await signal->Wait();

//     std::cout << " IN two 222 " << std::endl;

//     auto query_request = std::make_shared<QueryRequest>();
//     auto query_response = std::make_shared<GeneralResponse>();

//     query_request->set_query_message("rpc是什么? 计算机领域");
//     query_request->set_token(token);

//     AiServer_Stub stub(channel.get());

//     auto* closure = rpc::RpcClosure::Create(
//       [=]() {
//         std::cout << "\n=== RPC Callback ===" << std::endl;
//         if (controller->Failed()) {
//             std::cerr << "RPC Failed: " << controller->ErrorText() << std::endl;
//         } else {
//             std::cout << "接收数据为: " << query_response->respon_message() << std::endl;
//             signal->Signal();      
//         }
//       });

//     stub.Query(controller.get(), query_request.get(), query_response.get(), closure);
//     co_await signal->Wait();
//     std::cout << "结束 " << std::endl;
//     co_return;
// }


using channel_t = boost::asio::experimental::channel<void(boost::system::error_code)>;
// int main(){

//   boost::asio::io_context iox;
//   chat::InitLogger(iox);
//   chat::Config::Instance().LoadConfigFile("/home/tlx/project/servertech-chat/tools/rpc/test/doc/zoo.cfg");

//   auto request = std::make_shared<InitRequest>();
//   auto response = std::make_shared<GeneralResponse>();
//   auto controller = std::make_shared<rpc::RpcController>();
//   request->set_user_id(1);
//   request->set_session_id(1);
//   request->set_api_key("sk-91d3a22bf7824e4dbc69a8383cd5cebb");
//   std::string token;
//   // 创建strand保证顺序执行
//  // auto strand = boost::asio::make_strand(iox);
//   auto channel = rpc::create_rpc_channel("AiServer", iox);
//   //auto rpc_done_channel = std::make_shared<channel_t>(strand, 1);

//   boost::asio::co_spawn(iox,
//     [&]() -> boost::asio::awaitable<void> {
        
//         auto error = co_await channel->Start();
//         if(error.ec){
//           std::cout << "错误! " << error.msg << std::endl;
//           co_return;
//         }
        
//         // 使用protobuf自动生成的存根类
//         AiServer_Stub stub(channel.get());
//         auto signal = std::make_shared<SimpleSignal>(iox.get_executor());
//         // 创建闭包
//         auto* closure = rpc::RpcClosure::Create(
//             [&](){               

//               if (controller->Failed()) {
//                   std::cerr << "RPC Failed: " 
//                           << controller->ErrorText() << std::endl;
//               } else {
//                   std::cout << "Respon message is : " 
//                           << response->respon_message() << std::endl;
//               }
//               token = response->respon_message();
//               signal->Signal();      
//               std::cout << "应该在前面" << std::endl;
//             });

//         // 发起异步调用
//         stub.SetInitInfo(
//             controller.get(),
//             request.get(),
//             response.get(),
//             closure);
//         co_await signal->Wait();
//         std::cout << "应该在后面" << std::endl;
//         signal->Reset();
//         co_await query_rpc(channel,controller,signal,token);
//         co_return;
//     },
//     boost::asio::detached
//    );





//   // 运行事件循环
//   std::cout << "Starting IO context..." << std::endl;
//   try {
//       iox.run();
//       std::cout << "IO context exited normally" << std::endl;
//   } catch (const std::exception& e) {
//       std::cerr << "IO context error: " << e.what() << std::endl;
//   }

//   return 0;
// }



int main(){

  boost::asio::io_context iox;
  chat::RpcClient rpc_client(iox.get_executor());
  chat::InitLogger(iox.get_executor());
  chat::Config::Instance().LoadConfigFile("/home/tlx/project/servertech-chat/tools/rpc/test/doc/zoo.cfg");

  boost::asio::co_spawn(iox,
    [&]() -> boost::asio::awaitable<void> {
        
       co_await rpc_client.SetInitInfo(1,1,"sk-91d3a22bf7824e4dbc69a8383cd5cebb");
       auto result = co_await rpc_client.Query("rpc是什么？");
       if (result.has_error()){
          std::cerr << "错误 !" << std::endl;
       }else
          std::cerr << "返回结果为 ： " << result.value() << std::endl;
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
}