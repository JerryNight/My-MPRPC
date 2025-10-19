#include "rpc_serser.h"
#include "rpc_protocol_helper.h"
#include <exception>

namespace rpc {

RpcServer::RpcServer(const RpcServerConfig& config) 
    : config_(config)
     ,running_(false)
     ,heartbeat_running_(false) {}

RpcServer::~RpcServer() {
    stop();
}

// 启动服务器
bool RpcServer::start() {
    if (running_.load()) {
        return true;
    }

    // 初始化组件
    if (!initializeComponents()) {
        std::cerr << "Failed to initialize components" << std::endl;
        return false;
    }
    
    // 创建 TCP 服务器
    tcp_server_ = std::make_unique<TcpServerImpl>();
    if (!tcp_server_) {
        std::cerr << "Failed to create TCP server" << std::endl;
        return false;
    }

    // 设置连接回调
    tcp_server_->setConnectionCallback([this](std::shared_ptr<TcpConnection> connection) {
        handleNewConnection(connection);
    });

    // 启动 TCP 服务器
    if (!tcp_server_->start(config_.port, config_.host)) {
        std::cerr << "Failed to start TCP server" << std::endl;
        return false;
    }

    running_ = true;

    // 如果启动了服务注册，初始化注册中心，注册所有服务
    if (config_.enable_registry) {
        if (!initializeRegistry()) {
            // 初始化失败，无法使用服务发现，服务器正常运行
            std::cerr << "Failed to initialize registry" << std::endl;
        } else {
            // 注册所有服务
            std::shared_lock<std::shared_mutex> lock(services_mutex_);
            for (const auto& pair : services_) {
                registerToRegistry(pair.first);
            }
            // 启动心跳线程
            heartbeat_running_ = true;
            heartbeat_thread_ = std::thread(&RpcServer::heartbeatLoop, this);
        }
    }
    std::cout << "RPC Server started successfully on " << config_.host << ":" << config_.port << std::endl;
    return true;
}

// 停止服务器
void RpcServer::stop() {
    if (!running_) {
        return;
    }
    running_ = false;

    // 停止心跳线程
    if (heartbeat_running_) {
        heartbeat_running_ = false;
        if (heartbeat_thread_.joinable()) {
            heartbeat_thread_.join();
        }
    }

    // 从注册中心注销所有服务
    if (registry_) {
        std::shared_lock<std::shared_mutex> lock(services_mutex_);
        for (const auto& pair : services_) {
            unregisterFromRegistry(pair.first);
        }
    }

    // 停止 tcp 服务器
    if (tcp_server_) {
        tcp_server_->stop();
    }

    // 清理连接
    {
        std::unique_lock<std::shared_mutex> lock(connections_mutex_);
        connections_.clear();
    }
    std::cout << "RPC Server stopped" << std::endl;
}

// 注册 Protobuf 服务
bool RpcServer::registerService(google::protobuf::Service* service) {
    if (!service) {
        std::cerr << "Service cannot be null" << std::endl;
        return false;
    }

    // 注册服务
    std::string service_name = service->GetDescriptor()->name();
    {
        // 注册到本地
        std::unique_lock<std::shared_mutex> lock(services_mutex_);
        services_[service_name] = service;
    }
    // 注册到 zookeeper
    if (running_.load() && config_.enable_registry && registry_) {
        registerToRegistry(service_name);
    }
    std::cout << "Registered service: " << service->GetDescriptor()->name() << std::endl;
    return true;
}

// 注销服务
bool RpcServer::unregisterService(const std::string& service_name) {
    // 先从注册中心注销服务
    if (config_.enable_registry && registry_) {
        unregisterFromRegistry(service_name);
    }
    // 在从本地注销
    std::unique_lock<std::shared_mutex> lock(services_mutex_);
    auto it = services_.find(service_name);
    if (it != services_.end()) {
        services_.erase(it);
        std::cout << "Unregistered service: " << service_name << std::endl;
        return true;
    }
    return false;
}

// 获取服务器状态
bool RpcServer::isRunning() const {
    return running_.load();
}

// 获取服务器配置
const RpcServerConfig& RpcServer::getConfig() const {
    return config_;
}

// 获取已注册服务列表
std::vector<std::string> RpcServer::getRegisteredService() const {
    std::shared_lock<std::shared_mutex> lock(services_mutex_);
    std::vector<std::string> service_names;
    for (const auto& pair : services_) {
        service_names.push_back(pair.first);
    }
    return service_names;
}

// 获取当前连接数
size_t RpcServer::getConnectionCount() const {
    std::shared_lock<std::shared_mutex> lock(connections_mutex_);
    return connections_.size();
}

// 获取线程池大小
size_t RpcServer::getThreadPoolSize() const {
    if (!thread_pool_) {
        return 0;
    }
    return thread_pool_->getThreadCount();
}

// 初始化组件
bool RpcServer::initializeComponents() {
    // 创建编解码器
    frame_codec_ = std::make_unique<FrameCodec>();

    // 创建序列化器
    serializer_ = SerializerFactory::createSerializer(config_.serializer_type);
    if (!serializer_) {
        std::cerr << "Failed to create serializer: " << config_.serializer_type << std::endl;
        return false;
    }

    // 创建线程池
    thread_pool_ = std::make_unique<ThreadPool>(config_.thread_pool_size);
    if (!thread_pool_) {
        std::cerr << "Failed to create thread pool" << std::endl;
        return false;
    }

    return true;
}

// 处理新连接
void RpcServer::handleNewConnection(std::shared_ptr<TcpConnection> connection) {
    std::string connection_id = connection->getRemoteAddress();

    {
        std::unique_lock<std::shared_mutex> lock(connections_mutex_);
        connections_[connection] = connection_id;
    }

    // 设置消息回调
    connection->setMessageCallback([this](std::shared_ptr<TcpConnection> conn, const std::vector<uint8_t>& data) {
        handleMessage(conn, data);
    });
    // 设置错误回调
    connection->setErrorCallback([this](std::shared_ptr<TcpConnection> conn, const std::string& error) {
        handleError(conn, error);
    });

    std::cout << "New connection established: " << connection_id << std::endl;
}

// 处理消息
void RpcServer::handleMessage(std::shared_ptr<TcpConnection> connection, const std::vector<uint8_t>& data) {
    if (thread_pool_) {
        thread_pool_->submit([this, connection, data]() {
            handleRpcRequest(connection, data);
        });
    } else {
        handleRpcRequest(connection, data);
    }
}

// 处理rpc请求
void RpcServer::handleRpcRequest(std::shared_ptr<TcpConnection> connection, const std::vector<uint8_t>& request_data) {
    std::cout << "[DEBUG SERVER] handleRpcRequest: received " << request_data.size() << " bytes" << std::endl;

    try {
        // 解析RPC请求
        RpcRequest request = parseRpcRequest(request_data);
        std::cout << "[DEBUG SERVER] Parsed request: service=" << request.service_name 
                  << ", method=" << request.method_name 
                  << ", request_id=" << request.request_id << std::endl;
    
        // 调用服务方法
        std::vector<uint8_t> response_data = callServiceMethod(request.service_name, request.method_name, request.request_data);
        std::cout << "[DEBUG SERVER] Service method returned " << response_data.size() << " bytes" << std::endl;
    
        // 创建RPC响应
        RpcResponse response;
        response.request_id = request.request_id;
        response.response_data = response_data;
        response.success = true;
    
        // 发送响应
        sendResponse(connection, response);
    } catch (const std::exception& e) {
        std::cerr << "Error handling RPC request: " << e.what() << std::endl;

        // 发送错误响应
        RpcResponse response;
        response.request_id = 0;
        response.success = false;
        response.error_message = e.what();
        sendResponse(connection, response);
    }
}

// 处理连接断开
void RpcServer::handleConnectionClosed(std::shared_ptr<TcpConnection> connection) {
    std::string connection_id = connection->getRemoteAddress();
    {
        std::unique_lock<std::shared_mutex> lock(connections_mutex_);
        connections_.erase(connection);
    }
    std::cout << "Connection closed: " << connection_id << std::endl;
}

// 处理错误
void RpcServer::handleError(std::shared_ptr<TcpConnection> connection, const std::string& error_message) {
    std::cerr << "Connection error from " << connection->getRemoteAddress() << ": " << error_message << std::endl;
    handleConnectionClosed(connection);
}

// 发送响应
void RpcServer::sendResponse(std::shared_ptr<TcpConnection> connection, const RpcResponse& response) {
    if (!connection) {
        std::cerr << "[DEBUG SERVER] sendResponse: connection is null!" << std::endl;
        return;
    }
    std::cout << "[DEBUG SERVER] Preparing to send response, request_id: " << response.request_id 
              << ", success: " << response.success << std::endl;

    // 序列化响应
    auto response_proto_data = serializeRpcResponse(response);
    std::cout << "[DEBUG SERVER] Serialized response data size: " << response_proto_data.size() << " bytes" << std::endl;

    // 编码+帧前缀
    auto response_proto_encode = frame_codec_->encode(response_proto_data);
    std::cout << "[DEBUG SERVER] Encoded response size (with frame): " << response_proto_encode.size() << " bytes" << std::endl;

    // 发送
    if (!connection->send(response_proto_encode)) {
        std::cerr << "Failed to send response to " << connection->getRemoteAddress() << std::endl;
    } else {
        std::cout << "[DEBUG SERVER] Encoded response size (with frame): " << response_proto_encode.size() << " bytes" << std::endl;
    }
}

// 解析rpc请求
RpcRequest RpcServer::parseRpcRequest(const std::vector<uint8_t>& data) {
    try {
        return RpcProtocolHelper::parseRequest(data);
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse RPC request: " << e.what() << std::endl;
        throw std::runtime_error("Invalid RPC request format: " + std::string(e.what()));
    }
}

// 序列化rpc响应
std::vector<uint8_t> RpcServer::serializeRpcResponse(const RpcResponse& response) {
    try {
        return RpcProtocolHelper::serializeResponse(response);
    } catch (const std::exception& e) {
        std::cerr << "Failed to serialize RPC response: " << e.what() << std::endl;
        // 返回错误响应
        RpcResponse error_response;
        error_response.request_id = response.request_id;
        error_response.success = false;
        error_response.error_message = "Serialization error: " + std::string(e.what());
        // 尝试再次序列化（这次不会有数据）
        return RpcProtocolHelper::serializeResponse(error_response);
    }
}

// 调用服务方法
std::vector<uint8_t> RpcServer::callServiceMethod(const std::string& service_name, const std::string& method_name, const std::vector<uint8_t>& request_data) {
    std::shared_lock<std::shared_mutex> lock(services_mutex_);

    // 查找服务
    auto it = services_.find(service_name);
    if (it == services_.end()) {
        throw std::runtime_error("Service not found: " + service_name);
    }

    google::protobuf::Service* service = it->second;
    const google::protobuf::ServiceDescriptor* descriptor = service->GetDescriptor();

    // 查找方法
    const google::protobuf::MethodDescriptor* method = descriptor->FindMethodByName(method_name);
    if (!method) {
        throw std::runtime_error("Method not found: " + method_name);
    }

    // 创建请求和响应消息
    std::unique_ptr<google::protobuf::Message> request(service->GetRequestPrototype(method).New());
    std::unique_ptr<google::protobuf::Message> response(service->GetResponsePrototype(method).New());

    // 反序列化请求参数
    if (!request->ParseFromArray(request_data.data(), request_data.size())) {
        throw std::runtime_error("Failed to parse request");
    }
    
    // 调用服务方法
    service->CallMethod(method, nullptr, request.get(), response.get(), nullptr);

    // 序列化响应
    std::string response_data;
    if (!response->SerializeToString(&response_data)) {
        throw std::runtime_error("Failed to serialize response");
    }

    return std::vector<uint8_t>(response_data.begin(), response_data.end());
}

// 设置服务注册中心
void RpcServer::setRegistry(std::unique_ptr<ServiceRegistry> registry) {
    registry_ = std::move(registry);
}

// 获取注册中心
ServiceRegistry* RpcServer::getRegistry() const {
    return registry_.get();
}

// 注册服务到注册中心
bool RpcServer::registerToRegistry(const std::string& service_name) {
    if (!registry_) {
        std::cerr << "Registry not initialized" << std::endl;
        return false;
    }
    try {
        ServiceInstance instance(
            service_name,
            config_.host = "0.0.0.0" ? "127.0.0.1" : config_.host,
            config_.port,
            config_.service_weight
        );
    
        // 注册到注册中心
        if (registry_->registerService(instance)) {
            std::cout << "Registered service to registry: " << service_name << " @ " << instance.getId() << std::endl;
            return true;
        } else {
            std::cerr << "Failed to register service to registry: " << service_name << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception registering service: " << e.what() << std::endl;
    }
    return false;
}

// 从注册中心注销服务
bool RpcServer::unregisterFromRegistry(const std::string& service_name) {
    if (!registry_) {
        return false;
    }

    try {
        std::string instance_id = (config_.host == "0.0.0.0" ? "127.0.0.1" : config_.host) + ":" + std::to_string(config_.port);
        if (registry_->unregisterService(service_name, instance_id)) {
            std::cout << "Unregistered service from registry: " << service_name << " @ " << instance_id << std::endl;
            return true;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception unregistering service: " << e.what() << std::endl;
    }
    return false;
}

// 心跳线程函数
void RpcServer::heartbeatLoop() {
    std::cout << "Heartbeat thread started" << std::endl;

    while (heartbeat_running_.load()) {
        try {
            // 获取所有已注册服务
            std::vector<std::string> service_names;
            {
                std::shared_lock<std::shared_mutex> lock(services_mutex_);
                for (const auto& pair : services_) {
                    service_names.push_back(pair.first);
                }
            }
            // 向注册中心发送心跳
            if (registry_) {
                std::string instance_id = (config_.host == "0.0.0.0" ? "127.0.0.1" : config_.host) + ":" + std::to_string(config_.port);
                for (const auto& service_name : service_names) {
                    if (!registry_->sendHeartbeat(service_name, instance_id)) {
                        std::cerr << "Failed to send heartbeat for service: " << service_name << std::endl;
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Exception in heartbeat loop: " << e.what() << std::endl;
        }

        // 等待下一个心跳间隔
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.heartbeat_interval_ms));
    }
    std::cout << "Heartbeat thread stopped" << std::endl;
}

// 初始化服务注册中心
bool RpcServer::initializeRegistry() {
    if (!config_.enable_registry) {
        return false;
    }

    // 使用工厂方法创建注册中心
    registry_ = RegistryFactory::createZooKeeperRegistry();
    if (!registry_) {
        std::cerr << "Failed to create registry: " << config_.registry_type << std::endl;
        return false;
    }
    std::cout << "Initialized registry: " << config_.registry_type << std::endl;
    return true;
}

}