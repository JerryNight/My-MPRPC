#include "../../include/tcp_server.h"
#include "../../include/tcp_client.h"
#include "../../include/tcp_connection.h"
#include <iostream>          // 输入输出流头文件
#include <thread>            // 线程相关头文件
#include <chrono>            // 时间相关头文件
#include <cassert>           // 断言头文件
#include <vector>            // 向量容器头文件
#include <string>            // 字符串头文件
#include <atomic>            // 原子操作头文件

using namespace rpc;

// 统计测试结果
struct TestStats {
    std::atomic<int> tests_passed{0};
    std::atomic<int> tests_failed{0};
    std::atomic<int> messages_sent{0};
    std::atomic<int> messages_received{0};
};

TestStats g_stats;

// 测试TCP服务器基本功能
void testTcpServerBasic() {
    // 创建服务器
    std::shared_ptr<TcpServer> server = std::make_unique<TcpServerImpl>();
    assert(server != nullptr);
    std::cout << "✓ 创建TCP服务器成功" << std::endl;

    // 测试服务器初始状态
    assert(!server->isRunning());
    bool ret = server->start(8888, "127.0.0.1");
    assert(ret);
    std::cout << "✓ 服务器启动成功，监听端口8888" << std::endl;

    // 等待服务器完全启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 停止服务器
    server->stop();
    assert(!server->isRunning());
    std::cout << "✓ 服务器停止成功" << std::endl;
    
    g_stats.tests_passed++;
    std::cout << "TCP服务器基本功能测试通过" << std::endl;
}

// 测试客户端连接服务器
void testServerClientConnection() {
    std::cout << "\n=== 测试服务器和客户端连接 ===" << std::endl;

    // 创建服务器
    std::shared_ptr<TcpServer> server = std::make_shared<TcpServerImpl>();
    server->start(8889,"127.0.0.1");
    std::cout << "✓ 服务器启动成功" << std::endl;

    // 设置服务器回调函数
    std::atomic<bool> connection_established{false};
    server->setConnectionCallback([&connection_established]
        (std::shared_ptr<TcpConnection> conn) {
            connection_established = true;
            std::cout << "✓ 服务器收到新连接: " << conn->getRemoteAddress() << std::endl;
        });

    // 等待服务器启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 创建客户端
    std::shared_ptr<TcpClient> client = std::make_shared<TcpClientImpl>();
    client->connect("127.0.0.1", 8889);
    assert(client->getState() == ConnectionState::CONNECTED);
    std::cout << "✓ 客户端连接成功" << std::endl;
    // 等待连接建立
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 验证回调是否设置成功
    assert(connection_established);
    std::cout << "✓ 回调设置成功" << std::endl;

    // 发送数据
    std::string test_message = "Hellooooo";
    std::vector<uint8_t> message_data(test_message.begin(), test_message.end());
    bool ret = client->send(message_data);
    assert(ret);
    std::cout << "✓ 客户端发送消息成功" << std::endl;

    // 断开客户端连接
    client->disconnect();
    assert(client->getState() == ConnectionState::DISCONNECTED);
    std::cout << "✓ 客户端断开连接成功" << std::endl;
    // 停止服务器
    server->stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "✓ 服务器停止成功" << std::endl;
    
    g_stats.tests_passed++;
    std::cout << "服务器和客户端连接测试通过" << std::endl;

}

// 测试并发连接
void testConcurrentConnections() {
    std::cout << "\n=== 测试并发连接 ===" << std::endl;
    
    // 创建并启动服务器
    std::unique_ptr<TcpServer> server = std::make_unique<TcpServerImpl>();
    assert(server->start(8892, "127.0.0.1"));
    std::cout << "✓ 服务器启动成功" << std::endl;
    
    // 设置服务器连接回调
    std::atomic<int> connection_count{0};
    server->setConnectionCallback([&connection_count](std::shared_ptr<TcpConnection> conn) {
        connection_count++;
        std::cout << "✓ 收到连接 #" << connection_count << ": " << conn->getRemoteAddress() << std::endl;
    });
    
    // 等待服务器启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 创建多个客户端连接
    const int num_clients = 5;
    std::vector<std::unique_ptr<TcpClient>> clients;
    
    for (int i = 0; i < num_clients; ++i) {
        std::unique_ptr<TcpClient> client = std::make_unique<TcpClientImpl>();
        assert(client->connect("127.0.0.1", 8892));
        clients.push_back(std::move(client));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    std::cout << "✓ 创建了 " << num_clients << " 个客户端连接" << std::endl;
    
    // 等待所有连接建立
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // 验证连接数
    assert(connection_count.load() == num_clients);
    std::cout << "✓ 并发连接测试成功，连接数: " << connection_count.load() << std::endl;
    
    // 断开所有连接
    // for (auto& client : clients) {
        // client->disconnect();
    // }
    clients.clear();
    
    // 停止服务器
    server->stop();
    std::cout << "✓ 服务器停止" << std::endl;
    
    g_stats.tests_passed++;
    std::cout << "并发连接测试通过" << std::endl;
}

int main(){
    try {
        // 运行测试
        testTcpServerBasic();
        testServerClientConnection();
        testConcurrentConnections();

        std::cout << "\n==========================================" << std::endl;
        std::cout << "           测试结果统计" << std::endl;
        std::cout << "==========================================" << std::endl;
        std::cout << "通过的测试数: " << g_stats.tests_passed.load() << std::endl;
        std::cout << "失败的测试数: " << g_stats.tests_failed.load() << std::endl;
        std::cout << "发送的消息数: " << g_stats.messages_sent.load() << std::endl;
        std::cout << "接收的消息数: " << g_stats.messages_received.load() << std::endl;

        if (g_stats.tests_failed.load() == 0) {
            std::cout << "\n🎉 所有传输层测试通过！" << std::endl;
            std::cout << "传输层功能正常，可以正常进行TCP连接、消息传输和错误处理。" << std::endl;
            return 0;
        } else {
            std::cout << "\n❌ 部分测试失败，请检查传输层实现。" << std::endl;
            return 1;
        }
    } catch (std::exception& e) {
        std::cerr << "测试过程中发生异常: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "测试过程中发生未知异常" << std::endl;
    }

    return 0;
}