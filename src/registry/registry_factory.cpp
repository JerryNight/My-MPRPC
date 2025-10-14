#include "../../include/registry_factory.h"

namespace rpc {

std::unique_ptr<ServiceRegistry> RegistryFactory::createZooKeeperRegistry(const std::string& zk_hosts) {
    return std::make_unique<ZooKeeperRegistry>(zk_hosts);
}

std::unique_ptr<ServiceRegistry> RegistryFactory::createRegistry(const std::string& type, const std::unordered_map<std::string,std::string>& config) {
    if (type == "zookeeper") {
        std::string hosts = "localhost:2181";
        auto it = config.find("hosts");
        if (it != config.end()) {
            hosts = it->second;
        }
        return createZooKeeperRegistry(hosts);
    }
    return nullptr;
}


}