#pragma once

#include "tcp_connection.h"
#include <memory>        // 智能指针相关头文件
#include <vector>        // 向量容器头文件
#include <cstdint>       // 固定宽度整数类型头文件
#include <functional>    // 函数对象头文件

namespace rpc {

class TcpConnection;

// 编解码器
class FrameCodec {
public:
    FrameCodec() = default;
    ~FrameCodec() = default;

    // 编码：消息长度+消息正文
    std::vector<uint8_t> encode(const std::vector<uint8_t>& message);

    // 解码：将buffer解码至message
    bool decode(std::vector<uint8_t>& buffer, std::vector<uint8_t>& message);

    // 获取消息长度
    size_t getHeaderSize() const;

private:
    // 32位主机序 -> 网络序
    uint32_t hostToNetwork32(uint32_t value) const;

    // 32位网络序 -> 主机序
    uint32_t networkToHost32(uint32_t value) const;
};

}
