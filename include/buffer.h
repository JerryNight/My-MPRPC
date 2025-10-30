#pragma once

#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <stdexcept>
#include <arpa/inet.h>
#include <endian.h>

namespace rpc {

/**
 * 动态扩容的Buffer缓冲区
 * 特点：
 * 1. 支持自动扩容
 * 2. 内存连续，读写高效
 * 3. 支持peek操作（不移除数据）
 * 4. 自动回收空闲内存
 */
class Buffer {
public:
    static const size_t kInitialSize = 1024;        // 初始大小 1KB
    static const size_t kPrependSize = 8;           // 预留头部空间
    static const size_t kMaxBufferSize = 64 * 1024 * 1024; // 最大64MB

    explicit Buffer(size_t initial_size = kInitialSize)
        : buffer_(kPrependSize + initial_size)
        , reader_index_(kPrependSize)
        , writer_index_(kPrependSize) {
    }

    ~Buffer() = default;

    // 禁用拷贝，允许移动
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&&) noexcept = default;
    Buffer& operator=(Buffer&&) noexcept = default;

    // 可读字节数
    size_t readableBytes() const {
        return writer_index_ - reader_index_;
    }

    // 可写字节数
    size_t writableBytes() const {
        return buffer_.size() - writer_index_;
    }

    // 预留空间大小
    size_t prependableBytes() const {
        return reader_index_;
    }

    // 获取可读数据指针（不移除）
    const uint8_t* peek() const {
        return &buffer_[reader_index_];
    }

    // 查找CRLF（用于HTTP等协议）
    const uint8_t* findCRLF() const {
        const uint8_t* crlf = std::search(peek(), beginWrite(),
                                          reinterpret_cast<const uint8_t*>("\r\n"),
                                          reinterpret_cast<const uint8_t*>("\r\n") + 2);
        return crlf == beginWrite() ? nullptr : crlf;
    }

    // 移除已读数据
    void retrieve(size_t len) {
        if (len < readableBytes()) {
            reader_index_ += len;
        } else {
            retrieveAll();
        }
    }

    // 移除全部数据
    void retrieveAll() {
        reader_index_ = kPrependSize;
        writer_index_ = kPrependSize;
    }

    // 读取指定长度数据（返回vector）
    std::vector<uint8_t> retrieveAsVector(size_t len) {
        if (len > readableBytes()) {
            len = readableBytes();
        }
        std::vector<uint8_t> result(peek(), peek() + len);
        retrieve(len);
        return result;
    }

    // 读取全部数据
    std::vector<uint8_t> retrieveAllAsVector() {
        return retrieveAsVector(readableBytes());
    }

    // 读取指定长度数据到字符串
    std::string retrieveAsString(size_t len) {
        if (len > readableBytes()) {
            len = readableBytes();
        }
        std::string result(reinterpret_cast<const char*>(peek()), len);
        retrieve(len);
        return result;
    }

    // 读取全部数据到字符串
    std::string retrieveAllAsString() {
        return retrieveAsString(readableBytes());
    }

    // Peek操作（不移除数据）
    std::vector<uint8_t> peekAsVector(size_t len) const {
        if (len > readableBytes()) {
            len = readableBytes();
        }
        return std::vector<uint8_t>(peek(), peek() + len);
    }

    // 读取整数（网络字节序转主机字节序）
    template<typename T>
    T peekInt() const {
        static_assert(std::is_integral<T>::value, "T must be an integral type");
        if (readableBytes() < sizeof(T)) {
            throw std::runtime_error("Buffer has insufficient data for peekInt");
        }
        T value;
        std::memcpy(&value, peek(), sizeof(T));
        return networkToHost(value);
    }

    template<typename T>
    T readInt() {
        T value = peekInt<T>();
        retrieve(sizeof(T));
        return value;
    }

    // 写入数据
    void append(const void* data, size_t len) {
        ensureWritableBytes(len);
        std::memcpy(beginWrite(), data, len);
        hasWritten(len);
    }

    void append(const std::vector<uint8_t>& data) {
        append(data.data(), data.size());
    }

    void append(const std::string& data) {
        append(data.data(), data.size());
    }

    // 写入整数（主机字节序转网络字节序）
    template<typename T>
    void appendInt(T value) {
        static_assert(std::is_integral<T>::value, "T must be an integral type");
        T net_value = hostToNetwork(value);
        append(&net_value, sizeof(net_value));
    }

