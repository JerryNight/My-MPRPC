#include <string>
#include <iostream>
#include <buffer.h>
#include "sum.pb.h"
#include "rpcexception.h"

/**
 * RpcClient 是一个快递公司，负责打包消息，发送信件给收件人
 * CalculatorSereviceImpl 是收件人，它负责处理信件内容
 */
class RpcClient {
public:
    RpcClient(const char* ip, int port):ip_(ip),port_(port)
    {
        connectToServer();
    }

    // CallMethod 核心函数:可以调用任意函数
    // 负责封装RPC调用的通用逻辑：打包、发送、接收、解包
    void CallMethod(const std::string& service_name,
               const std::string& method_name,
               const google::protobuf::Message& request,
               google::protobuf::Message& response);

    // 用户调用sum，然后该函数调用RPC服务
    int sum(int a,int b);
    
    // 序列化:结果保存在pack_message
    void packMethod(const std::string& service_name, const std::string& method_name);
    
    // 把序列化后的数据，发给服务端
    void sendAll();

    // 接收客户端发来的数据
    std::string recvAll();


private:
    void connectToServer();
    std::string packProtoHeader(const std::string& service_name, const std::string& method_name);
    std::string packProtoBody(const google::protobuf::Message& request);
    std::string packProtobuf(const std::string& service_name, const std::string& method_name, const google::protobuf::Message& request);

private:
    // 缓冲区
    Buffer buffer_;
    int socket_fd_;

    // 服务端 ip port
    const char* ip_;
    int port_;

    // 序列化数据->发送到服务端
    std::string pack_message_;
    // 反序列化
    int result_;
};

void RpcClient::CallMethod(const std::string& service_name,
               const std::string& method_name,
               const google::protobuf::Message& request,
               google::protobuf::Message& response)
{
    try {
        // 打包
        pack_message_ = packProtobuf(service_name, method_name, request);
        // 发送
        sendAll();
        // 接收
        std::string recv_message = recvAll();
        // 解包
        response.ParseFromString(recv_message);
    } catch (const RpcException e) {
        std::cerr << "RPC Error: " << e.what() << std::endl;
        close (socket_fd_);
        throw;
    }
}


void RpcClient::connectToServer() {
    // 创建socket，连接服务端
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        throw RpcException("Failed to create socket");
    }
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(port_);
    inet_pton(AF_INET, ip_, &address.sin_addr);
    int connect_errno = connect(socket_fd_, (struct sockaddr*)&address, sizeof(address));
    if (connect_errno < 0) {
        close(socket_fd_);
        throw RpcException("Failed to connect to server");
    }
}

std::string RpcClient::packProtoHeader(const std::string& service_name, const std::string& method_name) {
    // 封装一个RPC协议包
    // 协议格式：[4 bytes: Magic] [4 bytes: 总长度] [4 bytes: 服务名长度] [服务名] [4 bytes: 方法名长度] [方法名] [Protobuf 消息体]
    // 服务名长度
    uint32_t service_len = service_name.length();
    uint32_t service_len_net = htonl(service_len);
    std::string service_len_str(reinterpret_cast<char*>(&service_len_net), sizeof(uint32_t));
    // 方法名长度
    uint32_t method_len = method_name.length();
    uint32_t method_len_net = htonl(method_len);
    std::string method_len_str(reinterpret_cast<char*>(&method_len_net), sizeof(uint32_t));

    return service_len_str + service_name + method_len_str + method_name;
}

std::string RpcClient::packProtoBody(const google::protobuf::Message& request) {
    return request.SerializeAsString();
}

std::string RpcClient::packProtobuf(const std::string& service_name, const std::string& method_name, const google::protobuf::Message& request) {
    std::string header = packProtoHeader(service_name, method_name);
    std::string body = packProtoBody(request);
    size_t total_len = 8 + service_name.length() + method_name.length() + body.length();
    // 魔数
    uint32_t magic = 0x12345678;
    uint32_t magic_net = htonl(magic);
    std::string magic_str(reinterpret_cast<char*>(&magic_net), sizeof(uint32_t));
    // 协议长度
    uint32_t total_len_net = htonl(total_len);
    std::string total_len_str(reinterpret_cast<char*>(&total_len_net), sizeof(uint32_t));
    // 打包
    return magic_str + total_len_str + header + body;
}

void RpcClient::sendAll() {
    int sended_bytes = 0;
    int bytes_to_send = pack_message_.length();
    while (sended_bytes < bytes_to_send) {
        int bytes_send = send(socket_fd_, pack_message_.c_str() + sended_bytes, bytes_to_send - sended_bytes, 0);
        if (bytes_send == -1) {
            if (errno == EAGAIN ||  errno == EWOULDBLOCK) {
                // 缓冲区满，不能写了
                continue;
            }
            throw RpcException("Failed to send data");
        } else if (bytes_send == 0){
            // 对端关闭
            throw RpcException("Connection closed by peer");
        }
        sended_bytes += bytes_send;
    }
}

std::string RpcClient::recvAll() {
    uint32_t magic = 0;
    uint32_t length = 0;
    std::string recv_message;
    while (true) {
        int n = buffer_.readFd(socket_fd_);
        if (buffer_.readableBytes() >= 8 && length == 0){
            // 读完了协议头
            magic = buffer_.readInt32();
            length = buffer_.readInt32();
        }
        if (buffer_.readableBytes() >= length) {
            // 读完了RPC响应包 读取Protobuf消息体
            recv_message = buffer_.retrieveAsString(length);
            break;
        }
        if (n == 0){
            // 对端关闭
            throw RpcException("Connection closed by peer");
        } else if (n < 0){
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                // readFd error
                throw RpcException("Failed to receive data");
            }
            // 缓冲区空了，但数据不够，半包
            continue;
        }
    }
    return recv_message;
}


// void RpcClient::packMethod(const std::string& service_name, const std::string& method_name) {
//     // 封装一个RPC协议包
//     // 协议格式：[4 bytes: Magic] [4 bytes: 总长度] [4 bytes: 服务名长度] [服务名] [4 bytes: 方法名长度] [方法名] [Protobuf 消息体]
//     // 服务名长度
//     uint32_t service_len = service_name.length();
//     uint32_t service_len_net = htonl(service_len);
//     std::string service_len_str(reinterpret_cast<char*>(&service_len_net), sizeof(uint32_t));
//     // 方法名长度
//     uint32_t method_len = method_name.length();
//     uint32_t method_len_net = htonl(method_len);
//     std::string method_len_str(reinterpret_cast<char*>(&method_len_net), sizeof(uint32_t));
//     // 创建SumRequest对象，序列化
//     SumRequest request;
//     request.set_a(a);
//     request.set_b(b);
//     std::string request_string = request.SerializeAsString();
//     // 计算总长度
//     size_t request_len = request_string.length();
//     size_t total_len = 4 + service_len + 4 + method_len + request_len;
//     uint32_t total_len_net = htonl(total_len);
//     std::string total_len_str(reinterpret_cast<char*>(&total_len_net), sizeof(uint32_t));
//     // 魔数
//     uint32_t magic = 0x12345678;
//     uint32_t magic_net = htonl(magic);
//     std::string magic_str(reinterpret_cast<char*>(&magic_net), sizeof(uint32_t));

//     pack_message_ = magic_str + total_len_str + service_len_str + service_name + method_len_str + method_name + request_string;
// }