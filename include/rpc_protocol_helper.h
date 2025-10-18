#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include "../proto/rpc_protocol.pb.h"
#include <google/protobuf/message.h>

namespace rpc {

// 前向声明(不需要再引入头文件)
struct RpcRequest;
struct RpcResponse;

// 协议序列化辅助类
class RpcProtocolHelper {
public:
    // 将RpcRequest序列化为字节数组
    static std::vector<uint8_t> serializeRequest(const RpcRequest& request);

    // 将字节数组反序列化为RpcRequest
    static RpcRequest parseRequest(const std::vector<uint8_t>& data);

    // 将RpcResponse序列化为字节数组
    static std::vector<uint8_t> serializeResponse(const RpcResponse& response);

    // 将字节数组反序列化为RpcResponse
    static RpcResponse parseResponse(const std::vector<uint8_t>& data);

    // 创建 RpcRequestProto 消息
    static RpcRequestProto createRequestProto(const RpcRequest& request);

    // 从RpcRequestProto中解析/创建RpcRequest
    static RpcRequest fromRequestProto(const RpcRequestProto& proto);

    // 创建 RpcResponseProto 消息
    static RpcResponseProto createResponseProto(const RpcResponse& response);

    // 从RpcResponseProto中解析/创建RpcResponse
    static RpcResponse fromResponseProto(const RpcResponseProto& proto);

    // 验证请求数据有效性
    static bool validateRequestData(const std::vector<uint8_t>& data);

    // 验证响应数据有效性
    static bool validateResponseData(const std::vector<uint8_t>& data);

};


}