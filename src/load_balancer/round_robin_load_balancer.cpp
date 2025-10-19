#include "load_balancer.h"
#include <stdexcept>

namespace rpc {

RoundRobinLoadBalancer::RoundRobinLoadBalancer() : current_index_(0) {}

// 选择一个实例
ServiceInstance RoundRobinLoadBalancer::select(const std::vector<ServiceInstance>& instances) {
    if (instances.empty()) {
        throw std::runtime_error("No available service instances");
    }

    // 获取健康的实例
    std::vector<ServiceInstance> healthy_instances;
    for (const auto& instance : instances) {
        if (instance.is_healthy) {
            healthy_instances.push_back(instance);
        }
    }

    if (healthy_instances.empty()) {
        throw std::runtime_error("No healthy service instances");
    }

    // 轮询选择
    uint64_t index = current_index_.fetch_add(1) % healthy_instances.size();
    return healthy_instances[index];
}

// 重置
void RoundRobinLoadBalancer::reset() {
    current_index_.store(0);
}


}
