#include "rpc_client.h"



namespace rpc {

// 直连模式，不使用服务发现
RpcClientStubImpl::RpcClientStubImpl(const std::string& service_name, const std::string& host, uint16_t port)
    :service_name_(service_name),
     host_(host),
     port_(port),
     connected_(false),
     use_service_discovery_(false)
    {
        frame_codec_ = std::make_unique<FrameCodec>();
    }

// 服务发现模式：使用注册中心和负载均衡器
RpcClientStubImpl::RpcClientStubImpl(const std::string& service_name, std::unique_ptr<ServiceRegistry> registry, std::unique_ptr<LoadBalancer> load_balancer) 
    :service_name_(service_name),
     host_(""),
     port_(0),
     connected_(false),
     use_service_discovery_(true),
     registry_(std::move(registry)),
     load_balancer_(std::move(load_balancer))
{
    frame_codec_ = std::make_unique<FrameCodec>();
    // 如果没有提供负载均衡器，使用轮询
    if (!load_balancer_) {
        load_balancer_ = LoadBalancerFactory::getInstance().create("RoundRobin");
    }
}

RpcClientStubImpl::~RpcClientStubImpl() {
    disconnect();
}

// 调用RPC方法
bool RpcClientStubImpl::callMethod(const std::string& method_name,
                const google::protobuf::Message& request,
                google::protobuf::Message& response) 
{
    // 如果使用服务发现模式，要先选择实例，再进行连接
    if (use_service_discovery_) {
        // 从注册中心发现服务，通过负载均衡器选择实例
        ServiceInstance instance = selectServiceInstance();
        // 检查是否需要重新连接：如果实例改变，需要重新连接
        std::string new_instance_id = instance.getId();
        if (!isConnected() || current_instance_id_ != new_instance_id) {
            //先断开旧连接
            disconnect();
            // 连接到新服务器
            if (!connectToInstance(instance)) {
                std::cerr << "Rpc_Client.cpp::Failed to connect to service instance: " << new_instance_id << std::endl;
                return false;
            }
            current_instance_id_ = new_instance_id;
        }
        // 如果使用的是最少连接数负载均衡器，要更新连接信息
        if (load_balancer_->getName() == "LeastConnection") {
            load_balancer_->updateStats(current_instance_id_, true);
        }
    } else { // 直连模式
        if (!isConnected()) {
            if (!connect()) {
                std::cerr << "Rpc_Client.cpp::Not connected to server" << std::endl;
                return false;
            }
        }
    }

    bool result = false;
    try {
        // 创建RPC请求
        RpcRequest rpc_request;
        rpc_request.service_name = service_name_;
        rpc_request.method_name = method_name;
        rpc_request.request_id = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    
        // 序列化请求
        std::string request_str;
        if (!request.SerializeToString(&request_str)) {
            std::cerr << "Rpc_Client.cpp::Failed to serialize request" << std::endl;
            result = false;
        }
        rpc_request.request_data = std::vector<uint8_t>(request_str.begin(), request_str.end());
    
        // 发送请求，获取响应
        RpcResponse rpc_response = sendRpcRequest(rpc_request);
        if (!rpc_response.success) {
            std::cerr << "Rpc_Client.cpp::RPC call failed: " << rpc_response.error_message << std::endl;
            result = false;
        }
    
        // 反序列化RPC响应
        if (!response.ParseFromArray(rpc_response.response_data.data(), rpc_response.response_data.size())) {
            std::cerr << "Rpc_Client.cpp::Failed to parse response" << std::endl;
            result = false;
        }

        result = true;
    } catch (const std::exception& e) {
        std::cerr << "Rpc_Client.cpp::RPC call error: " << e.what() << std::endl;
        return false;
    }

cleanup:
    if (use_service_discovery_ && load_balancer_ && 
        load_balancer_->getName() == "LeastConnection" && 
        !current_instance_id_.empty()) 
        {
            load_balancer_->updateStats(current_instance_id_, false);
        }
    return result;
}
    
// 连接服务器
bool RpcClientStubImpl::connect() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (isConnected()) {
        return true;
    }

