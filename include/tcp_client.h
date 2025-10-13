#pragma once

#include "transport.h"
#include <memory>
#include <functional>
#include <string>
#include <vector>
#include <cstdint>
#include <atomic>
#include <thread>
#include <mutex>
#include <sys/epoll.h>

namespace rpc {

// TCP客户端抽象基类
class TcpClient {
public:
    virtual ~TcpClient() = default;

    // 连接服务器
    virtual bool connect(const std::string& host, uint16_t port) = 0;

    // 断开连接
    virtual void disconnect() = 0;

    // 发送消息
    virtual bool send(const std::vector<uint8_t>& data) = 0;

    // 获取连接状态
    virtual ConnectionState getState() const = 0;

    // 设置回调
    virtual void setMessageCallback(MessageCallback callback) = 0;
    virtual void setConnectionCallback(ConnectionCallback callback) = 0;
    virtual void setErrorCallback(ErrorCallback callback) = 0;
};

class TcpClientImpl : public TcpClient, public std::enable_shared_from_this<TcpClientImpl> {
public:
    TcpClientImpl();
    ~TcpClientImpl();

    // 连接服务器
    bool connect(const std::string& host, uint16_t port) override;

    // 断开连接
    void disconnect() override;

    // 发送消息
    bool send(const std::vector<uint8_t>& data) override;

    // 获取连接状态
    ConnectionState getState() const override;

    // 设置回调
    void setMessageCallback(MessageCallback callback) override;
    void setConnectionCallback(ConnectionCallback callback) override;
    void setErrorCallback(ErrorCallback callback) override;

    // 返回服务器地址
    std::string getServerAddress() const;

    // 返回socket文件描述符
    int getSocketFd() const;

private:
    int sockfd_;
    int epoll_fd_;
    std::atomic<bool> running_;
    std::string server_addr_;
    ConnectionState state_;
    std::thread event_thread_;
    std::mutex send_mutex_;
    std::vector<uint8_t> buffer_;

    MessageCallback message_callback_;
    ConnectionCallback connection_callback_;
    ErrorCallback error_callback_;

    // epoll事件循环
    void eventLoop();
    
    // 处理读事件
    void handleRead();
    
    // 处理写事件
    bool handleWrite(const std::vector<uint8_t>& data);
    
    // 处理错误事件
    void handleError();

    // 处理错误
    void handleError(const std::string& error_msg);
};
}