#include <iostream>
#include <string>
#include <sstream>
#include <unordered_map>
#include <vector>

/**
 * 利用解析出来的server_map，生成服务类
 */

struct RpcMethod {
    std::string method_name;
    std::string request_type;
    std::string response_type;
};
// 包含一个服务，对应的所有rpc方法
std::unordered_map<std::string, std::vector<RpcMethod>> service_map;

void generateRpcServiceClientStub(std::unordered_map<std::string, std::vector<RpcMethod>>& service_map) {
    std::stringstream ss;
    
    // 包含头文件
    ss << "#include \"rpcclient.h\"\n";
    ss << "#include \"sum.pb.h\"\n\n";

    // 生成stub类
    for (auto service : service_map) {
        ss << "class " << service.first << "RpcClient {\n";
        ss << "public:\n";
        ss << "    " << service.first << "RpcClient(RpcClient* client):client_(client){}\n";

        // 生成服务内的所有方法
        for (auto method : service.second) {
            ss << "    " << method.response_type << " " << method.method_name << "(" << method.request_type << "& request) {\n";
            ss << "        " << method.response_type << " response;\n";
            ss << "        " << "client_->CallMethod(\"" << service.first << "\", \"" 
                             << method.method_name << "\", " << "request, response);\n";
            ss << "        " << "return response;\n";
            ss << "    }\n\n";
        }

        ss << "private:\n";
        ss << "    RpcClient* client_;\n";
        ss << "};\n\n";
    }

    // 打印看看
    std::cout << ss.str() << std::endl;
}

void generateRpcService(std::unordered_map<std::string, std::vector<RpcMethod>>& service_map) {
    std::stringstream ss;

    // 1. 包含头文件
    ss << "#include \"rpcclient.h\"\n";
    ss << "#include \"sum.pb.h\"\n";
    ss << "#include <google/protobuf/service.h>\n\n";

    // 2. 遍历service_map,为每个服务生成一个骨架类
    for (auto service : service_map) {
        const std::string service_name = service.first;
        const auto& methods = service.second;

        ss << "class " << service_name << " : public RpcService {\n";
        ss << "public:\n";

        // 3. 生成所有虚方法
        for (auto method : methods) {
            ss << "    " << "virtual void " << method.method_name << "(\n"
               << "        " << "google::protobuf::RpcController* controller,\n"
               << "        " << "const " << method.request_type << "* request,\n"
               << "        " << method.response_type << "* response,\n"
               << "        " << "google::protobuf::Closure* done) = 0;\n\n";
        }

        // 4. 实现通用的 CallMethod 分发器
        ss << "// CallMethod 分发器由框架调用，分发请求到具体的虚方法\n";
        ss << "    " << "void CallMethod(\n"
           << "        " << "const google::protobuf::MethodDescriptor* method,\n"
           << "        " << "google::protobuf::RpcController* controller,\n"
           << "        " << "const google::protobuf::Message* request,\n"
           << "        " << "google::protobuf::Message* response,\n"
           << "        " << "google::protobuf::Closure* done) {";
        
        // 5. 判断传入的method，类型转换，调用具体的方法
        for (auto item : methods) {
            ss << "        " << "if (method->name() == \"" << item.method_name << "\") {\n"
               << "            " << "// 动态类型转换，调用具体方法\n"
               << "            " << item.method_name << "\n"
               << "            " << "(controller,\n"
               << "            " << "static_cast<const " << item.request_type << "*>(request),\n"
               << "            " << "static_cast<" << item.response_type << "*>(response),\n"
               << "            " << "done);\n"
               << "        " << "}\n";
        }

        ss << "    " << "};\n\n"; // CallMethod结束
        ss << "};\n\n"; // 类结束
    }

    // 输出看一下
    std::cout << ss.str() << std::endl;
}