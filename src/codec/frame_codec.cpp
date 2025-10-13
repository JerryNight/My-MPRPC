#include "../../include/frame_codec.h"
#include <arpa/inet.h>        // 网络地址转换头文件
#include <cstring>            // 字符串操作头文件
#include <iostream>

namespace rpc {

// 编码：消息长度+消息正文
std::vector<uint8_t> FrameCodec::encode(const std::vector<uint8_t> &message) {
    if (message.empty()) {
        return {};
    }

    uint32_t message_length = static_cast<uint32_t>(message.size()); // 获取数据头
    uint32_t network_length = hostToNetwork32(message_length); // 转为网络序

    std::vector<uint8_t> frame; // 创建数据帧，准备组装数据
    frame.reserve(sizeof(network_length) + message.size());

    // 添加消息前缀
    const uint8_t* length_bytes = reinterpret_cast<const uint8_t*>(&network_length);
    frame.insert(frame.end(), length_bytes, length_bytes + sizeof(network_length));

    // 添加消息正文
    frame.insert(frame.end(), message.begin(), message.end());

    return frame;
}

// 解码：将buffer解码至message
bool FrameCodec::decode(std::vector<uint8_t> &buffer, std::vector<uint8_t> &message) {
    if (buffer.size() < getHeaderSize()) {
        return false;
    }

    // 先读消息头
    uint32_t network_length;
    std::memcpy(&network_length, buffer.data(), getHeaderSize());
    uint32_t message_length = networkToHost32(network_length);

    // 检查消息长度
    if (message_length > 1024 * 1024 * 100) { // 消息大于100M
        std::cerr << "Message too large: " << message_length << " bytes" << std::endl;
        return false;
    }
    size_t total_frame_size = getHeaderSize() + message_length;
    if (buffer.size() < total_frame_size) { // 数据不完整
        return false;
    }

    // 读消息正文
    message.clear();
    message.reserve(message_length);
    message.insert(message.end(), buffer.begin() + getHeaderSize(), buffer.begin() + total_frame_size);

    buffer.erase(buffer.begin(), buffer.begin() + total_frame_size);

    return true;
}

// 获取消息长度(4字节)
size_t FrameCodec::getHeaderSize() const {
    return sizeof(uint32_t);
}

// 32位主机序 -> 网络序
uint32_t FrameCodec::hostToNetwork32(uint32_t value) const {
    return htonl(value);
}

// 32位网络序 -> 主机序
uint32_t FrameCodec::networkToHost32(uint32_t value) const {
    return ntohl(value);
}

}