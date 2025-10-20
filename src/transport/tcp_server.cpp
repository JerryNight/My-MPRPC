#include "tcp_server.h"
#include <atomic>
#include <thread>
#include <vector>
#include <iostream>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <mutex>
#include <unistd.h>
#include <cstring>

namespace rpc {

    TcpServerImpl::TcpServerImpl()
        :listen_sockfd_(-1),
         epoll_fd_(-1),
         running_(false),
         max_connections_(1000)
    {}

    TcpServerImpl::~TcpServerImpl() {
        stop();
    }

    // 启动服务器
    bool TcpServerImpl::start(uint16_t port, const std::string& host) {
        if (running_) {
            return true;
        }

        // 创建 socket
        listen_sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_sockfd_ == -1) {
            std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
            return false;
        }

        // 设置 socket
        int opt = 1;
        if (setsockopt(listen_sockfd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
            std::cerr << "Failed to set socket option: " << strerror(errno) << std::endl;  // 输出错误信息
            close(listen_sockfd_);  // 关闭socket
            listen_sockfd_ = -1;  // 重置socket文件描述符
            return false;
        }

        // 绑定IP和端口
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        server_addr.sin_addr.s_addr = inet_addr(host.c_str());

        if (bind(listen_sockfd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
            std::cerr << "Failed to bind address: " << strerror(errno) << std::endl;  // 输出错误信息
            close(listen_sockfd_);  // 关闭socket
            listen_sockfd_ = -1;  // 重置socket文件描述符
            return false;
        }

        // 开始监听
        if (listen(listen_sockfd_, 128) == -1) {  // 开始监听连接请求
            std::cerr << "Failed to listen: " << strerror(errno) << std::endl;
            ::close(listen_sockfd_);
            listen_sockfd_ = -1;
            return false;
        }

        // 创建 epoll
        epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
        if (epoll_fd_ == -1) {  // 创建epoll失败
            std::cerr << "Failed to create epoll: " << strerror(errno) << std::endl;  // 输出错误信息
            ::close(listen_sockfd_);  // 关闭socket
            listen_sockfd_ = -1;  // 重置socket文件描述符
            return false;  // 返回失败
        }

        struct epoll_event event;
        event.events = EPOLLIN;
        event.data.fd = listen_sockfd_;
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_sockfd_, &event) == -1) {
            std::cerr << "Failed to add listen socket to epoll: " << strerror(errno) << std::endl;  // 输出错误信息
            close(epoll_fd_); 
            close(listen_sockfd_);
            epoll_fd_ = -1; 
            listen_sockfd_ = -1; 
            return false;
        }

        running_ = true;
        server_thread_ = std::thread(&TcpServerImpl::serverThreadMain, this);

