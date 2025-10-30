#include "tcp_connection.h"
#include "transport.h"
#include <sys/socket.h>      // 系统socket相关头文件
#include <unistd.h>          // Unix标准定义头文件 close
#include <fcntl.h>           // 文件控制头文件
#include <errno.h>           // 错误号定义头文件
#include <cstring>           // 字符串操作头文件 strerror
#include <arpa/inet.h>       // 网络地址转换头文件
#include <iostream>

namespace rpc {

    // 构造函数
    TcpConnectionImpl::TcpConnectionImpl(int sockfd, const std::string& peer_addr)
        :sockfd_(sockfd), peer_addr_(peer_addr), state_(ConnectionState::CONNECTED)
    {
        // 设置 sockfd 为非阻塞
        int flags = fcntl(sockfd_, F_GETFL, 0);  // 获取当前socket标志
        fcntl(sockfd_, F_SETFL, flags | O_NONBLOCK);  // 设置为非阻塞模式
    }

    // 析构函数
    TcpConnectionImpl::~TcpConnectionImpl() {
        close();
    }

    // 发送数据
    bool TcpConnectionImpl::send(const std::vector<uint8_t>& data) {
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

        // 调用写入完成回调
        if (write_complete_callback_) {  // 如果设置了写入完成回调
            write_complete_callback_(shared_from_this());  // 调用回调函数
        }

        return true;  // 发送成功
    }


    // 关闭连接
    void TcpConnectionImpl::close() {
        if (state_ != ConnectionState::DISCONNECTED) {  // 如果连接未断开
            state_ = ConnectionState::DISCONNECTING;  // 设置状态为正在断开
            if (sockfd_ != -1) {  // 如果socket有效
                ::close(sockfd_);  // 关闭socket
                sockfd_ = -1;  // 重置socket文件描述符
            }
            state_ = ConnectionState::DISCONNECTED;  // 设置状态为已断开
        }
    }

    // 获取连接状态
    ConnectionState TcpConnectionImpl::getState() const {
        return state_;
    }

    // 获取远程地址
    std::string TcpConnectionImpl::getRemoteAddress() const {
        return peer_addr_;
    }

    // 设置回调
    void TcpConnectionImpl::setMessageCallback(MessageCallback callback) {
        message_callback_ = std::move(callback);
    }
    void TcpConnectionImpl::setConnectionCallback(ConnectionCallback callback) {
        connection_callback_ = std::move(callback);
    }
    void TcpConnectionImpl::setWriteCompleteCallback(WriteCompleteCallback callback) {
        write_complete_callback_ = std::move(callback);
    }
    void TcpConnectionImpl::setErrorCallback(ErrorCallback callback) {
        error_callback_ = std::move(callback);
    }
    MessageCallback& TcpConnectionImpl::getMessageCallback() {
        return message_callback_;
    }
    ConnectionCallback& TcpConnectionImpl::getConnectionCallback() {
        return connection_callback_;
    }
    WriteCompleteCallback& TcpConnectionImpl::getWriteCompleteCallback() {
        return write_complete_callback_;
    }
    ErrorCallback& TcpConnectionImpl::getErrorCallback() {
        return error_callback_;
    }

    // 获取 sockfd
    int TcpConnectionImpl::getSocketFd() const {
        return sockfd_;
    }

    // 获取输入缓冲区
    Buffer* TcpConnectionImpl::getInputBuffer() {
        return &input_buffer_;
    }

    // 获取输出缓冲区
    Buffer* TcpConnectionImpl::getOutputBuffer() {
        return &output_buffer_;
    }

    // 处理错误
    void TcpConnectionImpl::handleError(const std::string& error_msg) {
        state_ = ConnectionState::DISCONNECTED;
        if (error_callback_) {
            // 调用回调函数
            error_callback_(shared_from_this(), error_msg);
        }
    }

    // 解码一个完整的帧（使用新的Buffer）
    bool TcpConnectionImpl::decodeFrame(std::vector<uint8_t>& frame_data) {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        
        // 检查是否有足够的数据读取长度字段
        if (input_buffer_.readableBytes() < 4) {
            return false;
        }
        
        // 读取长度字段（不移除）
        uint32_t length_host = input_buffer_.peekInt<uint32_t>();
        
        // 验证长度
        const uint32_t MAX_FRAME_SIZE = 10 * 1024 * 1024;
        if (length_host == 0 || length_host > MAX_FRAME_SIZE) {
            std::cerr << "Invalid frame length: " << length_host << std::endl;
            input_buffer_.retrieveAll();  // 清空缓冲区，防止一直出错
            return false;
        }
        
        // 检查是否有完整帧（4字节长度 + 消息体）
        if (input_buffer_.readableBytes() < length_host + 4) {
            // 半包，等待更多数据
            return false;
        }

        // 移除长度字段
        input_buffer_.retrieve(4);
        
        // 提取完整帧数据
        frame_data = input_buffer_.retrieveAsVector(length_host);
        
        return true;
    }


}