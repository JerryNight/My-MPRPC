#include "load_balancer.h"
#include <stdexcept>
#include <iostream>

namespace rpc {
LoadBalancerFactory::LoadBalancerFactory() {
    initializeBuiltInLoadBalancers();
}
LoadBalancerFactory& LoadBalancerFactory::getInstance() {
    static LoadBalancerFactory instance;
    return instance;
}   

// 注册负载均衡创建函数
bool LoadBalancerFactory::registerCreator(const std::string& name, LoadBalancerCreator creator) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    // 检查是否已经注册
    if (creators_.find(name) != creators_.end()) {
        return false;  // 已存在，注册失败
    }
    
    creators_[name] = creator;
    return true;
}

// 创建负载均衡器
std::unique_ptr<LoadBalancer> LoadBalancerFactory::create(const std::string& name, const std::unordered_map<std::string, std::string>& config) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = creators_.find(name);
    if (it != creators_.end()) {
        return it->second(config);
    }
    
    // 未找到，返回默认的轮询负载均衡器
    auto default_it = creators_.find("round_robin");
    if (default_it != creators_.end()) {
        return default_it->second(config);
    }
    
    return nullptr;
}

// 查询所有已注册负载均衡器
std::vector<std::string> LoadBalancerFactory::getRegisteredNames() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::vector<std::string> names;
    names.reserve(creators_.size());
    
    for (const auto& pair : creators_) {
        names.push_back(pair.first);
    }
    
    return names;
}

// 是否支持指定的负载均衡器
bool LoadBalancerFactory::isSupported(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return creators_.find(name) != creators_.end();
}

// 静态方法-创建负载均衡器
std::unique_ptr<LoadBalancer> LoadBalancerFactory::createLoadBalancer(const std::string& name, const std::unordered_map<std::string, std::string>& config) {
    return getInstance().create(name, config);
}
std::vector<std::string> LoadBalancerFactory::getSupportedLoadBalancers() {
    return {
        "round_robin",
        "random",
        "weighted_round_robin",
        "weighted_random",
        "least_connection",
        "consistent_hash"
    };
}


// 初始化内置负载均衡器
void LoadBalancerFactory::initializeBuiltInLoadBalancers() {
    // 注册负载均衡器
    registerCreator("round_robin", [](const std::unordered_map<std::string, std::string>&) {
        return std::make_unique<RoundRobinLoadBalancer>();
    });
    registerCreator("RoundRobin", [](const std::unordered_map<std::string, std::string>&) {
        return std::make_unique<RoundRobinLoadBalancer>();
    });

    // 注册加权轮询负载均衡器
    registerCreator("weighted_round_robin", [](const std::unordered_map<std::string, std::string>&) {
        return std::make_unique<WeightedRoundRobinLoadBalancer>();
    });
    registerCreator("WeightedRoundRobin", [](const std::unordered_map<std::string, std::string>&) {
        return std::make_unique<WeightedRoundRobinLoadBalancer>();
    });

    // 注册最少连接负载均衡器
    registerCreator("least_connection", [](const std::unordered_map<std::string, std::string>&) {
        return std::make_unique<LeastConnectionLoadBalancer>();
    });
    registerCreator("LeastConnection", [](const std::unordered_map<std::string, std::string>&) {
        return std::make_unique<LeastConnectionLoadBalancer>();
    });

    // 注册一致性哈希负载均衡器（支持配置）
    registerCreator("consistent_hash", [](const std::unordered_map<std::string, std::string>& config) {
        int virtual_nodes = 100;  // 默认值
        auto it = config.find("virtual_nodes");
        if (it != config.end()) {
            try {
                virtual_nodes = std::stoi(it->second);
            } catch (...) {
                std::cout << "consistent_hash config error\n";
            }
        }
        return std::make_unique<ConsistentHashLoadBalancer>(virtual_nodes);
    });
    registerCreator("ConsistentHash", [](const std::unordered_map<std::string, std::string>& config) {
        int virtual_nodes = 100;
        auto it = config.find("virtual_nodes");
        if (it != config.end()) {
            try {
                virtual_nodes = std::stoi(it->second);
            } catch (...) {
                std::cout << "consistent_hash config error\n";
            }
        }
        return std::make_unique<ConsistentHashLoadBalancer>(virtual_nodes);
    });

}


}