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


namespace rpc {

// RPC 客户端stub基类
class RpcClientStub {
public:
    virtual ~RpcClientStub();

    // 调用RPC方法
    virtual bool callMethod(const std::string& method_name,
                            const google::protobuf::Message& request,
                            google::protobuf::Message& response) = 0;

};

// RPC 客户端stub实现类
class RpcClientStubImpl : public RpcClientStub{
public:
    RpcClientStubImpl(const std::string& service_name, const std::string& host, uint16_t port);
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
private:
    std::string service_name_; // 服务名
    std::string host_;  // 服务器地址
    uint16_t port_;  // 服务器端口
    std::shared_ptr<TcpClient> tcp_client_; // tcp客户端
    std::atomic<bool> connected_;  // 连接状态
    std::mutex mutex_;  // 互斥锁
    std::unique_ptr<FrameCodec> frame_codec_;

    // 发送 RPC请求
    RpcResponse sendRpcRequest(const RpcRequest& request);

};


}