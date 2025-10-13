#pragma once

#include "transport.h"
#include "tcp_connection.h"
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

// TCP服务器抽象基类
class TcpServer {
public:
    virtual ~TcpServer() = default;

    // 启动服务器
    virtual bool start(uint16_t port, const std::string& host = "0.0.0.0") = 0;

    // 停止服务器
    virtual void stop() = 0; 

    // 设置回调
    virtual void setConnectionCallback(ConnectionCallback callback) = 0;

    // 获取服务器状态
    virtual bool isRunning() const = 0; 
};

class TcpServerImpl : public TcpServer {
public:
    TcpServerImpl();
    ~TcpServerImpl();

    // 启动服务器
    bool start(uint16_t port, const std::string& host = "0.0.0.0") override;

    // 停止服务器
    void stop() override;

    // 设置回调
    void setConnectionCallback(ConnectionCallback callback) override;

    // 获取服务器状态
    bool isRunning() const override;


private:
    int listen_sockfd_; // 监听sockfd
    int epoll_fd_;
    std::atomic<bool> running_; // 运行状态
    std::thread server_thread_; // 服务器线程
    std::vector<std::shared_ptr<TcpConnectionImpl>> connections_; // 连接列表
    std::mutex connections_mutex_;
    ConnectionCallback connection_callback_;
    size_t max_connections_;

    // 服务器线程主函数
    void serverThreadMain();

    // 处理新连接
    void handleNewConnection();

    // 处理客户端事件
    void handleClientEvent(const struct epoll_event& event);

    // 处理读事件
    void handleClientRead(TcpConnectionImpl* connection);

    // 处理关闭事件
    void handleClientClose(TcpConnectionImpl* connection);
};

}