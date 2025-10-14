#pragma once

#include <memory>
#include <string>        // 字符串头文件
#include <vector>        // 向量容器头文件
#include <unordered_map> // 无序映射容器头文件
#include <functional>    // 函数对象头文件
#include <google/protobuf/message.h>  // protobuf消息基类头文件
#include <google/protobuf/service.h>  // protobuf服务基类头文件

namespace rpc {

// 前向声明
class TcpConnection;

// rpc 请求结构
struct RpcRequest {
    uint64_t request_id; // 请求ID
    std::string service_name; // 服务名称
    std::string method_name; // 方法名称
    std::vector<uint8_t> request_data; // 请求数据(序列化之后的数据)
    std::shared_ptr<TcpConnection> connection; // 客户端连接

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

// 服务分发器
class ServiceDispatcher {
public:
    ServiceDispatcher() = default;
    ~ServiceDispatcher() = default;

    // 注册 protobuf 服务
    bool registerService(google::protobuf::Service* service);

    // 分发 RPC 请求 -- 处理请求
    bool dispatcher(const RpcRequest& request, RpcResponse& response);

    // 注销 protobuf 服务
    bool unregisterService(const std::string& service_name);

    // 获取所有服务
    std::vector<std::string> getRegisteredServices() const;

    // 查询服务
    bool isServiceRegistered(const std::string& service_name) const;
private:
    // 服务映射表 服务名->服务实例
    std::unordered_map<std::string, google::protobuf::Service*> services_;

    // 生成错误响应
    void generateErrorResponse(uint64_t request_id, const std::string& error_message, RpcResponse& response);
};

}