        std::cout << "TCP Server started on " << host << ":" << port << std::endl;
        return true;
    }

    // 停止服务器
    void TcpServerImpl::stop() {
        if (!running_) {  // 如果服务器未运行
            return;
        }

        running_ = false;

        // 关闭监听socket
        if (listen_sockfd_ != -1) {  // 如果监听socket有效
            ::close(listen_sockfd_);
            listen_sockfd_ = -1;
        }

        // 关闭所有连接
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            for (auto& connection : connections_) {  // 遍历所有连接
                connection->close();  // 关闭连接
            }
            connections_.clear();  // 清空连接列表
        }

        // 关闭epoll
        if (epoll_fd_ != -1) {
            ::close(epoll_fd_);
            epoll_fd_ = -1;
        }

        // 等待服务器线程结束
        if (server_thread_.joinable()) {
            server_thread_.join();  // 等待线程结束
        }

        std::cout << "TCP Server stopped" << std::endl;
    }

    // 设置回调
    void TcpServerImpl::setConnectionCallback(ConnectionCallback callback) {
        connection_callback_ = std::move(callback);
    }

    // 获取服务器状态
    bool TcpServerImpl::isRunning() const {
        return running_;
    }

    // 服务器线程主函数
    void TcpServerImpl::serverThreadMain() {
        const int max_events = 100;
        struct epoll_event events[max_events];

        while (running_) {
            int nfds = epoll_wait(epoll_fd_, events, max_events, 1000);
            if (nfds == -1) {  // 等待事件失败
                if (errno == EINTR) {  // 被信号中断
                    continue;  // 继续循环
                } else {  // 其他错误
                    std::cerr << "epoll_wait failed: " << strerror(errno) << std::endl;  // 输出错误信息
                    break;  // 退出循环
                }
            }

            for (int i = 0; i < nfds; ++i) {  // 处理每个事件
                if (events[i].data.fd == listen_sockfd_) {  // 如果是监听socket事件
                    handleNewConnection();  // 处理新连接
                } else {  // 如果是客户端连接事件
                    handleClientEvent(events[i]);  // 处理客户端事件
                }
            }
        }
    }

    // 处理新连接
    void TcpServerImpl::handleNewConnection() {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr); 
        int client_sockfd = accept(listen_sockfd_, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_sockfd == -1) {  // 接受连接失败
            if (errno != EAGAIN && errno != EWOULDBLOCK) {  // 如果不是资源暂时不可用
                std::cerr << "Failed to accept connection: " << strerror(errno) << std::endl;
            }
            return;  // 返回
        }

        // 检查连接数限制
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);  // 获取连接锁
            if (connections_.size() >= max_connections_) {  // 如果连接数超过限制
                close(client_sockfd);  // 关闭新连接
                std::cerr << "Connection limit exceeded" << std::endl;  // 输出错误信息
                return;  // 返回
            }
        }

        // 创建连接对象
        std::string peer_addr = inet_ntoa(client_addr.sin_addr);
        peer_addr += ":" + std::to_string(ntohs(client_addr.sin_port)); 
        auto connection = std::make_shared<TcpConnectionImpl>(client_sockfd, peer_addr); 

        // 将客户端socket添加到epoll
        struct epoll_event event; 
        event.events = EPOLLIN | EPOLLET;  // 边沿触发
        event.data.ptr = connection.get();  // 设置数据指针为连接对象
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_sockfd, &event) == -1) {
            std::cerr << "Failed to add client socket to epoll: " << strerror(errno) << std::endl;
            connection->close();
            return;
        }

        // 添加到连接列表
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            connections_.push_back(connection);
        }

        // 调用连接回调
        if (connection_callback_) { 
            connection_callback_(connection); 
        }

        std::cout << "New connection from " << peer_addr << std::endl;
    }

    // 处理客户端事件（写事件-发送响应，在TcpConnection里处理-send）
    void TcpServerImpl::handleClientEvent(const struct epoll_event& event) {
        auto* connection = static_cast<TcpConnectionImpl*>(event.data.ptr);
        if (!connection) {
            return;
        }

        if (event.events & EPOLLIN) {
            handleClientRead(connection);
        }

        if (event.events & (EPOLLERR | EPOLLHUP)) {
            handleClientClose(connection);
        }
    }

    // 处理读事件
    void TcpServerImpl::handleClientRead(TcpConnectionImpl* connection) {
        std::vector<uint8_t> buffer(4096);
        ssize_t n = recv(connection->getSocketFd(), buffer.data(), buffer.size(), 0);

        if (n > 0) {
            // TODO 接收到数据，调用消息处理回调
            buffer.resize(n);
            // 把数据追加到connection里的读缓冲区
            connection->appendToReadBuffer(buffer);
            // 尝试解码完整的帧
            std::vector<uint8_t> frame_data;
            // 使用while循环，解决粘包问题（一次接收到多个请求）
            while (connection->decodeFrame(frame_data)) {
                // 触发 MessageCallback -> 调用 rpc_server 里的parseRequest
                if (connection->getMessageCallback()) {
                    connection->getMessageCallback()(connection->shared_from_this(), frame_data);
                }
            }
        } else if (n == 0) {
            handleClientClose(connection);
        } else {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {  // 如果不是资源暂时不可用
                std::cerr << "Failed to receive data: " << strerror(errno) << std::endl;
                handleClientClose(connection);
            }
        }
    }

    // 处理关闭事件
    void TcpServerImpl::handleClientClose(TcpConnectionImpl* connection) {
        // 从epoll中移除
        int sockfd = connection->getSocketFd();
        std::string peer_addr = connection->getRemoteAddress();
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, sockfd, nullptr);

        // 从连接列表中移除
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);  // 获取连接锁
            auto it = std::find_if(connections_.begin(), connections_.end(),  // 查找连接
                [connection](const std::shared_ptr<TcpConnectionImpl>& conn) {
                    return conn.get() == connection;
                });
            if (it != connections_.end()) { 
                connections_.erase(it); 
            }
        }

        connection->close(); 
        std::cout << "Connection closed: " << peer_addr << std::endl;
    }

}