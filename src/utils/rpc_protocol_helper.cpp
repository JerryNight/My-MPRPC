#include "../../include/rpc_protocol_helper.h"
#include "../../include/transport.h"
#include <iostream>
#include <stdexcept>

namespace rpc {

// 将RpcRequest序列化为字节数组
std::vector<uint8_t> RpcProtocolHelper::serializeRequest(const RpcRequest& request) {
    // 创建RpcRequestProto消息，序列化为字符串
    RpcRequestProto proto = createRequestProto(request);

    // 序列化
    std::string serialized;
    if (!proto.SerializeToString(&serialized)) {
        throw std::runtime_error("Failed to serialize RpcRequestProto");
    }

    // 转为字节数组
    return std::vector<uint8_t>(serialized.begin(), serialized.end());
}

// 将字节数组反序列化为RpcRequest
RpcRequest RpcProtocolHelper::parseRequest(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        throw std::runtime_error("Empty data for request parsing");
    }

    // 反序列化
    RpcRequestProto proto;
    if (!proto.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
        throw std::runtime_error("Failed to parse RpcRequestProto");
    }

    // 转换为 RpcRequest
    return fromRequestProto(proto);
}

// 将RpcResponse序列化为字节数组
std::vector<uint8_t> RpcProtocolHelper::serializeResponse(const RpcResponse& response) {
    // 创建RpcResponseProto消息，序列化为字符串
    RpcResponseProto proto = createResponseProto(response);

    // 序列化
    std::string serialized;
    if (!proto.SerializeToString(&serialized)) {
        throw std::runtime_error("Failed to serialize RpcResponseProto");
    }

    // 转为字节数组
    return std::vector<uint8_t>(serialized.begin(), serialized.end());
}

// 将字节数组反序列化为RpcResponse
RpcResponse RpcProtocolHelper::parseResponse(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        throw std::runtime_error("Empty data for response parsing");
    }
    
    // 反序列化Protobuf消息
    RpcResponseProto proto;
    if (!proto.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
        throw std::runtime_error("Failed to parse RpcResponseProto");
    }
    
    // 转换为RpcResponse
    return fromResponseProto(proto);
}

// 创建 RpcRequestProto 消息
RpcRequestProto RpcProtocolHelper::createRequestProto(const RpcRequest& request) {
    RpcRequestProto proto;

    proto.set_request_id(request.request_id);
    proto.set_service_name(request.service_name);
    proto.set_method_name(request.method_name);

    proto.set_request_data(request.request_data.data(), request.request_data.size());

    return proto;
}

// 从RpcRequestProto中解析/创建RpcRequest
RpcRequest RpcProtocolHelper::fromRequestProto(const RpcRequestProto& proto) {
    RpcRequest request;

    request.request_id = proto.request_id();
    request.service_name = proto.service_name();
    request.method_name = proto.method_name();

    const std::string& data = proto.request_data();
    request.request_data = std::vector<uint8_t>(data.begin(), data.end());

    return request;
}

// 创建 RpcResponseProto 消息
RpcResponseProto RpcProtocolHelper::createResponseProto(const RpcResponse& response) {
    RpcResponseProto proto;

    proto.set_request_id(response.request_id);
    proto.set_success(response.success);
    proto.set_response_data(response.response_data.begin(), response.response_data.end());

    // 设置错误细腻
    if (!response.success) {
        proto.set_error_code(static_cast<int32_t>(RpcErrorCode::SERVER_ERROR));
        proto.set_error_message(response.error_message);
    } else {
        proto.set_error_code(static_cast<int32_t>(RpcErrorCode::SUCCESS));
    }

    return proto;
}

// 从RpcResponseProto中解析/创建RpcResponse
RpcResponse RpcProtocolHelper::fromResponseProto(const RpcResponseProto& proto) {
    RpcResponse response;
    
    response.request_id = proto.request_id();
    response.success = proto.success();
    response.error_message = proto.error_message();
    
    // 获取响应数据
    const std::string& data = proto.response_data();
    response.response_data = std::vector<uint8_t>(data.begin(), data.end());
    
    return response;
}

// 验证请求数据有效性
bool RpcProtocolHelper::validateRequestData(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return false;
    }

    RpcRequestProto proto;
    if (!proto.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
        return false;
    }

    // 验证必需字段
    if (proto.service_name().empty() || proto.method_name().empty()) {
        return false;
    }

    return true;
}

// 验证响应数据有效性
bool RpcProtocolHelper::validateResponseData(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return false;
    }
    
    RpcResponseProto proto;
    if (!proto.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
        return false;
    }
    
    return true;
}




}