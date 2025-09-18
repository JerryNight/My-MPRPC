#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cassert>

// 注册中心的地址
const char* REGISTRY_IP = "127.0.0.1";
const int REGISTRY_PORT = 8080;

// 服务提供者(本服务)的地址
const char* MY_IP = "127.0.0.1";
const int MY_PORT = 9000;
const std::string SERVICE_NAME = "calc_service";

// 心跳线程：定时向注册中心发送心跳
void heartbeat_thread() {
    // 创建socket
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        std::cerr << "Socket creation error" << std::endl;
        return;
    }

    // 创建sockaddr
    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(REGISTRY_PORT);
    if (inet_pton(AF_INET, REGISTRY_IP, &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported" << std::endl;
        close(sock_fd);
        return;
    }

    // 向注册中心发送连接请求
    int err = connect(sock_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (err < 0) {
        perror("connect to registry failed");
        close(sock_fd);
        return;
    }
    
    std::string address = std::string(MY_IP) + ":" + std::to_string(MY_PORT);
    std::string registration_msg = SERVICE_NAME + ":" + address;

    // 第一次注册
    send(sock_fd, registration_msg.c_str(), registration_msg.length(), 0);
    std::cout << "[Provider] Registered as " << registration_msg << std::endl;

    // 定期发送心跳
    while(true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        send(sock_fd, registration_msg.c_str(), registration_msg.length(), 0);
        std::cout << "[Provider] Sent heartbeat to registry" << std::endl;
    }
    
    close(sock_fd);
}

// 处理RPC请求线程
void rpc_service_thread(){
    // 创建sokcet
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        return;
    }
    
    // 创建sockaddr
    sockaddr_in address;
    socklen_t addr_len = sizeof(address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(MY_PORT);

    // 绑定监听端口
    if (bind(server_fd,(struct sockaddr*)&address, addr_len) < 0) {
        perror("bind failed");
        return;
    }
    
    // 监听
    if (listen(server_fd, 5) < 0) {
        perror("listen failed");
        return;
    }

    std::cout << "[Provider] Listening for RPC requests on port " << MY_PORT << std::endl;

    while (true) {
        int new_socket = accept(server_fd, (struct sockaddr*)&address, &addr_len);
        if (new_socket < 0) {
            perror("accept failed");
            continue;
        }
        
        char buffer[1024] = {0};
        int bytes_read = recv(new_socket, buffer, 1024, 0);
        if (bytes_read > 0) {
            std::string request(buffer, bytes_read);
            std::cout << "[Provider] Received RPC request: " << request << std::endl;

            std::string response = "Response for " + request;
            send(new_socket, response.c_str(), response.length(), 0);
        }
        
        close(new_socket);
    }
}

int main(){
    std::thread heartbeat_t(heartbeat_thread);
    heartbeat_t.detach();

    std::thread rpc_thread(rpc_service_thread);
    rpc_thread.join();

    return 0;
}