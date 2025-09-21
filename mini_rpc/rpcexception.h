#include <string>
#include <stdexcept>

class RpcException : public std::runtime_error {
public:
    explicit RpcException(const std::string& message)
        :std::runtime_error(message) 
        {}

};