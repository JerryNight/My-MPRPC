#pragma once

#include "../proto/calculator.pb.h"
#include <google/protobuf/service.h>

namespace rpc {

class CalculatorServiceImpl : public CalculatorService {
public:
    CalculatorServiceImpl() = default;
    ~CalculatorServiceImpl() override = default;

    virtual void Add(::google::protobuf::RpcController* controller,
                       const ::AddRequest* request,
                       ::AddResponse* response,
                       ::google::protobuf::Closure* done) override;
    virtual void Sub(::google::protobuf::RpcController* controller,
                       const ::SubRequest* request,
                       ::SubResponse* response,
                       ::google::protobuf::Closure* done) override;
    virtual void Mul(::google::protobuf::RpcController* controller,
                       const ::MultiRequest* request,
                       ::MultiResponse* response,
                       ::google::protobuf::Closure* done) override;
    virtual void Div(::google::protobuf::RpcController* controller,
                       const ::DivideRequest* request,
                       ::DivideResponse* response,
                       ::google::protobuf::Closure* done) override;
};



}