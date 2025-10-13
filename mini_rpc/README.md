## 核心阶段与学习路径
### 1.链路层 (Link Layer)：整个系统的基石。
目标: 实现一个基于 epoll 的高性能 TCP 服务，能够高效地接收和发送二进制数据。
学习重点: 序列化与反序列化 (Protobuf)，TcpServer 类设计与实现。socket 编程、非阻塞 I/O、epoll 原理与实践。

### 2.协议层 (Protocol Layer)：我们的 RPC 系统需要一个自定义的协议来规范数据格式。
目标: 定义一个 RPC 协议头，包含请求的类型、方法名、参数长度等元信息。
学习重点: 网络协议设计、报文分包与组包。

### 3.序列化层 (Serialization Layer)：将对象转换为二进制数据。
目标: 封装 Protobuf，使其能更方便地与协议层对接。

### 4.代理层 (Proxy Layer)：这是 RPC 系统的“门面”，客户端通过代理层像调用本地函数一样调用远程函数。
目标: 使用 C++ 模板和宏来生成代理类，实现对远程调用的封装。
学习重点: C++ 模板元编程、宏的高级用法。

### 5.路由层 (Routing Layer)：客户端如何知道去哪台服务器发起调用？
目标: 实现一个客户端路由策略，比如简单的负载均衡。
学习重点: 负载均衡算法（如轮询、随机）。

### 6.注册中心层 (Registry Center Layer)：服务端如何被发现？
目标: 实现一个简单的服务注册与发现机制，让服务提供者能注册自己，服务消费者能找到它们。
学习重点: 分布式系统原理、服务注册与发现模型。

### 7.容错层 (Fault Tolerance Layer)：当服务出现故障时，系统如何应对？
目标: 实现超时、重试等容错机制，提高系统的健壮性。
学习重点: 分布式系统中的容错设计。

### 协议层
一个完整的RPC协议，应包含：
1. 协议头：简单的协议头设计，可以是一个魔数+一个数据长度（`magic`+`length`）
2. 服务名长度：一个4字节整数，表示服务名的长度
3. 服务名：一个字符串
4. 方法名长度：一个4字节整数，表示方法名的长度
5. 方法名：一个字符串
6. Protobuf 消息体：序列化后的数据

### 序列化层
#### 使用Protoc生成RPC代码
由于protobuf版本较低，需要手动实现服务类：服务基类 + 服务实现类
重写 `CallMethod` 方法：
> `CallMethod`封装了服务函数的具体实现，是底层网络和上层业务逻辑的唯一入口，由上层的`TcpServer`调用
> `CallMethod`接收泛型`Message`指针，要进行类型转换

### 代理层
#### 代码生成器：解析 proto 文件，自动生成 c++ 文件
一个通用的RPC框架，需要一个够能-自动生成服务实现者代码-的工具。
代码生成器的工作流程：
1. 读取 `Proto` 文件
2. 解析文件，解析出服务名、方法名、请求类型、响应类型
3. 生成 c++ 代码文件，包含处理网络通信的通用逻辑。

首先要理解三个角色：服务端开发者，RPC框架，客户端开发者。
RPC框架会根据`.proto`文件里定义的服务，分别生成一份客户端代理类（给客户端开发者使用），和一份服务端代理类（给服务端开发者使用）。
-代码生成器-属于RPC框架，目的就是解析`.proto`文件内容，生成客户端和服务端的代理类。

我们的目标是让用户像调用本地函数一样，调用远程方法。因此，需要一个代理类`stub`。
代理类会为`.proto`文件中定义的每个RPC方法，生成一个对应的包装函数。
假设我现在是客户端，我想要调用一个rpc method，那么，我就要创建一个代理类对象，调用代理类对象的method.

客户端用户会这么使用RPC框架
```
RpcClient client("127.0.0.1", 8080);
// 创建代理类对象
CalculatorServiceRpcClient stub(&client);

SumRequest request;
request.set_a(10);
request.set_b(20);

// 用户像调用本地方法一样，调用RPC方法
SumResponse response = stub.sum(request);

std::cout << "10 + 20 = " << response.sum() << std::endl;
```

代码生成器自动生成的客户端代理类，像这个样子：
```
SumResponse CalculatorServiceRpcClient::Sum(const SumRequest& request) {
    SumResponse response;
    // 调用我们之前编写的通用 RPC 调用函数
    client_->CallMethod("CalculatorService", "Sum", request, response);
    return response;
}
```

服务端用户会这么使用RPC框架
```
// 继承服务基类，编写服务实现类
class CalculatorServiceImpl : public CalculatorService {
public:
    // 重写rpc方法，实现业务逻辑
    void Sum(const SumRequest* request, SumResponse* response) override {
        // 业务逻辑...
    }
}
```

代码生成器自动生成的服务端代理类，像这个样子：
```
// RpcServer是服务基类
class CalculatorService : public RpcService {
public:
    // 服务端开发者需要重写的业务方法
    virtual void Sum(const SumRequest* request, SumResponse* response) = 0;
    // ... 其他方法
}
```

### 注册中心 Registry
注册中心只提供服务注册和服务发现的功能
服务注册：服务器向注册中心注册服务，告诉注册中心，我可以提供xx服务
服务发现：客户端向注册中心查询服务列表，然后，客户端自己选择一个健康的服务器地址，请求服务

### protobuf 序列化原理
protobuf使用 标签-类型-值（Tag-Type-Value）编码。
`string name = 1;`
标签是指唯一数字`1`
字段类型是`string`，编码器会根据类型选择合适的编码方式
值是字段的实际数据
编码时，protobuf不会存储字段名(name)，而是将标签和类型打包成一个数字，然后紧跟着实际的值。
例如：`int32 age = 1;` `age`的值为25
编码后的字节流像这样：`|1000|25|`；第一个字节`1000`包含了标签`1`和类型信息，第二个字节是实际的值。