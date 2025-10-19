#include "rpc_serser.h"
#include "calculator_service.h"
#include "registry.h"
#include "load_balancer.h"
#include <thread>
#include <chrono>

using namespace rpc;

void runServer(bool use_registry = false) {
    std::cout << "启动 RPC 服务器" << std::endl;

    // 1. 配置服务器
    RpcServerConfig config;
    config.host = "0.0.0.0"; // 监听所有接口
    config.port = 9000; // 监听端口
    config.thread_pool_size = 4; // 线程池大小
    config.max_connections = 100; // 最大连接数
    config.serializer_type = "protobuf"; // 序列化方式
    if (use_registry) {
        // 配置服务注册中心
        config.enable_registry = true;
        config.registry_type = "zookeeper";
        config.registry_address = "localhost:2181";
        config.service_weight = 1; // 服务权重
        config.heartbeat_interval_ms = 10000; // 心跳间隔
        std::cout << "服务注册模式：已启用" << std::endl;
        std::cout << "注册中心: " << config.registry_address << std::endl;
    } else {
        config.enable_registry = false;
        std::cout << "服务注册模式：未启用（直连模式）" << std::endl;
    }

    // 2. 创建服务器
    RpcServer server(config);
    std::cout << "RPC 服务器已创建" << std::endl;

    // 3. 创建服务实例
    CalculatorServiceImpl calculatorService;

    // 4. 注册服务
    if (server.registerService(&calculatorService)) {
        std::cout << "服务已注册: CalculatorService" << std::endl;
    } else {
        std::cerr << "服务注册失败" << std::endl;
        return;
    }

    // 5. 启动服务器
    if (server.start()) {
        std::cout << "RPC 服务器已在后台启动！等待客户端连接..." << std::endl;
    } else {
        std::cerr << "服务器启动失败" << std::endl;
        return;
    }

    // 保持运行
    for (int i = 100; i >= 0; --i) {
        std::cout << "RPC 服务器将在" << i << "秒后关闭" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    server.stop();
    std::cout << "RPC 服务器已停止" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage<直连模式>: ./rpc_server_demo server" << std::endl;
        std::cout << "Usage<服务发现模式>: ./rpc_server_demo server-registry" << std::endl;
    }
    std::string mode = argv[1];
    if (mode == "server") {
        runServer(false);
    } else if (mode == "server-registry") {
        runServer(true);
    } else {
        std::cout << "Usage<直连模式>: ./rpc_server_demo server" << std::endl;
        std::cout << "Usage<服务发现模式>: ./rpc_server_demo server-registry" << std::endl;
    }
    return 0;
}