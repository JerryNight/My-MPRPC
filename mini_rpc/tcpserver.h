#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <assert.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <unordered_map>
#include <memory>
#include "sum.pb.h"
#include "buffer.h"

// 回调函数；Message cb(Message msg);
using MessageCallback = std::function<std::shared_ptr<google::protobuf::Message> (std::shared_ptr<google::protobuf::Message>)>;

// 读取数据-状态机
enum ReadingState {
    READING_HEADER,
    READING_BODY
};

// conn连接
typedef struct Connection {
    // 这里用share_ptr：因为conn对象如果发生拷贝，buffer还是裸指针，他们指向同一个。当一个conn销毁，另一个conn中的buffer指针就悬空了。
    std::shared_ptr<Buffer> buffer;
    ReadingState state;
    uint32_t body_length;
} Connection;

class TcpServer {
public:
    // 构造函数、接收 IP 和端口
    TcpServer(const std::string& ip, int port);
    ~TcpServer();

    // 启动服务
    void start();
    // 停止服务
    void stop();
    // 设置回调
    void setMessageCallback(MessageCallback cb);

private:
    // epoll 文件
    int m_epollfd;
    // socket 文件
    int m_listenfd;
    // 保存每个连接的conn对象
    std::unordered_map<int, std::shared_ptr<Connection>> m_client_map;
    // 回调函数对象
    MessageCallback m_callback;
    // 运行/关闭
    bool m_running;

private:

    // 处理事件循环
    void handleEvents();
    // 处理新连接
    void handleNewConnection();
    // 处理数据读写
    void handleRead(int client_fd);
    void handleWrite(int client_fd);
    // 删除连接
    void removeClient(int client_fd);
    // 设置非阻塞
    void setNonblocking(int fd);
};

TcpServer::TcpServer(const std::string& ip, int port):m_running(true){
    // 创建socket、bind、listen、epoll
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    setNonblocking(listen_fd);
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &address.sin_addr);
    int bindErrno = bind(listen_fd, (struct sockaddr*)&address, sizeof(address));
    int listenErrno = listen(listen_fd, 1024);
    assert(listenErrno >= 0);

    int epoll_fd = epoll_create1(0);
    struct epoll_event event;
    event.data.fd = listen_fd;
    event.events = EPOLLIN | EPOLLET; // 边缘触发
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &event);

    m_epollfd = epoll_fd;
    m_listenfd = listen_fd;
}
TcpServer::~TcpServer(){
    close(m_epollfd);
    close(m_listenfd);
}

// 启动服务
void TcpServer::start(){
    handleEvents();
}
// 停止服务
void TcpServer::stop(){
    m_running = false;
}
// 设置回调
void TcpServer::setMessageCallback(MessageCallback cb){
    m_callback = std::move(cb);
}
// 处理事件循环
void TcpServer::handleEvents(){
    struct epoll_event events[1024];
    while (m_running) {
        int num_events = epoll_wait(m_epollfd, events, 1024, -1);
        for (int i = 0; i < num_events; ++i) {
            int fd = events[i].data.fd;
            if (fd == m_listenfd) {
                handleNewConnection();
            } else if (events[i].events & EPOLLIN) {
                handleRead(fd);
            } else if (events[i].events & EPOLLOUT) {
                handleWrite(fd);
            }
        }
    }
}
// 处理新连接
void TcpServer::handleNewConnection(){
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    int conn_fd = accept(m_listenfd, (struct sockaddr*)&client_addr,&len);
    assert(calloc >= 0);

    struct epoll_event event;
    event.data.fd = conn_fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(m_epollfd, EPOLL_CTL_ADD, conn_fd, &event);
    setNonblocking(conn_fd);

    auto conn = std::make_shared<Connection>();
    conn->buffer = std::make_shared<Buffer>();
    conn->state = READING_HEADER;
    m_client_map[conn_fd] = conn;
}

