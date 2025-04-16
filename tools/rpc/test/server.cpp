#include <iostream>
#include "rpc_server.h"
#include "rpc_demo.pb.h"
#include <google/protobuf/message.h>
#include "google/protobuf/service.h"
#include "boost/asio.hpp"
#include "boost/asio/co_spawn.hpp"
#include "log/logger_wrapper.h"
// #include <iostream>
// #include "result_with_message.h"
class DemoServiceImpl : public DemoService{
 public:
  DemoServiceImpl() {}
  virtual ~DemoServiceImpl() {}

    // 原有方法：返回 SKT-L6505
  void GetSerialNumber(::google::protobuf::RpcController* controller,
                      const RPCRequest* request,
                      RPCResponse* response,
                      ::google::protobuf::Closure* done) override {
      response->set_serial_number("SKT-L6505");
      chat::LOG("Debug") << "serial_size" << response->ByteSizeLong();
      done->Run();
  }

  // 新增方法：返回 SZU
  void GetAnotherSerialNumber(::google::protobuf::RpcController* controller,
                             const RPCRequest* request,
                             RPCResponse* response,
                             ::google::protobuf::Closure* done) override {
      response->set_serial_number("SZU");
      done->Run();
  }
};

int main()
{
  boost::asio::io_context iox;
  auto rpc_server_ptr = rpc::create_rpc_server(iox);

  DemoService* demoService = new DemoServiceImpl();
  if (!rpc_server_ptr->RegisterService(demoService)){
    std::cout << "register service failed" << std::endl;
    return -1;
  }
  std::string server_addr("0.0.0.0:8000");
    // 启动协程执行 Start 函数
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

  // 运行 io_context
  iox.run();
  std::cout << "？？？？" << std::endl;
  return 0;
}

