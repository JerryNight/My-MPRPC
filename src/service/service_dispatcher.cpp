#include "../../include/service_dispatcher.h"

namespace rpc {


// 注册 protobuf 服务
bool ServiceDispatcher::registerService(google::protobuf::Service* service) {
    if (!service) {
        std::cerr << "Cannot register null protobuf service" << std::endl;
        return false;
    }

    // 获取服务名称
    std::string service_name = service->GetDescriptor()->full_name();
    if (service_name.empty()) {
        std::cerr << "Service name cannot be empty" << std::endl;
        return false;
    }

    auto it = services_.find(service_name);
    if (it != services_.end()) {
        // 服务已存在
        std::cerr << "Service '" << service_name << "' is already registered" << std::endl;
        return false;
    }

    // 注册
    services_[service_name] = service;
    return true;
}

// 分发 RPC 请求 -- 处理请求
bool ServiceDispatcher::dispatcher(const RpcRequest& request, RpcResponse& response) {
    if (request.service_name.empty()) {
        generateErrorResponse(request.request_id, "Service name is empty", response);
        return false;
    }
    if (request.method_name.empty()) {
        generateErrorResponse(request.request_id, "Method name is empty", response);
        return false;
    }
    if (!isServiceRegistered(request.service_name)) {
        generateErrorResponse(request.request_id, "Service not found: " + request.service_name, response);
        return false;
    }

    // 根据服务名和方法名查找对象
    auto service = services_[request.service_name];
    auto method = service->GetDescriptor()->FindMethodByName(request.method_name);
    if (!method) {
        generateErrorResponse(request.request_id, "Method not found: " + request.method_name, response);
        return false;
    }

    // 处理请求
    // 创建请求对象 & 响应对象
    std::unique_ptr<google::protobuf::Message> request_message(service->GetRequestPrototype(method).New());
    std::unique_ptr<google::protobuf::Message> response_message(service->GetResponsePrototype(method).New());

    // 反序列化请求对象
    bool ret = request_message->ParseFromString(std::string(request.request_data.begin(), request.request_data.end()));
    if (!ret) {
        generateErrorResponse(request.request_id, "Failed to parse request data", response); 
        return false;
    }

    // 调用服务方法
    service->CallMethod(method, nullptr, request_message.get(), response_message.get(), nullptr);

    // 序列化响应数据
    std::string serialized_response;
    bool rt = response_message->SerializeToString(&serialized_response);
    if (!rt) {
        generateErrorResponse(request.request_id, "Failed to serialize response data", response);
        return false;
    }

    // 设置响应结果
    response.request_id = request.request_id;
    response.success = true;
    response.error_message.clear();
    response.response_data.assign(serialized_response.begin(), serialized_response.end());

    return true;
}

// 注销 protobuf 服务
bool ServiceDispatcher::unregisterService(const std::string& service_name) {
    if (service_name.empty()) {
        return false;
    }

    auto it = services_.find(service_name);
    if (it == services_.end()) {
        return false;
    }
    
    // 注销
    services_.erase(it);
    return true;
}

// 获取所有服务
std::vector<std::string> ServiceDispatcher::getRegisteredServices() const {
    std::vector<std::string> service_names;
    service_names.reserve(services_.size());

    for (const auto& pair : services_) {
        service_names.push_back(pair.first);
    }
    
    return service_names;
}

// 查询服务
bool ServiceDispatcher::isServiceRegistered(const std::string& service_name) const {
    return services_.find(service_name) != services_.end(); 
}

// 生成错误响应
void ServiceDispatcher::generateErrorResponse(uint64_t request_id, const std::string& error_message, RpcResponse& response) {
    response.request_id = request_id;
    response.success = false;
    response.error_message = error_message;
    response.response_data.clear();
}


}