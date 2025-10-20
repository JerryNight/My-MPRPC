#include "../../include/registry.h"
#include "../../include/registry_factory.h"

using namespace rpc;

void testZooKeeperRegistry() {
    std::cout << "=== Testing ZooKeeper Registry ===" << std::endl;
    
    try {
        auto registry = RegistryFactory::createZooKeeperRegistry("localhost:2181");
        if (!registry) {
            std::cerr << "Failed to create ZooKeeper registry" << std::endl;
            return;
        }

        // 等待连接建立
        std::cout << "Waiting for ZooKeeper connection..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // 测试服务注册
        ServiceInstance instance1("zk_test_service", "127.0.0.1", 8080, 1);
        ServiceInstance instance2("zk_test_service", "127.0.0.1", 8081, 2);
        
        if (registry->registerService(instance1)) {
            std::cout << "✓ ZooKeeper service instance 1 registered successfully" << std::endl;
        } else {
            std::cerr << "✗ Failed to register ZooKeeper service instance 1" << std::endl;
        }

        if (registry->registerService(instance2)) {
            std::cout << "✓ ZooKeeper service instance 2 registered successfully" << std::endl;
        } else {
            std::cerr << "✗ Failed to register ZooKeeper service instance 2" << std::endl;
        }

        // 测试服务发现
        auto instances = registry->discoverService("zk_test_service");
        std::cout << "✓ Found " << instances.size() << " instances of zk_test_service" << std::endl;
        
        for (const auto& instance : instances) {
            std::cout << "  - " << instance.getId() << " (weight: " << instance.weight << ")" << std::endl;
        }

        // 测试服务注销
        if (registry->unregisterService("zk_test_service", instance1.getId())) {
            std::cout << "✓ ZooKeeper service instance 1 unregistered successfully" << std::endl;
        } else {
            std::cerr << "✗ Failed to unregister ZooKeeper service instance 1" << std::endl;
        }

        std::cout << "ZooKeeper Registry test completed" << std::endl << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "ZooKeeper Registry test failed: " << e.what() << std::endl;
        std::cout << "Note: Make sure ZooKeeper is running on localhost:2181" << std::endl << std::endl;
    }
}

int main() {
    testZooKeeperRegistry();
}