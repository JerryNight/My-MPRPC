# C++ RPC Framework

一个功能完整、高性能的C++ RPC框架，支持服务注册发现、负载均衡、异步调用等特性。

## 整体架构

```
┌───────────────────────────────────────────────────────────────────┐
│                         RPC Framework                             │
│                                                                   │
│  ┌─────────────┐         ┌──────────────┐         ┌────────────┐  │
│  │   Client    │◄───────►│  ZooKeeper   │◄───────►│  Server    │  │
│  │             │         │   Registry   │         │            │  │
│  │ calculator_ │         │  (Service    │         │ calculator_│  │
│  │  client     │         │  Discovery)  │         │  server    │  │
│  └──────┬──────┘         └──────────────┘         └──────┬─────┘  │
│         │                                                 │       │
│         │         Direct RPC Call (TCP)                   │       │
│         └─────────────────────────────────────────────────┘       │
│                                                                   │
└───────────────────────────────────────────────────────────────────┘
```

## 项目学习阶段

### 1.链路层 (Link Layer)：整个系统的基石。
目标: 实现一个基于 epoll 的高性能 TCP 服务，能够高效地接收和发送二进制数据。
学习重点: TcpServer 类设计与实现。socket 编程、非阻塞 I/O、epoll 原理与实践。

### 2.协议层 (Protocol Layer)：RPC 系统可以自定义的协议来规范数据格式、或者使用 Protobuf 封装协议。
目标: 定义一个 RPC 协议头，包含请求的类型、方法名、参数长度等元信息。
学习重点: 网络协议设计、报文分包与组包。

### 3.序列化层 (Serialization Layer)：将对象转换为二进制数据。
目标: 封装 Protobuf，使其能更方便地与协议层对接。
学习重点：序列化与反序列化 (Protobuf)

### 4.代理层 (Proxy Layer)：这是 RPC 系统的“门面”，客户端通过代理层像调用本地函数一样调用远程函数。
目标: 对远程调用的封装。低版本（如 3.0.x 到 3.20.x）的 protobuf 支持内置 RPC ，可以使用 Service 关键字，编译器会自动生成一个抽象的服务基类和一个客户端代理类。高版本（3.21 及以后）不会自动生成服务代理。需要明确安装并使用 gRPC 编译器插件来生成 RPC 相关的代码。
学习重点: 学习使用低版本 protobuf 生成服务代理、学习宏的高级用法。

### 5.负载均衡层 (Load Balancer Layer)：实现负载均衡的功能，客户端如何知道去哪台服务器发起调用？
目标: 实现多种负载均衡算法（轮询、加权轮询、最少连接数、一致性哈希），用工厂模式、注册表模式实现多态调用。
学习重点: 负载均衡算法（如轮询、随机）、注册表模式。

### 6.注册中心层 (Registry Center Layer)：服务端如何被发现？
目标: 该项目会实现两种模式：直连模式（客户端直接连接服务器），服务发现模式（客户端先向 zookeeper 查询提供服务的实例，用负载均衡器选择实例，再进行连接）。我们要封装 zookeeper 的方法，实现简单的服务注册与发现机制类实现，让服务提供者能注册自己，服务消费者能找到它们。
学习重点: 分布式系统原理、zookeeper 的 C 语言接口、服务注册与发现模型。

### 7.容错层 (Fault Tolerance Layer)：当服务出现故障时，系统如何应对？
目标: 实现超时、重试等容错机制，提高系统的健壮性。
学习重点: 分布式系统中的容错设计。


## 项目结构（部分）

```
MyRPC/
├── cpp_rpc_framework/
├── include/              # 头文件
│   ├── rpc_server.h     # RPC 服务器
│   ├── rpc_client.h     # RPC 客户端
│   ├── load_balancer.h  # 负载均衡器
│   └── registry.h       # 服务注册中心
│   
├── src/                  # 源文件
│   ├── core/            # 核心实现
│   ├── load_balancer/   # 负载均衡器
│   ├── registry/        # 注册中心
│   └── transport/       # 传输层
│
├── examples/             # 示例程序 
│   ├── rpc_server_demo.cpp    # 服务器示例
│   └── rpc_client_demo.cpp    # 客户端实例
│
└── service/               # 本地服务
│       └── calculator_service.cpp       # 运算器服务
│
└── test/                     # 测试文件（对每个模块进行测试）
│       ├── codec            # 测试传输层
│       ├── transport        # 测试编解码层
│       └── registry         # 测试注册中心层
│
├── README.md           # 本文件
└── CMakeLists.txt      # 编译文件
```

