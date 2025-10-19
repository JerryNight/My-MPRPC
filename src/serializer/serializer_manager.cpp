#include "serializer_manager.h"
#include "serializer_factory.h"

namespace rpc {

SerializerManager::SerializerManager(){

}

// 注册序列化器
bool SerializerManager::registerSerializer(const std::string& name, std::unique_ptr<Serializer> serializer){
    if (!serializer) {
        std::cerr << "Cannot register null serializer" << std::endl;
        return false;
    }

    if (name.empty()) {
        std::cerr << "Serializer name cannot be empty" << std::endl;
        return false;
    }

    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = serializers_.find(name);
    if (it != serializers_.end()) {
        std::cerr << "Serializer '" << name << "' is already registered" << std::endl;
        return false; 
    }

    // 注册序列化器
    serializers_[name] = std::move(serializer);
    std::cout << "Registered serializer: " << name << std::endl;
    return true;
}

// 获取序列化器
std::shared_ptr<Serializer> SerializerManager::getSerializer(const std::string& name){
    std::shared_lock<std::shared_mutex> lock(mutex_);  // 获取读锁

    auto it = serializers_.find(name);
    if (it != serializers_.end()) {
        return it->second;
    }

    std::cerr << "Serializer '" << name << "' not found" << std::endl;
    return nullptr; 
}

// 移除序列化器
bool SerializerManager::removeSerializer(const std::string& name){
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = serializers_.find(name);  // 查找序列化器
    if (it != serializers_.end()) { 
        serializers_.erase(it); 
        std::cout << "Removed serializer: " << name << std::endl;
        return true;
    }

    std::cerr << "Serializer '" << name << "' not found for removal" << std::endl;
    return false; 
}

// 获取所有序列化器
std::vector<std::string> SerializerManager::getRegisteredSerializers() {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::vector<std::string> names;   
    for (const auto& pair : serializers_) {  // 遍历序列化器映射表
        names.push_back(pair.first);  // 添加序列化器名称
    }
    
    return names;  // 返回序列化器名称列表
}

// 清空所有序列化器
void SerializerManager::clear(){
    std::unique_lock<std::shared_mutex> lock(mutex_);
    serializers_.clear();
    std::cout << "已清空所有序列化器" << std::endl;
}

// 初始化默认序列化器
void SerializerManager::initializeDefaultSerializers(){
    std::cout << "初始化默认序列化器..." << std::endl;
    auto protobuf_serializer = SerializerFactory::createProtobufSerializer();
    if (protobuf_serializer) {
        registerSerializer("protobuf", std::move(protobuf_serializer));
    }

    auto json_serializer = SerializerFactory::createJsonSerializer();
    if (json_serializer) {
        registerSerializer("json", std::move(json_serializer));
    }

    std::cout << "默认序列化器初始化完成" << std::endl;
}

// 判断是否注册
bool SerializerManager::isRegistered(const std::string& name){
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return serializers_.find(name) != serializers_.end();
}

// 获取序列化器数量
size_t SerializerManager::getSerializerCount(){
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return serializers_.size();
}

}