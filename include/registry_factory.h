#include "registry.h"

namespace rpc {

class RegistryFactory {
public:
    // 静态工厂方法
    static std::unique_ptr<ServiceRegistry> createZooKeeperRegistry(const std::string& zk_hosts = "localhost:2181");

    static std::unique_ptr<ServiceRegistry> createRegistry(const std::string& type, const std::unordered_map<std::string,std::string>& config = {});
};

}