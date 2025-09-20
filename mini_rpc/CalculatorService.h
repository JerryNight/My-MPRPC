#include "sum.pb.h"
#include <google/protobuf/service.h>
#include <string>
#include <iostream>


/**
 * 在高版本的protobuf里，这个文件会自动生成在sum.pb.h中，
 * 现在，要创建一个服务基类
 */

class CalculatorService {
public:
    // 通用的RPC方法，由RPC框架调用
    virtual void CallMethod(const google::protobuf::MethodDescriptor* method,
                          google::protobuf::RpcController* controller, 
                          const google::protobuf::Message* request,
                          google::protobuf::Message* response, 
                          google::protobuf::Closure* done) = 0;

    // 定义自己的RPC方法
    virtual void Sum(google::protobuf::RpcController* controller,
                     const SumRequest* request,
                     SumResponse* response,
                     google::protobuf::Closure* done) = 0;

};

// 服务实现类
class CalculatorServiceImpl : public CalculatorService {
public:
    virtual void Sum(google::protobuf::RpcController* controller,
                     const SumRequest* request,
                     SumResponse* response,
                     google::protobuf::Closure* done) override
    {
        int a = request->a();
        int b = request->b();
        int result = a + b;
        response->set_sum(result);
        std::cout << "Sum called: " << request->a() << " + " << request->b() << " = " << result << std::endl;
    }

    virtual void CallMethod(const google::protobuf::MethodDescriptor* method,
                          google::protobuf::RpcController* controller, 
                          const google::protobuf::Message* request,
                          google::protobuf::Message* response, 
                          google::protobuf::Closure* done) override
    {
        // 检查method是否为sum
        std::string method_name = method->name();
        if (method_name == "Sum") {
            // 类型转换
            const SumRequest* req = static_cast<const SumRequest*>(request);
            SumResponse* res = static_cast<SumResponse*>(response);
            // 调用Sum
            Sum(controller, req, res, done);
        }
    }
};