    // 前置写入（在预留空间写入）
    void prepend(const void* data, size_t len) {
        if (len > prependableBytes()) {
            throw std::runtime_error("Not enough prepend space");
        }
        reader_index_ -= len;
        std::memcpy(&buffer_[reader_index_], data, len);
    }

    template<typename T>
    void prependInt(T value) {
        static_assert(std::is_integral<T>::value, "T must be an integral type");
        T net_value = hostToNetwork(value);
        prepend(&net_value, sizeof(net_value));
    }

    // 收缩缓冲区（释放过多的内存）
    void shrink(size_t reserve = 0) {
        // 将有效数据移到前面
        size_t readable = readableBytes();
        std::memmove(&buffer_[kPrependSize], peek(), readable);
        reader_index_ = kPrependSize;
        writer_index_ = reader_index_ + readable;
        
        // 收缩缓冲区
        size_t new_size = kPrependSize + readable + reserve;
        buffer_.resize(new_size);
        buffer_.shrink_to_fit();
    }

    // 确保可写空间
    void ensureWritableBytes(size_t len) {
        if (writableBytes() < len) {
            makeSpace(len);
        }
    }

    // 标记已写入
    void hasWritten(size_t len) {
        writer_index_ += len;
    }

    // 获取写指针
    uint8_t* beginWrite() {
        return &buffer_[writer_index_];
    }

    const uint8_t* beginWrite() const {
        return &buffer_[writer_index_];
    }

    // 从socket读取数据
    ssize_t readFromFd(int fd, int* saved_errno);

    // 获取缓冲区总大小
    size_t capacity() const {
        return buffer_.size();
    }

    // 交换
    void swap(Buffer& rhs) noexcept {
        buffer_.swap(rhs.buffer_);
        std::swap(reader_index_, rhs.reader_index_);
        std::swap(writer_index_, rhs.writer_index_);
    }

private:
    void makeSpace(size_t len) {
        // 如果可写空间+预留空间不足，则扩容
        if (writableBytes() + prependableBytes() < len + kPrependSize) {
            // 需要扩容
            size_t new_size = writer_index_ + len;
            if (new_size > kMaxBufferSize) {
                throw std::runtime_error("Buffer size exceeds maximum limit");
            }
            buffer_.resize(new_size);
        } else {
            // 空间足够，将数据移到前面
            size_t readable = readableBytes();
            std::memmove(&buffer_[kPrependSize], peek(), readable);
            reader_index_ = kPrependSize;
            writer_index_ = reader_index_ + readable;
        }
    }

    // 字节序转换
    template<typename T>
    T networkToHost(T value) const {
        if (sizeof(T) == 1) {
            return value;
        } else if (sizeof(T) == 2) {
            uint16_t v;
            std::memcpy(&v, &value, sizeof(v));
            v = ntohs(v);
            T result;
            std::memcpy(&result, &v, sizeof(result));
            return result;
        } else if (sizeof(T) == 4) {
            uint32_t v;
            std::memcpy(&v, &value, sizeof(v));
            v = ntohl(v);
            T result;
            std::memcpy(&result, &v, sizeof(result));
            return result;
        } else if (sizeof(T) == 8) {
            uint64_t v;
            std::memcpy(&v, &value, sizeof(v));
            v = be64toh(v);
            T result;
            std::memcpy(&result, &v, sizeof(result));
            return result;
        }
        return value;
    }

    template<typename T>
    T hostToNetwork(T value) const {
        if (sizeof(T) == 1) {
            return value;
        } else if (sizeof(T) == 2) {
            uint16_t v;
            std::memcpy(&v, &value, sizeof(v));
            v = htons(v);
            T result;
            std::memcpy(&result, &v, sizeof(result));
            return result;
        } else if (sizeof(T) == 4) {
            uint32_t v;
            std::memcpy(&v, &value, sizeof(v));
            v = htonl(v);
            T result;
            std::memcpy(&result, &v, sizeof(result));
            return result;
        } else if (sizeof(T) == 8) {
            uint64_t v;
            std::memcpy(&v, &value, sizeof(v));
            v = htobe64(v);
            T result;
            std::memcpy(&result, &v, sizeof(result));
            return result;
        }
        return value;
    }

private:
    std::vector<uint8_t> buffer_;   // 底层存储
    size_t reader_index_;            // 读索引
    size_t writer_index_;            // 写索引
};

} // namespace rpc

