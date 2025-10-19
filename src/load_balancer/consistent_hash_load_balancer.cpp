#include "load_balancer.h"
#include <stdexcept>


namespace rpc {

ConsistentHashLoadBalancer::ConsistentHashLoadBalancer(int virtual_nodes) 
    : virtual_nodes_(virtual_nodes), last_key_("") {}

// 哈希函数
uint32_t ConsistentHashLoadBalancer::hash(const std::string& key) {
    uint32_t hash = 2166136261u;
    for (char c : key) {
        hash ^= static_cast<uint32_t>(c);
        hash *= 16777619u;
    }
    return hash;
}
    
// 重建哈希环
void ConsistentHashLoadBalancer::rebuildHashRing(const std::vector<ServiceInstance>& instances) {
    hash_ring_.clear();
    for (const auto& instance : instances) {
        if (!instance.is_healthy) {
            continue;
        }
        std::string instance_id = instance.getId();
        // 为每个实例创建虚拟节点
        for (int i = 0; i < virtual_nodes_; ++i) {
            std::string virtual_key = instance_id + "#" + std::to_string(i);
            uint32_t hash_value = hash(virtual_key);
            hash_ring_[hash_value] = instance_id;
        }
    }
}


// 根据提供的key值进行哈希（用户ID，请求ID等）
ServiceInstance ConsistentHashLoadBalancer::select(const std::vector<ServiceInstance>& instances) {
    return selectByKey(instances, last_key_);
}

ServiceInstance ConsistentHashLoadBalancer::selectByKey(const std::vector<ServiceInstance>& instances, const std::string& key) {
    if (instances.empty()) {
        throw std::runtime_error("No available service instances");
    }

    // 过滤健康的实例
    std::vector<ServiceInstance> healthy_instances;
    for (const auto& instance : instances) {
        if (instance.is_healthy) {
            healthy_instances.push_back(instance);
        }
    }

    if (healthy_instances.empty()) {
        throw std::runtime_error("No healthy service instances");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    // 重建哈希环（如果需要）
    if (hash_ring_.empty()) {
        rebuildHashRing(healthy_instances);
    }

    if (hash_ring_.empty()) {
        // 如果还是空的，直接返回第一个健康实例
        return healthy_instances[0];
    }
    // 计算哈希值
    std::string actual_key = key.empty() ? "default" : key;
    last_key_ = actual_key;
    uint32_t hash_value = hash(actual_key);
    // 在hash环上查找
    auto it = hash_ring_.lower_bound(hash_value);
    // 如果没有，选择第一个节点
    if (it == hash_ring_.end()) {
        it = hash_ring_.begin();
    }
    std::string selected_id = it->second;

    // 返回对应实例
    for (const auto& instance : healthy_instances) {
        if (instance.getId() == selected_id) {
            return instance;
        }
    }

    // 如果没有找到匹配的实例（可能实例已经下线），重建哈希环并返回第一个
    rebuildHashRing(healthy_instances);
    return healthy_instances[0];
}
void ConsistentHashLoadBalancer::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    hash_ring_.clear();
    last_key_ = "";
}

}