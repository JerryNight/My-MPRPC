#pragma once

#include "serializer.h"
#include <shared_mutex>

namespace rpc {

class SerializerManager {
public:
    SerializerManager();
    ~SerializerManager() = default;

    // 注册序列化器
    bool registerSerializer(const std::string& name, std::unique_ptr<Serializer> serializer);

    // 获取序列化器
    std::shared_ptr<Serializer> getSerializer(const std::string& name);

    // 移除序列化器
    bool removeSerializer(const std::string& name);

    // 获取所有序列化器
    std::vector<std::string> getRegisteredSerializers() ;

    // 清空所有序列化器
    void clear();

    // 初始化默认序列化器
    void initializeDefaultSerializers();

    // 判断是否注册
    bool isRegistered(const std::string& name);

    // 获取序列化器数量
    size_t getSerializerCount();
private:
    std::unordered_map<std::string, std::shared_ptr<Serializer>> serializers_; // 保存所有序列化器 - 映射表
    std::shared_mutex mutex_; // 读写锁
};

}