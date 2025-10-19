#pragma once

#include <string>
#include <atomic>
#include <mutex>
#include <memory>
#include <google/protobuf/service.h>
#include <google/protobuf/message.h>
#include "tcp_client.h"
#include "rpc_protocol_helper.h"
#include "transport.h"
#include "registry_factory.h"
#include "load_balancer.h"


namespace rpc {

// RPC 客户端stub基类
class RpcClientStub {
public:
    virtual ~RpcClientStub() = default;

    // 调用RPC方法
    virtual bool callMethod(const std::string& method_name,
                            const google::protobuf::Message& request,
                            google::protobuf::Message& response) = 0;

};

// RPC 客户端stub实现类
class RpcClientStubImpl : public RpcClientStub{
public:
    // 直连模式，不使用服务发现
    RpcClientStubImpl(const std::string& service_name, const std::string& host, uint16_t port);

    // 服务发现模式：使用注册中心和负载均衡器
    RpcClientStubImpl(const std::string& service_name, std::unique_ptr<ServiceRegistry> registry, std::unique_ptr<LoadBalancer> load_balancer = nullptr);

    ~RpcClientStubImpl() override;

    // 调用RPC方法
    bool callMethod(const std::string& method_name,
                            const google::protobuf::Message& request,
                            google::protobuf::Message& response) override;
    
    // 连接服务器
    bool connect();

    // 断开连接
    void disconnect();

    // 检查连接状态
    bool isConnected() const;

    // 设置负载均衡器
    void setLoadBalancer(std::unique_ptr<LoadBalancer> load_balancer);

    // 获取负载均衡器
    LoadBalancer* getLoadBalancer() const;
private:
    std::string service_name_; // 服务名
    std::string host_;  // 服务器地址
    uint16_t port_;  // 服务器端口
    std::shared_ptr<TcpClient> tcp_client_; // tcp客户端
    std::atomic<bool> connected_;  // 连接状态
    std::mutex mutex_;  // 互斥锁
    std::unique_ptr<FrameCodec> frame_codec_;
    // 服务发现相关
    bool use_service_discovery_; // 是否使用服务发现
    std::unique_ptr<ServiceRegistry> registry_; // 服务注册中心
    std::unique_ptr<LoadBalancer> load_balancer_; // 负载均衡器
    std::string current_instance_id_; // 当前实例ID

    // 发送 RPC请求
    RpcResponse sendRpcRequest(const RpcRequest& request);

    // 从注册中心发现服务，选择实例
    ServiceInstance selectServiceInstance();

    // 连接到指定的服务实例
    bool connectToInstance(const ServiceInstance& instance);

};


}