// 处理数据读写
void TcpServer::handleRead(int client_fd) {
    auto it = m_client_map.find(client_fd);
    if (it == m_client_map.end()) {
        removeClient(client_fd);
        return;
    }
    std::shared_ptr<Connection> conn = it->second;

    // 调用buffer的readFd来读数据
    size_t n = conn->buffer->readFd(client_fd);
    if (n > 0) {
        // 解析数据，读完协议头了，才能解析，否则继续等待数据
        while (conn->buffer->readableBytes() >= 8) {  // 头部是否足够
            // 1. 解析协议头
            uint32_t magic = conn->buffer->readInt32();
            uint32_t length = conn->buffer->readInt32();
            conn->body_length = length;            

            // 解析数据
            if (conn->buffer->readableBytes() >= length + 8) {  // 正文是否足够
                // 完整接收了一个RPC包
                conn->buffer->retrieve(8); // 2. 消费头部
                // 3. 解析服务名、方法名
                uint32_t service_len = conn->buffer->readInt32();
                std::string service_name = conn->buffer->retrieveAsString(service_len);
                uint32_t method_len = conn->buffer->readInt32();
                std::string method_name = conn->buffer->retrieveAsString(method_len);

                // 4. 获取protobuf消息体
                uint32_t pro_body_len = conn->body_length - service_len - method_len - 8;
                std::string proto_body = conn->buffer->retrieveAsString(pro_body_len);

                // 5. 反序列化 Protobuf 消息，调用服务。假设已经注册并能够找到 CalculatorServiceImpl 实例
                // SumRequest request;
                // request.ParseFromString(proto_body);
                // SumResponse response;
                // ... 调用 CalculatorServiceImpl::CallMethod ...

                // 6. 处理完了，清空缓冲区，重置状态
                conn->buffer->retrieve(length);
            } else {
                // 数据没读完、半包
                break;
            }
        }
    } else if (n == 0) {
        // 客户端断开连接
        removeClient(client_fd);
    } else if (n < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            // readv error
            removeClient(client_fd);
        }
    }

}

void TcpServer::handleWrite(int client_fd){

}

// 删除连接
void TcpServer::removeClient(int client_fd){
    epoll_ctl(m_epollfd, EPOLL_CTL_DEL, client_fd, nullptr);
    close(client_fd);
    m_client_map.erase(client_fd);
}

void TcpServer::setNonblocking(int fd) {
    int flag = fcntl(fd, F_GETFL,0);
    fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}


// 用状态机和vector<char>版本的headleRead(int client_fd)
// void TcpServer::handleRead(int client_fd) {
//     auto it = m_client_map.find(client_fd);
//     if (it == m_client_map.end()) {
//         // 没有该连接
//         std::cout << "[TcpServer] Client doesn't exist!" << std::endl;
//         return;
//     }
//     Connection & conn = it->second;

//     char buf[1024] = {0};
//     // 循环读取
//     while (true) {
//         if (conn.state == READING_HEADER) {
//             int bytes_read = read(client_fd, buf, TcpHeaderLength);
//             if (bytes_read > 0) {
//                 conn.buffer.insert(conn.buffer.end(), buf, buf + bytes_read);
//                 if (conn.buffer.size() == TcpHeaderLength) {
//                     // 读完了协议头，解析，更新状态
//                     uint32_t body_length_net;
//                     memcpy(&body_length_net, conn.buffer.data() + 4, 4);
//                     conn.body_length = ntohl(body_length_net);
//                     conn.state = READING_BODY;
//                 } else {
//                     // 没读完
//                     continue;
//                 }
//             } else if (bytes_read == 0) {
//                 // 对端断开连接了
//                 break;
//             } else if (bytes_read < 0) {
//                 if (errno == EAGAIN || errno == EWOULDBLOCK) {
//                     // 没有数据了
//                     break;
//                 }
//                 // read error
//                 break;
//             }
//         }

//         if (conn.state == READING_BODY) {
//             int bytes_read = read(client_fd, buf, conn.body_length + TcpHeaderLength - conn.buffer.size());
//             if (bytes_read == 0) {
//                 // 断开连接了
//                 break;
//             } else if (bytes_read < 0) {
//                 if (errno == EAGAIN || errno == EWOULDBLOCK) {
//                     // 没有数据了
//                     break;
//                 }
//                 // read error
//                 break;
//             } 
            
//             // 把数据插入到conn.buffer
//             conn.buffer.insert(conn.buffer.end(), buf, buf + bytes_read);
//             if (conn.buffer.size() < conn.body_length + TcpHeaderLength) {
//                 // 没读完
//                 continue;
//             } else if (conn.buffer.size() == conn.body_length + TcpHeaderLength) {
//                 // 读完了，处理

//                 // 处理完，重置
//                 conn.buffer.clear();
//                 conn.state = READING_HEADER;
//             }
//         }
//     }
// }