#include "../../include/rpc_client.h"



namespace rpc {

RpcClientStubImpl::RpcClientStubImpl(const std::string& service_name, const std::string& host, uint16_t port)
    :service_name_(service_name),
     host_(host),
     port_(port),
     connected_(false)
    {
        frame_codec_ = std::make_unique<FrameCodec>();
    }

RpcClientStubImpl::~RpcClientStubImpl() {
    disconnect();
}

// 调用RPC方法
bool RpcClientStubImpl::callMethod(const std::string& method_name,
                const google::protobuf::Message& request,
                google::protobuf::Message& response) 
{
    if (!isConnected()) {
        std::cerr << "Not connected to server" << std::endl;
        return false;
    }

    try {
        // 创建RPC请求
        RpcRequest rpc_request;
        rpc_request.service_name = service_name_;
        rpc_request.method_name = method_name;
        rpc_request.request_id = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    
        // 序列化请求
        std::string request_str;
        if (!request.SerializeToString(&request_str)) {
            std::cerr << "Failed to serialize request" << std::endl;
            return false;
        }
        rpc_request.request_data = std::vector<uint8_t>(request_str.begin(), request_str.end());
    
        // 发送请求，获取响应
        RpcResponse rpc_response = sendRpcRequest(rpc_request);
        if (!rpc_response.success) {
            std::cerr << "RPC call failed: " << rpc_response.error_message << std::endl;
            return false;
        }
    
        // 反序列化RPC响应
        if (response.ParseFromArray(rpc_response.response_data.data(), rpc_response.response_data.size())) {
            std::cerr << "Failed to parse response" << std::endl;
            return false;
        }

    } catch (const std::exception& e) {
        std::cerr << "RPC call error: " << e.what() << std::endl;
        return false;
    }

    return true;
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
        std::cerr << "Failed to create TCP client" << std::endl;
        return false;
    }

    // 连接服务器
    if (!tcp_client_->connect(host_, port_)) {
        std::cerr << "Failed to connect to server" << std::endl;
        return false;
    }

    connected_ = true;
    std::cout << "Connected to RPC server: " << host_ << ":" << port_ << std::endl;
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
        throw std::runtime_error("Client not initialized");
    }

    // 序列化请求
    std::vector<uint8_t> request_data;
    try {
        request_data = RpcProtocolHelper::serializeRequest(request);
    } catch (std::exception& e) {
        throw std::runtime_error("Failed to serialize request: " + std::string(e.what()));
    }

    // 编码+帧前缀
    std::vector<uint8_t> request_encoded = frame_codec_->encode(request_data);

    // 发送请求
    if (!tcp_client_->send(request_encoded)) {
        throw std::runtime_error("Failed to send request");
    }

    // 接收响应
    std::vector<uint8_t> response_data;
    tcp_client_->receive(response_data); // receive里有解码

    // 解析响应
    try {
        return RpcProtocolHelper::parseResponse(response_data);
    } catch (std::exception& e) {
        throw std::runtime_error("Failed to parse response: " + std::string(e.what()));
    }
}


}