#include "../../include/frame_codec.h"
#include "../../include/message_handler.h"
#include <iostream>

using namespace rpc;

void testFramCodecAndMessageHandle(){
    // 1. 创建编解码器
    auto codec = std::make_unique<FrameCodec>();
    // 2. 创建消息处理器
    std::vector<std::vector<uint8_t>> messages; // 报错网络消息
    MessageCallback callback = [&messages](std::shared_ptr<TcpConnection>, const std::vector<uint8_t>& message) {
        messages.push_back(message);
        std::cout << "收到消息，长度: " << message.size() << " 字节" << std::endl;
    };
    MessageHandler handler(std::move(codec), callback);

    // 3. 模拟网络数据接收
    std::string test_msg = "Hello, RPC Framework!";
    std::vector<uint8_t> original_message(test_msg.begin(), test_msg.end());

    // 4. 编码
    auto temp_codec = std::make_unique<FrameCodec>();
    auto frame = temp_codec->encode(original_message);

    std::cout << "原始消息: " << test_msg << std::endl;
    std::cout << "编码帧大小: " << frame.size() << " 字节" << std::endl;

    // 5. 使用handler接收数据，并解码
    auto connection = std::shared_ptr<TcpConnection>(nullptr);
    handler.handleData(connection, frame);

    // 6. handler处理完后，调用callback, 把frame添加至messages
    if (messages.size() == 1) {
        std::string decoded_msg(messages[0].begin(), messages[0].end());
        std::cout << "解码消息: " << decoded_msg << std::endl;
        std::cout << "✓ 协作成功！" << std::endl;
    } else {
        std::cout << "❌ 协作失败！" << std::endl;
    }

}

int main() {
    testFramCodecAndMessageHandle();
    return 0;
}