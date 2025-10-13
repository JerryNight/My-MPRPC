#pragma once

#include "transport.h"
#include <memory>
#include <functional>
#include <string>
#include <vector>
#include <cstdint>

namespace rpc {

// TCP连接抽象基类
class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
public:
    virtual ~TcpConnection() = default;

    // 发送数据
    virtual bool send(const std::vector<uint8_t>& data) = 0;

    // 关闭连接
    virtual void close() = 0;

    // 获取连接状态
    virtual ConnectionState getState() const = 0;

    // 获取远程地址
    virtual std::string getRemoteAddress() const = 0;

    // 设置回调
    virtual void setMessageCallback(MessageCallback callback) = 0;
    virtual void setConnectionCallback(ConnectionCallback callback) = 0;
    virtual void setWriteCompleteCallback(WriteCompleteCallback callback) = 0;
    virtual void setErrorCallback(ErrorCallback callback) = 0;
};

class TcpConnectionImpl : public TcpConnection {
public:
    TcpConnectionImpl(int sockfd, const std::string& peer_addr);
    ~TcpConnectionImpl();

    // 发送数据
    bool send(const std::vector<uint8_t>& data) override;

    // 关闭连接
    void close() override;

    // 获取连接状态
    ConnectionState getState() const override;

    // 获取远程地址
    std::string getRemoteAddress() const override;

    // 设置回调
    void setMessageCallback(MessageCallback callback) override;
    void setConnectionCallback(ConnectionCallback callback) override;
    void setWriteCompleteCallback(WriteCompleteCallback callback) override;
    void setErrorCallback(ErrorCallback callback) override;

    // 获取 socket
    int getSocketFd() const;

private:
    int sockfd_; // 客户端fd
    std::string peer_addr_; // 对端地址
    ConnectionState state_; // 连接状态
    MessageCallback message_callback_;
    ConnectionCallback connection_callback_;
    WriteCompleteCallback write_complete_callback_;
    ErrorCallback error_callback_;
    
    // 处理错误
    void handleError(const std::string& error_msg);
};

}