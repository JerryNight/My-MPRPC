#pragma once

#include "registry.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <random>
#include <functional>

namespace rpc {

// 负载均衡器-抽象基类
class LoadBalancer {
public:
    virtual ~LoadBalancer() = default;

    // 选择一个实例
    virtual ServiceInstance select(const std::vector<ServiceInstance>& instances) = 0;

    // 更新统计信息（用于最小连接数策略）
    virtual void updateStats(const std::string& instance_id, bool connection_start) {}

    // 获取负载均衡器名称
    virtual std::string getName() const = 0;

    // 重置状态
    virtual void reset() {}
};

// 轮询负载均衡器
class RoundRobinLoadBalancer : public LoadBalancer {
public:
    RoundRobinLoadBalancer();
    ~RoundRobinLoadBalancer() override = default;

    // 选择一个实例
    ServiceInstance select(const std::vector<ServiceInstance>& instances) override;

    // 获取名称
    std::string getName() const { return "RoundRobin"; }

    // 重置
    void reset() override;

private:
    std::atomic<uint64_t> current_index_;
};

// 加权轮询负载均衡器
class WeightedRoundRobinLoadBalancer : public LoadBalancer {
public:
    WeightedRoundRobinLoadBalancer();
    ~WeightedRoundRobinLoadBalancer() override = default;

    // 选择实例
    ServiceInstance select(const std::vector<ServiceInstance>& instances) override;

    // 获取名称
    std::string getName() const { return "WeightedRoundRobin"; }

    // 重置
    void reset() override;

private:
    std::atomic<uint64_t> current_index_;
    std::mutex mutex_;
    std::unordered_map<std::string, int> current_weights_;
};

// 最少连接数-负载均衡器
class LeastConnectionLoadBalancer : public LoadBalancer {
public:
    LeastConnectionLoadBalancer();
    ~LeastConnectionLoadBalancer() override = default;

    // 选择实例
    ServiceInstance select(const std::vector<ServiceInstance>& instances) override;
    // 更新状态
    void updateStats(const std::string& instance_id, bool connection_start) override;
    // 获取名称
    std::string getName() const override { return "LeastConnection"; }
    // 重置
    void reset() override;
private:
    std::mutex mutex_;
    std::unordered_map<std::string, std::atomic<int>> connection_counts_;
};

// 一致性哈希-负载均衡器
class ConsistentHashLoadBalancer : public LoadBalancer {
public:
    explicit ConsistentHashLoadBalancer(int virtual_nodes = 100);
    ~ConsistentHashLoadBalancer() override = default;

    // 根据提供的key值进行哈希（用户ID，请求ID等）
    ServiceInstance select(const std::vector<ServiceInstance>& instances) override;
    ServiceInstance selectByKey(const std::vector<ServiceInstance>& instances, const std::string& key);
    std::string getName() const override { return "ConsistentHash"; }
    void reset() override;

private:
    int virtual_nodes_; // 虚拟节点
    std::mutex mutex_;
    std::map<uint32_t, std::string> hash_ring_; // 哈希环
    std::string last_key_; // 默认key

    // 哈希函数
    uint32_t hash(const std::string& key);
    
    // 重建哈希环
    void rebuildHashRing(const std::vector<ServiceInstance>& instances);
};


// 负载均衡器->创建器 定义
using LoadBalancerCreator = std::function<std::unique_ptr<LoadBalancer>(const std::unordered_map<std::string, std::string>&)>;

// 负载均衡器工厂类 -- 使用注册表模式
class LoadBalancerFactory {
public:
    // 获取工厂单例
    static LoadBalancerFactory& getInstance();

    // 注册负载均衡创建函数
    bool registerCreator(const std::string& name, LoadBalancerCreator creator);

    // 创建负载均衡器
    std::unique_ptr<LoadBalancer> create(const std::string& name, const std::unordered_map<std::string, std::string>& config = {});

    // 查询所有已注册负载均衡器
    std::vector<std::string> getRegisteredNames() const;

    // 是否支持指定的负载均衡器
    bool isSupported(const std::string& name) const;

    // 静态方法-创建负载均衡器
    static std::unique_ptr<LoadBalancer> createLoadBalancer(const std::string& name, const std::unordered_map<std::string, std::string>& config = {});
    static std::vector<std::string> getSupportedLoadBalancers();

private:
    LoadBalancerFactory();
    ~LoadBalancerFactory() = default;
    // 禁用拷贝
    LoadBalancerFactory(const LoadBalancerFactory&) = delete;
    LoadBalancerFactory& operator=(const LoadBalancerFactory&) = delete;

    // 初始化内置负载均衡器
    void initializeBuiltInLoadBalancers();

    std::unordered_map<std::string, LoadBalancerCreator> creators_; // 注册表
    mutable std::shared_mutex mutex_;
};

// 实现自动注册的辅助类：静态对象 + 构造方法里实现注册
class LoadBalancerRegistrar {
public: 
    LoadBalancerRegistrar(const std::string& name, LoadBalancerCreator creator) {
        // 程序执行前，会先加载这个构造函数。
        LoadBalancerFactory::getInstance().registerCreator(name, creator);
    }
};

// 注册：在新的cpp中，加上下面这句，就可以完成注册，不用修改工厂类
// static LoadBalancerRegistrar registrar("name", [](){});

// 使用宏，简化注册过程: 在新的cpp下面，只要写：REGISTER_LOAD_BALANCER(random, RandomLoadBalancer);
#define REGISTER_LOAD_BALANCER(name, class_name) \
    namespace { \
        static LoadBalancerRegistrar lb_registrar##class_name( \
            name, \
            [](const std::unordered_map<std::string, std::string>& config)-> std::unique_ptr<LoadBalancer> { \
                return std::make_unique<>(class_name); \
            } \
        ); \
    } 

}