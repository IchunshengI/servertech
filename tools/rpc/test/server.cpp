#include <iostream>
#include "config/config.h"
#include "rpc_server.h"
#include "rpc_demo.pb.h"
#include <google/protobuf/message.h>
#include <pthread.h>
#include <thread>
#include "google/protobuf/service.h"
#include "boost/asio.hpp"
#include "boost/asio/co_spawn.hpp"
#include "log/logger_wrapper.h"
#include "config/config.h"
#include <thread>
#include <memory>

// #include <iostream>
// #include "result_with_message.h"
class DemoServiceImpl : public DemoService, public std::enable_shared_from_this<DemoServiceImpl>{
 public:
  DemoServiceImpl(boost::asio::io_context& iox) : iox_(iox){}
  virtual ~DemoServiceImpl() {}

  // 原有方法：返回 SKT-L6505
  void GetSerialNumber(::google::protobuf::RpcController* controller,
                      const RPCRequest* request,
                      RPCResponse* response,
                      ::google::protobuf::Closure* done) override {
      
      /* 
        起一个协程来兼容业务方面的操作
      */
      
      std::cerr << "调用了" << std::endl;
      auto self = shared_from_this();
      boost::asio::co_spawn(
        iox_,
        [done,response,request,controller,self]() -> boost::asio::awaitable<void> {

            /* 业务处理部分 ***************************************************************** */
            std::string data = "SKT-L6505-TLX";
            response->set_serial_number(data);
            chat::LOG("Debug") << "serial_size" << response->ByteSizeLong();
           boost::asio::steady_timer timer(self->iox_);
            std::cerr <<"执行线程为1：" << pthread_self()<<std::endl;
            timer.expires_after(std::chrono::milliseconds(100));
            co_await timer.async_wait(boost::asio::use_awaitable);
             std::cerr <<"执行线程为1：" << pthread_self()<<std::endl;
            /* 业务处理部分 ***************************************************************** */
            /* 清理内存 ***************************************************************** */
            delete request;
            delete controller;

            /* 执行回调 */
            std::cerr << "开始执行回调" << std::endl;
            done->Run();
            co_return;
        },
        boost::asio::detached
    );
     std::cerr <<"执行线程为：" << pthread_self()<<std::endl;
    std::cerr << "结束了" << std::endl;

  }

  // 新增方法：返回 SZU
  void GetAnotherSerialNumber(::google::protobuf::RpcController* controller,
                             const RPCRequest* request,
                             RPCResponse* response,
                             ::google::protobuf::Closure* done) override {
      response->set_serial_number("SZU");
      done->Run();
  }
 private:
  boost::asio::io_context& iox_;
};

int main()
{
  boost::asio::io_context iox;
  chat::Config::Instance().LoadConfigFile("/home/tlx/project/servertech-chat/tools/rpc/test/zoo.cfg");
  auto rpc_server_ptr = rpc::create_rpc_server(iox);

  //DemoService* demoService = new DemoServiceImpl(iox);
  auto demoService = std::make_shared<DemoServiceImpl>(iox); 
  if (!rpc_server_ptr->RegisterService(demoService.get())){
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
  std::thread thread2([&]() {iox.run();});
  std::thread thread3([&]() {iox.run();});
  std::thread thread4([&]() {iox.run();});

  iox.run();
  std::cout << "？？？？" << std::endl;
  return 0;
}

