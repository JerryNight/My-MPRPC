#pragma once

#include <memory>
#include <vector>
#include <google/protobuf/message.h>
#include <string>
#include <unordered_map>

namespace rpc {

// 序列化器 - 基类
class Serializer {
public:
    virtual ~Serializer() = default;

    // 序列化
    virtual std::vector<uint8_t> serialize(const google::protobuf::Message& message) = 0;

    // 反序列化
    virtual bool deserialize(const std::vector<uint8_t>& data, google::protobuf::Message& message) = 0;

    // 获取序列化名称
    virtual std::string getName() const = 0;
};

// protobuf 序列化器
class ProtobufSerializer : public Serializer {
public:
    ProtobufSerializer() = default;
    ~ProtobufSerializer() = default;

    // 序列化
    std::vector<uint8_t> serialize(const google::protobuf::Message& message) override;

    // 反序列化
    bool deserialize(const std::vector<uint8_t>& data, google::protobuf::Message& message) override;

    // 获取序列化名称
    std::string getName() const override;
private:
    // 验证消息是否有效
    bool isValidMessage(const google::protobuf::Message& message) const;
};

// json 序列化器
class JsonSerializer : public Serializer {
public:
    JsonSerializer() = default;
    ~JsonSerializer() = default;

    // 序列化
    std::vector<uint8_t> serialize(const google::protobuf::Message& message) override;

    // 反序列化
    bool deserialize(const std::vector<uint8_t>& data, google::protobuf::Message& message) override;

    // 获取序列化名称
    std::string getName() const override;
private:
    // protobuf -> json
    std::string messageToJson(const google::protobuf::Message& message) const;

    // json -> protobuf
    bool jsonToMessage(const std::string& json, google::protobuf::Message& message) const;

    // 验证 JSON 是否有效
    bool isValidJson(const std::string& json) const;

};

}