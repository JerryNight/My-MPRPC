#include "serializer.h"
#include <sstream>

namespace rpc {

// 序列化
std::vector<uint8_t> ProtobufSerializer::serialize(const google::protobuf::Message &message) {
    if (!isValidMessage(message)) {  // 如果消息无效
        std::cerr << "Invalid protobuf message for serialization" << std::endl;  // 输出错误信息
        return {};
    }

    std::string serialized_data;
    if (!message.SerializeToString(&serialized_data)) { // 如果序列化失败
        std::cerr << "Failed to serialize protobuf message" << std::endl; 
        return {};
    }

    // 将字符串转换为字节向量
    std::vector<uint8_t> result(serialized_data.begin(), serialized_data.end());
    return result;
}

// 反序列化
bool ProtobufSerializer::deserialize(const std::vector<uint8_t> &data, google::protobuf::Message &message) {
    if (data.empty()) {
        std::cerr << "Empty data for protobuf deserialization" << std::endl; 
        return false;
    }

    // 将字节向量转换为字符串
    std::string serialized_data(data.begin(), data.end()); 
    if (!message.ParseFromString(serialized_data)) {  // 反序列化失败
        std::cerr << "Failed to deserialize protobuf message" << std::endl;
        return false;
    }
    return true;
}

// 获取序列化名称
std::string ProtobufSerializer::getName() const {
    return "protobuf";
}

// 验证消息是否有效
bool ProtobufSerializer::isValidMessage(const google::protobuf::Message& message) const {
    if (!message.IsInitialized()) {  // 如果消息未初始化
        std::cerr << "Protobuf message is not initialized" << std::endl;
        return false;
    }

    if (!message.GetDescriptor()) {  // 如果消息没有描述符
        std::cerr << "Protobuf message has no descriptor" << std::endl;
        return false;
    }

    return true;
}

}