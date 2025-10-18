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

// rpc 请求结构
struct RpcRequest {
    uint64_t request_id; // 请求ID
    std::string service_name; // 服务名称
    std::string method_name; // 方法名称
    std::vector<uint8_t> request_data; // 请求数据(序列化之后的数据)

    // 初始化，id为0
    RpcRequest():request_id(0) {}
};

// rpc 响应结构
struct RpcResponse {
    uint64_t request_id;
    bool success;
    std::string error_message;
    std::vector<uint8_t> response_data;

    // 初始化
    RpcResponse():request_id(0),success(false) {}
};

}