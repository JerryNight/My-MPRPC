#include "../../include/calculator_service.h"
#include <stdexcept>


namespace rpc {


void CalculatorServiceImpl::Add(::google::protobuf::RpcController* controller,
                       const ::AddRequest* request,
                       ::AddResponse* response,
                       ::google::protobuf::Closure* done) 
{
    try {
        int32_t result = request->a() + request->b();
        response->set_result(result);
    
        if (done) {
            done->Run();
        }
    } catch (const std::exception& e) {
        if (controller) {
            controller->SetFailed("Add operation failed: " + std::string(e.what()));
        }
        if (done) {
            done->Run();
        }
    }
}

void CalculatorServiceImpl::Sub(::google::protobuf::RpcController* controller,
                       const ::SubRequest* request,
                       ::SubResponse* response,
                       ::google::protobuf::Closure* done) 
{
    int32_t result = request->a() - request->b();
    response->set_response(result);
    if (done) {
        done->Run();
    }
}

void CalculatorServiceImpl::Mul(::google::protobuf::RpcController* controller,
                       const ::MultiRequest* request,
                       ::MultiResponse* response,
                       ::google::protobuf::Closure* done) 
{
    int32_t result = request->a() * request->b();
    response->set_response(result);
    if (done) {
        done->Run();
    }
}

void CalculatorServiceImpl::Div(::google::protobuf::RpcController* controller,
                       const ::DivideRequest* request,
                       ::DivideResponse* response,
                       ::google::protobuf::Closure* done) 
{
    if (request->b() == 0) {
        response->set_result(-1.0);
    }
    double result = static_cast<double>(request->a()) / static_cast<double>(request->b());
    response->set_result(result);
    if (done) {
        done->Run();
    }
}

}