    // 创建TCP客户端
    tcp_client_ = std::make_shared<TcpClientImpl>();
    if (!tcp_client_) {
        std::cerr << "Rpc_Client.cpp::Failed to create TCP client" << std::endl;
        return false;
    }

    // 连接服务器
    if (!tcp_client_->connect(host_, port_)) {
        std::cerr << "Rpc_Client.cpp::Failed to connect to server" << std::endl;
        return false;
    }

    connected_ = true;
    std::cout << "Rpc_Client.cpp::Connected to RPC server: " << host_ << ":" << port_ << std::endl;
    return true;
}

// 断开连接
void RpcClientStubImpl::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (tcp_client_) {
        tcp_client_->disconnect();
        tcp_client_.reset();
    }
    
    connected_ = false;
}

// 检查连接状态
bool RpcClientStubImpl::isConnected() const {
    return connected_.load();
}

// 发送 RPC请求
RpcResponse RpcClientStubImpl::sendRpcRequest(const RpcRequest& request) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!tcp_client_) {
        throw std::runtime_error("Rpc_Client.cpp::Client not initialized");
    }

    // 序列化请求
    std::vector<uint8_t> request_data;
    try {
        request_data = RpcProtocolHelper::serializeRequest(request);
    } catch (std::exception& e) {
        throw std::runtime_error("Rpc_Client.cpp::Failed to serialize request: " + std::string(e.what()));
    }

    // 编码+帧前缀
    std::vector<uint8_t> request_encoded = frame_codec_->encode(request_data);

    // 发送请求
    if (!tcp_client_->send(request_encoded)) {
        throw std::runtime_error("Rpc_Client.cpp::Failed to send request");
    }

    // 接收响应
    std::vector<uint8_t> response_data;
    if (!tcp_client_->receive(response_data)) {
        throw std::runtime_error("Failed to receive response from server");
    }; // receive里有解码

    // 解析响应
    try {
        return RpcProtocolHelper::parseResponse(response_data);
    } catch (std::exception& e) {
        throw std::runtime_error("Rpc_Client.cpp::Failed to parse response: " + std::string(e.what()));
    }
}

// 从注册中心发现服务，选择实例
ServiceInstance RpcClientStubImpl::selectServiceInstance() {
    if (!registry_) {
        throw std::runtime_error("Rpc_Client.cpp::Service registry not initialized");
    }

    // 从注册中心发现实例
    std::vector<ServiceInstance> instances = registry_->discoverService(service_name_);
    if (instances.empty()) {
        throw std::runtime_error("Rpc_Client.cpp::No available service instances for: " + service_name_);
    }

    // 使用负载均衡器选择实例
    if (!load_balancer_) {
        for (const auto& instance : instances) {
            if (instance.is_healthy) {
                return instance;
            }
        }
        throw std::runtime_error("Rpc_Client.cpp::No healthy service instances for: " + service_name_);
    }

    return load_balancer_->select(instances);
}

// 连接到指定的服务实例
bool RpcClientStubImpl::connectToInstance(const ServiceInstance& instance) {
    std::lock_guard<std::mutex> lock(mutex_);
    // 更新连接信息
    host_ = instance.host;
    port_ = instance.port;
    // 创建TCP客户端
    tcp_client_ = std::make_shared<TcpClientImpl>();
    if (!tcp_client_) {
        std::cerr << "Rpc_Client.cpp::Failed to create TCP client" << std::endl;
        return false;
    }
    // 连接服务器
    if (!tcp_client_->connect(host_, port_)) {
        std::cerr << "Rpc_Client.cpp::Failed to connect to " << instance.getId() << std::endl;
        return false;
    }
    connected_ = true;
    return true;
}

// 设置负载均衡器
void RpcClientStubImpl::setLoadBalancer(std::unique_ptr<LoadBalancer> load_balancer) {
    std::lock_guard<std::mutex> lock(mutex_);
    load_balancer_ = std::move(load_balancer);
}

// 获取负载均衡器
LoadBalancer* RpcClientStubImpl::getLoadBalancer() const {
    return load_balancer_.get();
}

}