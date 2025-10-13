#include "sum.pb.h"
#include <google/protobuf/service.h>
#include <string>
#include <iostream>

class RpcService {
public:
    RpcService(){}

    // 通用的RPC方法，由RPC框架调用
    virtual void CallMethod(const google::protobuf::MethodDescriptor* method,
                          google::protobuf::RpcController* controller, 
                          const google::protobuf::Message* request,
                          google::protobuf::Message* response, 
                          google::protobuf::Closure* done) = 0;

};