## 快速构建

### 安装依赖

```bash
sudo apt-get install -y \
    build-essential \
    cmake \
    libprotobuf-dev \
    protobuf-compiler \
    libzookeeper-mt-dev

```

### 编译项目

```bash
# 创建构建目录
mkdir build && cd build

# 配置项目
cmake ..

# 编译（使用多核并行编译）
make
```

编译成功后：
```
build/
├── lib/
│   └── libmyrpc.a          # 静态库
└── bin/                     # 示例程序
    ├── rpc_server_demo
    └── rpc_client_demo
```

### 运行 demo

```bash
# 直连模式
./rpc_server_demo server
./rpc_client_demo client
```

```bash
# 服务注册模式
./rpc_server_demo server-registry
./rpc_client_demo client-registry
```

看到这个输出表示服务器启动成功：
```
RPC 服务器已启动！等待客户端连接...
```
看到这个输出表示客户端启动成功：
```
调用成功！
返回结果: 30
```


## myrpc RPC服务完整调用流程图

### 客户端发送流程
```
┌─────────────────────────────────────────────────────────────────┐
│ 1. 用户代码                                                      │
└───────────────────────┬─────────────────────────────────────────┘
                        ↓
    stub.callMethod("Add", request, response)
                        ↓
┌─────────────────────────────────────────────────────────────────┐
│ 2. RpcStubImpl::callMethod()                                    │
│    - 创建RpcRequest结构                                          │
│    - request.service_name = "CalculatorService"                 │
│    - request.method_name = "Add"                                │
│    - request.data = [序列化的AddRequest]                         │
└───────────────────────┬─────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────────────┐
│ 3. RpcProtocolHelper::serializeRequest()                        │
│    - 将RpcRequest转为RpcRequestProto，封装RPC协议                 │
│    - Protobuf序列化                                              │
│    输出: [protobuf二进制数据]                                     │
└───────────────────────┬─────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────────────┐
│ 4. FrameCodec::encode()   【解决粘包/半包 - 发送端】              │
│    - 计算数据长度: len = data.size()                             │
│    - 添加4字节长度前缀（大端序）                                   │
│    输出: [0x00][0x00][0x01][0x2A][protobuf数据...]               │
│           ↑________↑                                             │
│           长度 = 298字节                                          │
└───────────────────────┬─────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────────────┐
│ 5. TcpClient::send()                                            │
│    - 循环发送直到所有数据发送完成                                  │
│    while (total_sent < total_size) {                             │
│        ssize_t sent = ::send(sockfd_, ...);                      │
│        total_sent += sent;                                       │
│    }                                                             │
└───────────────────────┬─────────────────────────────────────────┘
                        ↓
                   [TCP网络传输]
                        ↓
              (可能发生粘包/半包)
```

### 服务端接收流程

