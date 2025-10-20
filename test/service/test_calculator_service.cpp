#include "../../include/calculator_service.h"
#include "../../include/service_dispatcher.h"
#include "../../proto/calculator.pb.h"

#include <iostream>

using namespace rpc;

void testCalculatorService() {
    std::cout << "=== Testing Calculator Service ===" << std::endl;

    // 创建分发器
    ServiceDispatcher dispatcher;

    // 创建计算服务
    auto calculator_service = std::make_unique<CalculatorServiceImpl>();

    // 在分发器中注册服务
    bool registered = dispatcher.registerService(calculator_service.get());
    if (!registered) {  // 如果注册失败
        std::cerr << "Failed to register calculator service" << std::endl;  // 输出错误信息
        return;  // 返回
    }

    // 测试加法操作
    std::cout << "\n--- Testing Add Operation ---" << std::endl;
    // 创建请求
    AddRequest request;
    request.set_a(10);
    request.set_b(20);

    // 将 AddRequest 对象 -> string
    std::string request_data;
    request.SerializeToString(&request_data);

    // 创建一个 RPC 请求
    RpcRequest rpc_request;
    rpc_request.request_id = 1;
    rpc_request.service_name = "CalculatorService";
    rpc_request.method_name = "Add";
    rpc_request.request_data.assign(request_data.begin(), request_data.end());

    // 创建 RPC 响应
    RpcResponse rpc_response;
    bool success = dispatcher.dispatcher(rpc_request, rpc_response);
    if (!success) {
        std::cerr << "Failed to dispatch Add request: " << rpc_response.error_message << std::endl;  // 输出错误信息
        return;
    }

    if (!rpc_response.success) {  // 如果响应失败
        std::cerr << "Add operation failed: " << rpc_response.error_message << std::endl;  // 输出错误信息
        return;  // 返回
    }

    // 反序列化响应
    AddResponse response;
    bool ret = response.ParseFromString(std::string(rpc_response.response_data.begin(),rpc_response.response_data.end()));
    if (!ret) {
        std::cerr << "Failed to parse AddResponse" << std::endl;  // 输出错误信息
        return;  // 返回
    }

    // 验证结果
    int32_t expected = 30;
    int32_t actual = response.result();
    if (expected == actual) {
        std::cout << "✓ Add test passed: " << request.a() << " + " << request.b() << " = " << actual << std::endl;
    } else {
        std::cerr << "✗ Add test failed: expected " << expected << ", got " << actual << std::endl; 
    }



}

int main() {
    testCalculatorService();
}