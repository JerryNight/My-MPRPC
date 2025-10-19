#include "load_balancer.h"
#include <stdexcept>
#include <limits>

namespace rpc {

LeastConnectionLoadBalancer::LeastConnectionLoadBalancer() {}

// 选择实例
ServiceInstance LeastConnectionLoadBalancer::select(const std::vector<ServiceInstance>& instances) {
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
    // 初始化连接计数
    for (const auto& instance : healthy_instances) {
        std::string id = instance.getId();
        if (connection_counts_.find(id) == connection_counts_.end()) {
            connection_counts_[id].store(0);
        }
    }
    // 找到连接数最少的实例
    int min_connections = std::numeric_limits<int>::max();
    ServiceInstance selected_instance = healthy_instances[0];

    for (const auto& instance : healthy_instances) {
        std::string id = instance.getId();
        int connections = connection_counts_[id].load();
        if (connections < min_connections) {
            min_connections = connections;
            selected_instance = instance;
        }
    }

    return selected_instance;
}

// 更新状态
void LeastConnectionLoadBalancer::updateStats(const std::string& instance_id, bool connection_start) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (connection_counts_.find(instance_id) == connection_counts_.end()) {
        connection_counts_[instance_id].store(0);
    }
    if (connection_start) {
        // 连接开始，计数加1
        connection_counts_[instance_id].fetch_add(1);
    } else {
        // 连接结束，计数减1
        int current = connection_counts_[instance_id].load();
        if (current > 0) {
            connection_counts_[instance_id].fetch_sub(1);
        }
    }
}

// 重置
void LeastConnectionLoadBalancer::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    connection_counts_.clear();
}



}