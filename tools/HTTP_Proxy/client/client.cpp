#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

const char* SERVER_IP = "127.0.0.1";
const int SERVER_PORT = 8000;
const int BUFFER_SIZE = 8192;

// 新增函数：可靠地接收指定字节数
bool receive_exact(int sockfd, char* buffer, size_t length) {
    size_t received = 0;
    while (received < length) {
        ssize_t r = recv(sockfd, buffer + received, length - received, 0);
        if (r <= 0) {
            return false; // 接收失败或连接关闭
        }
        received += r;
    }
    return true;
}

int main() {
    int client_socket;
    struct sockaddr_in server_addr;

    // 创建套接字
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        return 1;
    }

    // 配置服务器地址
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(client_socket);
        return 1;
    }

    // 连接服务器
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection failed");
        close(client_socket);
        return 1;
    }

    std::cout << "Connected to server at " << SERVER_IP << ":" << SERVER_PORT << std::endl;

    // 第一次发送
    const char* message = "API-KEY:sk-13791da59b264b5eb778cae83765878c";
    if (send(client_socket, message, strlen(message), 0) == -1) {
        perror("Send failed");
        close(client_socket);
        return 1;
    }

    // 第一次接收
    {
        // 1. 先接收4字节长度头
        uint32_t message_len;
        if (!receive_exact(client_socket, (char*)(&message_len), sizeof(message_len))) {
            perror("Failed to receive length header");
            close(client_socket);
            return 1;
        }
        message_len = ntohl(message_len);
        std::cout << "Expecting message of length: " << message_len << std::endl;

        // 2. 接收实际数据
        char* buffer = new char[message_len + 1]; // +1 for null terminator
        if (!receive_exact(client_socket, buffer, message_len)) {
            perror("Failed to receive message body");
            delete[] buffer;
            close(client_socket);
            return 1;
        }
        buffer[message_len] = '\0';
        std::cout << "Received from server: " << buffer << std::endl;
        delete[] buffer;
    }

    sleep(2);

    // 第二次发送
    const char* message2 = "rpc一般是什么东西？详细一点回答";
    if (send(client_socket, message2, strlen(message2), 0) == -1) {
        perror("Send failed");
        close(client_socket);
        return 1;
    }

    // 接收响应（匹配服务端协议）
    {
        // 1. 接收4字节总长度
        uint32_t total_len;
        if (!receive_exact(client_socket, reinterpret_cast<char*>(&total_len), sizeof(total_len))) {
            perror("Failed to receive total length");
            close(client_socket);
            return 1;
        }
        total_len = ntohl(total_len);
        std::cout << "Total message length: " << total_len << std::endl;

        // 2. 接收4字节内容长度
        uint32_t content_len;
        if (!receive_exact(client_socket, reinterpret_cast<char*>(&content_len), sizeof(content_len))) {
            perror("Failed to receive content length");
            close(client_socket);
            return 1;
        }
        content_len = ntohl(content_len);

        // 3. 接收内容
        char* content = new char[content_len + 1];
        if (!receive_exact(client_socket, content, content_len)) {
            perror("Failed to receive content");
            delete[] content;
            close(client_socket);
            return 1;
        }
        content[content_len] = '\0';

        // 4. 接收4字节推理长度
        uint32_t reason_len;
        if (!receive_exact(client_socket, reinterpret_cast<char*>(&reason_len), sizeof(reason_len))) {
            perror("Failed to receive reason length");
            delete[] content;
            close(client_socket);
            return 1;
        }
        reason_len = ntohl(reason_len);

        // 5. 接收推理
        char* reason = new char[reason_len + 1];
        if (!receive_exact(client_socket, reason, reason_len)) {
            perror("Failed to receive reason");
            delete[] content;
            delete[] reason;
            close(client_socket);
            return 1;
        }
        reason[reason_len] = '\0';

        // 打印结果
        std::cout << "Content (" << content_len << " bytes): " << content << std::endl;
        std::cout << "Reason (" << reason_len << " bytes): " << reason << std::endl;

        // 释放内存
        delete[] content;
        delete[] reason;
    }
    // 第二次接收
    // {
    //     // 1. 先接收4字节长度头
    //     uint32_t message_len;
    //     if (!receive_exact(client_socket, reinterpret_cast<char*>(&message_len), sizeof(message_len))) {
    //         perror("Failed to receive length header");
    //         close(client_socket);
    //         return 1;
    //     }
    //     message_len = ntohl(message_len);
    //     std::cout << "Expecting message of length: " << message_len << std::endl;

    //     // 2. 接收实际数据
    //     char* buffer = new char[message_len + 1]; // +1 for null terminator
    //     if (!receive_exact(client_socket, buffer, message_len)) {
    //         perror("Failed to receive message body");
    //         delete[] buffer;
    //         close(client_socket);
    //         return 1;
    //     }
    //     buffer[message_len] = '\0';
    //     std::cout << "Received from server: " << buffer << std::endl;
    //     delete[] buffer;
    // }

    char* buffer = new char[BUFFER_SIZE];
     ssize_t bytes_received2 = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (bytes_received2 == -1) {
        perror("Receive failed");
        close(client_socket);
        return 1;
    }

    std::cout << buffer <<std::endl;
    while(1);
    close(client_socket);

    return 0;
}