#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sstream>

// 服务实例
struct ServiceInstance {
    std::string address;
    std::chrono::steady_clock::time_point last_heartbeat_time;
};

// 服务注册表
std::unordered_map<std::string, std::vector<ServiceInstance>> registry_map;
std::mutex registry_mutex;

// 设置文件为非阻塞
void set_nonblocking(int fd) {
    int flag = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}

// 监控线程
void heartbeat_monitor_thread() {
    while(true) {
        std::this_thread::sleep_for(std::chrono::seconds(5)); // 每5秒轮询一次
        
        std::lock_guard<std::mutex> lock(registry_mutex);
        auto now = std::chrono::steady_clock::now();
        
        for (auto map_it = registry_map.begin(); map_it != registry_map.end();) {
            for (auto vec_it = map_it->second.begin(); vec_it != map_it->second.end();) {
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - vec_it->last_heartbeat_time);
                if (duration.count() > 15) { // 超时移除
                    std::cout << "[Monitor] Removing timed out instance: " << vec_it->address << std::endl;
                    vec_it = map_it->second.erase(vec_it);
                } else {
                    ++vec_it;
                }
            }
            if (map_it->second.empty()) {
                std::cout << "[Monitor] Service " << map_it->first << " has no active instances, removing." << std::endl;
                map_it = registry_map.erase(map_it);
            } else {
                ++map_it;
            }
        }
    }
}

// 处理客户端请求（发现服务、注册服务、心跳）
void handle_client_request(int client_fd){
    char buffer[1024] = {0};
    int bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

    if (bytes_read <= 0) {
        close(client_fd);
        std::cout << "[IO] Client " << client_fd << " disconnected." << std::endl;
        return;
    }

    // request: GET:service_name  或者  service_name:address
    std::string request(buffer, bytes_read);
    size_t pos = request.find(":");
    if (pos == std::string::npos) {
        std::cout << "[IO] Invalid request format from client " << client_fd << std::endl;
        return;
    }

    std::string str1 = request.substr(0, pos);
    std::string str2 = request.substr(pos + 1);

    std::lock_guard<std::mutex> lock(registry_mutex);
    
    if (str1 == "GET") {
        std::string response;
        std::string target_service(std::move(str2));

        std::cout << "[IO] Discovery request for " << target_service << std::endl;
        auto map_it = registry_map.find(target_service);
        if (map_it == registry_map.end() || map_it->second.empty()) {
            response = "No Service";
        } else {
            // 返回所有实例地址，用逗号分隔
            std::stringstream ss;
            for (const auto& instance : map_it->second) {
                ss << instance.address << ",";
            }
            // 移除末尾','
            response = ss.str().substr(0, ss.str().length() - 1);
        }
        send(client_fd, response.c_str(), response.length(), 0);
        return;
    }

    std::string service_name(std::move(str1));
    std::string address(std::move(str2));
    auto now = std::chrono::steady_clock::now();
    auto it = registry_map.find(service_name);

    if (it != registry_map.end()) {
        bool found = false;
        for (auto& ins : it->second) {
            if (ins.address == address) {
                ins.last_heartbeat_time = now;
                found = true;
                std::cout << "[IO] Heartbeat received from " << address << std::endl;
                break;
            }
        }
        if (!found) {
            it->second.push_back({address, now});
            std::cout << "[IO] New instance registered: " << address << std::endl;
        }
    } else {
        registry_map[service_name].push_back({address, now});
        std::cout << "[IO] New service registered: " << service_name << " at " << address << std::endl;
    }
}

int main() {
    // 开启监控线程
    std::thread monitor_t(heartbeat_monitor_thread);
    monitor_t.detach();

    // 创建socket
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    set_nonblocking(listen_fd);
    
    struct sockaddr_in addr;
    memset(&addr, 0 ,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8080);

    // 监听
    bind(listen_fd,(struct sockaddr*)&addr, sizeof(addr));
    listen(listen_fd, 5);

    // 创建epoll
    int epoll_fd = epoll_create1(0);
    struct epoll_event event;
    event.data.fd = listen_fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &event);

    struct epoll_event events[64];
    std::cout << "Registry Center started on port 8080. Waiting for connections..." << std::endl;

    while (true) {
        int num_events = epoll_wait(epoll_fd, events, 64, -1);
        for (int i = 0; i < num_events; i++) {
            if (events[i].data.fd == listen_fd) {
                while (true) {
                    sockaddr_in client_addr;
                    socklen_t client_len = sizeof(client_addr);
                    int conn_fd = accept(listen_fd,(struct sockaddr*)&client_addr, &client_len);
                    if (conn_fd == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        } else {
                            perror("accept");
                            break;
                        }
                    }
                    set_nonblocking(conn_fd);
                    event.data.fd = conn_fd;
                    event.events = EPOLLIN | EPOLLET;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &event);
                    std::cout << "[IO] New client connected on fd " << conn_fd << std::endl;
                }
            } else {
                handle_client_request(events[i].data.fd);
            }
        }
    }

    close(listen_fd);
    close(epoll_fd);
    return 0;
}