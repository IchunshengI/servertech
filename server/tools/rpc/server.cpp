#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "rpc_demo.pb.h"
#include <google/protobuf/message.h>

class DemoServiceImpl : public DemoService {
public:
    // 原有方法：返回 SKT-L6505
    void GetSerialNumber(::google::protobuf::RpcController* controller,
                        const RPCRequest* request,
                        RPCResponse* response,
                        ::google::protobuf::Closure* done) override {
        response->set_serial_number("SKT-L6505");
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

int main() {
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    // 创建Socket（代码同之前示例）
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    bind(server_fd, (sockaddr*)&address, sizeof(address));
    listen(server_fd, 5);

    std::cout << "Server listening on port 8080..." << std::endl;

    while (true) {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
        
        // 读取请求
        char buffer[1024];
        ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer));

        // 反序列化请求（需客户端指定调用的方法）
        RPCRequest request;
        request.ParseFromArray(buffer, bytes_read);

        // 处理请求（根据请求类型分发到不同方法）
        DemoServiceImpl service;
        RPCResponse response;

        // -------------------------------
        // 关键：根据客户端请求选择调用哪个方法
        // （需客户端在请求中携带方法标识符）
        // -------------------------------
        if (request.client_name() == "CallGetSerialNumber") {
            service.GetSerialNumber(
                nullptr,
                &request,
                &response,
                google::protobuf::NewPermanentCallback([] {})
            );
        } else if (request.client_name() == "CallGetAnotherSerialNumber") {
            service.GetAnotherSerialNumber(
                nullptr,
                &request,
                &response,
                google::protobuf::NewPermanentCallback([] {})
            );
        } else {
            response.set_serial_number("Unknown Method");
        }

        // 序列化响应
        std::string response_str;
        response.SerializeToString(&response_str);
        write(client_fd, response_str.c_str(), response_str.size());
        close(client_fd);
    }

    google::protobuf::ShutdownProtobufLibrary();
    close(server_fd);
    return 0;
}