#include "../../include/message_handler.h"
#include <arpa/inet.h>        // 网络地址转换头文件
#include <cstring>            // 字符串操作头文件
#include <iostream>

namespace rpc{

MessageHandler::MessageHandler(std::unique_ptr<FrameCodec> codec, MessageCallback callback)
    :codec_(std::move(codec)),
     message_callback_(callback) {}

// 处理接收到的数据
void MessageHandler::handleData(std::shared_ptr<TcpConnection> connection, const std::vector<uint8_t> &data) {
    if (data.empty()) {
        return;
    }

    receive_buffer_.insert(receive_buffer_.end(), data.begin(), data.end());

    while (true) {
        std::vector<uint8_t> message;

        if (codec_->decode(receive_buffer_, message)) {
            // 解码成功
            if (message_callback_) {
                message_callback_(connection, message);
            }
        } else {
            break;
        }
    }

}

}