#include "../../include/tcp_client.h"
#include <sys/socket.h>      // 系统socket相关头文件
#include <netinet/in.h>      // 网络地址结构头文件
#include <arpa/inet.h>       // 网络地址转换头文件
#include <unistd.h>          // Unix标准定义头文件
#include <fcntl.h>           // 文件控制头文件
#include <sys/epoll.h>       // epoll相关头文件
#include <sys/select.h>      // select相关头文件
#include <errno.h>           // 错误号定义头文件
#include <cstring>           // 字符串操作头文件
#include <iostream>          // 输入输出流头文件
#include <stdexcept>         // 异常处理头文件

namespace rpc {

    TcpClientImpl::TcpClientImpl()
        :sockfd_(-1),
         state_(ConnectionState::DISCONNECTED),
         epoll_fd_(-1),
         running_(false)
    {}

    TcpClientImpl::~TcpClientImpl() {
        disconnect();
    }

    // 连接服务器
    bool TcpClientImpl::connect(const std::string& host, uint16_t port) {
        if (state_ == ConnectionState::CONNECTED) {  // 如果已连接
            return true;
        }

        state_ = ConnectionState::CONNECTING;

        // 创建socket
        sockfd_ = socket(AF_INET, SOCK_STREAM, 0);  // 创建TCP socket
        if (sockfd_ == -1) {
            std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
            state_ = ConnectionState::DISCONNECTED; 
            return false;
        }

        // 设置socket为非阻塞模式
        int flags = fcntl(sockfd_, F_GETFL, 0);
        if (fcntl(sockfd_, F_SETFL, flags | O_NONBLOCK) == -1) {
            std::cerr << "Failed to set socket to non-blocking: " << strerror(errno) << std::endl;
            ::close(sockfd_);
            sockfd_ = -1;
            state_ = ConnectionState::DISCONNECTED;
            return false;
        }

        // 准备服务器地址
        struct sockaddr_in server_addr;  // 服务器地址结构
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        if (inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr) <= 0) {  // 转换IP地址失败
            std::cerr << "Invalid server address: " << host << std::endl;
            close(sockfd_);
            sockfd_ = -1;
            state_ = ConnectionState::DISCONNECTED;
            return false;
        }

        // 尝试连接
        int result = ::connect(sockfd_, (struct sockaddr*)&server_addr, sizeof(server_addr));
        if (result == -1) {  // 连接失败
            if (errno == EINPROGRESS) {  // 如果连接正在进行中（非阻塞socket的正常情况）
                // 使用select等待连接完成
                fd_set write_fds;
                fd_set error_fds;
                struct timeval timeout;

                FD_ZERO(&write_fds);
                FD_ZERO(&error_fds);
                FD_SET(sockfd_, &write_fds);
                FD_SET(sockfd_, &error_fds);

                timeout.tv_sec = 5; // 5秒
                timeout.tv_usec = 0;

                int select_result = select(sockfd_ + 1, nullptr, &write_fds, &error_fds, &timeout);
                if (select_result == -1) { // select 失败
                    std::cerr << "select failed: " << strerror(errno) << std::endl;
                    close(sockfd_);
                    sockfd_ = -1; 
                    state_ = ConnectionState::DISCONNECTED;
                    return false;
                } else if (select_result == 0) { // 超时
                    std::cerr << "Connection timeout" << std::endl;
                    close(sockfd_);
                    sockfd_ = -1; 
                    state_ = ConnectionState::DISCONNECTED;
                    return false;
                } else if (FD_ISSET(sockfd_, &error_fds)) {
                    std::cerr << "Connection failed" << std::endl;
                    close(sockfd_);
                    sockfd_ = -1; 
                    state_ = ConnectionState::DISCONNECTED;
                    return false;
                } else if (FD_ISSET(sockfd_, &write_fds)) {
                    // 检查连接是否真的成功
                    int error = 0;
                    socklen_t len = sizeof(error);
                    if (getsockopt(sockfd_, SOL_SOCKET, SO_ERROR, &error, &len) == -1 || error != 0) {
                        std::cerr << "Connection failed: " << (error ? strerror(error) : "Unknown error") << std::endl;
                        close(sockfd_);
                        sockfd_ = -1; 
                        state_ = ConnectionState::DISCONNECTED;
                        return false;
                    }
                }
            } else { // 其他连接错误
                std::cerr << "Failed to connect: " << strerror(errno) << std::endl;
                close(sockfd_);
                sockfd_ = -1; 
                state_ = ConnectionState::DISCONNECTED;
                return false;
            }
        }
        
