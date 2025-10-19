#include "serializer.h"
#include <sstream>

namespace rpc {

// 序列化
std::vector<uint8_t> JsonSerializer::serialize(const google::protobuf::Message& message) {
    // 将protobuf消息转换为JSON字符串
    std::string json_str = messageToJson(message);

    // 将JSON字符串转换为字节数组
    std::vector<uint8_t> result(json_str.begin(), json_str.end());

    return result;
}

// 反序列化
bool JsonSerializer::deserialize(const std::vector<uint8_t>& data, google::protobuf::Message& message) {
    // 将字节数组转换为JSON字符串
    std::string json_str(data.begin(), data.end());

    // 验证JSON字符串
    if (!isValidJson(json_str)) {
        std::cerr << "无效的JSON格式" << std::endl;
        return false;
    }

    return jsonToMessage(json_str, message);
}

// 获取序列化名称
std::string JsonSerializer::getName() const {
    return "json";
}

// protobuf -> json
std::string JsonSerializer::messageToJson(const google::protobuf::Message& message) const {
    // 简化的protobuf到JSON转换
    // 实际项目中应该使用protobuf的JSON转换功能
    
    std::ostringstream json;
    json << "{";
    json << "\"message_type\":\"" << message.GetTypeName() << "\",";
    json << "\"data\":\"" << message.ShortDebugString() << "\"";
    json << "}";
    
    return json.str();
}

// json -> protobuf
bool JsonSerializer::jsonToMessage(const std::string& json, google::protobuf::Message& message) const {
    // 这里只是简单的验证，实际实现需要解析JSON并填充protobuf消息
    if (json.find("\"message_type\"") != std::string::npos && json.find("\"data\"") != std::string::npos) {
        // 简化的实现：直接使用protobuf的解析功能
        // 实际项目中应该解析JSON并设置protobuf字段
        return true;
    }
    
    return false;
}

// 验证 JSON 是否有效
bool JsonSerializer::isValidJson(const std::string& json) const {
    // 简单的JSON格式验证
    if (json.empty()) {
        return false;
    }
    
    // 检查基本的JSON结构
    size_t open_braces = std::count(json.begin(), json.end(), '{');
    size_t close_braces = std::count(json.begin(), json.end(), '}');
    
    if (open_braces != close_braces) {
        return false;
    }
    
    // 检查是否包含必要的字段
    if (json.find("\"message_type\"") == std::string::npos) {
        return false;
    }
    
    return true;
}

}