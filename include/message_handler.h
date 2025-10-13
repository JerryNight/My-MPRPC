#pragma once

#include "frame_codec.h"
#include <memory>        // 智能指针相关头文件
#include <vector>        // 向量容器头文件
#include <cstdint>       // 固定宽度整数类型头文件
#include <functional>    // 函数对象头文件

namespace rpc {

// 接收完数据，并解析完成后，调用该回调。
using MessageCallback = std::function<void(std::shared_ptr<TcpConnection>, const std::vector<uint8_t>&)>;

// 前向声明
class TcpConnection;

class MessageHandler {
public:
    explicit MessageHandler(std::unique_ptr<FrameCodec> codec, MessageCallback callback);
    ~MessageHandler() = default;

    // 处理接收到的数据
    void handleData(std::shared_ptr<TcpConnection> connection, const std::vector<uint8_t>& data);

    // 设置回调
    void setMessageCallback(MessageCallback callback) {
        message_callback_ = std::move(callback);
    }

private:
    std::unique_ptr<FrameCodec> codec_; // 编解码器
    std::vector<uint8_t> receive_buffer_; // 接收缓冲区:保存网络数据
    MessageCallback message_callback_; // 回调

};
    
}