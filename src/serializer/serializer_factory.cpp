#include "../../include/serializer_factory.h"
#include <memory>

namespace rpc {

// 获取Protobuf序列化器
std::unique_ptr<Serializer> SerializerFactory::createProtobufSerializer() {
    return std::make_unique<ProtobufSerializer>();
}

// 获取JSON序列化器
std::unique_ptr<Serializer> SerializerFactory::createJsonSerializer() {
    return std::make_unique<JsonSerializer>();
}

// 根据名称创建序列化器
std::unique_ptr<Serializer> SerializerFactory::createSerializer(const std::string &name) {
    if (name == "protobuf") {
        return createProtobufSerializer();
    } else if (name == "json") {
        return createJsonSerializer();
    }
    
    return nullptr;
}

// 获取所有序列化器名称
std::vector<std::string> SerializerFactory::getSupportedSerializer() {
    return {"protobuf", "json"};
}

// 检查是否支持指定的序列化器
bool SerializerFactory::isSupported(const std::string &name) {
    auto supported = getSupportedSerializer();
    return std::find(supported.begin(), supported.end(), name) != supported.end();
}

}