```
                   [TCP网络传输]
                        ↓
┌─────────────────────────────────────────────────────────────────┐
│ 1. TcpServer Event Loop                                         │
│    - epoll_wait检测到可读事件                                    │
│    - handleClientRead()                                         │
└───────────────────────┬─────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────────────┐
│ 2. TcpConnection::handleRead()                                  │
│    - recv(sockfd_, buffer, 4096, 0)                             │
│    - 数据暂存到connection的buffer_                               │
│    - 触发消息回调                                                │
└───────────────────────┬─────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────────────┐
│ 3. RpcServer::handleMessage()                                   │
│    - 提交到线程池处理                                             │
│    thread_pool_->submit([...]() {                                │
│        handleRpcRequest(connection, data);                       │
│    });                                                           │
└───────────────────────┬─────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────────────┐
│ 4. FrameCodec::decode()  【解决粘包/半包 - 接收端】               │
│    ① 读取4字节长度前缀                                            │
│       length = (data[0]<<24)|(data[1]<<16)|(data[2]<<8)|data[3] │
│    ② 精确读取length字节的数据                                     │
│       while (read_bytes < length) { ... }                       │
│    输出: [完整的一帧数据]                                         │
└───────────────────────┬─────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────────────┐
│ 5. RpcServer::parseRpcRequest()                                 │
│    - RpcProtocolHelper::parseRequest(data)                      │
│    - Protobuf反序列化 → RpcRequestProto                          │
│    - 转换为RpcRequest结构                                        │
└───────────────────────┬─────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────────────┐
│ 6. RpcServer::callServiceMethod()                               │
│    ① 查找服务: services_["CalculatorService"]                    │
│    ② 查找方法: FindMethodByName("Add")                           │
│    ③ 反序列化业务请求: request->ParseFromArray(data)              │
│    ④ 调用服务: service->CallMethod(method, ...)                  │
│    ⑤ 序列化业务响应: response->SerializeToString()                │
└───────────────────────┬─────────────────────────────────────────┘
                        ↓
       [CalculatorServiceImpl::Add() 执行业务逻辑]
                        ↓
                result = a + b
                        ↓
┌─────────────────────────────────────────────────────────────────┐
│ 7. RpcServer::serializeRpcResponse()                            │
│    - 创建RpcResponse结构                                         │
│    - response.success = true                                    │
│    - response.data = [序列化的AddResponse]                       │
│    - RpcProtocolHelper::serializeResponse()                     │
└───────────────────────┬─────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────────────┐
│ 8. FrameCodec::encode()                                         │
│    - 添加长度前缀                                                │
│    输出: [0x00][0x00][0x00][0x15][RpcResponseProto数据]          │
└───────────────────────┬─────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────────────┐
│ 9. TcpConnection::send()                                        │
│    - 发送响应数据                                                │
└───────────────────────┬─────────────────────────────────────────┘
                        ↓
                   [TCP网络传输]
```

### 客户端接收流程

```
                   [TCP网络传输]
                        ↓
┌─────────────────────────────────────────────────────────────────┐
│ 1. TcpClient Event Loop (异步线程)                               │
│    - epoll_wait检测到可读事件                                    │
│    - handleRead()                                               │
│    - 数据追加到buffer_                                           │
└───────────────────────┬─────────────────────────────────────────┘
                        ↓
           [buffer_: 累积接收的数据]
                        ↓
┌─────────────────────────────────────────────────────────────────┐
│ 2. TcpClient::receive()  【粘包/半包处理核心】                    │
│    ────────────────────────────────────────                     │
│    ① 读取4字节长度前缀:                                           │
│       readExactly(4, length_bytes)                               │
│       - 先从buffer_读取                                           │
│       - 不够则循环recv                                            │
│                                                                  │
│    ② 解析长度:                                                    │
│       length = (bytes[0]<<24)|(bytes[1]<<16)|...                 │
│                                                                  │
│    ③ 读取完整消息:                                                │
│       readExactly(length, data)                                  │
│       - 先从buffer_读取                                           │
│       - 不够则循环recv                                            │
│       - 直到凑够length字节                                        │
│    ────────────────────────────────────────                     │
│    输出: [完整的一帧RpcResponseProto数据]                         │
└───────────────────────┬─────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────────────┐
│ 3. FrameCodec::decode()                                         │
│    - 移除长度前缀（已在receive中处理）                            │
│    - 这里主要做验证                                              │
└───────────────────────┬─────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────────────┐
│ 4. RpcProtocolHelper::parseResponse()                           │
│    - Protobuf反序列化 → RpcResponseProto                         │
│    - 转换为RpcResponse结构                                       │
└───────────────────────┬─────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────────────┐
│ 5. RpcStubImpl::callMethod()                                    │
│    - 检查response.success                                        │
│    - 反序列化业务响应:                                            │
│      response.ParseFromArray(rpc_response.data)                 │
└───────────────────────┬─────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────────────┐
│ 6. 用户代码                                                      │
│    - 得到AddResponse                                             │
│    - result = response.result()                                  │
│    - 打印结果: 10 + 20 = 30                                      │
└─────────────────────────────────────────────────────────────────┘
```


