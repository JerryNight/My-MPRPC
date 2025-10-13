#include "rpcclient.h"
#include "sum.pb.h"
#include "rpcservice.h"
#include <google/protobuf/service.h>

class CalculatorServiceRpcClient {
public:
    CalculatorServiceRpcClient(RpcClient* client):client_(client){}
    SumResponse sum(SumRequest& request) {
        SumResponse response;
        client_->CallMethod("CalculatorService", "sum", request, response);
        return response;
    }

    DevideResponse Devide(DevideRequest& request) {
        DevideResponse response;
        client_->CallMethod("CalculatorService", "Devide", request, response);
        return response;
    }
private:
    RpcClient* client_;
};

// 骨架类
class CalculatorService : RpcService {
    // 定义rpc方法
    virtual void Sum(const SumRequest* request,
                     SumResponse* response) = 0;

    virtual void Sum(google::protobuf::RpcController* controller,
                     const SumRequest* request,
                     SumResponse* response,
                     google::protobuf::Closure* done) = 0;
    
    virtual void Devide(const SumRequest* request,
                        SumResponse* response) = 0;
                        
    // 通用的RPC方法，由RPC框架调用
    virtual void CallMethod(const google::protobuf::MethodDescriptor* method,
                          google::protobuf::RpcController* controller, 
                          const google::protobuf::Message* request,
                          google::protobuf::Message* response, 
                          google::protobuf::Closure* done) = 0;
};