        state_ = ConnectionState::CONNECTED;
        server_addr_ = host + ":" + std::to_string(port);

        // 创建 epoll
        epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
        if (epoll_fd_ == -1) {
            std::cerr << "Failed to create epoll: " << strerror(errno) << std::endl;
            close(sockfd_);
            sockfd_ = -1;
            state_ = ConnectionState::DISCONNECTED;
            return false;
        }

        // 将socket添加到epoll
        struct epoll_event event;
        event.events = EPOLLIN | EPOLLET;  // 边沿触发
        event.data.fd = sockfd_;
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, sockfd_, &event) == -1) {
            std::cerr << "Failed to add socket to epoll: " << strerror(errno) << std::endl;
            close(epoll_fd_);
            close(sockfd_);
            epoll_fd_ = -1;
            sockfd_ = -1;
            state_ = ConnectionState::DISCONNECTED;
            return false;
        }

        // 启动事件循环线程
        running_ = true;
        event_thread_ = std::thread(&TcpClientImpl::eventLoop, this);

        // TUDO 调用连接回调。

        std::cout << "Connected to " << server_addr_ << std::endl;  // 输出连接成功信息
        return true;
    }

    // 断开连接
    void TcpClientImpl::disconnect() {
        if (state_ != ConnectionState::DISCONNECTED) {
            state_ = ConnectionState::DISCONNECTING;
            running_ = false;
            if (sockfd_ != -1) {
                close(sockfd_);
                sockfd_ = -1;
            }
            // 关闭epoll
            if (epoll_fd_ != -1) {
                close(epoll_fd_);
                epoll_fd_ = -1;
            }
            // 等待事件循环线程结束
            if (event_thread_.joinable()) {
                event_thread_.join();
            }
            state_ = ConnectionState::DISCONNECTED;
            server_addr_.clear(); // 清空服务器地址
        }
    }

    // 发送消息
    bool TcpClientImpl::send(const std::vector<uint8_t>& data) {
        return handleWrite(data);
    }

    // 接收消息
    bool TcpClientImpl::receive(std::vector<uint8_t>& data) {
        if (state_ != ConnectionState::CONNECTED) {
            std::cerr << "Cannot receive: not connected" << std::endl;
            return false;
        }

        // 先读4字节的长度前缀
        std::vector<uint8_t> length_bytes;
        if (!readExactly(4, length_bytes)) {
            std::cerr << "Failed to read length prefix" << std::endl;
            return false;
        }

        // 解析长度 网络序->主机序
        uint32_t message_length_net;
        std::memcpy(&message_length_net, length_bytes.data(), 4);
        uint32_t message_length_host = ntohl(message_length_net);

        // 验证长度合理性
        const uint32_t MAX_MESSAGE_SIZE = 10 * 1024 * 1024; // 10M
        if (message_length_host == 0 || message_length_host > MAX_MESSAGE_SIZE) {
            std::cerr << "Invalid message length: " << message_length_host << std::endl;
            return false;
        }

        // 读取完整消息
        if (!readExactly(message_length_host, data)) {
            std::cerr << "Failed to read message data" << std::endl;
            return false;
        }

        return true;
    }

    // 获取连接状态
    ConnectionState TcpClientImpl::getState() const {
        return state_;
    }

    // 设置回调
    void TcpClientImpl::setMessageCallback(MessageCallback callback) {
        message_callback_ = std::move(callback);
    }
    void TcpClientImpl::setConnectionCallback(ConnectionCallback callback) {
        connection_callback_ = std::move(callback);
    }
    void TcpClientImpl::setErrorCallback(ErrorCallback callback) {
        error_callback_ = std::move(callback);
    }

    // 返回服务器地址
    std::string TcpClientImpl::getServerAddress() const {
        return server_addr_;  
    }

    // 返回socket文件描述符
    int TcpClientImpl::getSocketFd() const {
        return sockfd_;  
    }

    // 处理错误
    void TcpClientImpl::handleError(const std::string& error_msg) {
        state_ = ConnectionState::DISCONNECTED;
        std::cerr << "TcpClient error: " << error_msg << std::endl;
    }

    // 事件循环
    void TcpClientImpl::eventLoop() {
        const int max_events = 10;
        struct epoll_event events[max_events];

        while (running_) {
            int nfds = epoll_wait(epoll_fd_, events, max_events, 1000);  // 1秒超时
            if (nfds == -1) {
                if (errno == EINTR) {
                    continue;
                } else {
                    std::cerr << "epoll_wait failed: " << strerror(errno) << std::endl;
                    break;
                }
            }

            for (int i = 0; i < nfds; ++i) {
                if (events[i].data.fd == sockfd_) {
                    if (events[i].events & EPOLLIN) {
                        handleRead(buffer_);
                    }
                    if (events[i].events & EPOLLOUT) {
                        handleWrite(buffer_);
                    }
                    if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                        handleError();
                    }
                }
            }
        }
    }
    
    // 处理读事件
    void TcpClientImpl::handleRead(std::vector<uint8_t>& data) {
        ssize_t n = recv(sockfd_, data.data(), data.size(), 0);

        if (n > 0) {
            // 调用消息回调
            if (message_callback_) {
                // 创建一个临时的TcpConnection对象来调用回调
                // 这里简化处理，实际项目中可能需要更复杂的连接管理
                // message_callback_(shared_from_this(), buffer);
            }
            std::cout << "Received " << n << " bytes from " << server_addr_ << std::endl;
        } else if (n == 0) {
            // 连接被对端关闭
            handleError("Connection closed by peer");
        } else {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                handleError("Failed to receive data: " + std::string(strerror(errno)));
            }
        }
    }
    
    // 处理写事件
    bool TcpClientImpl::handleWrite(const std::vector<uint8_t>& data) {
        if (state_ != ConnectionState::CONNECTED) {  // 检查连接状态
            return false;  // 连接未建立，发送失败
        }

        size_t total_sent = 0;  // 已发送字节数
        size_t total_size = data.size();  // 总数据大小

        while (total_sent < total_size) {  // 循环发送直到所有数据发送完成
            ssize_t sent = ::send(sockfd_, data.data() + total_sent, total_size - total_sent, MSG_NOSIGNAL);  // 发送数据
            if (sent == -1) {  // 发送失败
                if (errno == EAGAIN || errno == EWOULDBLOCK) {  // 发送缓冲区满，稍后重试
                    continue;  // 继续尝试发送
                } else {  // 其他错误
                    handleError("Send failed: " + std::string(strerror(errno)));  // 处理错误
                    return false;  // 发送失败
                }
            } else if (sent == 0) {  // 连接被对端关闭
                handleError("Connection closed by peer");  // 处理连接关闭
                return false;  // 发送失败
            }
            total_sent += sent;  // 更新已发送字节数
        }

        return true;  // 发送成功
    }
    
    // 处理错误事件
    void TcpClientImpl::handleError() {
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(sockfd_, SOL_SOCKET, SO_ERROR, &error, &len) == 0 && error != 0) {
            handleError("Socket error: " + std::string(strerror(error)));
        } else {
            handleError("Unknown socket error");
        }
    }

    // 读取指定长度的数据
    bool TcpClientImpl::readExactly(size_t length, std::vector<uint8_t>& data) {
        data.clear();
        data.reserve(length);

        std::lock_guard<std::mutex> lock(buffer_mutex_);

        // 先从buffer_里读数据
        if (!buffer_.empty()) {
            size_t bytes_from_buffer = std::min(length, buffer_.size());
            data.insert(data.end(), buffer_.begin(), buffer_.begin() + bytes_from_buffer);
            buffer_.erase(buffer_.begin(), buffer_.begin() + bytes_from_buffer);
        }

        // 还需要数据，就从socket读
        while (data.size() < length) {
            size_t remaining = length - data.size();
            std::vector<uint8_t> temp_buffer(remaining);

            // 阻塞
            ssize_t n = recv(sockfd_, temp_buffer.data(), remaining, 0);

            if (n > 0) {
                temp_buffer.resize(n);
                data.insert(data.end(), temp_buffer.begin(), temp_buffer.end());
            } else if (n == 0) {
                std::cerr << "Connection closed by peer while reading" << std::endl;
                return false;
            } else {
                if (errno == EINTR) {
                    // 被信号中断，继续接收
                    continue;
                } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 非阻塞socket超时，继续等待
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                } else {
                    std::cerr << "recv error: " << strerror(errno) << std::endl;
                    return false;
                }
            }
        }
    }
}