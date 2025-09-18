#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>

// 注册中心地址
const char* REGISTRY_IP = "127.0.0.1";
const int REGISTRY_PORT = 8080;

// RPC请求的目标服务
const std::string TARGET_SERVICE = "calc_service";

// 服务提供者地址列表
std::vector<std::string> providers;
size_t round_robin_index = 0;

// 向注册中心查询服务地址
void discover_service(){
    // 创建socket
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        std::cerr << "Socket creation error" << std::endl;
        return;
    }

    // 创建sockaddr
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(REGISTRY_PORT);
    if (inet_pton(AF_INET, REGISTRY_IP, &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported" << std::endl;
        close(sock_fd);
        return;
    }

    // 向注册中心发送连接请求
    if (connect(sock_fd, (struct sockaddr*)&serv_addr,sizeof(serv_addr)) < 0) {
        std::cerr << "Connection to registry failed" << std::endl;
        close(sock_fd);
        return;
    }

    // 发送GET请求，获取服务实例
    std::string query = "GET:" + TARGET_SERVICE;
    send(sock_fd, query.c_str(), query.length(), 0);

    char buffer[1024] = {0};
    int bytes_read = recv(sock_fd, buffer, 1024, 0);
    std::string response(buffer, bytes_read);

    close(sock_fd);

    // 没有该服务
    if (response == "No Service" || response.empty()){
        std::cout << "[Consumer] " << response << " for " << TARGET_SERVICE << std::endl;
        providers.clear();
        return;
    }
    
    // 解析地址列表
    providers.clear();
    std::stringstream ss(response);
    std::string address;
    while(std::getline(ss, address, ',')) {
        providers.push_back(address);
    }
    
    std::cout << "[Consumer] Discovered " << providers.size() << " providers for " << TARGET_SERVICE << std::endl;
}

// 模拟发起RPC调用
void call_rpc(const std::string& address) {
    size_t pos = address.find(':');
    if (pos == std::string::npos) {
        std::cerr << "[Consumer] Invalid address: " << address << std::endl;
        return;
    }
    std::string ip = address.substr(0, pos);
    int port = std::stoi(address.substr(pos + 1));

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        std::cerr << "[Consumer] Socket creation failed" << std::endl;
        return;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr) <= 0) {
        std::cerr << "[Consumer] Invalid address " << ip << std::endl;
        close(sock_fd);
        return;
    }

    if (connect(sock_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
        std::cerr << "[Consumer] Failed to connect to provider " << address << std::endl;
        close(sock_fd);
        return;
    }

    std::string request = "Hello from Client!";
    send(sock_fd, request.c_str(), request.length(), 0);

    char buffer[1024] = {0};
    int bytes_read = recv(sock_fd, buffer, 1024, 0);
    if (bytes_read > 0) {
        std::cout << "[Consumer] Received response from " << address << ": " << std::string(buffer) << std::endl;
    }
    
    close(sock_fd);
}

int main() {
    while (true) {
        discover_service();
        
        // 负载均衡：轮询选择服务实例
        if (!providers.empty()) {
            std::string selected_provider = providers[round_robin_index % providers.size()];
            round_robin_index++;
            
            std::cout << "[Consumer] Calling provider at " << selected_provider << std::endl;
            call_rpc(selected_provider);
        } else {
            std::cout << "[Consumer] No providers available, waiting..." << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    return 0;
}