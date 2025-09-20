#include "sum.pb.h"
#include <arpa/inet.h> // htonl
#include <unistd.h> // 包含 close 函数
#include "buffer.h"

/**
 * 一个RPC客户端，可以用一个类来封装。管理底层TCP连接、调用RPC方法等
 */

class Buffer;

// 客户端使用的接口。实现一个sum操作。 
// 但要调用RPC服务，让服务端来做sum，然后接收服务端返回的结果
void callSum(int a, int b) {
    // 封装一个RPC协议包
    // 协议格式：[4 bytes: Magic] [4 bytes: 总长度] [4 bytes: 服务名长度] [服务名] [4 bytes: 方法名长度] [方法名] [Protobuf 消息体]
    std::string service_name("CalculatorService");
    // 服务名长度
    uint32_t service_len = service_name.length();
    uint32_t service_len_net = htonl(service_len);
    std::string service_len_str(reinterpret_cast<char*>(&service_len_net), sizeof(uint32_t));
    // 方法名
    std::string method_name("Sum");
    //方法名长度
    uint32_t method_len = method_name.length();
    uint32_t method_len_net = htonl(method_len);
    std::string method_len_str(reinterpret_cast<char*>(&method_len_net), sizeof(uint32_t));
    // 创建SumRequest对象，序列化
    SumRequest request;
    request.set_a(a);
    request.set_b(b);
    std::string request_string = request.SerializeAsString();
    // 计算总长度
    size_t request_len = request_string.length();
    size_t total_len = 4 + service_len + 4 + method_len + request_len;
    uint32_t total_len_net = htonl(total_len);
    std::string total_len_str(reinterpret_cast<char*>(&total_len_net), sizeof(uint32_t));
    // 魔数
    uint32_t magic = 0x12345678;
    uint32_t magic_net = htonl(magic);
    std::string magic_str(reinterpret_cast<char*>(&magic_net), sizeof(uint32_t));

    std::string rpc_pack = magic_str + total_len_str + service_len_str + service_name + method_len_str + method_name + request_string;

    // 发送RPC请求(服务器端口8080)
    const char* ip = "127.0.0.1";
    int port = 8080;
    int server_fd = connectToServer(ip, port);
    assert(server_fd >= 0);

    sendAll(rpc_pack, server_fd);

    // 接收服务端响应
    std::string response_string = recvAll(server_fd);

    // 反序列化
    SumResponse response;
    response.ParseFromString(response_string);

    // 输出结果
    std:: cout << "Sum(" << a << "+" << b << ") = " << response.sum() << std:: endl;
}

int connectToServer(const char* ip, int port) {
    // 创建socket，连接服务端
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, ip, &address.sin_addr);
    int connect_errno = connect(socket_fd, (struct sockaddr*)&address, sizeof(address));
    if (connect_errno < 0) {
        std::cout << "connect to server failed" << std::endl;
        close(socket_fd);
        return -1;
    }
    return socket_fd;
}

int sendAll(std::string message, int fd) {
    int sended_bytes = 0;
    int bytes_to_send = message.length();
    while (sended_bytes < bytes_to_send) {
        int bytes_send = send(fd, message.c_str() + sended_bytes, bytes_to_send - sended_bytes, 0);
        if (bytes_send == -1) {
            if (errno == EAGAIN ||  errno == EWOULDBLOCK) {
                // 缓冲区满，不能写了
                continue;
            }
            std::cout << "send massage to server failed" << std::endl;
            return -1;
        } else if (bytes_send == 0){
            // 对端关闭
            std::cout << "connection closed by peer" << std::endl;
            return -1;
        }
        sended_bytes += bytes_send;
    }
    return sended_bytes;
}


std::string recvAll(int fd) {
    Buffer buffer;
    uint32_t magic = 0;
    uint32_t length = 0;
    std::string result;
    while (true) {
        int n = buffer.readFd(fd);
        if (buffer.readableBytes() >= 8 && length == 0){
            // 读完了协议头
            magic = buffer.readInt32();
            length = buffer.readInt32();
        }
        if (buffer.readableBytes() >= 8 + length) {
            // 读完了RPC响应包 读取Protobuf消息体
            result = buffer.retrieveAsString(length);
            break;
        }
        if (n == 0){
            // 对端关闭
            result = "";
            break;
        } else if (n < 0){
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                // readFd error
                result = "";
                break;
            }
            // 缓冲区空了，但数据不够，半包
            continue;
        }
    }
    return result;
}

