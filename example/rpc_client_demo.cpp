#include "rpc_client.h"
#include "calculator_service.h"

using namespace rpc;

// 客户端：调用 RPC 服务（直连模式）
void runClient_DirectMode() {
    std::cout << "客户端启动 - 直连模式" << std::endl;
    
    // 1. 创建客户端(服务名、服务器IP、服务器端口)
    RpcClientStubImpl client("CalculatorService", "127.0.0.1", 9000);
    std::cout << "客户端已创建" << std::endl;

    // 2. 连接到服务器
    if (client.connect()) {
        std::cout << "已连接到服务器" << std::endl;
    } else {
        std::cerr << "连接失败！请确保服务器已启动" << std::endl;
        return;
    }
    
    // 3. 调用远程方法
    std::cout << "\n【测试 1】调用 Add 方法" << std::endl;
    AddRequest add_request;
    add_request.set_a(10);
    add_request.set_b(20);
    AddResponse add_response;
    if (client.callMethod("Add", add_request, add_response)) {
        std::cout << "调用成功！" << std::endl;
        std::cout << "返回结果: " << add_response.result() << std::endl;
    } else {
        std::cerr << "调用失败" << std::endl;
    }

    std::cout << "\n【测试 2】调用 Sub 方法" << std::endl;
    SubRequest sub_request;
    sub_request.set_a(50);
    sub_request.set_b(30);
    SubResponse sub_response;
    if (client.callMethod("Sub", sub_request, sub_response)) {
        std::cout << "调用成功！" << std::endl;
        std::cout << "返回结果: " << sub_response.response() << std::endl;
    } else {
        std::cerr << "调用失败" << std::endl;
    }

    std::cout << "\n【测试 3】调用 Multi 方法" << std::endl;
    MultiRequest multi_request;
    multi_request.set_a(50);
    multi_request.set_b(30);
    MultiResponse multi_response;
    if (client.callMethod("Mul", multi_request, multi_response)) {
        std::cout << "调用成功！" << std::endl;
        std::cout << "返回结果: " << multi_response.response() << std::endl;
    } else {
        std::cerr << "调用失败" << std::endl;
    }

    std::cout << "\n【测试 4】调用 Divide 方法" << std::endl;
    DivideRequest divide_request;
    divide_request.set_a(50);
    divide_request.set_b(5);
    DivideResponse divide_response;
    if (client.callMethod("Mul", divide_request, divide_response)) {
        std::cout << "调用成功！" << std::endl;
        std::cout << "返回结果: " << divide_response.result() << std::endl;
    } else {
        std::cerr << "调用失败" << std::endl;
    }

    // 断开连接
    std::cout << "断开连接..." << std::endl;
    client.disconnect();
    std::cout << "已断开连接" << std::endl;
}

void runClient_ServiceDiscoveryMode() {
    std::cout << "客户端启动 - 服务发现模式" << std::endl;

    try {
        // 1. 创建服务端
        auto registry = RegistryFactory::createZooKeeperRegistry("localhost:2181");
        std::cout << "已创建 zookeeper 注册中心\n"; 
        auto load_balancer = LoadBalancerFactory::createLoadBalancer("RoundRobin");
        std::cout << "已创建负载均衡器\n"; 
        RpcClientStubImpl client("CalculatorService", std::move(registry), std::move(load_balancer));
        std::cout << "已创建客户端\n"; 
        std::cout << "客户端会自动从注册中心发现可用的服务实例\n";
    
        // 2. 调用远程方法
        // 多次调用，观察负载均衡效果
        for (int i = 1; i <= 30; ++i) {
            std::cout << "\n【调用 " << i << "】Add 方法" << std::endl;
            AddRequest request;
            request.set_a(i * 10);
            request.set_b(i * 5);
            
            AddResponse response;
            
            std::cout << "📤 发送请求: Add(" << request.a() << ", " << request.b() << ")" << std::endl;
            std::cout << "   ℹ️  自动从注册中心发现服务并选择实例..." << std::endl;
            
            if (client.callMethod("Add", request, response)) {
                std::cout << "✅ 调用成功！" << std::endl;
                std::cout << "📥 返回结果: " << response.result() << std::endl;
            } else {
                std::cerr << "❌ 调用失败" << std::endl;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cout << "服务发现模式测试完成！" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "❌ 异常: " << e.what() << std::endl;
    }

}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage<直连模式>: ./rpc_client_demo client\n";
        std::cout << "Usage<服务发现模式>: ./rpc_client_demo client-registry\n";
        return 0;
    }
    std::string mode = argv[1];
    if (mode == "client") {
        runClient_DirectMode();
    } else if (mode == "client-registry") {
        runClient_ServiceDiscoveryMode();
    } else {
        std::cout << "Usage<直连模式>: ./rpc_client_demo client\n";
        std::cout << "Usage<服务发现模式>: ./rpc_client_demo client-registry\n";
        return 0;
    }
}