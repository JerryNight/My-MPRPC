#include "../../include/serializer.h"
#include "../../include/serializer_factory.h"
#include "../../include/serializer_manager.h"
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <cassert>


using namespace rpc;

void testSrializerFactory() {
    auto protobuf_serialize = SerializerFactory::createProtobufSerializer();
    auto json_serialize = SerializerFactory::createJsonSerializer();
    assert(protobuf_serialize);
    assert(json_serialize);

    // 测试根据名称创建序列化器
    auto serializer_by_name = SerializerFactory::createSerializer("protobuf");
    assert(serializer_by_name != nullptr);
    std::cout << "✓ 根据名称创建序列化器成功" << std::endl;
    
    // 测试无效名称
    auto invalid_serializer = SerializerFactory::createSerializer("invalid");
    assert(invalid_serializer == nullptr);
    std::cout << "✓ 无效名称处理正确" << std::endl;
    
}

void testSerializerManager() {
    SerializerManager manager;
    manager.initializeDefaultSerializers();
    std::vector<std::string> serializers = manager.getRegisteredSerializers();
    assert(!serializers.empty());
    std::cout << "✓ 已注册的序列化器: ";
    for (const auto& name : serializers) {
        std::cout << name << " ";
    }
    std::cout << std::endl;

    // 测试获取序列化器
    auto protobuf_serializer = manager.getSerializer("protobuf");
    assert(protobuf_serializer != nullptr);
    std::cout << "✓ 成功获取protobuf序列化器" << std::endl;
    
    // 测试获取不存在的序列化器
    auto non_existent = manager.getSerializer("non_existent");
    assert(non_existent == nullptr);
    std::cout << "✓ 不存在的序列化器处理正确" << std::endl;
    
    // 测试注册新序列化器
    auto new_serializer = SerializerFactory::createProtobufSerializer();
    bool registered = manager.registerSerializer("test_serializer", std::move(new_serializer));
    assert(registered);
    std::cout << "✓ 成功注册新序列化器" << std::endl;
    
    // 测试获取新注册的序列化器
    auto test_serializer = manager.getSerializer("test_serializer");
    assert(test_serializer != nullptr);
    std::cout << "✓ 成功获取新注册的序列化器" << std::endl;
    
    // 测试移除序列化器
    bool removed = manager.removeSerializer("test_serializer");
    assert(removed);
    std::cout << "✓ 成功移除序列化器" << std::endl;
    
    // 测试移除不存在的序列化器
    bool not_removed = manager.removeSerializer("non_existent");
    assert(!not_removed);
    std::cout << "✓ 移除不存在的序列化器处理正确" << std::endl;
}

int main() {
    testSrializerFactory();
    testSerializerManager();
}