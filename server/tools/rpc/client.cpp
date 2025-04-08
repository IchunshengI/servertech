#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "rpc_demo.pb.h"

int main() {
    // 创建Socket
    {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(8080);
        inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    // 连接服务器

    // 构建请求：通过 client_name 指定要调用的方法
    
        connect(sock, (sockaddr*)&serv_addr, sizeof(serv_addr));
        RPCRequest request;
        request.set_client_name("CallGetAnotherSerialNumber");  // 指定调用第二个方法

        // 序列化请求
        std::string request_str;
        request.SerializeToString(&request_str);

        // 发送请求
        write(sock, request_str.c_str(), request_str.size());

        // 接收响应
        char buffer[1024] = {0};
        read(sock, buffer, sizeof(buffer));

        // 反序列化响应
        RPCResponse response;
        response.ParseFromString(std::string(buffer));

        std::cout << "Server response: " << response.serial_number() << std::endl;
         close(sock);
    }

    {
        int sock2 = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in serv_addr2;
        serv_addr2.sin_family = AF_INET;
        serv_addr2.sin_port = htons(8080);
        inet_pton(AF_INET, "127.0.0.1", &serv_addr2.sin_addr);

    // 连接服务器

    // 构建请求：通过 client_name 指定要调用的方法
    
        connect(sock2, (sockaddr*)&serv_addr2, sizeof(serv_addr2));
        RPCRequest request2;
        request2.set_client_name("CallGetSerialNumber");  // 指定调用第二个方法

        // 序列化请求
        std::string request_str2;
        request2.SerializeToString(&request_str2);

        // 发送请求
        write(sock2, request_str2.c_str(), request_str2.size());

        // 接收响应
        char buffer2[1024] = {0};
        read(sock2, buffer2, sizeof(buffer2));

        // 反序列化响应
        RPCResponse response2;
        response2.ParseFromString(std::string(buffer2));

        std::cout << "Server response: " << response2.serial_number() << std::endl;
         close(sock2);
    }
    return 0;
}