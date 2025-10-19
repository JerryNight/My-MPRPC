#pragma once

#include "../proto/calculator.pb.h"
#include <google/protobuf/service.h>

namespace rpc {

class CalculatorServiceImpl : public CalculatorService {
public:
    CalculatorServiceImpl() = default;
    ~CalculatorServiceImpl() override = default;

  virtual void Add(::google::protobuf::RpcController* controller,
                       const ::rpc::AddRequest* request,
                       ::rpc::AddResponse* response,
                       ::google::protobuf::Closure* done);
  virtual void Sub(::google::protobuf::RpcController* controller,
                       const ::rpc::SubRequest* request,
                       ::rpc::SubResponse* response,
                       ::google::protobuf::Closure* done);
  virtual void Mul(::google::protobuf::RpcController* controller,
                       const ::rpc::MultiRequest* request,
                       ::rpc::MultiResponse* response,
                       ::google::protobuf::Closure* done);
  virtual void Div(::google::protobuf::RpcController* controller,
                       const ::rpc::DivideRequest* request,
                       ::rpc::DivideResponse* response,
                       ::google::protobuf::Closure* done);
};



}