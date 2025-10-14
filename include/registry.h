#pragma once

#include <zookeeper/zookeeper.h>
#include <google/protobuf/message.h>
#include <memory>        // 智能指针相关头文件
#include <string>        // 字符串头文件
#include <vector>        // 向量容器头文件
#include <functional>    // 函数对象头文件
#include <unordered_map> // 无序映射容器头文件
#include <mutex>         // 互斥锁头文件
#include <shared_mutex>  // 共享互斥锁头文件
#include <thread>        // 线程相关头文件
#include <atomic>        // 原子操作头文件
#include <condition_variable>

namespace rpc {

// 服务实例
struct ServiceInstance {
    std::string service_name; // 服务名称
    std::string host; // 主机地址
    uint16_t port; // 端口号
    int weight;   // 权重
    bool is_healthy;  // 健康状态
    uint64_t last_heartbeat; // 最后心跳时间戳
    std::unordered_map<std::string, std::string> metadata; // 元数据

    // 默认构造
    ServiceInstance(): port(0),weight(1),is_healthy(false),last_heartbeat(0){}
    // 构造
    ServiceInstance(const std::string& name, const std::string& host_addr, uint16_t port_num,int instance_weight = 1)
        :service_name(name),
         host(host_addr),
         port(port_num),
         weight(instance_weight),
         is_healthy(true),
         last_heartbeat(0) {}
    
    // 获取服务实例 ip:port
    std::string getId() const {
        return host + ":" + std::to_string(port);
    }
};

// 实例变化->回调函数
using ServiceInstanceCallback = std::function<void(const std::string& service_name, const std::vector<ServiceInstance>& instance)>;


// 服务注册中心-抽象基类
class ServiceRegistry {
public:
    virtual ~ServiceRegistry() = default;

    // 注册服务实例
    virtual bool registerService(const ServiceInstance& instance) = 0;

    // 注销服务实例
    virtual bool unregisterService(const std::string& service_name, const std::string& instance_id) = 0;

    // 发现服务实例
    virtual std::vector<ServiceInstance> discoverService(const std::string& service_name) = 0;

    // 订阅服务变化
    virtual bool subsribeService(const std::string& service_name, ServiceInstanceCallback callback) = 0;

    // 取消订阅服务变化
    virtual bool unsubsribeService(const std::string& service_name) = 0;

    // 发送心跳
    virtual bool sendHeartbeat(const std::string& service_name, const std::string& instance_id) = 0;

    // 获取所有注册的服务名称
    virtual std::vector<std::string> getAllService() = 0;
};

// zookeeper 服务注册中心
class ZooKeeperRegistry : public ServiceRegistry {
public:
    explicit ZooKeeperRegistry(const std::string& hosts = "localhost:2181", int session_timeout = 30000);
    ~ZooKeeperRegistry() override;

    // 禁用拷贝
    ZooKeeperRegistry(const ZooKeeperRegistry&) = delete;
    ZooKeeperRegistry& operator=(const ZooKeeperRegistry&) = delete;

    // 注册服务实例
    bool registerService(const ServiceInstance& instance) override;

    // 注销服务实例
    bool unregisterService(const std::string& service_name, const std::string& instance_id) override;

    // 发现服务实例
    std::vector<ServiceInstance> discoverService(const std::string& service_name) override;

    // 订阅服务变化
    bool subsribeService(const std::string& service_name, ServiceInstanceCallback callback) override;

    // 取消订阅服务变化
    bool unsubsribeService(const std::string& service_name) override;

    // 发送心跳
    bool sendHeartbeat(const std::string& service_name, const std::string& instance_id) override;

    // 获取所有注册的服务名称
    std::vector<std::string> getAllService() override;

    // 检查连接状态
    bool isConnected() const;

    // 等待连接建立
    bool waitForConnection(int timeout_ms = 5000);


private:
    std::string hosts_; // zookeeper服务器地址
    int session_timeout_; // 会话超时时间
    zhandle_t* zk_handle_; // zookeeper句柄
    std::atomic<bool> connected_; // 连接状态
    std::atomic<bool> running_; // 运行状态
    std::thread watcher_thread_; // 监听线程
    std::mutex mutex_; // 互斥锁
    std::condition_variable cv_; // 条件变量

    // 服务实例缓存
    std::unordered_map<std::string, std::unordered_map<std::string, ServiceInstance>> service_cache_;
    std::unordered_map<std::string, ServiceInstanceCallback> callbacks_;

    // zookeeper路径常量
    static const std::string ROOT_PATH; // 根路径
    static const std::string SERVICE_PATH; // 服务路径

    // 初始化 zookeeper
    bool initializeZooKeeper();

    // 关闭 zookeeper
    void closeZooKeeper();

    // zookeeper 连接回调 watcher
    static void connectionWatcher(zhandle_t* zh, int type, int state, const char* path, void* watcherCtx);

    // 服务实例变化 监听回调
    static void serviceWatcher(zhandle_t* zh, int type, int state, const char* path, void* watcherCtx);

    // 监听线程-主函数
    void watcherMain();

    // 创建服务路径
    bool createServicePath(const std::string& service_name);

    // 创建服务实例节点
    std::string createServiceInstanceNode(const std::string& service_name, const ServiceInstance& instance);

    // 删除服务实例节点
    bool deleteServiceInstanceNode(const std::string& service_name, const std::string& instance_id);

    // 获取服务实例
    std::vector<ServiceInstance> getServiceInstances(const std::string& service_name);

    // 序列化服务实例：struct ServiceInstance -> vector<uint8_t>
    std::vector<uint8_t> serializeInstance(const ServiceInstance& instance);

    // 反序列化服务实例：vector<uint8_t> -> struct ServiceInstance
    ServiceInstance deserializeInstance(const std::vector<uint8_t>& data);

    // 通知服务变化
    void notifyServiceChange(const std::string& service_name);

    // 更新服务缓存
    void updateServiceCache(const std::string& service_name);
};

}