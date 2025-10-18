#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <functional>
#include "google/protobuf/service.h"
#include "google/protobuf/message.h"
#include "tcp_connection.h"
#include "serializer.h"
#include "serializer_factory.h"
#include "tcp_server.h"
#include "frame_codec.h"
#include "thread_pool.h"

namespace rpc {

// rpc 请求结构体
struct RpcRequest {
    std::string service_name;
    std::string method_name;
    std::vector<uint8_t> data;
    uint64_t request_id;
    std::string client_id;
};

// rpc 响应结构体
struct RpcResponse {
    uint64_t request_id;
    std::vector<uint8_t> data;
    bool success;
    std::string error_message;
};

// rpc 服务器配置
struct RpcServerConfig {
    std::string host; // 服务器监听地址
    uint16_t port;    // 服务器监听端口
    size_t thread_pool_size; // 线程池大小
    size_t max_connections;  // 最大连接数
    int connection_timeout_ms; // 连接超时（毫秒）
    int request_timeout_ms;    // 请求超时（毫秒）
    std::string serializer_type; // 序列化器类型

    RpcServerConfig()
        :host("0.0.0.0"),
         port(8080),
         thread_pool_size(std::thread::hardware_concurrency()),
         max_connections(1000),
         connection_timeout_ms(30000),
         request_timeout_ms(5000),
         serializer_type("protobuf") {}
};


// rpc 服务器
class RpcServer {
public:
    RpcServer(const RpcServerConfig& config = RpcServerConfig());
    ~RpcServer();

    // 禁用拷贝
    RpcServer(const RpcServer&) = delete;
    RpcServer& operator=(const RpcServer&) = delete;

    // 启动服务器
    bool start();

    // 停止服务器
    void stop();

    // 注册 Protobuf 服务
    bool registerService(google::protobuf::Service* service);

    // 注销服务
    bool unregisterService(const std::string& service_name);

    // 获取服务器状态
    bool isRunning() const;

    // 获取服务器配置
    const RpcServerConfig& getConfig() const;

    // 获取已注册服务列表
    std::vector<std::string> getRegisteredService() const;

    // 获取当前连接数
    size_t getConnectionCount() const;

    // 获取线程池大小
    size_t getThreadPoolSize() const;

private:
    RpcServerConfig config_; // 服务器配置
    std::unique_ptr<TcpServer> tcp_server_; // TCP服务器
    std::unique_ptr<FrameCodec> frame_codec_; // 编解码器
    std::unique_ptr<Serializer> serializer_; // 序列化器
    std::unique_ptr<ThreadPool> thread_pool_; // 线程池
    std::unordered_map<std::string, google::protobuf::Service*> services_; // 服务映射表
    std::unordered_map<std::shared_ptr<TcpConnection>, std::string> connections_; // 连接映射表
    mutable std::shared_mutex connections_mutex_; 
    mutable std::shared_mutex services_mutex_;
    std::atomic<bool> running_; // 运行状态标志

    // 初始化组件
    bool initializeComponents();

    // 处理新连接
    void handleNewConnection(std::shared_ptr<TcpConnection> connection);

    // 处理消息
    void handleMessage(std::shared_ptr<TcpConnection> connection, const std::vector<uint8_t>& data);

    // 处理rpc请求
    void handleRpcRequest(std::shared_ptr<TcpConnection> connection, const std::vector<uint8_t>& request_data);

    // 处理连接断开
    void handleConnectionClosed(std::shared_ptr<TcpConnection> connection);

    // 处理错误
    void handleError(std::shared_ptr<TcpConnection> connection, const std::string& error_message);

    // 发送响应
    void sendResponse(std::shared_ptr<TcpConnection> connection, const RpcResponse& response);

    // 解析rpc请求
    RpcRequest parseRpcRequest(const std::vector<uint8_t>& data);

    // 序列化rpc响应
    std::vector<uint8_t> serializeRpcResponse(const RpcResponse& response);

    // 调用服务方法
    std::vector<uint8_t> callServiceMethod(const std::string& service_name, const std::string& method_name, const std::vector<uint8_t>& request_data);
};


}