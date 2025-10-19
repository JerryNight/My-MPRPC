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
#include "registry.h"
#include "registry_factory.h"

namespace rpc {


// rpc 服务器配置
struct RpcServerConfig {
    std::string host; // 服务器监听地址
    uint16_t port;    // 服务器监听端口
    size_t thread_pool_size; // 线程池大小
    size_t max_connections;  // 最大连接数
    int connection_timeout_ms; // 连接超时（毫秒）
    int request_timeout_ms;    // 请求超时（毫秒）
    std::string serializer_type; // 序列化器类型
    // 配置服务注册中心
    bool enable_registry; // 是否启用服务注册中心
    std::string registry_type; // 类型（zookeeper）
    std::string registry_address; // 地址
    int service_weight; // 服务权重
    int heartbeat_interval_ms; // 心跳间隔（毫秒）

    RpcServerConfig()
        :host("0.0.0.0"),
         port(8080),
         thread_pool_size(std::thread::hardware_concurrency()),
         max_connections(1000),
         connection_timeout_ms(30000),
         request_timeout_ms(5000),
         serializer_type("protobuf"),
         enable_registry(false),
         registry_type("zookeeper"),
         registry_address("localhost:2181"),
         service_weight(1),
         heartbeat_interval_ms(10000) {}
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

    // 设置服务注册中心
    void setRegistry(std::unique_ptr<ServiceRegistry> registry);

    // 获取注册中心
    ServiceRegistry* getRegistry() const;
private:
    RpcServerConfig config_; // 服务器配置
    std::unique_ptr<TcpServer> tcp_server_; // TCP服务器
    std::unique_ptr<FrameCodec> frame_codec_; // 编解码器
    std::unique_ptr<Serializer> serializer_; // 序列化器
    std::unique_ptr<ThreadPool> thread_pool_; // 线程池
    std::unique_ptr<ServiceRegistry> registry_; // 服务注册中心
    std::unordered_map<std::string, google::protobuf::Service*> services_; // 服务映射表
    std::unordered_map<std::shared_ptr<TcpConnection>, std::string> connections_; // 连接映射表
    mutable std::shared_mutex connections_mutex_; 
    mutable std::shared_mutex services_mutex_;
    std::atomic<bool> running_; // 运行状态标志
    std::thread heartbeat_thread_; // 心跳线程
    std::atomic<bool> heartbeat_running_; // 心跳线程是否运行

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

    // 注册服务到注册中心
    bool registerToRegistry(const std::string& service_name);

    // 从注册中心注销服务
    bool unregisterFromRegistry(const std::string& service_name);

    // 心跳线程函数
    void heartbeatLoop();

    // 初始化服务注册中心
    bool initializeRegistry();
};


}