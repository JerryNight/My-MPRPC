#pragma once

#include "serializer.h"

namespace rpc {

class SerializerFactory {
public:
    // 获取Protobuf序列化器
    static std::unique_ptr<Serializer> createProtobufSerializer();

    // 获取JSON序列化器
    static std::unique_ptr<Serializer> createJsonSerializer();

    // 根据名称创建序列化器
    static std::unique_ptr<Serializer> createSerializer(const std::string& name);

    // 获取所有序列化器名称
    static std::vector<std::string> getSupportedSerializer();

    // 检查是否支持指定的序列化器
    static bool isSupported(const std::string& name);
};

}