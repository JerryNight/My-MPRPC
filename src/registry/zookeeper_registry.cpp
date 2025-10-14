#include "../../include/registry.h"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <sstream>

namespace rpc {

const std::string ZooKeeperRegistry::ROOT_PATH = "/rpc";
const std::string ZooKeeperRegistry::SERVICE_PATH = "/rpc/services";

ZooKeeperRegistry::ZooKeeperRegistry(const std::string& hosts, int session_timeout) 
    :hosts_(hosts),
     session_timeout_(session_timeout),
     zk_handle_(nullptr),
     connected_(false),
     running_(false)
{
    if (!initializeZooKeeper()) {
        std::cerr << "Failed to initialize ZooKeeper connection" << std::endl;
    }
}

ZooKeeperRegistry::~ZooKeeperRegistry() {
    closeZooKeeper();
}

// 注册服务实例
bool ZooKeeperRegistry::registerService(const ServiceInstance& instance) {
    if (!connected_) {
        std::cerr << "ZooKeeper not connected" << std::endl;
        return false;
    }

    if (instance.service_name.empty()) {
        std::cerr << "Service name cannot be empty" << std::endl;
        return false;
    }

    // 创建服务路径
    if (!createServicePath(instance.service_name)) {
        return false;
    }

    // 创建服务实例节点
    std::string node_path = createServiceInstanceNode(instance.service_name, instance);
    if (node_path.empty()) {
        return false;
    }

    std::cout << "Registered service instance: " << instance.service_name 
              << " (" << instance.getId() << ")" << std::endl;
    return true;
}

// 注销服务实例
bool ZooKeeperRegistry::unregisterService(const std::string& service_name, const std::string& instance_id) {
    if (!connected_) {
        std::cerr << "ZooKeeper not connected" << std::endl;
        return false;
    }

    return deleteServiceInstanceNode(service_name, instance_id);
}

// 发现服务实例
std::vector<ServiceInstance> ZooKeeperRegistry::discoverService(const std::string& service_name) {
    if (!connected_) {
        std::cerr << "ZooKeeper not connected" << std::endl;
        return {};
    }

    return getServiceInstances(service_name);
}

// 订阅服务变化
bool ZooKeeperRegistry::subsribeService(const std::string& service_name, ServiceInstanceCallback callback) {
    if (service_name.empty() || !callback) {
        std::cerr << "Invalid service name or callback" << std::endl;
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_[service_name] = std::move(callback);

    // 设置服务路径监听
    std::string service_path = SERVICE_PATH + "/" + service_name;
    int rc = zoo_wexists(zk_handle_, service_path.c_str(), serviceWatcher, this, nullptr);
    if (rc != ZOK) {
        std::cerr << "Failed to set watcher on service path: " << zerror(rc) << std::endl;
        return false;
    }

    std::cout << "Subscribed to service: " << service_name << std::endl;
    return true;
}

// 取消订阅服务变化
bool ZooKeeperRegistry::unsubsribeService(const std::string& service_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = callbacks_.find(service_name);
    if (it != callbacks_.end()) {
        callbacks_.erase(it);
        std::cout << "Unsubscribed from service: " << service_name << std::endl;
        return true;
    }
    return false;
}

// 发送心跳
bool ZooKeeperRegistry::sendHeartbeat(const std::string& service_name, const std::string& instance_id) {
    if (!connected_) {
        return false;
    }
    return true;
}

// 获取所有注册的服务名称
std::vector<std::string> ZooKeeperRegistry::getAllService() {
    if (!connected_) {
        return {};
    }

    std::vector<std::string> services;
    struct String_vector children = {0, nullptr};
    
    int rc = zoo_get_children(zk_handle_, SERVICE_PATH.c_str(), 0, &children);
    if (rc == ZOK) {
        for (int i = 0; i < children.count; ++i) {
            services.push_back(children.data[i]);
        }
        deallocate_String_vector(&children);
    }

    return services;
}

// 检查连接状态
bool ZooKeeperRegistry::isConnected() const {
    return connected_.load();
}

// 等待连接建立
bool ZooKeeperRegistry::waitForConnection(int timeout_ms) {
    std::unique_lock<std::mutex> lock(mutex_);
    return cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                        [this] { return connected_.load(); });
}

// 初始化 zookeeper
bool ZooKeeperRegistry::initializeZooKeeper() {
    // 初始化 zookeeper 客户端
    zk_handle_ = zookeeper_init(hosts_.c_str(), connectionWatcher, session_timeout_,
                                nullptr, this, 0);
    if (!zk_handle_) {
        std::cerr << "Failed to initialize ZooKeeper client" << std::endl;
        return false;
    }

    // 启动监听线程
    running_ = true;
    watcher_thread_ = std::thread(&ZooKeeperRegistry::watcherMain, this);

    // 等待连接建立
    return waitForConnection(5000);
}

// 关闭 zookeeper
void ZooKeeperRegistry::closeZooKeeper() {
    running_ = false;
    connected_ = false;
    // 等待线程结束
    if (watcher_thread_.joinable()) {
        watcher_thread_.join();
    }
    // 关闭zookeeper连接
    if (zk_handle_) {
        zookeeper_close(zk_handle_);
        zk_handle_ = nullptr;
    }
}

// zookeeper 连接回调 watcher
void ZooKeeperRegistry::connectionWatcher(zhandle_t* zh, int type, int state, const char* path, void* watcherCtx) {
    auto registry = static_cast<ZooKeeperRegistry*>(watcherCtx);
    if (!registry) return;

    std::lock_guard<std::mutex> lock(registry->mutex_);

    if (state == ZOO_CONNECTED_STATE) {
        registry->connected_ = true;
        registry->cv_.notify_all(); // 连接成功，唤醒所有阻塞任务
        std::cout << "ZooKeeper connected successfully" << std::endl;
    } else if (state == ZOO_EXPIRED_SESSION_STATE) {
        registry->connected_ = false;
        std::cerr << "ZooKeeper session expired" << std::endl;
    } else if (state == ZOO_AUTH_FAILED_STATE) {
        registry->connected_ = false;
        std::cerr << "ZooKeeper authentication failed" << std::endl;
    }

}

// 服务实例变化 监听回调
void ZooKeeperRegistry::serviceWatcher(zhandle_t* zh, int type, int state, const char* path, void* watcherCtx) {
    auto* registry = static_cast<ZooKeeperRegistry*>(watcherCtx);
    if (!registry) return;

    if (type == ZOO_CHILD_EVENT) {
        // 服务实例列表发生变化
        std::string service_name = std::string(path).substr(SERVICE_PATH.length() + 1);
        registry->updateServiceCache(service_name);
        registry->notifyServiceChange(service_name);
    }
}

// 监听线程-主函数
void ZooKeeperRegistry::watcherMain() {
    while (running_) {
        if (connected_) {
            // 处理zookeeper事件
            int rc = zoo_wexists(zk_handle_, ROOT_PATH.c_str(), serviceWatcher, this, nullptr);
            if (rc != ZOK) {
                std::cerr << "Failed to set watcher on root path: " << zerror(rc) << std::endl;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// 创建服务路径
bool ZooKeeperRegistry::createServicePath(const std::string& service_name) {
    // 创建根路径
    int rc = zoo_create(zk_handle_, ROOT_PATH.c_str(), nullptr,0,&ZOO_OPEN_ACL_UNSAFE,0,nullptr,0);
    if (rc != ZOK && rc != ZNODEEXISTS) {
        std::cerr << "Failed to create root path: " << zerror(rc) << std::endl;
        return false;
    }

    // 创建服务路径
    std::string services_path = SERVICE_PATH;
    rc = zoo_create(zk_handle_, services_path.c_str(), nullptr, 0,
                   &ZOO_OPEN_ACL_UNSAFE, 0, nullptr, 0);
    if (rc != ZOK && rc != ZNODEEXISTS) {
        std::cerr << "Failed to create services path: " << zerror(rc) << std::endl;
        return false;
    }

    // 创建具体服务路径
    std::string service_path = services_path + "/" + service_name;
    rc = zoo_create(zk_handle_, service_path.c_str(), nullptr, 0,
                   &ZOO_OPEN_ACL_UNSAFE, 0, nullptr, 0);
    if (rc != ZOK && rc != ZNODEEXISTS) {
        std::cerr << "Failed to create service path: " << zerror(rc) << std::endl;
        return false;
    }

    return true;
}

// 创建服务实例节点
std::string ZooKeeperRegistry::createServiceInstanceNode(const std::string& service_name, const ServiceInstance& instance) {
    std::string service_path = SERVICE_PATH + "/" + service_name;
    std::string instance_path = service_path + "/" + instance.getId();
    // 序列化服务实例
    auto data = serializeInstance(instance);
    // 创建临时顺序节点
    char path_buffer[1024];
    int path_len = sizeof(path_buffer);
    int rc = zoo_create(zk_handle_, instance_path.c_str(), 
                       reinterpret_cast<const char*>(data.data()), data.size(),
                       &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL | ZOO_SEQUENCE,
                       path_buffer, path_len);
    
    if (rc != ZOK) {
        std::cerr << "Failed to create service instance node: " << zerror(rc) << std::endl;
        return "";
    }

    return std::string(path_buffer);
}

// 删除服务实例节点
bool ZooKeeperRegistry::deleteServiceInstanceNode(const std::string& service_name, const std::string& instance_id) {
    std::string service_path = SERVICE_PATH + "/" + service_name;
    // 获取所有子节点
    struct String_vector children = {0, nullptr};
    int rc = zoo_get_children(zk_handle_, service_path.c_str(), 0, &children);
    if (rc != ZOK) {
        std::cerr << "Failed to get children: " << zerror(rc) << std::endl;
        return false;
    }
    // 查找匹配的实例节点
    for (int i = 0; i < children.count; ++i) {
        std::string child_path = service_path + "/" + children.data[i];
        
        // 获取节点数据
        char buffer[1024];
        int buffer_len = sizeof(buffer);
        struct Stat stat;
        rc = zoo_get(zk_handle_, child_path.c_str(), 0, buffer, &buffer_len, &stat);
        if (rc == ZOK) {
            // 反序列化数据
            std::vector<uint8_t> data(buffer, buffer + buffer_len);
            ServiceInstance instance = deserializeInstance(data);
            
            if (instance.getId() == instance_id) {
                // 删除节点
                rc = zoo_delete(zk_handle_, child_path.c_str(), -1);
                deallocate_String_vector(&children);
                return rc == ZOK;
            }
        }
    }

    deallocate_String_vector(&children);
    return false;
}

// 获取服务实例
std::vector<ServiceInstance> ZooKeeperRegistry::getServiceInstances(const std::string& service_name) {
    std::string service_path = SERVICE_PATH + "/" + service_name;
    std::vector<ServiceInstance> instances;

    // 获取该服务下的所有子节点（实例znode）
    struct String_vector children = {0, nullptr};
    int rc = zoo_get_children(zk_handle_, service_path.c_str(), 0, &children);
    if (rc != ZOK) {
        std::cerr << "Failed to get children: " << zerror(rc) << std::endl;
        return instances;
    }

    for (int i = 0; i < children.count; ++i) {
        std::string child_path = service_path + "/" + children.data[i];
        
        // 获取节点数据
        char buffer[1024];
        int buffer_len = sizeof(buffer);
        struct Stat stat;
        rc = zoo_get(zk_handle_, child_path.c_str(), 0, buffer, &buffer_len, &stat);
        if (rc == ZOK) {
            // 反序列化数据
            std::vector<uint8_t> data(buffer, buffer + buffer_len);
            ServiceInstance instance = deserializeInstance(data);
            instances.push_back(instance);
        }
    }

    deallocate_String_vector(&children);
    return instances;
}

// 序列化服务实例：struct ServiceInstance -> vector<uint8_t>
std::vector<uint8_t> ZooKeeperRegistry::serializeInstance(const ServiceInstance& instance) {
    // 简单的序列化实现，将服务实例信息编码为字节数组
    std::string data;
    data += instance.service_name + "\n";
    data += instance.host + "\n";
    data += std::to_string(instance.port) + "\n";
    data += std::to_string(instance.weight) + "\n";
    data += (instance.is_healthy ? "1" : "0") + std::string("\n");
    data += std::to_string(instance.last_heartbeat) + "\n";
    
    // 序列化元数据
    for (const auto& pair : instance.metadata) {
        data += pair.first + "=" + pair.second + "\n";
    }

    return std::vector<uint8_t>(data.begin(), data.end());
}

// 反序列化服务实例：vector<uint8_t> -> struct ServiceInstance
ServiceInstance ZooKeeperRegistry::deserializeInstance(const std::vector<uint8_t>& data) {
    ServiceInstance instance;
    
    std::string data_str(data.begin(), data.end());
    std::istringstream iss(data_str);
    std::string line;
    
    if (std::getline(iss, line)) instance.service_name = line;
    if (std::getline(iss, line)) instance.host = line;
    if (std::getline(iss, line)) instance.port = std::stoi(line);
    if (std::getline(iss, line)) instance.weight = std::stoi(line);
    if (std::getline(iss, line)) instance.is_healthy = (line == "1");
    if (std::getline(iss, line)) instance.last_heartbeat = std::stoull(line);
    
    // 反序列化元数据
    while (std::getline(iss, line)) {
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            instance.metadata[key] = value;
        }
    }

    return instance;
}

// 通知服务变化
void ZooKeeperRegistry::notifyServiceChange(const std::string& service_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = callbacks_.find(service_name);
    if (it != callbacks_.end() && it->second) {
        auto instances = getServiceInstances(service_name);
        it->second(service_name, instances);
    }
}

// 更新服务缓存
void ZooKeeperRegistry::updateServiceCache(const std::string& service_name) {
    auto instances = getServiceInstances(service_name);
    
    std::lock_guard<std::mutex> lock(mutex_);
    service_cache_[service_name].clear();
    for (const auto& instance : instances) {
        service_cache_[service_name][instance.getId()] = instance;
    }
}

}