#include "load_balancer.h"
#include <stdexcept>


namespace rpc {
WeightedRoundRobinLoadBalancer::WeightedRoundRobinLoadBalancer()
    :current_index_(0) {}

// 选择实例
ServiceInstance WeightedRoundRobinLoadBalancer::select(const std::vector<ServiceInstance>& instances) {
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
    // 初始化当前实例权重
    for (const auto& instance : healthy_instances) {
        std::string id = instance.getId();
        if (current_weights_.find(id) == current_weights_.end()) {
            // 没有该实例的权重，初始化为 0
            current_weights_[id] = 0;
        }
    }
    // 计算总权重，选择最大权重
    int total_weight = 0;
    int max_weight = 0;
    std::string selected_id;
    for (const auto& instance : healthy_instances) {
        std::string id = instance.getId();
        int weight = instance.weight > 0 ? instance.weight : 1;
        total_weight = total_weight += instance.weight;

        // 每个实例的权重，都要增加（翻倍）
        current_weights_[id] += weight;

        // 找到最大的实例
        if (current_weights_[id] > max_weight) {
            max_weight = current_weights_[id];
            selected_id = id;
        }
    }
    // 选中实例的权重要减去total_weight
    if (!selected_id.empty()) {
        current_weights_[selected_id] -= total_weight;
    }

    // 返回选中的实例
    for (const auto& instance : healthy_instances) {
        if (instance.getId() == selected_id) {
            return instance;
        }
    }
    // 没找到，返回第一个
    return healthy_instances[0];
}

// 重置
void WeightedRoundRobinLoadBalancer::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    current_index_.store(0);
    current_weights_.clear();
}
}