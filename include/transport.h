#pragma once

#include <memory>
#include <functional>
#include <string>
#include <vector>
#include <cstdint>

namespace rpc {

// 前向声明
class TcpConnection;

// 连接状态
enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    DISCONNECTING
};

// 回调函数
using MessageCallback = std::function<void(std::shared_ptr<TcpConnection>, const std::vector<uint8_t>&)>;
using ConnectionCallback = std::function<void(std::shared_ptr<TcpConnection>)>;
using WriteCompleteCallback = std::function<void(std::shared_ptr<TcpConnection>)>;
using ErrorCallback = std::function<void(std::shared_ptr<TcpConnection>, const std::string&)>;

}