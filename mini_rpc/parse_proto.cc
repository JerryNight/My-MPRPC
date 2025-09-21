#include <string>
#include <iostream>
#include <fstream>
#include <dirent.h> // struct dirent
#include <string.h> // getline()
#include <sstream> // std::stringstream
#include <algorithm>
#include <regex>  // regex
#include <vector>
#include <unordered_map>
#include <assert.h>

/**
 * 解析proto文件，生成相应的c++代码
 * 
 */


// 参数为文件路径，找到Proto文件
std::string readFullFilePath(const std::string& proto_file_path) {
    // 根据文件路径，读取文件内容到string
    DIR* path_dir = opendir(proto_file_path.c_str());
    if (path_dir == nullptr) {
        std::cout << "Failed to open dir: " << proto_file_path << std::endl;
        assert(path_dir == nullptr);
    }

    // 找到proto文件
    struct dirent* pdirent;
    std::string file_name;
    bool is_find = false;
    while ((pdirent = readdir(path_dir)) != nullptr) {
        file_name = pdirent->d_name;
        if (file_name.rfind(".proto") == file_name.length() - 6) {
            // 找到了proto文件
            is_find = true;
            break;
        }
    }
    if (!is_find) {
        // 该目录下没有proto文件、
        std::cout << "Failed to find proto file in dir: " << proto_file_path << std::endl;
        assert(is_find == false);
    }
    return proto_file_path + "/" + file_name;
}

// 参数问文件名，读取文件内容
std::string readFileContent(const std::string& full_file_path) {
    // 读文件
    std::ifstream ifs(full_file_path);
    if (!ifs.is_open()) {
        std::cout << "Failed to open file: " << full_file_path << std::endl;
        assert(ifs.is_open() == false);
    }

    // 一次性读取
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    return buffer.str();
}

// 解析文件内容
// service <> { ... };
// rpc <> () returns ();
// message <> { ... };
struct RpcMethod {
    std::string method_name;
    std::string request_type;
    std::string response_type;
};
// 包含一个服务，对应的所有rpc方法
std::unordered_map<std::string, std::vector<RpcMethod>> service_map;

void parseFileContent(std::string& content) {
    std::vector<std::string> service_name_vec;
    std::vector<RpcMethod> rpcMethod_vec;

    // 1. 移除注释
    // 移除单行注释
    content = std::regex_replace(content, std::regex("//.*"), "");
    // 移除多行注释
    content = std::regex_replace(content, std::regex("/\\*.*?\\*/"), "");
    
    // 2. 匹配所有服务块
    std::regex service_regex(R"(service\s+(\w+)\s*\{([\s\S]*?)\})");
    std::smatch service_match;
    std::string::const_iterator search_start(content.cbegin());

    while (std::regex_search(search_start, content.cend(), service_match, service_regex)) {
        std::string service_name = service_match[1].str();
        std::string service_block = service_match[2].str();

        // 3. 在服务块里，解析所有rpc方法
        std::regex rpc_regex(R"(rpc\s+(\w+)\s*\(\s*(\w+)\s*\)\s*returns\s*\(\s*(\w+)\s*\)\s*;)");
        std::smatch rpc_match;
        std::string::const_iterator rpc_search_start(service_block.cbegin());
        
        while (std::regex_search(rpc_search_start, service_block.cend(), rpc_match, rpc_regex)) {
            RpcMethod method;
            method.method_name = rpc_match[1].str();
            method.request_type = rpc_match[2].str();
            method.response_type = rpc_match[3].str();
            // 添加到map里
            service_map[service_name].push_back(method);
            rpc_search_start = rpc_match.suffix().first;
        }
        search_start = service_match.suffix().first;
    }
}


// 解析proto文件
void parseProto(const std::string& path) {
    std::string proto_full_path = readFullFilePath(path);
    std::string proto_content = readFileContent(proto_full_path);
    parseFileContent(proto_content);
    for (auto it : service_map){
        for (auto i : it.second){
            std::cout << it.first << ":" << i.method_name << " - " << i.request_type << " - " << i.response_type << std::endl;
        }
    }
}


// 测试
int main() {
    parseProto("/home/liyongtao/project/mprpc-master/My-MPRPC/mini_rpc");
}
