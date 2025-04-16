// client_main.cpp 修复版本
#include <iostream>
#include "rpc_demo.pb.h"       // 包含自动生成的存根
#include "rpc_controller.h"
#include "rpc_channel.h"
#include "rpc_closure.h"
#include <boost/asio.hpp>
#include <memory>
#include "log/logger_wrapper.h"
void print_usage() {
    std::cout << "Usage: echo_client <ip> <port>\n"
              << "Example: 127.0.0.1 12321" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        print_usage();
        return -1;
    }

    // 初始化ASIO
    boost::asio::io_context iox;
    /* 初始化日志 */
    chat::InitLogger(iox);
    // 使用智能指针管理核心对象
    auto request = std::make_shared<RPCRequest>();
    auto response = std::make_shared<RPCResponse>();
    auto controller = std::make_shared<rpc::RpcController>();
    request->set_client_name("Client for tonull");

    // 创建RPC通道（改为shared_ptr）
    const std::string addr = std::string(argv[1]) + ":" + argv[2];
    auto channel = rpc::create_rpc_channel(addr, iox); 

    // 启动异步调用协程（修复捕获方式）
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
            DemoService_Stub stub(channel.get());

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
            stub.GetSerialNumber(
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
}