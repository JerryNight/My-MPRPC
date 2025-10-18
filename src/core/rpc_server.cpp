#include "../../include/rpc_serser.h"
#include "../../include/rpc_protocol_helper.h"
#include <exception>

namespace rpc {

RpcServer::RpcServer(const RpcServerConfig& config) 
    : config_(config)
     ,running_(false) {}

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
    std::cout << "RPC Server started successfully on " << config_.host << ":" << config_.port << std::endl;
    return true;
}

// 停止服务器
void RpcServer::stop() {
    if (!running_) {
        return;
    }
    running_ = false;

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
    std::unique_lock<std::shared_mutex> lock(services_mutex_);
    services_[service->GetDescriptor()->name()] = service;
    std::cout << "Registered service: " << service->GetDescriptor()->name() << std::endl;
    return true;
}

// 注销服务
bool RpcServer::unregisterService(const std::string& service_name) {
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
    try {
        // 解析RPC请求
        RpcRequest request = parseRpcRequest(request_data);
    
        // 调用服务方法
        std::vector<uint8_t> response_data = callServiceMethod(request.service_name, request.method_name, request.data);
    
        // 创建RPC响应
        RpcResponse response;
        response.request_id = request.request_id;
        response.data = response_data;
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
        return;
    }

    // 序列化响应
    auto response_proto_data = serializeRpcResponse(response);

    // 编码+帧前缀
    auto response_proto_encode = frame_codec_->encode(response_proto_data);

    // 发送
    if (!connection->send(response_proto_encode)) {
        std::cerr << "Failed to send response to " << connection->getRemoteAddress() << std::endl;
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


}