### 服务端流程

```
启动服务器
    │
    ├─► 初始化组件（线程池、序列化器等）
    │
    ├─► 启动 TCP 服务器
    │
    ├─► [可选] 初始化注册中心
    │      │
    │      ├─► 注册所有服务到注册中心
    │      │
    │      └─► 启动心跳线程
    │
    └─► 等待客户端连接
           │
           └─► 处理 RPC 请求
```

### 客户端流程（服务发现模式）

```
创建客户端
    │
    ├─► 设置注册中心和负载均衡器
    │
    └─► 调用 RPC 方法
           │
           ├─► 从注册中心发现服务实例
           │
           ├─► 使用负载均衡器选择实例
           │
           ├─► 连接到选中的实例
           │
           ├─► 发送请求
           │
           ├─► 接收响应
           │
           └─► 更新连接统计
```


## 关键数据结构

### 1. RpcRequest (业务层)

```cpp
struct RpcRequest {
    std::string service_name;      // "CalculatorService"
    std::string method_name;       // "Add"
    std::vector<uint8_t> data;     // Protobuf序列化的AddRequest
    uint64_t request_id;           // 12345（当前时间戳）
    std::string client_id;         // "client-001"
};
```

### 2. RpcRequestProto (协议层)

```protobuf
message RpcRequestProto {
    uint64 request_id = 1;
    string service_name = 2;
    string method_name = 3;
    string client_id = 4;
    bytes request_data = 5;        // 嵌套的Protobuf数据
    map<string, string> metadata = 7;
}
```

如果不想使用 protobuf 定义协议，可以实现自定义的协议，例如下面这样：
```
+----------------+------------------+------------------+------------------+----------------+
| Header Length  | Service Name Len | Method Name Len  | Request ID       | Data Length    |
|   (4 bytes)    |   (4 bytes)      |   (4 bytes)      |   (8 bytes)      |   (4 bytes)    |
+----------------+------------------+------------------+------------------+----------------+
| Service Name   | Method Name      | Request Data (Protobuf binary)                      |
|   (variable)   |   (variable)     |   (variable)                                        |
+----------------+------------------+-----------------------------------------------------+
```
1. **Header Length (4 bytes)**: 元数据部分的总长度（不包括Data部分）
2. **Service Name Length (4 bytes)**: 服务名长度
3. **Method Name Length (4 bytes)**: 方法名长度
4. **Request ID (8 bytes)**: 请求ID
5. **Data Length (4 bytes)**: Protobuf数据长度
6. **Service Name (variable)**: UTF-8编码的服务名
7. **Method Name (variable)**: UTF-8编码的方法名
8. **Request Data (variable)**: Protobuf序列化的二进制数据

### 3. 网络传输格式

```
┌────────────┬─────────────────────────────────────────┐
│  Length    │      RpcRequestProto (Protobuf)         │
│  (4 bytes) │                                         │
│  Big-Endian│  ┌────────────────────────────────┐     │
│            │  │ request_id = 12345             │     │
│            │  │ service_name = "Calculator..." │     │
│            │  │ method_name = "Add"            │     │
│            │  │ request_data = [AddRequest{    │     │
│            │  │                  a=10, b=20    │     │
│            │  │                }]              │     │
│            │  └────────────────────────────────┘     │
└────────────┴─────────────────────────────────────────┘
```


## 🚀 特性

- **高性能传输层**: 支持同步通信或异步通信
- **协议缓冲**: 使用Google Protocol Buffers进行序列化
- **服务注册发现**: 使用Zookeeper支持服务注册和动态发现
- **负载均衡**: 支持轮询、随机、加权轮询等策略
- **异步调用**: 支持同步和异步RPC调用
- **线程池**: 内置线程池处理并发请求
- **模块化设计**: 清晰的架构分层，易于扩展